#include "quant_math.h"

tile_t quant_mul(tile_t lhs, tile_t rhs, utile_t base) {
	double_tile_t intermediate = (double_tile_t)lhs;
	intermediate *= (double_tile_t)rhs;
	// rounding step.
	intermediate += (1 << (base - 1));

	return (tile_t)(intermediate >> base);
}

tile_t quant_div(tile_t lhs, tile_t rhs, utile_t base) {
	double_tile_t intermediate = ((double_tile_t)lhs) << base;
	// ignore rounding behaviour for now.
	return (tile_t)(intermediate / base);	
}