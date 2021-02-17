#ifndef _WORK_H
#define _WORK_H

#include "../rl.h"

enum work_type {
	WORK_REQUEST_WORKER_COUNT,
	WORK_SET_WORKER_COUNT,
	WORK_NEW_CONFIG,
	WORK_STATE_VECTOR
};

union work_body {
	uint32_t worker_count;
	__addr40 _declspec(emem) struct rl_config *cfg;
	tile_t state[RL_DIMENSION_MAX];
};

struct work {
	enum work_type type;
	union work_body body;
};

#endif /* !_WORK_H_ */