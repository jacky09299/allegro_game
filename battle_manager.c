#include "battle_manager.h"
#include "globals.h"
#include "player.h"
#include "boss.h"
#include "player_attack.h"
#include "projectile.h"
#include "floor.h"
#include "escape_gate.h"
#include "config.h"  // Included for completeness, many constants come via globals.h
#include "types.h"   // Included for completeness, many types come via globals.h
#include "utils.h"   // For calculate_distance_between_points

#include <stdio.h>   // For sprintf, snprintf, printf
#include <allegro5/allegro_primitives.h> // For al_draw_filled_circle, al_draw_line, al_clear_to_color etc.
#include <allegro5/allegro_color.h> // For al_map_rgb (though often included with primitives)

// Note: allegro_event.h is included in battle_manager.h

void start_new_battle(void) {
    // Initialize player for battle
    player.hp = player.max_hp; // Assuming player is global from globals.h
    player.x = 0.0f;           // Using 0.0f as PLAYER_START_X/Y are not defined
    player.y = 0.0f;           // Using 0.0f

    init_player_knife();      // From player.c/h
    init_bosses_by_archetype(); // From boss.c/h
    init_projectiles();       // From projectile.c/h
    init_escape_gate();       // From escape_gate.c/h

    // Initialize camera position
    // Assuming camera_x and camera_y are global variables from globals.h
    camera_x = player.x - SCREEN_WIDTH / 2.0f;
    camera_y = player.y - SCREEN_HEIGHT / 2.0f;
}

void manage_battle_state(void) {
    // Logic copied from main.c's BATTLE phase (after updates)

    // Player death check
    if (player.hp <= 0) { 
        printf("遊戲結束 - 你被擊敗了！\n"); // Or use a more sophisticated message system
        game_phase = MENU; 
        for (int i = 0; i < 3; ++i) { // Assuming 3 menu buttons from context (globals.h defines menu_buttons[3])
            menu_buttons[i].is_hovered = false; 
        }
        // Potentially reset other states as needed upon death
        return; // Exit early as battle is over
    }

    // All bosses defeated check
    bool all_bosses_defeated_this_round = true; 
    for (int i = 0; i < MAX_BOSSES; ++i) { // MAX_BOSSES from config.h, accessible via globals.h
        if (bosses[i].is_alive) { 
            all_bosses_defeated_this_round = false; 
            break; 
        }
    }

    if (all_bosses_defeated_this_round && MAX_BOSSES > 0) { // Battle won
        int money_earned = 500 * MAX_BOSSES; // Example earning logic
        player.money += money_earned; 

        current_day++;
        day_time = 1; // Reset to morning/first period
        game_phase = GROWTH;

        // Reset escape gate
        escape_gate.is_active = false;
        escape_gate.is_counting_down = false;
        escape_gate.countdown_frames = 0;

        // Reset UI button states
        for (int i = 0; i < 3; ++i) { // Menu buttons
            menu_buttons[i].is_hovered = false;
        }
        // MAX_GROWTH_BUTTONS is from config.h, accessible via globals.h
        for (int i = 0; i < MAX_GROWTH_BUTTONS; ++i) { // Growth buttons
            growth_buttons[i].is_hovered = false;
        }
        
        snprintf(growth_message, sizeof(growth_message), "勝利！獲得了 %d 金幣", money_earned);
        growth_message_timer = 180; // Display message for ~3 seconds (assuming 60 FPS)
    }
}

void battle_manager_handle_input(ALLEGRO_EVENT* ev, bool keys[]) {
    // Pass input to player's input handler
    // The global 'player' is used here.
    player_handle_input(&player, ev, keys);
    
    // If there were other battle-wide input events to handle (e.g., pausing the battle),
    // they could be processed here. For now, it's mainly player input.
}

void battle_manager_update(void) {
    // Update game entities
    update_player_character(); // Uses global player
    update_player_knife();     // Uses global player_knife

    for (int i = 0; i < MAX_BOSSES; ++i) {
        if (bosses[i].is_alive) {
            update_boss_character(&bosses[i]);
        }
    }

    update_active_projectiles(); // Uses global projectiles array
    update_game_camera();      // Uses global player and camera_x, camera_y
    update_escape_gate();      // Uses global escape_gate

    // Manage overall battle state (win/loss conditions, etc.)
    manage_battle_state(); // Existing function in battle_manager.c
}

void battle_manager_render(void) {
    al_clear_to_color(al_map_rgb(10, 10, 10)); // Clear screen

    // Render floor
    // get_battle_floor_texture() from floor.c, camera_x, camera_y are global
    render_battle_floor(get_battle_floor_texture(), camera_x, camera_y);

    // Render player
    // player_render from player.c, player and camera_x, camera_y are global
    player_render(&player, camera_x, camera_y);

    // Render bosses
    for (int i = 0; i < MAX_BOSSES; ++i) {
        if (bosses[i].is_alive) {
            // boss_render from boss.c, camera_x, camera_y are global
            boss_render(&bosses[i], camera_x, camera_y);
        }
    }

    // Render projectiles (Logic moved from graphics.c)
    // projectiles, camera_x, camera_y are global
    // MAX_PROJECTILES, PROJECTILE_RADIUS from config.h or projectile.h
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].active) {
            float proj_screen_x = projectiles[i].x - camera_x;
            float proj_screen_y = projectiles[i].y - camera_y;
            ALLEGRO_COLOR proj_color;
            switch (projectiles[i].type) {
                case PROJ_TYPE_WATER: proj_color = al_map_rgb(0, 150, 255); break;
                case PROJ_TYPE_FIRE: proj_color = al_map_rgb(255, 100, 0); break;
                case PROJ_TYPE_ICE: proj_color = al_map_rgb(150, 255, 255); break;
                case PROJ_TYPE_PLAYER_FIREBALL: proj_color = al_map_rgb(255, 50, 50); break;
                case PROJ_TYPE_GENERIC:
                default: proj_color = al_map_rgb(200, 200, 200); break;
            }
            al_draw_filled_circle(proj_screen_x, proj_screen_y, PROJECTILE_RADIUS, proj_color);
            if (projectiles[i].type == PROJ_TYPE_ICE) {
                al_draw_circle(proj_screen_x, proj_screen_y, PROJECTILE_RADIUS + 2, al_map_rgb(255, 255, 255), 1.0f);
            } else if (projectiles[i].type == PROJ_TYPE_PLAYER_FIREBALL || projectiles[i].type == PROJ_TYPE_FIRE) {
                al_draw_circle(proj_screen_x, proj_screen_y, PROJECTILE_RADIUS + 1, al_map_rgb(255, 200, 0), 1.0f);
            }
        }
    }

    // Render player knife attack
    // player_attack_render_knife from player_attack.c, player_knife, camera_x, camera_y are global
    player_attack_render_knife(&player_knife, camera_x, camera_y);

    // Render Lightning Bolt Effect (Logic moved from graphics.c)
    // player, bosses, camera_x, camera_y are global
    // SCREEN_WIDTH, SCREEN_HEIGHT, PLAYER_LIGHTNING_SKILL_COOLDOWN, PLAYER_LIGHTNING_RANGE, MAX_BOSSES from config/globals
    // calculate_distance_between_points from utils.h
    // This effect renders for one frame when the cooldown timer is at a specific value.
    // Note: player_screen_x/y are fixed because player is centered.
    float player_render_screen_x = SCREEN_WIDTH / 2.0f;
    float player_render_screen_y = SCREEN_HEIGHT / 2.0f;
    if (player.skill_cooldown_timers[SKILL_LIGHTNING_BOLT] == PLAYER_LIGHTNING_SKILL_COOLDOWN - 1) {
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
    
    // Render Battle UI (player coordinates, skill cooldowns, help text - logic moved from graphics.c)
    // font, player, SCREEN_HEIGHT, SCREEN_WIDTH, FPS from globals/config
    if (font) { // Ensure font is loaded
        al_draw_textf(font, al_map_rgb(220, 220, 220), 10, 10, 0, "玩家世界座標: (%.0f, %.0f)", player.x, player.y);

        int ui_skill_text_y_start = SCREEN_HEIGHT - 140;
        const char* skill_key_hints[] = {"", "K", "L", "U", "I", "O"}; // SKILL_NONE is 0
        const char* skill_display_names[] = {"", "水彈", "冰錐", "閃電", "治療", "火球"};

        for (PlayerSkillIdentifier skill_id_enum_val = SKILL_WATER_ATTACK; skill_id_enum_val <= SKILL_FIREBALL; ++skill_id_enum_val) {
            if (player.learned_skills[skill_id_enum_val] != SKILL_NONE) {
                char skill_status_text[60];
                if (player.skill_cooldown_timers[skill_id_enum_val] > 0) {
                    sprintf(skill_status_text, "%s(%s): 冷卻 %ds", skill_display_names[skill_id_enum_val], skill_key_hints[skill_id_enum_val],
                           player.skill_cooldown_timers[skill_id_enum_val] / FPS + 1);
                } else {
                    sprintf(skill_status_text, "%s(%s): 就緒", skill_display_names[skill_id_enum_val], skill_key_hints[skill_id_enum_val]);
                }
                ALLEGRO_COLOR text_color = (player.skill_cooldown_timers[skill_id_enum_val] > 0)
                                         ? al_map_rgb(150, 150, 150)
                                         : al_map_rgb(150, 255, 150);
                al_draw_text(font, text_color, 10, ui_skill_text_y_start + (skill_id_enum_val - 1) * 20, 0, skill_status_text);
            }
        }

        al_draw_text(font, al_map_rgb(200, 200, 200), SCREEN_WIDTH - 10, 10, ALLEGRO_ALIGN_RIGHT, "操作說明:");
        al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 35, ALLEGRO_ALIGN_RIGHT, "J: 攻擊");
        al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 55, ALLEGRO_ALIGN_RIGHT, "K: 水彈 | L: 冰錐");
        al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 75, ALLEGRO_ALIGN_RIGHT, "U: 閃電 | I: 治療");
        al_draw_text(font, al_map_rgb(180, 180, 180), SCREEN_WIDTH - 10, 95, ALLEGRO_ALIGN_RIGHT, "O: 火球 | ESC: 選單");
    }
    
    // Render escape gate
    // render_escape_gate from escape_gate.c, font is global
    render_escape_gate(font); 
    // Note: render_escape_gate itself might do al_flip_display or other things.
    // For now, we assume it just draws. The final al_flip_display will be in main.c's loop.
}
