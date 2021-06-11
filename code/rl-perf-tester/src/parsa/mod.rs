mod q;
mod tilecode;
mod writeback;

pub use self::{q::*, tilecode::*, writeback::*};

use bus::{Bus, BusReader};
use control::{Setup, Tile};
use num_traits::{Num, NumCast, PrimInt};
use std::{
	cell::UnsafeCell,
	collections::HashMap,
	ops::*,
	sync::{atomic::Ordering, Arc},
};

type Policy<T> = Arc<Vec<UnsafeCell<Vec<T>>>>;

#[derive(Clone)]
enum ParsaMessage<T> {
	Action(Vec<T>),
	Update(T, usize, Vec<T>),
	Exit,
}

fn parsa_control(
	wb: Writeback,
	state: &Vec<i32>,
	setup: &Setup<i32>,
	bus: &mut Bus<ParsaMessage<i32>>,
	work_ct: usize,
	past_states: &mut HashMap<i32, (i32, usize, Vec<i32>)>,
	past_rewards: &mut HashMap<i32, i32>,
) -> usize {
	wb.clear_actions();
	bus.broadcast(ParsaMessage::Action(state.clone()));
	wb.gather(work_ct);

	let (val, act) = wb.select_max();

	if setup.do_updates {
		match (past_states.get(&state[0]),past_rewards.get(&1)) {
			(Some((l_val, l_act, l_state)), Some(reward)) => {
				let dt = reward + setup.gamma.q_mul(val, setup.quantiser_shift as i32) - l_val;
				let dt = dt.q_mul(setup.alpha, setup.quantiser_shift as i32);

				bus.broadcast(ParsaMessage::Update(dt, *l_act, l_state.clone()));
				wb.gather(work_ct);
			},
			_ => {},
		}

		past_states.insert(state[0], (val, act, state.clone()));
	}

	act
}

fn tile_code(
	state: &Vec<i32>,
	task: &Task,
	shift_amts: &Vec<i32>,
	setup: &Setup<i32>,
	tile_set_data: &Vec<UsefulTileSet>,
	adjusted_maxes: &Vec<i32>,
	widths: &Vec<i32>,
) -> usize {
	if task.is_bias {
		return 0;
	}

	let mut width_product: usize = 1;
	let mut local_tile = tile_set_data[task.tiling_set_idx].start_tile;
	local_tile += task.tiling_idx * tile_set_data[task.tiling_set_idx].tiling_size;

	for active_dim in &task.dims {
		let mut val = state[*active_dim];
		let shift = task.tiling_idx as i32 * shift_amts[*active_dim];
		let max = adjusted_maxes[*active_dim] - shift;
		let min = setup.mins[*active_dim] - shift;

		val = if val > min { val - min } else { 0 };

		let reduce_tile = val >= max;

		let inner_hit = (val as u32).q_div(widths[*active_dim] as u32, setup.quantiser_shift as u32)
			- if reduce_tile { 1 } else { 0 };

		local_tile += width_product * inner_hit as usize;

		width_product *= setup.tiles_per_dim as usize;
	}

	local_tile
}

pub struct UsefulTileSet {
	pub start_tile: usize,
	pub tiling_size: usize,
	pub dims: Vec<usize>,
}

pub struct Task {
	pub is_bias: bool,
	pub dims: Vec<usize>,
	pub tiling_idx: usize,
	pub tiling_set_idx: usize,
}

// #[derive(Clone, Debug, Serialize, Deserialize)]
// pub struct Setup<T: Tile> {
// 	pub quantiser_shift: u8,

// 	// serialize me as a u4!
// 	pub force_update_to_happen: Option<bool>,

// 	// serialize me as a u4!
// 	pub disable_action_writeout: bool,

// 	pub limit_workers: Option<u16>,

// 	pub n_dims: u16,

// 	pub tiles_per_dim: u16,

// 	pub tilings_per_set: u16,

// 	pub n_actions: u16,

// 	pub epsilon: T,

// 	pub alpha: T,

// 	pub gamma: T,

// 	pub epsilon_decay_amt: T,

// 	pub epsilon_decay_freq: u32,

// 	pub do_updates: bool,

// 	pub state_key: KeySource<T>,

// 	pub reward_key: KeySource<T>,

// 	pub maxes: Vec<T>,

// 	pub mins: Vec<T>,
// }

fn parsa_minion_loop(
	wb: Writeback,
	mut input: BusReader<ParsaMessage<i32>>,
	tasks: Vec<Task>,
	policy: Policy<i32>,
	shift_amts: Vec<i32>,
	setup: Setup<i32>,
	tile_set_data: Vec<UsefulTileSet>,
	adjusted_maxes: Vec<i32>,
	widths: Vec<i32>,
) {
	while let Ok(msg) = input.recv() {
		match msg {
			ParsaMessage::Action(state) =>
				for task in tasks.iter() {
					let hit = tile_code(
						&state,
						task,
						&shift_amts,
						&setup,
						&tile_set_data,
						&adjusted_maxes,
						&widths,
					);
					let policy_slot = &policy[hit];
					unsafe {
						let our_vec = &*policy_slot.get();

						for (src, dest) in our_vec.iter().zip(wb.vals.iter()) {
							let _ =
								dest.fetch_add(num_traits::cast(*src).unwrap(), Ordering::Relaxed);
						}
					}
				},
			ParsaMessage::Update(delta, action, state) =>
				for task in tasks.iter() {
					let hit = tile_code(
						&state,
						task,
						&shift_amts,
						&setup,
						&tile_set_data,
						&adjusted_maxes,
						&widths,
					);
					let policy_slot = &policy[hit];
					unsafe {
						let our_vec = &mut *policy_slot.get();
						our_vec[action] += delta;
					}
				},
			ParsaMessage::Exit => {
				break;
			},
		}

		wb.ack();
	}
}

pub fn parsa_experiment() {
	// do these, and online/offline.
	let worker_cts = [num_cpus::get_physical(), 129];

	// criterion::black_box
}
