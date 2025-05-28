#ifndef FLOOR_H
#define FLOOR_H

#include <allegro5/allegro.h>

// Function declarations for battle scene floor rendering
ALLEGRO_BITMAP* create_battle_floor_texture(int tile_size, int num_tiles_w, int num_tiles_h);
void render_battle_floor(ALLEGRO_BITMAP* floor_texture, float camera_x, float camera_y);
void init_floor(void);
void destroy_floor_texture(void);
ALLEGRO_BITMAP* get_battle_floor_texture(void); // Getter function

#endif // FLOOR_H
