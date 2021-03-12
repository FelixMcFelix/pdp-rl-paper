use super::*;

#[derive(Serialize, Deserialize, Debug, Copy, Clone)]
pub struct RatioDef<T: Integer> {
	pub numer: T,
	pub denom: T,
}

impl<T> From<RatioDef<T>> for Ratio<T>
where
	T: Integer + Clone,
{
	fn from(def: RatioDef<T>) -> Ratio<T> {
		Ratio::new(def.numer, def.denom)
	}
}

impl<T> From<Ratio<T>> for RatioDef<T>
where
	T: Integer + Clone,
{
	fn from(def: Ratio<T>) -> RatioDef<T> {
		RatioDef {
			numer: def.numer().clone(),
			denom: def.denom().clone(),
		}
	}
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Setup<T: Tile> {
	pub quantiser_shift: u8,

	// serialize me as a u4!
	pub force_update_to_happen: Option<bool>,

	// serialize me as a u4!
	pub disable_action_writeout: bool,

	pub n_dims: u16,

	pub tiles_per_dim: u16,

	pub tilings_per_set: u16,

	pub n_actions: u16,

	pub epsilon: T,

	pub alpha: T,

	pub gamma: T,

	pub epsilon_decay_amt: T,

	pub epsilon_decay_freq: u32,

	pub do_updates: bool,

	pub state_key: KeySource<T>,

	pub reward_key: KeySource<T>,

	pub maxes: Vec<T>,

	pub mins: Vec<T>,
}

impl<T: Tile> Default for Setup<T> {
	fn default() -> Self {
		Self {
			quantiser_shift: 8,
			force_update_to_happen: None,
			disable_action_writeout: false,
			do_updates: true,
			n_dims: 0,
			tiles_per_dim: 1,
			tilings_per_set: 1,
			n_actions: 2,
			epsilon: T::from_float(0.1 * ((1 << 8) as f32)),
			alpha: T::from_float(0.05 * ((1 << 8) as f32)),
			gamma: T::from_float(0.8 * ((1 << 8) as f32)),
			epsilon_decay_amt: T::from_int(1),
			epsilon_decay_freq: 1,

			state_key: KeySource::Field(0),
			reward_key: KeySource::Shared,

			maxes: vec![],
			mins: vec![],
		}
	}
}

impl<T: Tile> Setup<T> {
	pub fn validate(&mut self) {
		assert_eq!(self.n_dims as usize, self.maxes.len());
		assert_eq!(self.n_dims as usize, self.mins.len());

		let e = self.epsilon.float() / ((1u32 << self.quantiser_shift) as f32);

		let a = self.alpha.float() / ((1u32 << self.quantiser_shift) as f32);

		assert!(e >= 0.0 && e <= 1.0);
		assert!(a >= 0.0 && a <= 1.0);

		assert!(self.tiles_per_dim >= 1);
		assert!(self.tilings_per_set >= 1);
	}
}

#[derive(Debug, Serialize, Deserialize)]
pub enum KeySource<T: Tile> {
	Shared,
	Field(i32),
	Value(T),
}

impl<T: Tile> KeySource<T> {
	pub fn id(&self) -> u8 {
		match self {
			KeySource::Shared => 0,
			KeySource::Field(_) => 1,
			KeySource::Value(_) => 2,
		}
	}

	pub fn body(&self) -> i32 {
		match self {
			KeySource::Shared => 0,
			KeySource::Field(f) => *f,
			KeySource::Value(v) => v.wideint(),
		}
	}
}
