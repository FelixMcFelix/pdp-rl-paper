#ifndef _EXTERNAL_WRITEBACK_H
#define _EXTERNAL_WRITEBACK_H

#include <stdint.h>
#include "../rl.h"
#include "../worker_config.h"
#include "common_writeback.h"

#ifndef _RL_CORE_OLD_POLICY_WORK
__declspec(remote read_reg) struct worker_ack reflector_writeback[RL_WORKER_WRITEBACK_SLOTS];
__declspec(remote) SIGNAL reflector_writeback_sig;
__declspec(import, cls) uint32_t reflector_writeback_locks[RL_WORKER_WRITEBACK_SLOTS];
__declspec(import, cls) uint32_t spare_lock;

__declspec(import, emem) uint32_t emem_writeback_locks[RL_WORKER_WRITEBACK_SLOTS];
#endif /* _RL_CORE_OLD_POLICY_WORK */

enum writeback_result external_writeback_ack(
	uint8_t target_slot,
	struct worker_ack ack
);

enum writeback_result external_writeback_ack_ctx(
	uint8_t target_slot,
	struct worker_ack ack,
	uint8_t consumer_ctx
);

#endif /* !_EXTERNAL_WRITEBACK_H_ */