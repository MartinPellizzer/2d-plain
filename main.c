#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <math.h>
#include <sys/mman.h>

#include "main.h"
#include "util.h"

#if 0
	#define SCREEN_WIDTH (1920)
	#define SCREEN_HEIGHT (1080)
#else
	#define SCREEN_WIDTH (1920 / 2)
	#define SCREEN_HEIGHT (1080 / 2)
#endif

#define TILE_MAP_COUNT_X 8 
#define TILE_MAP_COUNT_Y 8

#define move_buffer_num 16
int32_t move_buffer_index;
vec2f_t move_buffer[move_buffer_num];

#define STATS_MOVE 4

enum turn_state_e
{
	CUTSCENE,
	DIALOGUE,
	EXPLORE,
	MENU,
	MOVE,
	ATT
};

uint32_t turn_state;

uint32_t key_z_pressed;


static int32_t
clipmini(int32_t val, int32_t min)
{
	return (val > min) ? val : min;
}

static int32_t
clipmaxi(int32_t val, int32_t max)
{
	return (val < max) ? val : max;
}

#if 0
	static int32_t
	clipmaxf(float val, float max)
	{
		return (val < max) ? val : max;
	}
#endif

// -------------------------------------------------------------------------
// TODO(martin): postprocess -----------------------------------------------
// -------------------------------------------------------------------------
typedef struct fade_t
{
	uint32_t initialized;
	uint32_t completed;
	int32_t val;
	int32_t speed;
} fade_t;
fade_t fade = {0, 10};

static uint32_t
postprocess_fadein(SDL_Renderer *renderer)
{
	if(!fade.initialized)
	{
		fade.initialized = 1;
		fade.completed = 0;

		fade.val = 255;
		fade.speed = 10;
	}
	
	fade.val -= fade.speed;
	fade.val = clipmini(fade.val, 0);

	if(fade.val <= 0) 
	{
		fade.initialized = 0;
		fade.completed = 1;
	}

	return fade.completed;
}

static uint32_t
postprocess_fadeout(SDL_Renderer *renderer)
{
	if(!fade.initialized)
	{
		fade.initialized = 1;
		fade.completed = 0;

		fade.val = 0;
		fade.speed = 10;
	}
	
	fade.val += fade.speed;
	fade.val = clipmaxi(fade.val, 255);

	if(fade.val >= 255) 
	{
		fade.initialized = 0;
		fade.completed = 1;
	}

	return fade.completed;
}

// -------------------------------------------------------------------------
// dialogue ----------------------------------------------------------------
// -------------------------------------------------------------------------
TTF_Font *font_dialogue;
uint32_t cursor_index;

static void
dialogue_render(SDL_Renderer *renderer, char *text, uint32_t text_length, uint32_t x)
{
	SDL_Color _color = {0, 0, 0, 255};

#define LINES_NUM 3
	char lines[LINES_NUM][32] = {0};
	uint32_t line_index = 0;
	uint32_t character_index = 0;

	for(int i = 0; i < text_length; i++) 
	{
		if(text[i] != '\n')
		{
			lines[line_index][character_index] = text[i];
			character_index++;
		}
		else
		{
			line_index++;
			character_index = 0;
		}
	}

	for(uint32_t i = 0; i < LINES_NUM; i++)
	{
		SDL_Surface *_surface = TTF_RenderText_Solid(font_dialogue, lines[i], _color);;
		SDL_Texture *_texture = SDL_CreateTextureFromSurface(renderer, _surface);;
		SDL_Rect _dst_rect;

		int32_t y = 350 + (i * 50);
		_dst_rect.x = x;
		_dst_rect.y = y;
		_dst_rect.w = 0;
		_dst_rect.h = 0;
		SDL_QueryTexture(_texture, NULL, NULL, &_dst_rect.w, &_dst_rect.h);

		SDL_RenderCopy(renderer, _texture, NULL, &_dst_rect);

		SDL_FreeSurface(_surface);
		SDL_DestroyTexture(_texture);
	}
}

static uint32_t
dialogue_play(SDL_Renderer *renderer, SDL_Texture *texture, char *text, uint32_t text_length, uint32_t alignment)
{
	cursor_index++;

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	SDL_Rect bg_dst_rect;
	bg_dst_rect.x = (alignment == 0) ? 0 : 800 - 700;
	bg_dst_rect.y = 350;
	bg_dst_rect.w = 700;
	bg_dst_rect.h = 200;
	SDL_RenderFillRect(renderer, &bg_dst_rect);

	SDL_Rect player_src_rect;
	player_src_rect.x = 0;
	player_src_rect.y = 0;
	player_src_rect.w = 8;
	player_src_rect.h = 8;

	SDL_Rect player_dst_rect; 
	player_dst_rect.x = (alignment == 0) ? 32 : 600;
	player_dst_rect.y = 350;
	player_dst_rect.w = 3 * 8 * 8;
	player_dst_rect.h = 3 * 8 * 8;

	SDL_RenderCopy(renderer, texture, &player_src_rect, &player_dst_rect);

	if(cursor_index > text_length) 
	{
		cursor_index = text_length;
		
		if(key_z_pressed)
		{
			cursor_index = 0;
			key_z_pressed = 0;
			return 1;
		}
	}
	else
	{
		if(key_z_pressed)
		{
			key_z_pressed = 0;
			cursor_index = text_length;
		}
	}

	if(alignment == 0)
		dialogue_render(renderer, text, cursor_index, 250);
	else
		dialogue_render(renderer, text, cursor_index, 150);
	
	return 0;
}

uint32_t tiles_distance(vec2i_t tile1_pos, vec2i_t tile2_pos)
{
	return (abs(tile1_pos.x - tile2_pos.x) + abs(tile1_pos.y - tile2_pos.y));
}

void ui_font_text_render(SDL_Renderer *renderer, TTF_Font *font, SDL_Color _color, char *text, int32_t x, int32_t y)
{
	SDL_Color color = _color;
	SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_Rect rect = {x, y, 0, 0};
	SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);

	SDL_RenderCopy(renderer, texture, NULL, &rect);

	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}

void memory_arena_init(memory_arena_t *arena, uint32_t size, uint32_t *base)
{
	arena->size = size;
	arena->base = base;
	arena->used = 0;
}

void* push_struct(memory_arena_t *arena, uint32_t size)
{
	void *res = arena->base + arena->used;
	arena->used += size;
	return res;
}

float distance(tile_t *a, tile_t *b)
{
	return sqrt((a->pos.x - b->pos.x) * (a->pos.x - b->pos.x) + (a->pos.y - b->pos.y) * (a->pos.y - b->pos.y));
}

tile_t* get_tile(tile_t tiles[], uint32_t x, uint32_t y)
{
	return &tiles[y * TILE_MAP_COUNT_X  + x];
}

// -------------------------------------------------------------------------
// TODO(martin): astar - sort nodes by globals -----------------------------
// -------------------------------------------------------------------------
static void
tiles_init(tile_t *tiles, uint32_t *tilemap)
{
	for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
	{
		for(int x = 0; x < TILE_MAP_COUNT_X; x++)
		{
			tile_t *tile = get_tile(tiles, x, y);

			tile->id = tilemap[y * TILE_MAP_COUNT_X + x]; 
			tile->obstacle = tilemap[y * TILE_MAP_COUNT_X + x];
			tile->pos.x = x;
			tile->pos.y = y;

			if(y > 0)
			{
				tile_t *tile_neighbor = get_tile(tiles, x + 0, y - 1);
				tile->neighbors[tile->neighbors_num] = tile_neighbor;  
				tile->neighbors_num += 1;
			}
			if(y < TILE_MAP_COUNT_Y - 1)
			{
				tile_t *tile_neighbor = get_tile(tiles, x + 0, y + 1);
				tile->neighbors[tile->neighbors_num] = tile_neighbor; 
				tile->neighbors_num += 1;
			}
			if(x > 0)
			{
				tile_t *tile_neighbor = get_tile(tiles, x - 1, y + 0);
				tile->neighbors[tile->neighbors_num] = tile_neighbor; 
				tile->neighbors_num += 1;
			}
			if(x < TILE_MAP_COUNT_X - 1)
			{
				tile_t *tile_neighbor = get_tile(tiles, x + 1, y + 0);
				tile->neighbors[tile->neighbors_num] = tile_neighbor; 
				tile->neighbors_num += 1;
			}
		}
	}
}

void solve_astar(tile_t *tiles, tile_t *tile_start, tile_t *tile_end)
{
	for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
	{
		for(int x = 0; x < TILE_MAP_COUNT_X; x++)
		{
			int i = y * TILE_MAP_COUNT_X + x;

			tiles[i].parent = 0;
			tiles[i].visited = 0;
			tiles[i].global = 9999;
			tiles[i].local = 9999;
		}
	}

	tile_t *tile_current = tile_start;
	tile_start->local = 0.0f;
	tile_start->global = distance(tile_start, tile_end);

	int index = -1;
#define BUFFER_SIZE 256
	tile_t *tiles_to_test[BUFFER_SIZE];

	tiles_to_test[++index] = tile_start;

	while(index >= 0/* && tile_current != tile_end*/)
	{
		
		if(tiles_to_test[index]->visited == 1)
			index--;
		
	///	if(index < 0)
	//		break;
		
		tile_current = tiles_to_test[index];
		tile_current->visited = 1;

		int i = 0;
		while(i < tile_current->neighbors_num)
		{
			tile_t *tile_neighbor = tile_current->neighbors[i];

			if(!tile_neighbor->visited && tile_neighbor->obstacle == 0 && tile_neighbor->reachable)
				tiles_to_test[++index] = tile_neighbor;
			
			float possibly_lower_goal = tile_current->local + distance(tile_current, tile_neighbor);

			if(possibly_lower_goal < tile_neighbor->local)
			{
				tile_neighbor->parent = tile_current;
				tile_neighbor->local = possibly_lower_goal;
				tile_neighbor->global = tile_neighbor->local + distance(tile_neighbor, tile_end);
			}

			i++;
		}
	}
}

uint32_t generate_moves_tmp(tile_t *tile_end)
{
	// get array of tiles position
	uint32_t res = 0;
	if(tile_end != 0)
	{
		tile_t *p = tile_end;
		while(p != 0)
		{
			res++;
			p = p->parent;
		}
	}
	return res;
}

void generate_moves(tile_t *tile_end)
{
	// get array of tiles position
	int parents_buffer_index = -1;
	vec2f_t parents_buffer[16];
	if(tile_end != 0)
	{
		tile_t *p = tile_end;
		while(p != 0)
		{
			vec2f_t pos = {p->pos.x, p->pos.y};
			parents_buffer[++parents_buffer_index] = pos;
			p = p->parent;
		}
	}

	// generate and add moves to move_buffer
	vec2f_t pos_prev = {parents_buffer[0].x, parents_buffer[0].y};
	for(int i = 0; i <= parents_buffer_index; i++)
	{
		vec2f_t pos_current = {parents_buffer[i].x, parents_buffer[i].y};
		vec2f_t move = {pos_prev.x - pos_current.x, pos_prev.y - pos_current.y};
		pos_prev = pos_current;
		
		if(move.x != 0 || move.y != 0)
			move_buffer[++move_buffer_index] = move;
	}
}

// ------------------------------------------------------------------------------------------
// MENU BATTLE ------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
enum menu_options_e
{
	menu_option_move,
	menu_option_att,
	menu_option_pass,
};

uint32_t menu_index = 0;
#define menu_options_count 3
uint32_t menu_options[menu_options_count] = {0};

void menu_battle_move_to_next_available_option()
{
	int32_t tmp_index = menu_index;
	tmp_index++;
	while(tmp_index < 3 && menu_options[tmp_index] == 1)
		tmp_index++;
	if(tmp_index < 3)
		menu_index = tmp_index;
}

void menu_battle_move_to_prev_available_option()
{
	int32_t tmp_index = menu_index;
	tmp_index--;
	while(tmp_index >= 0 && menu_options[tmp_index] == 1)
		tmp_index--;
	if(tmp_index >= 0)
		menu_index = tmp_index;
}

// -------------------------------------------------------------------------
// TODO(martin): player ----------------------------------------------------
// -------------------------------------------------------------------------
static void
player_init(entity_t *player, world_t *world, SDL_Texture *texture, uint32_t tile_pos_x, uint32_t tile_pos_y, 
	char *name, int32_t hp, int32_t att, int32_t speed, float move_speed, int32_t team)
{
	player->texture = texture;
	player->tile_pos.x = tile_pos_x;
	player->tile_pos.y = tile_pos_y;
	player->pos.x = player->tile_pos.x * world->tile_size * world->scale + world->offset.x;
	player->pos.y = player->tile_pos.y * world->tile_size * world->scale + world->offset.y;
	player->name = name;
	player->hp = hp;
	player->att = att;
	player->speed = speed;
	player->move_speed = move_speed;
	player->team = team;
}

void player_update_animation(entity_t *player)
{
	player->animation_counter++;
	if(player->animation_counter >= 10)
	{
		player->animation_counter = 0;
		player->animation_frame++;
		player->animation_frame = player->animation_frame % 2;
	}
}

entity_t* get_player_under_cursor(entity_t *players, cursor_t *cursor)
{
	for(int i = 0; i < PLAYERS_NUM; i++)
	{
		if(cursor->tile_pos.x == players[i].tile_pos.x && cursor->tile_pos.y == players[i].tile_pos.y)
			return &players[i];
	}
	
	return 0;
}

// ------------------------------------------------------------------------------------------
// cutscene ---------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
uint32_t current_cutscene_id;
uint32_t cutscene_index;

world_t world;

uint32_t delay_frames_counter;


action_t action = {};

static uint32_t
delay(uint32_t frames)
{
	if(!action.initialized)
	{
		action.initialized = 1;
		action.completed = 0;

		delay_frames_counter = 0;
	}

	delay_frames_counter++;
	if(delay_frames_counter > frames)
	{	
		action.initialized = 0;
		action.completed = 1;
	}

	return action.completed;
}

static uint32_t
move(entity_t *entity, int32_t x, int32_t y)
{
	if(!action.initialized)
	{
		action.initialized = 1;
		action.completed = 0;

		entity->tile_pos.x += x;
		entity->tile_pos.y += y;
		entity->move_lerp_start.x = entity->pos.x;
		entity->move_lerp_start.y = entity->pos.y;
		entity->move_lerp_end.x = entity->pos.x + (x * world.tile_size * world.scale);
		entity->move_lerp_end.y = entity->pos.y + (y * world.tile_size * world.scale);
		entity->move_lerp_dt = 0.0f;
	}

	entity->move_lerp_dt += entity->move_speed;
	if(entity->move_lerp_dt >= 1.0f)
	{
		entity->move_lerp_dt = 1.0f;
		action.initialized = 0;
		action.completed = 1;
	}
	entity->pos.x = lerp(entity->move_lerp_start.x, entity->move_lerp_end.x, entity->move_lerp_dt);
	entity->pos.y = lerp(entity->move_lerp_start.y, entity->move_lerp_end.y, entity->move_lerp_dt);

	return action.completed;
}

static uint32_t
move_left(entity_t *entity)
{
	return move(entity, -1, 0);
}

static uint32_t
move_right(entity_t *entity)
{
	return move(entity, 1, 0);
}

static uint32_t
move_up(entity_t *entity)
{
	return move(entity, 0, -1);
}

static uint32_t
move_down(entity_t *entity)
{
	return move(entity, 0, 1);
}

static void
cutscene_1_play(SDL_Renderer *renderer, entity_t *entities)
{
	if(cutscene_index == 0)
		cutscene_index += postprocess_fadein(renderer);
	if(cutscene_index == 1)
		cutscene_index += delay(30);
	if(cutscene_index == 2)
		cutscene_index += move_down(&entities[0]);
	if(cutscene_index == 3)
		cutscene_index += move_right(&entities[0]);
	if(cutscene_index == 4)
		cutscene_index += delay(30);
	if(cutscene_index == 5)
	{	
		turn_state = DIALOGUE;
		cutscene_index += dialogue_play(renderer, 
						entities[0].texture,
						"I'm a SLIME...\nMOTHERFUCKER!!!\n",
						sizeof("I'm a SLIME...\nMOTHERFUCKER!!!\n"),
						0);
	}
	if(cutscene_index == 6)
		cutscene_index += delay(30);
	if(cutscene_index == 7)
		cutscene_index += move_left(&entities[2]);
	if(cutscene_index == 8)
		cutscene_index += move_up(&entities[2]);
	if(cutscene_index == 9)
		cutscene_index += move_right(&entities[2]);
	if(cutscene_index == 10)
		cutscene_index += move_down(&entities[2]);
	if(cutscene_index == 11)
		cutscene_index += delay(30);
	if(cutscene_index == 12)
	{
		turn_state = DIALOGUE;
		cutscene_index += dialogue_play(renderer, 
						entities[1].texture,
						"I'm a BUNNY...\nSUPER-MOTHERFUCKER!!!\n",
						sizeof("I'm a BUNNY...\nSUPER-MOTHERFUCKER!!!\n"),
						1);
	}
	if(cutscene_index == 13)
		cutscene_index += delay(30);

	if(cutscene_index == 14)
	{
		current_cutscene_id = 0;
		cutscene_index = 0;
		turn_state = MENU;
	}
}

static uint32_t
cutscene_2_play(SDL_Renderer *renderer, entity_t *entities)
{
	uint32_t completed = 0;

	if(cutscene_index == 0)
		cutscene_index += delay(30);
	if(cutscene_index == 1)
	{	
		turn_state = DIALOGUE;
		cutscene_index += dialogue_play(renderer, 
						entities[0].texture,
						"Me SUPPAPAWAAA!!!\n",
						sizeof("Me SUPPAPAWAAA!!!\n"),
						0);
	}
	if(cutscene_index == 2)
		cutscene_index += delay(30);
	if(cutscene_index == 3)
		cutscene_index += postprocess_fadeout(renderer);

	if(cutscene_index == 4)
	{
		current_cutscene_id = 0;
		cutscene_index = 0;
		completed = 1;


		turn_state = MENU;
	}
	
	return completed;
}

static uint32_t
cutscene_3_play(SDL_Renderer *renderer, entity_t *entities)
{
	uint32_t completed = 0;

	if(cutscene_index == 0)
		cutscene_index += delay(30);
	if(cutscene_index == 1)
	{	
		cutscene_index += postprocess_fadein(renderer);
	}

	if(cutscene_index == 2)
	{
		current_cutscene_id = 0;
		cutscene_index = 0;
		completed = 1;


		turn_state = MENU;
	}
	
	return completed;
}



static SDL_Texture*
texture_load(SDL_Renderer *renderer, char *path)
{
	SDL_Surface *surface = IMG_Load(path);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	return texture;
}

// ------------------------------------------------------------------------------------------
// TODO(martin): next stage -----------------------------------------------------------------
// ------------------------------------------------------------------------------------------
battle_t battle = {};
static void
battle_over(entity_t *players)
{
	uint32_t game_win = 1;
	uint32_t game_lose = 2;
	for(int i = 0; i < PLAYERS_NUM; i++)
	{
		if(players[i].team == 1 && !players[i].dead)
			game_lose = 0;
		if(players[i].team == 2 && !players[i].dead)
			game_win = 0;
	}

	if(game_lose != 0)
		battle.is_over = game_lose;
	else if(game_win != 0)
		battle.is_over = game_win;
}



// ------------------------------------------------------------------------------------------
// TODO(martin): next turn ------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
turn_t turn = {};
static void
turn_pass(entity_t *players, entity_t **current_player, tile_t *current_tiles, cursor_t *cursor)
{
	// update speed accumulator if player is alive
	for(int i = 0; i < PLAYERS_NUM; i++)
		if(players[i].hp > 0)
			players[i].speed_accumulator += players[i].speed;
		else
			players[i].speed_accumulator = 0;

	// TODO(martin): get player with highest priority - fix priority logic
	int32_t tmp_index = -1;
	uint32_t higher_speed = 0;
	for(int i = 0; i < PLAYERS_NUM; i++)
	{
		if(players[i].speed_accumulator >= higher_speed)
		{
			higher_speed = players[i].speed_accumulator;
			tmp_index = i;
		}
	}
	players[tmp_index].speed_accumulator -= players[tmp_index].speed;
	*current_player = &players[tmp_index];

	// set cursor pos to player
	cursor->tile_pos = (*current_player)->tile_pos;

	// compute moving area
	for(int i = 0; i < TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X; i++)
	{
		current_tiles[i].truly_reachable = 0;

		vec2i_t tile_to_test_pos = {current_tiles[i].pos.x, current_tiles[i].pos.y};

		if(tiles_distance(tile_to_test_pos, (*current_player)->tile_pos) < STATS_MOVE && !current_tiles[i].obstacle) 
			current_tiles[i].reachable = 1;
		else
			current_tiles[i].reachable = 0;
	}
	for(int i = 0; i < PLAYERS_NUM; i++)
	{
			if(&players[i] != (*current_player))
				get_tile(current_tiles, players[i].tile_pos.x, players[i].tile_pos.y)->reachable = 0;
	}
	for(int i = 0; i < TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X; i++)
	{
		if(current_tiles[i].reachable == 1)
		{
			tile_t *tile_start = get_tile(current_tiles, (*current_player)->tile_pos.x, (*current_player)->tile_pos.y);
			tile_t *tile_end = &current_tiles[i];
			solve_astar(current_tiles,  tile_start, tile_end);
			uint32_t moves_num = generate_moves_tmp(tile_end);

			if(moves_num <= STATS_MOVE && moves_num != 1)
				current_tiles[i].truly_reachable = 1;
		}
	}

	// reset actions
	turn_state = MENU;
	menu_options[menu_option_move] = 0;
	menu_options[menu_option_att] = 0;
	turn.has_moved = 0;
	turn.has_attacked = 0;
}

int main()
{
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window *window = SDL_CreateWindow("Oliark", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("./font.ttf", 24);
	font_dialogue = TTF_OpenFont("./font.ttf", 48);

	memory_t memory = {};
	memory.permanent_storage_size = 64ULL * 1024ULL * 1024ULL;
	//memory.permanent_storage = mmap(0, memory.permanent_storage_size , PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	memory.permanent_storage = calloc(memory.permanent_storage_size, sizeof(char));

	if(!memory.permanent_storage)
	{
		return 1;
	}

	game_state_t *game_state = (game_state_t*)memory.permanent_storage;
	memory_arena_init(&game_state->memory_arena, memory.permanent_storage_size - sizeof(game_state), (uint32_t*)memory.permanent_storage + sizeof(game_state));
	game_state->tiles = (uint32_t*)push_struct(&game_state->memory_arena, sizeof(uint32_t) * TILE_MAP_COUNT_X  * TILE_MAP_COUNT_Y);

	world.tile_size = 8;
	world.scale = 8;
	world.offset.x = 100;
	world.offset.y = 30;



	// ------------------------------------------------------------------
	// TODO(martin): init tiles  ----------------------------------------
	// ------------------------------------------------------------------
	uint32_t tilemap1[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		{1,1,1,1,1,1,1,1,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,1,0,1,1,1,0,1,},
		{1,0,0,0,1,0,0,1,},
		{1,0,0,0,1,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,1,1,1,1,1,1,1,},
	};
	uint32_t tilemap2[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		{1,1,1,1,1,1,1,1,},
		{1,0,1,0,1,0,0,1,},
		{1,0,1,0,1,0,0,1,},
		{1,0,1,0,1,1,0,1,},
		{1,0,1,0,0,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,0,1,0,0,1,},
		{1,1,1,1,1,1,1,1,},
	};

	
	tile_t tiles1[TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X] = {};
	tiles_init(tiles1, *tilemap1);

	tile_t tiles2[TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X] = {};
	tiles_init(tiles2, *tilemap2);

#define TILES_NUM 2
	tile_t *tiles[TILES_NUM] = {};
	tiles[0] = tiles1;
	tiles[1] = tiles2;

	tile_t *current_tiles = tiles[0];
	
	// ------------------------------------------------------------------
	// TODO(martin): init players ---------------------------------------
	// ------------------------------------------------------------------
	SDL_Texture *slime_texture = texture_load(renderer, "./slime.png");
	SDL_Texture *bunny_texture = texture_load(renderer, "./bunny.png");

	entity_t players[PLAYERS_NUM] = {};
	player_init(&players[0], &world, slime_texture, 1, 1, "Slime 1", 21, 255, 120, 0.05f, 1);
	player_init(&players[1], &world, bunny_texture, 5, 2, "Bunny 1", 24, 255, 100, 0.1f, 2);
	player_init(&players[2], &world, bunny_texture, 6, 6, "Bunny 2", 24, 255, 100, 0.2f, 2);

	uint32_t player_current_index = 0;
	entity_t *player_current = &players[player_current_index];

	// init cursor
	cursor_t cursor = {};
	cursor.tile_pos.x = player_current->tile_pos.x;
	cursor.tile_pos.y = player_current->tile_pos.y;



	// ------------------------------------------------------------------
	// TODO(martin): load battle menu ui resources ----------------------
	// ------------------------------------------------------------------
	SDL_Texture *ui_move_en_texture = texture_load(renderer, "./ui_move_en.png");
	SDL_Texture *ui_move_dis_texture = texture_load(renderer, "./ui_move_dis.png");
	SDL_Texture *ui_move_done_texture = texture_load(renderer, "./ui_move_done.png");
	SDL_Texture *ui_att_en_texture = texture_load(renderer, "./ui_att_en.png");
	SDL_Texture *ui_att_dis_texture = texture_load(renderer, "./ui_att_dis.png");
	SDL_Texture *ui_att_done_texture = texture_load(renderer, "./ui_att_done.png");
	SDL_Texture *ui_pass_en_texture = texture_load(renderer, "./ui_pass_en.png");
	SDL_Texture *ui_pass_dis_texture = texture_load(renderer, "./ui_pass_dis.png");





	uint32_t is_moving = 0;



	move_buffer_index = -1;









	turn_state = MENU;



	turn_pass(players, &player_current, current_tiles, &cursor);
	


	uint32_t dialogue_num = 0;


	// cutscene 1
#if 0
	turn_state = CUTSCENE;
	current_cutscene_id = 1;
#endif



	int is_running = 1;
	int fps = 30;
	int32_t millis_per_frame = 1000 / fps;
	int32_t current_millis = 0;
	while(is_running)
	{
		// set fps
		float dt = SDL_GetTicks() - current_millis;
		float time_to_wait = millis_per_frame - dt;
		if(time_to_wait < millis_per_frame)
			SDL_Delay(time_to_wait);
		current_millis = SDL_GetTicks();

		int32_t test_tile_x = 0;
		int32_t test_tile_y = 0;

		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
				{
					is_running = 0;
				} break;

				case SDL_KEYDOWN:
				{
					switch(event.key.keysym.sym)
					{
						case SDLK_ESCAPE:
						{
							is_running = 0;
						} break;

						case SDLK_LEFT:
						{
							if(turn_state == EXPLORE)
							{
								cursor.tile_pos.x += -1;
							}
							else if(turn_state == MOVE || turn_state == ATT)
							{
								if(move_buffer_index < 0 && !is_moving)
									test_tile_x = -1;
							}
						} break;

						case SDLK_RIGHT:
						{
							if(turn_state == EXPLORE)
							{
								cursor.tile_pos.x += 1;
							}
							else if(turn_state == MOVE || turn_state == ATT)
							{
								if(move_buffer_index < 0 && !is_moving)
									test_tile_x = +1;
							}
						} break;

						case SDLK_UP:
						{
							if(turn_state == EXPLORE)
							{
								cursor.tile_pos.y -= 1;
							}
							else if(turn_state == MENU)
							{
								menu_battle_move_to_prev_available_option();
							}
							else if(turn_state == MOVE || turn_state == ATT)
							{
								if(move_buffer_index < 0 && !is_moving)
									test_tile_y = -1;
							}
						} break;

						case SDLK_DOWN:
						{
							if(turn_state == EXPLORE)
							{
								cursor.tile_pos.y += 1;
							}
							else if(turn_state == MENU)
							{
								menu_battle_move_to_next_available_option();
							}
							else if(turn_state == MOVE || turn_state == ATT)
							{
								if(move_buffer_index < 0 && !is_moving)
									test_tile_y = +1;
							}
						} break;

						case SDLK_SPACE:
						{
						} break;

						case SDLK_h:
						{
						} break;

						case SDLK_j:
						{
						} break;

						case SDLK_k:
						{
						} break;

						case SDLK_l:
						{
						} break;

						case SDLK_q:
						{
							if(current_cutscene_id == 0)
							{
								turn_state = CUTSCENE;
								current_cutscene_id = 1;
							}
						} break;

						case SDLK_x:
						{
							if(turn_state == MENU)
							{
								cursor.tile_pos = player_current->tile_pos;
								turn_state = EXPLORE;
							}
							else if(turn_state == MOVE)
							{
								cursor.tile_pos = player_current->tile_pos;
								turn_state = MENU;
							}
							else if(turn_state == ATT)
							{
								cursor.tile_pos = player_current->tile_pos;
								turn_state = MENU;
							}
						} break;

						case SDLK_z:
						{
							if(turn_state == EXPLORE)
							{
								cursor.tile_pos = player_current->tile_pos;
								turn_state = MENU;
							}
							else if(turn_state == DIALOGUE)
							{
								key_z_pressed = 1;
							}
							else if(turn_state == MENU)
							{
								if(menu_index == 0 && !turn.has_moved)
									turn_state = MOVE;
								else if(menu_index == 1 && !turn.has_attacked)
									turn_state = ATT;
								else if(menu_index == 2)
								{		
									turn_pass(players, &player_current, current_tiles, &cursor);
									turn_state = MENU;
									menu_index = menu_option_move;
								}
							}
							else if(turn_state == MOVE)
							{
								tile_t *start_tile = get_tile(current_tiles, player_current->tile_pos.x, player_current->tile_pos.y);
								tile_t *end_tile = get_tile(current_tiles, cursor.tile_pos.x, cursor.tile_pos.y);

								if(!end_tile->obstacle && end_tile->truly_reachable && tiles_distance(end_tile->pos, start_tile->pos) < STATS_MOVE)
								{
									if(move_buffer_index < 0 && !is_moving)
									{
										// update possible movements
										solve_astar(current_tiles, start_tile, end_tile);
										generate_moves(end_tile);

										turn.has_moved = 1;
									}
								}
							}

							else if(turn_state == ATT)
							{
								entity_t *target = get_player_under_cursor(players, &cursor);
								if(target && target->hp > 0) 
								{
									target->hp -= player_current->att;
									if(target->hp <= 0)
									{
										target->dead = 1;
									}
									cursor.tile_pos = player_current->tile_pos;
									menu_options[menu_option_att] = 1;
									menu_index++;
									turn.has_attacked = 1;
									turn_state = MENU;

									if(player_current->hp <= 0)
									{
										turn_state = MENU;
										menu_index = menu_option_move;
									}
								}
							}
						} break;

					}
				} break;
			}
		}


		// move cursor inside map
		if(cursor.tile_pos.x + test_tile_x >= 0 && cursor.tile_pos.x + test_tile_x < TILE_MAP_COUNT_X)
			cursor.tile_pos.x += test_tile_x;
		if(cursor.tile_pos.y + test_tile_y >= 0 && cursor.tile_pos.y + test_tile_y < TILE_MAP_COUNT_Y)
			cursor.tile_pos.y += test_tile_y;



		// ------------------------------------------------------------------
		// TODO(martin): end battle -----------------------------------------
		// ------------------------------------------------------------------
#if 0
		battle_over(players);
		
#else
		uint32_t game_win = 1;
		uint32_t game_lose = 1;
		for(int i = 0; i < PLAYERS_NUM; i++)
		{
			if(players[i].team == 1 && !players[i].dead)
				game_lose = 0;
			if(players[i].team == 2 && !players[i].dead)
				game_win = 0;
		}
		if(game_win == 1)
		{
			printf("you win\n");
			for(int i = 0; i < PLAYERS_NUM; i++)
			{
				players[i].dead = 0;
				players[i].hp = 2;
			}
			current_cutscene_id = 2;
		}
		if(game_lose == 1)
		{
			printf("you lose\n");
			for(int i = 0; i < PLAYERS_NUM; i++)
			{
				players[i].dead = 0;
				players[i].hp = 2;
			}
			current_cutscene_id = 2;
		}
#endif
		

		// ------------------------------------------------------------------
		// TODO(martin): move player to cursor ------------------------------
		// ------------------------------------------------------------------
		if(move_buffer_index >= 0)
		{
			uint32_t action_completed = move(player_current, move_buffer[move_buffer_index].x, move_buffer[move_buffer_index].y);
			if(action_completed) move_buffer_index--;
			if(move_buffer_index < 0)
			{
				menu_options[menu_option_move] = 1;
				menu_battle_move_to_next_available_option();
				turn_state = MENU;
			}
		}











		// render background
		SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
		SDL_RenderClear(renderer);

		// render tilemap + moving area
		for(int i = 0; i < TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X; i++)
		{
			// tilemap
			int32_t tile_id = current_tiles[i].id;

			if(tile_id == 0)
				SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
			else if(tile_id == 1)
				SDL_SetRenderDrawColor(renderer, 48, 48, 48, 255);

			SDL_Rect tile_rect = {	current_tiles[i].pos.x * world.tile_size * world.scale + world.offset.x, 
						current_tiles[i].pos.y * world.scale * world.tile_size + world.offset.y, 
						world.tile_size * world.scale, 
						world.tile_size * world.scale};
			SDL_RenderFillRect(renderer, &tile_rect);

			// moving area
			if(turn_state == MOVE)
			{
				if(move_buffer_index < 0 && !is_moving && !turn.has_moved)
				{
					if(current_tiles[i].truly_reachable && !current_tiles[i].obstacle)
					{
						SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
						SDL_Rect area_rect = {	current_tiles[i].pos.x * world.tile_size * world.scale + world.offset.x, 
									current_tiles[i].pos.y * world.scale * world.tile_size + world.offset.y, 
									world.tile_size * world.scale, 
									world.tile_size * world.scale};
						SDL_RenderFillRect(renderer, &area_rect);
					}
				}
			}

			// attack area
			else if(turn_state == ATT)
			{
				if(move_buffer_index < 0 && !is_moving)
				{
					if(tiles_distance(current_tiles[i].pos, player_current->tile_pos) < 2) 
					{
						SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
						SDL_Rect attack_rect = {current_tiles[i].pos.x * world.tile_size * world.scale + world.offset.x, 
									current_tiles[i].pos.y * world.scale * world.tile_size + world.offset.y, 
									world.tile_size * world.scale, 
									world.tile_size * world.scale};
						SDL_RenderFillRect(renderer, &attack_rect);
					}
				}
			}
		}

		// player bg debug
		SDL_SetRenderDrawColor(renderer, 255, 128, 0, 255);
		SDL_Rect player_bg_debug_rect = {	player_current->tile_pos.x * world.tile_size * world.scale + world.offset.x, 
						player_current->tile_pos.y * world.scale * world.tile_size + world.offset.y, 
						world.tile_size * world.scale, 
						world.tile_size * world.scale};
		SDL_RenderFillRect(renderer, &player_bg_debug_rect);

		// render cursor
		SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
		SDL_Rect cursor_rect = {cursor.tile_pos.x * world.tile_size * world.scale + world.offset.x, 
					cursor.tile_pos.y * world.scale * world.tile_size + world.offset.y, 
					world.tile_size * world.scale, 
					world.tile_size * world.scale};
		SDL_RenderFillRect(renderer, &cursor_rect);

		// render players
		for(int i = 0; i < PLAYERS_NUM; i++)
		{
			SDL_Rect src_rect = {	0, 
						0, 
						world.tile_size, 
						world.tile_size};
			if(players[i].hp > 0)
			{
				player_update_animation(&players[i]);
				src_rect.x = players[i].animation_frame * world.tile_size;
			}
			else
			{
				src_rect.x = 16;
			}
			SDL_Rect dst_rect = {	players[i].pos.x, 
						players[i].pos.y, 
						world.tile_size * world.scale, 
						world.tile_size * world.scale};
			SDL_RenderCopy(renderer, players[i].texture, &src_rect, &dst_rect);
		}

		// render ui battle menu
		if(turn_state == MENU)
		{
			if(menu_index == 0)
			{
				if(menu_options[menu_option_move] == 0)
				{
					SDL_Rect ui_move_en_dst_rect = {32, 320, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_move_en_texture, NULL, &ui_move_en_dst_rect);
				}
				else
				{
					SDL_Rect ui_move_done_dst_rect = {32, 320, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_move_done_texture, NULL, &ui_move_done_dst_rect);
				}

				if(!turn.has_attacked)
				{
					SDL_Rect ui_att_dis_dst_rect = {32, 400, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_att_dis_texture, NULL, &ui_att_dis_dst_rect);
				}
				else
				{
					SDL_Rect ui_att_done_dst_rect = {32, 400, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_att_done_texture, NULL, &ui_att_done_dst_rect);
				}

				SDL_Rect ui_pass_dis_dst_rect = {32, 480, 32 * world.scale, 8 * world.scale};
				SDL_RenderCopy(renderer, ui_pass_dis_texture, NULL, &ui_pass_dis_dst_rect);
			}
			else if(menu_index == 1)
			{
				if(menu_options[menu_option_move] == 0)
				{
					SDL_Rect ui_move_dis_dst_rect = {32, 320, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_move_dis_texture, NULL, &ui_move_dis_dst_rect);
				}
				else
				{
					SDL_Rect ui_move_done_dst_rect = {32, 320, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_move_done_texture, NULL, &ui_move_done_dst_rect);
				}

				if(!turn.has_attacked)
				{
					SDL_Rect ui_att_en_dst_rect = {32, 400, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_att_en_texture, NULL, &ui_att_en_dst_rect);
				}
				else
				{
					SDL_Rect ui_att_done_dst_rect = {32, 400, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_att_done_texture, NULL, &ui_att_done_dst_rect);
				}

				SDL_Rect ui_pass_dis_dst_rect = {32, 480, 32 * world.scale, 8 * world.scale};
				SDL_RenderCopy(renderer, ui_pass_dis_texture, NULL, &ui_pass_dis_dst_rect);
			}
			else if(menu_index == 2)
			{
				if(menu_options[menu_option_move] == 0)
				{
					SDL_Rect ui_move_dis_dst_rect = {32, 320, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_move_dis_texture, NULL, &ui_move_dis_dst_rect);
				}
				else
				{
					SDL_Rect ui_move_done_dst_rect = {32, 320, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_move_done_texture, NULL, &ui_move_done_dst_rect);
				}

				if(!turn.has_attacked)
				{
					SDL_Rect ui_att_dis_dst_rect = {32, 400, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_att_dis_texture, NULL, &ui_att_dis_dst_rect);
				}
				else
				{
					SDL_Rect ui_att_done_dst_rect = {32, 400, 32 * world.scale, 8 * world.scale};
					SDL_RenderCopy(renderer, ui_att_done_texture, NULL, &ui_att_done_dst_rect);
				}

				SDL_Rect ui_pass_en_dst_rect = {32, 480, 32 * world.scale, 8 * world.scale};
				SDL_RenderCopy(renderer, ui_pass_en_texture, NULL, &ui_pass_en_dst_rect);
			}

		}





		// --------------------------------------------------------
		// TODO(martin): dialogue 
		// --------------------------------------------------------
		if(turn_state == DIALOGUE)
		{
			
			if(dialogue_num == 0)
			{
			}
			else if(dialogue_num == 1)
			{
			}
		}

		// --------------------------------------------------------
		// TODO(martin): cutscene 
		// --------------------------------------------------------
		if(current_cutscene_id == 1)
		{
			cutscene_1_play(renderer, players);
		}
		else if(current_cutscene_id == 2)
		{
			if(cutscene_2_play(renderer, players))
			{
				current_tiles = tiles[1];
				current_cutscene_id = 3;
			}
		}
		else if(current_cutscene_id == 3)
		{
			if(cutscene_3_play(renderer, players))
			{
			}
		}



		// --------------------------------------------------------
		// TODO(martin): postprocess 
		// --------------------------------------------------------
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, (fade.val));
		SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
		SDL_RenderFillRect(renderer, &rect);




		// --------------------------------------------------------
		// DEBUG ONLY!!
		// --------------------------------------------------------
		char *dst = "";

		SDL_Color color = {255, 255, 255, 255};
		dst = concat(dst, "(y: ");
		dst = concat(dst, _itoa(cursor.tile_pos.y));
		dst = concat(dst, "  x: ");
		dst = concat(dst, _itoa(cursor.tile_pos.x));
		dst = concat(dst, ")");
		ui_font_text_render(renderer, font, color, dst, 16, 16);

		dst = "";
		dst = concat(dst, "FPS: ");
		dst = concat(dst, _itoa(fps));
		dst = concat(dst, " - MILLIS: ");
		dst = concat(dst, _itoa(millis_per_frame));
		ui_font_text_render(renderer, font, color, dst, 16, 48);

		dst = "";
		dst = concat(dst, "DT: ");
		dst = concat(dst, _itoa(dt));
		ui_font_text_render(renderer, font, color, dst, 16, 80);

		dst = "";
		dst = concat(dst, "NAME: ");
		dst = concat(dst, player_current->name);
		dst = concat(dst, " - HP: ");
		dst = concat(dst, _itoa(player_current->hp));
		dst = concat(dst, " - ATT: ");
		dst = concat(dst, _itoa(player_current->att));
		ui_font_text_render(renderer, font, color, dst, 16, 144);

		entity_t *target = 0;
		target = get_player_under_cursor(players, &cursor);

		if(target)
		{
			dst = "";
			dst = concat(dst, "NAME: ");
			dst = concat(dst, target->name);
			dst = concat(dst, " - HP: ");
			dst = concat(dst, _itoa(target->hp));
			dst = concat(dst, " - ATT: ");
			dst = concat(dst, _itoa(target->att));
			ui_font_text_render(renderer, font, color, dst, 16, 176);
		}



		SDL_RenderPresent(renderer);
	}

	SDL_Quit();

	return 0;
}

