#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h> // Though font is global, good practice
#include <stdio.h> // For printf during development
#include <math.h> // For sin and cos

#include "globals.h"
#include "minigame_flower.h"
#include "types.h" // Included by globals.h or minigame_flower.h, but good for clarity

// Static global variables for the minigame
static MinigameFlowerPlant flower_plant;
static Button minigame_buttons[NUM_MINIGAME_FLOWER_BUTTONS];
static bool seed_planted = false;
static bool is_singing = false;
static const int songs_to_flower = 8;

void init_minigame_flower(void) {
    flower_plant.songs_sung = 0;
    flower_plant.growth_stage = 0;
    seed_planted = false;
    is_singing = false;

    float button_width = 200;
    float button_height = 50;
    float center_x = SCREEN_WIDTH / 2.0f - button_width / 2.0f;

    // Button 0: "種下種子" (Plant Seed)
    minigame_buttons[0].x = center_x;
    minigame_buttons[0].y = SCREEN_HEIGHT - 200;
    minigame_buttons[0].width = button_width;
    minigame_buttons[0].height = button_height;
    minigame_buttons[0].text = "種下種子";
    minigame_buttons[0].color = al_map_rgb(70, 170, 70);
    minigame_buttons[0].hover_color = al_map_rgb(100, 200, 100);
    minigame_buttons[0].text_color = al_map_rgb(255, 255, 255);
    minigame_buttons[0].action_phase = MINIGAME_FLOWER; // Or a dummy/specific action
    minigame_buttons[0].is_hovered = false;

    // Button 1: "開始唱歌" (Start Singing)
    minigame_buttons[1].x = center_x;
    minigame_buttons[1].y = SCREEN_HEIGHT - 200; // Same position, shown conditionally
    minigame_buttons[1].width = button_width;
    minigame_buttons[1].height = button_height;
    minigame_buttons[1].text = "開始唱歌";
    minigame_buttons[1].color = al_map_rgb(70, 70, 170);
    minigame_buttons[1].hover_color = al_map_rgb(100, 100, 200);
    minigame_buttons[1].text_color = al_map_rgb(255, 255, 255);
    minigame_buttons[1].action_phase = MINIGAME_FLOWER;
    minigame_buttons[1].is_hovered = false;

    // Button 2: "重新開始" (Restart) - Appears with "Finish Singing"
    minigame_buttons[2].x = center_x - button_width / 2 - 10; // Left of center
    minigame_buttons[2].y = SCREEN_HEIGHT - 200;
    minigame_buttons[2].width = button_width;
    minigame_buttons[2].height = button_height;
    minigame_buttons[2].text = "重新開始";
    minigame_buttons[2].color = al_map_rgb(170, 70, 70);
    minigame_buttons[2].hover_color = al_map_rgb(200, 100, 100);
    minigame_buttons[2].text_color = al_map_rgb(255, 255, 255);
    minigame_buttons[2].action_phase = MINIGAME_FLOWER;
    minigame_buttons[2].is_hovered = false;

    // Button 3: "完成歌唱" (Finish Singing) - Appears with "Restart"
    minigame_buttons[3].x = center_x + button_width / 2 + 10; // Right of center
    minigame_buttons[3].y = SCREEN_HEIGHT - 200;
    minigame_buttons[3].width = button_width;
    minigame_buttons[3].height = button_height;
    minigame_buttons[3].text = "完成歌唱";
    minigame_buttons[3].color = al_map_rgb(70, 170, 170);
    minigame_buttons[3].hover_color = al_map_rgb(100, 200, 200);
    minigame_buttons[3].text_color = al_map_rgb(255, 255, 255);
    minigame_buttons[3].action_phase = MINIGAME_FLOWER;
    minigame_buttons[3].is_hovered = false;
    
    // Button 4: "離開小遊戲" (Exit Minigame)
    minigame_buttons[4].width = 150; // Slightly smaller
    minigame_buttons[4].height = 40;
    minigame_buttons[4].x = SCREEN_WIDTH - minigame_buttons[4].width - 20;
    minigame_buttons[4].y = SCREEN_HEIGHT - minigame_buttons[4].height - 20;
    minigame_buttons[4].text = "離開小遊戲";
    minigame_buttons[4].color = al_map_rgb(100, 100, 100);
    minigame_buttons[4].hover_color = al_map_rgb(130, 130, 130);
    minigame_buttons[4].text_color = al_map_rgb(255, 255, 255);
    minigame_buttons[4].action_phase = GROWTH; // This will trigger phase change
    minigame_buttons[4].is_hovered = false;
}

void render_minigame_flower(void) {
    al_clear_to_color(al_map_rgb(50, 50, 70)); // Dark blue-grey background

    float plant_base_y = SCREEN_HEIGHT * 0.65f; // Adjusted for more space for flower
    float plant_x = SCREEN_WIDTH / 2.0f;
    float stem_height = 20 + flower_plant.growth_stage * 15; // Increased growth factor

    // Draw Plant
    if (!seed_planted) {
        al_draw_text(font, al_map_rgb(255, 255, 255), plant_x, plant_base_y - 50, ALLEGRO_ALIGN_CENTER, "請先種下種子");
    } else {
        // Stage 0 (Seed)
        if (flower_plant.growth_stage == 0) {
            al_draw_filled_circle(plant_x, plant_base_y, 10, al_map_rgb(139, 69, 19)); // Brown
        } else { // Stages 1-8 (Growing stem and flower)
            // Stem (common to all growth stages > 0)
            al_draw_filled_rectangle(plant_x - 4, plant_base_y - stem_height, plant_x + 4, plant_base_y, al_map_rgb(0, 128, 0)); // Green stem

            // Leaves (example, can be more elaborate)
            if (flower_plant.growth_stage >= 2) { // First leaf
                 al_draw_filled_triangle(plant_x + 4, plant_base_y - stem_height * 0.3f,
                                        plant_x + 4, plant_base_y - stem_height * 0.3f - 15,
                                        plant_x + 4 + 25, plant_base_y - stem_height * 0.3f - 7,
                                        al_map_rgb(34, 139, 34)); // Forest Green
            }
            if (flower_plant.growth_stage >= 3) { // Second leaf (opposite side)
                 al_draw_filled_triangle(plant_x - 4, plant_base_y - stem_height * 0.5f,
                                        plant_x - 4, plant_base_y - stem_height * 0.5f - 15,
                                        plant_x - 4 - 25, plant_base_y - stem_height * 0.5f - 7,
                                        al_map_rgb(34, 139, 34));
            }
             if (flower_plant.growth_stage >= 4) { 
                 al_draw_filled_triangle(plant_x + 4, plant_base_y - stem_height * 0.7f,
                                        plant_x + 4, plant_base_y - stem_height * 0.7f - 15,
                                        plant_x + 4 + 20, plant_base_y - stem_height * 0.7f - 7,
                                        al_map_rgb(34, 139, 34)); 
            }


            // Stage 8 (Flower)
            if (flower_plant.growth_stage >= songs_to_flower) {
                // Petals
                for (int i = 0; i < 6; ++i) {
                    float angle = (ALLEGRO_PI * 2.0f / 6.0f) * i;
                    al_draw_filled_circle(plant_x + cos(angle) * 20,
                                          (plant_base_y - stem_height - 25) + sin(angle) * 20,
                                          12, al_map_rgb(255, 105, 180)); // Pink petals
                }
                // Flower Center
                al_draw_filled_circle(plant_x, plant_base_y - stem_height - 25, 18, al_map_rgb(255, 255, 0)); // Yellow center
                al_draw_text(font, al_map_rgb(255, 215, 0), plant_x, plant_base_y - stem_height - 70, ALLEGRO_ALIGN_CENTER, "開花了!");
            }
        }
    }

    // Draw Buttons (conditionally)
    // Plant Seed button
    if (!seed_planted) {
        Button* b = &minigame_buttons[0];
        al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color);
        al_draw_text(font, b->text_color, b->x + b->width / 2, b->y + (b->height / 2) - (al_get_font_line_height(font) / 2), ALLEGRO_ALIGN_CENTER, b->text);
    }
    // Start Singing button
    if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower) {
        Button* b = &minigame_buttons[1];
        al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color);
        al_draw_text(font, b->text_color, b->x + b->width / 2, b->y + (b->height / 2) - (al_get_font_line_height(font) / 2), ALLEGRO_ALIGN_CENTER, b->text);
    }
    // Restart and Finish Singing buttons (shown together)
    if (is_singing) {
        Button* b_restart = &minigame_buttons[2];
        al_draw_filled_rectangle(b_restart->x, b_restart->y, b_restart->x + b_restart->width, b_restart->y + b_restart->height, b_restart->is_hovered ? b_restart->hover_color : b_restart->color);
        al_draw_text(font, b_restart->text_color, b_restart->x + b_restart->width / 2, b_restart->y + (b_restart->height / 2) - (al_get_font_line_height(font) / 2), ALLEGRO_ALIGN_CENTER, b_restart->text);

        Button* b_finish = &minigame_buttons[3];
        al_draw_filled_rectangle(b_finish->x, b_finish->y, b_finish->x + b_finish->width, b_finish->y + b_finish->height, b_finish->is_hovered ? b_finish->hover_color : b_finish->color);
        al_draw_text(font, b_finish->text_color, b_finish->x + b_finish->width / 2, b_finish->y + (b_finish->height / 2) - (al_get_font_line_height(font) / 2), ALLEGRO_ALIGN_CENTER, b_finish->text);
    }
    // Exit Minigame button (always visible)
    Button* b_exit = &minigame_buttons[4];
    al_draw_filled_rectangle(b_exit->x, b_exit->y, b_exit->x + b_exit->width, b_exit->y + b_exit->height, b_exit->is_hovered ? b_exit->hover_color : b_exit->color);
    al_draw_text(font, b_exit->text_color, b_exit->x + b_exit->width / 2, b_exit->y + (b_exit->height / 2) - (al_get_font_line_height(font) / 2), ALLEGRO_ALIGN_CENTER, b_exit->text);


    // Draw Text Info
    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2, 30, ALLEGRO_ALIGN_CENTER, "唱歌種花小遊戲");
    if (seed_planted) {
        al_draw_textf(font, al_map_rgb(200, 200, 200), SCREEN_WIDTH - 150, 30, ALLEGRO_ALIGN_RIGHT, "已唱歌曲: %d / %d", flower_plant.songs_sung, songs_to_flower);
    }
}

void handle_minigame_flower_input(ALLEGRO_EVENT ev) {
    // Reset hover states for all buttons first
    for (int i = 0; i < NUM_MINIGAME_FLOWER_BUTTONS; ++i) {
        minigame_buttons[i].is_hovered = false;
    }

    // Mouse movement for hover effects
    if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
        // Plant Seed
        if (!seed_planted && ev.mouse.x >= minigame_buttons[0].x && ev.mouse.x <= minigame_buttons[0].x + minigame_buttons[0].width &&
            ev.mouse.y >= minigame_buttons[0].y && ev.mouse.y <= minigame_buttons[0].y + minigame_buttons[0].height) {
            minigame_buttons[0].is_hovered = true;
        }
        // Start Singing
        else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower &&
                 ev.mouse.x >= minigame_buttons[1].x && ev.mouse.x <= minigame_buttons[1].x + minigame_buttons[1].width &&
                 ev.mouse.y >= minigame_buttons[1].y && ev.mouse.y <= minigame_buttons[1].y + minigame_buttons[1].height) {
            minigame_buttons[1].is_hovered = true;
        }
        // Restart and Finish Singing (when is_singing is true)
        else if (is_singing) {
            if (ev.mouse.x >= minigame_buttons[2].x && ev.mouse.x <= minigame_buttons[2].x + minigame_buttons[2].width &&
                ev.mouse.y >= minigame_buttons[2].y && ev.mouse.y <= minigame_buttons[2].y + minigame_buttons[2].height) {
                minigame_buttons[2].is_hovered = true;
            }
            if (ev.mouse.x >= minigame_buttons[3].x && ev.mouse.x <= minigame_buttons[3].x + minigame_buttons[3].width &&
                ev.mouse.y >= minigame_buttons[3].y && ev.mouse.y <= minigame_buttons[3].y + minigame_buttons[3].height) {
                minigame_buttons[3].is_hovered = true;
            }
        }
        // Exit button (always check)
        if (ev.mouse.x >= minigame_buttons[4].x && ev.mouse.x <= minigame_buttons[4].x + minigame_buttons[4].width &&
            ev.mouse.y >= minigame_buttons[4].y && ev.mouse.y <= minigame_buttons[4].y + minigame_buttons[4].height) {
            minigame_buttons[4].is_hovered = true;
        }
    }
    // Mouse button click
    else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev.mouse.button == 1) { // Left mouse button
            bool button_clicked = false;
            // Plant Seed
            if (!seed_planted && minigame_buttons[0].is_hovered) {
                seed_planted = true;
                is_singing = false; // Reset singing state
                flower_plant.songs_sung = 0; // Reset progress if re-planting
                flower_plant.growth_stage = 0;
                button_clicked = true;
            }
            // Start Singing
            else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower && minigame_buttons[1].is_hovered) {
                is_singing = true;
                // TODO: Add audio recording start logic here
                printf("Minigame: Start Singing button clicked. Audio recording should start.\n"); // Temporary feedback
                button_clicked = true;
            }
            // Restart or Finish Singing
            else if (is_singing) {
                if (minigame_buttons[2].is_hovered) { // Restart
                    // For now, it just allows finishing again. Could reset timer if one existed.
                    // TODO: Add audio recording restart logic here (e.g., discard current recording, start new one)
                    printf("Minigame: Restart singing button clicked. Audio recording should restart.\n"); // Temporary feedback
                    // is_singing remains true
                    button_clicked = true;
                }
                else if (minigame_buttons[3].is_hovered) { // Finish Singing
                    is_singing = false;
                    // TODO: Add audio recording stop and processing logic here
                    printf("Minigame: Finish Singing button clicked. Audio recording should stop and be processed.\n"); // Temporary feedback
                    if (flower_plant.songs_sung < songs_to_flower) {
                        flower_plant.songs_sung++;
                    }
                    // Growth stage directly maps to songs sung, up to max
                    flower_plant.growth_stage = flower_plant.songs_sung; 
                    if (flower_plant.growth_stage > songs_to_flower) {
                        flower_plant.growth_stage = songs_to_flower;
                    }
                    button_clicked = true;
                }
            }
            // Exit button
            if (minigame_buttons[4].is_hovered) { // Check separately as it's always active
                game_phase = GROWTH; // Change game phase
                init_minigame_flower(); // Reset minigame state for next time
                button_clicked = true; // Technically a click, though phase changes
            }

            if (button_clicked) {
                for (int i = 0; i < NUM_MINIGAME_FLOWER_BUTTONS; ++i) {
                    minigame_buttons[i].is_hovered = false; // Reset hover on click
                }
            }
        }
    }
    // Key press
    else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GROWTH;
            init_minigame_flower(); // Reset for next time
        }
    }
}

void update_minigame_flower(void) {
    // Currently no continuous updates needed for this minigame logic
    // (e.g., animations, timers that run without player input)
}
