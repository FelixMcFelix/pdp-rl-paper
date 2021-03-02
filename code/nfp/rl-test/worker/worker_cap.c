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
#include "../subtask/external_writeback.h"

#include "worker_signal.h"

__declspec(export, emem) struct work cap_work = {0};
__declspec(export, emem) uint32_t bodied = 0;

__nnr struct work in_type_a;

#define IN_PORT in_type_a
#define OUT_PORT in_type_b
#define CAP

// C INCLUDE
// WARNING
#include "worker_body.c"
// WARNING

main() {
	int i = 0;
	__assign_relative_register(&worker_in_sig, WORKER_SIGNUM);

	#ifdef _RL_WORKER_DISABLED
	return 0;
	#endif /* _RL_WORKER_DISABLED */

	#ifdef _RL_WORKER_SLAVE_CTXES
	work(__ctx() == 0, 0);
	#else /* if !_RL_WORKER_SLAVE_CTXES */
	work(1, 0);
	#endif /* _RL_WORKER_SLAVE_CTXES */
}
