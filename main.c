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

typedef struct world_t
{
	float tile_size;
	float scale;
	vec2f_t offset;
} world_t;


#define move_buffer_num 8
int32_t move_buffer_index;
vec2f_t move_buffer[move_buffer_num];

int32_t check_valid_move(uint32_t test_tile_x, uint32_t test_tile_y, uint32_t player_tile_x, uint32_t player_tile_y)
{
	int32_t res = 0;

	if(abs(test_tile_x - player_tile_x) + abs(test_tile_y - player_tile_y) < 3)
	{
		res = 1;
	}
	
	return res;
	
}

void move_player_to_cursor(uint32_t cursor_tile_x, uint32_t cursor_tile_y, uint32_t *player_x, uint32_t *player_y, float tile_size)
{
	if(check_valid_move(cursor_tile_x, cursor_tile_y, *player_x, *player_y))
	{
		*player_x = cursor_tile_x;
		*player_y = cursor_tile_y;
	}
}

int32_t get_tile_value(tilemap_t *tilemap, uint32_t x, uint32_t y)
{
	return tilemap->tiles[y * TILE_MAP_COUNT_X  + x];
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

typedef struct node_t
{
	int id;
	int x;
	int y;
	int visited;
	int obstacle;
	int global;
	int local;
	int neighbors_num;
	struct node_t *neighbors[4];
	struct node_t *parent;
} node_t;

float distance(node_t *a, node_t *b)
{
	return sqrt((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y));
}

void solve_astar(node_t *nodes, node_t *node_start, node_t *node_end)
{
	for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
	{
		for(int x = 0; x < TILE_MAP_COUNT_X; x++)
		{
			int i = y * TILE_MAP_COUNT_X + x;

			nodes[i].parent = 0;
			nodes[i].visited = 0;
			nodes[i].global = 9999;
			nodes[i].local = 9999;
		}
	}

	node_t *node_current = node_start;
	node_start->local = 0.0f;
	node_start->global = distance(node_start, node_end);

	int index = -1;
#define BUFFER_SIZE 256
	node_t *nodes_to_test[BUFFER_SIZE];

	nodes_to_test[++index] = node_start;

	while(index >= 0 && node_current != node_end)
	{
		if(nodes_to_test[index]->visited == 1)
			index--;
		
		if(index < 0)
			break;
		
		node_current = nodes_to_test[index];
		node_current->visited = 1;

		int i = 0;
		while(i < node_current->neighbors_num)
		{
			node_t *node_neighbor = node_current->neighbors[i];

			if(!node_neighbor->visited && node_neighbor->obstacle == 0)
				nodes_to_test[++index] = node_neighbor;
			
			float possibly_lower_goal = node_current->local + distance(node_current, node_neighbor);

			if(possibly_lower_goal < node_neighbor->local)
			{
				node_neighbor->parent = node_current;
				node_neighbor->local = possibly_lower_goal;
				node_neighbor->global = node_neighbor->local + distance(node_neighbor, node_end);
			}

			i++;
		}
	}

	int parents_buffer_index = -1;
	vec2f_t parents_buffer[16];
	if(node_end != 0)
	{
		node_t *p = node_end;
		while(p != 0)
		{
			vec2f_t pos = {p->x, p->y};
			parents_buffer[++parents_buffer_index] = pos;
			p = p->parent;
		}
	}

	
	vec2f_t pos_prev = {parents_buffer[parents_buffer_index].x, parents_buffer[parents_buffer_index].y};
	while(parents_buffer_index >= 0)
	{
		vec2f_t pos_current = {parents_buffer[parents_buffer_index].x, parents_buffer[parents_buffer_index].y};
		parents_buffer_index--;

		vec2f_t move = {pos_current.x - pos_prev.x, pos_current.y - pos_prev.y};
		pos_prev = pos_current;

		if(move.x != 0 || move.y != 0)
			move_buffer[++move_buffer_index] = move;
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
	memory.permanent_storage = mmap(0, memory.permanent_storage_size , PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	memory.permanent_storage = calloc(memory.permanent_storage_size, sizeof(char));

	if(!memory.permanent_storage)
	{
		return 1;
	}

	game_state_t *game_state = (game_state_t*)memory.permanent_storage;

	memory_arena_init(&game_state->memory_arena, memory.permanent_storage_size - sizeof(game_state), (uint32_t*)memory.permanent_storage + sizeof(game_state));
	
#if 0
	game_state->test = (test_t*)push_struct(&game_state->memory_arena, sizeof(test_t));
	game_state->test->val = 3;

	game_state->player_pos = (player_pos_t*)push_struct(&game_state->memory_arena, sizeof(player_pos_t));
	game_state->player_pos->x = 2;
	game_state->player_pos->y = 2;
#endif

	game_state->tiles = (uint32_t*)push_struct(&game_state->memory_arena, sizeof(uint32_t) * TILE_MAP_COUNT_X  * TILE_MAP_COUNT_Y);

	uint32_t tilemap1[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		{1,1,1,1,1,1,1,1,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,0,0,0,0,0,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,0,0,0,0,0,0,1,},
		{1,1,1,1,1,1,1,1,},
	};

	for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
	{
		for(int x = 0; x < TILE_MAP_COUNT_X; x++)
		{
			game_state->tiles[y * TILE_MAP_COUNT_X + x] = tilemap1[y][x]; 
		}
	}

	world_t world;
	world.tile_size = 8;
	world.scale = 8;
	world.offset.x = 100;
	world.offset.y = 30;


	node_t nodes[TILE_MAP_COUNT_Y * TILE_MAP_COUNT_X] = {};

	// init astar
	for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
	{
		for(int x = 0; x < TILE_MAP_COUNT_X; x++)
		{
			int i = y * TILE_MAP_COUNT_X + x;

			nodes[i].x = x;
			nodes[i].y = y;

			if(y > 0)
			{
				nodes[i].neighbors[nodes[i].neighbors_num] = &nodes[(y - 1) * TILE_MAP_COUNT_X + (x + 0)];
				nodes[i].neighbors_num += 1;
			}
			if(y < TILE_MAP_COUNT_Y - 1)
			{
				nodes[i].neighbors[nodes[i].neighbors_num] = &nodes[(y + 1) * TILE_MAP_COUNT_X + (x + 0)];
				nodes[i].neighbors_num += 1;
			}
			if(x > 0)
			{
				nodes[i].neighbors[nodes[i].neighbors_num] = &nodes[(y + 0) * TILE_MAP_COUNT_X + (x - 1)];
				nodes[i].neighbors_num += 1;
			}
			if(x < TILE_MAP_COUNT_X - 1)
			{
				nodes[i].neighbors[nodes[i].neighbors_num] = &nodes[(y + 0) * TILE_MAP_COUNT_X + (x + 1)];
				nodes[i].neighbors_num += 1;
			}
			//printf("(%d %d) - ", nodes[y * TILE_MAP_COUNT_X + x].y, nodes[y * TILE_MAP_COUNT_X + x].x);
			//printf("%p %p %p %p\n", nodes[y * TILE_MAP_COUNT_X + x].neighbors[0], nodes[y * TILE_MAP_COUNT_X + x].neighbors[1], nodes[y * TILE_MAP_COUNT_X + x].neighbors[2], nodes[y * TILE_MAP_COUNT_X + x].neighbors[3]);
		}
	}



	vec2f_t player_tile = {2.0f, 2.0f};
	vec2f_t player_pos = {player_tile.x * world.tile_size * world.scale + world.offset.x,
				player_tile.y * world.tile_size * world.scale + world.offset.y};

	vec2f_t cursor_tile = {2.0f, 2.0f};

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
							{
								solve_astar(nodes, &nodes[(int)(player_tile.y * TILE_MAP_COUNT_X + player_tile.x)], &nodes[(int)(cursor_tile.y * TILE_MAP_COUNT_X + cursor_tile.x)]);
							}
							//printf("(%f %f)\n", player_tile.x, player_tile.y);
							//printf("(%f %f)\n", cursor_tile.x, cursor_tile.y);
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
		
		// move player to cursor
		if(!is_moving)
			if(move_buffer_index >= 0)
			{
				start_moving = 1;
			}
		if(start_moving)
		{
			start_moving = 0;

			player_tile.x += move_buffer[move_buffer_index].x;
			player_tile.y += move_buffer[move_buffer_index].y;

			start_x = player_pos.x;
			start_y = player_pos.y;
			end_x = player_pos.x + (move_buffer[move_buffer_index].x * world.tile_size * world.scale);
			end_y = player_pos.y + (move_buffer[move_buffer_index].y * world.tile_size * world.scale);
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
			player_pos.x = lerp(start_x, end_x, t);
			player_pos.y = lerp(start_y, end_y, t);
		}

		

		// render background
		SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
		SDL_RenderClear(renderer);

		// render tilemap
		for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
		{
			for(int x = 0; x < TILE_MAP_COUNT_X; x++)
			{
				int32_t tile_id = game_state->tiles[y * TILE_MAP_COUNT_X + x];
				if(tile_id == 0)
				{
					SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
				}
				if(tile_id == 1)
				{
					SDL_SetRenderDrawColor(renderer, 48, 48, 48, 255);
				}
				SDL_Rect tile_rect = {x * world.tile_size * world.scale + world.offset.x, y * world.scale * world.tile_size + world.offset.y, world.tile_size * world.scale, world.tile_size * world.scale};
				SDL_RenderFillRect(renderer, &tile_rect);
			}
		}

		// render moving area
#if 0
		for(int y = 0; y < TILE_MAP_COUNT_Y; y++)
		{
			for(int x = 0; x < TILE_MAP_COUNT_X; x++)
			{
				if(check_valid_move(x, y, player_x, player_y))
				{
					SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
					SDL_Rect area_rect = {x * world.tile_size * world.scale + world.offset.x, y * world.scale * world.tile_size + world.offset.y, world.tile_size * world.scale, world.tile_size * world.scale};
					SDL_RenderFillRect(renderer, &area_rect);
				}
			}
		}
#endif

		// render cursor
		SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
		SDL_Rect cursor_rect = {cursor_tile.x * world.tile_size * world.scale + world.offset.x, cursor_tile.y * world.scale * world.tile_size + world.offset.y, world.tile_size * world.scale, world.tile_size * world.scale};
		SDL_RenderFillRect(renderer, &cursor_rect);

		/// render player bg debug
		SDL_SetRenderDrawColor(renderer, 255, 128, 0, 255);
		SDL_Rect player_bg_rect = {player_tile.x * world.tile_size * world.scale + world.offset.x, player_tile.y * world.scale * world.tile_size + world.offset.y, world.tile_size * world.scale, world.tile_size * world.scale};
		SDL_RenderFillRect(renderer, &player_bg_rect);

		// render player
		SDL_Rect player_src_rect = {0, 0, 8, 8};
		SDL_Rect player_dst_rect = {player_pos.x, player_pos.y, world.tile_size * world.scale, world.tile_size * world.scale};
		SDL_RenderCopy(renderer, player_texture, &player_src_rect, &player_dst_rect);




		// DEBUG: render debug info on ui
		char *dst = "";

		dst = concat(dst, "(y: ");
		dst = concat(dst, _itoa(player_tile.y));
		dst = concat(dst, "  x: ");
		dst = concat(dst, _itoa(player_tile.x));
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

