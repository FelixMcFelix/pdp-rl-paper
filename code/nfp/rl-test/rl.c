#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp_mem_ring.h>
#include <nfp_mem_workq.h>
//#include <nfp/mem_ring.h>
#include "rl.h"
#include "rl-pkt-store.h"
#include "pif_parrep.h"

__declspec(i1.cls, export)tile_t t1_tiles[MAX_CLS_TILES] = {0};
__declspec(i1.ctm, export)tile_t t2_tiles[MAX_CTM_TILES] = {0};
__declspec(export imem)tile_t t3_tiles[MAX_IMEM_TILES] = {0};

// Maybe keep one of these locally, too?
__declspec(i1.cls, export) struct rl_config cfg = {0};

// ring head and tail on i25 or emem1
#define RL_RING_NUMBER 4
volatile __declspec(export, emem, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t mem_workq[512*1024] = {0};

volatile __declspec(export, ctm, addr40) struct rl_pkt_store *rl_pkts;
volatile __declspec(export, emem, addr40, aligned(sizeof(unsigned int))) uint8_t inpkt_buffer[RL_PKT_MAX_SZ * RL_PKT_STORE_COUNT] = {0};

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

void setup_packet(struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt) {
	int dim;
	int cursor = 0;

	// setup information.
	// rest of packet body is:
	// n_dims in all state vectors (u16)
	// tiles_per_dim (u16)
	// tilings_per_set (u16)
	// n_actions (u16)
	memcpy_cls_mem(
		(void*)&(cfg->num_dims),
		pkt->packet_payload + (cursor += (4 * sizeof(uint16_t))),
		4 * sizeof(uint16_t)
	);

	// epsilon (frac = 2xtile_t)
	// alpha (frac = 2xtile_t)
	// epsilon_d (tile_t)
	// epsilon_f (uint32_t)
	memcpy_cls_mem(
		(void*)&(cfg->epsilon),
		pkt->packet_payload + (cursor += (2 * sizeof(struct tile_fraction) + sizeof(tile_t) + sizeof(uint32_t))),
		2 * sizeof(struct tile_fraction) + sizeof(tile_t) + sizeof(uint32_t)
	);

	// n x tile_t maxes
	// n x tile_t mins
	memcpy_cls_mem(
		(void*)&(cfg->maxes),
		pkt->packet_payload + cursor,
		cfg->num_dims * 2 * sizeof(tile_t)
	);

	// Fill in width, shift_amt, adjusted_max...
	for (dim = 0; dim < cfg->num_dims; ++dim) {
		cfg->width[dim] = (cfg->maxes[dim] - cfg->mins[dim]) / (1 + cfg->tiles_per_dim);
		cfg->shift_amt[dim] = cfg->width[dim] / cfg->tilings_per_set;
		cfg->adjusted_maxes[dim] = cfg->maxes[dim] + cfg->width[dim];
	}
}

void tilings_packet(struct rl_config *cfg, __declspec(xfer_read_reg) struct rl_work_item *pkt) {
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
		int i = 0;
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

				cfg->tiling_sets[num_tilings].end_tile = tiles_in_tiling;
				break;
			default:
				// normal tile
				// location (u8)
				memcpy_cls_mem(
					(void*)&(cfg->tiling_sets[num_tilings].location),
					pkt->packet_payload + (cursor += sizeof(uint16_t)),
					sizeof(uint16_t)
				);
				// dims (num_dims * uint16_t)
				memcpy_cls_mem(
					(void*)&(cfg->tiling_sets[num_tilings].dims),
					pkt->packet_payload + (cursor += num_dims * sizeof(uint16_t)),
					num_dims * sizeof(uint16_t)
				);

				tiles_in_tiling *= cfg->tilings_per_set;

				for(i = 0; i < cfg->tiling_sets[num_tilings].num_dims; ++i) {
					tiles_in_tiling *= cfg->tiles_per_dim;
				}

				// need to handle offset reset to 0 on tier change

				cfg->tiling_sets[num_tilings].offset = 0;
				cfg->tiling_sets[num_tilings].start_tile = current_start_tile;
		}

		current_start_tile += (cfg->tiling_sets[num_tilings].tiling_size = tiles_in_tiling);
		cfg->tiling_sets[num_tilings].end_tile = current_start_tile;
		num_tilings++;
	}

	cfg->num_tilings = num_tilings;
}

main() {
	unsigned int mu_island = 1;
	unsigned int ring_number = (mu_island << 10) | RL_RING_NUMBER;
	SIGNAL sig;

	if (__ctx() == 0) {
		// init above huge blocks.
		// compiler complaining about pointer types. Consider fixing this...
		//memset_cls(t1_tiles, 0, MAX_CLS_SIZE);
		//memset_mem(t2_tiles, 0, MAX_CTM_SIZE);
		//memset_mem(t3_tiles, 0, MAX_IMEM_SIZE);

		//memset_cls(&cfg, 0, sizeof(struct rl_config));

		// try read some work?
		init_rl_pkt_store(rl_pkts, inpkt_buffer);
		cmd_mem_ring_init(ring_number, RING_SIZE_512K, mem_workq, mem_workq, 0);
	}

	// FIXME: Might want to make other contexts wait till queue init'd
	
	// After that, we want NN registers established between co-located MEs.
	// TODO

	// Now wait for RL packets from any P4 PIF MEs.
	while (1) {
		__declspec(read_reg) struct rl_work_item workq_read_register;

		cmd_mem_workq_add_thread(ring_number, &workq_read_register, RL_WORK_LWS, ctx_swap, &sig);

		// NOTE: look into using the header validity fields which are part of PIF.
		// Failing that, drop the magic number...
		switch (workq_read_register.rl_header[0]) {
			case 0:
				//cfg
				// type should occur at header + 4 * (PIF_PARREP_rct_OFF_LW - PIF_PARREP_rt_OFF_LW)
				switch (workq_read_register.rl_header[4 * (PIF_PARREP_rct_OFF_LW - PIF_PARREP_rt_OFF_LW)]) {
					case 0:
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
				break;
			case 2:
				//state
			default:
				break;
		}

		rl_pkt_return_slot(rl_pkts, workq_read_register.packet_payload);
	}
}