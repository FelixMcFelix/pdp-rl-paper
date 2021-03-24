use super::*;
use std::{io::Write, mem};

// pub type Tile = i32;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct TilingSet {
	pub tilings: Vec<Tiling>,
}

pub trait Tile {
	fn write_bytes<T: Write>(&self, buf: &mut T) -> IoResult<usize>;
	fn float(&self) -> f32;
	fn wideint(&self) -> i32;

	fn from_float(val: f32) -> Self;
	fn from_int(val: i32) -> Self;
	fn size_of() -> usize;

	fn from_float_with_quantiser(val: f32, quantiser: u8) -> Self
		where Self: Sized
	{
		Tile::from_float(val * ((1 << quantiser) as f32))
	}

	fn to_float_with_quantiser(&self, quantiser: u8) -> f32
		where Self: Sized
	{
		self.float() / ((1 << quantiser) as f32)
	}

	fn smallest_unit_float(quantiser: u8) -> f32
		where Self: Sized
	{
		Self::from_int(1).to_float_with_quantiser(quantiser)
	}
}

impl Tile for i8 {
	fn write_bytes<T: Write>(&self, buf: &mut T) -> IoResult<usize> {
		buf.write(&[*self as u8])?;

		Ok(mem::size_of::<Self>())
	}

	fn float(&self) -> f32 {
		*self as f32
	}

	fn wideint(&self) -> i32 {
		*self as i32
	}

	fn from_float(val: f32) -> Self {
		val as i8
	}

	fn from_int(val: i32) -> Self {
		val as i8
	}

	fn size_of() -> usize {
		mem::size_of::<Self>()
	}
}

impl Tile for i16 {
	fn write_bytes<T: Write>(&self, buf: &mut T) -> IoResult<usize> {
		buf.write_i16::<BigEndian>(*self)?;
		Ok(mem::size_of::<Self>())
	}

	fn float(&self) -> f32 {
		*self as f32
	}

	fn wideint(&self) -> i32 {
		*self as i32
	}

	fn from_float(val: f32) -> Self {
		val as i16
	}

	fn from_int(val: i32) -> Self {
		val as i16
	}

	fn size_of() -> usize {
		mem::size_of::<Self>()
	}
}

impl Tile for i32 {
	fn write_bytes<T: Write>(&self, buf: &mut T) -> IoResult<usize> {
		buf.write_i32::<BigEndian>(*self)?;
		Ok(mem::size_of::<Self>())
	}

	fn float(&self) -> f32 {
		*self as f32
	}

	fn wideint(&self) -> i32 {
		*self
	}

	fn from_float(val: f32) -> Self {
		val as i32
	}

	fn from_int(val: i32) -> Self {
		val as i32
	}

	fn size_of() -> usize {
		mem::size_of::<Self>()
	}
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
		let has_bias = self.tilings.iter().find(|el| el.dims.len() == 0).is_some();

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

#[derive(Clone, Debug, Serialize, Deserialize)]
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
