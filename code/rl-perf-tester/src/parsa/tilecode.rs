use super::*;

// pub fn tile_code_with_cfg_single<T: Quantable>(
// 	__addr40 __declspec(emem) tile_t *state,
// 	__declspec(cls) struct rl_config *cfg,
// 	uint8_t bias_tile_exists,
// 	uint8_t work_idx,
// 	uint16_t tiling_set_idx,
// 	uint16_t tiling_idx
// ) {
// 	uint16_t dim_idx;
// 	uint32_t local_tile;
// 	uint32_t width_product = cfg->num_actions;

// 	tile_t emergency_vals[4] = {0};

// 	// check if there's a bias tile.
// 	// => yes? set 0 has size 1, others have size cfg->tilings_per_set
// 	// => no? all have size cfg->tilings_per_set
// 	if (bias_tile_exists && work_idx == 0) {
// 		return 0;
// 	}

// 	// can also get by remul'ing
// 	local_tile = cfg->tiling_sets[tiling_set_idx].start_tile;

// 	local_tile += tiling_idx * cfg->tiling_sets[tiling_set_idx].tiling_size;

// 	// These goes over each dimension in the top-level loop
// 	for (dim_idx = 0; dim_idx < cfg->tiling_sets[tiling_set_idx].num_dims; ++dim_idx) {
// 		uint16_t active_dim = cfg->tiling_sets[tiling_set_idx].dims[dim_idx];
// 		uint8_t reduce_tile;
// 		tile_t val = state[active_dim];

// 		// This is what varies per tiling in a set.
// 		tile_t shift = tiling_idx * cfg->shift_amt[active_dim];

// 		tile_t max = cfg->adjusted_maxes[active_dim] - shift;
// 		tile_t min = cfg->mins[active_dim] - shift;

// 		// to get tile in dim... rescale dimension, divide by window.
// 		val = (val < max) ? val : max;
// 		reduce_tile = val == max;

// 		// WE NEED TO CAST VAL AS A UTILE *BEFORE* USE
// 		// OTHERWISE DIVISION WILL BE WRONG DUE
// 		// TO POTENTIAL OVERFLOW
// 		val = (val > min) ? val - min : 0;

// 		// emergency_vals[dim_idx] = val;

// 		local_tile +=
// 			width_product
// 			* (( ((utile_t)val) / cfg->width[active_dim]) - (reduce_tile ? 1 : 0));

// 		width_product *= cfg->tiles_per_dim;
// 	}

// 	#ifdef RL_DEBUG_ASSERTS
// 	if (local_tile < cfg->tiling_sets[tiling_set_idx].start_tile || local_tile >= cfg->tiling_sets[tiling_set_idx].end_tile) {
// 		life_sucks[0] = emergency_vals[0];
// 		life_sucks[1] = emergency_vals[1];
// 		life_sucks[2] = emergency_vals[2];
// 		life_sucks[3] = emergency_vals[3];
// 		__implicit_read(emergency_vals);
// 		__asm{ctx_arb[bpt]}
// 		__implicit_read(emergency_vals);
// 	}
// 	#endif

// 	__implicit_read(emergency_vals);

// 	return local_tile;
// }
