use super::*;

#[derive(Serialize, Deserialize, Debug, Copy, Clone)]
pub struct RatioDef<T: Integer> {
	pub numer: T,
	pub denom: T,
}

impl<T> From<RatioDef<T>> for Ratio<T> 
	where T: Integer + Clone {
	fn from(def: RatioDef<T>) -> Ratio<T> {
		Ratio::new(def.numer, def.denom)
	}
}

impl<T> From<Ratio<T>> for RatioDef<T> 
	where T: Integer + Clone {
	fn from(def: Ratio<T>) -> RatioDef<T> {
		RatioDef {
			numer: def.numer().clone(),
			denom: def.denom().clone(),
		}
	}
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Setup {
	pub n_dims: u16,

	pub tiles_per_dim: u16,

	pub tilings_per_set: u16,

	pub n_actions: u16,

	pub epsilon: RatioDef<Tile>,

	pub alpha: RatioDef<Tile>,

	pub epsilon_decay_amt: Tile,

	pub epsilon_decay_freq: u32,

	pub maxes: Vec<Tile>,

	pub mins: Vec<Tile>,
}

impl Default for Setup {
	fn default() -> Self {
		Self {
			n_dims: 0,
			tiles_per_dim: 1,
			tilings_per_set: 1,
			n_actions: 2,
			epsilon: Ratio::new(1, 10).into(),
			alpha: Ratio::new(1, 20).into(),
			epsilon_decay_amt: 1,
			epsilon_decay_freq: 1,
			maxes: vec![],
			mins: vec![],
		}
	}
}

impl Setup {
	pub fn validate(&mut self) {
		assert_eq!(self.n_dims as usize, self.maxes.len());
		assert_eq!(self.n_dims as usize, self.mins.len());

		let e = self.epsilon.numer as f32 / self.epsilon.denom as f32;

		let a = self.alpha.numer as f32 / self.alpha.denom as f32;

		assert!(e >= 0.0 && e <= 1.0);
		assert!(a >= 0.0 && a <= 1.0);

		assert!(self.tiles_per_dim >= 1);
		assert!(self.tilings_per_set >= 1);
	}
}
