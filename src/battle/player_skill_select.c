#include "player_skill_select.h"
#include "player_attack.h"
#include"globals.h"

#include "player.h"
#include "config.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>

#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include "asset_manager.h" // For load_bitmap_once
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
int player_skill_group[5] // 玩家技能組
static ALLEGRO_BITMAP* skill_img[MAX_PLAYER_SKILLS]; // 技能圖示陣列
// ...existing code...
const char* skill_icon_filenames[MAX_PLAYER_SKILLS] = {
    NULL, // SKILL_NONE
    "assets/image/skill1.png",
    "assets/image/skill2.png",
    "assets/image/skill3.png",
    "assets/image/skill4.png",
    "assets/image/skill5.png",
    "assets/image/skill6.png",
};

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


/*
typedef enum {
    SKILL_NONE,             // 無技能
    SKILL_WATER_ATTACK,     // 水彈攻擊
    SKILL_ICE_SHARD,        // 冰錐術
    SKILL_LIGHTNING_BOLT,   // 閃電鏈
    SKILL_HEAL,             // 治療術
    SKILL_FIREBALL,         // 火球術
    SKILL_PREFECT_DEFENSE,   // 完美防禦
    BOSS_SKILL_1,  // Boss 特殊技能 1
    BOSS_SKILL_2,  // Boss 特殊技能 2
    BOSS_SKILL_3,  // Boss 特殊技能 3
    BOSS_RANGE_PRIMARY, // Boss 遠程基礎技能
    BOSS_MELEE_PRIMARY  // Boss 近戰基礎技能
} SkillIdentifier;

這是在type.h中定義的技能總表1~6是玩家技能庫
*/

void init_skill_select() {
    // 初始化技能圖示陣列
    //i=0是SKILL_NONE,不需要載入圖示
    for (int i = 1; i < MAX_PLAYER_SKILLS; ++i) {
        if (skill_icon_filenames[i] != NULL) {
            skill_img[i] = load_bitmap_once(skill_icon_filenames[i]);
        } else {
            skill_img[i] = NULL;
        }
    }
}

// 輸入處理
void handle_skill_select_input(ALLEGRO_EVENT* ev) {
}


// 更新 (目前無內容，但保留結構)
void update_skill_select() {
    // 可以在此處加入一些動畫效果
}

// 渲染
void render_skill_select() {

}