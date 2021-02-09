#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <nfp/remote_me.h>
#include <rtl.h>

#include "shared_defs.h"

__declspec(export, emem) uint32_t ref_times_ct = 0;

// See https://groups.google.com/g/open-nfp/c/gAIBsqolNdY/m/gjbe4VIzBwAJ
// for reflector usage.

// See NFPCC p.58
// this is "ME0 program"
__declspec(write_reg) uint32_t me0_x;
__declspec(remote read_reg) uint32_t receiver_x;
__declspec(remote) SIGNAL receiver_sig;
//__assign_relative_register(&me1_sig, SIG_USE);

__declspec(visible read_reg) uint32_t foreign_x;
__declspec(visible) SIGNAL foreign_sig;

#define READ_REG_TYPE 0

// This run on 12.1

// details of thread who sends to us
#define PRODUCER_ME 4 // 0 + 4;
#define PRODUCER_ISLAND 12
#define PRODUCER_LOC (PRODUCER_ISLAND << 4) + PRODUCER_ME

// Details for thread we send to
#define CONSUMER_CTX 0
#define CONSUMER_ME 4 // 0 + 4.
#define CONSUMER_ISLAND 12
#define CONSUMER_LOC (CONSUMER_ISLAND << 4) + CONSUMER_ME

main() {
	uint32_t i = 0;
	uint32_t new_i = 0;
	unsigned int base_address;
	unsigned int remote_sig;
	unsigned int client_sig = __signal_number(&receiver_sig, CONSUMER_LOC);
	unsigned int register_no = __xfer_reg_number(&receiver_x, CONSUMER_LOC) & 4095;

	__declspec(xfer_write_reg) uint32_t cter;

	client_sig |= CONSUMER_CTX << 4;

	remote_sig = (client_sig << 9);

	base_address = CONSUMER_ISLAND << 24;
	base_address |= READ_REG_TYPE << 16;
	base_address |= CONSUMER_ME << 10;
	base_address |= register_no << 2;

	if (__ctx() != 0) {
		return 0;
	}

	__implicit_write(&foreign_x);
	__implicit_write((void*)&foreign_sig);

	while (1) {
		__implicit_write(&foreign_x);
		__implicit_write((void*)&foreign_sig);

		if (!signal_test(&foreign_sig)) {
			__implicit_write(&foreign_x);
			__implicit_write((void*)&foreign_sig);
			__wait_for_all(&foreign_sig);	
		}
		
		new_i = foreign_x;

		me0_x = new_i;

		__asm {
			local_csr_wr[cmd_indirect_ref_0, remote_sig]
			alu[remote_sig, --, B, 3, <<13]
			ct[reflect_write_sig_remote, me0_x, base_address, 0, 1], indirect_ref
		}

		mem_write32(&cter, &ref_times_ct, new_i);
	}
}