#include "floor.h"
#include "globals.h" // For SCREEN_WIDTH, SCREEN_HEIGHT (potentially, or pass as params if preferred)
#include "config.h"  // For SCREEN_WIDTH, SCREEN_HEIGHT if not in globals directly for this context
#include <allegro5/allegro_primitives.h>
#include <stdio.h>
#include <math.h> // For floor
#include <asset_manager.h> // Assuming this is where load_bitmap_once is defined
#include "stdlib.h" // For rand() and srand()

// Static global variable for the floor texture, local to this file
static ALLEGRO_BITMAP* battle_floor_texture = NULL;
ALLEGRO_BITMAP* nature_img = NULL;
static ALLEGRO_BITMAP* tree_img = NULL;
static ALLEGRO_BITMAP* tree2_img = NULL;
static ALLEGRO_BITMAP* rock_img = NULL;
static ALLEGRO_BITMAP* rock2_img = NULL;
static ALLEGRO_BITMAP* rock3_img = NULL;
#define MAX_NATURE 10000 // Define a maximum number of nature, adjust as needed
static Nature nature_list[MAX_NATURE]; // Assuming a maximum of 10,000 nature, adjust as neededrock_img = load_bitmap_once("assets/image/rock.png")


static FloorTheme current_floor_theme = FLOOR_THEME_DEFAULT;

// 新增：主題對應顏色
typedef struct {
    ALLEGRO_COLOR color1;
    ALLEGRO_COLOR color2;
} FloorThemeColors;

static FloorThemeColors floor_theme_colors[FLOOR_THEME_COUNT];

/**
 * 初始化主題顏色
 */
void init_floor_theme_colors() {
    floor_theme_colors[FLOOR_THEME_DEFAULT] = (FloorThemeColors){al_map_rgb(40, 60, 40), al_map_rgb(50, 70, 50)};
    floor_theme_colors[FLOOR_THEME_SNOW]    = (FloorThemeColors){al_map_rgb(220, 220, 240), al_map_rgb(200, 200, 220)};
    floor_theme_colors[FLOOR_THEME_SOIL]    = (FloorThemeColors){al_map_rgb(120, 80, 40), al_map_rgb(140, 100, 60)};
    floor_theme_colors[FLOOR_THEME_STONE]   = (FloorThemeColors){al_map_rgb(120, 120, 120), al_map_rgb(160, 160, 160)};
    floor_theme_colors[FLOOR_THEME_NIGHT]   = (FloorThemeColors){al_map_rgb(10, 10, 10), al_map_rgb(30, 30, 40)};
    floor_theme_colors[FLOOR_THEME_DREAM]   = (FloorThemeColors){al_map_rgb(80, 0, 120), al_map_rgb(180, 80, 255)};
}

/**
 * 創建戰鬥場景的地板紋理。
 */
ALLEGRO_BITMAP* create_battle_floor_texture(int tile_size, int num_tiles_w, int num_tiles_h, FloorTheme theme) {
    ALLEGRO_BITMAP* bg = al_create_bitmap(tile_size * num_tiles_w, tile_size * num_tiles_h);
    if (!bg) {
        fprintf(stderr, "創建戰鬥地板紋理失敗！\n");
        return NULL;
    }
    ALLEGRO_STATE old_state;
    al_store_state(&old_state, ALLEGRO_STATE_TARGET_BITMAP); 
    al_set_target_bitmap(bg); 

    ALLEGRO_COLOR color1 = floor_theme_colors[theme].color1;
    ALLEGRO_COLOR color2 = floor_theme_colors[theme].color2;

    for (int r = 0; r < num_tiles_h; ++r) {
        for (int c = 0; c < num_tiles_w; ++c) {
            ALLEGRO_COLOR color = ((r + c) % 2 == 0) ? color1 : color2;
            al_draw_filled_rectangle(c * tile_size, r * tile_size, (c + 1) * tile_size, (r + 1) * tile_size, color);
        }
    }
    al_restore_state(&old_state); 
    return bg;
}

/**
 * 初始化地板資源。
 */
void init_floor(void) {
    
    // 初始化主題顏色
    init_floor_theme_colors();

    // 隨機選擇主題
    current_floor_theme = (FloorTheme)(rand() % FLOOR_THEME_COUNT);
    printf("當前戰鬥場景主題: %d\n", current_floor_theme); // Debug output to check the theme

    if (battle_floor_texture) { // Destroy existing texture if any before creating a new one
        al_destroy_bitmap(battle_floor_texture);
    }
    // 修改：傳入主題
    battle_floor_texture = create_battle_floor_texture(100, 2, 2, current_floor_theme);
    init_nature();
}

/**
 * 渲染戰鬥場景的地板。
 */
void render_battle_floor(ALLEGRO_BITMAP* floor_texture, float camera_x, float camera_y) {
    float tex_w = al_get_bitmap_width(floor_texture);   
    float tex_h = al_get_bitmap_height(floor_texture);  
    if (tex_w > 0 && tex_h > 0) { 
        // Use SCREEN_WIDTH and SCREEN_HEIGHT from config.h (included via globals.h or directly)
        float cam_vx1 = camera_x; 
        float cam_vy1 = camera_y; 
        float cam_vx2 = camera_x + SCREEN_WIDTH; 
        float cam_vy2 = camera_y + SCREEN_HEIGHT; 
            
        int tile_start_x = floor(cam_vx1 / tex_w); 
        int tile_start_y = floor(cam_vy1 / tex_h); 
        int tile_end_x = floor((cam_vx2 - 1) / tex_w); 
        int tile_end_y = floor((cam_vy2 - 1) / tex_h); 
            
        for (int ty = tile_start_y; ty <= tile_end_y; ++ty) {
            for (int tx = tile_start_x; tx <= tile_end_x; ++tx) {
                al_draw_bitmap(floor_texture, (tx * tex_w) - camera_x, (ty * tex_h) - camera_y, 0); 
            }
        }
    }
    // Render nature
    for (int i = 0; i < MAX_NATURE; i++) {
        if (nature_list[i].x >= camera_x - SCREEN_WIDTH && nature_list[i].x <= camera_x + SCREEN_WIDTH &&
            nature_list[i].y >= camera_y - SCREEN_HEIGHT && nature_list[i].y <= camera_y + SCREEN_HEIGHT) {
            int nature_height=100; 
            int nature_width=100; 
            switch (nature_list[i].type)  // Assuming nature_list[i].type is an enum or int representing type
            {
                case TREE:
                    nature_img = tree_img;
                    nature_height = 100;
                    nature_width = 100;
                    break;
                case TREE2:
                    nature_img = tree2_img;
                    nature_height = 100;
                    nature_width = 100;
                    break;
                case ROCK:
                    nature_img = rock_img;
                    nature_height = 100;
                    nature_width = 50;
                    break;
                case ROCK2:
                    nature_img = rock2_img;
                    nature_height = 100;
                    nature_width = 100;
                    break;
                case ROCK3:
                    nature_img = rock3_img;
                    nature_height = 100;
                    nature_width = 100;
                    break;
                default:
                    nature_img = NULL; // Handle unexpected type
            }

            al_draw_scaled_bitmap(nature_img, 0, 0,
                        al_get_bitmap_width(nature_img),
                        al_get_bitmap_height(nature_img), 
                        nature_list[i].x - camera_x, 
                        nature_list[i].y - camera_y, 
                        nature_height, nature_width, 0); 

        }
    }
}

/**
 * 釋放地板紋理資源。
 */
void destroy_floor_texture(void) {
    if (battle_floor_texture) {
        al_destroy_bitmap(battle_floor_texture);
        battle_floor_texture = NULL;
    }
}
/**
 * 獲取戰鬥場景的地板紋理。
 */
ALLEGRO_BITMAP* get_battle_floor_texture(void) {
    return battle_floor_texture;
}

void init_nature() {
    tree_img = load_bitmap_once("assets/image/tree.png");
    tree2_img = load_bitmap_once("assets/image/tree2.png");
    rock_img = load_bitmap_once("assets/image/rock.png");
    rock2_img = load_bitmap_once("assets/image/rock2.png");
    rock3_img = load_bitmap_once("assets/image/rock3.png");
    int i = 0;
    while (i < MAX_NATURE) {
        int candidate_x = ((rand() % 20001) - 10000)*2; // Random x position within screen width
        int candidate_y = ((rand() % 20001) - 10000)*2; // Random y position within screen height
        // Ensure nature are not too close to each other
        bool position_valid = true;
        for (int j = 0; j < i; j++) {
            float distance = sqrtf((candidate_x - nature_list[j].x) * (candidate_x - nature_list[j].x) +
                                   (candidate_y - nature_list[j].y) * (candidate_y - nature_list[j].y));
            if (distance < 10.0f) { // Minimum distance between nature
                position_valid = false;
                break;
            }
        }
        if(position_valid) {
            nature_list[i].type = (rand() % NATURE_TYPE_COUNT); // Randomly assign type (0 for tree, 1 for rock)
            nature_list[i].x = candidate_x; // Assign valid position
            nature_list[i].y = candidate_y;
            i++;
        }
    }
}