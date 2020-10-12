use super::*;

#[derive(Debug, Serialize, Deserialize)]
pub enum PolicyFormat<T> {
	Sparse(Vec<SparsePolicyEntry<T>>),
	Dense(Vec<T>),
}

impl<T> Default for PolicyFormat<T> {
	fn default() -> Self {
		Self::Dense(Default::default())
	}
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SparsePolicyEntry<T> {
	pub offset: u32,
	pub data: Vec<T>,
}

#[derive(Debug, Serialize, Deserialize)]
pub enum Policy {
	Float { data: PolicyFormat<f32>, quantiser: f32 },
	Quantised { data: PolicyFormat<i32> },
}

impl Default for Policy {
	fn default() -> Self {
		Self::Quantised{ data: Default::default() }
	}
}

pub struct PolicyBoundaries(pub [usize; LOCATION_MAX as usize]);

impl PolicyBoundaries {
	pub fn compute(setup: &Setup, tiling: &TilingSet) -> Self {
		let mut out = [0; LOCATION_MAX as usize];
		let mut right_edge = 0;
		let mut last_loc = 0;

		let a = setup.n_actions as usize;
		let s = setup.tilings_per_set as usize;

		let n = setup.tiles_per_dim as usize;

		for t in tiling.tilings.iter() {
			if let Some(loc) = t.location {
				while last_loc < loc as usize {
					out[last_loc] = right_edge;
					last_loc += 1;
				}
				right_edge += a * s * n.pow(t.dims.len() as u32);
			} else {
				right_edge += a;
			}
		}

		out[last_loc] = right_edge;

		Self(out)
	}
}
