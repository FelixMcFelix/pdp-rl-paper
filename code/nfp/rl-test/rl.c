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

volatile __declspec(export, emem) uint64_t really_really_bad = 0;

__declspec(export, emem) struct worker_ack xd_lmao = 0;

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

// C FILE INCLUDES
// NEEDED DUE TO EXPORT DIRECTIVES OF TILING ON SAME CORE

// WARNING
#include "packet/packet.c"
#include "subtask/internal_writeback.c"
// WARNING

/* Assumes that lock over Ack[i] is held, and returns the number of acks that this packet contained.
*/
uint32_t aggregate_acks(struct worker_ack *aggregate, uint32_t new_i) {
	uint32_t j = 0;
	__asm{
		ctx_arb[bpt]
	}
	switch (aggregate->type) {
		case ACK_NO_BODY:
			return 1;
		case ACK_WORKER_COUNT:
			aggregate->body.worker_count += reflector_writeback[new_i].body.worker_count;
			return 1;
		case ACK_VALUE_SET:
			// TODO: impl.
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

	while (handled < needed_acks) {
		__asm{
			ctx_arb[bpt]
		}

		__wait_for_all(&reflector_writeback_sig);

		// at least one was modified.
		// check all.
		// TODO: do this multiple times. Skip over lock failures.
		for (i=min; i<max; i++) {
			uint32_t observed_lock = WB_FLAG_LOCK;
			__declspec(xfer_read_write_reg) uint32_t locking = WB_FLAG_LOCK;
			uint32_t lock_addr = (uint32_t) &(reflector_writeback_locks[i]);
			__declspec(xfer_read_write_reg) uint64_t nani = 0;

			// Acquire.
			// need to rely on "observed_lock" since the read direction is undef.
			while (observed_lock & WB_FLAG_LOCK) {
				__asm {
					cls[test_and_set, locking, lock_addr, 0, 1]
				}
				observed_lock = locking;
				sleep(100);
				//nani += 1;
				//mem_write64(&nani, &really_really_bad, sizeof(uint64_t));
			}

			if (locking & WB_FLAG_OCCUPIED) {
				// Fold this ack into last ack.
				handled += aggregate_acks(aggregate, i);
			}

			// Release.
			locking = (WB_FLAG_LOCK | WB_FLAG_OCCUPIED);
			__asm {
				cls[clr, locking, lock_addr, 0, 1]
			}
		}
	}

	return handled;
}

void slave_loop(unsigned int parent_sig) {
	uint32_t worker_ct = 0;
	uint32_t my_id = __ctx() - 1;
	uint8_t my_slot = my_id % RL_WORKER_WRITEBACK_SLOTS;

	uint8_t iter = 0;
	struct worker_ack ack = {0};

	//really_really_bad = 0;

	if (my_id >= 0) {
		return;
	}

	while (1) {
		enum writeback_result wb = WB_FULL;

		__wait_for_all(&reflector_writeback_sig);

		switch (local_ctx_work.type) {
			case WORK_REQUEST_WORKER_COUNT:
				// Should never ever be received...
				break;
			case WORK_SET_WORKER_COUNT:
				worker_ct = local_ctx_work.body.worker_count;

				ack.type = ACK_NO_BODY;

				while (wb != WB_SUCCESS) {
				iter += 1;
					wb = internal_writeback_ack(my_slot, ack, parent_sig);

					// TEMOP AS FUCK
					really_really_bad |= wb;
					break;

					if (wb != WB_SUCCESS) {
						sleep(1000);
					}
				}
				break;
			default:
				break;
		}
	}
}

__intrinsic void pass_work_on(struct work to_do, uint8_t sig_no) {
	int i = 1;
	//remote
	in_type_a = to_do;
	local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (14 << 3));

	//same ctx
	/*local_ctx_work = to_do;
	for (i=1; i<8; i++) {
		signal_ctx(i, sig_no);
	}*/
}

main() {
	__declspec(xfer_write_reg) uint64_t nani;
	mem_ring_addr_t r_addr;
	uint32_t total_num_workers = 0;

	if (__ctx() == 0) {
		// init above huge blocks.
		// compiler complaining about pointer types. Consider fixing this...
		init_rl_pkt_store(&rl_pkts, inpkt_buffer);
		r_addr = mem_workq_setup(RL_RING_NUMBER, rl_mem_workq, 512 * 1024);
	} else {
		r_addr = mem_ring_get_addr(rl_mem_workq);
	}

	t1_tiles[0] = __ctx();

	// FIXME: Might want to make other contexts wait till queue init'd
	
	// After that, we want NN registers established between co-located MEs.
	#ifdef _RL_CORE_SLAVE_CTXES
	in_type_a.body.worker_count = __n_ctx() - 1;
	#else
	in_type_a.body.worker_count = 0;
	#endif /* _RL_CORE_SLAVE_CTXES */
	in_type_a.type = WORK_REQUEST_WORKER_COUNT;

	local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (14 << 3));

	__implicit_write(&reflector_writeback_sig);
	__implicit_write(&reflector_writeback);

	if (__ctx() == 0) {
		// SET THIS NO MATTER WHAT.
		struct worker_ack aggregate = {0};
		struct work to_do = {0};

		aggregate.type = ACK_WORKER_COUNT;
		receive_worker_acks(&aggregate, 1);

		total_num_workers = aggregate.body.worker_count;

		to_do.type = WORK_SET_WORKER_COUNT;
		to_do.body.worker_count = total_num_workers;

		really_really_bad = total_num_workers;

		pass_work_on(to_do, __signal_number(&reflector_writeback_sig));
		nani = receive_worker_acks(&aggregate, total_num_workers);;

		mem_write64(&nani, &really_really_bad, sizeof(uint64_t));

		// fall into main evt loop.
	} else {
		unsigned int client_sig = __signal_number(&reflector_writeback_sig);
		slave_loop(client_sig);
	}

	// Now wait for RL packets from any P4 PIF MEs.
	while (1) {
		union pad_tile pt = {0};
		__declspec(read_reg) struct rl_work_item workq_read_register;

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
						break;
					case 1:
						tilings_packet(&cfg, &workq_read_register);
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
				state_packet(&cfg, &workq_read_register, workq_read_register.parsed_fields.state.dim_count);
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
	}

	__implicit_write(&reflector_writeback_sig);
	__implicit_write(&reflector_writeback);
	__implicit_read(&reflector_writeback_sig);
	__implicit_read(&reflector_writeback);
}