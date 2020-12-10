#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <rtl.h>

#include "shared_defs.h"

__nnr uint32_t journey;
__declspec(nn_remote_reg) uint32_t in_val = {0};

main() {
	int i = 0;

	if (__ctx() != 0) {
		return 0;
	}

	while (1) {
		in_val = journey;
	}
}