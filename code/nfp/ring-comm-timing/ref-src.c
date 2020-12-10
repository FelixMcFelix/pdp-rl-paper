#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <rtl.h>

#include "shared_defs.h"

__declspec(export, emem) uint32_t ref_times[NUM_MEASUREMENTS] = {0};

__declspec(visible read_reg) uint32_t receiver_x;
__declspec(visible) SIGNAL receiver_sig;

__declspec(write_reg) uint32_t me0_x;
__declspec(remote read_reg) uint32_t foreign_x;
__declspec(remote) SIGNAL foreign_sig;

// details of thread who sends to us
#define READ_REG_TYPE 0
#define PRODUCER_ME 5 // 1 + 4;
#define PRODUCER_ISLAND 12
#define PRODUCER_LOC (PRODUCER_ISLAND << 4) + PRODUCER_ME

// Details for thread we send to
#define CONSUMER_CTX 0
#define CONSUMER_ME 5 // 1 + 4.
#define CONSUMER_ISLAND 12
#define CONSUMER_LOC (CONSUMER_ISLAND << 4) + CONSUMER_ME

main() {
	uint32_t t0_part;
	uint32_t t1_part;
	uint64_t t0;
	uint64_t t1;
	__declspec(xfer_write_reg) uint32_t time_taken;
	int i = 0;
	uint32_t new_val = 0;

	unsigned int base_address;
	unsigned int remote_sig;
	unsigned int client_sig = __signal_number(&foreign_sig, CONSUMER_LOC);
	unsigned int register_no = __xfer_reg_number(&foreign_x, CONSUMER_LOC) & 4095;

	if (__ctx() != 0) {
		return 0;
	}

	client_sig |= CONSUMER_CTX << 4;

	base_address = CONSUMER_ISLAND << 24;
	base_address |= READ_REG_TYPE << 16;
	base_address |= CONSUMER_ME << 10;
	base_address |= register_no << 2;

	remote_sig = (1 << 13) | (client_sig << 9);

	__implicit_write(&receiver_x);
	__implicit_write((void*)&receiver_sig);

	for (i=1; i < (NUM_MEASUREMENTS + 1); i++) {
		t0_part = local_csr_read(local_csr_timestamp_high);
		t0 = (((uint64_t)t0_part) << 32) + local_csr_read(local_csr_timestamp_low);

		// write i into the remote register.
		//in_val = i;

		me0_x = i;

		__asm {
			local_csr_wr[cmd_indirect_ref_0, remote_sig]
			alu[remote_sig, --, B, 3, <<13]
			ct[reflect_write_sig_remote, me0_x, base_address, 0, 1], indirect_ref
		}

		__implicit_write(&receiver_x);
		__implicit_write((void*)&receiver_sig);

		__wait_for_all(&receiver_sig);
		new_val = receiver_x;

		t1_part = local_csr_read(local_csr_timestamp_high);
		t1 = (((uint64_t)t1_part) << 32) + local_csr_read(local_csr_timestamp_low);

		time_taken = (uint32_t)(t1 - t0);

		mem_write32(&time_taken, &ref_times[i], sizeof(uint32_t));
	}
}