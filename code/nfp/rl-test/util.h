#ifndef _UTIL_H
#define _UTIL_H

#include <lu/cam_hash.h>
#include <stdint.h>

union two_u16s {
	uint32_t raw;
	uint16_t ints[2];
	uint8_t bytes[4];
};

union four_u16s {
	uint64_t raw;
	uint32_t words[2];
	uint16_t shorts[4];
	uint8_t bytes[8];
};

#define CAMHT_LOOKUP_IDX_ADD(_name, _key, added)                            \
    camht_lookup_idx_add(CAMHT_HASH_TBL(_name), CAMHT_NB_ENTRIES(_name),    \
                     (void *)_key, sizeof(*_key), added)

#endif /* !_UTIL_H_ */