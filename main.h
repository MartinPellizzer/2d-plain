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

typedef struct cursor_t
{
	vec2i_t tile_pos;
} cursor_t;

typedef struct game_state_t
{
	memory_arena_t memory_arena;

	uint32_t *tiles;
} game_state_t;

typedef struct world_t
{
	float tile_size;
	float scale;
	vec2f_t offset;
} world_t;

#define PLAYERS_NUM 3
typedef struct entity_t
{
	SDL_Texture *texture;
	vec2i_t tile_pos;
	vec2f_t pos;

	float move_speed;
	float move_lerp_dt;
	vec2f_t move_lerp_start;
	vec2f_t move_lerp_end;

	uint32_t animation_frame;
	uint32_t animation_counter;

	char *name;
	int32_t hp;
	int32_t att;
	int32_t speed;
	int32_t speed_accumulator;

	uint32_t team;
	uint32_t dead;

	uint32_t has_moved;
} entity_t;

typedef struct action_t
{
	uint32_t id;
	entity_t *entity;
	uint32_t initialized;
	uint32_t completed;
} action_t;

typedef struct battle_t
{
	uint32_t is_over;
} battle_t;

typedef struct turn_t
{
	uint32_t has_moved;
	uint32_t has_attacked;
} turn_t;

#endif
