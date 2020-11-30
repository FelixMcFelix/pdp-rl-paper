#include "quant_math.h"

int32_t quant_mul(int32_t lhs, int32_t rhs, uint32_t base) {
	int64_t intermediate = (int64_t)lhs;
	intermediate *= (int64_t)rhs;
	// rounding step.
	intermediate += (1 << (base - 1));

	return (int32_t)(intermediate >> base);
}

int32_t quant_div(int32_t lhs, int32_t rhs, uint32_t base) {
	int64_t intermediate = ((int64_t)lhs) << base;
	// ignore rounding behaviour for now.
	return (int32_t)(intermediate / base);	
}