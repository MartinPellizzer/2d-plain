// TODO(martin): set player animation direction from spritesheet... add src_rect and dst_rect inside struct?

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <math.h>
#include <sys/mman.h>

#include "main.h"
#include "util.h"

#define MAIN_SCALE 3
#if 0
	#define SCREEN_WIDTH (1920)
	#define SCREEN_HEIGHT (1080)

	#define SCREEN_WIDTH (1920 / 2)
	#define SCREEN_HEIGHT (1080 / 2)
#else
	#define SCREEN_WIDTH (320 * MAIN_SCALE)
	#define SCREEN_HEIGHT (180 * MAIN_SCALE)
#endif

#define move_buffer_num 16
int32_t move_buffer_index;
vec2f_t move_buffer[move_buffer_num];

#define STATS_MOVE 4
TTF_Font *gb_font;

enum battle_state_e
{
	DEBUG_EXPLORE,

	CUTSCENE,
	PLACEMENT_SELECT_UNIT,
	PLACEMENT_PLACE_UNIT,
	EXPLORE,
	MENU,
	MOVE,
	ATT
};

camera_t camera = {};
world_t world = {};

uint32_t battle_state;

static vec2f_t
get_pos_from_tile(vec2i_t tile_pos, world_t *world)
{
	vec2f_t pos = {};
	pos.x = (tile_pos.x * world->tile_size * world->scale * 0.5f) - (tile_pos.y * world->tile_size * world->scale * 0.5f);
	pos.y = (tile_pos.x * world->tile_size * world->scale * 0.25f) + (tile_pos.y * world->tile_size * world->scale * 0.25f);
	return pos;
}



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



// ------------------------------------------------------------------------------------------
// TODO(martin): input ----------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
typedef struct input_t
{
	uint32_t key_escape_pressed; 
	uint32_t key_space_pressed; 
	uint32_t key_left_pressed; 
	uint32_t key_right_pressed; 
	uint32_t key_up_pressed; 
	uint32_t key_down_pressed; 
	uint32_t key_x_pressed; 
	uint32_t key_z_pressed; 
} input_t;

input_t input = {};

static void
input_update(uint32_t *is_running)
{
	input.key_right_pressed = 0;
	input.key_left_pressed = 0;
	input.key_up_pressed = 0;
	input.key_down_pressed = 0;
	input.key_z_pressed = 0;
	input.key_x_pressed = 0;

	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_QUIT:
			{
				*is_running = 0;
			} break;

			case SDL_KEYDOWN:
			{
				switch(event.key.keysym.sym)
				{
					case SDLK_ESCAPE:
					{
						*is_running = 0;
					} break;

					case SDLK_SPACE:
					{
						input.key_space_pressed = 1;
					} break;
						
					case SDLK_LEFT:
					{
						input.key_left_pressed = 1;
					} break;

					case SDLK_RIGHT:
					{
						input.key_right_pressed = 1;
					} break;

					case SDLK_UP:
					{
						input.key_up_pressed = 1;
					} break;

					case SDLK_DOWN:
					{
						input.key_down_pressed = 1;
					} break;

					case SDLK_x:
					{
						input.key_x_pressed = 1;
					} break;

					case SDLK_z:
					{
						input.key_z_pressed = 1;
					} break;
				}
			} break;
		}
	}
	
}



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

		int32_t y = 300 + (i * 50);
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
dialogue_play(SDL_Renderer *renderer, input_t *input, SDL_Texture *texture, char *text, uint32_t text_length, uint32_t alignment)
{
	uint32_t completed = 0;
	
	cursor_index++;

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	SDL_Rect bg_dst_rect;
	bg_dst_rect.x = (alignment == 0) ? 0 : SCREEN_WIDTH - 700;
	bg_dst_rect.y = SCREEN_HEIGHT - 250;
	bg_dst_rect.w = 700;
	bg_dst_rect.h = 200;
	SDL_RenderFillRect(renderer, &bg_dst_rect);

	SDL_Rect player_src_rect;
	player_src_rect.x = 0;
	player_src_rect.y = 0;
	player_src_rect.w = 8;
	player_src_rect.h = 8;

	SDL_Rect player_dst_rect; 
	player_dst_rect.x = (alignment == 0) ? 32 : SCREEN_WIDTH - 200;
	player_dst_rect.y = 280;
	player_dst_rect.w = 3 * 8 * 8;
	player_dst_rect.h = 3 * 8 * 8;

	SDL_RenderCopy(renderer, texture, &player_src_rect, &player_dst_rect);

	if(cursor_index > text_length) 
	{
		cursor_index = text_length;
		
		if(input->key_z_pressed)
		{
			cursor_index = 0;
			completed = 1;
		}
	}
	else
	{
		if(input->key_z_pressed)
		{
			cursor_index = text_length;
		}
	}

	if(alignment == 0)
	{
		dialogue_render(renderer, text, cursor_index, 250);
	}
	else
	{
		dialogue_render(renderer, text, cursor_index, 300);
	}
	
	return completed;
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

tile_t* get_tile(tilemap_t tilemap[], uint32_t x, uint32_t y)
{
	return &tilemap->tiles[y * tilemap->tiles_count_x  + x];
}

// -------------------------------------------------------------------------
// TODO(martin): =astar - sort nodes by globals ----------------------------
// -------------------------------------------------------------------------
static void
tiles_init(tilemap_t *tilemap, uint32_t *tileset)
{
	for(int y = 0; y < tilemap->tiles_count_y; y++)
	{
		for(int x = 0; x < tilemap->tiles_count_x; x++)
		{
			tile_t *tile = get_tile(tilemap, x, y);

			tile->id = tileset[y * tilemap->tiles_count_x + x]; 
			if(tileset[y * tilemap->tiles_count_x + x] == 0)
			{
				tile->obstacle = 1;
			}
			else
			{
				tile->obstacle = 0;
			}
			tile->pos.x = x;
			tile->pos.y = y;

			if(y > 0)
			{
				tile_t *tile_neighbor = get_tile(tilemap, x + 0, y - 1);
				tile->neighbors[tile->neighbors_num] = tile_neighbor;  
				tile->neighbors_num += 1;
			}
			if(y < tilemap->tiles_count_y - 1)
			{
				tile_t *tile_neighbor = get_tile(tilemap, x + 0, y + 1);
				tile->neighbors[tile->neighbors_num] = tile_neighbor; 
				tile->neighbors_num += 1;
			}
			if(x > 0)
			{
				tile_t *tile_neighbor = get_tile(tilemap, x - 1, y + 0);
				tile->neighbors[tile->neighbors_num] = tile_neighbor; 
				tile->neighbors_num += 1;
			}
			if(x < tilemap->tiles_count_x - 1)
			{
				tile_t *tile_neighbor = get_tile(tilemap, x + 1, y + 0);
				tile->neighbors[tile->neighbors_num] = tile_neighbor; 
				tile->neighbors_num += 1;
			}
		}
	}
}

void solve_astar(tilemap_t *tilemap, tile_t *tile_start, tile_t *tile_end)
{
	tile_t *tiles = tilemap->tiles;

	for(int y = 0; y < tilemap->tiles_count_y; y++)
	{
		for(int x = 0; x < tilemap->tiles_count_x; x++)
		{
			int i = y * tilemap->tiles_count_x + x;

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
		
		if(index < 0)
			break;
		
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
// TODO(martin): =player ---------------------------------------------------
// -------------------------------------------------------------------------
static void
player_update_animation(entity_t *player)
{
	player->animation_counter++;
	if(player->animation_counter >= 10)
	{
		player->animation_counter = 0;
		player->animation_frame++;
		player->animation_frame = player->animation_frame % 2;
	}
}

entity_t* get_player_under_cursor(entity_t **players, new_entity_t *cursor)
{
	int i = 0;
	while(players[i])
	{
		if(cursor->transform->coord.x == players[i]->tile_pos.x && cursor->transform->coord.y == players[i]->tile_pos.y)
		{
			return players[i];
		}
		i++;
	}
	
	return 0;
}

static void
player_render(SDL_Renderer *renderer, entity_t *player)
{
	SDL_Rect src_rect = {	0, 
				world.tile_size * player->dir, 
				world.tile_size, 
				world.tile_size};
	SDL_Rect dst_rect = {	player->pos.x + camera.x, 
				player->pos.y + camera.y - (world.tile_size * world.scale * 0.625f), 
				world.tile_size * world.scale, 
				world.tile_size * world.scale};
	if(player->hp > 0)
	{
		player_update_animation(player);
		src_rect.x = player->animation_frame * world.tile_size;
	}
	else
	{
		src_rect.x = 16;
	}
	SDL_RenderCopy(renderer, player->texture, &src_rect, &dst_rect);
}

// ------------------------------------------------------------------------------------------
// cutscene ---------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------


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

float camera_following;

typedef struct pan_t
{
	float dt;
	int32_t start_x;
	int32_t start_y;
	int32_t end_x;
	int32_t end_y;
} pan_t;

pan_t pan = {};

static uint32_t
camera_pan_to_entity(camera_t *camera, entity_t *entity, float speed, world_t *world)
{
	if(!action.initialized)
	{
		action.initialized = 1;
		action.completed = 0;

		camera_following = 0;

		pan.start_x = camera->x;
		pan.start_y = camera->y;
		pan.end_x = (SCREEN_WIDTH / 2) - entity->pos.x - (world->tile_size * world->scale / 2);
		pan.end_y = (SCREEN_HEIGHT/ 2) - entity->pos.y - (world->tile_size * world->scale / 2);

		pan.dt = 0;
	}

	pan.dt += speed;
	if(pan.dt >= 1.0f)
	{
		pan.dt = 1.0f;
		action.initialized = 0;
		action.completed = 1;
	}
	camera->x = lerp(pan.start_x, pan.end_x, pan.dt);
	camera->y = lerp(pan.start_y, pan.end_y, pan.dt);

	return action.completed;
}

static uint32_t
move(entity_t *entity, int32_t x, int32_t y)
{
	if(!action.initialized)
	{
		action.initialized = 1;
		action.completed = 0;
		if(x == -1 && y == 0) entity->dir = animation_dir_left;
		else if(x == 1 && y == 0) entity->dir = animation_dir_right;
		else if(x == 0 && y == -1) entity->dir = animation_dir_up;
		else if(x == 0 && y == 1) entity->dir = animation_dir_down;
		entity->tile_pos.x += x;
		entity->tile_pos.y += y;
		entity->move_lerp_start.x = entity->pos.x;
		entity->move_lerp_start.y = entity->pos.y;
		vec2f_t end_pos = get_pos_from_tile(entity->tile_pos, &world);
		entity->move_lerp_end.x = end_pos.x;
		entity->move_lerp_end.y = end_pos.y;
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

static entity_t*
get_entity_by_id(entity_t **entities, uint32_t id)
{
	uint32_t i = 0;
	while(entities[i])
	{
		if(entities[i]->id == id)
		{
			return entities[i];
		}
		i++;
	}
	return 0;
}


typedef struct cutscene_t
{
	uint32_t id;
	uint32_t index;
	uint32_t completed;
} cutscene_t;

cutscene_t cutscene;

static uint32_t
cutscene_1_play(SDL_Renderer *renderer, world_t *world, input_t *input, entity_t **players, entity_t **enemies, camera_t *camera)
{
	cutscene.completed = 0;

	// DEBUG
	//cutscene.index = 18;

	switch(cutscene.index)
	{
		case 0: cutscene.index += postprocess_fadein(renderer); break;
		case 1: cutscene.index += delay(30); break;
		case 2: cutscene.index += move_down(get_entity_by_id(players, 0)); break;
		case 3: cutscene.index += move_down(get_entity_by_id(players, 0)); break;
		case 4: cutscene.index += move_right(get_entity_by_id(players, 0)); break;
#if 0
		case 5: cutscene.index += move_right(get_entity_by_id(players, 0)); break;
		case 6: cutscene.index += move_right(get_entity_by_id(players, 0)); break;
		case 7: cutscene.index += move_right(get_entity_by_id(players, 0)); break;
		case 8: cutscene.index += delay(30); break;
		case 9:
		{
			cutscene.id = 0;
			cutscene.index = 0;
			cutscene.completed = 1;
		} break;
#else
		case 5: cutscene.index += dialogue_play(renderer, input, get_entity_by_id(players, 0)->texture, "I'm a SLIME...\nMOTHERFUCKER!!!\n", sizeof("I'm a SLIME...\nMOTHERFUCKER!!!\n"), 0); break;
		case 6: cutscene.index += delay(30); break;
		case 7: cutscene.index += camera_pan_to_entity(camera, enemies[1], 0.1f, world); break;
		case 8: cutscene.index += delay(30); break;
		case 9: cutscene.index += move_left(enemies[1]); break;
		case 10: cutscene.index += move_up(enemies[1]); break;
		case 11: cutscene.index += move_right(enemies[1]); break;
		case 12: cutscene.index += move_down(enemies[1]); break;
		case 13: cutscene.index += delay(30); break;
		case 14: cutscene.index += dialogue_play(renderer, input, enemies[1]->texture, "I'm a BUNNY...\nSUPER-MOTHERFUCKER!!!\n", sizeof("I'm a BUNNY...\nSUPER-MOTHERFUCKER!!!\n"), 1); break;
		case 15: cutscene.index += delay(30); break;
		case 16: cutscene.index += camera_pan_to_entity(camera, players[0], 0.1f, world); break;
		case 17: cutscene.index += delay(30); break;
		case 18:
		{
			cutscene.id = 0;
			cutscene.index = 0;
			cutscene.completed = 1;
		} break;
#endif
	}

	return cutscene.completed;
}

static uint32_t
cutscene_2_play(SDL_Renderer *renderer, input_t *input, entity_t **entities)
{
	cutscene.completed = 0;

	if(cutscene.index == 0)
	{	
		cutscene.index += dialogue_play(renderer, 
						input,
						entities[0]->texture,
						"Me SUPPAPAWAAA!!!\n",
						sizeof("Me SUPPAPAWAAA!!!\n"),
						0);
	}
	if(cutscene.index == 1)
		cutscene.index += delay(30);
	if(cutscene.index == 2)
		cutscene.index += postprocess_fadeout(renderer);

	if(cutscene.index == 3)
	{
		cutscene.id = 0;
		cutscene.index = 0;
		cutscene.completed = 1;
	}
	
	return cutscene.completed;
}

static uint32_t
cutscene_3_play(SDL_Renderer *renderer, input_t *input, entity_t **entities)
{
	cutscene.completed = 0;

	if(cutscene.index == 0)
		cutscene.index += delay(30);
	if(cutscene.index == 1)
	{	
		cutscene.index += postprocess_fadein(renderer);
	}

	if(cutscene.index == 2)
	{
		cutscene.id = 0;
		cutscene.index = 0;
		cutscene.completed = 1;
	}
	
	return cutscene.completed;
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
#if 0
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
#endif



// ------------------------------------------------------------------------------------------
// TODO(martin): next turn ------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
turn_t turn = {};

static entity_t*
get_player_with_highest_priority(entity_t **players, entity_t **enemies)
{
	uint32_t i = 0;

	int32_t players_tmp_index = -1;
	uint32_t players_higher_speed = 0;
	i = 0;
	while(players[i])
	{
		if(players[i]->hp > 0)
		{
			players[i]->speed_accumulator += players[i]->speed;
			if(players[i]->speed_accumulator >= players_higher_speed)
			{
				players_higher_speed = players[i]->speed_accumulator;
				players_tmp_index = i;
			}
		}
		else
		{
			players[i]->speed_accumulator = 0;
		}
		i++;
	}

	int32_t enemies_tmp_index = -1;
	uint32_t enemies_higher_speed = 0;
	i = 0;
	while(enemies[i])
	{
		if(enemies[i]->hp > 0)
		{
			enemies[i]->speed_accumulator += enemies[i]->speed;
			if(enemies[i]->speed_accumulator >= enemies_higher_speed)
			{
				enemies_higher_speed = enemies[i]->speed_accumulator;
				enemies_tmp_index = i;
			}
		}
		else
		{
			enemies[i]->speed_accumulator = 0;
		}
		i++;
	}

	if(players[players_tmp_index]->speed_accumulator > enemies[enemies_tmp_index]->speed_accumulator) 
	{
		players[players_tmp_index]->speed_accumulator -= players[players_tmp_index]->speed;
		return players[players_tmp_index];
	}
	else
	{
		enemies[enemies_tmp_index]->speed_accumulator -= enemies[enemies_tmp_index]->speed;
		return enemies[enemies_tmp_index];
	}
}

static void
turn_pass(entity_t **players, entity_t **enemies, entity_t **current_player, tilemap_t *tilemap, new_entity_t *cursor)
{
	tile_t *current_tiles = tilemap->tiles;
	*current_player = get_player_with_highest_priority(players, enemies);

	// set cursor pos to player
	cursor->transform->coord = (*current_player)->tile_pos;

	// compute moving area
	for(int i = 0; i < tilemap->tiles_count_y * tilemap->tiles_count_x; i++)
	{
		current_tiles[i].truly_reachable = 0;

		vec2i_t tile_to_test_pos = {current_tiles[i].pos.x, current_tiles[i].pos.y};

		if(tiles_distance(tile_to_test_pos, (*current_player)->tile_pos) < (*current_player)->stats_move && !current_tiles[i].obstacle) 
			current_tiles[i].reachable = 1;
		else
			current_tiles[i].reachable = 0;
	}

	// compute astar
	int i = 0;
	while(players[i])
	{
		if(players[i] != (*current_player))
		{
			get_tile(tilemap, players[i]->tile_pos.x, players[i]->tile_pos.y)->reachable = 0;
		}
		i++;
	}

	for(int i = 0; i < tilemap->tiles_count_y * tilemap->tiles_count_x; i++)
	{
		if(current_tiles[i].reachable == 1)
		{
			tile_t *tile_start = get_tile(tilemap, (*current_player)->tile_pos.x, (*current_player)->tile_pos.y);
			tile_t *tile_end = &current_tiles[i];
			solve_astar(tilemap,  tile_start, tile_end);
			uint32_t moves_num = generate_moves_tmp(tile_end);

			if(moves_num <= (*current_player)->stats_move && moves_num != 1)
				current_tiles[i].truly_reachable = 1;
		}
	}

	// reset actions
	battle_state = MENU;
	menu_options[menu_option_move] = 0;
	menu_options[menu_option_att] = 0;
	turn.has_moved = 0;
	turn.has_attacked = 0;
}



// ------------------------------------------------------------------------------------------
// TODO(martin): ;cursor
// ------------------------------------------------------------------------------------------
static void
cursor_move(new_entity_t *cursor, int32_t x, int32_t y, tilemap_t *tilemap)
{
	if(cursor->transform->coord.x + x >= 0 && cursor->transform->coord.x + x < tilemap->tiles_count_x)
		cursor->transform->coord.x += x;
	if(cursor->transform->coord.y + y >= 0 && cursor->transform->coord.y + y < tilemap->tiles_count_y)
		cursor->transform->coord.y += y;
}

static void
cursor_render(SDL_Renderer *renderer, new_entity_t *cursor)
{
	SDL_Rect src_rect = {	0, 
				0, 
				world.tile_size, 
				world.tile_size};
	vec2f_t pos = get_pos_from_tile(cursor->transform->coord, &world);
	SDL_Rect dst_rect = {	pos.x + camera.x, 
				pos.y + camera.y, 
				world.tile_size * world.scale, 
				world.tile_size * world.scale};
	SDL_RenderCopy(renderer, cursor->texture, &src_rect, &dst_rect);
}



// ------------------------------------------------------------------------------------------
// TODO(martin): camera ---------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
static void
camera_focus_entity(camera_t *camera, entity_t *entity)
{
	camera->x = (SCREEN_WIDTH / 2) - entity->pos.x;
	camera->y = (SCREEN_HEIGHT / 2) - entity->pos.y;
}

// ------------------------------------------------------------------------------------------
// TODO(martin): ;placement area
// ------------------------------------------------------------------------------------------
static void
placement_area_render(SDL_Renderer *renderer, tilemap_t *tilemap, entity_t *tmp_player, new_entity_t *cursor)
{
	for(int i = 0; i < 8; i++)
	{
		SDL_Rect src_rect = {	0, 
					0, 
					world.tile_size, 
					world.tile_size};
		vec2f_t pos = get_pos_from_tile(tilemap->placements[i].transform->coord, &world);
		SDL_Rect dst_rect = {	pos.x + camera.x, 
					pos.y + camera.y, 
					world.tile_size * world.scale, 
					world.tile_size * world.scale};
		SDL_RenderCopy(renderer, cursor->texture, &src_rect, &dst_rect);
	}
	if(tmp_player)
	{
		tmp_player->pos = get_pos_from_tile(tmp_player->tile_pos, &world);
		player_render(renderer, tmp_player);
	}
}
static void
placement_ui_render(SDL_Renderer *renderer, entity_t **players, int32_t index)
{
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_Rect bg_dst_rect = {0, 
				SCREEN_HEIGHT - 200, 
				SCREEN_WIDTH, 
				100};
	SDL_RenderFillRect(renderer, &bg_dst_rect);
	SDL_Rect player_src_rect = {	0, 
					0, 
					world.tile_size, 
					world.tile_size};
	SDL_Rect player_dst_rect = {	SCREEN_WIDTH / 2, 
					SCREEN_HEIGHT - 200, 
					world.tile_size * world.scale, 
					world.tile_size * world.scale};
	SDL_RenderCopy(renderer, players[index]->texture, &player_src_rect, &player_dst_rect);
}

int main()
{
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window *window = SDL_CreateWindow("Oliark", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("./font.ttf", 24);
	font_dialogue = TTF_OpenFont("./font.ttf", 48);
	gb_font = TTF_OpenFont("./gb_font.ttf", 16);

#if 0
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

#endif
	world.tile_size = 16;
	world.scale = MAIN_SCALE;


	camera.x = SCREEN_WIDTH / 2;
	camera.y = 100;



	// ------------------------------------------------------------------
	// TODO(martin): init tiles  ----------------------------------------
	// ------------------------------------------------------------------
	uint32_t tilemap_world_a[16][16] = 
	{
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
	};
	uint32_t tilemap1[8][8] = 
	{
		{0,1,1,1,1,1,0,0,},
		{1,1,1,1,1,1,1,0,},
		{1,1,1,1,1,1,1,1,},
		{0,1,1,0,0,1,1,1,},
		{0,1,1,0,0,1,1,1,},
		{1,1,1,1,1,1,1,1,},
		{0,1,1,1,1,1,1,1,},
		{0,0,1,1,0,0,1,1,},
	};
	uint32_t tilemap2[12][6] = 
	{
	};



	// ------------------------------------------------------------------
	// TODO(martin): tilemaps -------------------------------------------
	// ------------------------------------------------------------------
	tile_t tiles_world[16 * 16] = {};
	tilemap_t tilemap_world = {};
	tilemap_world.name = "WORLD MAP";
	tilemap_world.tiles_count_x = 16;
	tilemap_world.tiles_count_y = 16;
	tilemap_world.tiles = tiles_world;
	tiles_init(&tilemap_world, *tilemap_world_a);

	tile_t tiles1[8 * 8] = {};
	tilemap_t tilemap_1 = {};
	tilemap_1.name = "STAGE 1";
	tilemap_1.tiles_count_x = 8;
	tilemap_1.tiles_count_y = 8;
	tilemap_1.tiles = tiles1;
	tiles_init(&tilemap_1, *tilemap1);

	transform_t placement0 = {{1, 2}, {0, 0}};
	tilemap_1.placements[0].transform = &placement0;
	transform_t placement1 = {{2, 2}, {0, 0}};
	tilemap_1.placements[1].transform = &placement1;
	transform_t placement2 = {{1, 3}, {0, 0}};
	tilemap_1.placements[2].transform = &placement2;
	transform_t placement3 = {{2, 3}, {0, 0}};
	tilemap_1.placements[3].transform = &placement3;
	transform_t placement4 = {{1, 4}, {0, 0}};
	tilemap_1.placements[4].transform = &placement4;
	transform_t placement5 = {{2, 4}, {0, 0}};
	tilemap_1.placements[5].transform = &placement5;
	transform_t placement6 = {{1, 5}, {0, 0}};
	tilemap_1.placements[6].transform = &placement6;
	transform_t placement7 = {{2, 5}, {0, 0}};
	tilemap_1.placements[7].transform = &placement7;

	tile_t tiles2[12 * 6] = {};
	tilemap_t tilemap_2 = {};
	tilemap_2.name = "STAGE 2";
	tilemap_2.tiles_count_x = 12;
	tilemap_2.tiles_count_y = 6;
	tilemap_2.tiles = tiles2;
	tiles_init(&tilemap_2, *tilemap2);

	tilemap_t *tilemaps[3] = {};
	tilemaps[0] = &tilemap_world;
	tilemaps[1] = &tilemap_1;
	tilemaps[2] = &tilemap_2;

	tilemap_t *current_tilemap = tilemaps[1];
	


	// ------------------------------------------------------------------
	// TODO(martin): =assets
	// ------------------------------------------------------------------


#if 0
	SDL_Texture *bunny_texture = texture_load(renderer, "./bunny.png");
	SDL_Texture *tile_green_texture = texture_load(renderer, "./assets/8x8/tile_green.png");
	SDL_Texture *slime_grey_texture = texture_load(renderer, "./assets/8x8/slime_grey.png");
	SDL_Texture *slime_blue_texture = texture_load(renderer, "./assets/8x8/slime_blue.png");
	SDL_Texture *slime_red_texture = texture_load(renderer, "./assets/8x8/slime_red.png");
	SDL_Texture *cursor_texture = texture_load(renderer, "./assets/8x8/cursor.png");
	SDL_Texture *moving_area_texture = texture_load(renderer, "./assets/8x8/moving_area.png");
	SDL_Texture *attack_area_texture = texture_load(renderer, "./assets/8x8/attack_area.png");
#else
	SDL_Texture *tile_green_texture = texture_load(renderer, "./assets/16x16/tile_green.png");
	SDL_Texture *slime_grey_texture = texture_load(renderer, "./assets/16x16/slime_grey.png");
	SDL_Texture *slime_grey_shade_texture = texture_load(renderer, "./assets/16x16/slime_grey_shade.png");
	SDL_Texture *slime_blue_texture = texture_load(renderer, "./assets/16x16/slime_blue.png");
	SDL_Texture *slime_red_texture = texture_load(renderer, "./assets/16x16/slime_red.png");
	SDL_Texture *cursor_texture = texture_load(renderer, "./assets/16x16/cursor.png");
	SDL_Texture *moving_area_texture = texture_load(renderer, "./assets/16x16/moving_area.png");
	SDL_Texture *attack_area_texture = texture_load(renderer, "./assets/16x16/attack_area.png");
	SDL_Texture *heart_texture = texture_load(renderer, "./assets/16x16/heart.png");
	SDL_Texture *mana_texture = texture_load(renderer, "./assets/16x16/mana.png");
	SDL_Texture *dante_texture = texture_load(renderer, "./assets/16x16/dante.png");
	SDL_Texture *chibi1_texture = texture_load(renderer, "./assets/16x16/chibi1.png");
	SDL_Texture *wm_texture = texture_load(renderer, "./assets/16x16/wm.png");
	SDL_Texture *penelo_texture = texture_load(renderer, "./assets/16x16/penelo.png");
	SDL_Texture *bo_texture = texture_load(renderer, "./assets/16x16/bo.png");
	SDL_Texture *bob_texture = texture_load(renderer, "./assets/16x16/bob.png");
	SDL_Texture *siluette_texture = texture_load(renderer, "./assets/16x16/siluette.png");
	SDL_Texture *siluette_color_texture = texture_load(renderer, "./assets/16x16/siluette_color.png");
	SDL_Texture *siluette_shade_texture = texture_load(renderer, "./assets/16x16/siluette_shaded.png");
	SDL_Texture *template_texture = texture_load(renderer, "./assets/16x16/template.png");
#endif


	// ------------------------------------------------------------------
	// TODO(martin): ;player init
	// ------------------------------------------------------------------
	uint32_t id_index = 0;
	entity_t player1;
	player1.id = id_index++;
	player1.name = "Slime 1";
	player1.texture = slime_grey_texture;
	player1.tile_pos.x = 2;
	player1.tile_pos.y = 2;
	player1.pos = get_pos_from_tile(player1.tile_pos, &world);
	player1.hp = 21;
	player1.mp = 8;
	player1.att = 255;
	player1.speed = 120;
	player1.move_speed = 0.06f;
	player1.stats_move = 3;
	player1.team = 1;
	player1.dir = animation_dir_down;
	entity_t player2;
	player2.id = id_index++;
	player2.name = "Slime 2";
	player2.texture = slime_blue_texture;
	player2.tile_pos.x = 1;
	player2.tile_pos.y = 3;
	player2.pos = get_pos_from_tile(player2.tile_pos, &world);
	player2.hp = 21;
	player2.mp = 8;
	player2.att = 255;
	player2.speed = 120;
	player2.move_speed = 0.1f;
	player2.stats_move = 4;
	player2.team = 1;
	player2.dir = animation_dir_right;
	entity_t player3;
	player3.id = id_index++;
	player3.name = "Slime 3";
	player3.texture = slime_red_texture;
	player3.tile_pos.x = 2;
	player3.tile_pos.y = 4;
	player3.pos = get_pos_from_tile(player3.tile_pos, &world);
	player3.hp = 21;
	player3.mp = 8;
	player3.att = 255;
	player3.speed = 120;
	player3.move_speed = 0.1f;
	player3.stats_move = 5;
	player3.team = 1;
	player3.dir = animation_dir_right;
	entity_t *players[PLAYERS_NUM] = {};
	players[0] = &player1;
	players[1] = &player2;
	players[2] = &player3;

	// ------------------------------------------------------------------
	// TODO(martin): ;player battle
	// ------------------------------------------------------------------
	int32_t battle_players_index = -1;
	entity_t *battle_players[8] = {};

	battle_players_index++;
	battle_players[battle_players_index] = players[0];
	battle_players_index++;
	battle_players[battle_players_index] = players[1];
	battle_players_index++;
	battle_players[battle_players_index] = players[2];

	uint32_t current_player_index = 0;
	entity_t *current_player = battle_players[current_player_index];



	// ------------------------------------------------------------------
	// TODO(martin): ;enemies all
	// ------------------------------------------------------------------
	entity_t enemy1 = {};
	enemy1.id = 100;
	enemy1.name = "Bunny 1";
	enemy1.texture = penelo_texture;
	enemy1.tile_pos.x = 5;
	enemy1.tile_pos.y = 2;
	enemy1.pos = get_pos_from_tile(enemy1.tile_pos, &world);
	enemy1.hp = 24;
	enemy1.mp = 6;
	enemy1.att = 255;
	enemy1.speed = 100;
	enemy1.move_speed = 0.1f;
	enemy1.stats_move = 6;
	enemy1.team = 2;
	enemy1.dir = animation_dir_down;

	entity_t enemy2 = {};
	enemy2.id = 100;
	enemy2.name = "Neo Slime 1";
	enemy2.texture = penelo_texture;
	enemy2.tile_pos.x = 7;
	enemy2.tile_pos.y = 7;
	enemy2.pos = get_pos_from_tile(enemy2.tile_pos, &world);
	enemy2.hp = 24;
	enemy2.mp = 6;
	enemy2.att = 255;
	enemy2.speed = 100;
	enemy2.move_speed = 0.08f;
	enemy2.stats_move = 8;
	enemy2.team = 2;
	enemy2.dir = animation_dir_right;

	entity_t enemy3 = {};
	enemy3.id = 100;
	enemy3.name = "Neo Slime 1";
	enemy3.texture = penelo_texture;
	enemy3.tile_pos.x = 5;
	enemy3.tile_pos.y = 7;
	enemy3.pos = get_pos_from_tile(enemy3.tile_pos, &world);
	enemy3.hp = 24;
	enemy3.mp = 6;
	enemy3.att = 255;
	enemy3.speed = 100;
	enemy3.move_speed = 0.08f;
	enemy3.stats_move = 8;
	enemy3.team = 2;
	enemy3.dir = animation_dir_left;

	entity_t enemy4 = {};
	enemy4.id = 100;
	enemy4.name = "Neo Slime 1";
	enemy4.texture = penelo_texture;
	enemy4.tile_pos.x = 7;
	enemy4.tile_pos.y = 5;
	enemy4.pos = get_pos_from_tile(enemy4.tile_pos, &world);
	enemy4.hp = 24;
	enemy4.mp = 6;
	enemy4.att = 255;
	enemy4.speed = 100;
	enemy4.move_speed = 0.08f;
	enemy4.stats_move = 8;
	enemy4.team = 2;
	enemy4.dir = animation_dir_up;

	entity_t *enemies[8] = {};
	enemies[0] = &enemy1;
	enemies[1] = &enemy2;
	enemies[2] = &enemy3;
	enemies[3] = &enemy4;




	// ------------------------------------------------------------------
	// TODO(martin): ;cursor
	// ------------------------------------------------------------------
	transform_t transform = {};
	transform.coord.x = 0;
	transform.coord.y = 0;
	transform.pos = get_pos_from_tile(transform.coord, &world);
	new_entity_t cursor = {};
	cursor.texture = cursor_texture;
	cursor.transform = &transform;

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




	camera_following = 1;








	


	// cutscene 1
#if 0
	battle_state = CUTSCENE;
	cutscene.id = 1;
#else
	battle_state = DEBUG_EXPLORE;
	battle_state = PLACEMENT_SELECT_UNIT;
#endif

	//camera_focus_entity(&camera, current_player);

	int32_t placement_select_unit_index = 0;


	// ------------------------------------------------------------------
	// TODO(martin): fps ------------------------------------------------
	// ------------------------------------------------------------------
	uint32_t is_running = 1;
	int fps = 30;
	int32_t millis_per_frame = 1000 / fps;
	int32_t current_millis = 0;

	entity_t *tmp_player = 0;
	while(is_running)
	{
		// ------------------------------------------------------------------
		// TODO(martin): manage fps -----------------------------------------
		// ------------------------------------------------------------------
		float dt = SDL_GetTicks() - current_millis;
		float time_to_wait = millis_per_frame - dt;
		if(time_to_wait < millis_per_frame)
			SDL_Delay(time_to_wait);
		current_millis = SDL_GetTicks();



		// ------------------------------------------------------------------
		// TODO(martin): manage input ---------------------------------------
		// ------------------------------------------------------------------
		input_update(&is_running);

		if(0)
		{
		}

		else if(battle_state == DEBUG_EXPLORE)
		{
			if(input.key_right_pressed)
			{
				current_player->tile_pos.x += 1;
			}
			else if(input.key_left_pressed)
			{
				current_player->tile_pos.x -= 1;
			}
			else if(input.key_up_pressed)
			{
				current_player->tile_pos.y -= 1;
			}
			else if(input.key_down_pressed)
			{
				current_player->tile_pos.y += 1;
			}
			else if(input.key_z_pressed)
			{
				cursor.transform->coord = current_player->tile_pos;
				battle_state = MENU;
				camera_focus_entity(&camera, current_player);
			}
		}

		else if(battle_state == PLACEMENT_SELECT_UNIT)
		{
			if(input.key_right_pressed)
			{
				placement_select_unit_index += 1;
				if(placement_select_unit_index > 3 - 1) placement_select_unit_index = 3 - 1;
			}
			else if(input.key_left_pressed)
			{
				placement_select_unit_index -= 1;
				if(placement_select_unit_index < 0) placement_select_unit_index = 0;
			}
			else if(input.key_x_pressed)
			{
				uint32_t found = 0;
				uint32_t i = 0;
				for(i = 0; i < battle_players_index + 1; i++)
				{
					if(battle_players[i]->id == players[placement_select_unit_index]->id)
					{
						found = 1;
						break;
					}
				}
				if(found)
				{
					battle_players[i] = battle_players[battle_players_index];
					battle_players[battle_players_index] = 0;
					battle_players_index--;
				}
			}
			else if(input.key_z_pressed)
			{
				uint32_t found = 0;
				uint32_t i = 0;
				for(i = 0; i < battle_players_index + 1; i++)
				{
					if(battle_players[i]->id == players[placement_select_unit_index]->id)
					{
						found = 1;
						break;
					}
				}
				if(found)
				{
					battle_players[i] = battle_players[battle_players_index];
					battle_players[battle_players_index] = 0;
					battle_players_index--;
				}
				tmp_player = players[placement_select_unit_index];
				battle_state = PLACEMENT_PLACE_UNIT;
			}
			else if(input.key_space_pressed)
			{
				turn_pass(battle_players, enemies, &current_player, current_tilemap, &cursor);
				//camera_focus_entity(&camera, current_player, &world);
				battle_state = MENU;
			}
		}

		else if(battle_state == PLACEMENT_PLACE_UNIT)
		{
			if(input.key_right_pressed)
			{
				int32_t test_x = tmp_player->tile_pos.x + 1;
				int32_t test_y = tmp_player->tile_pos.y;
				uint32_t found = 0;
				for(int i = 0; i < 8; i++)
				{
					if(current_tilemap->placements[i].transform->coord.x == test_x && current_tilemap->placements[i].transform->coord.y == test_y)
					{
						found = 1;
						break;
					}
				}
				if(found)
				{
					tmp_player->tile_pos.x += 1;
					tmp_player->pos.x = tmp_player->pos.x + (1 * world.tile_size * world.scale);
				}
			}
			else if(input.key_left_pressed)
			{
				int32_t test_x = tmp_player->tile_pos.x - 1;
				int32_t test_y = tmp_player->tile_pos.y;
				uint32_t found = 0;
				for(int i = 0; i < 8; i++)
				{
					if(current_tilemap->placements[i].transform->coord.x == test_x && current_tilemap->placements[i].transform->coord.y == test_y)
					{
						found = 1;
						break;
					}
				}
				if(found)
				{
					tmp_player->tile_pos.x -= 1;
					tmp_player->pos.x = tmp_player->pos.x + (-1 * world.tile_size * world.scale);
				}
			}
			else if(input.key_up_pressed)
			{
				int32_t test_x = tmp_player->tile_pos.x;
				int32_t test_y = tmp_player->tile_pos.y - 1;
				uint32_t found = 0;
				for(int i = 0; i < 8; i++)
				{
					if(current_tilemap->placements[i].transform->coord.x == test_x && current_tilemap->placements[i].transform->coord.y == test_y)
					{
						found = 1;
						break;
					}
				}
				if(found)
				{
					tmp_player->tile_pos.y -= 1;
					tmp_player->pos.y = tmp_player->pos.y + (-1 * world.tile_size * world.scale);
				}
			}
			else if(input.key_down_pressed)
			{
				int32_t test_x = tmp_player->tile_pos.x;
				int32_t test_y = tmp_player->tile_pos.y + 1;
				uint32_t found = 0;
				for(int i = 0; i < 8; i++)
				{
					if(current_tilemap->placements[i].transform->coord.x == test_x && current_tilemap->placements[i].transform->coord.y == test_y)
					{
						found = 1;
						break;
					}
				}
				if(found)
				{
					tmp_player->tile_pos.y += 1;
					tmp_player->pos.y = tmp_player->pos.y + (1 * world.tile_size * world.scale);
				}
			}
			else if(input.key_x_pressed)
			{
				tmp_player = 0;
				battle_state = PLACEMENT_SELECT_UNIT;
			}
			else if(input.key_z_pressed)
			{
				uint32_t found = 0;
				uint32_t i = 0;
				while(battle_players[i])
				{
					if(battle_players[i]->tile_pos.x == tmp_player->tile_pos.x && battle_players[i]->tile_pos.y == tmp_player->tile_pos.y)
					{
						found = 1;
						break;
					}
					i++;
				}
				if(!found)
				{
					battle_players_index++;
					battle_players[battle_players_index] = tmp_player;
					battle_state = PLACEMENT_SELECT_UNIT;
					tmp_player = 0;
				}
			}
		}

		else if(battle_state == EXPLORE)
		{
			if(input.key_right_pressed)
			{
				cursor_move(&cursor, 1, 0, current_tilemap);
			}
			else if(input.key_left_pressed)
			{
				cursor_move(&cursor, -1, 0, current_tilemap);
			}
			else if(input.key_up_pressed)
			{
				cursor_move(&cursor, 0, -1, current_tilemap);
			}
			else if(input.key_down_pressed)
			{
				cursor_move(&cursor, 0, 1, current_tilemap);
			}
			else if(input.key_z_pressed)
			{
				cursor.transform->coord = current_player->tile_pos;
				battle_state = MENU;
			}
		}

		else if(battle_state == MENU)
		{
			if(input.key_up_pressed)
			{
				menu_battle_move_to_prev_available_option();
			}
			else if(input.key_down_pressed)
			{
				menu_battle_move_to_next_available_option();
			}
			else if(input.key_x_pressed)
			{
				cursor.transform->coord = current_player->tile_pos;
				battle_state = EXPLORE;
			}
			else if(input.key_z_pressed)
			{
				if(menu_index == 0 && !turn.has_moved)
				{
					battle_state = MOVE;
				}
				else if(menu_index == 1 && !turn.has_attacked)
				{
					battle_state = ATT;
				}
				else if(menu_index == 2)
				{		
					turn_pass(battle_players, enemies, &current_player, current_tilemap, &cursor);
					camera_focus_entity(&camera, current_player);
					menu_index = menu_option_move;
					battle_state = MENU;
				}
			}
		}

		else if(battle_state == MOVE)
		{
			if(input.key_right_pressed)
			{
				if(move_buffer_index < 0 && !is_moving)
					cursor_move(&cursor, 1, 0, current_tilemap);
			}
			else if(input.key_left_pressed)
			{
				if(move_buffer_index < 0 && !is_moving)
					cursor_move(&cursor, -1, 0, current_tilemap);
			}
			else if(input.key_up_pressed)
			{
				if(move_buffer_index < 0 && !is_moving)
					cursor_move(&cursor, 0, -1, current_tilemap);
			}
			else if(input.key_down_pressed)
			{
				if(move_buffer_index < 0 && !is_moving)
					cursor_move(&cursor, 0, 1, current_tilemap);
			}
			else if(input.key_x_pressed)
			{
				cursor.transform->coord = current_player->tile_pos;
				battle_state = MENU;
			}
			else if(input.key_z_pressed)
			{
				tile_t *start_tile = get_tile(current_tilemap, current_player->tile_pos.x, current_player->tile_pos.y);
				tile_t *end_tile = get_tile(current_tilemap, cursor.transform->coord.x, cursor.transform->coord.y);
				if(!end_tile->obstacle && end_tile->truly_reachable && tiles_distance(end_tile->pos, start_tile->pos) < current_player->stats_move)
				{
					if(move_buffer_index < 0 && !is_moving)
					{
						solve_astar(current_tilemap, start_tile, end_tile);
						generate_moves(end_tile);
						turn.has_moved = 1;
					}
				}
			}
		}

		else if(battle_state == ATT)
		{
			if(input.key_right_pressed)
			{
				cursor_move(&cursor, 1, 0, current_tilemap);
			}
			else if(input.key_left_pressed)
			{
				cursor_move(&cursor, -1, 0, current_tilemap);
			}
			else if(input.key_up_pressed)
			{
				cursor_move(&cursor, 0, -1, current_tilemap);
			}
			else if(input.key_down_pressed)
			{
				cursor_move(&cursor, 0, 1, current_tilemap);
			}
			else if(input.key_x_pressed)
			{
				cursor.transform->coord = current_player->tile_pos;
				battle_state = MENU;
			}
			else if(input.key_z_pressed)
			{
				entity_t *target = get_player_under_cursor(battle_players, &cursor);
				if(target && target->hp > 0) 
				{
					target->hp -= current_player->att;
					if(target->hp <= 0)
					{
						target->dead = 1;
					}
					cursor.transform->coord = current_player->tile_pos;
					menu_options[menu_option_att] = 1;
					menu_index++;
					turn.has_attacked = 1;
					battle_state = MENU;
					if(current_player->hp <= 0)
					{
						battle_state = MENU;
						menu_index = menu_option_move;
					}
				}
			}
		}



		// ------------------------------------------------------------------
		// TODO(martin): camera ---------------------------------------------
		// ------------------------------------------------------------------
		if(battle_state == MOVE)
		{
			camera_focus_entity(&camera, current_player);
		}

		// ------------------------------------------------------------------
		// TODO(martin): move player
		// ------------------------------------------------------------------
		if(move_buffer_index >= 0)
		{
			uint32_t action_completed = move(current_player, move_buffer[move_buffer_index].x, move_buffer[move_buffer_index].y);
			if(action_completed) move_buffer_index--;
			if(move_buffer_index < 0)
			{
				menu_options[menu_option_move] = 1;
				menu_battle_move_to_next_available_option();
				battle_state = MENU;
			}
		}

		// ------------------------------------------------------------------
		// TODO(martin): render section -------------------------------------
		// ------------------------------------------------------------------
		// render background
		SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
		SDL_RenderClear(renderer);

		// ------------------------------------------------------------------
		// TODO(martin): render tilemap -------------------------------------
		// ------------------------------------------------------------------
		for(int i = 0; i < current_tilemap->tiles_count_y * current_tilemap->tiles_count_x; i++)
		{
			int32_t tile_id = current_tilemap->tiles[i].id;
			if(tile_id == 0)
			{
			}
			else 
			{
				SDL_Rect src_rect; 
				SDL_Rect dst_rect; 
				src_rect.x = 0;
				src_rect.y = 0;
				src_rect.w = world.tile_size;
				src_rect.h = world.tile_size;
				vec2f_t pos = get_pos_from_tile(current_tilemap->tiles[i].pos, &world);
				dst_rect.x = pos.x + camera.x;
				dst_rect.y = pos.y + camera.y;
				dst_rect.w = world.tile_size * world.scale;
				dst_rect.h = world.tile_size * world.scale;
				SDL_RenderCopy(renderer, tile_green_texture, &src_rect, &dst_rect);
			}
		}

		// ------------------------------------------------------------------
		// TODO(martin): render moving area ---------------------------------
		// ------------------------------------------------------------------
		if(battle_state == MOVE)
		{
			if(move_buffer_index < 0 && !is_moving && !turn.has_moved)
			{
				for(int i = 0; i < current_tilemap->tiles_count_y * current_tilemap->tiles_count_x; i++)
				{
					if(current_tilemap->tiles[i].truly_reachable && !current_tilemap->tiles[i].obstacle)
					{
						SDL_Rect src_rect; 
						SDL_Rect dst_rect; 
						src_rect.x = 0;
						src_rect.y = 0;
						src_rect.w = world.tile_size;
						src_rect.h = world.tile_size;
						vec2f_t pos = get_pos_from_tile(current_tilemap->tiles[i].pos, &world);
						dst_rect.x = pos.x + camera.x;
						dst_rect.y = pos.y + camera.y;
						dst_rect.w = world.tile_size * world.scale;
						dst_rect.h = world.tile_size * world.scale;
						SDL_RenderCopy(renderer, moving_area_texture, &src_rect, &dst_rect);
					}
				}
			}
			
		}

		// ------------------------------------------------------------------
		// TODO(martin): render attack area
		// ------------------------------------------------------------------
		if(battle_state == ATT)
		{
			if(move_buffer_index < 0 && !is_moving)
			{
				for(int i = 0; i < current_tilemap->tiles_count_y * current_tilemap->tiles_count_x; i++)
				{
					if(tiles_distance(current_tilemap->tiles[i].pos, current_player->tile_pos) < 2) 
					{
						SDL_Rect src_rect = {	0, 
									0, 
									world.tile_size, 
									world.tile_size};
						vec2f_t pos = get_pos_from_tile(current_tilemap->tiles[i].pos, &world);
						SDL_Rect dst_rect = {	pos.x + camera.x, 
									pos.y + camera.y, 
									world.tile_size * world.scale, 
									world.tile_size * world.scale};
						SDL_RenderCopy(renderer, attack_area_texture, &src_rect, &dst_rect);
					}
				}
			}
		}

		// --------------------------------------------------------
		// TODO(martin): ;placement render
		// --------------------------------------------------------
		if(battle_state == PLACEMENT_SELECT_UNIT || battle_state == PLACEMENT_PLACE_UNIT)
		{
			placement_area_render(renderer, current_tilemap, tmp_player, &cursor);
		}
		if(battle_state == PLACEMENT_SELECT_UNIT)
		{
			placement_ui_render(renderer, players, placement_select_unit_index);
		}

		// --------------------------------------------------------
		// TODO(martin): =cursor render 
		// --------------------------------------------------------
		if(battle_state == EXPLORE || battle_state == MENU || battle_state == MOVE || battle_state == ATT)
		{
			cursor_render(renderer, &cursor);
		}

		// --------------------------------------------------------
		// TODO(martin): =player render
		// --------------------------------------------------------
		int i = 0;
		while(battle_players[i])
		{
			player_render(renderer, battle_players[i]);
			i++;
		}
		i = 0;
		while(enemies[i])
		{
			player_render(renderer, enemies[i]);
			i++;
		}

		// --------------------------------------------------------
		// TODO(martin): ;ui render
		// --------------------------------------------------------
		{
#if 0
			{
				SDL_Rect dst_rect;
				dst_rect.x = 32; 
				dst_rect.y = 96; 
				dst_rect.w = world.tile_size * world.scale;
				dst_rect.h = world.tile_size * world.scale;
				SDL_RenderCopy(renderer, heart_texture, NULL, &dst_rect);
				char *dst = "";
				SDL_Color color = {255, 255, 255, 255};
				dst = "";
				dst = concat(dst, "HP: ");
				dst = concat(dst, _itoa(current_player->hp));
				dst_rect.x = 32 + (world.tile_size * world.scale) + 8; 
				dst_rect.y = 96 + (world.tile_size / 2) + 4; 
				dst_rect.w = world.tile_size * world.scale;
				dst_rect.h = world.tile_size * world.scale;
				ui_font_text_render(renderer, gb_font, color, dst, dst_rect.x, dst_rect.y);
			}
			{
				SDL_Rect dst_rect;
				dst_rect.x = 32; 
				dst_rect.y = 144; 
				dst_rect.w = world.tile_size * world.scale;
				dst_rect.h = world.tile_size * world.scale;
				SDL_RenderCopy(renderer, mana_texture, NULL, &dst_rect);
				char *dst = "";
				SDL_Color color = {255, 255, 255, 255};
				dst = "";
				dst = concat(dst, "MP: ");
				dst = concat(dst, _itoa(current_player->mp));
				dst_rect.x = 32 + (world.tile_size * world.scale) + 8; 
				dst_rect.y = 144 + (world.tile_size / 2) + 4; 
				dst_rect.w = world.tile_size * world.scale;
				dst_rect.h = world.tile_size * world.scale;
				ui_font_text_render(renderer, gb_font, color, dst, dst_rect.x, dst_rect.y);
			}
#endif
		}

		if(battle_state == MENU)
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
		// TODO(martin): cutscene 
		// --------------------------------------------------------
		if(cutscene.id == 1)
		{
			cutscene_1_play(renderer, &world, &input, battle_players, enemies, &camera);
			if(cutscene.completed)
			{
				cursor.transform->coord = current_player->tile_pos;
				battle_state = DEBUG_EXPLORE;
			}
		}
		else if(cutscene.id == 2)
		{
			if(cutscene_2_play(renderer, &input, battle_players))
			{
				cutscene.id = 3;
			}
		}
		else if(cutscene.id == 3)
		{
			if(cutscene_3_play(renderer, &input, battle_players))
			{
			}
		}



		// --------------------------------------------------------
		// TODO(martin): render postprocess 
		// --------------------------------------------------------
		
		// fade
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, (fade.val));
		SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
		SDL_RenderFillRect(renderer, &rect);




		// --------------------------------------------------------
		// DEBUG ONLY!!
		// --------------------------------------------------------
		char *dst = "";
		SDL_Color color = {255, 255, 255, 255};
		dst = "";
		dst = concat(dst, "DT: ");
		dst = concat(dst, _itoa(dt));
#if 0
		ui_font_text_render(renderer, font, color, dst, 16, 80);
#else
		ui_font_text_render(renderer, font, color, dst, 16, 16);
#endif

#if 0
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
		dst = concat(dst, "NAME: ");
		dst = concat(dst, current_player->name);
		dst = concat(dst, " - HP: ");
		dst = concat(dst, _itoa(current_player->hp));
		dst = concat(dst, " - ATT: ");
		dst = concat(dst, _itoa(current_player->att));
		ui_font_text_render(renderer, font, color, dst, 16, 144);

		entity_t *target = 0;
		target = get_player_under_cursor(battle_players, &cursor);

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
#endif



		SDL_RenderPresent(renderer);
	}

	SDL_Quit();

	return 0;
}

