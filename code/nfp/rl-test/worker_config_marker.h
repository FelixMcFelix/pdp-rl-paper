#ifndef _WORK_CONFIG_MARKER_H
#define _WORK_CONFIG_MARKER_H

// --------------------
// Core design
// --------------------

// #define _RL_CONFIG_SOLO_INDEP_RL_CORES

// #define _RL_CONFIG_INDEP_PIPES

// #define _RL_CONFIG_SOLO_LOCAL_WORK

#define _RL_CONFIG_SOLO_REMOTE_WORK

// #define _RL_CONFIG_INDEP_REMOTE_WORK

// --------------------
// OPTIONS
// --------------------

/* -- Ideally, combine with ALLOC_CHUNK -- */
// #define WORK_ALLOC_RANDOMISE

#define WORK_ALLOC_STRAT ALLOC_CHUNK
// #define WORK_ALLOC_STRAT ALLOC_STRIDE
// #define WORK_ALLOC_STRAT ALLOC_STRIDE_OFFSET

#define WORKER_LOCAL_PREF_SLOTS 2

#endif /* !_WORK_CONFIG_MARKER_H_ */
