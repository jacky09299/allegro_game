#include "game_state.h"
#include "globals.h"    // For game_phase, menu_buttons, growth_buttons, player, camera, etc.
#include "config.h"     // For screen dimensions, button config
#include "player.h"     // For init_player, init_player_knife, skill usage functions
#include "boss.h"       // For init_bosses_by_archetype
#include "projectile.h" // For init_projectiles
#include "minigame_flower.h" // For the flower minigame
#include <allegro5/allegro_primitives.h> // For drawing rects/text
#include <allegro5/allegro_image.h> // For image loading
#include <stdio.h>      // For printf

/**
 * @brief 初始化主選單的按鈕。
 */
void init_menu_buttons() {
    float button_width = 200;
    float button_height = 50;
    float center_x = SCREEN_WIDTH / 2.0f;

    menu_buttons[0] = (Button){
        center_x - button_width / 2, SCREEN_HEIGHT / 2.0f - button_height * 1.5f - 20,
        button_width, button_height, "開始戰鬥", BATTLE,                              
        al_map_rgb(70, 70, 170), al_map_rgb(100, 100, 220), al_map_rgb(255, 255, 255), false
    };
    menu_buttons[1] = (Button){
        center_x - button_width / 2, SCREEN_HEIGHT / 2.0f,
        button_width, button_height, "養成", GROWTH,
        al_map_rgb(70, 170, 70), al_map_rgb(100, 220, 100), al_map_rgb(255, 255, 255), false
    };
    menu_buttons[2] = (Button){
        center_x - button_width / 2, SCREEN_HEIGHT / 2.0f + button_height * 1.5f + 20,
        button_width, button_height, "退出", EXIT,
        al_map_rgb(170, 70, 70), al_map_rgb(220, 100, 100), al_map_rgb(255, 255, 255), false
    };
}

/**
 * @brief 初始化養成畫面的按鈕。
 */
void init_growth_buttons() {
    float button_width = 280;
    float button_height = 55;
    float button_spacing = 15;
    float first_button_y = 350;
    float center_x = SCREEN_WIDTH / 2.0f;

    growth_buttons[0] = (Button){
        center_x - button_width / 2, first_button_y,
        button_width, button_height, "進行小遊戲挑戰 1", GROWTH, // Action phase might change if it goes to a new phase
        al_map_rgb(60, 160, 160), al_map_rgb(90, 190, 190), al_map_rgb(255, 255, 255), false
    };
    growth_buttons[1] = (Button){
        center_x - button_width / 2, first_button_y + (button_height + button_spacing),
        button_width, button_height, "進行小遊戲挑戰 2", GROWTH,
        al_map_rgb(60, 160, 160), al_map_rgb(90, 190, 190), al_map_rgb(255, 255, 255), false
    };
    growth_buttons[2] = (Button){
        center_x - button_width / 2, first_button_y + 2 * (button_height + button_spacing),
        button_width, button_height, "幸運抽獎", GROWTH,
        al_map_rgb(160, 160, 60), al_map_rgb(190, 190, 90), al_map_rgb(255, 255, 255), false
    };
    growth_buttons[3] = (Button){
        center_x - button_width / 2, first_button_y + 3 * (button_height + button_spacing),
        button_width, button_height, "開啟背包", GROWTH,
        al_map_rgb(160, 60, 160), al_map_rgb(190, 90, 190), al_map_rgb(255, 255, 255), false
    };
}

/**
 * @brief 渲染主選單畫面。
 */
void render_main_menu() {
    al_clear_to_color(al_map_rgb(30, 30, 50)); 
    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 4, ALLEGRO_ALIGN_CENTER, "遊戲 - 主選單");
    for (int i = 0; i < 3; i++) { // Assuming 3 menu buttons
        Button* b = &menu_buttons[i];
        ALLEGRO_COLOR bg_color = b->is_hovered ? b->hover_color : b->color; 
        al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, bg_color); 
        al_draw_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, al_map_rgb(200,200,200), 2.0f); 
        al_draw_text(font, b->text_color, b->x + b->width / 2, b->y + b->height / 2 - al_get_font_ascent(font) / 2, ALLEGRO_ALIGN_CENTER, b->text); 
    }
}

/**
 * @brief 渲染養成畫面。
 */
void render_growth_screen() {
    al_clear_to_color(al_map_rgb(40, 40, 60));
    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2, 50, ALLEGRO_ALIGN_CENTER, "養成畫面");

    float stats_x = 50;
    float stats_y_start = 120;
    float line_height = 30;
    al_draw_text(font, al_map_rgb(255, 255, 255), stats_x, stats_y_start, 0, "玩家數值:");
    al_draw_textf(font, al_map_rgb(200, 220, 255), stats_x, stats_y_start + line_height, 0, "生命值: %d / %d", player.hp, player.max_hp);
    al_draw_textf(font, al_map_rgb(200, 220, 255), stats_x, stats_y_start + 2 * line_height, 0, "力量: %d", player.strength);
    al_draw_textf(font, al_map_rgb(200, 220, 255), stats_x, stats_y_start + 3 * line_height, 0, "魔力: %d", player.magic);
    al_draw_textf(font, al_map_rgb(200, 220, 255), stats_x, stats_y_start + 4 * line_height, 0, "速度: %.1f", player.speed);
    al_draw_textf(font, al_map_rgb(255, 215, 0), stats_x, stats_y_start + 5 * line_height, 0, "金錢: %d", player.money);

    for (int i = 0; i < MAX_GROWTH_BUTTONS; i++) {
        Button* b = &growth_buttons[i];
        ALLEGRO_COLOR bg_color = b->is_hovered ? b->hover_color : b->color;
        al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, bg_color);
        al_draw_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, al_map_rgb(200,200,200), 2.0f);
        al_draw_text(font, b->text_color, b->x + b->width / 2, b->y + b->height / 2 - al_get_font_ascent(font) / 2, ALLEGRO_ALIGN_CENTER, b->text);
    }
    al_draw_text(font, al_map_rgb(200, 200, 200), SCREEN_WIDTH / 2, SCREEN_HEIGHT - 50, ALLEGRO_ALIGN_CENTER, "按 ESC 返回主選單");
}

/**
 * @brief 處理主選單的輸入事件。
 */
void handle_main_menu_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) { 
        for (int i = 0; i < 3; ++i) { 
            menu_buttons[i].is_hovered = (ev.mouse.x >= menu_buttons[i].x &&
                                         ev.mouse.x <= menu_buttons[i].x + menu_buttons[i].width &&
                                         ev.mouse.y >= menu_buttons[i].y &&
                                         ev.mouse.y <= menu_buttons[i].y + menu_buttons[i].height); 
        }
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && ev.mouse.button == 1) { 
        for (int i = 0; i < 3; ++i) {
            if (menu_buttons[i].is_hovered) { 
                game_phase = menu_buttons[i].action_phase; 
                if (game_phase == BATTLE) { 
                    player.hp = player.max_hp;
                    init_player_knife(); 
                    init_bosses_by_archetype(); 
                    init_projectiles();
                    camera_x = player.x - SCREEN_WIDTH/2.0f; 
                    camera_y = player.y - SCREEN_HEIGHT/2.0f;
                }
                if (game_phase == GROWTH) {
                     for (int k = 0; k < MAX_GROWTH_BUTTONS; ++k) {
                        growth_buttons[k].is_hovered = false;
                    }
                }
                for(int j = 0; j < 3; ++j) menu_buttons[j].is_hovered = false; 
                break;
            }
        }
    } else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) { 
        if (ev.keyboard.keycode == ALLEGRO_KEY_1 || ev.keyboard.keycode == ALLEGRO_KEY_ENTER) { 
            game_phase = BATTLE;
            player.hp = player.max_hp;
            init_player_knife();
            init_bosses_by_archetype(); 
            init_projectiles();
            camera_x = player.x - SCREEN_WIDTH/2.0f; camera_y = player.y - SCREEN_HEIGHT/2.0f;
        } else if (ev.keyboard.keycode == ALLEGRO_KEY_2) { 
            game_phase = GROWTH;
            for (int k = 0; k < MAX_GROWTH_BUTTONS; ++k) {
                growth_buttons[k].is_hovered = false;
            }
        } else if (ev.keyboard.keycode == ALLEGRO_KEY_3 || ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) { 
            if (game_phase == MENU) game_phase = EXIT; 
        }
    }
}

/**
 * @brief 處理養成畫面的輸入事件。
 */
void handle_growth_screen_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
        game_phase = MENU;
        for (int i = 0; i < 3; ++i) {
            menu_buttons[i].is_hovered = false;
        }
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
        for (int i = 0; i < MAX_GROWTH_BUTTONS; ++i) {
            growth_buttons[i].is_hovered = (ev.mouse.x >= growth_buttons[i].x &&
                                         ev.mouse.x <= growth_buttons[i].x + growth_buttons[i].width &&
                                         ev.mouse.y >= growth_buttons[i].y &&
                                         ev.mouse.y <= growth_buttons[i].y + growth_buttons[i].height);
        }
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && ev.mouse.button == 1) {
        if (growth_buttons[0].is_hovered) {
            on_minigame1_button_click();
        } else if (growth_buttons[1].is_hovered) {
            on_minigame2_button_click();
        } else if (growth_buttons[2].is_hovered) {
            on_lottery_button_click();
        } else if (growth_buttons[3].is_hovered) {
            on_backpack_button_click();
        }
    }
}

/**
 * @brief 處理戰鬥場景中的玩家動作輸入事件。
 */
void handle_battle_scene_input_actions(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) { 
        switch (ev.keyboard.keycode) {
        case ALLEGRO_KEY_J: player_perform_normal_attack(); break; 
        case ALLEGRO_KEY_K: player_use_water_attack(); break;      
        case ALLEGRO_KEY_L: player_use_ice_shard(); break;         
        case ALLEGRO_KEY_U: player_use_lightning_bolt(); break;    
        case ALLEGRO_KEY_I: player_use_heal(); break;              
        case ALLEGRO_KEY_O: player_use_fireball(); break;          
        case ALLEGRO_KEY_ESCAPE: 
            game_phase = MENU; 
            for (int i = 0; i < 3; ++i) {
                 menu_buttons[i].is_hovered = false;
            }
            break;         
        }
    }
}

// --- 養成畫面按鈕點擊事件的處理函式 ---
void on_minigame1_button_click() {
    game_phase = MINIGAME_FLOWER;
    init_minigame_flower(); // Initialize or reset the minigame state
}

void on_minigame2_button_click() {
    printf("按鈕 [進行小遊戲挑戰 2] 已點擊 - 請在此處添加您的邏輯。\n");
}

void on_lottery_button_click() {
    printf("按鈕 [幸運抽獎] 已點擊 - 請在此處添加您的邏輯。\n");
}

void on_backpack_button_click() {
    game_phase = BACKPACK_SCREEN;
    // Optionally, you can keep the printf for debugging or remove it.
    // For now, let's keep it to confirm the function is called.
    printf("按鈕 [開啟背包] 已點擊 - transitioning to BACKPACK_SCREEN.\n");
    // Potentially, reset hover states for backpack buttons if they are initialized elsewhere
    // For now, just changing the phase is the primary goal.
}

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

        // Placeholder for flower.png - draw a pink square
        // In a real scenario, you'd load flower.png here:
        // ALLEGRO_BITMAP* flower_img = al_load_bitmap("flower.png");
        // if (flower_img) {
        //     al_draw_scaled_bitmap(flower_img, 0, 0, al_get_bitmap_width(flower_img), al_get_bitmap_height(flower_img),
        //                           first_cell_x + 5, first_cell_y + 5, cell_size - 10, cell_size - 10, 0);
        //     al_destroy_bitmap(flower_img);
        // } else {
        // Fallback if flower.png is missing
        al_draw_filled_rectangle(first_cell_x + 5, first_cell_y + 5, 
                                 first_cell_x + cell_size - 5, first_cell_y + cell_size - 5, 
                                 al_map_rgb(255, 105, 180)); // Pink placeholder
        al_draw_text(font, al_map_rgb(0,0,0), first_cell_x + 10, first_cell_y + 10, 0, "F.PNG"); // Indicate missing
        // }
        
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


    // Reminder: flower.png is missing. The code above uses a placeholder.
    // Player struct and font are from globals.h
}

void handle_backpack_screen_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GROWTH; // Go back to Growth screen
        }
    }
    // Mouse handling for a "Back" button will be added in a subsequent step.
}