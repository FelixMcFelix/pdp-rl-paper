#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_atomic.h>
#include "rl-pkt-store.h"

// Need a free list full of void pointers.
// __addr40 void *rl_pkt_freelist[RL_PKT_STORE_COUNT] = {0};
// uint32_t rl_pkt_available = 0;

void init_rl_pkt_store(
	__addr40 __declspec(emem) struct rl_pkt_store *store,
	__addr40 __declspec(emem) uint8_t *pkt_base,
	uint32_t byte_ct
	) {
	int slot;
	__declspec(emem) __addr40 uint8_t *pkt_cursor = pkt_base;
	store->slots_available = RL_PKT_STORE_COUNT;

	// Populate free list according to base pointer.
	for (slot = 0; slot < RL_PKT_STORE_COUNT; ++slot) {
		store->freelist[slot] = pkt_cursor;
		pkt_cursor += byte_ct;
	}

	store->packet_buffer_base = pkt_base;

	// Initialise mutex
	store->lock = 0;
}

__declspec(emem) __addr40 uint8_t *rl_pkt_get_slot(__declspec(emem) __addr40 struct rl_pkt_store *store) {
	__xrw uint32_t xfer = 1;
	__declspec(emem) __addr40 uint8_t *out;

	while (1) {
		mem_test_set((void*)&xfer, (__addr40 void*)&(store->lock), sizeof(uint32_t));
		if (xfer == 1) {
			break;
		} else {
			sleep(100);
		}
	}

	if (store->slots_available) {
		// decrement available count
		// get top entry
		out = store->freelist[--store->slots_available];
	} else {
		// FIXME: try to wake here, or sleep until next free entry added.
		out = 0;
	}

	// unlock mutex
	xfer = 0;
	mem_write_atomic(&xfer, (__addr40 void*)&(store->lock), sizeof(uint32_t));

	return out;
}

void rl_pkt_return_slot(__declspec(emem) __addr40 struct rl_pkt_store *store, __declspec(emem) __addr40 uint8_t *slot) {
	__xrw uint32_t xfer = 1;

	// lock mutex
	while (1) {
		mem_test_set((void*)&xfer, (__addr40 void*)&(store->lock), sizeof(uint32_t));
		if (xfer == 1) {
			break;
		} else {
			sleep(100);
		}
	}

	// put slot into top entry
	// increment available count
	store->freelist[store->slots_available++] = slot;

	// unlock mutex
	xfer = 0;
	mem_write_atomic(&xfer, (__addr40 void*)&(store->lock), sizeof(uint32_t));
}