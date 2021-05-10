#ifndef main_h
#define main_h

#include "util.h"

typedef struct memory_t
{
	uint64_t permanent_storage_size;
	void *permanent_storage;
} memory_t;

typedef struct memory_arena_t
{
	uint32_t *base;
	uint32_t size;
	uint32_t used;
} memory_arena_t;

typedef struct world_t
{
	float tile_size;
	float scale;
	vec2f_t offset;
} world_t;

typedef struct tile_t
{
	int id;
	vec2i_t pos;
	int visited;
	int obstacle;
	int reachable;
	int truly_reachable;
	int global;
	int local;
	int neighbors_num;
	struct tile_t *neighbors[4];
	struct tile_t *parent;
} tile_t;

typedef struct game_state_t
{
	memory_arena_t memory_arena;

	uint32_t *tiles;
} game_state_t;

#endif
