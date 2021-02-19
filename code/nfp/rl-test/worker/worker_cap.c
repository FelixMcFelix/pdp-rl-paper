#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <rtl.h>

#include "../worker_config.h"
#include "../subtask/work.h"
#include "../subtask/ack.h"

#include "worker_signal.h"

__declspec(export, emem) struct work cap_work = {0};

__nnr struct work in_type_a;

// __declspec(remote read_reg) uint32_t receiver_x;
// __declspec(remote) SIGNAL receiver_sig;

#ifndef _RL_CORE_OLD_POLICY_WORK
// __declspec(remote read_reg) struct worker_ack reflector_writeback[RL_WORKER_WRITEBACK_SLOTS];
__declspec(remote xfer_read_reg) struct worker_ack reflector_writeback;
__declspec(remote) SIGNAL reflector_writeback_sig;
__declspec(import, emem) uint32_t reflector_writeback_locks[RL_WORKER_WRITEBACK_SLOTS];
#endif /* _RL_CORE_OLD_POLICY_WORK */

#define READ_REG_TYPE 0

// Details for thread we send to
#define CONSUMER_CTX 0
#define CONSUMER_ME 4 // 0 + 4.
#define CONSUMER_ISLAND 5
#define CONSUMER_LOC (CONSUMER_ISLAND << 4) + CONSUMER_ME

main() {
	int i = 0;
	__assign_relative_register(&worker_in_sig, WORKER_SIGNUM);

	#ifdef _RL_WORKER_DISABLED
	return 0;
	#endif /* _RL_WORKER_DISABLED */

	#ifdef _RL_WORKER_SLAVE_CTXES

	if (__ctx() == 0) {
		__declspec(xfer_write_reg) struct worker_ack tx_reg;
		uint32_t last_estimate = 0;

		unsigned int base_address;
		unsigned int remote_sig;
		unsigned int client_sig = __signal_number(&reflector_writeback_sig, CONSUMER_LOC);
		// unsigned int register_no = __xfer_reg_number(&reflector_writeback[0], CONSUMER_LOC) & 4095;
		unsigned int register_no = __xfer_reg_number(&reflector_writeback, CONSUMER_LOC) & 4095;

		client_sig |= CONSUMER_CTX << 4;

		remote_sig = (client_sig << 9);

		base_address = CONSUMER_ISLAND << 24;
		base_address |= READ_REG_TYPE << 16;
		base_address |= CONSUMER_ME << 10;
		base_address |= register_no << 2;

		// DO something: main prog.
		while (1) {
			uint32_t new_est = 0;

			__implicit_write(&in_type_a);
			__implicit_write(&worker_in_sig);

			wait_for_all(&worker_in_sig);

			new_est = in_type_a.body.worker_count + __n_ctx();

			// Only write and signal if this is actually meaningful work.
			if (new_est > last_estimate) {
				// SIGNAL

				cap_work.type = in_type_a.type;
				cap_work.body.worker_count = new_est;
				cap_work.type = base_address;

				tx_reg.type = ACK_WORKER_COUNT;
				tx_reg.body.worker_count = new_est;

				__asm {
					local_csr_wr[cmd_indirect_ref_0, remote_sig]
					alu[remote_sig, --, B, 3, <<13]
					//alu[remote_sig, --, B, 1, <<13]
					ct[reflect_write_sig_remote, tx_reg, base_address, 0, (sizeof(struct worker_ack) + 3) / 4], indirect_ref
				}
			}

			last_estimate = new_est;
		}
	} else {
		// Slave time.
	}

	#else /* if !_RL_WORKER_SLAVE_CTXES */

	// DO something: main prog.
	while (1) {
		cap_work = in_type_a;
	}

	#endif /* _RL_WORKER_SLAVE_CTXES */
}
