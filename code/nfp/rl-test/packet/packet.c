#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
//#include <nfp_mem_ring.h>
//#include <nfp_mem_workq.h>
#include <nfp/mem_bulk.h>
//#include <nfp/mem_lkup.h>
#include <nfp/mem_ring.h>
#include <rtl.h>
#include <lu/cam_hash.h>
#include "../util.h"
#include "../rl.h"
#include "../rl-pkt-store.h"
#include "../pif_parrep.h"

#include "../subtask/ack.h"
#include "../subtask/work.h"
#include "../quant_math.h"
#include "../worker_config.h"

volatile __declspec(export, emem) uint64_t really_really_bad_p = 0;

volatile __declspec(export, emem) uint32_t cycle_estimate = 0;

__declspec(export, emem) uint32_t global_tc_indices[RL_MAX_TILE_HITS] = {0};
__declspec(export, emem) uint16_t global_tc_count;

__declspec(export, emem) tile_t global_prefs[MAX_ACTIONS] = {0};


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
// #define SA_ENTRIES 0x10
// CAMHT_DECLARE(state_action_map, SA_ENTRIES, uint64_t)

// __declspec(emem) struct state_action_pair state_action_pairs[SA_ENTRIES];

// #define REWARD_ENTRIES 0x10
// CAMHT_DECLARE(reward_map, REWARD_ENTRIES, union pad_tile)

void setup_packet(__declspec(cls) struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt) {
	int dim;
	int cursor = 0;
	int i = 0;
	__declspec(xfer_read_reg) union two_u16s word;
	__declspec(xfer_read_reg) union four_u16s bigword;

	// setup information.
	// rest of packet body is:
	// do_updates (u8)
	// quantiser_shift (u8)
	// n_dims in all state vectors (u16)
	// tiles_per_dim (u16)
	// tilings_per_set (u16)
	// n_actions (u16)
	// epsilon (tile_t)
	// alpha (tile_t)
	// gamma (tile_t)
	// epsilon_d (tile_t)
	// epsilon_f (uint32_t)
	// * state_key (u8 + i32)
	// * reward_key (u8 + i32)
	mem_read32(
		&(word.raw),
		pkt->packet_payload,
		sizeof(union two_u16s)
	);
	cursor += sizeof(union two_u16s);

	cfg->do_updates = word.bytes[0] & (1 << 0);
	cfg->disable_action_writeout = word.bytes[0] & (1 << 1);
	cfg->force_update_to_happen = word.bytes[0] >> 4;
	cfg->quantiser_shift = word.bytes[1];

	cfg->worker_limit = word.ints[1];

	mem_read64(
		&(bigword.raw),
		pkt->packet_payload + cursor,
		sizeof(union four_u16s)
	);
	cursor += sizeof(union four_u16s);

	cfg->num_dims = bigword.shorts[0];
	cfg->tiles_per_dim = bigword.shorts[1];
	cfg->tilings_per_set = bigword.shorts[2];
	cfg->num_actions = bigword.shorts[3];

	mem_read32(
		&(word.raw),
		pkt->packet_payload + cursor,
		sizeof(union two_u16s)
	);
	cursor += sizeof(tile_t);

	cfg->epsilon = word.tiles[0];

	mem_read32(
		&(word.raw),
		pkt->packet_payload + cursor,
		sizeof(union two_u16s)
	);
	cursor += sizeof(tile_t);

	cfg->alpha = word.tiles[0];

	mem_read32(
		&(word.raw),
		pkt->packet_payload + cursor,
		sizeof(union two_u16s)
	);
	cursor += sizeof(tile_t);

	cfg->gamma = word.tiles[0];

	mem_read32(
		&(word.raw),
		pkt->packet_payload + cursor,
		sizeof(union two_u16s)
	);
	cursor += sizeof(tile_t);

	cfg->epsilon_decay_amt = word.tiles[0];

	mem_read32(
		&(word.raw),
		pkt->packet_payload + cursor,
		sizeof(union two_u16s)
	);
	cursor += sizeof(union two_u16s);

	cfg->epsilon_decay_freq = word.raw;

	cfg->state_key.kind = pkt->packet_payload[cursor];
	cursor += 1;
	mem_read32(
		&(word.raw),
		pkt->packet_payload + cursor,
		sizeof(union two_u16s)
	);
	cursor += sizeof(union two_u16s);
	switch(cfg->state_key.kind) {
		case KEY_SRC_FIELD:
			cfg->state_key.body.field_id = word.ints[1];
			break;
		case KEY_SRC_VALUE:
			cfg->state_key.body.value = (int32_t) word.raw;
			break;
		default:
			break;
	}

	cfg->reward_key.kind = pkt->packet_payload[cursor];
	cursor += 1;
	mem_read32(
		&(word.raw),
		pkt->packet_payload + cursor,
		sizeof(union two_u16s)
	);
	cursor += sizeof(union two_u16s);
	switch(cfg->reward_key.kind) {
		case KEY_SRC_FIELD:
			cfg->reward_key.body.field_id = word.ints[1];
			break;
		case KEY_SRC_VALUE:
			cfg->reward_key.body.value = (int32_t) word.raw;
			break;
		default:
			break;
	}

	// n x tile_t maxes
	// n x tile_t mins
	// FIXME: BROKEN BY CLS
	// ua_memcpy_mem40_mem40(
	// 	(__addr40 void*) &(cfg->maxes), 0,
	// 	pkt->packet_payload + cursor, 0,
	// 	cfg->num_dims * sizeof(tile_t)
	// );
	for (i=0; i<cfg->num_dims; i++) {
		cfg->maxes[i] = ((__declspec(emem) __addr40 tile_t *) (pkt->packet_payload + cursor))[i];
	}

	cursor += cfg->num_dims * sizeof(tile_t);

	// FIXME: BROKEN BY CLS
	// ua_memcpy_mem40_mem40(
	// 	(__addr40 void*) &(cfg->mins), 0,
	// 	pkt->packet_payload + cursor, 0,
	// 	cfg->num_dims * sizeof(tile_t)
	// );
	for (i=0; i<cfg->num_dims; i++) {
		cfg->mins[i] = ((__declspec(emem) __addr40 tile_t *) (pkt->packet_payload + cursor))[i];
	}

	cfg->epsilon_decay_freq_cnt = 0;

	// Fill in width, shift_amt, adjusted_max...
	if (cfg->tilings_per_set == 1) {
		// SPECIAL CASE:
		// do no cool shift/offset stuff.
		// map ONLY to the subtended space.
		// min max casing is already handled within.
		for (dim = 0; dim < cfg->num_dims; ++dim) {
			cfg->width[dim] = (cfg->maxes[dim] - cfg->mins[dim]) / (cfg->tiles_per_dim);
			cfg->shift_amt[dim] = 0;
			cfg->adjusted_maxes[dim] = cfg->maxes[dim];
		}
	} else {
		// Adds on an "extra tile" to the right of each dimension,
		// later tilings reach deeped
		for (dim = 0; dim < cfg->num_dims; ++dim) {
			cfg->width[dim] = (cfg->maxes[dim] - cfg->mins[dim]) / (cfg->tiles_per_dim - 1);
			cfg->shift_amt[dim] = cfg->width[dim] / cfg->tilings_per_set;
			cfg->adjusted_maxes[dim] = cfg->maxes[dim] + cfg->width[dim];
		}
	}
}

void tilings_packet(__declspec(cls) struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt) {
	// tiling information.
	// rest of packet body is, repeated till end:
	//  n_dims (u16)
	// then if n_dims != 0:
	//  tile_location
	//  n x u16 (dimensions in tiling)
	__declspec(xfer_read_reg) union two_u16s word;

	int i = 0;
	int cursor = 0;
	int num_tilings = 0;
	enum tile_location loc = TILE_LOCATION_T1;
	uint32_t inner_offset = 0;
	uint32_t current_start_tile = 0;
	uint16_t total_work_items = 0;

	while(cursor < pkt->packet_size) {
		uint32_t tiles_in_tiling = cfg->num_actions;
		uint16_t pow_helper;

		mem_read32(
			&(word.raw),
			pkt->packet_payload + cursor,
			sizeof(union two_u16s)
		);
		cursor += sizeof(uint16_t);

		// need to switch on num_dims
		cfg->tiling_sets[num_tilings].num_dims = word.ints[0];

		switch (word.ints[0]) {
			case 0:
				// bias tile
				// in a clean packet, this ALWAYS comes first if it does appear.
				cfg->tiling_sets[num_tilings].location = TILE_LOCATION_T1;
				cfg->tiling_sets[num_tilings].offset = 0;
				cfg->tiling_sets[num_tilings].start_tile = 0;

				cfg->tiling_sets[num_tilings].end_tile = tiles_in_tiling;

				total_work_items += 1;
				break;
			default:
				// normal tile
				// location (u8)
				cfg->tiling_sets[num_tilings].location = word.bytes[2];
				cursor += sizeof(uint8_t);

				// dims (num_dims * uint16_t)
				// FIXME: BROKEN BY CLS
				// ua_memcpy_mem40_mem40(
				// 	(__addr40 void*) &(cfg->tiling_sets[num_tilings].dims), 0,
				// 	pkt->packet_payload + cursor, 0,
				// 	word.ints[0] * sizeof(uint16_t)
				// );
				for (i=0; i<word.ints[0]; i++) {
					cfg->tiling_sets[num_tilings].dims[i] = ((__declspec(emem) __addr40 uint16_t *) (pkt->packet_payload + cursor))[i];
				}
				cursor += word.ints[0] * sizeof(uint16_t);

				tiles_in_tiling *= cfg->tilings_per_set;
				pow_helper = word.ints[0];
				while (pow_helper != 0) {
					tiles_in_tiling *= cfg->tiles_per_dim;
					pow_helper--;
				}

				// need to handle offset reset to 0 on tier change
				// Assumes that locs are specified in-order in the tiling packet.
				// also, skipping over previous locs should copy the right-edge.
				while (loc != cfg->tiling_sets[num_tilings].location) {
					inner_offset = 0;
					cfg->last_tier_tile[loc] = current_start_tile;
					loc++;
				}

				cfg->tiling_sets[num_tilings].offset = inner_offset;
				cfg->tiling_sets[num_tilings].start_tile = current_start_tile;

				total_work_items += cfg->tilings_per_set;
		}

		// loc transition? write current start tile to cfg->last_tier_tile;

		current_start_tile += tiles_in_tiling;
		inner_offset += tiles_in_tiling;

		cfg->last_tier_tile[loc] = current_start_tile;

		cfg->tiling_sets[num_tilings].tiling_size = tiles_in_tiling / cfg->tilings_per_set;
		cfg->tiling_sets[num_tilings].end_tile = current_start_tile;

		num_tilings++;
	}

	// fill out remaining right-edges.
	while (loc <= TILE_LOCATION_T3) {
		cfg->last_tier_tile[loc] = current_start_tile;
		loc++;
	}

	cfg->num_tilings = num_tilings;
	cfg->num_work_items = total_work_items;
}

void policy_block_copy(
	__declspec(cls) struct rl_config *cfg,
	__declspec(xfer_read_reg) struct rl_work_item *pkt,
	uint32_t tile
) {
	__declspec(xfer_write_reg) uint64_t nani;
	uint8_t loc;
	uint32_t offset;

	for (loc = 0; loc < 3; ++loc) {
		uint32_t loc_start = cfg->last_tier_tile[loc];
		if (tile < loc_start) {
			break;
		}
	}

	// gives us the "inner offset" in the active policy tier.
	offset = tile - ((loc == 0)
		? 0
		: cfg->last_tier_tile[loc-1]);

	switch (loc) {
		case TILE_LOCATION_T1:
			ua_memcpy_cls40_mem40(
				&(t1_tiles[offset]), 0,
				pkt->packet_payload, 0,
				pkt->packet_size
			);
			break;
		case TILE_LOCATION_T2:
			ua_memcpy_mem40_mem40(
				&(t2_tiles[offset]), 0,
				pkt->packet_payload, 0,
				pkt->packet_size
			);
			break;
		case TILE_LOCATION_T3:
			ua_memcpy_mem40_mem40(
				&(t3_tiles[offset]), 0,
				pkt->packet_payload, 0,
				pkt->packet_size
			);
			break;
		default:
			// nani = loc;
			// mem_write64(&nani, &really_really_bad_p, sizeof(uint64_t));
			break;
	}
}

#ifdef _RL_CORE_OLD_POLICY_WORK
void state_packet(
	__declspec(cls) struct rl_config *cfg,
	__declspec(xfer_read_reg) struct rl_work_item *pkt,
	mem_ring_addr_t r_out_addr,
	uint16_t dim_count
) {
	__declspec(write_reg) struct rl_answer_item workq_write_register;
	struct rl_answer_item action_item;

	#define _U8_ALD_RL_M_HITS (((RL_MAX_TILE_HITS + 7) / 8) * 8)
	__declspec(ctm, aligned(8)) uint32_t tc_indices[_U8_ALD_RL_M_HITS] = {0};
	uint16_t tc_count;
	uint16_t chosen_action;
	uint32_t rng_draw;

	// tile_t state[RL_DIMENSION_MAX];

	tile_t prefs[MAX_ACTIONS] = {0};

	uint32_t t0;
	uint32_t t1;

	__declspec(xfer_write_reg) uint32_t time_taken;
	__declspec(xfer_write_reg) uint64_t nani;

	__declspec(xfer_read_reg) union two_u16s word;
	uint16_t i = 0;
	uint16_t j = 0;

	//tile_t state_key = select_key(cfg->state_key, state);
	//tile_t reward_key = select_key(cfg->reward_key, state);

	t0 = local_csr_read(local_csr_timestamp_low);

	if (dim_count != cfg->num_dims) {
		nani = (dim_count << 16) | cfg->num_dims;
		mem_write64(&nani, &really_really_bad_p, sizeof(uint64_t));
		return;
	}

	//dst, src, len
	// ua_memcpy_mem40_mem40(
	// 	pkt->packet_payload, 0,
	// 	(void*)state, 0,
	// 	dim_count * sizeof(tile_t)
	// );

	tc_count = tile_code((__declspec(emem) __addr40 tile_t *)pkt->packet_payload, cfg, tc_indices);
	/*global_tc_count = tc_count;

	for (i=0; i<tc_count; i++) {
		global_tc_indices[i] = tc_indices[i];
	}*/

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

	if (!cfg->disable_action_writeout) {
		action_item.len = dim_count;
		action_item.action = chosen_action;
		action_item.state = (__declspec(emem) __addr40 tile_t *)rl_pkt_get_slot(&rl_actions);

		// do the memecopy
		// ua_memcpy_mem40_mem40(
		// 	(void*)action_item.state, 0,
		// 	(void*)pkt->packet_payload, 0,
		// 	dim_count * sizeof(tile_t)
		// );
		memcpy_mem40_mem40_al8(
			(__declspec(emem) __addr40 uint64_t *)action_item.state,
			(__declspec(emem) __addr40 uint64_t *)pkt->packet_payload,
			dim_count * sizeof(tile_t)
		);
		workq_write_register = action_item;
		mem_workq_add_work(
			RL_OUT_RING_NUMBER,
			r_out_addr,
			&workq_write_register,
			RL_ANSWER_LEN_ALIGN
		);
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
		// // For some reason, it bugs out if struct size is lte 32bit?
		uint64_t reward_key = fat_select_key(cfg->reward_key, (__declspec(emem) __addr40 tile_t *)pkt->packet_payload);
		uint64_t state_key = fat_select_key(cfg->state_key, (__declspec(emem) __addr40 tile_t *)pkt->packet_payload);

		// uint64_t reward_key = select_key(cfg->reward_key, state);
		// uint64_t state_key = select_key(cfg->state_key, state);

		int32_t state_added = 0;

		int32_t reward_found = CAMHT_LOOKUP(reward_map, &reward_key);

		// This is dummied until I figure out the mechanism better.
		int32_t state_found = CAMHT_LOOKUP_IDX_ADD(state_action_map, &state_key, &state_added);
		uint32_t changed_key = 0;

		uint8_t updating_on_this_cycle;

		if (state_action_map_key_tbl[state_found] != state_key) {
			state_action_map_key_tbl[state_found] = state_key;
			changed_key = 1;
		}

		updating_on_this_cycle = (!(state_added || changed_key)) && state_found >= 0 && reward_found >= 0;
		updating_on_this_cycle &= cfg->force_update_to_happen != BHAV_SKIP;

		if (cfg->force_update_to_happen == BHAV_ALWAYS) {
			if (state_found < 0) {
				state_found = 0;
			}
			if (reward_found < 0) {
				reward_found = 0;
			}
		}

		//nani = (((uint64_t) reward_found) << 32) | state_added;
		//mem_write64(&nani, &really_really_bad_p, sizeof(uint64_t));
		if ((cfg->force_update_to_happen == BHAV_ALWAYS) || updating_on_this_cycle) {
			tile_t matched_reward = reward_map_key_tbl[reward_found].data;

			tile_t value_of_chosen_action = prefs[chosen_action];

			tile_t adjustment = cfg->alpha;
			tile_t dt;
			// don't copy the tile list, that's probably TOO heavy.
		
			dt = matched_reward + quant_mul(cfg->gamma, value_of_chosen_action, cfg->quantiser_shift) - state_action_pairs[state_found].val;

			adjustment = quant_mul(adjustment, dt, cfg->quantiser_shift);

			// tc_indices, tc_count, cfg, prefs
			update_action_preferences(&(state_action_pairs[state_found].tiles[0]), tc_count, cfg, chosen_action, adjustment);
		}

		// TODO: figure out "broken-math" version of this (indiv tile shifts)

		// store the tile list, chosen action, and its value.
		state_action_pairs[state_found].action = chosen_action;
		state_action_pairs[state_found].val = prefs[chosen_action];
		state_action_pairs[state_found].len = tc_count;

		// dst, src, size
		// ua_memcpy_mem40_mem40(
		// 	&(state_action_pairs[state_found].tiles[0]), 0,
		// 	(void*)tc_indices, 0,
		// 	tc_count * sizeof(tile_t)
		// );
		memcpy_emem_ctm_al8(
			(__declspec(emem) uint64_t *) &(state_action_pairs[state_found].tiles[0]),
			(__declspec(emem) uint64_t *) tc_indices,
			tc_count * sizeof(uint32_t)
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
#endif /* _RL_CORE_OLD_POLICY_WORK */

void reward_packet(__declspec(cls) struct rl_config *cfg, union pad_tile value, uint32_t reward_insert_loc) {
	// Switch on self->reward_key
	// if shared, place into key 0 I guess?
	int32_t loc = 0;
	uint64_t long_key = reward_insert_loc;
	int32_t added = 1;

	switch (cfg->reward_key.kind) {
		case KEY_SRC_SHARED:
			break;
		case KEY_SRC_FIELD:
			// need to hash reward_insert_loc
			loc = CAMHT_LOOKUP_IDX_ADD(reward_map, &long_key, &added);
			break;
		case KEY_SRC_VALUE:
			// need to use reward_insert_loc
			loc = reward_insert_loc;
			break;
	}

	if (added >= 0) {
		reward_map_key_tbl[loc].data = value.data;
	}
}
