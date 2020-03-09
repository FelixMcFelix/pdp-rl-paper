#include <pif_headers.h>
#include <pif_plugin.h>
#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp_mem_workq.h>
#include "rl-proto.h"
#include "rl.h"
#include "rl-pkt-store.h"

// add "$(SDKDIR)\\components\\standardlibrary\\microc\\src\\nfp_mem_workq.c" to project's "c_source_files"
#include <nfp_mem_ring.h>

// https://groups.google.com/d/msg/open-nfp/K9qJRHPsf4c/lZSC7_BhCAAJ
// rely on this link, it has much useful knowlwedge about how to access payload
// in current version of sdk...
// https://github.com/open-nfpsw/p4c_firewall/blob/master/dynamic_rule_fw/plugin.c

// ring head and tail on i25 or emem1
#define RL_RING_NUMBER 4
volatile __declspec(import, emem, addr40, aligned(512*sizeof(unsigned int))) uint32_t mem_workq[512];
volatile __declspec(import, emem, addr40) struct rl_pkt_store *rl_pkts;

// Seems to be some  magic naming here: this connects to the p4 action "filter_func"
int pif_plugin_filter_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data) {
	__mem40 uint8_t *payload;
	uint32_t mu_len, ctm_len;
	int count, to_read;
	SIGNAL sig;

	struct rl_packet my_pkt;

	// packet my be split between "cluster target memory"
	// and "MU"
	// (memory unit, includes imem, emem, ctm)
	// parsed headers are always in CTM, apparently.
	if (pif_pkt_info_global.p_is_split) {
		mu_len = pif_pkt_info_global.p_len - ((256 << pif_pkt_info_global.p_ctm_size) - pkt.p_offset);
	} else {
		mu_len = 0;
	}

	count = pif_pkt_info_global.p_len - pif_pkt_info_spec.pkt_pl_off - mu_len;
	payload = pkt_ctm_ptr40(pkt.p_isl, pkt.p_pnum, pkt.p_offset);
	payload += pif_pkt_info_spec.pkt_pl_off;

	// Do stuff with payload (CTM chunk)
	// TODO
	while (count) {
		to_read = 0; // max of chunk size and count

		// STUDY lines 66--92 of that file.
		// seem to nead to use "mem_read8" to pull data into
		// tx registers, then pull out if I want to iterate over them...
		break;
	}

	// Do stuff with remainder of payload.
	if (mu_len) {
		// Point to the portion of payload data in MU.
		// and then index in by the amount of data which was kept locally in ctm,
		payload = (__mem40 void *)((uint64_t)pif_pkt_info_global.p_muptr << 11);
		payload += 256 << pif_pkt_info_global.p_ctm_size;

		count = mu_len;
		while (count) {
			break;
		}
	}

	if (pif_plugin_hdr_rt_present(headers)) {
		__declspec(write_reg) uint32_t workq_write_register;
		unsigned int mu_island = 1;
		unsigned int ring_number = (mu_island << 10) | RL_RING_NUMBER;
		__declspec(emem) __addr40 uint8_t *slot;

		// See "out/callbackapi/pif_plugin_rt.h"
		// Only these should come up...

		// SEE pif_parrep.c !!!
		// I believe these measures are in logn words, and SEVERAL MIGHT OVERLAP!
		// Identify first/last bytes we need in all cases...
		// These seem to be all laid out as one big block; just need to find the offsets.

		// RL cfg, if it exists.
		__lmem struct pif_header_rct *rct = (__lmem struct pif_header_rct *) (headers + PIF_PARREP_rct_OFF_LW);

		// RL Type, if it exists (it should).
		__lmem uint32_t *lm32 = headers + PIF_PARREP_rt_OFF_LW;

		// RL insert command.
		__lmem uint32_t *lm32_2 = headers + PIF_PARREP_ins_OFF_LW;

		// Upper bound is at... headers + PIF_PARREP_standard_metadata_OFF_LW
		// assuming nothing changes in the P4 SDK impl (it won't).

		// Okay, now copy this crap into mem,
		// TODO

		// Allocate the space here somehoe...
		// Using RL-island's yonder free-list
		// TODO

		slot = rl_pkt_get_slot(rl_pkts);

		// Now copy the packet body into the acquired freelist entry.
		// TODO

		// Now send the packet pointer and RL header over to the correct island.
		// TODO
		// for now, just send anything over to the RL ME.
		workq_write_register = __ctx();

		cmd_mem_workq_add_work(ring_number, &workq_write_register, 1, sig_done, &sig);

		return PIF_PLUGIN_RETURN_DROP;
	} else {
		// p4 cfg should make sure that only RL packets get stuck here.
		// This should just pass over any packets which aren't RL.
		return PIF_PLUGIN_RETURN_FORWARD;
	}
}
