#include <stdint.h>
#include <nfp/cls.h>
#include "../worker_config.h"
#include "../subtask/work.h"
#include "../subtask/ack.h"
#include "../rl.h"

#ifdef _RL_WORKER_DISABLED

void work(uint8_t is_master, unsigned int parent_sig) {}

#else

#ifndef NO_FORWARD
#include "../subtask/external_writeback.h"
#include "../policy/policy_mem.h"
#endif

#if defined(NO_FORWARD) || (defined(IN_PORT) && defined(OUT_PORT))

__intrinsic uint32_t tile_code_with_cfg_single(
	__addr40 _declspec(emem) tile_t *state,
	__addr40 _declspec(emem) struct rl_config *cfg,
	uint8_t bias_tile_exists,
	uint8_t work_idx
) {
	uint16_t dim_idx;
	uint16_t tiling_set_idx;
	uint32_t local_tile;
	uint16_t tiling_idx;
	uint32_t width_product = cfg->num_actions;

	// check if there's a bias tile.
	// => yes? set 0 has size 1, others have size cfg->tilings_per_set
	// => no? all have size cfg->tilings_per_set
	if (bias_tile_exists && work_idx == 0) {
		return 0;
	}

	tiling_set_idx = bias_tile_exists
		? (((work_idx - 1) / cfg->tilings_per_set) + 1)
		: (work_idx / cfg->tilings_per_set);

	// can also get by remul'ing
	local_tile = cfg->tiling_sets[tiling_set_idx].start_tile;
	tiling_idx = work_idx - local_tile;

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

		val = (val > min) ? val - min : 0;

		val /= cfg->width[active_dim];

		local_tile += width_product * ((uint32_t)val - (reduce_tile ? 1 : 0));
		width_product *= cfg->tiles_per_dim;
	}

	return local_tile;
}

__intrinsic void action_preferences_with_cfg_single(uint32_t tile_index, __addr40 _declspec(emem) struct rl_config *cfg) {
	__declspec(xfer_read_write_reg) uint32_t xfer_pref = 0;
	enum tile_location loc = TILE_LOCATION_T1;
	uint16_t j = 0;
	uint16_t act_count = cfg->num_actions;
	uint32_t loc_base = cfg->last_tier_tile[loc];
	uint32_t base;

	while (tile_index >= loc_base) {
		loc++;
		loc_base = cfg->last_tier_tile[loc];
	}

	// atomic_writeback_prefs

	// find location of tier in memory.
	switch (loc) {
		case TILE_LOCATION_T1:
			base = tile_index;
			for (j = 0; j < act_count; j++) {
				xfer_pref = t1_tiles[base + j];
				cls_test_add(&xfer_pref, &(atomic_writeback_prefs[j]), sizeof(uint32_t));
			}
			break;
		case TILE_LOCATION_T2:
			base = tile_index - cfg->last_tier_tile[loc-1];
			for (j = 0; j < act_count; j++) {
				xfer_pref = t2_tiles[base + j];
				cls_test_add(&xfer_pref, &(atomic_writeback_prefs[j]), sizeof(uint32_t));
			}
			break;
		case TILE_LOCATION_T3:
			base = tile_index - cfg->last_tier_tile[loc-1];
			for (j = 0; j < act_count; j++) {
				xfer_pref = t3_tiles[base + j];
				cls_test_add(&xfer_pref, &(atomic_writeback_prefs[j]), sizeof(uint32_t));
			}
			break;
	}
}

__intrinsic void update_action_preferences_with_cfg(uint32_t *tile_indices, uint16_t tile_hit_count, __addr40 _declspec(emem) struct rl_config *cfg, uint16_t action, tile_t delta) {
	enum tile_location loc = TILE_LOCATION_T1;
	uint16_t i = 0;

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

__intrinsic void update_action_preference_with_cfg_single(uint32_t tile_index, __addr40 _declspec(emem) struct rl_config *cfg, uint16_t action, tile_t delta) {
	enum tile_location loc = TILE_LOCATION_T1;

	uint32_t target = tile_index;
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

volatile __declspec(shared) struct work local_ctx_work = {0};

__intrinsic void compute_my_work_alloc(
	uint8_t my_id,
	uint8_t allocs_with_spill,
	uint8_t num_work_items,
	uint8_t worker_ct,
	uint8_t my_work_alloc_size,
	uint16_t *work_idxes
) {
	uint8_t iter = 0;
	uint32_t scratch = 0;
	switch (local_ctx_work.body.alloc.strat) {
		case ALLOC_CHUNK:
			// not too expensive to be dumb here.
			// i.e., not in a hot compute loop

			if (my_id >= allocs_with_spill) {
				scratch = allocs_with_spill * ((num_work_items / worker_ct) + 1);
				scratch += (my_id - allocs_with_spill) * (num_work_items / worker_ct);
			} else {
				scratch = my_id * ((num_work_items / worker_ct) + 1);
			}

			// scratch is the base of our block.
			for (iter = 0; iter < my_work_alloc_size; iter++) {
				work_idxes[iter] = local_ctx_work.body.alloc.work_indices[scratch + iter];
			}
			break;
		case ALLOC_STRIDE_OFFSET:
			// might have *some* saturation...
			// think about revisiting me, or finding a proof/counterexample.
			scratch = my_id % (num_work_items / worker_ct);
		case ALLOC_STRIDE:
			// scratch is the number to add to alloc.
			iter = 0;
			scratch *= worker_ct;
			scratch += my_id;

			// equal to (id + i * worker_cnt), using own id as an offset into this sequence.
			while (iter < my_work_alloc_size) {
				if (scratch > num_work_items) {
					scratch = my_id;
				}

				work_idxes[iter] = local_ctx_work.body.alloc.work_indices[scratch];

				scratch += worker_ct;

				iter++;
			}

			break;
	}
}

__declspec(shared imem) uint64_t iter_ct = 0;

void work(uint8_t is_master, unsigned int parent_sig) {

	__addr40 _declspec(emem) struct rl_config *cfg;

	__declspec(emem) volatile struct value_set local_prefs[WORKER_LOCAL_PREF_SLOTS] = {0};
	uint8_t active_pref_space = 0;

	uint32_t worker_ct = 0;
	uint32_t base_worker_ct = 0;

	uint32_t my_id = 0;
	uint8_t my_slot = 0;

	uint32_t scratch = 0;
	enum writeback_result wb = WB_LOCKED;

	// Must be at least 2 workers for this to be not-dumb.
	uint8_t my_work_alloc_size = 0;
	uint8_t allocs_with_spill = 0;
	uint8_t has_bias = 0;
	uint8_t iter = 0;
	uint16_t work_idxes[RL_MAX_TILE_HITS/2] = {0};
	uint32_t tile_hits[RL_MAX_TILE_HITS/2] = {0};

	#ifndef NO_FORWARD
	__assign_relative_register(&worker_in_sig, WORKER_SIGNUM);
	#endif

	while (1) {
		int should_writeback = 0;
		int should_clear_prefs = 0;

		#ifdef NO_FORWARD
		wait_for_all(&internal_handout_sig);
		#else
		wait_for_all(&worker_in_sig);
		#endif

		#ifndef NO_FORWARD
		wb = WB_LOCKED;

		// pass message along the chain if we're a master ctx.
		if (is_master) {
			int i = 1;

			#ifndef CAP
			OUT_PORT.type = IN_PORT.type;
			#endif /* !CAP */
			
			switch (IN_PORT.type) {
				case WORK_REQUEST_WORKER_COUNT:
					base_worker_ct = IN_PORT.body.worker_count;

					#ifndef CAP

					OUT_PORT.body.worker_count = base_worker_ct + __n_ctx();

					#else

					cap_work.type = IN_PORT.type;
					cap_work.body.worker_count = base_worker_ct + __n_ctx();

					atomic_writeback_prefs[0] = base_worker_ct + __n_ctx();

					should_writeback = 1;

					#endif /* !CAP */

					local_ctx_work.body.worker_count = base_worker_ct;

					break;
				default:
					#ifndef CAP
					OUT_PORT.body = IN_PORT.body;
					#endif /* !CAP */
					local_ctx_work = IN_PORT;
					break;		
			}

			#ifndef CAP
			local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (WORKER_SIGNUM << 3));
			#endif /* !CAP */

			// awaken children.
			for (i=1; i<8; i++) {
				signal_ctx(i, WORKER_SIGNUM);
			}
		}
		#endif /* !NO_FORWARD */

		// Standard processing.
		switch (local_ctx_work.type) {
			case WORK_REQUEST_WORKER_COUNT:
				if (!is_master) {
					base_worker_ct = local_ctx_work.body.worker_count;
				}
				break;
			case WORK_SET_WORKER_COUNT:
				worker_ct = local_ctx_work.body.worker_count;

				#ifndef NO_FORWARD
				my_id = base_worker_ct + __ctx();

				#else
				my_id = __ctx() - 1;

				#endif /* !NO_FORWARD */

				my_slot = my_id % RL_WORKER_WRITEBACK_SLOTS;

				should_writeback = 1;

				break;
			// case WORK_SET_BASE_WORKER_COUNT:
			// 	base_worker_ct = local_ctx_work.body.worker_count;
			// 	break;
			case WORK_NEW_CONFIG:
				cfg = local_ctx_work.body.cfg;

				should_writeback = 1;

				// spill.
				scratch = cfg->num_work_items % worker_ct;

				my_work_alloc_size = (cfg->num_work_items / worker_ct);
				allocs_with_spill = scratch;
				if (my_id < scratch) {
					my_work_alloc_size += 1;
				}

				has_bias = cfg->tiling_sets[0].num_dims == 0;

				break;
			case WORK_STATE_VECTOR:
				__critical_path(100);
				// "active_pref_space"
				//
				// for each id in work list:
				//  tile_code_with_cfg_single(local_ctx_work.state, cfg, has_bias, id);
				for (iter=0; iter < my_work_alloc_size; ++iter) {
					uint32_t hit_tile = tile_code_with_cfg_single(local_ctx_work.body.state, cfg, has_bias, work_idxes[iter]);
					// place tile into slot governed by active_pref_space
					action_preferences_with_cfg_single(hit_tile, cfg);
				}

				should_writeback = 1;
				break;
			case WORK_ALLOCATE:
				compute_my_work_alloc(
					my_id,
					allocs_with_spill,
					cfg->num_work_items,
					worker_ct,
					my_work_alloc_size,
					work_idxes
				);
				should_writeback = 1;
				break;
			case WORK_UPDATE_POLICY:
				__critical_path(90);
				for (iter=0; iter < my_work_alloc_size; ++iter) {
					uint32_t hit_tile = tile_code_with_cfg_single(local_ctx_work.body.update.state, cfg, has_bias, work_idxes[iter]);
					update_action_preference_with_cfg_single(
						hit_tile, 
						cfg,
						local_ctx_work.body.update.action,
						local_ctx_work.body.update.delta
					);
				}

				should_writeback = 1;
				break;
			default:
				__impossible_path();
		}

		if (should_writeback) {
			atomic_ack();
		}
	}
}

#else

#error "IN_PORT or OUT_PORT undefined!"

#endif /* IN_PORT */

#endif /* _RL_WORKER_DISABLED */
