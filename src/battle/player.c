#include "player.h"
#include "globals.h"   // For player, player_knife, bosses, camera_x, camera_y, font
#include "config.h"    // For various player and skill constants
#include "projectile.h"// For spawn_projectile
#include "utils.h"     // For calculate_distance_between_points, get_knife_path_point
#include "player_attack.h" // For player attack functions
#include "asset_manager.h" // For load_bitmap_once
#include "effect.h"     // For pawn_warning_circle, spawn_floating_text
#include <stdio.h>     // For printf, fprintf, stderr
#include <math.h>      // For cos, sin, fmin, atan2, sqrtf
#include <allegro5/allegro_primitives.h> // For al_draw_filled_circle, al_draw_line
#include <allegro5/allegro.h> // For ALLEGRO_EVENT (already in player.h but good for .c too)
#include "player_skill_select.h"
/**
 * 玩家執行金手指。
 */
void play_golden_finger() {
    player.hp = 1000;
    player.mp = 1000;

}
/**
 * 玩家執行防禦。
 */
void player_defense_start() {
    printf("玩家開始防禦\n");
    // if (!player.learned_skills[SKILL_PREFECT_DEFENSE].learned) return;

    if (player.state == STATE_STUN || player.mp <= 0) {
        player.state = STATE_NORMAL;
        return;
    }
    player.state = STATE_DEFENSE;
}
void player_defense_end() {
    player.state = STATE_NORMAL;
    player.defense_timer = -1;
}
/**
 * 玩家施放技能。
 */
void player_skill_start() {
    // char text[16];
    if (player.state == STATE_STUN || player.state == STATE_CHARGING) {
        spawn_floating_text(player.x + 15, player.y - 25, "無法使用", al_map_rgb(255, 200, 200));
        return;
    }
    if(player.mp <= 0) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
    }
    player.state = STATE_USING_SKILL;
    switch (player.current_skill_index)
    {
    case SKILL_NONE:
        player.state = STATE_NORMAL;
        break;
    case SKILL_ELEMENT_BALL:
        player_use_element_ball();
        player.state = STATE_NORMAL;
        break;
    case SKILL_ELEMENT_DASH:
        player_use_element_dash();
        break;
    case SKILL_FOCUS:
        player_start_focus();
        break;
    case SKILL_CHARGE_BEAM:
        player_start_charge_beam();
        break;
    case SKILL_ARCANE_ORB:
        player_use_arcane_orb();
        player.state = STATE_NORMAL;
        break;
    case SKILL_RUNE_IMPLOSION:
        player_use_rune_implosion();
        player.state = STATE_NORMAL;
        break;
    case SKILL_LIGHTNING_BOLT:
        player_use_lightning_bolt();
        player.state = STATE_NORMAL;
        break;
    case SKILL_HEAL:
        player_use_heal();
        player.state = STATE_NORMAL;
        break;
    case SKILL_REFLECT_BARRIER:
        player_use_reflect_barrier();
        player.state = STATE_NORMAL;
        break;
    case SKILL_BIG_HEAL:
        player_start_big_heal();
        break;
    case SKILL_ELEMENTAL_BLAST:
        player_use_elemental_blast();
        player.state = STATE_NORMAL;
        break;
    case SKILL_ELEMENTAL_SCATTER:
        player_use_elemental_scatter();
        player.state = STATE_NORMAL;
        break;
    case SKILL_RAPID_SHOOT:
        player_use_rapid_element_balls();
        player.state = STATE_NORMAL;
        break;
    default:
        player.state = STATE_NORMAL;
        break;
    }
}
void player_skill_end() {
    if (player.state == STATE_STUN || player.mp <= 0) {
        player.state = STATE_NORMAL;
        return;
    }
    player.state = STATE_USING_SKILL;
    switch (player.current_skill_index)
    {
    case SKILL_FOCUS:
        player_end_focus();
        player.state = STATE_NORMAL;
        break;
    case SKILL_CHARGE_BEAM:
        player_end_charge_beam();
        player.state = STATE_NORMAL;
        break;
    case SKILL_BIG_HEAL:
        player_end_big_heal();
        break;
    default:
        player.state = STATE_NORMAL;
        break;
    }
}
/**
 * 玩家切換技能。
 */

 void player_change_skill() {
    // 找出目前技能在裝備欄的位置
    int current_pos = -1;
    for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
        if (player_skill_group[i] == player.current_skill_index) {
            current_pos = i;
            break;
        }
    }
    if (current_pos == -1) return;

    int next_pos = current_pos;
    do {
        next_pos = (next_pos + 1) % MAX_EQUIPPED_SKILLS;
        // 跳過 SKILL_NONE 和 SKILL_PREFECT_DEFENSE
        if (player_skill_group[next_pos] != SKILL_NONE &&
            player_skill_group[next_pos] != SKILL_PREFECT_DEFENSE
            && player_skill_group[next_pos] != SKILL_ELEMENTAL_COUNTER) {
            player.current_skill_index = player_skill_group[next_pos];
            break;
        }
    } while (next_pos != current_pos);
}

 /**
 * 初始化玩家的屬性。
 */
bool is_init_at_beginning = false; // Flag to check if player is initialized at the beginning

void init_player() { // Changed return type to void, modifies global player
    player.sprite = load_bitmap_once("assets/image/player.png");
    if (!player.sprite) {
        fprintf(stderr, "Failed to load player sprite 'assets/image/player.png'.\n");
    }
    if (!is_init_at_beginning){
        player.money = 0;
        player.max_hp = 10000;
        player.max_mp = 1000;
        player.strength = 10;
        player.magic = 15;
        player.max_speed = PLAYER_SPEED;
    }
    player.hp = player.max_hp;
    player.mp = player.max_mp;
    player.x = 0.0f;
    player.y = 0.0f;
    player.v_x = 0.0f;
    player.v_y = 0.0f;
    player.facing_angle = 0.0f;
    player.normal_attack_cooldown_timer = 0;
    
    player.current_skill_index = SKILL_NONE;
    for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
        if (player_skill_group[i] != SKILL_NONE && player_skill_group[i] != SKILL_PREFECT_DEFENSE) {
            player.current_skill_index = player_skill_group[i];
            break;
        }
    }
    for (int i = 0; i < MAX_PLAYER_SKILLS; ++i) {
        // player.learned_skills[i].type = SKILL_NONE;
        player.learned_skills[i].cooldown_timers = 0;
        player.learned_skills[i].duration_timers = 0;
        player.learned_skills[i].charge_timers = 0;
        player.learned_skills[i].learned = false; // 保留修改
    }
    for (int i = 0; i < MAX_EQUIPPED_SKILLS; i++){
        if(player_skill_group[i]!=SKILL_NONE){
            player.learned_skills[player_skill_group[i]].learned = true;
        }
    }

    // player.learned_skills[SKILL_LIGHTNING_BOLT].type = SKILL_LIGHTNING_BOLT;
    // player.learned_skills[SKILL_HEAL].type = SKILL_HEAL;
    // player.learned_skills[SKILL_ELEMENT_BALL].type = SKILL_ELEMENT_BALL;
    // player.learned_skills[SKILL_ELEMENT_DASH].type = SKILL_ELEMENT_DASH;
    // player.learned_skills[SKILL_FOCUS].type = SKILL_FOCUS;
    // player.learned_skills[SKILL_RUNE_IMPLOSION].type = SKILL_RUNE_IMPLOSION;
    // player.learned_skills[SKILL_ARCANE_ORB].type = SKILL_ARCANE_ORB;
    // player.learned_skills[SKILL_CHARGE_BEAM].type = SKILL_CHARGE_BEAM;

    for (int i = 0; i < NUM_ITEMS; ++i) {
        player.item_quantities[i] = 0;
    }
    // Ensure sprite is initially NULL if not loaded or if struct is zero-initialized elsewhere
    // player.sprite = NULL; // This line is redundant if player is a global zero-initialized struct,
                             // and load_bitmap_once is called correctly above.
                             // If player is stack or heap allocated without zeroing, then explicit NULL is good.
                             // Given it's a global `player` object, it's likely zero-initialized.
    is_init_at_beginning = true; // Set flag to true after initialization

}

void draw_player_circle_hud() {
    float cx = 100;       // 圓心 x（左下角偏右）
    float cy = SCREEN_HEIGHT - 100; // 圓心 y
    float r_hp = 40;      // 血量圈半徑
    float r_mp = 50;      // 魔力圈半徑

    float hp_ratio = (float)player.hp / player.max_hp;
    float mp_ratio = (float)player.mp / player.max_mp;

    float full_circle = 2 * ALLEGRO_PI;

    // 背景圓圈（灰）
    al_draw_arc(cx, cy, r_hp, 0, full_circle, al_map_rgb(60, 60, 60), 5);
    al_draw_arc(cx, cy, r_mp, 0, full_circle, al_map_rgb(40, 40, 40), 5);

    // 實際值
    al_draw_arc(cx, cy, r_hp, -ALLEGRO_PI / 2, full_circle * hp_ratio, al_map_rgb(200, 50, 50), 5); // HP = 紅
    al_draw_arc(cx, cy, r_mp, -ALLEGRO_PI / 2, full_circle * mp_ratio, al_map_rgb(80, 80, 240), 5); // MP = 藍

    // 文字
    al_draw_textf(font, al_map_rgb(255, 255, 255), cx, cy - 10, ALLEGRO_ALIGN_CENTER, "HP %d", player.hp);
    al_draw_textf(font, al_map_rgb(180, 180, 255), cx, cy + 10, ALLEGRO_ALIGN_CENTER, "MP %d", player.mp);
}

void draw_player_skill_ui() {
    const char* skill_names[] = {
    "無", "閃電鏈", "治癒", "元素彈",
    "元素衝刺", "蓄力槍", "集中", "水晶浮球",
    "符文爆破", "反轉結界", "元素散彈", "元素暴衝",
    "大治療術", "元素連擊", 
    "元素反擊", "完美防禦" 
    };
    int skill_count = sizeof(skill_names) / sizeof(skill_names[0]);
    if(player.current_skill_index > skill_count) return;

    // 當前技能資料
    int current_id = player.current_skill_index;
    SKILL current_skill = player.learned_skills[current_id];
    const char* skill_name = skill_names[current_id];

    char skill_ui_text[64];
    char cooldown_ui_text[64];

    // 準備文字
    sprintf(skill_ui_text, "技能：%s", skill_name);

    if(current_skill.charge_timers > 0) {
        sprintf(cooldown_ui_text, "緒力中...");
    }
    else if (current_skill.cooldown_timers > 0) {
        sprintf(cooldown_ui_text, "冷卻：%d 秒", current_skill.cooldown_timers / FPS + 1);
    } else {
        sprintf(cooldown_ui_text, "就緒！");
    }

    char defense_ui_text[64];
    int defense_cd = player.learned_skills[SKILL_PREFECT_DEFENSE].cooldown_timers;
    if (player.learned_skills[SKILL_PREFECT_DEFENSE].learned) {
        if (defense_cd > 0) {
            sprintf(defense_ui_text, "完美防禦冷卻：%d 秒", defense_cd / FPS + 1);
        } else {
            sprintf(defense_ui_text, "完美防禦就緒！");
        }
        // 顯示在主技能上方
        al_draw_text(font, al_map_rgb(100, 200, 255), SCREEN_WIDTH - 10, SCREEN_HEIGHT - 100, ALLEGRO_ALIGN_RIGHT, defense_ui_text);
    }
    
    // 顯示位置設定（畫面右下角）
    float padding = 10;
    int text_x = SCREEN_WIDTH - padding;
    int text_y = SCREEN_HEIGHT - 50;

    al_draw_filled_rounded_rectangle(
    SCREEN_WIDTH - 220, SCREEN_HEIGHT - 60,
    SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10,
    10, 10, al_map_rgba(0, 0, 0, 160)
    );

    // 顯示文字
    al_draw_text(font, al_map_rgb(255, 255, 0), text_x, text_y, ALLEGRO_ALIGN_RIGHT, skill_ui_text);
    al_draw_text(font, al_map_rgb(200, 255, 200), text_x, text_y + 20, ALLEGRO_ALIGN_RIGHT, cooldown_ui_text);
}

void player_render(Player* p, float camera_x, float camera_y) {
    // Note: camera_x and camera_y are passed but might not be directly used if player is always screen center.
    // However, they are part of the function signature for consistency with other render functions.

    float player_screen_x = SCREEN_WIDTH / 2.0f;
    float player_screen_y = SCREEN_HEIGHT / 2.0f;

    if (p->sprite) { // Use p->sprite now
        float src_w = al_get_bitmap_width(p->sprite);
        float src_h = al_get_bitmap_height(p->sprite);
        if (src_w > 0 && src_h > 0) {
            float scale_x = (float)PLAYER_TARGET_WIDTH / src_w;
            float scale_y = (float)PLAYER_TARGET_HEIGHT / src_h;
            al_draw_scaled_rotated_bitmap(p->sprite,
                                        src_w / 2.0f, src_h / 2.0f,
                                        player_screen_x, player_screen_y,
                                        scale_x, scale_y,
                                        p->facing_angle,
                                        0);
        } else { // Fallback if sprite dimensions are zero
            al_draw_filled_circle(player_screen_x, player_screen_y, PLAYER_SPRITE_SIZE, al_map_rgb(0, 100, 200));
        }
    } else { // Fallback if sprite is not loaded
        al_draw_filled_circle(player_screen_x, player_screen_y, PLAYER_SPRITE_SIZE, al_map_rgb(0, 100, 200));
        // Draw a line to indicate facing direction if no sprite
        float line_end_x = player_screen_x + cos(p->facing_angle) * PLAYER_SPRITE_SIZE * 1.5f;
        float line_end_y = player_screen_y + sin(p->facing_angle) * PLAYER_SPRITE_SIZE * 1.5f;
        al_draw_line(player_screen_x, player_screen_y, line_end_x, line_end_y, al_map_rgb(255, 255, 255), 2.0f);
    }

    // Render player text (HP, stats)
    // Access global `font` variable (ensure it's accessible, e.g. via "globals.h")
    // SCREEN_WIDTH, PLAYER_TARGET_HEIGHT, PLAYER_SPRITE_SIZE are from "config.h"
    // float player_text_y_offset = (p->sprite ? PLAYER_TARGET_HEIGHT / 2.0f : PLAYER_SPRITE_SIZE) + 5;
    // if (font) { // Check if font is loaded
    //     al_draw_textf(font, al_map_rgb(255, 255, 255), player_screen_x, player_screen_y - player_text_y_offset - 20, ALLEGRO_ALIGN_CENTER, "玩家 (力:%d 魔:%d)", p->strength, p->magic);
    //     al_draw_textf(font, al_map_rgb(180, 255, 180), player_screen_x, player_screen_y - player_text_y_offset, ALLEGRO_ALIGN_CENTER, "HP: %d/%d", p->hp, p->max_hp);
    // }

    // 顯示當前技能
    draw_player_skill_ui();

    draw_player_circle_hud();
}

/**
 * 更新玩家角色的狀態。
 */
void update_player_character() {
    player.x += player.v_x; 
    player.y += player.v_y; 

    for (int i = 0; i < MAX_PLAYER_SKILLS; ++i) {
        if (player.learned_skills[i].cooldown_timers > 0) {
            player.learned_skills[i].cooldown_timers -= battle_speed_multiplier;
        }
        if (player.learned_skills[i].duration_timers > 0) {
            player.learned_skills[i].duration_timers -= battle_speed_multiplier;
        }
    }

    for(int i =0; i <MAX_STATE; ++i) {
        player.effect_timers[i] -= battle_speed_multiplier;
    }

    if (player.normal_attack_cooldown_timer > 0) {
        player.normal_attack_cooldown_timer -= battle_speed_multiplier;
    }

    // 暈眩狀態
    if(player.effect_timers[STATE_STUN] > 0) {
        player.state = STATE_STUN;
        player.speed = 0;
        if(player.effect_timers[STATE_STUN] == 1) player.speed = player.max_speed;
        if(player.effect_timers[STATE_STUN] == 1 && player.state == STATE_STUN) player.state = STATE_NORMAL;
    }
    // 暈眩特效
    if(player.effect_timers[STATE_STUN] > 0 && player.effect_timers[STATE_STUN]%18 == 0){
        spawn_floating_text(player.x + 15, player.y - 25, "暈眩", al_map_rgba(200, 100, 255, 250));
    }

    // 防禦狀態
    if (player.state == STATE_DEFENSE){
        if(player.mp <= 0 && player.state != STATE_STUN) {
            player.state = STATE_NORMAL;
        }
        if(player.defense_timer%6 == 0)
        player.mp -= 1 * battle_speed_multiplier;
        player.defense_timer += battle_speed_multiplier;
        spawn_warning_circle(player.x, player.y, 35, 
                            al_map_rgba(0, 0, 150, 200) , 2);
    }
    // 充能狀態
    if (player.state == STATE_CHARGING){
        if(player.mp <= 0 && player.state != STATE_STUN) {
            player.state = STATE_NORMAL;
        }
        if(player.learned_skills[player.current_skill_index].charge_timers < 300 
            && player.learned_skills[player.current_skill_index].charge_timers%6 == 0) {
            player.mp -= battle_speed_multiplier * 2;
        }
        player.learned_skills[player.current_skill_index].charge_timers += battle_speed_multiplier;
        // TODO 更換特效
        float ratio = (float)player.learned_skills[player.current_skill_index].charge_timers / 300;
        if(ratio >= 1) ratio = 1;
        spawn_warning_circle(player.x, player.y, 20 + 20 * ratio, 
                            al_map_rgba(0, 0, 150, 255* ratio) , 2);
    }
    // 緩速狀態
    if(player.effect_timers[STATE_SLOWNESS] > 0 && player.state != STATE_STUN) {
        if(player.speed > player.max_speed * 0.2f) player.speed = player.max_speed * 0.2f;
        if(player.effect_timers[STATE_SLOWNESS] == 1) player.speed = player.max_speed;
    }
    // 緩速特效
    if(player.effect_timers[STATE_SLOWNESS] > 0 && player.effect_timers[STATE_SLOWNESS]%18 == 0){
        spawn_floating_text(player.x + 15, player.y - 25, "緩速", al_map_rgba(0, 200, 255, 250));
    }
}

void player_handle_input(Player* p, ALLEGRO_EVENT* ev, bool keys[]) {
    // Movement Logic (from handle_player_battle_movement_input)
    // This part should ideally only run if ev->type == ALLEGRO_EVENT_TIMER or similar continuous update signal.
    // For now, assume keys[] are updated outside and this is called on timer event.
    p->v_x = 0.0f;
    p->v_y = 0.0f;
    if (keys[ALLEGRO_KEY_W] || keys[ALLEGRO_KEY_UP]) p->v_y -= 1.0f;
    if (keys[ALLEGRO_KEY_S] || keys[ALLEGRO_KEY_DOWN]) p->v_y += 1.0f;
    if (keys[ALLEGRO_KEY_A] || keys[ALLEGRO_KEY_LEFT]) p->v_x -= 1.0f;
    if (keys[ALLEGRO_KEY_D] || keys[ALLEGRO_KEY_RIGHT]) p->v_x += 1.0f;

    if (p->v_x != 0.0f || p->v_y != 0.0f) {
        float magnitude = sqrtf(p->v_x * p->v_x + p->v_y * p->v_y);
        if (magnitude != 0.0f) {
            p->v_x = (p->v_x / magnitude) * p->speed * battle_speed_multiplier ;
            p->v_y = (p->v_y / magnitude) * p->speed * battle_speed_multiplier ;
        }
    }

    // Aiming Logic (from handle_player_battle_aiming_input)
    if (ev && ev->type == ALLEGRO_EVENT_MOUSE_AXES) { // Check if ev is not NULL
        float player_screen_center_x = SCREEN_WIDTH / 2.0f;
        float player_screen_center_y = SCREEN_HEIGHT / 2.0f;
        p->facing_angle = atan2(ev->mouse.y - player_screen_center_y, ev->mouse.x - player_screen_center_x);
    }

    // Action Logic (from handle_player_battle_action_input)
    if (ev) { // Check if ev is not NULL
        if (ev->type == ALLEGRO_EVENT_KEY_DOWN) {
            switch (ev->keyboard.keycode) {
                case ALLEGRO_KEY_J: /* No action defined */ break;
                case ALLEGRO_KEY_K: /* No action defined */ break;
                case ALLEGRO_KEY_L: /* No action defined */ break;
                case ALLEGRO_KEY_U: /* No action defined */ break;
                case ALLEGRO_KEY_I: /* No action defined */ break;
                case ALLEGRO_KEY_O: /* No action defined */ break;
                case ALLEGRO_KEY_E: /* No action defined */ break;
                case ALLEGRO_KEY_Q: player_change_skill(); break;
                case ALLEGRO_KEY_R: player_skill_start(); break;
                case ALLEGRO_KEY_EQUALS: battle_speed_multiplier += 0.1f; break;  //測試用
                case ALLEGRO_KEY_MINUS: 
                    if (battle_speed_multiplier >= 0.1f)  battle_speed_multiplier -= 0.1f;  //測試用
                    break;
                case ALLEGRO_KEY_SPACE: player_defense_start(); break;
            }
        } else if (ev->type == ALLEGRO_EVENT_KEY_UP) {
            switch (ev->keyboard.keycode) {
                case ALLEGRO_KEY_SPACE: player_defense_end(); break;
                case ALLEGRO_KEY_R: player_skill_end(); break;
            }
        }else if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && ev->mouse.button == 1) {
            player_perform_normal_attack();
        }
    }
}

/**
 * 更新遊戲攝影機的位置。
 */
void update_game_camera() {
    camera_x = player.x - SCREEN_WIDTH / 2.0f;  
    camera_y = player.y - SCREEN_HEIGHT / 2.0f; 
}
