#include <stdint.h>
#include "../worker_config.h"
#include "../subtask/work.h"
#include "../subtask/ack.h"
#include "../rl.h"

#ifndef NO_FORWARD
#include "../subtask/external_writeback.h"
#endif

#if defined(NO_FORWARD) || (defined(IN_PORT) && defined(OUT_PORT))

volatile __declspec(shared) struct work local_ctx_work = {0};

void work(uint8_t is_master, unsigned int parent_sig) {
	__addr40 _declspec(emem) struct rl_config *cfg;

	uint32_t worker_ct = 0;
	uint32_t base_worker_ct = 0;

	uint32_t my_id = 0;
	uint8_t my_slot = 0;

	uint32_t scratch = 0;
	struct worker_ack ack = {0};
	enum writeback_result wb = WB_LOCKED;

	// Must be at least 2 workers for this to be not-dumb.
	uint8_t my_work_alloc_size = 0;
	uint8_t allocs_with_spill = 0;
	uint8_t iter = 0;
	uint16_t work_idxes[RL_MAX_TILE_HITS/2] = {0};

	#ifndef NO_FORWARD
	__assign_relative_register(&worker_in_sig, WORKER_SIGNUM);
	#endif

	while (1) {
		int should_writeback = 0;

		#ifdef NO_FORWARD
		wait_for_all(&internal_handout_sig);
		#else
		wait_for_all(&worker_in_sig);
		#endif

		#ifndef NO_FORWARD
		wb = WB_LOCKED;

		// pass message along the chain if we're a master ctx.
		if (is_master) {
			int i = 1;

			#ifndef CAP
			OUT_PORT.type = IN_PORT.type;
			#endif /* !CAP */
			
			switch (IN_PORT.type) {
				case WORK_REQUEST_WORKER_COUNT:
					base_worker_ct = IN_PORT.body.worker_count;

					#ifndef CAP

					OUT_PORT.body.worker_count = base_worker_ct + __n_ctx();

					#else

					cap_work.type = IN_PORT.type;
					cap_work.body.worker_count = base_worker_ct + __n_ctx();

					ack.type = ACK_WORKER_COUNT;
					ack.body.worker_count = base_worker_ct + __n_ctx();

					should_writeback = 1;

					#endif /* !CAP */

					local_ctx_work.type = WORK_SET_BASE_WORKER_COUNT;
					local_ctx_work.body.worker_count = base_worker_ct;

					break;
				default:
					#ifndef CAP
					OUT_PORT.body = IN_PORT.body;
					#endif /* !CAP */
					local_ctx_work = IN_PORT;
					break;		
			}

			#ifndef CAP
			local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (WORKER_SIGNUM << 3));
			#endif /* !CAP */

			// awaken children.
			for (i=1; i<8; i++) {
				signal_ctx(i, WORKER_SIGNUM);
			}
		}
		#endif /* !NO_FORWARD */

		wb = WB_LOCKED;

		// Standard processing.
		switch (local_ctx_work.type) {
			case WORK_SET_BASE_WORKER_COUNT:
				base_worker_ct = local_ctx_work.body.worker_count;
				break;
			case WORK_SET_WORKER_COUNT:
				worker_ct = local_ctx_work.body.worker_count;

				#ifndef NO_FORWARD
				my_id = base_worker_ct + __ctx();

				#else
				my_id = __ctx() - 1;

				#endif /* !NO_FORWARD */

				my_slot = my_id % RL_WORKER_WRITEBACK_SLOTS;

				ack.type = ACK_NO_BODY;
				should_writeback = 1;

				break;
			case WORK_NEW_CONFIG:
				cfg = local_ctx_work.body.cfg;

				ack.type = ACK_NO_BODY;
				should_writeback = 1;

				// spill.
				scratch = cfg->num_work_items % worker_ct;

				my_work_alloc_size = (cfg->num_work_items / worker_ct);
				allocs_with_spill = scratch;
				if (my_id < scratch) {
					my_work_alloc_size += 1;
				}

				break;
			case WORK_ALLOCATE:
				scratch = 0;

				switch (local_ctx_work.body.alloc.strat) {
					case ALLOC_CHUNK:
						// not too expensive to be dumb here.
						// i.e., not in a hot compute loop

						if (my_id >= allocs_with_spill) {
							scratch = allocs_with_spill * ((cfg->num_work_items / worker_ct) + 1);
							scratch += (my_id - allocs_with_spill) * (cfg->num_work_items / worker_ct);
						} else {
							scratch = my_id * ((cfg->num_work_items / worker_ct) + 1);
						}

						// scratch is the base of our block.
						for (iter = 0; iter < my_work_alloc_size; iter++) {
							work_idxes[iter] = local_ctx_work.body.alloc.work_indices[scratch + iter];
						}
						break;
					case ALLOC_STRIDE_OFFSET:
						// might have *some* saturation...
						// think about revisiting me, or finding a proof/counterexample.
						scratch = my_id % (cfg->num_work_items / worker_ct);
					case ALLOC_STRIDE:
						// scratch is the number to add to alloc.
						iter = 0;
						scratch *= worker_ct;
						scratch += my_id;

						// equal to (id + i * worker_cnt), using own id as an offset into this sequence.
						while (iter < my_work_alloc_size) {
							if (scratch > cfg->num_work_items) {
								scratch = my_id;
							}

							work_idxes[iter] = local_ctx_work.body.alloc.work_indices[scratch];

							scratch += worker_ct;

							iter++;
						}

						break;
				}

				ack.type = ACK_NO_BODY;
				should_writeback = 1;
				break;
			default:
				__implicit_read(&cfg);
				break;
		}

		if (should_writeback) {
			while (1) {
				#ifndef NO_FORWARD
				wb = external_writeback_ack(my_slot, ack);
				#else
				wb = internal_writeback_ack(my_slot, ack, parent_sig);
				#endif /* !NO_FORWARD */

				if (wb == WB_SUCCESS) {
					break;
				}

				sleep(100);
			}
		}
	}
}

#else

#error "IN_PORT or OUT_PORT undefined!"

#endif /* IN_PORT */
