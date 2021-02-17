#ifndef _ACK_H
#define _ACK_H

enum ack_type {
	ACK_NO_BODY,
	ACK_WORKER_COUNT,
	ACK_
};

union ack_body {
	uint32_t worker_count;
};

struct worker_ack {
	enum ack_type type;
	union ack_body body;
};

#endif /* !_ACK_H_ */