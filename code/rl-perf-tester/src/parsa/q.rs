use std::ops::*;

pub trait Quantable {
	fn q_mul(self, other: Self, depth: Self) -> Self;
	fn q_div(self, other: Self, depth: Self) -> Self;
}

// pub struct Quant<T: Quantable>{
// 	pub val: T,
// 	pub depth:
// };

// impl<T: Quantable> Add for Quant<T> {
// 	type Output = Self;

// 	fn add(self, other: Self) -> Self {
// 		Quant(self.0 + other.0)
// 	}
// }

impl Quantable for i32 {
	fn q_mul(self, other: Self, depth: Self) -> Self {
		let mut intermediate = self as i64;
		intermediate *= other as i64;
		intermediate += 1 << (depth - 1);

		(intermediate >> depth) as i32
	}

	fn q_div(self, other: Self, depth: Self) -> Self {
		let intermediate = (self as i64) << depth;
		(intermediate / other as i64) as i32
	}
}

impl Quantable for u32 {
	fn q_mul(self, other: Self, depth: Self) -> Self {
		let mut intermediate = self as u64;
		intermediate *= other as u64;
		intermediate += 1 << (depth - 1);

		(intermediate >> depth) as u32
	}

	fn q_div(self, other: Self, depth: Self) -> Self {
		let intermediate = (self as u64) << depth;
		(intermediate / other as u64) as u32
	}
}

impl Quantable for i16 {
	fn q_mul(self, other: Self, depth: Self) -> Self {
		let mut intermediate = self as i32;
		intermediate *= other as i32;
		intermediate += 1 << (depth - 1);

		(intermediate >> depth) as i16
	}

	fn q_div(self, other: Self, depth: Self) -> Self {
		let intermediate = (self as i32) << depth;
		(intermediate / other as i32) as i16
	}
}

impl Quantable for i8 {
	fn q_mul(self, other: Self, depth: Self) -> Self {
		let mut intermediate = self as i16;
		intermediate *= other as i16;
		intermediate += 1 << (depth - 1);

		(intermediate >> depth) as i8
	}

	fn q_div(self, other: Self, depth: Self) -> Self {
		let intermediate = (self as i16) << depth;
		(intermediate / other as i16) as i8
	}
}
