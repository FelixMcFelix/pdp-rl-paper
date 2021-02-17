#ifndef _WORKER_CONFIG_H
#define _WORKER_CONFIG_H

#include "worker_config_marker.h"

/*

ALL FLAGS:
 _RL_CORE_OLD_POLICY_WORK
  - Use the legacy, on core policy exec/update.
 _RL_CORE_SLAVE_CTXES
  - CTX != 0 on RL Core ME act as workers.
 _RL_CORE_LOCK_WORKERS
  - Access to remote worker threads is exclusively locked.
 _RL_WORKER_DISABLED
  - All remote worker threads are disabled/inactive.
 _RL_WORKER_NN_PER_CTX
  - RL worker CTXs are independent, each have their own NN registers.
 _RL_WORKER_SLAVE_CTXES
  - RL worker CTXs are signalled by CTX0, who controls NN register use.

*/

// Basic: Indep RL cores, all local.
#ifdef _RL_CONFIG_SOLO_INDEP_RL_CORES
#define _RL_CORE_OLD_POLICY_WORK
#define _RL_WORKER_DISABLED
#endif /* _RL_CONFIG_SOLO_INDEP_RL_CORES */



// RL cores reach workers over NN reg.
// All independent pipes, no locks.
#ifdef _RL_CONFIG_INDEP_PIPES
#define _RL_WORKER_NN_PER_CTX
#endif /* _RL_CONFIG_INDEP_PIPES */



// RL core reaches workers as CTXs on same core.
// Only one master == CTX 0.
#ifdef _RL_CONFIG_SOLO_LOCAL_WORK
#define _RL_CORE_SLAVE_CTXES
#define _RL_WORKER_DISABLED
#endif /* _RL_CONFIG_SOLO_LOCAL_WORK */



// RL core reaches workers as CTXs on same core, and via NNs.
//
// Only one master == CTX 0.
#ifdef _RL_CONFIG_SOLO_REMOTE_WORK
#define _RL_WORKER_SLAVE_CTXES
#define _RL_CORE_SLAVE_CTXES
#endif /* _RL_CONFIG_SOLO_REMOTE_WORK */



// RL cores reach workers via NNs. This is a full-size block, using all CTXs to perform ONE task.
//
// Access to compute block is locked, shared between 
#ifdef _RL_CONFIG_INDEP_REMOTE_WORK
#define _RL_WORKER_SLAVE_CTXES
#define _RL_CORE_LOCK_WORKERS
#endif /* _RL_CONFIG_INDEP_REMOTE_WORK */

#endif /* !_WORKER_CONFIG_H_ */
