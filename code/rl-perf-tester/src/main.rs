use clap::{App, Arg, ArgMatches, SubCommand};
use control::{GlobalConfig, TransportConfig};
use core::fmt::Debug;
use rl_perf_tester::Config;
use serde::{de::DeserializeOwned, Serialize};
use std::{fs::File, io::BufReader, net::IpAddr};

fn main() {
	let matches = App::new("PDP RL Core Execution Performance Evaluator")
		.version("0.1")
		.author("Kyle A. Simpson")
		.about("Generate (many!) control packets to measure in-switch RL firmware.")
		.arg(
			Arg::with_name("interface")
				.short("i")
				.long("interface")
				.value_name("IFACE")
				.help(
					"Interface to send packets over.\
				If none is named, the first available interface will be used.",
				)
				.takes_value(true),
		)
		.arg(
			Arg::with_name("datatype")
				.short("q")
				.long("datatype")
				.value_name("TYPE")
				.help(
					"Datatype to fit quantised policy values into.\
				Can be `i8`, `i16`, or `i32.\
				Must match the compile-time width of the target switch.",
				)
				.takes_value(true)
				.default_value("i32"),
		)
		.subcommand(SubCommand::with_name("list").about("List all bindable interfaces."))
		.subcommand(SubCommand::with_name("expts").about("List all experiment names for `run`."))
		.subcommand(
			SubCommand::with_name("run")
				.about("Sends a single setup packet.")
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
				)
				.arg(
					Arg::with_name("NAME")
						.help("Experiment name")
						.takes_value(true)
						.required(true),
				),
		)
		.get_matches();

	match matches.subcommand() {
		("list", Some(_sub_m)) => {
			control::list();
		},
		("expts", Some(_sub_m)) => {
			rl_perf_tester::list();
		},
		("run", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut t_cfg: TransportConfig = Default::default();

			if let Some(ip) = sub_m.value_of("src-ip") {
				t_cfg
					.src_addr
					.set_ip(ip.parse().expect("Invalid source IpV4Addr."));
			} else {
				let ip = (&g_cfg.iface.ips)
					.iter()
					.filter_map(|ip_n| match ip_n.ip() {
						IpAddr::V4(i) => Some(i),
						_ => None,
					})
					.next()
					.expect("Interface did not have a default IPv4 address to bind to.");
			}

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

			let name = format!(
				"{}/{}.json",
				rl_perf_tester::EXPERIMENT_DIR,
				sub_m.value_of("NAME").unwrap(),
			);

			let setup_file = File::open(&name).expect("Setup file could not be opened.");

			let experiment = serde_json::from_reader(BufReader::new(setup_file))
				.expect("Invalid experiment config file!");

			let mut cfg = Config {
				control_cfg: &mut g_cfg,
				transport_cfg: &mut t_cfg,
				experiment,
				name: &name,
			};

			rl_perf_tester::run_experiment(&mut cfg);
		},
		_ => {
			println!("{}", matches.usage());
		},
	}
}
