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
#include "player_skill_select.h" // For player skill selection functions

/**
 * 玩家執行防禦。
 */
void player_defense_start() {
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
 * 初始化玩家的屬性。
 */
void init_player() { // Changed return type to void, modifies global player
    player.sprite = load_bitmap_once("assets/image/player.png");
    if (!player.sprite) {
        fprintf(stderr, "Failed to load player sprite 'assets/image/player.png'.\n");
    }
    player.hp = 10000;
    player.max_hp = 10000;
    player.mp = 100;
    player.max_mp = 100;
    player.strength = 10;
    player.magic = 15;
    player.max_speed = PLAYER_SPEED;
    player.money = 0;
    player.x = 0.0f;
    player.y = 0.0f;
    player.v_x = 0.0f;
    player.v_y = 0.0f;
    player.facing_angle = 0.0f;
    player.normal_attack_cooldown_timer = 0;

    for (int i = 0; i < MAX_PLAYER_SKILLS; ++i) {
        player.learned_skills[i].type = SKILL_NONE;
        player.learned_skills[i].cooldown_timers = 0;
        player.learned_skills[i].duration_timers = 0;
    }
    player.learned_skills[0].type = SKILL_WATER_ATTACK;
    player.learned_skills[1].type = SKILL_ICE_SHARD;
    player.learned_skills[2].type = SKILL_LIGHTNING_BOLT;
    player.learned_skills[3].type = SKILL_HEAL;
    player.learned_skills[4].type = SKILL_FIREBALL;
    for (int i = 0; i < NUM_ITEMS; ++i) {
        player.item_quantities[i] = 0;
    }
    // Ensure sprite is initially NULL if not loaded or if struct is zero-initialized elsewhere
    // player.sprite = NULL; // This line is redundant if player is a global zero-initialized struct,
                             // and load_bitmap_once is called correctly above.
                             // If player is stack or heap allocated without zeroing, then explicit NULL is good.
                             // Given it's a global `player` object, it's likely zero-initialized.
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
    float player_text_y_offset = (p->sprite ? PLAYER_TARGET_HEIGHT / 2.0f : PLAYER_SPRITE_SIZE) + 5;
    if (font) { // Check if font is loaded
        al_draw_textf(font, al_map_rgb(255, 255, 255), player_screen_x, player_screen_y - player_text_y_offset - 20, ALLEGRO_ALIGN_CENTER, "玩家 (力:%d 魔:%d)", p->strength, p->magic);
        al_draw_textf(font, al_map_rgb(180, 255, 180), player_screen_x, player_screen_y - player_text_y_offset, ALLEGRO_ALIGN_CENTER, "HP: %d/%d", p->hp, p->max_hp);
    }

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

    if(player.state == STATE_STUN) {
        player.speed = 0;
    }

    if (player.state == STATE_DEFENSE){
        if(player.mp <= 0) {
            player.state = STATE_NORMAL;
        }
        player.defense_timer += battle_speed_multiplier;
        spawn_warning_circle(player.x, player.y, 35, 
                            al_map_rgba(0, 0, 150, 200) , 2);
    }
    // 緩速狀態
    if(player.effect_timers[STATE_SLOWNESS] > 0 && player.state != STATE_STUN) {
        if(player.speed > player.max_speed * 0.2f) player.speed = player.max_speed * 0.2f;
        if(player.effect_timers[STATE_SLOWNESS] == 1) player.speed = player.max_speed;
    }
    // 緩速特效
    if(player.effect_timers[STATE_SLOWNESS] > 0 && player.effect_timers[STATE_SLOWNESS]%60 == 0){
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
                case ALLEGRO_KEY_Q: player_switch_skill(); break;
                case ALLEGRO_KEY_R: player_use_selected_skill(); break;
                case ALLEGRO_KEY_EQUALS: battle_speed_multiplier += 0.1f; break;  //測試用
                case ALLEGRO_KEY_MINUS: 
                    if (battle_speed_multiplier >= 0.1f)  battle_speed_multiplier -= 0.1f;  //測試用
                    break;
                case ALLEGRO_KEY_SPACE: player_defense_start(); break;
            }
        } else if (ev->type == ALLEGRO_EVENT_KEY_UP) {
            switch (ev->keyboard.keycode) {
                case ALLEGRO_KEY_SPACE: player_defense_end(); break;
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
