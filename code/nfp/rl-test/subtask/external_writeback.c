#include <nfp/cls.h>
#include <nfp/me.h>
#include <nfp/mem_atomic.h>
#include "external_writeback.h"

#define READ_REG_TYPE 0

// Details for thread we send to
#define CONSUMER_CTX (0)
#define CONSUMER_ME (0 + 4)
#define CONSUMER_ISLAND (5) // Ideally, this would be a "current island" intrinsic
#define CONSUMER_LOC ((CONSUMER_ISLAND << 4) + CONSUMER_ME)

#include "../worker/worker_signums.h"

// TODO: integrate locking.

volatile __declspec(import, emem) uint64_t really_really_bad;

enum writeback_result external_writeback_ack(
	uint8_t target_slot,
	struct worker_ack ack
) {
	uint64_t nani = 0;
	__declspec(xfer_write_reg) uint64_t nanier = 0;
	SIGNAL local_comms;

	// Most of these are CT const.
	// Target slot *may* be CT const relative to callsite...
	__declspec(write_reg) struct worker_ack tx_reg;
	__declspec(xfer_read_write_reg) uint32_t locking = WB_FLAG_LOCK;
	unsigned int base_address;
	unsigned int remote_sig;
	unsigned int client_sig = __signal_number(&reflector_writeback_sig, CONSUMER_LOC);
	unsigned int register_no =
		(__xfer_reg_number(reflector_writeback, CONSUMER_LOC) & 4095)
		+ (target_slot * ACK_U32S);

	// uint32_t lock_addr = (uint32_t) &(reflector_writeback_locks[target_slot]);
	__addr40 void* em_addr = (__addr40 void*) &(emem_writeback_locks[target_slot]);

	__assign_relative_register(&local_comms, WRITEBACK_SIGNUM);

	if (target_slot >= RL_WORKER_WRITEBACK_SLOTS) {
		return WB_BAD_SLOT;
	}

	/*__asm{
		ctx_arb[bpt]
	}*/

	// __asm {
	// 	cls[test_and_set, locking, lock_addr, 0, 1], ctx_swap[local_comms]
	// }

	mem_test_set((void*)&locking, em_addr, sizeof(uint32_t));

	nani = locking;

	if (locking & WB_FLAG_LOCK) {
		return WB_LOCKED;
	} else if (locking & WB_FLAG_OCCUPIED) {
		// We got the lock, but no space.
		// Release.
		// __asm {
		// 	cls[clr_imm, --, lock_addr, 0, WB_FLAG_LOCK]
		// }
		locking = WB_FLAG_LOCK;

		// __asm {
		// 	cls[clr, locking, lock_addr, 0, 1], ctx_swap[local_comms]
		// }
		mem_test_clr((void*)&locking, em_addr, sizeof(uint32_t));
		return WB_FULL;
	}

	client_sig |= CONSUMER_CTX << 4;

	remote_sig = (client_sig << 9);

	base_address = CONSUMER_ISLAND << 24;
	base_address |= READ_REG_TYPE << 16;
	base_address |= CONSUMER_ME << 10;
	base_address |= register_no << 2;

	tx_reg = ack;

	//sleep(500);

	__asm {
		local_csr_wr[cmd_indirect_ref_0, remote_sig]
		// alu[remote_sig, --, B, 3, <<13] // override ctx
		alu[remote_sig, --, B, 1, <<13] //use current ctx
		ct[reflect_write_sig_both, tx_reg, base_address, 0, ACK_U32S], indirect_ref

		// Unset lock and set occupied status simultaneously.
		// cls[add_imm, --, lock_addr, 0, WB_FLAG_LOCK]
		ctx_arb[local_comms]
	}

	locking = WB_FLAG_OCCUPIED;

	//sleep(1500);

	mem_write_atomic((void*)&locking, em_addr, sizeof(uint32_t));

	// sleep(1500);

	// __asm {
	// 	cls[test_set, locking, lock_addr, 0, 1], ctx_swap[local_comms]
	// }

	// mem_test_set((void*)&locking, em_addr, sizeof(uint32_t));

	// nani = (locking << 8);

	// locking = WB_FLAG_LOCK;

	// sleep(1500);

	// // __asm {
	// // 	cls[test_clr, locking, lock_addr, 0, 1], ctx_swap[local_comms]
	// // }

	// mem_test_clr((void*)&locking, em_addr, sizeof(uint32_t));

	// nani = (locking << 16);

	//nanier = nani;

	//mem_write64(&nanier, &really_really_bad, sizeof(uint64_t));

	return WB_SUCCESS;
}

enum writeback_result external_writeback_ack_ctx(
	uint8_t target_slot,
	struct worker_ack ack,
	uint8_t consumer_ctx
) {
	__declspec(xfer_read_write_reg) uint32_t locking = WB_FLAG_LOCK;
	__declspec(write_reg) struct worker_ack tx_reg;
	unsigned int base_address;
	unsigned int remote_sig;
	unsigned int client_sig = __signal_number(&reflector_writeback_sig, CONSUMER_LOC);
	unsigned int register_no =
		(__xfer_reg_number(reflector_writeback, CONSUMER_LOC) & 4095)
		+ (target_slot * ACK_U32S);

	uint32_t lock_addr = (uint32_t) &(reflector_writeback_locks[target_slot]);

	if (target_slot >= RL_WORKER_WRITEBACK_SLOTS) {
		return WB_BAD_SLOT;
	}

	__asm {
		cls[test_and_set, locking, lock_addr, 0, 1]
	}

	if (locking & WB_FLAG_LOCK) {
		return WB_LOCKED;
	} else if (locking & WB_FLAG_OCCUPIED) {
		// We got the lock, but no space.
		// Release.
		__asm {
			cls[clr_imm, --, lock_addr, 0, WB_FLAG_LOCK]
		}
		return WB_FULL;
	}

	client_sig |= consumer_ctx << 4;

	remote_sig = (client_sig << 9);

	base_address = CONSUMER_ISLAND << 24;
	base_address |= READ_REG_TYPE << 16;
	base_address |= CONSUMER_ME << 10;
	base_address |= register_no << 2;

	tx_reg = ack;

	__asm {
		local_csr_wr[cmd_indirect_ref_0, remote_sig]
		alu[remote_sig, --, B, 3, <<13] // override ctx
		//alu[remote_sig, --, B, 1, <<13] //use current ctx
		ct[reflect_write_sig_remote, tx_reg, base_address, 0, ACK_U32S], indirect_ref

		// Unset lock and set occupied status simultaneously.
		cls[add_imm, --, lock_addr, 0, WB_FLAG_LOCK]
	}

	return WB_SUCCESS;
}

__intrinsic void atomic_ack() {
	__declspec(xfer_read_write_reg) uint32_t my_slot = 1;
	cls_test_add(&my_slot, &atomic_writeback_acks, sizeof(uint32_t));

	return my_slot;
}

__intrinsic uint32_t atomic_writeback_slot() {
	__declspec(xfer_read_write_reg) uint32_t my_slot = 1;
	cls_test_add(&my_slot, &atomic_writeback_hit_count, sizeof(uint32_t));

	return my_slot;
}
