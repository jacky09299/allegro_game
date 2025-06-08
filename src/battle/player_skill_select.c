#include "player_skill_select.h"
#include "player_attack.h"
#include"globals.h"

#include "player.h"
#include "config.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>

#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
/*
player.c
標頭檔加入-> #include "player_skill_select.h"
player_handle_input()中
"""
if (ev) { // Check if ev is not NULL
        if (ev->type == ALLEGRO_EVENT_KEY_DOWN) {
            switch (ev->keyboard.keycode) {
"""
中，改成->
switch (ev->keyboard.keycode) {
case ALLEGRO_KEY_Q: player_switch_skill(); break;
case ALLEGRO_KEY_R: player_use_selected_skill(); break;

*/

// 全域變數：目前選擇的技能索引
static int selected_skill_index = 0;

void player_switch_skill() {
    // 循環切換下一個已學會的技能
    int start = selected_skill_index;
    int idx = (start + 1) % MAX_PLAYER_SKILLS;
    while (idx != start) {
        if (player.learned_skills[idx].type != SKILL_NONE) {
            selected_skill_index = idx;
            return;
        }
        idx = (idx + 1) % MAX_PLAYER_SKILLS;
    }
    // 若只有一個技能，保持不變
}

void player_use_selected_skill() {
    switch (player.learned_skills[selected_skill_index].type) {
        case SKILL_WATER_ATTACK: player_use_water_attack(); break;
        case SKILL_ICE_SHARD: player_use_ice_shard(); break;
        case SKILL_LIGHTNING_BOLT: player_use_lightning_bolt(); break;
        case SKILL_HEAL: player_use_heal(); break;
        case SKILL_FIREBALL: player_use_fireball(); break;
        default: break;
    }
}

int get_selected_skill_index() {
    return selected_skill_index;
}




void init_skill_select() {
}

// 輸入處理
void handle_skill_select_input(ALLEGRO_EVENT* ev) {
    if (ev->type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GAME_PLAYING; // 返回主遊戲
        }
    }

    // --- 滑鼠點擊事件 ---
    if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->mouse.button & 1) { // 左鍵按下
            // 檢查是否點擊技能組
            for (int i = 0; i < SKILL_SLOT_MAX; i++) {
                float slot_x = SKILL_SET_X + i * (SLOT_SIZE + SLOT_PADDING);
                float slot_y = SKILL_SET_Y;
                if (ev->mouse.x >= slot_x && ev->mouse.x <= slot_x + SLOT_SIZE &&
                    ev->mouse.y >= slot_y && ev->mouse.y <= slot_y + SLOT_SIZE)
                {
                    if (equipped_skills[i] != SKILL_NONE) {
                        is_dragging = true;
                        dragged_skill_slot_idx = i;
                        dragged_skill_id = equipped_skills[i];
                        equipped_skills[i] = SKILL_NONE; // 從原位暫時移除
                        drag_mouse_x = ev->mouse.x;
                        drag_mouse_y = ev->mouse.y;
                        return; // 開始拖曳
                    }
                }
            }

            // 檢查是否點擊技能庫
            for (int i = 0; i < num_owned_skills; i++) {
                int row = i / LIBRARY_COLS;
                int col = i % LIBRARY_COLS;
                float lib_x = LIBRARY_X + col * (SLOT_SIZE + SLOT_PADDING);
                float lib_y = LIBRARY_Y + row * (SLOT_SIZE + SLOT_PADDING);

                if (ev->mouse.x >= lib_x && ev->mouse.x <= lib_x + SLOT_SIZE &&
                    ev->mouse.y >= lib_y && ev->mouse.y <= lib_y + SLOT_SIZE)
                {
                    // 點擊技能庫 -> 加入技能組
                    SkillType skill_to_add = player_owned_skills[i];
                    // 尋找技能組中的第一個空格
                    int first_empty_slot = -1;
                    for (int j = 0; j < SKILL_SLOT_MAX; j++) {
                        if (equipped_skills[j] == SKILL_NONE) {
                            first_empty_slot = j;
                            break;
                        }
                    }
                    if (first_empty_slot != -1) {
                        equipped_skills[first_empty_slot] = skill_to_add;
                    }
                    return;
                }
            }
        }
    }

    // --- 滑鼠移動事件 ---
    if (ev->type == ALLEGRO_EVENT_MOUSE_AXES) {
        if (is_dragging) {
            drag_mouse_x = ev->mouse.x;
            drag_mouse_y = ev->mouse.y;
        }
    }

    // --- 滑鼠釋放事件 ---
    if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
        if (is_dragging) {
            // 檢查釋放位置是否在技能組的某個格子內
            bool dropped_on_slot = false;
            for (int i = 0; i < SKILL_SLOT_MAX; i++) {
                float slot_x = SKILL_SET_X + i * (SLOT_SIZE + SLOT_PADDING);
                float slot_y = SKILL_SET_Y;
                if (ev->mouse.x >= slot_x && ev->mouse.x <= slot_x + SLOT_SIZE &&
                    ev->mouse.y >= slot_y && ev->mouse.y <= slot_y + SLOT_SIZE)
                {
                    // 拖放到格子 i
                    dropped_on_slot = true;
                    // 交換技能
                    SkillType temp_skill = equipped_skills[i]; // 該格子原有的技能
                    equipped_skills[i] = dragged_skill_id;      // 放入拖曳的技能
                    equipped_skills[dragged_skill_slot_idx] = temp_skill; // 原格子放入被交換的技能
                    break;
                }
            }

            // 如果沒有拖放到任何有效格子，則放回原位
            if (!dropped_on_slot) {
                equipped_skills[dragged_skill_slot_idx] = dragged_skill_id;
            }

            // 結束拖曳
            is_dragging = false;
            dragged_skill_slot_idx = -1;
            dragged_skill_id = SKILL_NONE;
        }
    }
}


// 更新 (目前無內容，但保留結構)
void update_skill_select() {
    // 可以在此處加入一些動畫效果
}

// 渲染
void render_skill_select() {

}