use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, Serialize)]
pub struct Experiment {
	bit_depths: Vec<String>,
	core_count: ExperimentRange,
	policy_dims: ExperimentRange,
	sample_count: u64,
	elements_to_time: Vec<TimeBreakdown>,
	fw_names: Vec<Rename>,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum Rename {
	Pass(String),
	To(String, String),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum ExperimentRange {
	Fixed(u64),
	List(Vec<u64>),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum TimeBreakdown {
	Compute,
	ComputeAndWriteout,
	UpdateOnlyPrep,
	UpdateAll,
}
