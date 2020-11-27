#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <rtl.h>

#include "shared_defs.h"

__declspec(import, addr40, emem) struct done_t me_time_lock;

volatile __emem_n(0) __declspec(import, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t me_time_workq[512*1024] = {0};
volatile __emem_n(0) __declspec(import, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t me_reply_workq[512*1024] = {0};

main() {
	mem_ring_addr_t r_addr;
	mem_ring_addr_t reply_addr;

	SIGNAL sig;
	__declspec(xfer_write_reg) uint32_t time_taken;
	__declspec(write_reg) struct dummy_work_item workq_write_register;
	__declspec(read_reg) struct dummy_work_item workq_read_register;
	__xrw uint32_t xfer = 1;
	int can_leave = 0;

	while (!can_leave) {
		__xrw uint32_t xfer = 1;

		while (xfer == 1) {
			mem_test_set((void*)&xfer, (__addr40 void*)&(me_time_lock.lock_val), sizeof(uint32_t));
		}

		can_leave = me_time_lock.done;

		xfer = 0;
		mem_write_atomic(&xfer, (__addr40 void*)&(me_time_lock.lock_val), sizeof(uint32_t));
	}

	r_addr = mem_ring_get_addr(me_time_workq);
	reply_addr = mem_ring_get_addr(me_reply_workq);

	for (;;) {
		// RECEIVE
		mem_workq_add_thread(
			ME_ME_RING_NUMBER,
			r_addr,
			&workq_read_register,
			DUMMY_WORK_LEN_ALIGN
		);

		//workq_write_register = workq_read_register;

		// SEND
		mem_workq_add_work(
			ME_REPLY_RING_NUMBER,
			reply_addr,
			&workq_write_register,
			DUMMY_WORK_LEN_ALIGN
		);
	}
}