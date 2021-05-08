#ifndef main_h
#define main_h

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

typedef struct tilemap_t
{
	uint32_t *tiles;
} tilemap_t;

typedef struct test_t
{
	uint32_t val;
} test_t;

typedef struct player_pos_t
{
	uint32_t x;
	uint32_t y;
} player_pos_t;

typedef struct game_state_t
{
	memory_arena_t memory_arena;

	//test_t *test;
	//player_pos_t *player_pos;
	//tilemap_t *tilemap1;
	uint32_t *tiles;
} game_state_t;

#endif
