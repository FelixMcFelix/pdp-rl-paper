mod config;
mod constants;
mod experiment;

pub use self::{config::*, constants::*, experiment::*};

use control::{GlobalConfig, SendStateConfig, Setup, SetupConfig, Tile, TilingsConfig};
use core::fmt::Debug;
use itertools::Itertools;
use serde::{de::DeserializeOwned, Serialize};
use std::{fs::File, io::Write, process::Command};

pub fn list() {
	match std::fs::read_dir(EXPERIMENT_DIR) {
		Ok(iter) =>
			for entry in iter.flatten() {
				eprintln!("{}", entry.path().file_stem().unwrap().to_str().unwrap());
			},
		Err(e) => eprintln!("Failed to iterate over experiments: {:?}", e),
	}
}

pub fn run_experiment(config: &Config, if_name: &str) {
	let fws = config
		.experiment
		.bit_depths
		.iter()
		.cloned()
		.cartesian_product(config.experiment.fws.iter().cloned());

	for (bit_depth, fw) in fws {
		eprintln!("{:?} {:?}", bit_depth, fw.output_name());

		match bit_depth.as_str() {
			"8" | "16" | "32" => {},
			_ => {
				eprintln!("INVALID BIT DEPTH: {}", bit_depth);
				continue;
			},
		}

		Command::new(config.rtecli_path)
			.args(&[
				"design-load",
				"-f",
				&format!("{}/{}bit/{}.nffw", FIRMWARE_DIR, bit_depth, fw.fw_name())[..],
				"-p",
				&format!("{}/{}bit/{}.json", FIRMWARE_DIR, bit_depth, fw.fw_name())[..],
				"-c",
				P4CFG_PATH,
			])
			.output()
			.expect("Failed to install firmware.");

		// NOTE: we must do this here because firmware installation
		// destroys and recreates the sending channel/virtual interfaces.
		let mut global = GlobalConfig::new(Some(if_name));

		(match bit_depth.as_str() {
			"8" => run_experiment_with_datatype::<i8>,
			"16" => run_experiment_with_datatype::<i16>,
			"32" => run_experiment_with_datatype::<i32>,
			_ => panic!("Invalid bit depth datatype: must be 8, 16, or 32."),
		})(config, &mut global, &bit_depth, &fw);
	}
}

fn run_experiment_with_datatype<T>(
	config: &Config,
	global: &mut GlobalConfig,
	bit_depth: &str,
	fw: &Firmware,
) where
	T: Tile + DeserializeOwned + Serialize + Debug + Default + Clone,
{
	let cfg_params = config
		.experiment
		.clone()
		.policy_dims
		.clone()
		.into_iter()
		.cartesian_product(config.experiment.core_count.clone().into_iter())
		.cartesian_product(config.experiment.elements_to_time.iter());

	for ((dim_count, core_count), timed_el) in cfg_params {
		eprintln!("\t{:?} {:?} {:?}", dim_count, core_count, timed_el);

		let transport = config.transport_cfg;

		// create the setup packet, tiling packet here
		// send those over.

		let mut setup: Setup<T> = config.experiment.setup.clone().into();
		let tiling = generate_tiling(&setup, dim_count as usize);

		prime_setup_with_timings(&mut setup, timed_el);
		setup.limit_workers = Some(core_count as u16);

		let dims = setup.n_dims.into();

		control::setup::<T>(&mut SetupConfig {
			global,
			transport,
			setup,
		});
		control::tilings(&mut TilingsConfig {
			global,
			tiling: tiling.clone(),
			transport,
		});

		let mut samples = Vec::with_capacity(config.experiment.sample_count as usize);

		eprint!("\t\t");
		for i in 0..config.experiment.sample_count {
			if i % 100 == 0 {
				eprint!("{}.. ", i);
				std::io::stdout().flush().unwrap();
			}
			if i > 0 && fw.resend_tiling() {
				control::tilings(&mut TilingsConfig {
					global,
					tiling: tiling.clone(),
					transport,
				});
			}

			control::send_state::<T>(
				&mut SendStateConfig { global, transport },
				vec![T::from_int(0); dims],
			);

			// measure and parse output of cycle-estimate.
			let mem_text = Command::new(config.rtsym_path)
				.args(&["_cycle_estimate"])
				.output()
				.expect("Failed to read NFP memory.");

			let mut cycle_ct = 0u64;

			let chunks = std::str::from_utf8(&mem_text.stdout[..])
				.expect("RTSym output not valid UTF-8.")
				.split_whitespace()
				.skip(1)
				.take(2);

			for (i, hex_word) in chunks.enumerate() {
				let without_prefix = hex_word.trim_start_matches("0x");
				let x = u64::from_str_radix(without_prefix, 16).expect("Not a hex-formatted word!");

				cycle_ct |= x << (32 * i);
			}

			samples.push(cycle_ct * 16);
		}
		eprint!("\n");

		eprintln!("\t\tDone!");

		let out_dir = format!("{}/{}/{}/", OUTPUT_DIR, config.name, bit_depth,);

		let out_file = format!(
			"{}{}.{}d.{}c.{:?}.dat",
			out_dir,
			fw.output_name(),
			dim_count,
			core_count,
			timed_el,
		);
        eprintln!("\t\tWriting out {} to: {}", config.name, out_file);

		let _ = std::fs::create_dir_all(&out_dir);

		let mut out_file =
			File::create(out_file).expect("Unable to open file for writing results.");

		for sample in samples {
			let to_push = format!("{}\n", sample);
			out_file
				.write_all(to_push.as_bytes())
				.expect("Write of individual value failed.");
		}

		out_file
			.flush()
			.expect("Failed to flush file contents to disk.");

		eprintln!("\t\tWritten!");
	}
}
