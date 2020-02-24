#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_ring.h>
#include "rl.h"

__declspec(i1.cls, export)tile_t t1_tiles[MAX_CLS_TILES];
__declspec(i1.ctm, export)tile_t t2_tiles[MAX_CTM_TILES];
__declspec(export imem)tile_t t3_tiles[MAX_IMEM_TILES];

// ring head and tail on i25 or emem1
#define RL_RING_NUMBER 4
__declspec(emem, addr40, aligned(512*sizeof(unsigned int))) uint32_t mem_workq[512];

main() {
	unsigned int mu_island = 1;
	unsigned int ring_number = (mu_island << 10) | 4;
	SIGNAL signal;

	mem_ring_init(ring_number, RING_SIZE_512, mem_workq, mem_workq, 0);

	if (__ctx() == 0) {
		// init above huge blocks.
		memset_cls(t1_tiles, 0, MAX_CLS_SIZE);
		memset_mem(t2_tiles, 0, MAX_CTM_SIZE);
		memset_mem(t3_tiles, 0, MAX_IMEM_SIZE);

		// try read some work?
		__declspec(read_reg) uint32_t read_register;
		mem_workq_add_thread(ring_number, &read_register, 1, ctx_swap, &signal);
		mem_workq_add_thread(ring_number, &read_register, 1, ctx_swap, &signal);
		mem_workq_add_thread(ring_number, &read_register, 1, ctx_swap, &signal);

	} else {
		__declspec(write_reg) uint32_t write_register;

		// Just print the other ctx ids out...
		write_register = __ctx();

		mem_workq_add_Work_imm(ring_number, &write_register, 1, sig_done, &signal)
	}
	// Try establishing a work queue.
	// See if I can figure out the assembly.
	
	// After that, we want NN registers established between co-located MEs.
}