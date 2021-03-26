use super::*;
use control::TransportConfig;

pub struct Config<'a> {
	pub transport_cfg: TransportConfig,
	pub experiment: Experiment,
	pub name: &'a str,
	pub rtecli_path: &'a str,
	pub rtsym_path: &'a str,
	pub force_build: bool,
}
