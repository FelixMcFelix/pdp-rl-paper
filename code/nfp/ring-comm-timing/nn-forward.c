#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h>
#include <nfp/mem_atomic.h>
#include <nfp/remote_me.h>
#include <rtl.h>

#include "shared_defs.h"

__nnr uint32_t in_val;

// See https://groups.google.com/g/open-nfp/c/gAIBsqolNdY/m/gjbe4VIzBwAJ
// for reflector usage.

// See NFPCC p.58
// this is "ME0 program"
__declspec(write_reg) uint32_t me0_x;
__declspec(remote read_reg) uint32_t me1_x;
__declspec(remote) SIGNAL me1_sig;
//__assign_relative_register(&me1_sig, SIG_USE);

#define READ_REG_TYPE 0
#define CONSUMER_CTX 0
#define CONSUMER_ME 4 // 0 + 4.
#define CONSUMER_ISLAND 4
#define CONSUMER_LOC (CONSUMER_ISLAND << 4) + CONSUMER_ME

main() {
	uint32_t i = 0;
	uint32_t new_i = 0;

	// temp to allow debugging other ctxs.
	return 0;

	if (__ctx() != 0) {
		return 0;
	}

	__implicit_write(&in_val);

	while (1) {
		__implicit_write(&in_val);
		new_i = in_val;
		if (new_i != i) {
			unsigned int base_address;
			unsigned int remote_sig;
			unsigned int client_sig = __signal_number(&me1_sig, CONSUMER_LOC);
			unsigned int register_no = __xfer_reg_number(&me1_x, CONSUMER_LOC) & 4095;
			i = new_i;

			me0_x = new_i;

			client_sig |= CONSUMER_CTX << 4;

			base_address = CONSUMER_ISLAND << 24;
			base_address |= READ_REG_TYPE << 16;
			base_address |= CONSUMER_ME << 10;
			base_address |= register_no << 2;

			remote_sig = (1 << 13) | (client_sig << 9);

			__asm {
				local_csr_wr[cmd_indirect_ref_0, remote_sig]
				alu[remote_sig, --, B, 3, <<13]
				ct[reflect_write_sig_remote, me0_x, base_address, 0, 1], indirect_ref
			}

			__free_write_buffer(&me0_x);
		}
	}
}