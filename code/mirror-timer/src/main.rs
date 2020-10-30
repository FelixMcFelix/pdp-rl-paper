use clap::{App, Arg, SubCommand};
use mirror_timer::*;
use std::net::IpAddr;

fn main() {
	let matches = App::new("(Netronome) Mirror timer")
		.version("0.1")
		.author("Kyle A. Simpson")
		.about("Time RTT for in-switch RL firmware.")
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
		.subcommand(SubCommand::with_name("list").about("List all bindable interfaces."))
		.subcommand(
			SubCommand::with_name("time")
				.about("Sends packets to a netronome interface, timing the RTT to mirror each.")
				.arg(
					Arg::with_name("src-ip")
						.short("s")
						.long("src-ip")
						.value_name("IPV4")
						.help("Source IP for control packets. Defaults to selected interface.")
						.takes_value(true)
						.default_value("192.168.1.2"),
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
					Arg::with_name("num-points")
						.help("Number of RTT measurements to take.")
						.takes_value(true)
						.required(true)
						.default_value("100000"),
				),
		)
		.get_matches();

	match matches.subcommand() {
		("list", Some(_sub_m)) => {
			mirror_timer::list();
		},
		("time", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));

			let iterations = sub_m
				.value_of("num-points")
				.and_then(|s| s.parse().ok())
				.expect("Must be a valid u64!");

			let mut cfg = Config::new(&mut g_cfg, iterations);

			if let Some(ip) = sub_m.value_of("src-ip") {
				cfg.transport
					.src_addr
					.set_ip(ip.parse().expect("Invalid source IpV4Addr."));
			} else {
				let ip = (&cfg.global.iface.ips)
					.iter()
					.filter_map(|ip_n| match ip_n.ip() {
						IpAddr::V4(i) => Some(i),
						_ => None,
					})
					.next()
					.expect("Interface did not have a default IPv4 address to bind to.");
				cfg.transport.src_addr.set_ip(ip);
			}

			if let Some(port) = sub_m.value_of("src-port") {
				cfg.transport
					.src_addr
					.set_port(port.parse().expect("Invalid source port (u16)."));
			}

			if let Some(ip) = sub_m.value_of("dst-ip") {
				cfg.transport
					.dst_addr
					.set_ip(ip.parse().expect("Invalid destination IpV4Addr."));
			}

			if let Some(port) = sub_m.value_of("dst-port") {
				cfg.transport
					.dst_addr
					.set_port(port.parse().expect("Invalid destination port (u16)."));
			}

			mirror_timer::run(&mut cfg);
		},
		_ => {
			println!("{}", matches.usage());
		},
	}
}
