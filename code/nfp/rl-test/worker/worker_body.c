#include <stdint.h>
#include <nfp/cls.h>
#include <nfp/mem_bulk.h>
#include "../worker_config.h"
#include "../subtask/work.h"
#include "../subtask/ack.h"
#include "../rl.h"
#include "../util.h"

#include <nfp6000/nfp_me.h>
#include <assert.h>

#ifdef _RL_WORKER_DISABLED

void work(uint8_t is_master, unsigned int parent_sig) {}

#else

#ifndef NO_FORWARD
#include "../subtask/external_writeback.h"
#include "../policy/policy_mem.h"
#endif

#if defined(NO_FORWARD) || (defined(IN_PORT) && defined(OUT_PORT))

__declspec(export, emem) tile_t life_sucks[4] = {0};

uint32_t tile_code_with_cfg_single(
	__addr40 _declspec(emem) tile_t *state,
	__addr40 _declspec(ctm) struct rl_config *cfg,
	uint8_t bias_tile_exists,
	uint8_t work_idx
) {
	uint16_t dim_idx;
	uint16_t tiling_set_idx;
	uint32_t local_tile;
	uint16_t tiling_idx;
	uint32_t width_product = cfg->num_actions;

	tile_t emergency_vals[4] = {0};

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
	tiling_idx = work_idx - (bias_tile_exists
		? 1 + ((tiling_set_idx-1) * cfg->tilings_per_set)
		: tiling_set_idx * cfg->tilings_per_set);
		// work_idx - local_tile;

	local_tile += tiling_idx * cfg->tiling_sets[tiling_set_idx].tiling_size;

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

	#ifdef RL_DEBUG_ASSERTS
	if (local_tile < cfg->tiling_sets[tiling_set_idx].start_tile || local_tile >= cfg->tiling_sets[tiling_set_idx].end_tile) {
		life_sucks[0] = emergency_vals[0];
		life_sucks[1] = emergency_vals[1];
		life_sucks[2] = emergency_vals[2];
		life_sucks[3] = emergency_vals[3];
		__implicit_read(emergency_vals);
		__asm{ctx_arb[bpt]}
		__implicit_read(emergency_vals);
	}
	#endif

	__implicit_read(emergency_vals);

	return local_tile;
}

// COPIED FROM ELSE
#define _CLS_CMD(cmdname, data, addr, size, max_size, sync, sig, cmin, cmax) \
do {                                                                    \
    struct nfp_mecsr_prev_alu ind;                                      \
    unsigned int count = (size >> 2);                                   \
    unsigned int max_count = (max_size >> 2);                           \
                                                                        \
    ctassert(__is_ct_const(max_size));                                  \
    ctassert(__is_ct_const(sync));                                      \
    ctassert(sync == sig_done || sync == ctx_swap);                     \
                                                                        \
    if (__is_ct_const(size)) {                                          \
        ctassert(size != 0);                                            \
        ctassert(__is_aligned(size, 4));                                \
                                                                        \
        if(size <= (cmax * 4)) {                                        \
            if (sync == sig_done) {                                     \
                __asm { cls[cmdname, *data, addr, 0, __ct_const_val(count)], \
                        sig_done[*sig] }                                \
            } else {                                                    \
                __asm { cls[cmdname, *data, addr, 0, __ct_const_val(count)], \
                        ctx_swap[*sig] }                                \
            }                                                           \
        } else {                                                        \
            ctassert(size <= 128);                                      \
                                                                        \
            /* Setup length in PrevAlu for the indirect */              \
            ind.__raw = 0;                                              \
            ind.ov_len = 1;                                             \
            ind.length = count - 1;                                     \
                                                                        \
            if (sync == sig_done) {                                     \
                __asm { alu[--, --, B, ind.__raw] }                     \
                __asm { cls[cmdname, *data, addr, 0, __ct_const_val(count)], \
                        sig_done[*sig], indirect_ref }                  \
            } else {                                                    \
                __asm { alu[--, --, B, ind.__raw] }                     \
                __asm { cls[cmdname, *data, addr, 0, __ct_const_val(count)], \
                        ctx_swap[*sig], indirect_ref }                  \
            }                                                           \
        }                                                               \
    } else {                                                            \
        /* Setup length in PrevAlu for the indirect */                  \
        ind.__raw = 0;                                                  \
        ind.ov_len = 1;                                                 \
        ind.length = count - 1;                                         \
                                                \
        if (sync == sig_done) {                                         \
            __asm { alu[--, --, B, ind.__raw] }                         \
            __asm { cls[cmdname, *data, addr, 0, __ct_const_val(max_count)], \
                    sig_done[*sig], indirect_ref }                      \
        } else {                                                        \
            __asm { alu[--, --, B, ind] }                               \
            __asm { cls[cmdname, *data, addr, 0, __ct_const_val(max_count)], \
                    ctx_swap[*sig], indirect_ref }                      \
        }                                                               \
    }                                                                   \
} while (0)

__intrinsic void
__cls_test_add64(__xrw void *data, __cls void *addr, size_t size,
               const size_t max_size, sync_t sync, SIGNAL *sig)
{
    try_ctassert(size <= 64);

    _CLS_CMD(test_add64, data, addr, size, size, sync, sig, 1, 8);
}

__intrinsic void
cls_test_add64(__xrw void *data, __cls void *addr, size_t size)
{
    SIGNAL sig;

    __cls_test_add64(data, addr, size, size, ctx_swap, &sig);
}

__intrinsic void
__cls_test_sub64(__xrw void *data, __cls void *addr, size_t size,
               const size_t max_size, sync_t sync, SIGNAL *sig)
{
    try_ctassert(size <= 64);

    _CLS_CMD(test_add64, data, addr, size, size, sync, sig, 1, 8);
}

__intrinsic void
cls_test_sub64(__xrw void *data, __cls void *addr, size_t size)
{
    SIGNAL sig;

    __cls_test_sub64(data, addr, size, size, ctx_swap, &sig);
}

// END COPY

void action_preferences_with_cfg_single(
	uint32_t tile_index,
	__addr40 _declspec(ctm) struct rl_config *cfg,
	enum tile_location loc
) {
	#define _NUM_U64S_TO_READ (4)
	#define _TILES_TO_READ (_NUM_U64S_TO_READ * TILES_IN_U64)

	__declspec(xfer_read_reg) tile_t read_words[_TILES_TO_READ];
	__declspec(xfer_read_write_reg) uint32_t xfer_pref = 0;
	uint16_t j = 0;
	uint16_t act_count = cfg->num_actions;
	uint32_t loc_base = cfg->last_tier_tile[loc];
	uint32_t base;

	uint8_t read_word_index = 0;

	#ifdef WORKER_BARGAIN_BUCKET_SIMD
	__declspec(xfer_read_write_reg) uint64_t atomic_prep = 0;
	uint64_t atomic_prep_scratch = 0;
	uint8_t atom_write_word_index = 0;
	uint8_t simd_shift = 0;
	uint16_t writing_j = 0;
	#endif

	if (loc > TILE_LOCATION_T3) {
		loc = TILE_LOCATION_T1;
		loc_base = cfg->last_tier_tile[loc];
		while (tile_index >= loc_base) {
			loc++;
			loc_base = cfg->last_tier_tile[loc];
		}
	}

	// WORKER_BARGAIN_BUCKET_SIMD
	// atomic_writeback_prefs_simd

	// #define SIMD_THROUGHPUT 4
	// #define SIMD_SHIFT_PER 

	// __declspec(i5.cls, export)tile_t t1_tiles[MAX_CLS_TILES] = {0};
	// __declspec(i5.ctm, export)tile_t t2_tiles[MAX_CTM_TILES] = {0};
	// __declspec(export imem)tile_t t3_tiles[MAX_IMEM_TILES] = {0};

	// DON'T SWAP UNTIL RIGHT BEFORE ADD?
	// for cls cls_read(__xread void *data, __cls void *addr, size_t size);
	// for ctm/imem mem_read64_swap(__xread void *data, __mem40 void *addr, const size_t size);

	// find location of tier in memory.
	switch (loc) {
		case TILE_LOCATION_T1:
			base = tile_index;
			break;
		case TILE_LOCATION_T2:
			base = tile_index - cfg->last_tier_tile[loc-1];
			break;
		case TILE_LOCATION_T3:
			base = tile_index - cfg->last_tier_tile[loc-1];
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

		#ifdef WORKER_BARGAIN_BUCKET_SIMD
		//bb simd
		if (atom_write_word_index >= SIMD_THROUGHPUT) {
			atom_write_word_index = 0;
			atomic_prep_scratch = 0;
			simd_shift = 0;
		}

		while (atom_write_word_index < SIMD_THROUGHPUT || read_word_index < _TILES_TO_READ) {
			atomic_prep_scratch |= read_words[read_word_index] << simd_shift;

			simd_shift += SIMD_SHIFT_PER;

			j++;
			atom_write_word_index++;
			read_word_index++;
		}

		if (atom_write_word_index >= SIMD_THROUGHPUT || j == act_count) {
			uint32_t add_scratch = 0;
			atomic_prep = atomic_prep_scratch;
			// TODO: SWAP WORDS??
			// is writing j correct? shouldn't this be mod throughput?!
			cls_test_add64(&atomic_prep, &(atomic_writeback_prefs_simd[writing_j]), sizeof(uint64_t));

			#ifdef _32_BIT_TILES
			// add_scratch = atomic_prep + ((uint32_t) atomic_prep_scratch);

			// atomic_prep + atomic_prep_scratch
			// if carry set, then sub.

			__asm {
				// add atomic_prep, ((uint32_t) atomic_prep_scratch) w/ no dest
				// store 0 + carry into add_scratch
				alu[--, atomic_prep, +, atomic_prep_scratch];
				alu[add_scratch, add_scratch, +carry, 0];
			}

			if ((atomic_prep | 0x80000000 == 1)) {
				atomic_prep = 0x0000000100000000;
				cls_test_sub64(&atomic_prep, &(atomic_writeback_prefs_simd[writing_j]), sizeof(uint64_t));
			}
			#endif /* _32_BIT_TILES */

			writing_j++;
		}
		#else
		//old-but-new way
		for (
			read_word_index = 0;
			read_word_index < _TILES_TO_READ && j < act_count;
			read_word_index++, j++
		) {
			xfer_pref = read_words[read_word_index];
			cls_test_add(&xfer_pref, &(atomic_writeback_prefs[j]), sizeof(uint32_t));
		}
		#endif /* WORKER_BARGAIN_BUCKET_SIMD */
	}

	#undef _TILES_TO_READ
	#undef _NUM_U64S_TO_READ
}

__intrinsic void update_action_preferences_with_cfg(uint32_t *tile_indices, uint16_t tile_hit_count, __addr40 _declspec(ctm) struct rl_config *cfg, uint16_t action, tile_t delta) {
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

__intrinsic void update_action_preference_with_cfg_single(
	uint32_t tile_index,
	__addr40 _declspec(ctm) struct rl_config *cfg,
	uint16_t action,
	tile_t delta,
	enum tile_location loc
) {
	uint32_t target = tile_index;
	uint32_t loc_base = cfg->last_tier_tile[loc];
	uint32_t base;

	if (loc > TILE_LOCATION_T3) {
		loc = TILE_LOCATION_T1;
		while (target >= loc_base) {
			loc++;
			loc_base = cfg->last_tier_tile[loc];
		}
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
	uint8_t offset = 0;
	uint32_t scratch = 0;
	switch (local_ctx_work.body.alloc.strat) {
		case ALLOC_FILL_HEAVY:
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
			offset = my_id;
		case ALLOC_STRIDE:
			// scratch is the number to add to alloc.
			scratch = my_id;

			// equal to (id + i * worker_cnt), using own id as an offset into this sequence.
			while (iter < my_work_alloc_size) {
				work_idxes[(iter + offset) % my_work_alloc_size] = local_ctx_work.body.alloc.work_indices[scratch];

				scratch += worker_ct;

				iter++;
			}

			break;
	}
}

__declspec(shared imem) uint64_t iter_ct = 0;
/*__declspec(export emem) uint32_t write_space[RL_MAX_TILE_HITS] = {0};
__declspec(export emem) uint32_t other_write_space[31] = {0};
__declspec (export emem) uint32_t otherr_write_space[31] = {0};*/
__declspec (export emem) uint64_t nntf = 0;

void work(uint8_t is_master, unsigned int parent_sig) {
	__declspec(ctm) struct rl_config *cfg;

	uint8_t active_pref_space = 0;

	uint32_t true_worker_ct = 0;
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

	uint32_t last_work_t;
	uint32_t b_t0;
	uint32_t b_t1;

	enum tile_location precache_locs[RL_MAX_PRECACHE] = {0};

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
				true_worker_ct = local_ctx_work.body.worker_count;
				worker_ct = true_worker_ct;

				// worker_ct = local_ctx_work.body.worker_count;

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

				if (cfg->worker_limit == 0) {
					worker_ct = true_worker_ct;
				} else {
					worker_ct = (cfg->worker_limit < true_worker_ct)
						? cfg->worker_limit
						: true_worker_ct;
				}

				if (my_id >= worker_ct) {
					my_work_alloc_size = 0;
				} else {
					// spill.
					scratch = cfg->num_work_items % worker_ct;

					my_work_alloc_size = (cfg->num_work_items / worker_ct);
					allocs_with_spill = scratch;
					if (my_id < scratch) {
						my_work_alloc_size += 1;
					}

					has_bias = cfg->tiling_sets[0].num_dims == 0;
				}

				break;
			case WORK_STATE_VECTOR:
				__critical_path(100);
				// "active_pref_space"
				//
				// for each id in work list:
				//  tile_code_with_cfg_single(local_ctx_work.state, cfg, has_bias, id);
				// b_t0 = local_csr_read(local_csr_timestamp_low);
				for (iter=0; iter < my_work_alloc_size; ++iter) {
					enum tile_location loc = (iter < RL_MAX_PRECACHE)
						? precache_locs[iter]
						: TILE_LOCATION_T3 + 1;
					// uint32_t t0 = local_csr_read(local_csr_timestamp_low);
					// uint32_t t1;
					uint32_t hit_tile = tile_code_with_cfg_single(local_ctx_work.body.state, cfg, has_bias, work_idxes[iter]);

					#ifdef RL_DEBUG_ASSERTS
					enum tile_location n_loc = TILE_LOCATION_T1;
					uint32_t loc_base = cfg->last_tier_tile[n_loc];
					while (hit_tile >= loc_base) {
						n_loc++;
						loc_base = cfg->last_tier_tile[n_loc];
					}


					if (n_loc != loc) {
						uint32_t tiling_set_idx = has_bias
							? (((work_idxes[iter] - 1) / cfg->tilings_per_set) + 1)
							: (work_idxes[iter] / cfg->tilings_per_set);

						nntf = (hit_tile << 16) | ((tiling_set_idx & 0xff) << 8) | (work_idxes[iter] & 0xff);
						__asm {ctx_arb[bpt]}
						nntf = (hit_tile << 16) | ((tiling_set_idx & 0xff) << 8) | (work_idxes[iter] & 0xff);
					}
					#endif

					// place tile into slot governed by active_pref_space
					action_preferences_with_cfg_single(hit_tile, cfg, loc);
					// t1 = local_csr_read(local_csr_timestamp_low);
					// write_space[work_idxes[iter]] = t1 - t0;
				}
				// b_t1 = local_csr_read(local_csr_timestamp_low);
				// other_write_space[my_id] = b_t1 - b_t0;

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

				for(iter=0; iter<my_work_alloc_size && iter<RL_MAX_PRECACHE; iter++) {
					enum tile_location loc = TILE_LOCATION_T1;
					uint32_t tiling_set_idx = has_bias
						? (((work_idxes[iter] - 1) / cfg->tilings_per_set) + 1)
						: (work_idxes[iter] / cfg->tilings_per_set);

					precache_locs[iter] = cfg->tiling_sets[tiling_set_idx].location;
				}

				// __asm {
				// 	ctx_arb[bpt]
				// }

				should_writeback = 1;
				break;
			case WORK_UPDATE_POLICY:
				__critical_path(90);
				// b_t0 = local_csr_read(local_csr_timestamp_low);
				for (iter=0; iter < my_work_alloc_size; ++iter) {
					enum tile_location loc = (iter < RL_MAX_PRECACHE)
						? precache_locs[iter]
						: TILE_LOCATION_T3 + 1;

					uint32_t hit_tile = tile_code_with_cfg_single(local_ctx_work.body.update.state, cfg, has_bias, work_idxes[iter]);
					update_action_preference_with_cfg_single(
						hit_tile, 
						cfg,
						local_ctx_work.body.update.action,
						local_ctx_work.body.update.delta,
						loc
					);
				}
				// b_t1 = local_csr_read(local_csr_timestamp_low);
				// otherr_write_space[my_id] = b_t1 - b_t0;

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
