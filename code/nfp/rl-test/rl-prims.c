/** Convert a state vector into a list of tile indices.
*
* Returns length of tile coded list.
*/
__intrinsic uint16_t tile_code(__addr40 __declspec(emem) tile_t *state, __declspec(cls) struct rl_config *cfg, __declspec(ctm) uint32_t *output) {
	uint16_t tiling_set_idx;
	uint16_t out_idx = 0;

	// FIXME: misses out special casing for bias tile?

	// Tiling sets: each relies upon its own dimension list.
	// Each of these is one entry in a tiling packet.
	for (tiling_set_idx = 0; tiling_set_idx < cfg->num_tilings; ++tiling_set_idx) {
		uint16_t tiling_idx;
		uint32_t local_tile_base = cfg->tiling_sets[tiling_set_idx].start_tile;

		if (cfg->tiling_sets[tiling_set_idx].num_dims == 0) {
			// This is a bias tile.
			// Forcibly select it!.
			output[out_idx++] = local_tile_base;

			continue;
		}

		// Repeat the selected tiling set, changing shift.
		// Each of these is a *copy* of the above loop, shifted along by the relevant indices.
		for (tiling_idx = 0; tiling_idx < cfg->tilings_per_set; ++tiling_idx) {
			uint16_t dim_idx;
			// We can either divide the tile base, and mult all the tile indices later.
			// Or... we could do it here, and save the trouble!
			// (ordinarily, this would start at 1 for "true tile indices" (action-free))
			uint32_t width_product = cfg->num_actions;
			uint32_t local_tile = local_tile_base;

			// These goes over each dimension in the top-level loop
			for (dim_idx = 0; dim_idx < cfg->tiling_sets[tiling_set_idx].num_dims; ++dim_idx) {
				uint16_t active_dim = cfg->tiling_sets[tiling_set_idx].dims[dim_idx];
				uint8_t reduce_tile;
				tile_t val = state[active_dim];

				// This is what varies per tiling in a set.
				tile_t shift = tiling_idx * cfg->shift_amt[active_dim];

				tile_t max = cfg->adjusted_maxes[active_dim] - shift;
				tile_t min = cfg->mins[active_dim] - shift;

				// to get tile in dim... rescale dimension, divide by window.
				val = (val < max) ? val : max;
				reduce_tile = val == max;

				// WE NEED TO CAST VAL AS A UTILE *BEFORE* USE
				// OTHERWISE DIVISION WILL BE WRONG DUE
				// TO POTENTIAL OVERFLOW
				val = (val > min) ? val - min : 0;

				// emergency_vals[dim_idx] = val;

				local_tile +=
					width_product
					* (( ((utile_t)val) / cfg->width[active_dim]) - (reduce_tile ? 1 : 0));
				width_product *= cfg->tiles_per_dim;
			}

			output[out_idx++] = local_tile;
			local_tile_base += cfg->tiling_sets[tiling_set_idx].tiling_size;
		}
	}
	return out_idx;
}

/** Convert a tile list into a list of action preferences.
*
* Length written into act_list is guaranteed to be equal to cfg->num_actions.
* Note, that act_list must be at least of size MAX_ACTIONS.
*/
__intrinsic void action_preferences(__declspec(ctm) uint32_t *tile_indices, uint16_t tile_hit_count, __declspec(cls) struct rl_config *cfg, tile_t *act_list) {
	#define _NUM_U64S_TO_READ (4)
	#define _TILES_TO_READ (_NUM_U64S_TO_READ * TILES_IN_U64)

	__declspec(xfer_read_reg) tile_t read_words[_TILES_TO_READ];
	__declspec(xfer_read_write_reg) uint32_t xfer_pref = 0;
	enum tile_location loc = TILE_LOCATION_T1;
	uint16_t i = 0;
	uint16_t j = 0;
	uint16_t act_count = cfg->num_actions;

	uint8_t read_word_index = 0;

	for (i = 0; i < tile_hit_count; i++) {
		uint32_t target = tile_indices[i];
		uint32_t loc_base = cfg->last_tier_tile[loc];
		uint32_t base;

		while (target >= loc_base) {
			loc++;
			loc_base = cfg->last_tier_tile[loc];
		}

		// find location of tier in memory.
		switch (loc) {
			case TILE_LOCATION_T1:
				base = target;
				break;
			case TILE_LOCATION_T2:
				base = target - cfg->last_tier_tile[loc-1];
				break;
			case TILE_LOCATION_T3:
				base = target - cfg->last_tier_tile[loc-1];
				break;
		}

		while (j < act_count) {
			if (j == 0 || read_word_index >= _TILES_TO_READ) {
				switch (loc) {
					case TILE_LOCATION_T1:
						cls_read(&read_words[0], &t1_tiles[base + j], _NUM_U64S_TO_READ * sizeof(uint64_t));
						break;
					case TILE_LOCATION_T2:
						mem_read64(&read_words[0], &t2_tiles[base + j], _NUM_U64S_TO_READ * sizeof(uint64_t));
						break;
					case TILE_LOCATION_T3:
						mem_read64(&read_words[0], &t3_tiles[base + j], _NUM_U64S_TO_READ * sizeof(uint64_t));
						break;
				}
				read_word_index = 0;
			}

			//old-but-new way
			for (
				read_word_index = 0;
				read_word_index < _TILES_TO_READ && j < act_count;
				read_word_index++, j++
			) {
				act_list[j] += read_words[read_word_index];
			}
		}
	}

	#undef _TILES_TO_READ
	#undef _NUM_U64S_TO_READ
}

// This is guaranteed to be sorted!
__intrinsic void update_action_preferences(__addr40 _declspec(emem) uint32_t *tile_indices, uint16_t tile_hit_count, __declspec(cls) struct rl_config *cfg, uint16_t action, tile_t delta) {
	enum tile_location loc = TILE_LOCATION_T1;
	uint16_t i = 0;
	uint16_t j = 0;
	uint16_t act_count = cfg->num_actions;

	for (i = 0; i < tile_hit_count; i++) {
		uint32_t target = tile_indices[i];
		uint32_t loc_base = cfg->last_tier_tile[loc];
		uint32_t base;

		while (target >= loc_base) {
			loc++;
			loc_base = cfg->last_tier_tile[loc];
		}

		// find location of tier in memory.
		switch (loc) {
			case TILE_LOCATION_T1:
				base = target;
				t1_tiles[base + action] += delta;
				break;
			case TILE_LOCATION_T2:
				base = target - cfg->last_tier_tile[loc-1];
				t2_tiles[base + action] += delta;
				break;
			case TILE_LOCATION_T3:
				base = target - cfg->last_tier_tile[loc-1];
				t3_tiles[base + action] += delta;
				break;
		}
	}
}

// Selects the state-action / reward key from a given state vector.
_intrinsic tile_t select_key(struct key_source key_src, tile_t *state_vec) {
	switch(key_src.kind) {
		case KEY_SRC_SHARED:
			break;
		case KEY_SRC_FIELD:
			return state_vec[key_src.body.field_id];
		case KEY_SRC_VALUE:
			return key_src.body.value;
	}

	// Assumes shared in usual case.
	return 0;
}

_intrinsic tile_t fat_select_key(struct key_source key_src, __addr40 __declspec(emem) tile_t *state_vec) {
	switch(key_src.kind) {
		case KEY_SRC_SHARED:
			break;
		case KEY_SRC_FIELD:
			return state_vec[key_src.body.field_id];
		case KEY_SRC_VALUE:
			return key_src.body.value;
	}

	// Assumes shared in usual case.
	return 0;
}
