#ifndef _TILE_H
#define _TILE_H

#include <stdint.h>
#include "tile_marker.h"

#ifdef _8_BIT_TILES
typedef int8_t tile_t;
typedef uint8_t utile_t;
typedef int16_t double_tile_t;
#endif /* _8_BIT_TILES */

#ifdef _16_BIT_TILES
typedef int16_t tile_t;
typedef uint16_t utile_t;
typedef int32_t double_tile_t;
#endif /* _16_BIT_TILES */

#ifdef _32_BIT_TILES
typedef int32_t tile_t;
typedef uint32_t utile_t;
typedef int64_t double_tile_t;
#endif /* _32_BIT_TILES */

union pad_tile {
	tile_t data;
	uint32_t _pad;
};

#endif /* !_TILE_H_ */
