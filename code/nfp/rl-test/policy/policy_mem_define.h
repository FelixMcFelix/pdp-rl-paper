#ifndef _P_MEM_DEF_H
#define _P_MEM_DEF_H

#include "../rl.h"

__declspec(cls, export, aligned(8))tile_t t1_tiles[MAX_CLS_TILES] = {0};
__declspec(ctm, export, aligned(8))tile_t t2_tiles[MAX_CTM_TILES] = {0};
__declspec(export, imem, aligned(8))tile_t t3_tiles[MAX_IMEM_TILES] = {0};

// Maybe keep one of these locally, too?
// probably should do this holy shit.
__declspec(export, cls) struct rl_config cfg = {0};

#ifdef VERIFY_OUTPUTS
__declspec(export, emem, aligned(8))tile_t acvals[MAX_ACTIONS] = {0};
__declspec(export, emem, aligned(8))tile_t chosenac = 0;
#endif /* VERIFY_OUTPUTS */

#endif /* !_P_MEM_DEF_H_ */