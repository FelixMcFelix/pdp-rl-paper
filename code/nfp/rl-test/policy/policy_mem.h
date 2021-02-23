#ifndef _P_MEM_H
#define _P_MEM_H

#include "../rl.h"

__declspec(i5.cls, import)tile_t t1_tiles[MAX_CLS_TILES] = {0};
__declspec(i5.ctm, import)tile_t t2_tiles[MAX_CTM_TILES] = {0};
__declspec(import imem)tile_t t3_tiles[MAX_IMEM_TILES] = {0};

__declspec(import, emem) struct rl_config cfg = {0};

#endif /* !_P_MEM_H_ */