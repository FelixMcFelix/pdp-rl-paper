#ifndef _WORK_H
#define _WORK_H

#include <stdint.h>
#include "../rl.h"

enum work_type {
	WORK_REQUEST_WORKER_COUNT,
	WORK_SET_WORKER_COUNT,
	WORK_SET_BASE_WORKER_COUNT,
	WORK_NEW_CONFIG,
	WORK_STATE_VECTOR,
	WORK_ALLOCATE
};

enum alloc_strategy {
	ALLOC_CHUNK,
	ALLOC_STRIDE,
	ALLOC_STRIDE_OFFSET
};

struct work_alloc {
	__addr40 _declspec(emem) uint32_t *work_indices;
	enum alloc_strategy strat;
};

union work_body {
	uint32_t worker_count;
	__addr40 _declspec(emem) struct rl_config *cfg;
	__addr40 _declspec(emem) tile_t *state; // len not needed: derived from config.
	struct work_alloc alloc;
};

struct work {
	enum work_type type;
	uint8_t req_ctx_num;
	union work_body body;
};

#endif /* !_WORK_H_ */