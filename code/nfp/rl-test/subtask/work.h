#ifndef _WORK_H
#define _WORK_H

#include <stdint.h>
#include "../rl.h"

enum work_type {
	WORK_REQUEST_WORKER_COUNT,
	WORK_SET_WORKER_COUNT,
	// WORK_SET_BASE_WORKER_COUNT,
	WORK_NEW_CONFIG,
	WORK_STATE_VECTOR,
	WORK_ALLOCATE,
	WORK_UPDATE_POLICY
};

enum alloc_strategy {
	ALLOC_CHUNK,
	ALLOC_STRIDE,
	ALLOC_STRIDE_OFFSET,
	ALLOC_FILL_HEAVY
};

struct work_alloc {
	__addr40 _declspec(emem) uint32_t *work_indices;
	enum alloc_strategy strat;
};

struct action_update {
	tile_t delta;
	uint16_t action;
	__addr40 _declspec(emem) tile_t *state;
};

union work_body {
	uint32_t worker_count;
	__addr40 _declspec(emem) struct rl_config *cfg;
	__addr40 _declspec(emem) tile_t *state; // len not needed: derived from config.
	struct work_alloc alloc;
	struct action_update update;
};

struct work {
	enum work_type type;
	uint8_t req_ctx_num;
	union work_body body;
};

struct dim_cache {
	tile_t min;
	tile_t adjusted_max;
	tile_t width;
	tile_t shift_amt;
	uint8_t dim;
};

struct work_item_cache {
	enum tile_location loc;
	uint16_t num_dims;
	uint32_t loc_base; // needed for prefs! this is "base"
	uint32_t start_tile;
	struct dim_cache per_dim[T3_MAX_DIMS];
};

#endif /* !_WORK_H_ */