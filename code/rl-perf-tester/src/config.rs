use super::*;
use control::{GlobalConfig, TransportConfig};

pub struct Config<'a> {
	pub control_cfg: &'a mut GlobalConfig,
	pub transport_cfg: &'a mut TransportConfig,
	pub experiment: Experiment,
	pub name: &'a str,
}
