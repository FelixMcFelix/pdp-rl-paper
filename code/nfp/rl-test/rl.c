#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
//#include <nfp_mem_ring.h>
//#include <nfp_mem_workq.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <rtl.h>
#include "rl.h"
#include "rl-pkt-store.h"
#include "pif_parrep.h"

__declspec(i5.cls, export)tile_t t1_tiles[MAX_CLS_TILES] = {0};
__declspec(i5.ctm, export)tile_t t2_tiles[MAX_CTM_TILES] = {0};
__declspec(export imem)tile_t t3_tiles[MAX_IMEM_TILES] = {0};

// Maybe keep one of these locally, too?
__declspec(export, emem) struct rl_config cfg = {0};

// ring head and tail on i25 or emem1
#define RL_RING_NUMBER 129
volatile __emem_n(0) __declspec(export, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t rl_mem_workq[512*1024] = {0};
_NFP_CHIPRES_ASM(.alloc_resource rl_mem_workq_rnum emem0_queues+RL_RING_NUMBER global 1)
//_NFP_CHIPRES_ASM(.init_mu_ring rl_mem_workq_rnum rl_mem_workq)

__declspec(export, emem) struct rl_pkt_store rl_pkts;
volatile __declspec(export, emem, addr40, aligned(sizeof(unsigned int))) uint8_t inpkt_buffer[RL_PKT_MAX_SZ * RL_PKT_STORE_COUNT] = {0};

__declspec(export, emem) struct rl_work_item test_work;

/** Convert a state vector into a list of tile indices.
*
* Returns length of tile coded list.
*/
uint16_t tile_code(tile_t *state, struct rl_config *cfg, uint32_t *output) {
	uint16_t tiling_set_idx;
	uint16_t out_idx = 0;

	for (tiling_set_idx = 0; tiling_set_idx < cfg->num_tilings; ++tiling_set_idx) {
		uint16_t tiling_idx;
		uint32_t local_tile_base = cfg->tiling_sets[tiling_set_idx].start_tile;

		for (tiling_idx = 0; tiling_idx < cfg->tilings_per_set; ++tiling_idx) {
			uint16_t dim_idx;
			uint32_t width_product = 1;
			uint32_t local_tile = local_tile_base;

			for (dim_idx = 0; dim_idx < cfg->tiling_sets[tiling_idx].num_dims; ++dim_idx) {
				uint16_t active_dim = cfg->tiling_sets[tiling_idx].dims[dim_idx];
				uint8_t reduce_tile;
				tile_t val = state[active_dim];
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
			local_tile_base += cfg->tiling_sets[tiling_set_idx].tiling_size;
		}
	}
	return out_idx;
}

union two_u16s {
	uint32_t raw;
	uint16_t ints[2];
};

union four_u16s {
	uint64_t raw;
	uint32_t words[2];
	uint16_t shorts[4];
};

void setup_packet(__addr40 _declspec(emem) struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt) {
	int dim;
	int cursor = 0;
	__declspec(xfer_read_reg) union two_u16s word;
	__declspec(xfer_read_reg) union four_u16s bigword;

	// setup information.
	// rest of packet body is:
	// n_dims in all state vectors (u16)
	// tiles_per_dim (u16)
	// tilings_per_set (u16)
	// n_actions (u16)
	// epsilon (frac = 2xtile_t)
	// alpha (frac = 2xtile_t)
	// epsilon_d (tile_t)
	// epsilon_f (uint32_t)
	mem_read64(
		&(bigword.raw),
		pkt->packet_payload,
		sizeof(union four_u16s)
	);
	cursor += sizeof(union four_u16s);

	cfg->num_dims = bigword.shorts[0];
	cfg->tiles_per_dim = bigword.shorts[1];
	cfg->tilings_per_set = bigword.shorts[2];
	cfg->num_actions = bigword.shorts[3];

	mem_read64(
		&(bigword.raw),
		pkt->packet_payload + cursor,
		sizeof(union four_u16s)
	);
	cursor += sizeof(union four_u16s);

	cfg->epsilon.numerator = (tile_t) bigword.words[0];
	cfg->epsilon.divisor = (tile_t) bigword.words[1];

	mem_read64(
		&(bigword.raw),
		pkt->packet_payload + cursor,
		sizeof(union four_u16s)
	);
	cursor += sizeof(union four_u16s);

	cfg->alpha.numerator = (tile_t) bigword.words[0];
	cfg->alpha.divisor = (tile_t) bigword.words[1];

	mem_read64(
		&(bigword.raw),
		pkt->packet_payload + cursor,
		sizeof(union four_u16s)
	);
	cursor += sizeof(union four_u16s);

	cfg->epsilon_decay_amt = (tile_t) bigword.words[0];
	cfg->epsilon_decay_freq = bigword.words[1];

	// n x tile_t maxes
	// n x tile_t mins
	ua_memcpy_mem40_mem40(
		&(cfg->maxes), 0,
		pkt->packet_payload + cursor, 0,
		cfg->num_dims * sizeof(tile_t)
	);

	cursor += cfg->num_dims * sizeof(tile_t);

	ua_memcpy_mem40_mem40(
		&(cfg->mins), 0,
		pkt->packet_payload + cursor, 0,
		cfg->num_dims * sizeof(tile_t)
	);

	// Fill in width, shift_amt, adjusted_max...
	for (dim = 0; dim < cfg->num_dims; ++dim) {
		cfg->width[dim] = (cfg->maxes[dim] - cfg->mins[dim]) / (1 + cfg->tiles_per_dim);
		cfg->shift_amt[dim] = cfg->width[dim] / cfg->tilings_per_set;
		cfg->adjusted_maxes[dim] = cfg->maxes[dim] + cfg->width[dim];
	}
}

void tilings_packet(__addr40 _declspec(emem) struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt) {
	// tiling information.
	// rest of packet body is, repeated till end:
	//  n_dims (u16)
	// then if n_dims != 0:
	//  tile_location
	//  n x u16 (dimensions in tiling)

	int cursor = 0;
	int num_tilings = 0;
	enum tile_location loc = TILE_LOCATION_T1;
	uint32_t inner_offset = 0;
	uint32_t current_start_tile = 0;

	while(cursor < pkt->packet_size) {
		uint32_t tiles_in_tiling = cfg->num_actions;
		uint16_t num_dims;

		// need to switch on num_dims
		memcpy_cls_mem(
			(void*)&(cfg->tiling_sets[num_tilings].num_dims),
			pkt->packet_payload + (cursor += sizeof(uint16_t)),
			sizeof(uint16_t)
		);

		num_dims = cfg->tiling_sets[num_tilings].num_dims;

		switch (num_dims) {
			case 0:
				// bias tile
				// in a clean packet, this ALWAYS comes first if it does appear.
				cfg->tiling_sets[num_tilings].location = TILE_LOCATION_T1;
				cfg->tiling_sets[num_tilings].offset = 0;
				cfg->tiling_sets[num_tilings].start_tile = 0;
				cfg->first_tier_tile[0] = 0;

				cfg->tiling_sets[num_tilings].end_tile = tiles_in_tiling;
				break;
			default:
				// normal tile
				// location (u8)
				memcpy_cls_mem(
					(void*)&(cfg->tiling_sets[num_tilings].location),
					pkt->packet_payload + (cursor += sizeof(uint8_t)),
					sizeof(uint8_t)
				);
				// dims (num_dims * uint16_t)
				memcpy_cls_mem(
					(void*)&(cfg->tiling_sets[num_tilings].dims),
					pkt->packet_payload + (cursor += num_dims * sizeof(uint16_t)),
					num_dims * sizeof(uint16_t)
				);

				tiles_in_tiling *= cfg->tilings_per_set * cfg->tiles_per_dim * cfg->tiling_sets[num_tilings].num_dims;

				// need to handle offset reset to 0 on tier change
				// Assumes that locs are specified in-order in the tiling packet.
				if (loc != cfg->tiling_sets[num_tilings].location) {
					inner_offset = 0;
					loc = cfg->tiling_sets[num_tilings].location;
					cfg->first_tier_tile[loc] = current_start_tile;
				}

				cfg->tiling_sets[num_tilings].offset = inner_offset;
				cfg->tiling_sets[num_tilings].start_tile = current_start_tile;
		}

		current_start_tile += tiles_in_tiling;
		inner_offset += tiles_in_tiling;

		cfg->tiling_sets[num_tilings].tiling_size = tiles_in_tiling;
		cfg->tiling_sets[num_tilings].end_tile = current_start_tile;

		num_tilings++;
	}

	cfg->num_tilings = num_tilings;
}

void policy_block_copy(__addr40 _declspec(emem) struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt) {
	uint8_t loc;
	int cursor = 0;
	struct policy_install_data info;
	uint32_t offset;

	memcpy_lmem_mem(
		(void*)&info,
		pkt->packet_payload + (cursor += sizeof(struct policy_install_data)),
		sizeof(struct policy_install_data)
	);

	for (loc = 0; loc < 3; ++loc) {
		uint32_t loc_start = cfg->first_tier_tile[loc];
		if (info.tile < loc_start) {
			loc -= 1;
			break;
		}
	}

	// gives us the "inner offset" in the active policy tier.
	offset = info.tile - cfg->first_tier_tile[loc];

	switch (loc) {
		case TILE_LOCATION_T1:
			memcpy_cls_mem(
				(void*)&(t1_tiles[offset]),
				pkt->packet_payload + cursor,
				info.count * sizeof(tile_t)
			);
			break;
		case TILE_LOCATION_T2:
			memcpy_lmem_mem(
				(void*)&(t2_tiles[offset]),
				pkt->packet_payload + cursor,
				info.count * sizeof(tile_t)
			);
			break;
		case TILE_LOCATION_T3:
			memcpy_lmem_mem(
				(void*)&(t3_tiles[offset]),
				pkt->packet_payload + cursor,
				info.count * sizeof(tile_t)
			);
			break;
	}
}

main() {
	mem_ring_addr_t r_addr;
	SIGNAL sig;

	if (__ctx() == 0) {
		// init above huge blocks.
		// compiler complaining about pointer types. Consider fixing this...
		//memset_cls(t1_tiles, 0, MAX_CLS_SIZE);
		//memset_mem(t2_tiles, 0, MAX_CTM_SIZE);
		//memset_mem(t3_tiles, 0, MAX_IMEM_SIZE);

		//memset_cls(&cfg, 0, sizeof(struct rl_config));

		// try read some work?
		init_rl_pkt_store(&rl_pkts, inpkt_buffer);
		//mem_ring_init(ring_number, RING_SIZE_512K, mem_workq, mem_workq, 0);
		r_addr = mem_workq_setup(RL_RING_NUMBER, rl_mem_workq, 512 * 1024);
	} else {
		r_addr = mem_ring_get_addr(rl_mem_workq);
	}

	t1_tiles[0] = __ctx();

	// FIXME: Might want to make other contexts wait till queue init'd
	
	// After that, we want NN registers established between co-located MEs.
	// TODO

	// Now wait for RL packets from any P4 PIF MEs.
	while (1) {
		__declspec(read_reg) struct rl_work_item workq_read_register;

		mem_workq_add_thread(RL_RING_NUMBER, r_addr, &workq_read_register, RL_WORK_LWS);

		// NOTE: look into using the header validity fields which are part of PIF.
		// Failing that, drop the magic number...
		switch (workq_read_register.rl_header[0]) {
			case 0:
				//cfg
				// type should occur at header + 4 * (PIF_PARREP_rct_OFF_LW - PIF_PARREP_rt_OFF_LW)
				switch (workq_read_register.rl_header[4 * (PIF_PARREP_rct_OFF_LW - PIF_PARREP_rt_OFF_LW)]) {
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
			case 1:
				//ins
				// block policy insertion
				policy_block_copy(&cfg, &workq_read_register);
				break;
			case 2:
				//state
			default:
				break;
		}

		rl_pkt_return_slot(&rl_pkts, workq_read_register.packet_payload);
	}
}