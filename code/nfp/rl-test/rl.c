#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp_mem_ring.h>
#include <nfp_mem_workq.h>
//#include <nfp/mem_ring.h>
#include "rl.h"

__declspec(i1.cls, export)tile_t t1_tiles[MAX_CLS_TILES] = {0};
__declspec(i1.ctm, export)tile_t t2_tiles[MAX_CTM_TILES] = {0};
__declspec(export imem)tile_t t3_tiles[MAX_IMEM_TILES] = {0};

// Maybe keep one of these locally, too?
__declspec(i1.cls, export) struct rl_config cfg = {0};

// ring head and tail on i25 or emem1
#define RL_RING_NUMBER 4
__declspec(export, emem, addr40, aligned(512*sizeof(unsigned int))) uint32_t mem_workq[512] = {0};

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
				uint32_t val = state[active_dim];
				tile_t max = cfg->maxes[active_dim];
				tile_t min = cfg->mins[active_dim];

				// to get tile in dim... rescale dimension, divide by window.
				val = (val < max) ? val : max;
				val = (val > min) ? val - min : 0;
				val /= cfg->width[active_dim];

				local_tile += width_product * val;
				width_product *= cfg->tiles_per_dim;
			}

			output[out_idx++] = local_tile;
			local_tile_base += cfg->tiling_sets[tiling_set_idx].tiling_size;
		}
	}
	return out_idx;
}

main() {
	unsigned int mu_island = 1;
	unsigned int ring_number = (mu_island << 10) | RL_RING_NUMBER;
	SIGNAL sig;

	cmd_mem_ring_init(ring_number, RING_SIZE_512, mem_workq, mem_workq, 0);

	if (__ctx() == 0) {
		__declspec(read_reg) uint32_t read_register;

		// init above huge blocks.
		// compiler complaining about pointer types. Consider fixing this...
		//memset_cls(t1_tiles, 0, MAX_CLS_SIZE);
		//memset_mem(t2_tiles, 0, MAX_CTM_SIZE);
		//memset_mem(t3_tiles, 0, MAX_IMEM_SIZE);

		//memset_cls(&cfg, 0, sizeof(struct rl_config));

		// try read some work?
		cmd_mem_workq_add_thread(ring_number, &read_register, 1, ctx_swap, &sig);
		cmd_mem_workq_add_thread(ring_number, &read_register, 1, ctx_swap, &sig);
		cmd_mem_workq_add_thread(ring_number, &read_register, 1, ctx_swap, &sig);

	} else {
		__declspec(write_reg) uint32_t write_register;

		// Just print the other ctx ids out...
		write_register = __ctx();

		cmd_mem_workq_add_work(ring_number, &write_register, 1, sig_done, &sig);
	}
	// Try establishing a work queue.
	// See if I can figure out the assembly.
	
	// After that, we want NN registers established between co-located MEs.
	// TODO

	// Now wait for RL packets from any P4 PIF MEs.
	while (1) {
		__declspec(read_reg) uint32_t read_register;
		cmd_mem_workq_add_thread(ring_number, &read_register, 1, ctx_swap, &sig);
	}
}