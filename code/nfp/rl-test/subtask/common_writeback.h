#ifndef _COMMON_WRITEBACK_H
#define _COMMON_WRITEBACK_H

#include "ack.h"

enum writeback_result {
	WB_SUCCESS,
	WB_LOCKED,
	WB_FULL,
	WB_BAD_SLOT
};

#define RL_WORKER_WRITEBACK_SLOTS (5)

#define WB_FLAG_LOCK     (1)
#define WB_FLAG_OCCUPIED (1 << 1)

#endif /* !_COMMON_WRITEBACK_H_ */