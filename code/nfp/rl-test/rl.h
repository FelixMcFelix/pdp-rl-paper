#ifndef _RL_H
#define _RL_H

#include <stdint.h>

typedef int32_t tile_t;

#define MAX_ACTIONS 10
#define TILE_SIZE sizeof(tile_t)
#define MAX_TILINGS_PER_SET 8
#define MAX_TILES_PER_DIM 6

#define ACTION_SET_SIZE MAX_ACTIONS*TILE_SIZE
#define TILE_SET_CONST_PART MAX_TILINGS_PER_SET*MAX_ACTIONS // needs mult by tile count.

#define T1_MAX_DIMS 1
#define T2_MAX_DIMS 2
#define T3_MAX_DIMS 4
#define RL_DIMENSION_MAX 20

#define T1_MAX_SETS 8
#define T2_MAX_SETS 8
#define T3_MAX_SETS 1

enum tile_location {
	TILE_LOCATION_T1,
	TILE_LOCATION_T2,
	TILE_LOCATION_T3,
};

// I don't think there's a conveneient way to macro this
#define T1_MAX_TILES_PER_TILING MAX_TILES_PER_DIM
#define T2_MAX_TILES_PER_TILING MAX_TILES_PER_DIM*MAX_TILES_PER_DIM
#define T3_MAX_TILES_PER_TILING MAX_TILES_PER_DIM*MAX_TILES_PER_DIM*MAX_TILES_PER_DIM*MAX_TILES_PER_DIM

#define MAX_CLS_TILES T1_MAX_SETS*T1_MAX_TILES_PER_TILING*TILE_SET_CONST_PART
#define MAX_CTM_TILES T2_MAX_SETS*T2_MAX_TILES_PER_TILING*TILE_SET_CONST_PART
#define MAX_IMEM_TILES T3_MAX_SETS*T3_MAX_TILES_PER_TILING*TILE_SET_CONST_PART

#define MAX_CLS_SIZE MAX_CLS_TILES*ACTION_SET_SIZE
#define MAX_CTM_SIZE MAX_CTM_TILES*ACTION_SET_SIZE
#define MAX_IMEM_SIZE MAX_IMEM_TILES*ACTION_SET_SIZE

// FIXME: reorder these get decent field alignment & small size.

struct tile_fraction {
	tile_t numerator;
	tile_t divisor;
};

// Okay time for control block info.
struct tiling_options {
	uint16_t num_dims;
	uint16_t dims[T3_MAX_DIMS];

	// enum tile_location
	uint8_t location;
	uint32_t offset;

	// Might need these cached to simplify tile counting.
	// These are computed once the policy structure is installed...
	uint32_t start_tile;
	uint32_t end_tile;
	uint32_t tiling_size;
};

struct rl_config {
	//Need to store info for each active tiling.
	struct tiling_options tiling_sets[T1_MAX_SETS+T2_MAX_SETS+T3_MAX_SETS];
	uint16_t num_tilings;

	uint16_t num_dims;
	uint16_t tiles_per_dim;
	uint16_t tilings_per_set;
	uint16_t num_actions;

	// one max/min pair per dimension.
	// populated by config packets.
	tile_t maxes[RL_DIMENSION_MAX];
	tile_t mins[RL_DIMENSION_MAX];

	// 1-D width of a tile in each dimension.
	// must be computed!
	// this is w' = (max - min) / (n_tiles_per_dim + 1)
	tile_t width[RL_DIMENSION_MAX];
	// every tiling in the same set is then shifted by n * this.
	// This is w'/tilings_per_set
	tile_t shift_amt[RL_DIMENSION_MAX];
	// The upper bound of the wide tiling. Precomputed. max + w'
	tile_t adjusted_maxes[RL_DIMENSION_MAX];

	// exploration param.
	// the decay in the numerator, and how often to do so.
	struct tile_fraction epsilon;
	struct tile_fraction alpha;
	tile_t epsilon_decay_amt;
	uint32_t epsilon_decay_freq;
};

// FIXME: absolute guess, need to import the right headers to compute this on both app islands...
#define RL_HEADER_BYTES 16

struct rl_work_item {
	__declspec(emem) __addr40 uint8_t *packet_payload;
	uint16_t packet_size;
	uint8_t rl_header[RL_HEADER_BYTES];
};

// idea -> round up to next whole LW, i.e. 4-bytes.
#define RL_WORK_LWS ((sizeof(struct rl_work_item) + 3) >> 2)

#endif /* !_RL_H_ */
