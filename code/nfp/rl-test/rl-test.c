#include <pif_headers.h>
#include <pif_plugin.h>
#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_bulk.h>
#include <nfp/mem_ring.h> // should add the flowenv workq functions...
#include <rtl.h>
//#include <nfp_mem_workq.h>
#include <nfp/mem_atomic.h>
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
#define RL_RING_NUMBER 129
//_NFP_CHIPRES_ASM(.alloc_resource rl_mem_workq_rnum emem0_queues+RL_RING_NUMBER global 1)

volatile __emem_n(0) __declspec(import, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t rl_mem_workq[512*1024];

__declspec(import, emem) struct rl_pkt_store rl_pkts;

// Seems to be some  magic naming here: this connects to the p4 action "filter_func"
int pif_plugin_filter_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data) {
	__mem40 uint8_t *payload;
	uint32_t mu_len, ctm_len;
	int count, to_read;
	SIGNAL sig;

	if (pif_plugin_hdr_rt_present(headers)) {
		__declspec(write_reg) struct rl_work_item workq_write_register;
		__declspec(write_reg) uint32_t test_write_register;
		unsigned int mu_island = 1;
		unsigned int ring_number = (mu_island << 10) | RL_RING_NUMBER;
		struct rl_work_item work_to_send;

		work_to_send.ctldata = *(__lmem struct pif_parrep_ctldata *)headers;
		work_to_send.parsed_fields.raw[0] = headers[PIF_PARREP_T4_OFF_LW];

		// Allocate the space here somehow...
		// Using RL-island's yonder free-list
		// FIXME: handle case where packet space cannot be acquired.
		work_to_send.packet_payload = rl_pkt_get_slot(&rl_pkts);

		test_write_register = sizeof(__declspec(emem) __addr40 uint8_t *);
	
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
		
		// Seems to work!
		ua_memcpy_mem40_mem40(
			work_to_send.packet_payload, 0,
			payload, 0,
			count
		);

		// From Emem to packet space...
		payload = (__mem40 void *)((uint64_t)pif_pkt_info_global.p_muptr << 11);
		payload += 256 << pif_pkt_info_global.p_ctm_size;

		// unknown if works: never had a big enough pkt.
		ua_memcpy_mem40_mem40(
			(work_to_send.packet_payload + count), 0,
			payload, 0,
			mu_len
		);

		// Now send the packet pointer and RL header over to the correct island.
		work_to_send.packet_size = (uint16_t)(count + mu_len);
		workq_write_register = work_to_send;

		// I believe the doc-compliant version of these might be located in <nfp/mem_ring.h> (flowenv/lib/...)
		//cmd_mem_workq_add_work(ring_number, &workq_write_register, RL_WORK_LWS, sig_done, &sig);
		/**
 		* Put entries onto a work queue.
 		* @param rnum          Work queue "ring" number
 		* @param raddr         Address bits for the queue engine island
 		* @param data          Input data
 		* @param size          Size of input
 		*
 		* Note that no work queue overflow checking is performed.
 		*/
		mem_workq_add_work(
			RL_RING_NUMBER,
			mem_ring_get_addr(rl_mem_workq),
			&workq_write_register,
			RL_WORK_LEN_ALIGN
		);

		__implicit_read(&payload);
		__implicit_read(&workq_write_register);
		__implicit_read(&count);

		return PIF_PLUGIN_RETURN_DROP;
	} else {
		// p4 cfg should make sure that only RL packets get stuck here.
		// This should just pass over any packets which aren't RL.
		return PIF_PLUGIN_RETURN_FORWARD;
	}
}
