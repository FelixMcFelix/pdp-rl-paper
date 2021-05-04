use control::{KeySource, Setup, Tile, Tiling, TilingSet};
use rand::{Rng, RngCore};
use serde::{Deserialize, Serialize};
use std::{
	convert::TryInto,
	iter::{IntoIterator, Iterator},
};

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct ExperimentFile {
	pub bit_depths: Vec<String>,
	pub core_count: ExperimentRange,
	pub policy_dims: ExperimentRange,
	pub sample_count: u64,
	pub elements_to_time: Vec<TimeBreakdown>,
	pub fws: Vec<Firmware>,

	pub setup: Option<ProtoSetup>,
	pub warmup_len: Option<u64>,
	pub sanitise_bounds: Option<bool>,
}

#[derive(Clone, Debug)]
pub struct Experiment {
	pub bit_depths: Vec<String>,
	pub core_count: ExperimentRange,
	pub policy_dims: ExperimentRange,
	pub sample_count: u64,
	pub elements_to_time: Vec<TimeBreakdown>,
	pub fws: Vec<Firmware>,

	pub setup: ProtoSetup,
	pub warmup_len: u64,
	pub sanitise_bounds: bool,
}

// define these in terms of

#[derive(Clone, Debug, Default, Deserialize, Serialize)]
pub struct ProtoSetup {
	pub quantiser_shift: Option<u8>,
	pub n_dims: Option<u16>,
	pub tiles_per_dim: Option<u16>,
	pub tilings_per_set: Option<u16>,
	pub n_actions: Option<u16>,
	pub epsilon: Option<f32>,
	pub alpha: Option<f32>,
	pub gamma: Option<f32>,
	pub epsilon_decay_calcs: Option<usize>,
	pub state_key: Option<FloatKeySource>,
	pub reward_key: Option<FloatKeySource>,
	pub maxes: Option<Vec<f32>>,
	pub mins: Option<Vec<f32>>,
}

#[derive(Copy, Clone, Debug, Serialize, Deserialize)]
pub enum FloatKeySource {
	Shared,
	Field(i32),
	Value(f32),
}

impl FloatKeySource {
	fn quantise<T>(self, quantiser_shift: u8) -> KeySource<T>
	where
		T: Tile,
	{
		use FloatKeySource::*;
		match self {
			Shared => KeySource::Shared,
			Field(f) => KeySource::Field(f),
			Value(v) => KeySource::Value(T::from_float_with_quantiser(v, quantiser_shift)),
		}
	}
}

impl ProtoSetup {
	pub fn instantiate<T: Tile>(&self, sanitise_bounds: bool) -> Setup<T> {
		// IDEA: 1 "sign bit", dedidate 2/3 of remaining to fractional part.
		let quantiser_shift = self
			.quantiser_shift
			.unwrap_or_else(|| (2 * ((T::size_of() * 8) - 1) / 3).try_into().unwrap());

		let n_dims = self.n_dims.unwrap_or(20);

		let tiles_per_dim = self.tiles_per_dim.unwrap_or(6);

		let tilings_per_set = self.tilings_per_set.unwrap_or(8);

		let n_actions = self.n_actions.unwrap_or(10);

		let raw_epsilon = self.epsilon.unwrap_or(0.1);
		let epsilon = T::from_float_with_quantiser(raw_epsilon, quantiser_shift);

		let alpha = T::from_float_with_quantiser(self.alpha.unwrap_or(0.05), quantiser_shift);

		let gamma = T::from_float_with_quantiser(self.gamma.unwrap_or(0.8), quantiser_shift);

		let epsilon_decay_calcs = self.epsilon_decay_calcs.unwrap_or(10_000) as f32;

		let per_ts_decay = raw_epsilon / epsilon_decay_calcs;
		let test_ep_decay_amt = T::from_float_with_quantiser(per_ts_decay, quantiser_shift);

		let (epsilon_decay_amt, epsilon_decay_freq) = if test_ep_decay_amt.wideint() < 1 {
			// Too small: need to bump up freq.
			let min_el = T::smallest_unit_float(quantiser_shift);
			let freq = (per_ts_decay / min_el).ceil() as u32;
			(T::from_int(1), freq)
		} else {
			// Just right
			(test_ep_decay_amt, 1)
		};

		let state_key = self
			.state_key
			.unwrap_or(FloatKeySource::Field(0))
			.quantise(quantiser_shift);

		let reward_key = self
			.reward_key
			.unwrap_or(FloatKeySource::Shared)
			.quantise(quantiser_shift);

		let mut maxes = self
			.maxes
			.clone()
			.unwrap_or_else(|| vec![10.0; n_dims.into()]);
		let mut mins = self
			.mins
			.clone()
			.unwrap_or_else(|| vec![-10.0; n_dims.into()]);

		maxes.resize(n_dims.into(), maxes.last().cloned().unwrap_or(10.0));
		mins.resize(n_dims.into(), mins.last().cloned().unwrap_or(-10.0));

		if sanitise_bounds {
			let format_min = T::minimum_float(quantiser_shift);
			let format_max = T::maximum_float(quantiser_shift);

			if tilings_per_set <= 1 {
				for val in mins.iter_mut() {
					*val = val.max(format_min);
				}

				for val in maxes.iter_mut() {
					*val = val.min(format_max);
				}
			} else {
				//mins
				for (max, min) in maxes.iter_mut().zip(mins.iter_mut()) {
					let width = (*max - *min) / ((tiles_per_dim - 1) as f32);
					let beyond_bound = *min - width;

					if beyond_bound <= format_min {
						let frac = beyond_bound / format_min;
						*max /= frac;
						*min /= frac;
					}
				}

				//maxes
				for (max, min) in maxes.iter_mut().zip(mins.iter_mut()) {
					let width = (*max - *min) / ((tiles_per_dim - 1) as f32);
					let beyond_bound = *max + width;

					if beyond_bound >= format_max {
						let frac = beyond_bound / format_max;
						*max /= frac;
						*min /= frac;
					}
				}
			};
		}

		let maxes = maxes
			.drain(..)
			.map(|v| T::from_float_with_quantiser(v, quantiser_shift))
			.collect();
		let mins = mins
			.drain(..)
			.map(|v| T::from_float_with_quantiser(v, quantiser_shift))
			.collect();

		Setup {
			quantiser_shift,
			n_dims,
			tiles_per_dim,
			tilings_per_set,
			n_actions,
			epsilon,
			alpha,
			gamma,
			epsilon_decay_amt,
			epsilon_decay_freq,
			state_key,
			reward_key,
			maxes,
			mins,

			do_updates: false,
			disable_action_writeout: false,
			force_update_to_happen: None,
			limit_workers: None,
		}
	}
}

impl From<ExperimentFile> for Experiment {
	fn from(val: ExperimentFile) -> Self {
		Self {
			bit_depths: val.bit_depths,
			core_count: val.core_count,
			policy_dims: val.policy_dims,
			sample_count: val.sample_count,
			elements_to_time: val.elements_to_time,
			fws: val.fws,

			setup: val.setup.unwrap_or_default(),
			warmup_len: val.warmup_len.unwrap_or(1000),
			sanitise_bounds: val.sanitise_bounds.unwrap_or(true),
		}
	}
}

impl From<Experiment> for ExperimentFile {
	fn from(val: Experiment) -> Self {
		Self {
			bit_depths: val.bit_depths,
			core_count: val.core_count,
			policy_dims: val.policy_dims,
			sample_count: val.sample_count,
			elements_to_time: val.elements_to_time,
			fws: val.fws,

			setup: Some(val.setup),
			warmup_len: Some(val.warmup_len),
			sanitise_bounds: Some(val.sanitise_bounds),
		}
	}
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum ExperimentRange {
	Fixed(u64),
	List(Vec<u64>),
	Range(u64, u64),
	RangeStride(u64, u64, u64),
}

#[derive(Clone, Debug)]
pub struct ExperimentRangeIter {
	range: ExperimentRange,
	idx: usize,
}

impl Iterator for ExperimentRangeIter {
	type Item = u64;

	fn next(&mut self) -> Option<Self::Item> {
		let out = match &self.range {
			ExperimentRange::Fixed(v) if self.idx == 0 => Some(*v),
			ExperimentRange::List(l) => l.get(self.idx).copied(),
			ExperimentRange::Range(s, e) if e >= s && (self.idx as u64) <= e - s =>
				Some(s + self.idx as u64),
			ExperimentRange::RangeStride(s, e, step)
				if e >= s && (self.idx as u64) * step <= e - s =>
				Some(s + step * (self.idx as u64)),
			_ => None,
		};

		if out.is_some() {
			self.idx += 1;
		}

		out
	}
}

impl IntoIterator for ExperimentRange {
	type Item = u64;
	type IntoIter = ExperimentRangeIter;

	fn into_iter(self) -> Self::IntoIter {
		ExperimentRangeIter {
			range: self,
			idx: 0,
		}
	}
}

#[derive(Copy, Clone, Debug, Deserialize, Serialize)]
pub enum TimeBreakdown {
	Compute,
	ComputeAndWriteout,
	UpdateOnlyPrep,
	UpdateAll,
}

#[derive(Copy, Clone, Debug, Deserialize, Serialize)]
pub enum Firmware {
	SingleCore,
	Chunk,
	Randomised,
	Stride,
	OffsetStride,
	Balanced,
}

impl Default for Firmware {
	fn default() -> Self {
		Self::Balanced
	}
}

impl Firmware {
	pub fn fw_name(&self) -> &str {
		use Firmware::*;
		match self {
			SingleCore => "single",
			Chunk => "multi.chunk",
			Randomised => "multi.chunk_rand",
			Stride => "multi.stride",
			OffsetStride => "multi.stride_off",
			Balanced => "multi.balanced",
		}
	}

	pub fn output_name(&self) -> &str {
		use Firmware::*;
		match self {
			SingleCore => "single",
			Chunk => "chunk",
			Randomised => "randomised",
			Stride => "stride",
			OffsetStride => "offset_stride",
			Balanced => "balanced",
		}
	}

	pub fn resend_tiling(&self) -> bool {
		matches!(self, Firmware::Randomised)
	}
}

pub fn generate_tiling<T: Tile>(setup: &Setup<T>, num_work_dims: usize) -> TilingSet {
	const MAX_DIMS_PER_LOC: [usize; 3] = [1, 2, 4];
	const MAX_SETS_PER_LOC: [usize; 3] = [8, 8, 1];

	let dims_to_skip: [Option<usize>; 2] = [
		setup.state_key.field().map(|v| v as usize),
		setup.reward_key.field().map(|v| v as usize),
	];

	let mut tilings = Vec::new();
	let mut remaining_dims_to_spend = num_work_dims;

	let mut location: u8 = 0;
	let mut sets_in_this_location = 0;

	let mut skips = 0;

	while remaining_dims_to_spend > 0 && location < 3 {
		let (tiling, dim_ct): (Tiling, usize) = match (location, sets_in_this_location) {
			(0, 0) => (
				Tiling {
					dims: vec![],
					location: None,
				},
				1,
			),
			_ => {
				let dims_to_use_here =
					MAX_DIMS_PER_LOC[location as usize].min(remaining_dims_to_spend);
				let floor = num_work_dims + skips - remaining_dims_to_spend;

				let mut dims = Vec::new();
				let mut local_skips = 0;
				for i in floor..(floor + dims_to_use_here) {
					while dims_to_skip[..]
						.contains(&Some((i + local_skips) & setup.n_dims as usize))
					{
						local_skips += 1;
					}

					dims.push((i + local_skips) as u16 % setup.n_dims)
				}

				skips += local_skips;

				let tiling = Tiling {
					dims,
					location: Some(location),
				};

				(tiling, dims_to_use_here)
			},
		};

		tilings.push(tiling);

		remaining_dims_to_spend -= dim_ct;

		sets_in_this_location += 1;
		if sets_in_this_location >= MAX_SETS_PER_LOC[location as usize] {
			location += 1;
			sets_in_this_location = 0;
		}
	}

	TilingSet { tilings }
}

pub fn prime_setup_with_timings<T: Tile>(setup: &mut Setup<T>, timed_el: &TimeBreakdown) {
	use TimeBreakdown::*;
	match timed_el {
		Compute => {
			setup.do_updates = false;
			setup.disable_action_writeout = true;
		},
		ComputeAndWriteout => {
			setup.do_updates = false;
			setup.disable_action_writeout = false;
		},
		UpdateOnlyPrep => {
			setup.do_updates = true;
			setup.disable_action_writeout = false;
			setup.force_update_to_happen = Some(false);
		},
		UpdateAll => {
			setup.do_updates = true;
			setup.disable_action_writeout = false;
			setup.force_update_to_happen = Some(true);
		},
	}
}

pub fn generate_state<T: Tile + Clone>(setup: &Setup<T>, rng: &mut impl RngCore) -> Vec<T> {
	let mut out = Vec::new();

	for i in 0..setup.n_dims as usize {
		let min = setup.mins[i].wideint();
		let max = setup.maxes[i].wideint();
		let sample = rng.gen_range(min..=max);

		out.push(T::from_int(sample))
	}

	if let Some(f) = setup.state_key.field() {
		if let Some(valid) = out.get_mut(f as usize) {
			*valid = T::from_int(3);
		}
	}

	if let Some(f) = setup.reward_key.field() {
		if let Some(valid) = out.get_mut(f as usize) {
			*valid = T::from_int(5);
		}
	}

	out
	//vec![T::from_int(0); setup.n_dims as usize]
}
