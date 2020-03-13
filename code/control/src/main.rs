use clap::{
	Arg,
	App,
	SubCommand,
};
use control::{
	GlobalConfig,
	SetupConfig,
};

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
		.get_matches();

	match matches.subcommand() {
		("list", Some(_sub_m)) => {
			control::list();
		}
		("setup", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut cfg = SetupConfig::new(&mut g_cfg);

			println!("{}", serde_json::to_string(&cfg.setup).unwrap());

			control::setup(&mut cfg);
		}
		_ => {
			println!("{}", matches.usage());
		}
	}
}
