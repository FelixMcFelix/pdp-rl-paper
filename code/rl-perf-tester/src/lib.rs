mod config;
mod constants;
mod experiment;

pub use self::{config::*, constants::*, experiment::*};

use control::{GlobalConfig, SendStateConfig, Setup, SetupConfig, Tile, TilingsConfig};
use core::fmt::Debug;
use itertools::Itertools;
use rand::SeedableRng;
use rand_chacha::ChaChaRng;
use serde::{de::DeserializeOwned, Serialize};
use std::{
	fs::File,
	io::Write,
	process::Command,
	thread,
	time::{Duration, Instant},
};

pub fn list() {
	match std::fs::read_dir(EXPERIMENT_DIR) {
		Ok(iter) =>
			for entry in iter.flatten() {
				eprintln!("{}", entry.path().file_stem().unwrap().to_str().unwrap());
			},
		Err(e) => eprintln!("Failed to iterate over experiments: {:?}", e),
	}
}

pub fn get_list() -> Vec<String> {
	match std::fs::read_dir(EXPERIMENT_DIR) {
		Ok(iter) => iter
			.flatten()
			.map(|ent| ent.path().file_stem().unwrap().to_str().unwrap().into())
			.collect(),
		Err(e) => {
			eprintln!("Failed to iterate over experiments: {:?}", e);
			vec![]
		},
	}
}

pub fn run_experiment(config: &Config, if_name: &str, times: &mut InstallTimes) {
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

		(match bit_depth.as_str() {
			"8" => run_experiment_with_datatype::<i8>,
			"16" => run_experiment_with_datatype::<i16>,
			"32" => run_experiment_with_datatype::<i32>,
			_ => panic!("Invalid bit depth datatype: must be 8, 16, or 32."),
		})(config, if_name, &bit_depth, &fw, times);
	}
}

fn run_experiment_with_datatype<T>(
	config: &Config,
	if_name: &str,
	bit_depth: &str,
	fw: &Firmware,
	times: &mut InstallTimes,
) where
	T: Tile + DeserializeOwned + Serialize + Debug + Default + Clone,
{
	let cfg_params = config
		.experiment
		.clone()
		.policy_dims
		.into_iter()
		.cartesian_product(config.experiment.core_count.clone().into_iter())
		.cartesian_product(config.experiment.elements_to_time.iter());

	for ((dim_count, core_count), timed_el) in cfg_params {
		let mut rng = ChaChaRng::seed_from_u64(0xcafe_f00d_dead_beef);

		eprintln!("\t{:?} {:?} {:?}", dim_count, core_count, timed_el);

		let in_file = format!("{}/{}bit/{}.nffw", FIRMWARE_DIR, bit_depth, fw.fw_name());
		let out_dir = format!("{}/{}/{}/", OUTPUT_DIR, config.name, bit_depth,);
		let out_file = format!(
			"{}{}.{}d.{}c.{:?}.dat",
			out_dir,
			fw.output_name(),
			dim_count,
			core_count,
			timed_el,
		);

		// Try to skip if we don't need to rebuild.
		let files_changed = {
			let in_file_meta =
				std::fs::metadata(&in_file).expect("Target firmware does not exist!");
			match std::fs::metadata(&out_file) {
				Ok(out_file_meta) => {
					let t_firmware = in_file_meta.modified().unwrap();
					let t_out = out_file_meta.modified().unwrap();

					// Ok => output is newer than firmware
					// Err(_) => output is older than firmware => changed!
					t_out.duration_since(t_firmware).is_err()
				},
				_ => true,
			}
		};
		if !(config.force_build || files_changed) {
			eprintln!("\tNo change to {}. Skipping.", &in_file);
			continue;
		}

		if !config.skip_firmware_install {
			// NOTE: I do this here to try and mitigate any randomness between sets foe now.
			// And/or strange bugs that lead to permanent early exit.
			eprint!("\tInstalling firmware... ");
			std::io::stderr().flush().unwrap();

			let fw_t0 = Instant::now();
			let cmd_output = Command::new(config.rtecli_path)
				.args(&[
					"design-load",
					"-f",
					&in_file[..],
					"-p",
					&format!("{}/{}bit/{}.json", FIRMWARE_DIR, bit_depth, fw.fw_name())[..],
					"-c",
					P4CFG_PATH,
				])
				.output()
				.expect("Failed to install firmware.");
			let fw_t1 = Instant::now();

			if !cmd_output.status.success() {
				panic!(
					"Failed to install firmware (code {:?}): {:?} {:?}",
					cmd_output.status.code(),
					std::str::from_utf8(&cmd_output.stdout[..]),
					std::str::from_utf8(&cmd_output.stderr[..]),
				);
			}

			eprintln!("Installed!");

			let fw_install_time = fw_t1 - fw_t0;
			times.fws.push(fw_install_time);

			std::thread::sleep(std::time::Duration::from_secs(3));
		}

		// NOTE: we must do this here because firmware installation
		// destroys and recreates the sending channel/virtual interfaces.
		let mut global_base = GlobalConfig::new(Some(if_name));
		let global = &mut global_base;

		let transport = config.transport_cfg;

		// create the setup packet, tiling packet here
		// send those over.

		let mut setup: Setup<T> = config
			.experiment
			.setup
			.instantiate(config.experiment.sanitise_bounds);
		let tiling = generate_tiling(&setup, dim_count as usize);

		prime_setup_with_timings(&mut setup, timed_el);
		setup.limit_workers = Some(core_count as u16);

		let mut setup_cfg = SetupConfig {
			global,
			transport,
			setup: setup.clone(),
		};

		if !config.skip_setup {
			control::setup::<T>(&mut setup_cfg);
			let setup_time = get_cycle_ct(config, CycleSource::ConfigRaw);

			control::tilings(&mut TilingsConfig {
				global,
				tiling: tiling.clone(),
				transport,
			});
			let tilings_time = get_cycle_ct(config, CycleSource::ConfigFull);

			times.configs.push(ConfigInstallTime {
				cfg_time: setup_time,
				tiling_time: tilings_time,
				dim_count,
				core_count,
				fw: *fw,
			});
		}

		let mut samples = Vec::with_capacity(config.experiment.sample_count as usize);

		eprint!(
			"\t\tWarming up for {} packets... ",
			config.experiment.warmup_len
		);

		let mut delay_target = Instant::now() + Duration::from_millis(2);

		std::io::stderr().flush().unwrap();
		for i in 0..config.experiment.warmup_len {
			if !(config.skip_retiling) && i > 0 && fw.resend_tiling() {
				control::tilings(&mut TilingsConfig {
					global,
					tiling: tiling.clone(),
					transport,
				});
			}

			control::send_state::<T>(
				&mut SendStateConfig { global, transport },
				generate_state(&setup, &mut rng),
			);

			if let Some(t_left) = delay_target.checked_duration_since(Instant::now()) {
				thread::sleep(t_left);
			}
			delay_target = Instant::now() + Duration::from_millis(2);
		}

		eprintln!("Warmed up!");
		std::io::stderr().flush().unwrap();
		delay_target = Instant::now() + Duration::from_millis(2);

		eprint!("\t\t");
		for i in 0..config.experiment.sample_count {
			if i % 100 == 0 {
				eprint!("{}.. ", i);
				std::io::stderr().flush().unwrap();
			}
			if !(config.skip_retiling) && fw.resend_tiling() {
				control::tilings(&mut TilingsConfig {
					global,
					tiling: tiling.clone(),
					transport,
				});
			}

			control::send_state::<T>(
				&mut SendStateConfig { global, transport },
				generate_state(&setup, &mut rng),
			);

			samples.push(get_cycle_ct(config, CycleSource::WorkItem));

			if let Some(t_left) = delay_target.checked_duration_since(Instant::now()) {
				thread::sleep(t_left);
			}
			delay_target = Instant::now() + Duration::from_millis(2);
		}
		eprintln!();

		eprintln!("\t\tDone!");

		if !config.skip_writeout {
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
}

#[derive(Default)]
pub struct InstallTimes {
	pub configs: Vec<ConfigInstallTime>,
	pub fws: Vec<Duration>,
}

#[derive(Default)]
pub struct ConfigInstallTime {
	pub cfg_time: u64,
	pub tiling_time: u64,
	pub dim_count: u64,
	pub core_count: u64,
	pub fw: Firmware,
}

enum CycleSource {
	WorkItem,
	ConfigRaw,
	ConfigFull,
}

impl CycleSource {
	fn get_register_name(&self) -> &'static str {
		use CycleSource::*;

		match self {
			WorkItem => "_cycle_estimate",
			ConfigRaw => "_config_cycle_estimate",
			ConfigFull => "_full_config_cycle_estimate",
		}
	}
}

fn get_cycle_ct(config: &Config, source: CycleSource) -> u64 {
	// measure and parse output of cycle-estimate.
	let mem_text = Command::new(config.rtsym_path)
		.args(&[source.get_register_name()])
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

	cycle_ct * 16
}
