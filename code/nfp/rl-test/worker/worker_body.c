#include <stdint.h>
#include "../worker_config.h"
#include "../subtask/work.h"
#include "../subtask/ack.h"
#include "../subtask/external_writeback.h"

#ifdef IN_PORT
#ifdef OUT_PORT

// enum work_type {
// 	WORK_REQUEST_WORKER_COUNT,
// 	WORK_SET_WORKER_COUNT,
// 	WORK_NEW_CONFIG,
// 	WORK_STATE_VECTOR
// };

volatile __declspec(shared) struct work local_ctx_work = {0};

void work(uint8_t is_master) {
	uint32_t worker_ct = 0;
	uint32_t base_worker_ct = 0;

	uint32_t my_id = 0;
	uint8_t my_slot = 0;

	uint8_t iter = 0;
	struct worker_ack ack = {0};
	enum writeback_result wb = WB_LOCKED;

	__assign_relative_register(&worker_in_sig, WORKER_SIGNUM);

	while (1) {
		wait_for_all(&worker_in_sig);

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

					while (wb != WB_SUCCESS) {
						bodied = 1;
						wb = external_writeback_ack(
							0,
							ack
						);
						cap_work.type = wb;
					}
					bodied = 2;

					#endif /* !CAP */

					break;
				default:
					#ifndef CAP
					OUT_PORT.body = IN_PORT.body;
					#endif /* !CAP */
					break;		
			}

			#ifndef CAP
			local_csr_write(local_csr_next_neighbor_signal, (1 << 7) | (WORKER_SIGNUM << 3));
			#endif /* !CAP */

			// awaken children.
			local_ctx_work = IN_PORT;
			for (i=1; i<8; i++) {
				signal_ctx(i, WORKER_SIGNUM);
			}
		}

		// Standard processing.
		switch (local_ctx_work.type) {
			case WORK_SET_WORKER_COUNT:
				worker_ct = local_ctx_work.body.worker_count;
				my_id = base_worker_ct + __ctx();
				my_slot = my_id % RL_WORKER_WRITEBACK_SLOTS;

				ack.type = ACK_NO_BODY;

				while (wb != WB_SUCCESS) {
					wb = external_writeback_ack(my_slot, ack);

					if (wb != WB_SUCCESS) {
						sleep(1000);
					}
				}

				break;
			default:
				break;
		}
	}
}

#else

#error "OUT_PORT undefined!"

#endif /* OUT_PORT */

#else

#error "IN_PORT undefined!"

#endif /* IN_PORT */
