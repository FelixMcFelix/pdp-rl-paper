#ifndef _PACKET_H
#define _PACKET_H

#include <stdint.h>
#include "../rl.h"

void setup_packet(
	__addr40 _declspec(emem) struct rl_config *cfg,
	__declspec(xfer_read_reg) struct rl_work_item *pkt
);

void tilings_packet(
	__addr40 _declspec(emem) struct rl_config *cfg,
	__declspec(xfer_read_reg) struct rl_work_item *pkt
);

void policy_block_copy(
	__addr40 _declspec(emem) struct rl_config *cfg,
	__declspec(xfer_read_reg) struct rl_work_item *pkt,
	uint32_t tile
);

void state_packet(
	__addr40 _declspec(emem) struct rl_config *cfg,
	__declspec(xfer_read_reg) struct rl_work_item *pkt,
	uint16_t dim_count
);

void reward_packet(
	__addr40 _declspec(emem) struct rl_config *cfg,
	union pad_tile value,
	uint32_t reward_insert_loc
);

#endif /* !_PACKET_H_ */