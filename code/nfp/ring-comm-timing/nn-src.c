#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <rtl.h>

#include "shared_defs.h"

__declspec(export, emem) uint32_t nn_times[NUM_MEASUREMENTS] = {0};

__declspec(nn_remote_reg) uint32_t in_val = 0;

__declspec(visible read_reg) uint32_t me1_x;
__declspec(visible) SIGNAL me1_sig;

#define READ_REG_TYPE 0
#define PRODUCER_ME 5 // 1 + 4;
#define PRODUCER_ISLAND 4
#define PRODUCER_LOC (PRODUCER_ISLAND << 4) + PRODUCER_ME

main() {
	uint32_t t0_part;
	uint32_t t1_part;
	uint64_t t0;
	uint64_t t1;
	__declspec(xfer_write_reg) uint32_t time_taken;
	int i = 0;
	uint32_t new_val = 0;

	// temp to allow debugging other ctxs.
	return 0;

	if (__ctx() != 0) {
		return 0;
	}

	__implicit_write(&me1_sig);
	__implicit_write(&me1_x);

	for (i=1; i < (NUM_MEASUREMENTS + 1); i++) {
		t0_part = local_csr_read(local_csr_timestamp_high);
		t0 = (((uint64_t)t0_part) << 32) + local_csr_read(local_csr_timestamp_low);

		in_val = i;

		__implicit_write((void*)&me1_sig);
		__implicit_write(&me1_x);

		__wait_for_all(&me1_sig);
		new_val = me1_x;

		t1_part = local_csr_read(local_csr_timestamp_high);
		t1 = (((uint64_t)t1_part) << 32) + local_csr_read(local_csr_timestamp_low);

		time_taken = (uint32_t)(t1 - t0);

		mem_write32(&time_taken, &nn_times[i], sizeof(uint32_t));
	}
}