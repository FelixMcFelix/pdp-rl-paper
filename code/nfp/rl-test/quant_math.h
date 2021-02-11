#ifndef __QUANT_MATH_H__
#define __QUANT_MATH_H__

#include "tile.h"

tile_t quant_mul(tile_t lhs, tile_t rhs, utile_t base);
tile_t quant_div(tile_t lhs, tile_t rhs, utile_t base);

#endif /* __QUANT_MATH_H__ */