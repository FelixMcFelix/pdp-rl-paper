#ifndef _P_MEM_H
#define _P_MEM_H

#include "../rl.h"

__declspec(cls, import, aligned(8))tile_t t1_tiles[MAX_CLS_TILES];
__declspec(ctm, import, aligned(8))tile_t t2_tiles[MAX_CTM_TILES];
__declspec(import, imem, aligned(8))tile_t t3_tiles[MAX_IMEM_TILES];

__declspec(import, cls) struct rl_config cfg = {0};

#ifdef _VERIFY_OUTPUTS
__declspec(import, emem, aligned(8))tile_t acvals[MAX_ACTIONS];
__declspec(import, emem, aligned(8))tile_t chosenac;
#endif /* _VERIFY_OUTPUTS*/

#endif /* !_P_MEM_H_ */