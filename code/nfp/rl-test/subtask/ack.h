#ifndef _ACK_H
#define _ACK_H

#include <stdint.h>
#include "../rl.h"

enum ack_type {
	ACK_NO_BODY,
	ACK_WORKER_COUNT,
	ACK_VALUE_SET
};

struct value_set {
	uint32_t num_items;
	tile_t prefs[MAX_ACTIONS];
};

union ack_body {
	uint32_t worker_count;
	__addr40 _declspec(emem) struct value_set *value_set;
};

struct worker_ack {
	enum ack_type type;
	union ack_body body;
};

#endif /* !_ACK_H_ */