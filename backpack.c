#include "backpack.h"
#include "globals.h"    // For font, player, SCREEN_WIDTH, SCREEN_HEIGHT, game_phase (potentially)
#include "config.h"     // For screen dimensions if not in globals
#include "game_state.h" // For game_phase (if used to switch back to GROWTH)
                        // and potentially other states if backpack leads elsewhere.
#include <stdio.h>      // For sprintf, printf (if any remains)
#include <allegro5/allegro_primitives.h> // For drawing
#include <allegro5/allegro_font.h> // For text
// Note: flower_image_asset is in globals.h

/**
 * @brief 渲染背包畫面。
 */
void render_backpack_screen(void) {
    al_clear_to_color(al_map_rgb(40, 30, 40)); // Dark purple background

    // Draw Title
    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2, 50, ALLEGRO_ALIGN_CENTER, "背包");

    // Grid properties
    int grid_rows = 3;
    int grid_cols = 3;
    float cell_size = 80; // Size of each grid cell
    float grid_padding = 10; // Padding around the grid and between cells
    float total_grid_width = grid_cols * cell_size + (grid_cols - 1) * grid_padding;
    float total_grid_height = grid_rows * cell_size + (grid_rows - 1) * grid_padding;
    float grid_start_x = (SCREEN_WIDTH - total_grid_width) / 2.0f;
    float grid_start_y = (SCREEN_HEIGHT - total_grid_height) / 2.0f;

    // Draw Grid Cells
    for (int r = 0; r < grid_rows; ++r) {
        for (int c = 0; c < grid_cols; ++c) {
            float cell_x = grid_start_x + c * (cell_size + grid_padding);
            float cell_y = grid_start_y + r * (cell_size + grid_padding);
            al_draw_filled_rectangle(cell_x, cell_y, cell_x + cell_size, cell_y + cell_size, al_map_rgb(70, 60, 70)); // Darker cell background
            al_draw_rectangle(cell_x, cell_y, cell_x + cell_size, cell_y + cell_size, al_map_rgb(100, 90, 100), 2.0f); // Cell border
        }
    }

    // Display Flowers (if any)
    if (player.flowers_collected > 0) {
        float first_cell_x = grid_start_x;
        float first_cell_y = grid_start_y;

        if (flower_image_asset) {
            float img_w = al_get_bitmap_width(flower_image_asset);
            float img_h = al_get_bitmap_height(flower_image_asset);
            if (img_w > 0 && img_h > 0) { // Check for valid bitmap dimensions
                // float cell_draw_area_size = cell_size - 10.0f; // No longer primary for scaling w/h
                // float draw_x_base = first_cell_x + 5.0f; // Used directly
                // float draw_y_base = first_cell_y + 5.0f; // Used directly
                
                // REMOVED: Fit image within cell_draw_area_size, maintaining aspect ratio
                // float aspect_ratio = img_w / img_h;
                // float scaled_w, scaled_h, offset_x = 0, offset_y = 0;
                // if (aspect_ratio > 1) { ... } else { ... }

                // CHANGED to "Fill (Stretch)" logic:
                al_draw_scaled_bitmap(flower_image_asset,
                                      0, 0, img_w, img_h, // Source bitmap x, y, width, height
                                      first_cell_x + 5.0f, first_cell_y + 5.0f, // Destination x, y
                                      cell_size - 10.0f, cell_size - 10.0f, // Destination width, height
                                      0); // Flags
            } else { // Bitmap loaded but has invalid dimensions
                al_draw_filled_rectangle(first_cell_x + 5, first_cell_y + 5,
                                         first_cell_x + cell_size - 5, first_cell_y + cell_size - 5,
                                         al_map_rgb(255, 0, 0)); 
                al_draw_text(font, al_map_rgb(255,255,255), first_cell_x + cell_size / 2, first_cell_y + cell_size / 2 - 10, ALLEGRO_ALIGN_CENTER, "IMG_ERR");
            }
        } else { // flower_image_asset is NULL (failed to load)
            al_draw_filled_rectangle(first_cell_x + 5, first_cell_y + 5,
                                     first_cell_x + cell_size - 5, first_cell_y + cell_size - 5,
                                     al_map_rgb(255, 100, 100)); // Placeholder for missing asset
            al_draw_text(font, al_map_rgb(0,0,0), first_cell_x + cell_size / 2, first_cell_y + cell_size / 2 - 10, ALLEGRO_ALIGN_CENTER, "NO_IMG");
        }
        
        // Draw quantity text
        char quantity_text[10];
        sprintf(quantity_text, "x%d", player.flowers_collected);
        al_draw_text(font, al_map_rgb(255, 255, 255), 
                     first_cell_x + cell_size - 5, first_cell_y + cell_size - al_get_font_line_height(font) - 5, 
                     ALLEGRO_ALIGN_RIGHT, quantity_text);
    }

    // Draw "Back" button (placeholder text, actual button later)
    // For now, just text. Actual button will be handled by handle_backpack_screen_input and init_backpack_buttons
    float back_button_x = SCREEN_WIDTH / 2.0f;
    float back_button_y = SCREEN_HEIGHT - 100;
    al_draw_text(font, al_map_rgb(200, 200, 200), back_button_x, back_button_y, ALLEGRO_ALIGN_CENTER, "[ Back ]");
    al_draw_text(font, al_map_rgb(150, 150, 150), back_button_x, back_button_y + 20, ALLEGRO_ALIGN_CENTER, "(ESC also works)");


    // Reminder: flower.png is missing. The code above uses a placeholder. // This comment is now outdated as loading is handled in graphics.c
    // Player struct and font are from globals.h

    // Display Devil Flowers (if any) - New block
    if (player.devil_flowers_collected > 0) {
        // Display in the second cell of the first row
        float item_cell_x = grid_start_x + 1 * (cell_size + grid_padding); 
        float item_cell_y = grid_start_y;

        if (devil_flower_image_asset) { // Use the new asset variable
            float img_w = al_get_bitmap_width(devil_flower_image_asset);
            float img_h = al_get_bitmap_height(devil_flower_image_asset);
            if (img_w > 0 && img_h > 0) {
                al_draw_scaled_bitmap(devil_flower_image_asset,
                                      0, 0, img_w, img_h, // Source bitmap x, y, width, height
                                      item_cell_x + 5.0f, item_cell_y + 5.0f, // Destination x, y
                                      cell_size - 10.0f, cell_size - 10.0f, // Destination width, height
                                      0); // Flags
            } else { // Bitmap loaded but has invalid dimensions
                al_draw_filled_rectangle(item_cell_x + 5, item_cell_y + 5,
                                         item_cell_x + cell_size - 5, item_cell_y + cell_size - 5,
                                         al_map_rgb(255, 0, 0)); 
                al_draw_text(font, al_map_rgb(255,255,255), item_cell_x + cell_size / 2, item_cell_y + cell_size / 2 - 10, ALLEGRO_ALIGN_CENTER, "IMG_ERR");
            }
        } else { // devil_flower_image_asset is NULL (failed to load)
            al_draw_filled_rectangle(item_cell_x + 5, item_cell_y + 5,
                                     item_cell_x + cell_size - 5, item_cell_y + cell_size - 5,
                                     al_map_rgb(255, 100, 100)); // Placeholder
            al_draw_text(font, al_map_rgb(0,0,0), item_cell_x + cell_size / 2, item_cell_y + cell_size / 2 - 10, ALLEGRO_ALIGN_CENTER, "NO_IMG");
        }
        
        // Draw quantity text for devil flowers
        char quantity_text_devil[10];
        sprintf(quantity_text_devil, "x%d", player.devil_flowers_collected);
        al_draw_text(font, al_map_rgb(255, 255, 255), 
                     item_cell_x + cell_size - 5, item_cell_y + cell_size - al_get_font_line_height(font) - 5, 
                     ALLEGRO_ALIGN_RIGHT, quantity_text_devil);
    }
}

void handle_backpack_screen_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GROWTH; // Go back to Growth screen
        }
    }
    // Mouse handling for a "Back" button will be added in a subsequent step.
}
