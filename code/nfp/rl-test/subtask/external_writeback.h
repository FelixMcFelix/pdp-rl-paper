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

__declspec(import, cls) uint32_t atomic_writeback_acks;
__declspec(import, cls) uint32_t atomic_writeback_prefs[MAX_ACTIONS];

#ifdef WORKER_BARGAIN_BUCKET_SIMD
__declspec(import, cls) uint64_t atomic_writeback_prefs_simd[MAX_SIMD_WB_SLOTS];
#endif /* WORKER_BARGAIN_BUCKET_SIMD */

#endif /* _RL_CORE_OLD_POLICY_WORK */

__intrinsic void atomic_ack();

#endif /* !_EXTERNAL_WRITEBACK_H_ */