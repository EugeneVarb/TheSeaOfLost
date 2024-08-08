float sin_breathe (float time, float rate) {
	return (sin(time * rate) + 1.0) / 2.0;
}

bool almost_equals(float a, float b, float epsilon) {
 return fabs(a - b) <= epsilon;
}

bool animate_f32_to_target(float* value, float target, float delta_t, float rate) {
	*value += (target - *value) * (1.0 - pow(2.0f, -rate * delta_t));
	if (almost_equals(*value, target, 0.001f))
	{
		*value = target;
		return true; // reached
	}
	return false;
}

void animate_v2_to_target(Vector2* value, Vector2 target, float delta_t, float rate) {
	animate_f32_to_target(&(value->x), target.x, delta_t, rate);
	animate_f32_to_target(&(value->y), target.y, delta_t, rate);
}


// ^^^ generic utils

const int tile_width = 8;
const float entity_selection_radius = 16.0f;
const float player_pickup_radius = 6.0f;
const int reef_health = 3;
const int loot_health = 1;


int world_pos_to_tile_pos(float world_pos) {
	return roundf(world_pos / (float)tile_width);
}
float tile_pos_to_world_pos(int tile_pos) {
	return ((float)tile_pos * (float)tile_width);
}
Vector2 round_v2_to_tile(Vector2 world_pos) {
	world_pos.x = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.x));
	world_pos.y = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.y));
	return world_pos;
}


typedef struct Sprite {
	Gfx_Image* image;
} Sprite;


typedef enum SpriteID {
	SPRITE_nil,
	SPRITE_player,
	SPRITE_reef0,
	SPRITE_reef1,
	SPRITE_loot0,
	SPRITE_item_floatwood,
	SPRITE_item_seaweed,

	SPRITE_MAX,
} SpriteID;
Sprite sprites[SPRITE_MAX];
Sprite* get_sprite(SpriteID id) { 
	if (id >= 0 && id < SPRITE_MAX) { //used to be id <= 0 
		return &sprites[id];
	}
	return &sprites[0];
}

Vector2 get_sprite_size(Sprite* sprite) {
	return (Vector2) { sprite->image->width, sprite->image->height };
}

// typedef struct Item {
// } Item;
// typedef enum ItemID {
// 	ITEM_nil,
// } ItemID;



typedef enum EntityArchetype {
	arch_nil = 0,
	arch_reef = 1,
	arch_loot = 2,
	arch_player = 3,
	
	arch_item_floatwood = 4,
	arch_item_seaweed = 5,
	ARCH_MAX,
} EntityArchetype;

typedef struct Entity {
bool is_valid;
EntityArchetype arch;
Vector2 pos;
bool render_sprite;
SpriteID sprite_id;	
int health;
bool destroyable_world_item;
bool is_item;
} Entity;

// :entity
#define MAX_ENTITY_COUNT 1024

typedef struct ItemData {
	int amount;
} ItemData;

// :world
typedef struct World {
	Entity entities [MAX_ENTITY_COUNT];
	ItemData inventory_items [ARCH_MAX];
} World;
World* world = 0;

typedef struct WorldFrame {
	Entity* selected_entity;
} WorldFrame;
WorldFrame world_frame;

Entity* entity_create() {
	Entity* entity_found = 0;
	for (int i = 0; i < MAX_ENTITY_COUNT; i++){
		Entity* existing_entity = &world->entities[i];
		if (!existing_entity->is_valid) {
			entity_found = existing_entity;
			break;
		}
	}
	assert(entity_found, "No more entities!");
	entity_found->is_valid = true;
	return entity_found;
}


void entity_destroy(Entity* entity) {
	memset(entity, 0, sizeof(Entity));
} //Clear the entire thing to 0

void setup_player (Entity* en) {
	en->arch = arch_player;
	en->sprite_id = SPRITE_player;
}
void setup_reef (Entity* en) {
	en->arch = arch_reef;
	en->sprite_id = SPRITE_reef0;
	en->health = reef_health;
	en->destroyable_world_item = true;
	//en->sprite_id = SPRITE_reef1;
}
void setup_loot (Entity* en) {
	en->arch = arch_loot;
	en->sprite_id = SPRITE_loot0;
	en->health = loot_health;
	en->destroyable_world_item = true;
}

void setup_item_floatwood (Entity* en) {
    en->arch = arch_item_floatwood;
    en->sprite_id = SPRITE_item_floatwood;
	en->destroyable_world_item = false;
	en->is_item = true;
}

Vector2 screen_to_world() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.view;
	float window_w = window.width;
	float window_h = window.height;

	//Normalize the mouse coordinates
	float ndc_x = (mouse_x / (window_w * 0.5f)) - 1.0f;
	float ndc_y = (mouse_y / (window_h * 0.5f)) - 1.0f;

	//Transform to world coordinates
	Vector4 world_pos = v4(ndc_x, ndc_y, 0, 1);
	world_pos = m4_transform(m4_inverse(proj), world_pos);
	world_pos = m4_transform(view, world_pos);
	// log("%f %f", world_pos.x, world_pos.y);

	//Return as 2d vector
	return (Vector2){ world_pos.x, world_pos.y };
}


int entry(int argc, char **argv) {
	
	window.title = STR("LOST SEAS");
	window.width = 1280;
	window.height = 720;
	window.x = 200;
	window.y = 200;
	window.clear_color = hex_to_rgba(0x2a2d3aff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	sprites[SPRITE_player] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/player.png"), get_heap_allocator())};
	sprites[SPRITE_reef0] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/reef0.png"), get_heap_allocator())};
	sprites[SPRITE_reef1] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/reef1.png"), get_heap_allocator())};
	sprites[SPRITE_loot0] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/loot0.png"), get_heap_allocator())};
	sprites[SPRITE_item_floatwood] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/item_floatwood.png"), get_heap_allocator())};
	sprites[SPRITE_item_seaweed] = (Sprite) { .image=load_image_from_disk(STR("res/sprites/item_seaweed.png"), get_heap_allocator())};
	
	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf");
	const u32 font_height = 48;
	
	// :init

	// test item adding
	{
		world->inventory_items[arch_item_floatwood].amount = 5;
		
	
	}


	Entity* player_en = entity_create();
	setup_player(player_en);

		
	for (int i = 0; i < 10; i++) {
		Entity* en = entity_create();
		setup_reef(en);
		en->pos = v2(get_random_float32_in_range(-100, 100), get_random_float32_in_range(-100, 100));
		en->pos = round_v2_to_tile(en->pos);
		// en->pos.y -= tile_width * 0.5;
	}
	
	for (int i = 0; i < 10; i++) {
		Entity* en = entity_create();
		setup_loot(en);
		en->pos = v2(get_random_float32_in_range(-100, 100), get_random_float32_in_range(-100, 100));
		en->pos = round_v2_to_tile(en->pos);
		//en->pos.y -= tile_width * 0.5;
	}


	float64 seconds_counter = 0.0;
	s32 frame_count = 0;
	
	float zoom = 5.3;
	Vector2 camera_pos = v2(0, 0);	

	float64 last_time = os_get_current_time_in_seconds();
	while (!window.should_close) {
		reset_temporary_storage();
		world_frame = (WorldFrame){0};

		float64 now = os_get_current_time_in_seconds();
		float64 delta_t = now - last_time;
		last_time = now;

		os_update();
		
		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);		
		

		// CAMERA
		{
		Vector2 target_pos = player_en->pos;
		animate_v2_to_target(&camera_pos, target_pos, delta_t, 30.0f); //&camera_pos, in the video
		//camera_pos = player_en->pos;
		draw_frame.view = m4_make_scale(v3(1.0, 1.0, 1.0));
		draw_frame.view = m4_mul(draw_frame.view, m4_make_translation(v3(camera_pos.x, camera_pos.y, 1.0)));
		draw_frame.view = m4_mul(draw_frame.view, m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0)));
		}			
		
			Vector2 mouse_pos_world = screen_to_world();
			int mouse_tile_x = world_pos_to_tile_pos(mouse_pos_world.x);
			int mouse_tile_y = world_pos_to_tile_pos(mouse_pos_world.y);
		
		//mouse pos in world space test
		Entity* selected_entity;
		{
			float smallest_dist = INFINITY;
			// log("%f, %f", pos.x, pos.y);
			//draw_text(font, sprint(temp_allocator, STR("%f %f"), pos.x, pos.y), font_height, mouse_pos, v2(0.1, 0.1), COLOR_RED);
			
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid&& en->destroyable_world_item) {
					Sprite* sprite = get_sprite(en->sprite_id);

					int entity_tile_x = world_pos_to_tile_pos(en->pos.x);
					int entity_tile_y = world_pos_to_tile_pos(en->pos.y);

					float dist = fabsf(v2_dist(en->pos, mouse_pos_world));
					if (dist < entity_selection_radius) {
						if (!world_frame.selected_entity || (dist < smallest_dist)) {
							world_frame.selected_entity = en;
							smallest_dist = dist;
						}
						//draw_rect(v2(tile_pos_to_world_pos(entity_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(entity_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
					};
					
					/*{Range2f bounds = range2f_make_bottom_center(sprite->size);
					bounds = range2f_shift(bounds, en->pos);

					bounds.min = v2_sub(bounds.min, v2(10.0, 10.0));
					bounds.max = v2_add(bounds.max, v2(10.0, 10.0));

					Vector4 col = COLOR_WHITE;
					col.a = 0.4;
					if (range2f_contains(bounds, mouse_pos_world)) {
						col.a = 1.0;
					}

					draw_rect(bounds.min, range2f_size(bounds), col); }*/
				}
			}
		}

		// TILE RENDERING (DRAWING THE GRID)
		{
			int player_tile_x = world_pos_to_tile_pos(player_en->pos.x);
			int player_tile_y = world_pos_to_tile_pos(player_en->pos.y);
			int tile_radius_x = 40;
			int tile_radius_y = 30;
			for (int x = player_tile_x - tile_radius_x; x < player_tile_x + tile_radius_x; x++) {
				for (int y = player_tile_y - tile_radius_y; y < player_tile_y + tile_radius_y; y++) {
					if ((x + (y % 2 == 0) ) % 2 == 0) {
						Vector4 col = v4(0.1, 0.1, 0.1, 0.1);
						float x_pos = x * tile_width;
						float y_pos = y * tile_width;
						draw_rect(v2(x_pos + tile_width * -0.5, y_pos + tile_width * -0.5), v2(tile_width, tile_width), col);
					}
				}
			}

			// highlighting tiles over the mouse cursor
			// draw_rect(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
		}

		// :update entities
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				if (en->is_valid) {

					// :pick up items
						if (en->is_item) {
							// TODO epic pick up effects for later
							if (fabsf(v2_dist(en->pos, player_en->pos)) < player_pickup_radius) {
								//pickup
								world->inventory_items[en->arch].amount += 1;
								entity_destroy(en);
							}
						}
				}
			}

		//CLICKY CLKIKY
		{
			Entity* selected_en = world_frame.selected_entity;

			if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				if (selected_en) {
					selected_en->health -= 1;
					if (selected_en->health <= 0) {

						switch (selected_en->arch) {
							case arch_loot: {
								// spawn thing
								{
									Entity* en = entity_create();
									setup_item_floatwood(en);
									en->pos = selected_en->pos;
								}
							} break;

							case arch_reef: {
								// 
							} break;

							default: { } break;
						}
						entity_destroy(selected_en);
					}
				}
			}
		}

		// :RENDER
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if (en->is_valid) {

				switch (en->arch) {
										
					default:
					{
						Sprite* sprite = get_sprite(en->sprite_id);
						Matrix4 xform = m4_scalar(1.0);
						if (en->is_item) {
							xform         = m4_translate(xform, v3(0, 1.0 * sin_breathe(os_get_current_time_in_seconds(), 10.0), 0));
						}
						// Scale the reef entity to twice its original size
                		// if (en->arch == arch_reef) {
                    	// xform = m4_scale(xform, v3(2.0, 2.0, 2.0));
                		// }
						xform         = m4_translate(xform, v3(0, tile_width * - 0.5, 0));
						xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
						xform         = m4_translate(xform, v3(sprite->image->width * -0.5, 0.0, 0)); //the original line, down by the x axis
						//xform         = m4_translate(xform, v3(0, sprite->size.y * -0.5, 0)); //attempt to center down the y axis
						
						Vector4 col = COLOR_WHITE;
						if (world_frame.selected_entity == en) {
							col = COLOR_RED;
						}

						draw_image_xform(sprite->image, xform, get_sprite_size(sprite), col);
						// debug pos 
						// draw_text(font, sprint(temp, STR("%f %f"), en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_WHITE);
						break;
					}
				}

			}
		}

		if (is_key_just_pressed(KEY_ESCAPE)) {
			window.should_close = true;
		}
		Vector2 input_axis = v2(0, 0);
		if (is_key_down('A')) {
			input_axis.x -= 1.0;
		}
		if (is_key_down('D')) {
			input_axis.x += 1.0;
		}
		if (is_key_down('S')) {
			input_axis.y -= 1.0;
		}
		if (is_key_down('W')) {
			input_axis.y += 1.0;
		}
		input_axis = v2_normalize(input_axis);

		player_en->pos = v2_add(player_en->pos, v2_mulf(input_axis, 5.0 * delta_t));
		player_en->pos = v2_add(player_en->pos, v2_mulf(input_axis, 100.0 * delta_t));
		
		gfx_update();
		seconds_counter += delta_t;
		frame_count += 1;
		if (seconds_counter > 1.0) {
			log("fps: %i", frame_count);
			seconds_counter = 0.0;
			frame_count = 0;
		}
	}
	return 0;
}
