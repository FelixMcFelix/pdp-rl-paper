#include <pif_headers.h>
#include <pif_plugin.h>
#include <memory.h>
#include <nfp.h>
#include <nfp/me.h>
#include <nfp/mem_atomic.h>
#include <nfp/mem_ring.h>
#include <stdint.h>
#include "stress.h"

#include "rl-proto.h"
#include "rl.h"
#include "rl-pkt-store.h"
#include "rl_interface.h"

#include <nfp_mem_ring.h>

volatile __emem_n(0) __declspec(import, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t rl_mem_workq[512*1024] = {0};

__declspec(import, emem) struct rl_pkt_store rl_pkts;
volatile __declspec(import, emem, addr40, aligned(sizeof(unsigned int))) uint8_t inpkt_buffer[RL_PKT_MAX_SZ * RL_PKT_STORE_COUNT] = {0};

// ring head and tail on i25 or emem1
volatile __emem_n(0) __declspec(import, addr40, aligned(512*1024*sizeof(unsigned int))) uint32_t rl_out_workq[512*1024] = {0};

__declspec(import, emem) struct rl_pkt_store rl_actions;
volatile __declspec(import, emem, addr40, aligned(sizeof(unsigned int))) uint8_t rl_out_state_buffer[ALIGNED_OUT_STATE_SZ * RL_PKT_STORE_COUNT] = {0};

volatile __declspec(import, emem) uint32_t begin_stress;

main() {
	uint32_t t0, t1;
	__declspec(write_reg) struct rl_work_item workq_write_register = {0};
	__declspec(read_reg) struct rl_answer_item workq_dump;
	mem_ring_addr_t r_addr;
	mem_ring_addr_t r_out_addr;
	struct rl_work_item work_to_send;
	__xread uint32_t ready_to_blit;

	// TODO:
	// * Fill out req struct
	// * fill out reply struct

	// need to dummy this semi-reasonably...

	work_to_send.ctldata.t4_type = PIF_PARREP_TYPE_in_state;
	work_to_send.parsed_fields.state.dim_count = RL_DIMENSION_MAX;

	// wait for valid config
	while (1) {
 		mem_read_atomic((void*)&ready_to_blit, (__addr40 void*)&begin_stress, sizeof(uint32_t));

		if (ready_to_blit == 1) {
			break;
		}

		sleep(10000);
	}

	r_addr = mem_ring_get_addr(rl_mem_workq);
	r_out_addr = mem_ring_get_addr(rl_out_workq);

	while (1) {
		__declspec(emem) __addr40 uint8_t *payload_dest = 0;

		// record t0
		t0 = local_csr_read(local_csr_timestamp_low);

		// need to get packet slot!
		while (1) {
			payload_dest = rl_pkt_get_slot(&rl_pkts);
			if (payload_dest != 0) {
				break;
			}

			// full disclosure; it could be a *long* time before a packet is freed.
			sleep(10000);
		}
		work_to_send.packet_payload = payload_dest;
		workq_write_register = work_to_send;

		// send req
		mem_workq_add_work(
			RL_RING_NUMBER,
			r_addr,
			&workq_write_register,
			RL_WORK_LEN_ALIGN
		);

		// await response
		mem_workq_add_thread(
			RL_OUT_RING_NUMBER,
			r_out_addr,
			&workq_dump,
			RL_ANSWER_LEN_ALIGN
		);
		// sleep(1000);
		rl_pkt_return_slot(&rl_actions, workq_dump.state);

		// record t1
		t1 = local_csr_read(local_csr_timestamp_low);

		if (t1 - t0 < STRESS_TEST_INDIV_GAP_RCYCLES) {
			// Big note: this takes actual cycles, but the asm blocks take 16*cycles.
			sleep((t1 - t0) << 4);
		}
	}
}