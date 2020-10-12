use super::*;

pub type Tile = i32;

#[derive(Debug, Serialize, Deserialize)]
pub struct TilingSet {
	pub tilings: Vec<Tiling>,
}

impl Default for TilingSet {
	fn default() -> Self {
		Self {
			tilings: vec![Default::default()],
		}
	}
}

impl TilingSet {
	pub fn validate(&mut self) {
		let mut clean = vec![];

		// Place bias tile at top, if present.
		// Remove excess bias tiles.
		// Bias tile must have no location.
		let has_bias = self.tilings.iter()
			.find(|el| el.dims.len() == 0)
			.is_some();

		if has_bias {
			clean.push(Default::default());
		}

		// Sort tilings by location.
		for el in itertools::sorted(self.tilings.drain(0..)) {
			if el.dims.len() != 0 {
				// All non-bias tiles must have a location.
				assert!(el.location.is_some());

				// Ensure no tiles in invalid location.
				let loc = el.location.unwrap();
				assert!(loc < LOCATION_MAX);

				clean.push(el);
			}
		}

		self.tilings = clean;
	}
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Tiling {
	pub dims: Vec<u16>,
	pub location: Option<u8>,
}

impl Default for Tiling {
	fn default() -> Self {
		Self {
			dims: vec![],
			location: None,
		}
	}
}

impl Ord for Tiling {
	fn cmp(&self, other: &Self) -> Ordering {
		let s_none = self.location.is_none();
		let o_none = other.location.is_none();

		if s_none && o_none {
			Ordering::Equal
		} else if s_none {
			Ordering::Less
		} else if o_none {
			Ordering::Greater
		} else {
			self.location.unwrap().cmp(&other.location.unwrap())
		}
	}
}

impl PartialOrd for Tiling {
	fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
		Some(self.cmp(other))
	}
}

impl PartialEq for Tiling {
	fn eq(&self, other: &Self) -> bool {
		self.location == other.location
	}
}

impl Eq for Tiling {}
