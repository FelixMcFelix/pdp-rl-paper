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
pub struct Setup {
	pub quantiser_shift: u8,

	pub n_dims: u16,

	pub tiles_per_dim: u16,

	pub tilings_per_set: u16,

	pub n_actions: u16,

	pub epsilon: Tile,

	pub alpha: Tile,

	pub gamma: Tile,

	pub epsilon_decay_amt: Tile,

	pub epsilon_decay_freq: u32,

	pub do_updates: bool,

	pub state_key: KeySource,

	pub reward_key: KeySource,

	pub maxes: Vec<Tile>,

	pub mins: Vec<Tile>,
}

impl Default for Setup {
	fn default() -> Self {
		Self {
			quantiser_shift: 8,
			do_updates: true,
			n_dims: 0,
			tiles_per_dim: 1,
			tilings_per_set: 1,
			n_actions: 2,
			epsilon: (0.1 * ((1 << 8) as f32)) as Tile,
			alpha: (0.05 * ((1 << 8) as f32)) as Tile,
			gamma: (0.8 * ((1 << 8) as f32)) as Tile,
			epsilon_decay_amt: 1,
			epsilon_decay_freq: 1,

			state_key: KeySource::Field(0),
			reward_key: KeySource::Shared,

			maxes: vec![],
			mins: vec![],
		}
	}
}

impl Setup {
	pub fn validate(&mut self) {
		assert_eq!(self.n_dims as usize, self.maxes.len());
		assert_eq!(self.n_dims as usize, self.mins.len());

		let e = (self.epsilon as f32) / ((1u32 << self.quantiser_shift) as f32);

		let a = (self.alpha as f32) / ((1u32 << self.quantiser_shift) as f32);

		assert!(e >= 0.0 && e <= 1.0);
		assert!(a >= 0.0 && a <= 1.0);

		assert!(self.tiles_per_dim >= 1);
		assert!(self.tilings_per_set >= 1);
	}
}

#[derive(Debug, Serialize, Deserialize)]
pub enum KeySource {
	Shared,
	Field(i32),
	Value(Tile),
}

impl KeySource {
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
			KeySource::Value(v) => *v,
		}
	}
}
