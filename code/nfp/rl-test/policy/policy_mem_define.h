#ifndef _P_MEM_DEF_H
#define _P_MEM_DEF_H

#include "../rl.h"

__declspec(i5.cls, export)tile_t t1_tiles[MAX_CLS_TILES] = {0};
__declspec(i5.ctm, export)tile_t t2_tiles[MAX_CTM_TILES] = {0};
__declspec(export imem)tile_t t3_tiles[MAX_IMEM_TILES] = {0};

// Maybe keep one of these locally, too?
// probably should do this holy shit.
__declspec(export, emem) struct rl_config cfg = {0};

#endif /* !_P_MEM_DEF_H_ */