#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <rtl.h>

#include "shared_defs.h"

__declspec(export, addr40, emem) struct done_t isl_time_lock = {0};

volatile __emem_n(1) __declspec(export, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t isl_time_workq[512*1024] = {0};
_NFP_CHIPRES_ASM(.alloc_resource isl_time_workq_rnum emem1_queues+ISL_ISL_RING_NUMBER global 1)

volatile __emem_n(1) __declspec(export, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t isl_reply_workq[512*1024] = {0};
_NFP_CHIPRES_ASM(.alloc_resource isl_reply_workq_rnum emem1_queues+ISL_REPLY_RING_NUMBER global 1)

__declspec(export, emem) uint32_t isl_isl_times[NUM_MEASUREMENTS] = {0};

main() {
	mem_ring_addr_t r_addr;
	mem_ring_addr_t reply_addr;
	SIGNAL sig;
	uint32_t t0_part;
	uint32_t t1_part;
	uint64_t t0;
	uint64_t t1;
	__declspec(xfer_write_reg) uint32_t time_taken;
	__declspec(write_reg) struct dummy_work_item workq_write_register;
	__declspec(read_reg) struct dummy_work_item workq_read_register;
	int i = 0;

	if (__ctx() == 0) {
		__xrw uint32_t xfer = 1;

		while (xfer == 1) {
			mem_test_set((void*)&xfer, (__addr40 void*)&(isl_time_lock.lock_val), sizeof(uint32_t));
		}

		// init above huge blocks.
		// compiler complaining about pointer types. Consider fixing this...
		r_addr = mem_workq_setup(ISL_ISL_RING_NUMBER, isl_time_workq, 512 * 1024);
		reply_addr = mem_workq_setup(ISL_REPLY_RING_NUMBER, isl_reply_workq, 512 * 1024);

		isl_time_lock.done = 1;

		xfer = 0;
		mem_write_atomic(&xfer, (__addr40 void*)&(isl_time_lock.lock_val), sizeof(uint32_t));
	} else {
		return 0;
	}

	for (i=0; i < NUM_MEASUREMENTS; i++) {
		t0_part = local_csr_read(local_csr_timestamp_high);
		t0 = (((uint64_t)t0_part) << 32) + local_csr_read(local_csr_timestamp_low);

		// SEND
		mem_workq_add_work(
			ISL_ISL_RING_NUMBER,
			r_addr,
			&workq_write_register,
			DUMMY_WORK_LEN_ALIGN
		);

		// RECEIVE
		mem_workq_add_thread(
			ISL_REPLY_RING_NUMBER,
			reply_addr,
			&workq_read_register,
			DUMMY_WORK_LEN_ALIGN
		);

		__implicit_read(&workq_read_register);

		t1_part = local_csr_read(local_csr_timestamp_high);
		t1 = (((uint64_t)t1_part) << 32) + local_csr_read(local_csr_timestamp_low);

		time_taken = (uint32_t)(t1 - t0);

		mem_write32(&time_taken, &isl_isl_times[i], sizeof(uint32_t));
	}
}