mod q;
mod tilecode;
mod writeback;

pub use self::{q::*, tilecode::*, writeback::*};

use crate::{ProtoSetup, TimeBreakdown, OUTPUT_DIR};
use bus::{Bus, BusReader};
use control::{Setup, Tile, TilingSet};
use num_traits::{Num, NumCast, PrimInt};
use rand::{Rng, SeedableRng};
use rand_chacha::ChaChaRng;
use std::{
	cell::UnsafeCell,
	cmp::{Ord, Ordering as CmpOrd, PartialEq, PartialOrd, Reverse},
	collections::{BinaryHeap, HashMap},
	fs::File,
	io::Write,
	ops::*,
	sync::{atomic::Ordering, Arc},
	time::Instant,
};

type Policy<T> = Arc<Vec<UnsafeCell<Vec<T>>>>;

#[derive(Clone)]
enum ParsaMessage<T> {
	Action(Vec<T>),
	Update(T, usize, Vec<T>),
	Exit,
}

fn parsa_control(
	wb: &Writeback,
	state: &Vec<i32>,
	setup: &Setup<i32>,
	bus: &mut Bus<ParsaMessage<i32>>,
	work_ct: usize,
	past_states: &mut HashMap<i32, (i32, usize, Vec<i32>)>,
	past_rewards: &mut HashMap<i32, i32>,
	task_ct: usize,
) -> usize {
	wb.clear_actions();
	bus.broadcast(ParsaMessage::Action(state.clone()));
	wb.gather(work_ct);

	let (mut val, mut act) = wb.select_max();

	// e-greedy for fairness against my other impl
	if rand::random::<f32>() < 0.05 {
		act = rand::thread_rng().gen_range(0..setup.n_actions as usize);
	}

	if setup.do_updates {
		val /= task_ct as i32;

		match (past_states.get(&state[0]), past_rewards.get(&1)) {
			(Some((l_val, l_act, l_state)), Some(reward)) => {
				let dt = reward + setup.gamma.q_mul(val, setup.quantiser_shift as i32) - l_val;
				let dt = dt.q_mul(setup.alpha, setup.quantiser_shift as i32);

				// println!("Inputs: gam {}, alph: {}, r {}, l_val {}, val {}", setup.gamma, setup.alpha, reward, l_val, val);

				// println!("DELAT: {}", dt);

				bus.broadcast(ParsaMessage::Update(dt, *l_act, l_state.clone()));
				wb.gather(work_ct);
			},
			_ => {},
		}

		past_states.insert(state[0], (val as i32, act, state.clone()));

		// println!("{}", state[0]);
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

		let inner_hit = (val as u32)
			.q_div(widths[*active_dim] as u32, setup.quantiser_shift as u32)
			- if reduce_tile { 1 } else { 0 };

		local_tile += width_product * inner_hit as usize;

		width_product *= setup.tiles_per_dim as usize;
	}

	local_tile
}

#[derive(Clone)]
pub struct UsefulTileSet {
	pub start_tile: usize,
	pub tiling_size: usize,
	pub dims: Vec<usize>,
}

#[derive(Clone)]
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
						our_vec[action] = our_vec[action].wrapping_add(delta);
					}
				},
			ParsaMessage::Exit => {
				break;
			},
		}

		wb.ack();
	}
}

const PARSA_RUNS: usize = 10;
const PARSA_WARMUP: usize = 1_000;
const PARSA_MEASURES: usize = 100_000;

pub fn parsa_experiment() {
	let mut setup: Setup<i32> = ProtoSetup::default().instantiate(true);
	let tiling = crate::generate_tiling(&setup, 28);
	setup.limit_workers = None;

	// do these, and online/offline.
	// let worker_cts = [num_cpus::get_physical(), 129];
	let worker_cts = [num_cpus::get_physical()];

	let host = gethostname::gethostname().into_string().unwrap();

	// criterion::black_box

	// pub fn generate_state<T: Tile + Clone>(setup: &Setup<T>, rng: &mut impl RngCore) -> Vec<T> {
	let mut rng = ChaChaRng::seed_from_u64(0xcafe_f00d_dead_beef);

	eprintln!("Building state map...");
	let big_state_collectorate = (0..=PARSA_WARMUP + PARSA_MEASURES)
		.map(|_| {
			let mut v = crate::generate_state(&setup, &mut rng);
			v[0] = 15;
			v
		})
		.collect::<Vec<Vec<i32>>>();
	eprintln!("Built!");

	for bdown in &[TimeBreakdown::ComputeAndWriteout, TimeBreakdown::UpdateAll] {
		eprintln!("Timing {:?}", bdown);

		crate::prime_setup_with_timings(&mut setup, bdown);

		for ct in worker_cts.iter() {
			eprintln!("Workers ct set to {}...", ct);

			// create writeback.
			let wb = new_writeback(setup.n_actions.into());
			let mut bus = Bus::new(100);

			eprintln!("\tBuilding schedule...");
			let (mut tasks, task_ct, preprep) = preprep_state(&setup, &tiling, *ct);
			eprintln!("\tBuilt!");

			// create worker threads
			for task_set in tasks.drain(..) {
				let my_wb = wb.clone();
				let my_bus = bus.add_rx();
				let my_setup = setup.clone();
				let my_preprep = preprep.clone();

				std::thread::spawn(|| {
					criterion::black_box(parsa_minion_loop(
						// wb: Writeback,
						my_wb,
						// mut input: BusReader<ParsaMessage<i32>>,
						my_bus,
						// tasks: Vec<Task>,
						task_set,
						// policy: Policy<i32>,
						my_preprep.policy,
						// shift_amts: Vec<i32>,
						my_preprep.shift_amts,
						// setup: Setup<i32>,
						my_setup,
						// tile_set_data: Vec<UsefulTileSet>,
						my_preprep.tile_set_data,
						// adjusted_maxes: Vec<i32>,
						my_preprep.adjusted_maxes,
						// widths: Vec<i32>,
						my_preprep.widths,
					));
				});
			}

			eprintln!("\tWorkers spawned.");

			// Now either/and: block timing, and indiv timing.
			// Make sure that states, places to write times, etc, are all preallocated.
			let mut latencies = vec![];
			let mut tputs = vec![];
			for run_idx in 0..PARSA_RUNS {
				let mut starts = Vec::with_capacity(PARSA_MEASURES);
				let mut ends = Vec::with_capacity(PARSA_MEASURES);

				let mut reward_map = HashMap::new();
				reward_map.insert(
					1,
					i32::from_float_with_quantiser(0.1, setup.quantiser_shift),
				);

				let mut state_map = HashMap::new();

				for (state_idx, state) in big_state_collectorate.iter().enumerate() {
					let t0 = Instant::now();
					criterion::black_box(parsa_control(
						// wb: Writeback,
						&wb,
						// state: &Vec<i32>,
						state,
						// setup: &Setup<i32>,
						&setup,
						// bus: &mut Bus<ParsaMessage<i32>>,
						&mut bus,
						// work_ct: usize,
						*ct,
						// past_states: &mut HashMap<i32, (i32, usize, Vec<i32>)>,
						&mut state_map,
						// past_rewards: &mut HashMap<i32, i32>,
						&mut reward_map,
						task_ct,
					));
					let t1 = Instant::now();

					if state_idx >= PARSA_WARMUP {
						starts.push(t0);
						ends.push(t1);
					}
				}

				let total_time = *ends.last().unwrap() - *starts.first().unwrap();
				let total_time_f = total_time.as_secs_f64();
				let tput = (PARSA_MEASURES as f64) / total_time_f;

				eprintln!(
					"\tA{}: Processed 100K in {} (Tput: {})",
					run_idx, total_time_f, tput
				);

				for (start, end) in starts.iter().zip(ends.iter()) {
					latencies.push(*end - *start);
				}

				tputs.push(tput);
			}

			bus.broadcast(ParsaMessage::Exit);

			// Write out results
			let out_dir = format!("{}/parsa/", OUTPUT_DIR);
			let _ = std::fs::create_dir_all(&out_dir);

			let tput_file_name = format!("{}{}.tput.{:?}.dat", out_dir, host, bdown);

			let mut out_file =
				File::create(tput_file_name).expect("Unable to open file for writing results.");

			for sample in tputs {
				let to_push = format!("{:.3}\n", sample);
				out_file
					.write_all(to_push.as_bytes())
					.expect("Write of individual value failed.");
			}

			let lats_file_name = format!("{}{}.lat.{:?}.dat", out_dir, host, bdown);

			let mut out_file =
				File::create(lats_file_name).expect("Unable to open file for writing results.");

			for sample in latencies {
				let to_push = format!("{:.9}\n", sample.as_secs_f64());
				out_file
					.write_all(to_push.as_bytes())
					.expect("Write of individual value failed.");
			}
		}
	}
}

// // tasks: Vec<Task>,
// unimplemented!(),
// // policy: Policy<i32>,
// unimplemented!(),
// // shift_amts: Vec<i32>,
// unimplemented!(),
// // setup: Setup<i32>,
// my_setup,
// // tile_set_data: Vec<UsefulTileSet>,
// unimplemented!(),
// // adjusted_maxes: Vec<i32>,
// unimplemented!(),
// // widths: Vec<i32>,
// unimplemented!(),

// #[derive(Clone)]
// pub struct UsefulTileSet {
// 	pub start_tile: usize,
// 	pub tiling_size: usize,
// 	pub dims: Vec<usize>,
// }

// #[derive(Clone)]
// pub struct Task {
// 	pub is_bias: bool,
// 	pub dims: Vec<usize>,
// 	pub tiling_idx: usize,
// 	pub tiling_set_idx: usize,
// }

// #[derive(Clone, Debug, Serialize, Deserialize)]
// pub struct Tiling {
// 	pub dims: Vec<u16>,
// 	pub location: Option<u8>,
// }

fn preprep_state(
	setup: &Setup<i32>,
	tiling: &TilingSet,
	n_workers: usize,
) -> (Vec<Vec<Task>>, usize, PrePrep) {
	let mut widths = Vec::new();
	let mut shift_amts = Vec::new();
	let mut adjusted_maxes = Vec::new();

	if setup.tilings_per_set == 1 {
		for dim in 0..setup.n_dims as usize {
			widths.push((setup.maxes[dim] - setup.mins[dim]) / setup.tiles_per_dim as i32);
			shift_amts.push(0);
			adjusted_maxes.push(setup.maxes[dim]);
		}
	} else {
		for dim in 0..setup.n_dims as usize {
			widths.push((setup.maxes[dim] - setup.mins[dim]) / (setup.tiles_per_dim - 1) as i32);
			shift_amts.push(widths[dim] / setup.tilings_per_set as i32);
			adjusted_maxes.push(setup.maxes[dim] + widths[dim]);
		}
	}

	let mut all_tasks = Vec::new();
	let mut tile_set_data = Vec::new();
	let mut curr_policy_tiles = 0;

	for (i, t) in tiling.tilings.iter().enumerate() {
		let t_dims: Vec<usize> = t.dims.iter().map(|a| *a as usize).collect();
		// NOTE: don't premul by action here, because we need to use unsafecell indices.
		let to_add = if t.location.is_none() {
			// bias tile
			all_tasks.push(Task {
				is_bias: true,
				dims: t_dims.clone(),
				tiling_idx: 0,
				tiling_set_idx: i,
			});

			tile_set_data.push(UsefulTileSet {
				start_tile: curr_policy_tiles,
				tiling_size: 1,
				dims: t_dims,
			});

			1
		} else {
			let tiles_in_tiling = (setup.tiles_per_dim as usize).pow(t.dims.len() as u32);
			let mut cost = 0;

			for tiling_idx in 0..(setup.tilings_per_set as usize) {
				all_tasks.push(Task {
					is_bias: true,
					dims: t_dims.clone(),
					tiling_idx,
					tiling_set_idx: i,
				});
				cost += tiles_in_tiling;
			}

			tile_set_data.push(UsefulTileSet {
				start_tile: curr_policy_tiles,
				tiling_size: tiles_in_tiling,
				dims: t_dims,
			});

			cost
		};

		curr_policy_tiles += to_add;
	}

	let policy = Arc::new(
		(0..curr_policy_tiles)
			.map(|_| UnsafeCell::new(vec![0; setup.n_actions as usize]))
			.collect(),
	);

	let task_ct = all_tasks.len();

	let tasks = if all_tasks.len() == n_workers {
		all_tasks.drain(..).map(|t| vec![t]).collect()
	} else {
		task_sched(all_tasks, n_workers)
	};

	(
		tasks,
		task_ct,
		PrePrep {
			policy,
			shift_amts,
			tile_set_data,
			adjusted_maxes,
			widths,
		},
	)
}

#[derive(Eq)]
struct TaskCost {
	cost: usize,
	id: usize,
}

impl Ord for TaskCost {
	fn cmp(&self, other: &Self) -> CmpOrd {
		self.cost.cmp(&other.cost)
	}
}

impl PartialOrd for TaskCost {
	fn partial_cmp(&self, other: &Self) -> Option<CmpOrd> {
		self.cost.partial_cmp(&other.cost)
	}
}

impl PartialEq for TaskCost {
	fn eq(&self, other: &Self) -> bool {
		self.cost == other.cost
	}
}

fn task_sched(mut tasks: Vec<Task>, n_workers: usize) -> Vec<Vec<Task>> {
	let mut out = vec![vec![]; n_workers];
	let mut where_next = BinaryHeap::new();
	for i in 0..n_workers {
		where_next.push(Reverse(TaskCost { cost: 0, id: i }));
	}

	for task in tasks.drain(..).rev() {
		let my_cost = 1 + task.dims.len();
		let mut slot_info = where_next.pop().unwrap();
		out[slot_info.0.id].push(task);
		slot_info.0.cost += my_cost;
		where_next.push(slot_info);
	}

	out
}

#[derive(Clone)]
struct PrePrep {
	policy: Policy<i32>,
	shift_amts: Vec<i32>,
	tile_set_data: Vec<UsefulTileSet>,
	adjusted_maxes: Vec<i32>,
	widths: Vec<i32>,
}

unsafe impl Send for PrePrep {}
