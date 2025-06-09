#include "battle_manager.h"
#include "globals.h"
#include "player.h"
#include "boss.h"
#include "player_attack.h"
#include "projectile.h"
#include "floor.h"
#include "escape_gate.h"
#include "config.h"  // 為了完整性而包含，許多常數來自 globals.h
#include "types.h"   // 為了完整性而包含，許多型別來自 globals.h
#include "utils.h"   // 用於 calculate_distance_between_points
#include "effect.h" 
#include "backpack.h" // 用於背包相關功能

#include <stdio.h>  
#include <allegro5/allegro_primitives.h> // 用於 al_draw_filled_circle, al_draw_line, al_clear_to_color 等
#include <allegro5/allegro_color.h> // 用於 al_map_rgb

bool is_backpack_open = false; // 是否打開背包
bool last_key_b_state = false;
bool is_pause = false; // 是否暫停
bool last_key_esc_state = false;

void start_new_battle(void) {
    // 初始化玩家
    player.hp = player.max_hp; 
    player.x = 0.0f;         
    player.y = 0.0f;
    player.mp = player.max_mp;
    player.speed = player.max_speed;

    init_player_knife();
    init_bosses_by_archetype(); 
    init_projectiles();   
    init_escape_gate();  
    init_floor(); 
    init_player();

    // 初始化攝影機位置
    camera_x = player.x - SCREEN_WIDTH / 2.0f;
    camera_y = player.y - SCREEN_HEIGHT / 2.0f;

    // 重製戰鬥計時
    battle_time = 0;
    
    is_backpack_open = false; // 重置背包狀態
    is_pause = false; // 重置暫停狀態

}

void manage_battle_state(void) {
    // 玩家死亡檢查
    if (player.hp <= 0) { 
        printf("遊戲結束 - 你被擊敗了！\n"); 
        game_phase = MENU; 
        for (int i = 0; i < 3; ++i) { 
            menu_buttons[i].is_hovered = false; 
        }
        // 如果做restart鈕，這裡要改
        return; // 戰鬥結束，提前返回
    }

    // 檢查所有 Boss 是否被擊敗
    bool all_bosses_defeated_this_round = true; 
    for (int i = 0; i < MAX_BOSSES; ++i) { 
        if (bosses[i].is_alive) { 
            all_bosses_defeated_this_round = false; 
            break; 
        }
    }

    if (all_bosses_defeated_this_round && MAX_BOSSES > 0) { // 勝利情況
        int money_earned = 500 * MAX_BOSSES; // 獲得金錢的邏輯
        player.money += money_earned; 

        current_day++;
        day_time = 1; // 重設為早上
        game_phase = GROWTH;

        // 重設逃脫門狀態
        escape_gate.is_active = false;
        escape_gate.is_counting_down = false;
        escape_gate.countdown_frames = 0;

        // 重設 UI 按鈕狀態
        for (int i = 0; i < 3; ++i) { // 選單按鈕
            menu_buttons[i].is_hovered = false;
        }

        for (int i = 0; i < MAX_GROWTH_BUTTONS; ++i) { // 成長按鈕
            growth_buttons[i].is_hovered = false;
        }
        
        snprintf(growth_message, sizeof(growth_message), "勝利！獲得了 %d 金幣", money_earned);
        growth_message_timer = 180; // 顯示訊息約 3 秒（假設 60 FPS）
    }
}

void battle_manager_handle_input(ALLEGRO_EVENT* ev, bool keys[]) {
    if (keys[ALLEGRO_KEY_B] && !last_key_b_state){
        is_backpack_open = !is_backpack_open; // 切換背包狀態
        init_backpack(); // 初始化背包內容
    }
    if (keys[ALLEGRO_KEY_ESCAPE] && !last_key_esc_state && !is_backpack_open) {
        is_pause = !is_pause; 
    }
    last_key_b_state = keys[ALLEGRO_KEY_B]; // 更新 B 鍵狀態
    last_key_esc_state = keys[ALLEGRO_KEY_ESCAPE]; // 更新 B 鍵狀態

    // 傳遞輸入給玩家的輸入處理器
    if (is_backpack_open) {
        if (keys[ALLEGRO_KEY_ESCAPE]) {
            return; // 不處理 ESC，直接結束這次函式呼叫
        }
        handle_backpack_input(*ev); // 處理背包輸入
    } 
    else if (is_pause) {
            return;
    }
    else{
        player_handle_input(&player, ev, keys);
    }
}

void battle_manager_update(void) {
    if(!is_backpack_open && !is_pause) {
        // 更新遊戲實體
        update_player_character();
        update_player_skill();
        update_player_knife();

        for (int i = 0; i < MAX_BOSSES; ++i) {
            if (bosses[i].is_alive) {
                update_boss_character(&bosses[i]);
            }
        }

        update_active_projectiles(); 
        update_game_camera();  
        update_escape_gate(); 
        update_effects();

        // 管理整體戰鬥狀態（勝利/失敗條件等）
        manage_battle_state();

        // 戰鬥計時
        battle_time += battle_speed_multiplier;
    }
    else if(is_backpack_open){
        // 如果背包開啟，則更新背包內容
        update_backpack();
    }
    else{
        return;
    }

}

void battle_manager_render(void) {
    al_clear_to_color(al_map_rgb(10, 10, 10)); // 清除畫面

    // 繪製地板
    render_battle_floor(get_battle_floor_texture(), camera_x, camera_y);

    // 繪製逃脫門
    render_escape_gate(font);

    // 繪製戰鬥特效(背景)
    render_effects_back();

    // 繪製玩家
    player_render(&player, camera_x, camera_y);

    // 繪製 Boss
    for (int i = 0; i < MAX_BOSSES; ++i) {
        if (bosses[i].is_alive) {
            boss_render(&bosses[i], camera_x, camera_y);
        }
    }

    // 繪製投射物
    render_active_projectiles();

    // 繪製玩家近戰攻擊
    player_attack_render_knife(&player_knife, camera_x, camera_y);

    // 繪製閃電技能效果
    float player_render_screen_x = SCREEN_WIDTH / 2.0f;
    float player_render_screen_y = SCREEN_HEIGHT / 2.0f;
    if (player.learned_skills[SKILL_LIGHTNING_BOLT].cooldown_timers == PLAYER_LIGHTNING_SKILL_COOLDOWN - 1) {
        for (int i = 0; i < MAX_BOSSES; ++i) {
            if (bosses[i].is_alive) {
                float dist = calculate_distance_between_points(player.x, player.y, bosses[i].x, bosses[i].y);
                if (dist < PLAYER_LIGHTNING_RANGE) {
                    float boss_screen_x = bosses[i].x - camera_x;
                    float boss_screen_y = bosses[i].y - camera_y;
                    al_draw_line(player_render_screen_x, player_render_screen_y, boss_screen_x, boss_screen_y, al_map_rgb(255, 255, 0), 3.0f);
                }
            }
        }
    }

    // 繪製戰鬥特效(前景)
    render_effects_front();
    
    // 繪製戰鬥 UI（玩家座標、技能冷卻、操作說明等）
    if (font) { // 確保字型已載入
        al_draw_textf(font, al_map_rgb(220, 220, 220), 10, 10, 0, "玩家世界座標: (%.0f, %.0f)", player.x, player.y);

        int ui_skill_text_y_start = SCREEN_HEIGHT - 140;
        const char* skill_key_hints[] = {"", "K", "L", "U", "I", "O"}; // SKILL_NONE 是 0
        const char* skill_display_names[] = {"", "水彈", "冰錐", "閃電", "治療", "火球"};

        for (int skill_id_enum_val = 0; skill_id_enum_val <= MAX_PLAYER_SKILLS; ++skill_id_enum_val) {
            if (player.learned_skills[skill_id_enum_val].type != SKILL_NONE) {
                char skill_status_text[60];
                if (player.learned_skills[skill_id_enum_val].cooldown_timers > 0) {
                    sprintf(skill_status_text, "%s(%s): 冷卻 %ds", skill_display_names[skill_id_enum_val], skill_key_hints[skill_id_enum_val],
                           player.learned_skills[skill_id_enum_val].cooldown_timers / FPS + 1);
                } else {
                    sprintf(skill_status_text, "%s(%s): 就緒", skill_display_names[skill_id_enum_val], skill_key_hints[skill_id_enum_val]);
                }
                ALLEGRO_COLOR text_color = (player.learned_skills[skill_id_enum_val].cooldown_timers > 0)
                                         ? al_map_rgb(150, 150, 150)
                                         : al_map_rgb(150, 255, 150);
                al_draw_text(font, text_color, 10, ui_skill_text_y_start + (skill_id_enum_val - 1) * 20, 0, skill_status_text);
            }
        }
        if(is_backpack_open){
            render_backpack(); // 繪製背包內容
        } else {
            al_draw_text(font, al_map_rgb(200, 200, 200), 10, ui_skill_text_y_start + 100, 0, "按 B 打開背包");
        }

        // al_draw_text(font, al_map_rgb(200, 200, 200), SCREEN_WIDTH - 10, 10, ALLEGRO_ALIGN_RIGHT, "操作說明:");
        // al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 35, ALLEGRO_ALIGN_RIGHT, "Left click: 攻擊");
        // al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 55, ALLEGRO_ALIGN_RIGHT, "K: 水彈 | L: 冰錐");
        // al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 75, ALLEGRO_ALIGN_RIGHT, "U: 閃電 | I: 治療");
        // al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 95, ALLEGRO_ALIGN_RIGHT, "O: 火球 | ESC: 選單");
        // al_draw_textf(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 115, ALLEGRO_ALIGN_RIGHT, "速度倍率: %.1fx", battle_speed_multiplier); //測試用
    }
    
        
}
