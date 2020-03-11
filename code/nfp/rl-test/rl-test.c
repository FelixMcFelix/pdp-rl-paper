#include <pif_headers.h>
#include <pif_plugin.h>
#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_ring.h> // should add the flowenv workq functions...
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
volatile __declspec(import, emem, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t mem_workq[512*1024];
volatile __declspec(import, emem, addr40) struct rl_pkt_store *rl_pkts;

// Seems to be some  magic naming here: this connects to the p4 action "filter_func"
int pif_plugin_filter_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data) {
	__mem40 uint8_t *payload;
	uint32_t mu_len, ctm_len;
	int count, to_read;
	SIGNAL sig;

	if (pif_plugin_hdr_rt_present(headers)) {
		__declspec(write_reg) struct rl_work_item workq_write_register;
		unsigned int mu_island = 1;
		unsigned int ring_number = (mu_island << 10) | RL_RING_NUMBER;
		struct rl_work_item work_to_send;
		// __declspec(emem) __addr40 uint8_t *slot;

		// See "out/callbackapi/pif_plugin_rt.h"
		// Only these should come up...

		// SEE pif_parrep.c !!!
		// I believe these measures are in logn words, and SEVERAL MIGHT OVERLAP!
		// Identify first/last bytes we need in all cases...
		// These seem to be all laid out as one big block; just need to find the offsets.

		// RL Type, if it exists (it should).
		//__lmem uint32_t *lb = headers + PIF_PARREP_rt_OFF_LW;
		// RL cfg, if it exists.
		//__lmem struct pif_header_rct *rct = (__lmem struct pif_header_rct *) (headers + PIF_PARREP_rct_OFF_LW);
		// RL insert command.
		//__lmem uint32_t *lm32_2 = headers + PIF_PARREP_ins_OFF_LW;

		// Upper bound is at... headers + PIF_PARREP_standard_metadata_OFF_LW
		// assuming nothing changes in the P4 SDK impl (it won't).
		// Okay, now copy this crap into mem,
		memcpy_lmem_lmem(
			(void*)work_to_send.rl_header,
			(void*)(headers + PIF_PARREP_rt_OFF_LW),
			(PIF_PARREP_standard_metadata_OFF_LW - PIF_PARREP_rt_OFF_LW) * sizeof(uint32_t)
		);

		// Allocate the space here somehow...
		// Using RL-island's yonder free-list
		// FIXME: handle case where packet space cannot be acquired.
		work_to_send.packet_payload = rl_pkt_get_slot(rl_pkts);

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

		// Now copy the packet body into the acquired freelist entry.
		// From CTM to packet space...
		// dest, src, count.
		memcpy_mem_mem(
			(void*)work_to_send.packet_payload,
			payload,
			count,
		);

		// From Emem to packet space...
		payload = (__mem40 void *)((uint64_t)pif_pkt_info_global.p_muptr << 11);
		payload += 256 << pif_pkt_info_global.p_ctm_size;

		memcpy_mem_mem(
			(void*)(work_to_send.packet_payload + count),
			payload,
			mu_len,
		);

		// Now send the packet pointer and RL header over to the correct island.
		workq_write_register = work_to_send;

		// FIXME: change this (and the receive end) to make sure that the right
		// workq size is used (need to round up to next amount of uint_32 words).

		// I believe the doc-compliant version of these might be located in <nfp/mem_ring.h> (flowenv/lib/...)
		cmd_mem_workq_add_work(ring_number, &workq_write_register, RL_WORK_LWS, sig_done, &sig);

		return PIF_PLUGIN_RETURN_DROP;
	} else {
		// p4 cfg should make sure that only RL packets get stuck here.
		// This should just pass over any packets which aren't RL.
		return PIF_PLUGIN_RETURN_FORWARD;
	}
}
