pub const EXPERIMENT_DIR: &str = "experiments";
pub const OUTPUT_DIR: &str = "../../results/rl-perf-tester";
pub const FIRMWARE_DIR: &str = "../nfp/rl-test/fws";
pub const RTE_PATH: &str = "/opt/netronome/p4/bin/rtecli";
pub const P4CFG_PATH: &str = "../nfp/rl-test/testcfg.p4cfg";
pub const RTSYM_PATH: &str = "/opt/netronome/bin/nfp-rtsym";
pub const PKTGEN_PATH: &str = "/home/clouduser/dpdk/pktgen-dpdk";

pub const DUMB_HACK_PATH: &str = "/var/rl-perf-tester/";
pub const DUMB_HACK_FILE: &str = "/var/rl-perf-tester/progress";

pub mod fw_consts {
	pub const MAX_ACTIONS: usize = 10;
	pub const MAX_TILINGS_PER_SET: usize = 8;
	pub const MAX_TILES_PER_DIM: usize = 6;

	pub const TILE_SET_CONST_PART: usize = MAX_TILINGS_PER_SET * 1;

	pub const T1_MAX_DIMS: usize = 1;
	pub const T2_MAX_DIMS: usize = 2;
	pub const T3_MAX_DIMS: usize = 4;
	pub const RL_DIMENSION_MAX: usize = 20;

	pub const T1_MAX_SETS: usize = 8;
	pub const T2_MAX_SETS: usize = 8;
	pub const T3_MAX_SETS: usize = 1;

	pub const T1_MAX_TILES_PER_TILING: usize = MAX_TILES_PER_DIM;
	pub const T2_MAX_TILES_PER_TILING: usize = MAX_TILES_PER_DIM.pow(T2_MAX_DIMS as u32);
	pub const T3_MAX_TILES_PER_TILING: usize = MAX_TILES_PER_DIM.pow(T3_MAX_DIMS as u32);

	pub const T1_TILE_COUNT: usize = T1_MAX_SETS * T1_MAX_TILES_PER_TILING * TILE_SET_CONST_PART;
	pub const T2_TILE_COUNT: usize = T2_MAX_SETS * T2_MAX_TILES_PER_TILING * TILE_SET_CONST_PART;
	pub const T3_TILE_COUNT: usize = T3_MAX_SETS * T3_MAX_TILES_PER_TILING * TILE_SET_CONST_PART;

	pub const T1_FIRST_TILE: usize = 0;
	pub const T2_FIRST_TILE: usize = T1_TILE_COUNT;
	pub const T3_FIRST_TILE: usize = T2_FIRST_TILE + T2_TILE_COUNT;
}
