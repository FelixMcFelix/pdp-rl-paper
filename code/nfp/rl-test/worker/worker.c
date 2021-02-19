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

__nnr struct work in_type_a;
__declspec(nn_remote_reg) struct work in_type_b = {0};

main() {
	int i = 0;
	__assign_relative_register(&worker_in_sig, WORKER_SIGNUM);

	#ifdef _RL_WORKER_DISABLED
	return 0;
	#endif /* _RL_WORKER_DISABLED */

	#ifdef _RL_WORKER_SLAVE_CTXES

	if (__ctx() == 0) {
		// DO something: main prog.
		while (1) {
			__implicit_write(&in_type_a);
			__implicit_write(&worker_in_sig);

			wait_for_all(&worker_in_sig);

			in_type_b.type = in_type_a.type;
			in_type_b.body.worker_count = in_type_a.body.worker_count + __n_ctx();

			local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (WORKER_SIGNUM << 3));
		}
	} else {
		// Slave time.
	}

	#else /* if !_RL_WORKER_SLAVE_CTXES */

	// DO something: main prog.

	while (1) {
		in_type_b = in_type_a;
	}

	#endif /* _RL_WORKER_SLAVE_CTXES */
}
