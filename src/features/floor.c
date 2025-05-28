#include "include/floor.h"
#include "include/globals.h" // For SCREEN_WIDTH, SCREEN_HEIGHT (potentially, or pass as params if preferred)
#include "include/config.h"  // For SCREEN_WIDTH, SCREEN_HEIGHT if not in globals directly for this context
#include <allegro5/allegro_primitives.h>
#include <stdio.h>
#include <math.h> // For floor

// Static global variable for the floor texture, local to this file
static ALLEGRO_BITMAP* battle_floor_texture = NULL;

/**
 * 創建戰鬥場景的地板紋理。
 */
ALLEGRO_BITMAP* create_battle_floor_texture(int tile_size, int num_tiles_w, int num_tiles_h) {
    ALLEGRO_BITMAP* bg = al_create_bitmap(tile_size * num_tiles_w, tile_size * num_tiles_h);
    if (!bg) {
        fprintf(stderr, "創建戰鬥地板紋理失敗！\n");
        return NULL;
    }
    ALLEGRO_STATE old_state;
    al_store_state(&old_state, ALLEGRO_STATE_TARGET_BITMAP); 
    al_set_target_bitmap(bg); 

    for (int r = 0; r < num_tiles_h; ++r) {
        for (int c = 0; c < num_tiles_w; ++c) {
            ALLEGRO_COLOR color = ((r + c) % 2 == 0) ? al_map_rgb(40, 60, 40) : al_map_rgb(50, 70, 50); 
            al_draw_filled_rectangle(c * tile_size, r * tile_size, (c + 1) * tile_size, (r + 1) * tile_size, color);
        }
    }
    // Example accent, remove if not desired
    al_draw_filled_circle(tile_size * num_tiles_w * 0.25f, tile_size * num_tiles_h * 0.25f, 10, al_map_rgb(100,20,20));
    al_restore_state(&old_state); 
    return bg;
}

/**
 * 初始化地板資源。
 */
void init_floor(void) {
    if (battle_floor_texture) { // Destroy existing texture if any before creating a new one
        al_destroy_bitmap(battle_floor_texture);
    }
    battle_floor_texture = create_battle_floor_texture(100, 2, 2); // Using existing parameters
}

/**
 * 渲染戰鬥場景的地板。
 */
void render_battle_floor(ALLEGRO_BITMAP* floor_texture, float camera_x, float camera_y) {
    if (floor_texture) { // Check if the texture is valid
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
