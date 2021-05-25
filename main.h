#ifndef main_h
#define main_h

#include "util.h"

typedef struct vec2i_t
{
	int32_t x;
	int32_t y;
} vec2i_t;

typedef struct vec2f_t
{
	float x;
	float y;
} vec2f_t;

char* _itoa(int32_t val)
{
	static char buff[8];
	sprintf(buff, "%d", val);
	return buff;
}

int32_t _ftoi(float num)
{
	return (int32_t)floorf(num);
}

// TODO(martin): this function is for debug purposes only!!
char* concat(char *src_1, char *src_2)
{
	static char dst[80];

	uint32_t i_dst = 0;
	uint32_t i_src = 0;
	while(src_1[i_src] != '\0')
	{
		dst[i_dst] = src_1[i_src];
		i_dst++;
		i_src++;
	}
	i_src = 0;
	while(src_2[i_src] != '\0')
	{
		dst[i_dst] = src_2[i_src];
		i_dst++;
		i_src++;
	}
	dst[i_dst] = '\0';
	
	return dst;
}

float lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

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
	uint32_t id;
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

typedef struct tilemap_t
{
	char *name;
	uint32_t tiles_count_x;
	uint32_t tiles_count_y;
	vec2i_t starting_pos[8];

	tile_t *tiles;
} tilemap_t;


typedef struct camera_t
{
	int32_t x;
	int32_t y;
} camera_t;

typedef struct cursor_t
{
	SDL_Texture *texture;
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
} world_t;

#define PLAYERS_NUM 3
typedef struct entity_t
{
	uint32_t id;
	SDL_Texture *texture;
	vec2i_t tile_pos;
	vec2f_t pos;

	float move_speed;
	float move_lerp_dt;
	vec2f_t move_lerp_start;
	vec2f_t move_lerp_end;

	int32_t dir;
	uint32_t animation_frame;
	uint32_t animation_counter;

	char *name;
	int32_t hp;
	int32_t att;
	int32_t speed;
	int32_t speed_accumulator;
	int32_t stats_move;

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

enum animation_dir_e
{
	animation_dir_left,
	animation_dir_right,
	animation_dir_up,
	animation_dir_down,
} animation_dir_e;

#endif
