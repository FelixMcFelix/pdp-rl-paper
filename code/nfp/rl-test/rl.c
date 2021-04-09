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
#include "rl.h"
#include "rl-pkt-store.h"
#include "pif_parrep.h"

#include "subtask/ack.h"
#include "subtask/work.h"
#include "quant_math.h"
#include "worker_config.h"

#include "policy/policy_mem_define.h"
#include "util.h"

// ring head and tail on i25 or emem1
volatile __emem_n(0) __declspec(export, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t rl_mem_workq[512*1024] = {0};
_NFP_CHIPRES_ASM(.alloc_resource rl_mem_workq_rnum emem0_queues+RL_RING_NUMBER global 1)
//_NFP_CHIPRES_ASM(.init_mu_ring rl_mem_workq_rnum rl_mem_workq)

__declspec(export, emem) struct rl_pkt_store rl_pkts;
volatile __declspec(export, emem, addr40, aligned(sizeof(unsigned int))) uint8_t inpkt_buffer[RL_PKT_MAX_SZ * RL_PKT_STORE_COUNT] = {0};

// ring head and tail on i25 or emem1
volatile __emem_n(0) __declspec(export, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t rl_out_workq[512*1024] = {0};
_NFP_CHIPRES_ASM(.alloc_resource rl_out_workq_rnum emem0_queues+RL_OUT_RING_NUMBER global 1)
//_NFP_CHIPRES_ASM(.init_mu_ring rl_mem_workq_rnum rl_mem_workq)

__declspec(export, emem) struct rl_pkt_store rl_actions;
volatile __declspec(export, emem, addr40, aligned(sizeof(unsigned int))) uint8_t rl_out_state_buffer[RL_DIMENSION_MAX * sizeof(tile_t) * RL_PKT_STORE_COUNT] = {0};

volatile __declspec(export, emem) uint64_t really_really_bad = 0;

__declspec(export, emem) struct worker_ack xd_lmao = {0};

__declspec(export, emem) struct rl_work_item test_work;

// Testing out CAM table creation here.

/*#define SA_CAM_BUCKETS 0x40000
#define SA_TABLE_SZ (SA_CAM_BUCKETS * 16)

__export __emem __align(SA_TABLE_SZ) struct mem_lkup_cam32_16B_table_bucket_entry lkup_table[SA_CAM_BUCKETS];*/

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

volatile __declspec(shared) struct work local_ctx_work = {0};

#include "worker/worker_signums.h"

#include "subtask/internal_writeback.h"

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

// This is guaranteed to be sorted!
void update_action_preferences(__addr40 _declspec(emem) uint32_t *tile_indices, uint16_t tile_hit_count, __addr40 _declspec(emem) struct rl_config *cfg, uint16_t action, tile_t delta) {
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

tile_t fat_select_key(struct key_source key_src, __addr40 __declspec(emem) tile_t *state_vec) {
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

// C FILE INCLUDES
// NEEDED DUE TO EXPORT DIRECTIVES OF TILING ON SAME CORE

#define NO_FORWARD

// WARNING
#include "packet/packet.c"
#include "subtask/internal_writeback.c"
#include "worker/worker_body.c"
// WARNING

#ifndef _RL_WORKER_DISABLED

/* Assumes that lock over Ack[i] is held, and returns the number of acks that this packet contained.
*/
uint32_t aggregate_acks(struct worker_ack *aggregate, uint32_t new_i) {
	uint32_t j = 0;
	/*__asm{
		ctx_arb[bpt]
	}*/
	switch (aggregate->type) {
		case ACK_NO_BODY:
			return 1;
		case ACK_WORKER_COUNT:
			aggregate->body.worker_count = reflector_writeback[new_i].body.worker_count;
			really_really_bad = aggregate->body.worker_count;
			return 1;
		case ACK_VALUE_SET:
			for (j=0; j< MAX_ACTIONS; j++) {
				aggregate->body.value_set->prefs[j] += reflector_writeback[new_i].body.value_set->prefs[j];
			}
			return reflector_writeback[new_i].body.value_set->num_items;
		default:
			return 1;
	}
}

/* Returns the number of acknowledgements received in this call.
*/
uint32_t receive_worker_acks(struct worker_ack *aggregate, uint32_t needed_acks) {
	uint32_t handled = 0;

	// TODO: update these for pipeline mode?
	uint32_t min = 0;
	uint32_t max = RL_WORKER_WRITEBACK_SLOTS;
	uint32_t i;

	while (1) {
		if (atomic_writeback_acks >= needed_acks) {
			break;
		}

		sleep(50);
	}

	return handled;
}

__intrinsic void pass_work_on(struct work to_do, uint8_t sig_no) {
	int i = 0;
	atomic_writeback_acks = 0;

	for (i=0; i<MAX_ACTIONS; i++) {
		atomic_writeback_prefs[i] = 0;
	}

	//remote
	in_type_a = to_do;
	local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (WORKER_SIGNUM << 3));

	//same ctx
	local_ctx_work = to_do;
	for (i=1; i<8; i++) {
		signal_ctx(i, sig_no);
	}
}

void state_packet_delegate(
	__addr40 _declspec(emem) struct rl_config *cfg,
	__declspec(xfer_read_reg) struct rl_work_item *pkt,
	mem_ring_addr_t r_out_addr,
	uint16_t dim_count,
	uint8_t worker_ct
) {
	__declspec(write_reg) struct rl_answer_item workq_write_register;
	struct rl_answer_item action_item;
	__declspec(emem) volatile struct value_set vals = {0};
	uint16_t chosen_action;
	uint32_t rng_draw;

	struct worker_ack aggregate = {0};
	struct work to_do = {0};

	__declspec(emem) volatile tile_t state[RL_DIMENSION_MAX];

	__declspec(emem) volatile tile_t prefs[MAX_ACTIONS] = {0};

	uint32_t t0;
	uint32_t t1;

	__declspec(xfer_write_reg) uint32_t time_taken;
	__declspec(xfer_write_reg) uint64_t nani;

	__declspec(xfer_read_reg) union two_u16s word;
	int i = 0;

	//tile_t state_key = select_key(cfg->state_key, state);
	//tile_t reward_key = select_key(cfg->reward_key, state);

	t0 = local_csr_read(local_csr_timestamp_low);


	to_do.type = WORK_STATE_VECTOR;
	to_do.body.state = (__declspec(emem) __addr40 tile_t *)pkt->packet_payload;

	pass_work_on(to_do, __signal_number(&internal_handout_sig));
	aggregate.type = ACK_VALUE_SET;
	aggregate.body.value_set = &vals;

	// copy state into writeback here?
	if (!cfg->disable_action_writeout) {
		action_item.len = dim_count;
		action_item.action = chosen_action;
		action_item.state = (__declspec(emem) __addr40 tile_t *)rl_pkt_get_slot(&rl_actions);

		// do the memecopy
		ua_memcpy_mem40_mem40(
			(void*)action_item.state, 0,
			(void*)pkt->packet_payload, 0,
			dim_count * sizeof(tile_t)
		);
	}

	receive_worker_acks(&aggregate, worker_ct);
	
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
			if (vals.prefs[i] > vals.prefs[chosen_action]) {
				chosen_action = i;
			}
		}
	}

	if (!cfg->disable_action_writeout) {
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
		// For some reason, it bugs out if struct size is lte 32bit?
		uint64_t reward_key = fat_select_key(cfg->reward_key, (__declspec(emem) __addr40 tile_t *)pkt->packet_payload);
		uint64_t state_key = fat_select_key(cfg->state_key, (__declspec(emem) __addr40 tile_t *)pkt->packet_payload);

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
		
			dt = matched_reward + quant_mul(cfg->alpha, value_of_chosen_action, cfg->quantiser_shift) - state_action_pairs[state_found].val;

			adjustment = quant_mul(adjustment, dt, cfg->quantiser_shift);

			to_do.type = WORK_UPDATE_POLICY;
			to_do.body.update.action = chosen_action;
			to_do.body.update.delta = adjustment;
			to_do.body.update.state = &(state_action_pairs[state_found].state[0]);

			// currently both versions are impl'd wrong!
			// should be using the stored tiles, NOT the new ones...

			pass_work_on(to_do, __signal_number(&internal_handout_sig));

			aggregate.type = ACK_NO_BODY;
			receive_worker_acks(&aggregate, worker_ct);
		}

		// TODO: figure out "broken-math" version of this (indiv tile shifts)

		// store the tile list, chosen action, and its value.
		state_action_pairs[state_found].action = chosen_action;
		state_action_pairs[state_found].val = prefs[chosen_action];

		// dst, src, size
		ua_memcpy_mem40_mem40(
			&(state_action_pairs[state_found].state[0]), 0,
			(void*)pkt->packet_payload, 0,
			dim_count * sizeof(tile_t)
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

#endif /* !_RL_WORKER_DISABLED */

__declspec(export, emem) volatile uint32_t vis_indices[RL_MAX_TILE_HITS] = {0};

__intrinsic void heavy_first_work_allocation(
	__addr40 __declspec(emem) uint32_t *indices,
	__addr40 __declspec(emem) struct rl_config *cfg
) {
	// FIXME: in future, this could be made to rely upon larger CT/runtime bounds.
	uint32_t class_costs[4] = {6, 10, 13, 19};
	uint32_t available_ctxs[4] = {7, 8, 8, 8};
	uint32_t first_ctx[4] = {0, 7, 15, 23};
	uint32_t first_missing_ctx[4] = {7, 15, 23, 31};

	uint32_t ctx_costs[31] = {0};
	uint8_t ctx_items_allocd[31] = {0};
	uint8_t ctx_items_max[31] = {0};

	uint32_t me_costs[4] = {0};
	uint8_t me_items_allocd[4] = {0};
	uint8_t me_items_max[4] = {0};

	int32_t work_index_to_place;
	uint8_t bias_tile_exists = cfg->tiling_sets[0].num_dims == 0;

	uint16_t usable_ctxs = (cfg->worker_limit != 0)
		? cfg->worker_limit
		: 31;
	uint16_t usable_mes = 0;
	uint8_t ctx_ct = 0;

	uint8_t base_alloc_sz = cfg->num_work_items / usable_ctxs;
	uint8_t allocs_with_spill = cfg->num_work_items % usable_ctxs;

	while (ctx_ct < usable_ctxs) {
		uint16_t amt_to_add = available_ctxs[usable_mes];
		amt_to_add = (amt_to_add > (usable_ctxs - ctx_ct))
			? (usable_ctxs - ctx_ct)
			: amt_to_add;

		available_ctxs[usable_mes] = amt_to_add;

		usable_mes++;
		ctx_ct += amt_to_add;
	}

	// setup maxes for all workers.
	for (ctx_ct = 0; ctx_ct < usable_ctxs; ctx_ct++) {
		uint8_t my_me = 0;

		if (ctx_ct < allocs_with_spill) {
			ctx_items_max[ctx_ct]= base_alloc_sz + 1;
		} else {
			ctx_items_max[ctx_ct] = base_alloc_sz;
		}

		while (ctx_ct >= first_missing_ctx[my_me]) {
			my_me++;
		}

		me_items_max[my_me] += ctx_items_max[ctx_ct];

		//emergency test
		ctx_costs[ctx_ct] = available_ctxs[my_me] - (first_missing_ctx[my_me] - ctx_ct) - 1;

		// ctx_costs[ctx_ct] *= 2;
		me_costs[my_me] += ctx_costs[ctx_ct];
	}

	// do work alloc
	for (work_index_to_place = cfg->num_work_items - 1; work_index_to_place >= 0; --work_index_to_place) {
		// caclulate loc of each work item => cost
		// place by heuristic
		//   calculate dest me
		//   then calculate dest ctx.
		// use these to place `work_index_to_place` into indices.

		uint32_t tiling_set_idx = bias_tile_exists
			? (((work_index_to_place - 1) / cfg->tilings_per_set) + 1)
			: (work_index_to_place / cfg->tilings_per_set);

		uint8_t loc = cfg->tiling_sets[tiling_set_idx].location + 1;

		uint8_t best_me = usable_mes - 1;
		uint8_t base_ctx = 0;
		uint8_t best_sub_ctx = 0;
		int8_t i;
		uint16_t my_target;

		loc -= (bias_tile_exists && work_index_to_place == 0)
			? 1
			: 0;

		for (i = usable_mes - 1; i >= 0; --i) {
			if (me_items_allocd[i] < me_items_max[i]
					&& ((me_costs[i] * available_ctxs[best_me]) < (me_costs[best_me] * available_ctxs[i]))) {
					// && ((me_costs[i]) < (me_costs[best_me]))) {
				best_me = i;
			}
		}

		base_ctx = first_ctx[best_me];

		for (i = 0; i < available_ctxs[best_me]; ++i) {
			uint8_t l_ctx = base_ctx + i;
			if (ctx_items_allocd[l_ctx] < ctx_items_max[l_ctx] && ctx_costs[base_ctx + i] < ctx_costs[base_ctx + best_sub_ctx]) {
				best_sub_ctx = i;
			}
		}

		me_items_allocd[best_me]++;
		me_costs[best_me] += class_costs[loc];

		base_ctx += best_sub_ctx;

		// ind[x] = ...
		// where x = first work_idx for this ctx + ctx_items_allocd[base_ctx]

		if (base_ctx >= allocs_with_spill) {
			my_target = allocs_with_spill * (base_alloc_sz + 1);
			my_target += (base_ctx - allocs_with_spill) * base_alloc_sz;
		} else {
			my_target = base_ctx * (base_alloc_sz + 1);
		}

		indices[my_target + ctx_items_allocd[base_ctx]] = work_index_to_place;

		ctx_items_allocd[base_ctx]++;
		ctx_costs[base_ctx] += class_costs[loc];

		// if (work_index_to_place == 0) {
		// 	__asm {
		// 		ctx_arb[bpt]
		// 	}
		// }
	}
}

main() {
	__declspec(xfer_write_reg) uint64_t nani;
	mem_ring_addr_t r_addr;
	mem_ring_addr_t r_out_addr;
	uint32_t total_num_workers = 0;
	uint8_t has_setup = 0;
	uint8_t has_tiling = 0;
	uint8_t i = 0;
	uint32_t random_sample = 0;
	uint32_t temp_idx = 0;
	__declspec(emem) volatile uint32_t indices[RL_MAX_TILE_HITS] = {0};

	#ifndef _RL_WORKER_DISABLED
	__assign_relative_register(&reflector_writeback_sig, WRITEBACK_SIGNUM);
	__assign_relative_register(&internal_handout_sig, WORKER_SIGNUM);
	#endif /* !_RL_WORKER_DISABLED */

	if (__ctx() == 0) {
		// init above huge blocks.
		// compiler complaining about pointer types. Consider fixing this...
		init_rl_pkt_store(&rl_pkts, inpkt_buffer, RL_PKT_MAX_SZ);
		r_addr = mem_workq_setup(RL_RING_NUMBER, rl_mem_workq, 512 * 1024);

		init_rl_pkt_store(&rl_actions, rl_out_state_buffer, RL_DIMENSION_MAX * sizeof(tile_t));
		r_out_addr = mem_workq_setup(RL_OUT_RING_NUMBER, rl_out_workq, 512 * 1024);

		// init RNG
		local_csr_write(local_csr_pseudo_random_number, 0xcafed00d);
	} else {
		r_addr = mem_ring_get_addr(rl_mem_workq);
		r_out_addr = mem_ring_get_addr(rl_out_workq);
	}

	t1_tiles[0] = __ctx();

	// FIXME: Might want to make other contexts wait till queue init'd
	

	#ifndef _RL_WORKER_DISABLED

	// After that, we want NN registers established between co-located MEs.
	#ifdef _RL_CORE_SLAVE_CTXES
	in_type_a.body.worker_count = __n_ctx() - 1;
	#else
	in_type_a.body.worker_count = 0;
	#endif /* _RL_CORE_SLAVE_CTXES */
	in_type_a.type = WORK_REQUEST_WORKER_COUNT;

	local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (WORKER_SIGNUM << 3));

	__implicit_write(&reflector_writeback_sig);
	__implicit_write(&reflector_writeback);

	if (__ctx() == 0) {
		// SET THIS NO MATTER WHAT.
		struct worker_ack aggregate = {0};
		struct work to_do = {0};

		aggregate.type = ACK_WORKER_COUNT;
		receive_worker_acks(&aggregate, 1);

		total_num_workers = atomic_writeback_prefs[0];

		to_do.type = WORK_SET_WORKER_COUNT;
		to_do.body.worker_count = total_num_workers;

		really_really_bad = total_num_workers;

		pass_work_on(to_do, __signal_number(&internal_handout_sig));
		nani = receive_worker_acks(&aggregate, total_num_workers);;

		mem_write64(&nani, &really_really_bad, sizeof(uint64_t));

		// fall into main evt loop.
	} else {
		unsigned int client_sig = __signal_number(&reflector_writeback_sig);
		work(0, client_sig);
	}

	#endif /* !_RL_WORKER_DISABLED */

	// init index list.
	for (i=0; i<RL_MAX_TILE_HITS; ++i) {
		indices[i] = i;
	}

	// Now wait for RL packets from any P4 PIF MEs.
	while (1) {
		union pad_tile pt = {0};
		__declspec(read_reg) struct rl_work_item workq_read_register;

		struct worker_ack aggregate = {0};
		struct work to_do = {0};
		uint8_t new_cfg = 0;

		mem_workq_add_thread(
			RL_RING_NUMBER,
			r_addr,
			&workq_read_register,
			RL_WORK_LEN_ALIGN
		);

		// NOTE: look into using the header validity fields which are part of PIF.
		// Failing that, drop the magic number...
		switch (workq_read_register.ctldata.t4_type) {
			case PIF_PARREP_TYPE_rct:
				switch (workq_read_register.parsed_fields.config.cfg_type) {
					case 0:
						test_work = workq_read_register;
						setup_packet(&cfg, &workq_read_register);

						really_really_bad = (uint64_t) &cfg;

						new_cfg = 1;
						has_setup = 1;
						break;
					case 1:
						tilings_packet(&cfg, &workq_read_register);

						new_cfg = 1;
						has_tiling = 1;
						break;
					default:
						break;
				}
				break;
			case PIF_PARREP_TYPE_ins:
				// block policy insertion
				policy_block_copy(&cfg, &workq_read_register, workq_read_register.parsed_fields.insert.offset);
				break;
			case PIF_PARREP_TYPE_in_state:
				//state
				#ifdef _RL_CORE_OLD_POLICY_WORK
				state_packet(&cfg, &workq_read_register, r_out_addr, workq_read_register.parsed_fields.state.dim_count);
				#else
				state_packet_delegate(&cfg, &workq_read_register, r_out_addr, workq_read_register.parsed_fields.state.dim_count, total_num_workers);
				#endif /* _RL_CORE_OLD_POLICY_WORK */

				// TEMP: this is here to handle actions put on the line.
				if (!cfg.disable_action_writeout) {
					__declspec(read_reg) struct rl_answer_item workq_dump;
					mem_workq_add_thread(
						RL_OUT_RING_NUMBER,
						r_out_addr,
						&workq_dump,
						RL_ANSWER_LEN_ALIGN
					);
					rl_pkt_return_slot(&rl_actions, workq_dump.state);
				}
				break;
			case PIF_PARREP_TYPE_in_reward:
				pt._pad = workq_read_register.parsed_fields.reward.measured_value;
				//reward
				reward_packet(
					&cfg,
					pt,
					workq_read_register.parsed_fields.reward.lookup_key
				);
				break;
			case 999:
				//fixme: not got a code for this yet
			default:
				break;
		}

		rl_pkt_return_slot(&rl_pkts, workq_read_register.packet_payload);

		#ifndef _RL_WORKER_DISABLED

		if (new_cfg) {
			if (has_tiling && has_setup) {
				to_do.type = WORK_NEW_CONFIG;
				to_do.body.cfg = &cfg;
				// to_do.body.cfg = (__addr40 _declspec(emem) struct rl_config *) (((uint64_t)&cfg) << 32 | ((uint64_t)&cfg) >> 32);
				pass_work_on(to_do, __signal_number(&internal_handout_sig));
				aggregate.type = ACK_NO_BODY;
				receive_worker_acks(&aggregate, total_num_workers);

				#ifdef WORK_ALLOC_RANDOMISE
				// init index list.
				for (i=0; i<RL_MAX_TILE_HITS; ++i) {
					indices[i] = i;
				}

				// Knuth shuffle.
				for (i=((cfg.num_work_items) - 1); i>0; i--) {
					random_sample = 0;
					while (1) {
						random_sample = local_csr_read(local_csr_pseudo_random_number);
						/* __asm{
							ctx_arb[bpt]
						} */
						if (random_sample > (0xffffffff % (i + 1))) {
							break;
						}

						sleep(500);
					}

					random_sample %= (i + 1);

					// swap indices[i], w/ indices[random_sample]
					temp_idx = indices[i];
					indices[i] = indices[random_sample];
					indices[random_sample] = temp_idx;
				}

				#endif /* WORK_ALLOC_RANDOMISE */

				if (WORK_ALLOC_STRAT == ALLOC_FILL_HEAVY) {
					heavy_first_work_allocation(&indices, &cfg);
				}

				to_do.type = WORK_ALLOCATE;
				to_do.body.alloc.strat = WORK_ALLOC_STRAT;
				to_do.body.alloc.work_indices = &indices;
				pass_work_on(to_do, __signal_number(&internal_handout_sig));
				aggregate.type = ACK_NO_BODY;
				receive_worker_acks(&aggregate, total_num_workers);

				for (i=0; i<RL_MAX_TILE_HITS; ++i) {
					vis_indices[i] = indices[i];
				}
			}
		}

		#endif /* !_RL_WORKER_DISABLED */
	}

	#ifndef _RL_WORKER_DISABLED
	__implicit_write(&reflector_writeback_sig);
	__implicit_write(&reflector_writeback);
	__implicit_read(&reflector_writeback_sig);
	__implicit_read(&reflector_writeback);
	#endif /* !_RL_WORKER_DISABLED */
}
