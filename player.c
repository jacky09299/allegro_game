#include "player.h"
#include "globals.h"   // For player, player_knife, bosses, camera_x, camera_y, font
#include "config.h"    // For various player and skill constants
#include "projectile.h"// For spawn_projectile
#include "utils.h"     // For calculate_distance_between_points, get_knife_path_point
#include "player_attack.h" // For player attack functions
#include "asset_manager.h" // For load_bitmap_once
#include <stdio.h>     // For printf, fprintf, stderr
#include <math.h>      // For cos, sin, fmin, atan2, sqrtf
#include <allegro5/allegro_primitives.h> // For al_draw_filled_circle, al_draw_line
#include <allegro5/allegro.h> // For ALLEGRO_EVENT (already in player.h but good for .c too)


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
    player.strength = 10;
    player.magic = 15;
    player.speed = PLAYER_SPEED;
    player.money = 0;
    player.x = 0.0f;
    player.y = 0.0f;
    player.v_x = 0.0f;
    player.v_y = 0.0f;
    player.facing_angle = 0.0f;
    player.normal_attack_cooldown_timer = 0;

    for (int i = 0; i < MAX_PLAYER_SKILLS; ++i) {
        player.learned_skills[i] = SKILL_NONE;
        player.skill_cooldown_timers[i] = 0;
    }
    player.learned_skills[SKILL_WATER_ATTACK] = SKILL_WATER_ATTACK;
    player.learned_skills[SKILL_ICE_SHARD] = SKILL_ICE_SHARD;
    player.learned_skills[SKILL_LIGHTNING_BOLT] = SKILL_LIGHTNING_BOLT;
    player.learned_skills[SKILL_HEAL] = SKILL_HEAL;
    player.learned_skills[SKILL_FIREBALL] = SKILL_FIREBALL;
    for (int i = 0; i < NUM_ITEMS; ++i) {
        player.item_quantities[i] = 0;
    }
    // Ensure sprite is initially NULL if not loaded or if struct is zero-initialized elsewhere
    // player.sprite = NULL; // This line is redundant if player is a global zero-initialized struct,
                             // and load_bitmap_once is called correctly above.
                             // If player is stack or heap allocated without zeroing, then explicit NULL is good.
                             // Given it's a global `player` object, it's likely zero-initialized.
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
}

/**
 * 更新玩家角色的狀態。
 */
void update_player_character() {
    player.x += player.v_x; 
    player.y += player.v_y; 

    for (int i = 0; i < MAX_PLAYER_SKILLS; ++i) {
        if (player.skill_cooldown_timers[i] > 0) {
            player.skill_cooldown_timers[i]--; 
        }
    }

    if (player.normal_attack_cooldown_timer > 0) {
        player.normal_attack_cooldown_timer--;
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
            p->v_x = (p->v_x / magnitude) * p->speed;
            p->v_y = (p->v_y / magnitude) * p->speed;
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
                case ALLEGRO_KEY_K: player_use_water_attack(); break;
                case ALLEGRO_KEY_L: player_use_ice_shard(); break;
                case ALLEGRO_KEY_U: player_use_lightning_bolt(); break;
                case ALLEGRO_KEY_I: player_use_heal(); break;
                case ALLEGRO_KEY_O: player_use_fireball(); break;
                case ALLEGRO_KEY_ESCAPE:
                    game_phase = MENU; // Global interaction
                    for (int i = 0; i < 3; ++i) { // Assumes 3 menu_buttons, global interaction
                        menu_buttons[i].is_hovered = false;
                    }
                    break;
            }
        } else if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && ev->mouse.button == 1) {
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
