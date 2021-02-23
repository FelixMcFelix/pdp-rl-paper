#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <rtl.h>
#include <lu/cam_hash.h>
#include "rl.h"
#include "rl-pkt-store.h"
#include "pif_parrep.h"

#include "subtask/ack.h"
#include "subtask/work.h"
#include "quant_math.h"
#include "worker_config.h"

__declspec(i5.cls, export)tile_t t1_tiles[MAX_CLS_TILES] = {0};
__declspec(i5.ctm, export)tile_t t2_tiles[MAX_CTM_TILES] = {0};
__declspec(export imem)tile_t t3_tiles[MAX_IMEM_TILES] = {0};

// Maybe keep one of these locally, too?
// probably should do this holy shit.
__declspec(export, emem) struct rl_config cfg = {0};

// ring head and tail on i25 or emem1
#define RL_RING_NUMBER 129
volatile __emem_n(0) __declspec(export, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t rl_mem_workq[512*1024] = {0};
_NFP_CHIPRES_ASM(.alloc_resource rl_mem_workq_rnum emem0_queues+RL_RING_NUMBER global 1)
//_NFP_CHIPRES_ASM(.init_mu_ring rl_mem_workq_rnum rl_mem_workq)

__declspec(export, emem) struct rl_pkt_store rl_pkts;
volatile __declspec(export, emem, addr40, aligned(sizeof(unsigned int))) uint8_t inpkt_buffer[RL_PKT_MAX_SZ * RL_PKT_STORE_COUNT] = {0};

volatile __declspec(export, emem) uint64_t really_really_bad = 0;

volatile __declspec(export, emem) uint32_t cycle_estimate = 0;

__declspec(export, emem) uint32_t global_tc_indices[RL_MAX_TILE_HITS] = {0};
__declspec(export, emem) uint16_t global_tc_count;

__declspec(export, emem) tile_t global_prefs[MAX_ACTIONS] = {0};

__declspec(export, emem) struct rl_work_item test_work;

// Testing out CAM table creation here.

/*#define SA_CAM_BUCKETS 0x40000
#define SA_TABLE_SZ (SA_CAM_BUCKETS * 16)

__export __emem __align(SA_TABLE_SZ) struct mem_lkup_cam32_16B_table_bucket_entry lkup_table[SA_CAM_BUCKETS];*/

#define CAMHT_LOOKUP_IDX_ADD(_name, _key, added)                            \
    camht_lookup_idx_add(CAMHT_HASH_TBL(_name), CAMHT_NB_ENTRIES(_name),    \
                     (void *)_key, sizeof(*_key), added)

// Need to use this due to size limits on struct in CAMHT def, even
// if key is different.
// #define SA_ENTRIES 0x20000
#define SA_ENTRIES 0x10
CAMHT_DECLARE(state_action_map, SA_ENTRIES, uint64_t)

__declspec(emem) struct state_action_pair state_action_pairs[SA_ENTRIES];

#define REWARD_ENTRIES 0x10
CAMHT_DECLARE(reward_map, REWARD_ENTRIES, union pad_tile)

#ifndef _RL_WORKER_DISABLED
__declspec(nn_remote_reg) struct work in_type_a = {0};
#endif /* !_RL_WORKER_DISABLED */

#ifndef _RL_CORE_OLD_POLICY_WORK
// __declspec(visible read_reg) struct worker_ack reflector_writeback[RL_WORKER_WRITEBACK_SLOTS];
__declspec(visible read_reg) struct worker_ack reflector_writeback[RL_WORKER_WRITEBACK_SLOTS];
// __declspec(visible xfer_read_reg) struct worker_ack reflector_writeback;
__declspec(visible) SIGNAL reflector_writeback_sig;
__declspec(export, emem) uint32_t reflector_writeback_locks[RL_WORKER_WRITEBACK_SLOTS] = {0};
#endif /* _RL_CORE_OLD_POLICY_WORK */

/** Convert a state vector into a list of tile indices.
*
* Returns length of tile coded list.
*/
uint16_t tile_code(tile_t *state, __addr40 _declspec(emem) struct rl_config *cfg, uint32_t *output) {
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

				val = (val > min) ? val - min : 0;

				val /= cfg->width[active_dim];

				local_tile += width_product * ((uint32_t)val - (reduce_tile ? 1 : 0));
				width_product *= cfg->tiles_per_dim;
			}

			output[out_idx++] = local_tile;
			local_tile_base += cfg->tiling_sets[tiling_set_idx].tiling_size / cfg->tilings_per_set;
		}
	}
	return out_idx;
}

/** Convert a tile list into a list of action preferences.
*
* Length written into act_list is guaranteed to be equal to cfg->num_actions.
* Note, that act_list must be at least of size MAX_ACTIONS.
*/
void action_preferences(uint32_t *tile_indices, uint16_t tile_hit_count, __addr40 _declspec(emem) struct rl_config *cfg, tile_t *act_list) {
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
				for (j = 0; j < act_count; j++) {
					act_list[j] += t1_tiles[base + j];
				}
				break;
			case TILE_LOCATION_T2:
				base = target - cfg->last_tier_tile[loc-1];
				for (j = 0; j < act_count; j++) {
					act_list[j] += t2_tiles[base + j];
				}
				break;
			case TILE_LOCATION_T3:
				base = target - cfg->last_tier_tile[loc-1];
				for (j = 0; j < act_count; j++) {
					act_list[j] += t3_tiles[base + j];
				}
				break;
		}
	}
}

void update_action_preferences(uint32_t *tile_indices, uint16_t tile_hit_count, __addr40 _declspec(emem) struct rl_config *cfg, uint16_t action, tile_t delta) {
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
tile_t select_key(struct key_source key_src, tile_t *state_vec) {
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

void state_packet(__addr40 _declspec(emem) struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt, uint16_t dim_count) {
	uint32_t tc_indices[RL_MAX_TILE_HITS] = {0};
	uint16_t tc_count;
	uint16_t chosen_action;
	uint32_t rng_draw;

	tile_t state[RL_DIMENSION_MAX];

	tile_t prefs[MAX_ACTIONS] = {0};

	uint32_t t0;
	uint32_t t1;

	__declspec(xfer_write_reg) uint32_t time_taken;
	__declspec(xfer_write_reg) uint64_t nani;

	__declspec(xfer_read_reg) union two_u16s word;
	int i = 0;

	//tile_t state_key = select_key(cfg->state_key, state);
	//tile_t reward_key = select_key(cfg->reward_key, state);

	t0 = local_csr_read(local_csr_timestamp_low);

	if (dim_count != cfg->num_dims) {
		// nani = (dim_count << 16) | cfg->num_dims;
		// mem_write64(&nani, &really_really_bad, sizeof(uint64_t));
		return;
	}

	while (i != dim_count) {
		mem_read32(
			&(word.raw),
			pkt->packet_payload + (i * sizeof(union two_u16s)),
			sizeof(union two_u16s)
		);
		state[i] = (tile_t)word.raw;
		i++;
	}

	tc_count = tile_code(state, cfg, tc_indices);
	global_tc_count = tc_count;

	for (i=0; i<tc_count; i++) {
		global_tc_indices[i] = tc_indices[i];
	}

	action_preferences(tc_indices, tc_count, cfg, prefs);
	
	// choose action
	rng_draw = local_csr_read(local_csr_pseudo_random_number);
	if ((rng_draw % (1 << cfg->quantiser_shift)) <= cfg->epsilon) {
		// Choose random
		// This is probably subtly biased... whatever
		chosen_action = local_csr_read(local_csr_pseudo_random_number) % cfg->num_actions;
	} else {
		// Choose maximum.
		chosen_action = 0;
		for (i=0; i<cfg->num_actions; i++) {
			if (prefs[i] > prefs[chosen_action]) {
				chosen_action = i;
			}
		}
	}

	// reduce epsilon as required.
	// realisation: can reinduce training without needing policy re-insertion!
	if (cfg->epsilon > 0) {
		cfg->epsilon_decay_freq_cnt += 1;
		if (cfg->epsilon_decay_freq_cnt >= cfg->epsilon_decay_freq) {
			cfg->epsilon_decay_freq_cnt = 0;
			cfg->epsilon -= cfg->epsilon_decay_amt;
		}
	}

	if (cfg->do_updates) {
		// This dummies the access cost.
		// For some reason, it bugs out if struct size is lte 32bit?
		uint64_t reward_key = select_key(cfg->reward_key, state);
		uint64_t state_key = select_key(cfg->state_key, state);

		int32_t state_added = 0;

		int32_t reward_found = CAMHT_LOOKUP(reward_map, &reward_key);

		// This is dummied until I figure out the mechanism better.
		int32_t state_found = CAMHT_LOOKUP_IDX_ADD(state_action_map, &state_key, &state_added);
		uint32_t changed_key = 0;

		if (state_action_map_key_tbl[state_found] != state_key) {
			state_action_map_key_tbl[state_found] = state_key;
			changed_key = 1;
		}


		//nani = (((uint64_t) reward_found) << 32) | state_added;
		//mem_write64(&nani, &really_really_bad, sizeof(uint64_t));
		if ((!(state_added || changed_key)) && state_found >= 0 && reward_found >= 0) {
			tile_t matched_reward = reward_map_key_tbl[reward_found].data;
			struct state_action_pair lsap = state_action_pairs[state_found];

			tile_t value_of_chosen_action = prefs[chosen_action];

			tile_t adjustment = cfg->alpha;
			tile_t dt;

			lsap.action = chosen_action;
			lsap.val = value_of_chosen_action;
			// don't copy the tile list, that's probably TOO heavy.
		
			dt = matched_reward + quant_mul(cfg->alpha, value_of_chosen_action, cfg->quantiser_shift) - lsap.val;

			adjustment = quant_mul(adjustment, dt, cfg->quantiser_shift);

			// tc_indices, tc_count, cfg, prefs
			update_action_preferences(tc_indices, tc_count, cfg, chosen_action, adjustment);
		}

		// TODO: figure out "broken-math" version of this (indiv tile shifts)

		// store the tile list, chosen action, and its value.
		state_action_pairs[state_found].action = chosen_action;
		state_action_pairs[state_found].val = prefs[chosen_action];
		state_action_pairs[state_found].len = tc_count;

		// dst, src, size
		ua_memcpy_mem40_mem40(
			&(state_action_pairs[state_found].tiles), 0,
			(void*)tc_indices, 0,
			tc_count * sizeof(tile_t)
		);
	}

	// NOTE: alpha, gamma, policy must all have same base!

	t1 = local_csr_read(local_csr_timestamp_low);

	time_taken = t1 - t0;

	mem_write32(&time_taken, &cycle_estimate, sizeof(uint32_t));

	i=0;
	for (i=0; i < cfg->num_actions; i++) {
		global_prefs[i] = prefs[i];
	}
}
