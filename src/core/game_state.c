#include "game_state.h"
#include "globals.h"    // For game_phase, menu_buttons, growth_buttons, player, camera, etc.
#include "config.h"     // For screen dimensions, button config
#include "player.h"     // For init_player, init_player_knife, skill usage functions
#include "boss.h"       // For init_bosses_by_archetype
#include "projectile.h" // For init_projectiles
#include "battle_manager.h" 
#include <allegro5/allegro_primitives.h> 
#include <stdio.h>
#include "escape_gate.h" // For init_escape_gate()
#include "minigame1.h"
#include "minigame2.h"
#include "lottery.h"
#include "backpack.h"
#include "player_skill_select.h" // For player skill management
static int count_play_minigame[3] = {0};

/**
 * 初始化主選單的按鈕。
 */
void init_menu_buttons() {
    float button_width = 200;
    float button_height = 50;
    float center_x = SCREEN_WIDTH / 2.0f;

    menu_buttons[0] = (Button){
        center_x - button_width / 2, SCREEN_HEIGHT / 2.0f - button_height * 1.5f - 20,
        button_width, button_height, "開始遊戲", GROWTH,                              
        al_map_rgb(70, 70, 170), al_map_rgb(100, 100, 220), al_map_rgb(255, 255, 255), false
    };
    menu_buttons[1] = (Button){
        center_x - button_width / 2, SCREEN_HEIGHT / 2.0f,
        button_width, button_height, "遊戲手冊", TUTORIAL,
        al_map_rgb(70, 170, 70), al_map_rgb(100, 220, 100), al_map_rgb(255, 255, 255), false
    };
    menu_buttons[2] = (Button){
        center_x - button_width / 2, SCREEN_HEIGHT / 2.0f + button_height * 1.5f + 20,
        button_width, button_height, "退出", EXIT,
        al_map_rgb(170, 70, 70), al_map_rgb(220, 100, 100), al_map_rgb(255, 255, 255), false
    };
}

/**
 * 初始化養成畫面的按鈕。
 */
void init_growth_buttons() {
    float button_width = 280;
    float button_height = 55;
    float button_spacing = 15;
    float first_button_y = 350;
    float center_x = SCREEN_WIDTH / 2.0f;

    growth_buttons[0] = (Button){
        center_x - button_width / 2, first_button_y,
        button_width, button_height, "進行小遊戲挑戰 1", GROWTH, 
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
    growth_buttons[4] = (Button){
    center_x - button_width / 2, first_button_y + 4 * (button_height + button_spacing),
    button_width, button_height, "跳過時段", GROWTH,
    al_map_rgb(100, 100, 100), al_map_rgb(150, 150, 150), al_map_rgb(255, 255, 255), false
};
}

/**
 * 渲染主選單畫面。
 */
void render_main_menu() {
    al_clear_to_color(al_map_rgb(30, 30, 50)); 
    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 4, ALLEGRO_ALIGN_CENTER, "遊戲 - 主選單");
    for (int i = 0; i < 3; i++) { // 先假設只有三個按鈕
        Button* b = &menu_buttons[i];
        ALLEGRO_COLOR bg_color = b->is_hovered ? b->hover_color : b->color; 
        al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, bg_color); 
        al_draw_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, al_map_rgb(200,200,200), 2.0f); 
        al_draw_text(font, b->text_color, b->x + b->width / 2, b->y + b->height / 2 - al_get_font_ascent(font) / 2, ALLEGRO_ALIGN_CENTER, b->text); 
    }
}

/**
 * 渲染養成畫面。
 */
void render_growth_screen() {
    const char* period_str[] = { "早上", "中午", "晚上" };

    al_clear_to_color(al_map_rgb(40, 40, 60));
    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2, 50, ALLEGRO_ALIGN_CENTER, "養成畫面");
    al_draw_textf(font, al_map_rgb(220,220,255), SCREEN_WIDTH / 2, 30, ALLEGRO_ALIGN_CENTER,"第 %d 天 - %s", current_day, period_str[day_time - 1]);

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

    if (growth_message_timer > 0) {
    al_draw_text(font, al_map_rgb(255, 255, 0),
                 SCREEN_WIDTH / 2, 100,
                 ALLEGRO_ALIGN_CENTER, growth_message);
    growth_message_timer--;
}
}

/**
 * 處理主選單的輸入事件。
 */
void handle_main_menu_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {  // 判斷是否在按鈕上
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
                    start_new_battle(); // MODIFIED
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
    } 
}

/**
 * 處理養成畫面的輸入事件。
 */
void handle_growth_screen_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
        game_phase = MENU;
        for (int i = 0; i < 3; ++i) {
            menu_buttons[i].is_hovered = false;
        }
    }
    /*
    else if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_E) {
        game_phase = EQUIPMENT;
    }
    */
    else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
        for (int i = 0; i < MAX_GROWTH_BUTTONS; ++i) {
            growth_buttons[i].is_hovered = (ev.mouse.x >= growth_buttons[i].x &&
                                         ev.mouse.x <= growth_buttons[i].x + growth_buttons[i].width &&
                                         ev.mouse.y >= growth_buttons[i].y &&
                                         ev.mouse.y <= growth_buttons[i].y + growth_buttons[i].height);
        }
    } 
    else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && ev.mouse.button == 1) {
        if (growth_buttons[0].is_hovered) {
            on_minigame1_button_click();
        } else if (growth_buttons[1].is_hovered) {
            on_minigame2_button_click();
        } else if (growth_buttons[2].is_hovered) {
            on_lottery_button_click();
        } else if (growth_buttons[3].is_hovered) {
            on_backpack_button_click();
        }else if (growth_buttons[4].is_hovered) {
            int restore = (2 - count_play_minigame[day_time-1]) * 5;
            if(restore > 0) {
                player.hp += restore;
                if(player.hp > player.max_hp) player.hp = player.max_hp;
                snprintf(growth_message, sizeof(growth_message), "剩餘的養成次數已轉為血量(hp+%d)",restore);
                growth_message_timer = 180; // 顯示訊息約 3 秒（假設 60 FPS）
            }
            if (day_time<3){
                day_time++;
            }
            else {
                day_time = 1;
                current_day++;
                count_play_minigame[0] = 0;
                count_play_minigame[1] = 0;
                count_play_minigame[2] = 0;
                game_phase = BATTLE;
                start_new_battle(); 
            }
        }
    }
}

// --- 養成畫面按鈕點擊事件的處理函式 ---
void on_minigame1_button_click() {
    if(count_play_minigame[day_time-1]<2){
        count_play_minigame[day_time-1]++;
        game_phase = MINIGAME1;
        init_minigame1();
    } else {
        snprintf(growth_message, sizeof(growth_message), "養成次數已用罄");
        growth_message_timer = 180; // 顯示訊息約 3 秒（假設 60 FPS）
    }
}

void on_minigame2_button_click() {
    if(count_play_minigame[day_time-1]<2){
        count_play_minigame[day_time-1]++;
        game_phase = MINIGAME2;
    } else 
    {
        snprintf(growth_message, sizeof(growth_message), "養成次數已用罄");
        growth_message_timer = 180; // 顯示訊息約 3 秒（假設 60 FPS）
    }
    
}

void on_lottery_button_click() {
    game_phase = LOTTERY;
    init_lottery();
}

void on_backpack_button_click() {
    game_phase = BACKPACK;
    //init_backpack();
}