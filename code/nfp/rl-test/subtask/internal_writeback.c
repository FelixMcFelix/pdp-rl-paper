#include <nfp/cls.h>
#include <nfp/me.h>
#include <nfp/mem_atomic.h>
#include "internal_writeback.h"

#define READ_REG_TYPE 0

#define CONSUMER_CTX (0)
#define CONSUMER_ME (0 + 4)
#define CONSUMER_ISLAND (5) // Ideally, this would be a "current island" intrinsic
#define CONSUMER_LOC ((CONSUMER_ISLAND << 4) + CONSUMER_ME)

#ifndef _RL_WORKER_DISABLED

__intrinsic void atomic_ack() {
	__declspec(xfer_read_write_reg) uint32_t my_slot = 1;
	cls_test_add(&my_slot, &atomic_writeback_acks, sizeof(uint32_t));
}

#endif /* !_RL_WORKER_DISABLED */
