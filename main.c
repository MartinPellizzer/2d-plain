#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <math.h>
#include <sys/mman.h>

#include "main.h"
#include "util.h"

#define TILE_MAP_COUNT_X 8 
#define TILE_MAP_COUNT_Y 8

#define move_buffer_num 16
int32_t move_buffer_index;
vec2f_t move_buffer[move_buffer_num];

#define STATS_MOVE 4

uint32_t tiles_distance(vec2i_t tile1_pos, vec2i_t tile2_pos)
{
	return (abs(tile1_pos.x - tile2_pos.x) + abs(tile1_pos.y - tile2_pos.y) < STATS_MOVE);
}

void ui_font_text_render(SDL_Renderer *renderer, TTF_Font *font, char *text, int32_t x, int32_t y)
{
	SDL_Color color = {255, 255, 255, 255};
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

tile_t* get_tile(tile_t tiles[], uint32_t x, uint32_t y)
{
	return &tiles[y * TILE_MAP_COUNT_X  + x];
}

void player_update_animation(player_t *player)
{
	player->animation_counter++;
	if(player->animation_counter >= 10)
	{
		player->animation_counter = 0;
		player->animation_frame++;
		player->animation_frame = player->animation_frame % 2;
	}
}
int main()
{
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window *window = SDL_CreateWindow("Oliark", 0, 0, 800, 600, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("./font.ttf", 24);

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

	world_t world;
	world.tile_size = 8;
	world.scale = 8;
	world.offset.x = 100;
	world.offset.y = 30;

	uint32_t tilemap1[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		{1,1,1,1,1,1,1,1,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,1,1,0,0,0,},
		{1,0,0,1,0,1,0,1,},
		{1,0,0,0,1,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,1,1,1,1,1,1,1,},
	};

	tile_t tiles[TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X] = {};

	// init astar
	for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
	{
		for(int x = 0; x < TILE_MAP_COUNT_X; x++)
		{
			tile_t *tile = get_tile(tiles, x, y);

			tile->id = tilemap1[y][x]; 
			tile->obstacle = tilemap1[y][x]; 
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

	player_t player1 = {};
	player1.tile_pos.x = 3;
	player1.tile_pos.y = 2;
	player1.pos.x = player1.tile_pos.x * world.tile_size * world.scale + world.offset.x;
	player1.pos.y = player1.tile_pos.y * world.tile_size * world.scale + world.offset.y;
	player1.animation_frame = 0;
	player1.animation_counter = 0;

	player_t player2 = {};
	player2.tile_pos.x = 5;
	player2.tile_pos.y = 2;
	player2.pos.x = player2.tile_pos.x * world.tile_size * world.scale + world.offset.x;
	player2.pos.y = player2.tile_pos.y * world.tile_size * world.scale + world.offset.y;
	player2.animation_frame = 0;
	player2.animation_counter = 0;

	player_t player3 = {};
	player3.tile_pos.x = 4;
	player3.tile_pos.y = 6;
	player3.pos.x = player3.tile_pos.x * world.tile_size * world.scale + world.offset.x;
	player3.pos.y = player3.tile_pos.y * world.tile_size * world.scale + world.offset.y;
	player3.animation_frame = 0;
	player3.animation_counter = 0;

#define PLAYERS_NUM 3
	player_t *players[PLAYERS_NUM] = {};
	players[0] = &player1;
	players[1] = &player2;
	players[2] = &player3;

	uint32_t player_current_index = 0;
	player_t *player_current = players[player_current_index];
	


	vec2i_t cursor_tile = {player_current->tile_pos.x, player_current->tile_pos.y};

	SDL_Surface *surface_tmp = IMG_Load("./slime.png");
	SDL_Texture *player_texture = SDL_CreateTextureFromSurface(renderer, surface_tmp);
	SDL_FreeSurface(surface_tmp);

	int is_running = 1;
	int fps = 30;
	int32_t millis_per_frame = 1000 / fps;
	int32_t current_millis = 0;

	uint32_t start_moving = 0;
	uint32_t is_moving = 0;

	float start_x = 0.0f;
	float start_y = 0.0f;
	float end_x = 0.0f;
	float end_y = 0.0f;
	float t = 0.0f;

	float speed = 0.08f;

	move_buffer_index = -1;








	uint32_t change_player = 1;
	uint32_t player_has_moved = 0;

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
							if(move_buffer_index < 0 && !is_moving)
								test_tile_x = -1;
						} break;

						case SDLK_RIGHT:
						{
							if(move_buffer_index < 0 && !is_moving)
								test_tile_x = +1;
						} break;

						case SDLK_UP:
						{
							if(move_buffer_index < 0 && !is_moving)
								test_tile_y = -1;
						} break;

						case SDLK_DOWN:
						{
							if(move_buffer_index < 0 && !is_moving)
								test_tile_y = +1;
						} break;

						case SDLK_SPACE:
						{
							if(move_buffer_index < 0 && !is_moving)
								change_player = 1;
						} break;

						case SDLK_z:
						{
							if(!player_has_moved)
							{
								tile_t *start_tile = get_tile(tiles, player_current->tile_pos.x, player_current->tile_pos.y);
								tile_t *end_tile = get_tile(tiles, cursor_tile.x, cursor_tile.y);

								if(!end_tile->obstacle && end_tile->truly_reachable && tiles_distance(end_tile->pos, start_tile->pos))
								{
									if(move_buffer_index < 0 && !is_moving)
									{
										// update possible movements
										solve_astar(tiles, start_tile, end_tile);
										generate_moves(end_tile);

										player_has_moved = 1;
									}
								}
							}
						} break;
					}
				} break;
			}
		}



		// move cursor inside map
		if(cursor_tile.x + test_tile_x >= 0 && cursor_tile.x + test_tile_x < TILE_MAP_COUNT_X)
			cursor_tile.x += test_tile_x;
		if(cursor_tile.y + test_tile_y >= 0 && cursor_tile.y + test_tile_y < TILE_MAP_COUNT_Y)
			cursor_tile.y += test_tile_y;
		
		// pass turn
		if(change_player)
		{
			// change player
			change_player = 0;
			player_current_index++;			
			player_current_index %= PLAYERS_NUM;
			player_current = players[player_current_index];
			cursor_tile.x = player_current->tile_pos.x;
			cursor_tile.y = player_current->tile_pos.y;
			
			player_has_moved = 0;

			// compute moving area
			for(int i = 0; i < TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X; i++)
			{
				tiles[i].truly_reachable = 0;

				vec2i_t tile_to_test_pos = {tiles[i].pos.x, tiles[i].pos.y};

				if(tiles_distance(tile_to_test_pos, player_current->tile_pos) && !tiles[i].obstacle) 
					tiles[i].reachable = 1;
				else
					tiles[i].reachable = 0;
			}

			for(int i = 0; i < PLAYERS_NUM; i++)
			{
				if(players[i] != player_current)
					get_tile(tiles, players[i]->tile_pos.x, players[i]->tile_pos.y)->reachable = 0;
			}
											
			for(int i = 0; i < TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X; i++)
			{
				if(tiles[i].reachable == 1)
				{
					tile_t *tile_start = get_tile(tiles, player_current->tile_pos.x, player_current->tile_pos.y);
					tile_t *tile_end = &tiles[i];
					solve_astar(tiles,  tile_start, tile_end);
					uint32_t moves_num = generate_moves_tmp(tile_end);

					if(moves_num <= 4 && moves_num != 1)
						tiles[i].truly_reachable = 1;
				}
			}
		}

		// move player to cursor
		if(!is_moving)
		{
			if(move_buffer_index >= 0)
			{
				start_moving = 1;
			}
		}
		if(start_moving)
		{
			start_moving = 0;

			player_current->tile_pos.x += move_buffer[move_buffer_index].x;
			player_current->tile_pos.y += move_buffer[move_buffer_index].y;

			start_x = player_current->pos.x;
			start_y = player_current->pos.y;
			end_x = player_current->pos.x + (move_buffer[move_buffer_index].x * world.tile_size * world.scale);
			end_y = player_current->pos.y + (move_buffer[move_buffer_index].y * world.tile_size * world.scale);
			t = 0.0f;

			move_buffer_index--;

			is_moving = 1;
		}
		if(is_moving)
		{
			t += speed;
			if(t >= 1.0f)
			{
				t = 1.0f;
				is_moving = 0;
			}
			player_current->pos.x = lerp(start_x, end_x, t);
			player_current->pos.y = lerp(start_y, end_y, t);
		}

		


		// render background
		SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
		SDL_RenderClear(renderer);

		// render tilemap + moving area
		for(int i = 0; i < TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X; i++)
		{
			// tilemap
			int32_t tile_id = tiles[i].id;

			if(tile_id == 0)
				SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
			else if(tile_id == 1)
				SDL_SetRenderDrawColor(renderer, 48, 48, 48, 255);

			SDL_Rect tile_rect = {	tiles[i].pos.x * world.tile_size * world.scale + world.offset.x, 
						tiles[i].pos.y * world.scale * world.tile_size + world.offset.y, 
						world.tile_size * world.scale, 
						world.tile_size * world.scale};
			SDL_RenderFillRect(renderer, &tile_rect);

			// moving area
			if(move_buffer_index < 0 && !is_moving && !player_has_moved)
			{
				if(tiles[i].truly_reachable && !tiles[i].obstacle)
				{
					SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
					SDL_Rect area_rect = {	tiles[i].pos.x * world.tile_size * world.scale + world.offset.x, 
								tiles[i].pos.y * world.scale * world.tile_size + world.offset.y, 
								world.tile_size * world.scale, 
								world.tile_size * world.scale};
					SDL_RenderFillRect(renderer, &area_rect);
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
		SDL_Rect cursor_rect = {cursor_tile.x * world.tile_size * world.scale + world.offset.x, 
					cursor_tile.y * world.scale * world.tile_size + world.offset.y, 
					world.tile_size * world.scale, 
					world.tile_size * world.scale};
		SDL_RenderFillRect(renderer, &cursor_rect);

		// render players
		for(int i = 0; i < PLAYERS_NUM; i++)
		{
			player_update_animation(players[i]);
			SDL_Rect src_rect = {	players[i]->animation_frame * world.tile_size, 
						0, 
						world.tile_size, 
						world.tile_size};
			SDL_Rect dst_rect = {	players[i]->pos.x, 
						players[i]->pos.y, 
						world.tile_size * world.scale, 
						world.tile_size * world.scale};
			SDL_RenderCopy(renderer, player_texture, &src_rect, &dst_rect);
		}










		// DEBUG: render debug info on ui
		char *dst = "";

		dst = concat(dst, "(y: ");
		dst = concat(dst, _itoa(cursor_tile.y));
		dst = concat(dst, "  x: ");
		dst = concat(dst, _itoa(cursor_tile.x));
		dst = concat(dst, ")");
		ui_font_text_render(renderer, font, dst, 16, 16);

		dst = "";
		dst = concat(dst, "FPS: ");
		dst = concat(dst, _itoa(fps));
		dst = concat(dst, " - MILLIS: ");
		dst = concat(dst, _itoa(millis_per_frame));
		ui_font_text_render(renderer, font, dst, 16, 48);

		dst = "";
		dst = concat(dst, "DT: ");
		dst = concat(dst, _itoa(dt));
		ui_font_text_render(renderer, font, dst, 16, 80);

		SDL_RenderPresent(renderer);
	}

	SDL_Quit();

	return 0;
}

