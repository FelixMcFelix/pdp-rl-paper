use super::*;
use control::TransportConfig;

pub struct Config<'a> {
	pub transport_cfg: TransportConfig,
	pub experiment: Experiment,
	pub name: &'a str,
	pub rtecli_path: &'a str,
	pub rtsym_path: &'a str,
	pub force_build: bool,
	pub skip_firmware_install: bool,
	pub skip_writeout: bool,
	pub skip_setup: bool,
	pub skip_retiling: bool,
}

pub struct HostConfig {
	pub rtecli_path: String,
}

pub struct StressConfig<'a> {
	pub transport_cfg: TransportConfig,
	pub host_server: &'a str,
	pub port: u16,
	pub pci_addr: &'a str,
	pub bind_driver: &'a str,
	pub min_rate: u32,
	pub max_rate: u32,
	pub num_trials: u32,
}
