#ifndef _INTERNAL_WRITEBACK_H
#define _INTERNAL_WRITEBACK_H

#include <stdint.h>
#include "../rl.h"
#include "../worker_config.h"
#include "common_writeback.h"

#ifndef _RL_CORE_OLD_POLICY_WORK
__declspec(visible read_reg) volatile struct worker_ack reflector_writeback[RL_WORKER_WRITEBACK_SLOTS];
__declspec(visible) volatile SIGNAL reflector_writeback_sig;
volatile SIGNAL internal_handout_sig;
__declspec(export, cls) uint32_t reflector_writeback_locks[RL_WORKER_WRITEBACK_SLOTS] = {0};
__declspec(export, cls) uint32_t spare_lock = 0;

__declspec(export, emem) uint32_t emem_writeback_locks[RL_WORKER_WRITEBACK_SLOTS] = {0};

// full disclosure, this might just spookily *not work*.
// fall back to CTM in worst case.
__declspec(export, cls) uint32_t atomic_writeback_acks = 0;
__declspec(export, cls) uint32_t atomic_writeback_prefs[MAX_ACTIONS] = {0};

#ifdef WORKER_BARGAIN_BUCKET_SIMD
__declspec(export, cls) uint64_t atomic_writeback_prefs_simd[MAX_SIMD_WB_SLOTS] = {0};
#endif /* WORKER_BARGAIN_BUCKET_SIMD */

#endif /* _RL_CORE_OLD_POLICY_WORK */

__intrinsic void atomic_ack();

#endif /* !_INTERNAL_WRITEBACK_H_ */