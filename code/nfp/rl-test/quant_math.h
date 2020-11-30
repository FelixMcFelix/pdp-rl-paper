#ifndef __QUANT_MATH_H__
#define __QUANT_MATH_H__

#include <stdint.h>

int32_t quant_mul(int32_t lhs, int32_t rhs, uint32_t base);
int32_t quant_div(int32_t lhs, int32_t rhs, uint32_t base);

#endif /* __QUANT_MATH_H__ */