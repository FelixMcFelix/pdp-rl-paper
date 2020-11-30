#ifndef _RL_H
#define _RL_H

#include <stdint.h>
#include "pif_parrep.h"
#include "callbackapi/pif_plugin.h"
#include "callbackapi/pif_plugin_ins.h"
#include "callbackapi/pif_plugin_rct.h"
#include "callbackapi/pif_plugin_in_state.h"

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

#define RL_MAX_SETS (T1_MAX_SETS+T2_MAX_SETS+T3_MAX_SETS)

enum tile_location {
	TILE_LOCATION_T1,
	TILE_LOCATION_T2,
	TILE_LOCATION_T3,
};

// maximum number of tiles that can be returned as active
// calls to `tile_code` must have at least this size.
#define RL_MAX_TILE_HITS MAX_TILINGS_PER_SET*RL_MAX_SETS

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

// 8 byte, 4-b abigned
struct tile_fraction {
	tile_t numerator;
	tile_t divisor;
};

// Okay time for control block info.
// u32 -> 4B align
// 24B size
struct tiling_options {
	uint16_t num_dims; // 0
	uint16_t dims[T3_MAX_DIMS]; // 2

	// enum tile_location
	uint8_t location; // 4
	uint32_t offset; // 8

	// Might need these cached to simplify tile counting.
	// These are computed once the policy structure is installed...
	uint32_t start_tile; //12
	uint32_t end_tile; //16
	uint32_t tiling_size; //20 -> 24
};

enum key_source_kind {
	KEY_SRC_SHARED,
	KEY_SRC_FIELD,
	KEY_SRC_VALUE
};

union key_source_body {
	uint16_t field_id;
	tile_t value;
};

struct key_source {
	enum key_source_kind kind;
	union key_source_body body;
};

struct rl_config {
	//Need to store info for each active tiling.
	struct tiling_options tiling_sets[T1_MAX_SETS+T2_MAX_SETS+T3_MAX_SETS];
	uint16_t num_tilings; // (8 + 8 + 1) * 24 = 408

	uint16_t num_dims; // 410
	uint16_t tiles_per_dim; // 412
	uint16_t tilings_per_set; // 414
	uint16_t num_actions; // 416 -> 418

	uint32_t last_tier_tile[3]; // 420 -> 432

	// one max/min pair per dimension.
	// populated by config packets.
	tile_t maxes[RL_DIMENSION_MAX]; // 432 -> 512
	tile_t mins[RL_DIMENSION_MAX]; // + 20*4 =  512 -> 592

	// 1-D width of a tile in each dimension.
	// must be computed!
	// this is w' = (max - min) / (n_tiles_per_dim - 1)
	tile_t width[RL_DIMENSION_MAX]; // 592 -> 672
	// every tiling in the same set is then shifted by n * this.
	// This is w'/tilings_per_set
	tile_t shift_amt[RL_DIMENSION_MAX]; // 672 -> 752
	// The upper bound of the wide tiling. Precomputed. max + w'
	tile_t adjusted_maxes[RL_DIMENSION_MAX]; // 752 -> 832

	// exploration param.
	// the decay in the numerator, and how often to do so.
	struct tile_fraction epsilon; // 832
	struct tile_fraction alpha; // 840
	struct tile_fraction gamma; // 840
	tile_t epsilon_decay_amt; // 848
	uint32_t epsilon_decay_freq; // 852
	// 856

	// TODO: remove bases from alpha, gamma, and use this for policy.
	uint32_t quantiser_shift;

	struct key_source state_key;
	struct key_source reward_key;

	uint8_t do_updates;
};

// FIXME: absolute guess, need to import the right headers to compute this on both app islands...
#define RL_HEADER_PARREP_START (PIF_PARREP_T4_OFF_LW << 2)
// NOTE: need to manually update according to size.
#define RL_HEADER_LEN_LW (PIF_PARREP_ins_LEN_LW)
#define RL_HEADER_BYTES_EXACT (RL_HEADER_LEN_LW << 2)
#define RL_HEADER_BYTES (((RL_HEADER_BYTES_EXACT + 7) >> 3) << 3)

// want ctl data, tier4

union t4_data {
	uint32_t raw[1];
	struct pif_plugin_rct config;
	struct pif_plugin_ins insert;
	struct pif_plugin_in_state state;
};

struct rl_work_item {
	__declspec(emem) __addr40 uint8_t *packet_payload; // 8?
	struct pif_parrep_ctldata ctldata; // 32
	union t4_data parsed_fields;
	uint16_t packet_size;
};

// idea -> round up to next whole LW, i.e. 4-bytes.
#define RL_WORK_LWS ((sizeof(struct rl_work_item) + 3) >> 2)
#define RL_WORK_LEN_ALIGN (RL_WORK_LWS << 2)

struct policy_install_data {
	uint32_t tile;
	uint16_t count;
};

struct state_action_pair {
	uint16_t action;
	uint16_t len;
	tile_t val;
	tile_t tiles[RL_MAX_TILE_HITS];
};

#endif /* !_RL_H_ */
