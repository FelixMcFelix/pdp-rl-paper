#ifndef __RL_PKT_STORE_H__
#define __RL_PKT_STORE_H__

#include <stdint.h>
#include <nfp.h>

#define RL_PKT_MAX_SZ 1500
#define RL_PKT_STORE_COUNT 10

struct rl_pkt_store {
	// Need a mutex
	uint8_t lock;

	// Free list itself...
	// May need to store sizes, too? idk
	__addr40 void *freelist[RL_PKT_STORE_COUNT];
	uint32_t slots_available;

	__addr40 uint8_t *packet_buffer_base;
};

void init_rl_pkt_store(__addr40 __declspec(ctm) struct rl_pkt_store *store, __addr40 __declspec(emem) uint8_t *pkt_base);
__addr40 uint8_t *rl_pkt_get_slot(__addr40 struct rl_pkt_store *store);
void rl_pkt_return_slot(__addr40 struct rl_pkt_store *store, __addr40 uint8_t *slot);

#endif /* __RL_PKT_STORE_H__ */