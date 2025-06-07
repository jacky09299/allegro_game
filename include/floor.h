#ifndef FLOOR_H
#define FLOOR_H

#include <allegro5/allegro.h>

typedef enum {
    FLOOR_THEME_DEFAULT,
    FLOOR_THEME_SNOW,
    FLOOR_THEME_SOIL,
    FLOOR_THEME_STONE,
    FLOOR_THEME_NIGHT,
    FLOOR_THEME_DREAM,
    FLOOR_THEME_COUNT
} FloorTheme;

typedef enum {
    TREE,
    ROCK,
    TREE2,
    ROCK2,
    ROCK3,
    NATURE_TYPE_COUNT
} NatureType;

typedef struct {
    float x;
    float y;
    NatureType type; // 使用枚舉類型來表示自然物體的類型
} Nature;


// Function declarations for battle scene floor rendering
ALLEGRO_BITMAP* create_battle_floor_texture(int tile_size, int num_tiles_w, int num_tiles_h, FloorTheme theme);
void init_floor_theme_colors(void);
void render_battle_floor(ALLEGRO_BITMAP* floor_texture, float camera_x, float camera_y);
void init_floor(void);
void destroy_floor_texture(void);
ALLEGRO_BITMAP* get_battle_floor_texture(void); // Getter function
void init_nature(void);

#endif // FLOOR_H
