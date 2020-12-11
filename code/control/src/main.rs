use clap::{App, Arg, SubCommand};
use control::*;
use std::{fs::File, io::BufReader, net::IpAddr};

fn main() {
	let matches = App::new("(Netronome) PDP RL Control Packet Generator")
		.version("0.1")
		.author("Kyle A. Simpson")
		.about("Generate control packets for in-switch RL firmware.")
		.arg(Arg::with_name("interface")
			.short("i")
			.long("interface")
			.value_name("IFACE")
			.help(
				"Interface to send packets over.\
				If none is named, the first available interface will be used.")
			.takes_value(true))
		.subcommand(SubCommand::with_name("list")
			.about("List all bindable interfaces."))
		.subcommand(SubCommand::with_name("setup")
			.about("Sends a single setup packet.")
			.arg(Arg::with_name("src-ip")
				.short("s")
				.long("src-ip")
				.value_name("IPV4")
				.help("Source IP for control packets. Defaults to selected interface.")
				.takes_value(true))
			.arg(Arg::with_name("dst-ip")
				.short("d")
				.long("dst-ip")
				.value_name("IPV4")
				.help("Destination IP for control packets. Defaults to selected interface.")
				.takes_value(true)
				.default_value("192.168.1.141"))
			.arg(Arg::with_name("src-port")
				.short("p")
				.long("src-port")
				.value_name("U16")
				.help("Source UDP port for control packets.")
				.takes_value(true)
				.default_value("16767"))
			.arg(Arg::with_name("dst-port")
				.short("P")
				.long("dst-port")
				.value_name("U16")
				.help("Destination port for control packets.")
				.takes_value(true)
				.default_value("16768"))
			.arg(Arg::with_name("SETUP")
				.help("Input JSON file used to build config packet.")
				.takes_value(true)
				.required(true)))
		.subcommand(SubCommand::with_name("tiling")
			.about("Sends a single tiling packet.")
			.arg(Arg::with_name("src-ip")
				.short("s")
				.long("src-ip")
				.value_name("IPV4")
				.help("Source IP for control packets. Defaults to selected interface.")
				.takes_value(true))
			.arg(Arg::with_name("dst-ip")
				.short("d")
				.long("dst-ip")
				.value_name("IPV4")
				.help("Destination IP for control packets. Defaults to selected interface.")
				.takes_value(true)
				.default_value("192.168.1.141"))
			.arg(Arg::with_name("src-port")
				.short("p")
				.long("src-port")
				.value_name("U16")
				.help("Source UDP port for control packets.")
				.takes_value(true)
				.default_value("16767"))
			.arg(Arg::with_name("dst-port")
				.short("P")
				.long("dst-port")
				.value_name("U16")
				.help("Destination port for control packets.")
				.takes_value(true)
				.default_value("16768"))
			.arg(Arg::with_name("TILING")
				.help("Input JSON file used to build config packet.")
				.takes_value(true)
				.required(true)))
		.subcommand(SubCommand::with_name("insert-policy")
			.about("Sends packets containing an RL agent policy.")
			.arg(Arg::with_name("src-ip")
				.short("s")
				.long("src-ip")
				.value_name("IPV4")
				.help("Source IP for control packets. Defaults to selected interface.")
				.takes_value(true))
			.arg(Arg::with_name("dst-ip")
				.short("d")
				.long("dst-ip")
				.value_name("IPV4")
				.help("Destination IP for control packets. Defaults to selected interface.")
				.takes_value(true)
				.default_value("192.168.1.141"))
			.arg(Arg::with_name("src-port")
				.short("p")
				.long("src-port")
				.value_name("U16")
				.help("Source UDP port for control packets.")
				.takes_value(true)
				.default_value("16767"))
			.arg(Arg::with_name("dst-port")
				.short("P")
				.long("dst-port")
				.value_name("U16")
				.help("Destination port for control packets.")
				.takes_value(true)
				.default_value("16768"))
			.arg(Arg::with_name("POLICY")
				.help("Input JSON file used to build policy packets.")
				.takes_value(true)
				.required(true))
			.arg(Arg::with_name("SETUP")
				.help("Input JSON file used to build setup packet.")
				.takes_value(true)
				.required(true))
			.arg(Arg::with_name("TILING")
				.help("Input JSON file used to build tiling packet.")
				.takes_value(true)
				.required(true)))
		.subcommand(SubCommand::with_name("state")
			.about("Sends state packet.")
			.arg(Arg::with_name("src-ip")
				.short("s")
				.long("src-ip")
				.value_name("IPV4")
				.help("Source IP for control packets. Defaults to selected interface.")
				.takes_value(true))
			.arg(Arg::with_name("dst-ip")
				.short("d")
				.long("dst-ip")
				.value_name("IPV4")
				.help("Destination IP for control packets. Defaults to selected interface.")
				.takes_value(true)
				.default_value("192.168.1.141"))
			.arg(Arg::with_name("src-port")
				.short("p")
				.long("src-port")
				.value_name("U16")
				.help("Source UDP port for control packets.")
				.takes_value(true)
				.default_value("16767"))
			.arg(Arg::with_name("dst-port")
				.short("P")
				.long("dst-port")
				.value_name("U16")
				.help("Destination port for control packets.")
				.takes_value(true)
				.default_value("16768"))
			.arg(Arg::with_name("STATE")
				.help("Input JSON file used to build state packet.")
				.takes_value(true)
				.required(true)))
		.subcommand(SubCommand::with_name("fake-policy")
			.about("Generates a dummy policy according to the tiling and config information given.")
			.arg(Arg::with_name("SETUP")
				.help("Input JSON file used to build setup packet.")
				.takes_value(true)
				.required(true))
			.arg(Arg::with_name("TILING")
				.help("Input JSON file used to build tiling packet.")
				.takes_value(true)
				.required(true)))
		.subcommand(SubCommand::with_name("fake-state")
			.about("Generates a dummy state according to the config information given.")
			.arg(Arg::with_name("SETUP")
				.help("Input JSON file used to build setup packet.")
				.takes_value(true)
				.required(true)))
		.subcommand(SubCommand::with_name("reward")
			.about("Sends reward packet.")
			.arg(Arg::with_name("src-ip")
				.short("s")
				.long("src-ip")
				.value_name("IPV4")
				.help("Source IP for control packets. Defaults to selected interface.")
				.takes_value(true))
			.arg(Arg::with_name("dst-ip")
				.short("d")
				.long("dst-ip")
				.value_name("IPV4")
				.help("Destination IP for control packets. Defaults to selected interface.")
				.takes_value(true)
				.default_value("192.168.1.141"))
			.arg(Arg::with_name("src-port")
				.short("p")
				.long("src-port")
				.value_name("U16")
				.help("Source UDP port for control packets.")
				.takes_value(true)
				.default_value("16767"))
			.arg(Arg::with_name("dst-port")
				.short("P")
				.long("dst-port")
				.value_name("U16")
				.help("Destination port for control packets.")
				.takes_value(true)
				.default_value("16768"))
			.arg(Arg::with_name("value-location")
				.short("l")
				.long("value-location")
				.value_name("I32")
				.help("Destination port for control packets.")
				.takes_value(true)
				.default_value("0"))
			.arg(Arg::with_name("REWARD")
				.help("Floating point reward value.")
				.takes_value(true)
				.required(true))
			.arg(Arg::with_name("SHIFT")
				.help("Quantiser value (i.e., bitshift).")
				.takes_value(true)
				.required(true)))
		.get_matches();

	match matches.subcommand() {
		("list", Some(_sub_m)) => {
			control::list();
		},
		("setup", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut cfg = SetupConfig::new(&mut g_cfg);

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

			let setup_file = File::open(sub_m.value_of("SETUP").unwrap())
				.expect("Setup file could not be opened.");

			cfg.setup = serde_json::from_reader(BufReader::new(setup_file))
				.expect("Invalid setup packet config file!");

			println!("{}", serde_json::to_string_pretty(&cfg.setup).unwrap());

			control::setup(&mut cfg);
		},
		("tiling", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut cfg = TilingsConfig::new(&mut g_cfg);

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

			let setup_file = File::open(sub_m.value_of("TILING").unwrap())
				.expect("Tiling file could not be opened.");

			cfg.tiling = serde_json::from_reader(BufReader::new(setup_file))
				.expect("Invalid setup packet config file!");

			println!("{}", serde_json::to_string_pretty(&cfg.tiling).unwrap());
			println!("{:#?}", cfg.tiling);

			control::tilings(&mut cfg);
		},
		("insert-policy", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut cfg = PolicyConfig::new(&mut g_cfg);

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

			let policy_file = File::open(sub_m.value_of("POLICY").unwrap())
				.expect("Policy file could not be opened.");

			let setup_file = File::open(sub_m.value_of("SETUP").unwrap())
				.expect("Setup file could not be opened.");

			let tiling_file = File::open(sub_m.value_of("TILING").unwrap())
				.expect("Tiling file could not be opened.");

			//

			cfg.policy = serde_json::from_reader(BufReader::new(policy_file))
				.expect("Invalid policy config file!");

			cfg.setup = serde_json::from_reader(BufReader::new(setup_file))
				.expect("Invalid setup packet config file!");

			cfg.tiling = serde_json::from_reader(BufReader::new(tiling_file))
				.expect("Invalid tiling packet config file!");

			println!("{}", serde_json::to_string_pretty(&cfg.policy).unwrap());
			println!("{:#?}", cfg.policy);

			control::insert_policy(&mut cfg);
		},
		("state", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut cfg = SendStateConfig::new(&mut g_cfg);

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

			let state_file = File::open(sub_m.value_of("STATE").unwrap())
				.expect("Policy file could not be opened.");

			//

			let state: Vec<Tile> = serde_json::from_reader(BufReader::new(state_file))
				.expect("Invalid policy config file!");

			// println!("{}", serde_json::to_string_pretty(&cfg.policy).unwrap());
			// println!("{:#?}", cfg.policy);

			control::send_state(&mut cfg, state);
		},
		("fake-policy", Some(sub_m)) => {
			let setup_file = File::open(sub_m.value_of("SETUP").unwrap())
				.expect("Setup file could not be opened.");

			let tiling_file = File::open(sub_m.value_of("TILING").unwrap())
				.expect("Tiling file could not be opened.");

			let cfg = FakePolicyGeneratorConfig {
				tiling: serde_json::from_reader(BufReader::new(tiling_file))
					.expect("Invalid tiling packet config file!"),
				setup: serde_json::from_reader(BufReader::new(setup_file))
					.expect("Invalid setup packet config file!"),
			};

			control::generate_policy(&cfg);
		},
		("fake-state", Some(sub_m)) => {
			let setup_file = File::open(sub_m.value_of("SETUP").unwrap())
				.expect("Setup file could not be opened.");

			let cfg: FakeStateGeneratorConfig = serde_json::from_reader(BufReader::new(setup_file))
				.expect("Invalid tiling packet config file!");

			control::generate_state(&cfg);
		},
		("reward", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut cfg = SendRewardConfig::new(&mut g_cfg);

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

			let value_loc = sub_m.value_of("value-location")
				.unwrap()
				.parse::<i32>()
				.expect("Must be a valid i32.");

			let reward = sub_m.value_of("REWARD")
				.unwrap()
				.parse::<f32>()
				.expect("Must be a valid f32.");

			assert!(reward.is_finite());

			let shift = sub_m.value_of("SHIFT")
				.unwrap()
				.parse::<usize>()
				.expect("Must be a valid usize.");

			control::send_reward(&mut cfg, value_loc, reward, shift);
		},
		_ => {
			println!("{}", matches.usage());
		},
	}
}
