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
			.about("Sends a single setup packet."))
		.get_matches();

	match matches.subcommand() {
		("list", Some(_sub_m)) => {
			control::list();
		}
		("setup", Some(sub_m)) => {
			let mut g_cfg = GlobalConfig::new(matches.value_of("interface"));
			let mut cfg = SetupConfig::new(&mut g_cfg);
			control::setup(&mut cfg);
		}
		_ => {

		}
	}
}
