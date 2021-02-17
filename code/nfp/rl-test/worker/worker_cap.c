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

__nnr struct work in_type_a;

main() {
	int i = 0;

	#ifdef _RL_WORKER_DISABLED
	return 0;
	#endif /* _RL_WORKER_DISABLED */

	#ifdef _RL_WORKER_SLAVE_CTXES

	if (__ctx() == 0) {
		// DO something: main prog.
	} else {
		// Slave time.
	}

	#else /* if !_RL_WORKER_SLAVE_CTXES */

	// DO something: main prog.

	#endif /* _RL_WORKER_SLAVE_CTXES */
}
