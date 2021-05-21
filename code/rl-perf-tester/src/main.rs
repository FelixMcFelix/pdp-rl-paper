use clap::{App, Arg, ArgMatches, SubCommand};
use control::TransportConfig;
use core::iter::Iterator;
use rl_perf_tester::{Config, ExperimentFile, InstallTimes, OUTPUT_DIR, RTE_PATH, RTSYM_PATH};
use std::{fs::File, io::{BufReader, Write}};

fn main() {
	let run_base = SubCommand::with_name("run")
		.about("Runs the given experiments.")
		.arg(
			Arg::with_name("force-build")
				.short("B")
				.long("force-build")
				.help("Forces individual datafiles to be rebuilt even if firmware hasn't changed.")
				.takes_value(false),
		)
		.arg(
			Arg::with_name("no-fw")
				.short("N")
				.long("no-fw")
				.help("Skip firmware installation stage.")
				.takes_value(false),
		)
		.arg(
			Arg::with_name("no-write")
				.short("W")
				.long("no-write")
				.help("Skip writing output data.")
				.takes_value(false),
		)
		.arg(
			Arg::with_name("no-setup")
				.short("S")
				.long("no-setup")
				.help("Skip setup packets.")
				.takes_value(false),
		)
		.arg(
			Arg::with_name("no-rand")
				.short("R")
				.long("no-rand")
				.help("Skip randomisation between state packets.")
				.takes_value(false),
		)
		.arg(
			Arg::with_name("src-ip")
				.short("s")
				.long("src-ip")
				.value_name("IPV4")
				.help("Source IP for control packets. Defaults to selected interface.")
				.takes_value(true)
				.default_value("192.168.0.4"),
		)
		.arg(
			Arg::with_name("dst-ip")
				.short("d")
				.long("dst-ip")
				.value_name("IPV4")
				.help("Destination IP for control packets. Defaults to selected interface.")
				.takes_value(true)
				.default_value("192.168.1.141"),
		)
		.arg(
			Arg::with_name("src-port")
				.short("p")
				.long("src-port")
				.value_name("U16")
				.help("Source UDP port for control packets.")
				.takes_value(true)
				.default_value("16767"),
		)
		.arg(
			Arg::with_name("dst-port")
				.short("P")
				.long("dst-port")
				.value_name("U16")
				.help("Destination port for control packets.")
				.takes_value(true)
				.default_value("16768"),
		);

	let matches = App::new("PDP RL Core Execution Performance Evaluator")
		.version("0.1")
		.author("Kyle A. Simpson")
		.about("Generate (many!) control packets to measure in-switch RL firmware.")
		.arg(
			Arg::with_name("interface")
				.short("i")
				.long("interface")
				.value_name("IFACE")
				.help("Interface to send packets over.")
				.takes_value(true)
				.default_value("vf0_0"),
		)
		.arg(
			Arg::with_name("rtecli-path")
				.short("c")
				.long("rtecli-path")
				.value_name("PATH")
				.help("Path to the executable `rtecli`.")
				.takes_value(true)
				.default_value(RTE_PATH),
		)
		.arg(
			Arg::with_name("rtsym-path")
				.short("r")
				.long("rtsym-path")
				.value_name("PATH")
				.help("Path to the executable `nfp-rtsym`.")
				.takes_value(true)
				.default_value(RTSYM_PATH),
		)
		.subcommand(SubCommand::with_name("list").about("List all bindable interfaces."))
		.subcommand(SubCommand::with_name("expts").about("List all experiment names for `run`."))
		.subcommand(
			run_base.clone().arg(
				Arg::with_name("NAME")
					.help("Experiment name")
					.takes_value(true)
					.min_values(1)
					.required(true),
			),
		)
		.subcommand(run_base.clone().name("run-all"))
		.get_matches();

	match matches.subcommand() {
		("list", Some(_sub_m)) => {
			control::list();
		},
		("expts", Some(_sub_m)) => {
			rl_perf_tester::list();
		},
		("run-all", Some(sub_m)) => {
			run_core(
				&matches,
				&sub_m,
				rl_perf_tester::get_list().iter().map(|s| s.as_str()),
			);
		},
		("run", Some(sub_m)) =>
			if let Some(names) = sub_m.values_of("NAME") {
				run_core(&matches, &sub_m, names);
			} else {
				println!("No experiment names given.");
				println!("{}", matches.usage());
			},
		_ => {
			println!("{}", matches.usage());
		},
	}
}

fn run_core<'a>(
	matches: &ArgMatches<'_>,
	sub_m: &ArgMatches<'_>,
	expts: impl Iterator<Item = &'a str>,
) {
	let if_name = matches.value_of("interface").expect(
		"Interface name is always set: you MUST know what it will be after fw installation.",
	);
	let mut t_cfg: TransportConfig = Default::default();

	t_cfg.src_addr.set_ip(
		sub_m
			.value_of("src-ip")
			.expect("Assign a source IP: the test framework will not assign routing info.")
			.parse()
			.expect("Invalid source IpV4Addr."),
	);

	if let Some(port) = sub_m.value_of("src-port") {
		t_cfg
			.src_addr
			.set_port(port.parse().expect("Invalid source port (u16)."));
	}

	if let Some(ip) = sub_m.value_of("dst-ip") {
		t_cfg
			.dst_addr
			.set_ip(ip.parse().expect("Invalid destination IpV4Addr."));
	}

	if let Some(port) = sub_m.value_of("dst-port") {
		t_cfg
			.dst_addr
			.set_port(port.parse().expect("Invalid destination port (u16)."));
	}

	let mut install_times = InstallTimes::default();

	for raw_name in expts {
		let name = format!("{}/{}.json", rl_perf_tester::EXPERIMENT_DIR, raw_name);

		let setup_file = File::open(&name).expect("Setup file could not be opened.");

		let experiment_file: ExperimentFile = serde_json::from_reader(BufReader::new(setup_file))
			.expect("Invalid experiment config file!");

		let experiment = experiment_file.into();

		let cfg = Config {
			transport_cfg: t_cfg,
			experiment,
			name: &raw_name,
			rtecli_path: matches
				.value_of("rtecli-path")
				.expect("Always has a value by default."),
			rtsym_path: matches
				.value_of("rtsym-path")
				.expect("Always has a value by default."),
			force_build: sub_m.is_present("force-build"),
			skip_firmware_install: sub_m.is_present("no-fw"),
			skip_writeout: sub_m.is_present("no-write"),
			skip_setup: sub_m.is_present("no-setup"),
			skip_retiling: sub_m.is_present("no-rand"),
		};

		rl_perf_tester::run_experiment(&cfg, if_name, &mut install_times);
	}

	if !sub_m.is_present("no-write") {
		// Writeout FW times.
		// TODO: make folder for install times.
		// TODO: make files with current date in 'em.
		let out_dir = format!("{}/setup-times/", OUTPUT_DIR);
		let time = chrono::offset::Utc::now();
		let time_str = time.format("%FT %H %M %S");

		let _ = std::fs::create_dir_all(&out_dir);

		// fw install times.
		let cfg_install_file = format!("{}install-{}.dat", out_dir, time_str);
		let mut out_file =
			File::create(cfg_install_file).expect("Unable to open file for writing results.");

		for sample in install_times.fws {
			let to_push = format!("{:.6}\n", sample.as_secs_f64());
			out_file
				.write_all(to_push.as_bytes())
				.expect("Write of individual value failed.");
		}

		out_file
			.flush()
			.expect("Failed to flush file contents to disk.");

		// cfg times
		let cfg_time_file = format!("{}cfg-{}.dat", out_dir, time_str);
		let mut out_file =
			File::create(cfg_time_file).expect("Unable to open file for writing results.");

		for cfg_time in install_times.configs {
			let to_push = format!(
				"{} {} {} {} {:?}\n",
				cfg_time.cfg_time,
				cfg_time.tiling_time,
				cfg_time.dim_count,
				cfg_time.core_count,
				cfg_time.fw,
			);
			out_file
				.write_all(to_push.as_bytes())
				.expect("Write of individual value failed.");
		}

		out_file
			.flush()
			.expect("Failed to flush file contents to disk.");
	}
}
