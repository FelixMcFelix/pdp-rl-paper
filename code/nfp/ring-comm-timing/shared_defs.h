#ifndef _COMM_TIME_H
#define _COMM_TIME_H

#define ME_ME_RING_NUMBER 129
#define ME_REPLY_RING_NUMBER 130
#define ISL_ISL_RING_NUMBER 131
#define ISL_REPLY_RING_NUMBER 132

#define NUM_MEASUREMENTS 2048

struct dummy_work_item {
	uint32_t test;
};

struct done_t {
	uint32_t lock_val;
	uint32_t done;
};

#define DUMMY_WORK_LWS ((sizeof(struct dummy_work_item) + 3) >> 2)
#define DUMMY_WORK_LEN_ALIGN (DUMMY_WORK_LWS << 2)

#endif /* !_COMM_TIME_H_ */