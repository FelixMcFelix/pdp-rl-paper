#include "internal_writeback.h"

#define READ_REG_TYPE 0

#define CONSUMER_CTX (0)
#define CONSUMER_ME (0 + 4)
#define CONSUMER_ISLAND (5) // Ideally, this would be a "current island" intrinsic
#define CONSUMER_LOC ((CONSUMER_ISLAND << 4) + CONSUMER_ME)

enum writeback_result internal_writeback_ack(
	uint8_t target_slot,
	struct worker_ack ack,
	unsigned int client_sig
) {
	// Most of these are CT const.
	// Target slot *may* be CT const relative to callsite...
	__declspec(write_reg) struct worker_ack tx_reg;
	__declspec(xfer_read_write_reg) uint32_t locking = WB_FLAG_LOCK;
	unsigned int base_address;
	unsigned int remote_sig;
	unsigned int register_no =
		(__xfer_reg_number(reflector_writeback) & 4095)
		+ (target_slot * ACK_U32S);

	uint32_t lock_addr = (uint32_t) &(reflector_writeback_locks[target_slot]);

	if (target_slot >= RL_WORKER_WRITEBACK_SLOTS) {
		really_really_bad |= locking << 8;
		really_really_bad |= reflector_writeback_locks[target_slot] << 16;
		really_really_bad |= 1 << 24;
		return WB_BAD_SLOT;
	}

	__asm {
		cls[test_and_set, locking, lock_addr, 0, 1]
	}

	if (locking & WB_FLAG_LOCK) {
		really_really_bad |= locking << 8;
		really_really_bad |= reflector_writeback_locks[target_slot] << 16;
		really_really_bad |= 2 << 24;
		return WB_LOCKED;
	} else if (locking & WB_FLAG_OCCUPIED) {
		// We got the lock, but no space.
		// Release.
		__asm {
			cls[clr_imm, --, lock_addr, 0, WB_FLAG_LOCK]
		}

		really_really_bad |= locking << 8;
		really_really_bad |= reflector_writeback_locks[target_slot] << 16;
		really_really_bad |= 3 << 24;
		return WB_FULL;
	}

	client_sig |= CONSUMER_CTX << 4;

	remote_sig = (client_sig << 9);

	base_address = CONSUMER_ISLAND << 24;
	base_address |= READ_REG_TYPE << 16;
	base_address |= CONSUMER_ME << 10;
	base_address |= register_no << 2;

	tx_reg = ack;

	__asm {
		local_csr_wr[cmd_indirect_ref_0, remote_sig]
		// alu[remote_sig, --, B, 3, <<13] // override ctx
		alu[remote_sig, --, B, 1, <<13] //use current ctx
		ct[reflect_write_sig_remote, tx_reg, base_address, 0, ACK_U32S], indirect_ref

		// Unset lock and set occupied status simultaneously.
		cls[add_imm, --, lock_addr, 0, WB_FLAG_LOCK]
	}

	really_really_bad |= locking << 8;
	really_really_bad |= reflector_writeback_locks[target_slot] << 16;
	really_really_bad |= 4 << 24;
	return WB_SUCCESS;
}

