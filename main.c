#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>

#include "config.h"
#include "types.h"
#include "globals.h"
#include "player.h"
#include "boss.h"
#include "projectile.h"
#include "game_state.h"
#include "graphics.h"
#include "minigame_flower.h" // Added for the flower minigame
// No need to include utils.h here if only other modules use it

/**
 * @brief 初始化遊戲所需的 Allegro 系統、資源和遊戲物件。
 */
void init_game_systems_and_assets() {
    al_init(); 
    srand(time(NULL)); 

    al_init_font_addon();      
    al_init_ttf_addon();       
    al_init_primitives_addon();
    al_init_image_addon();     
    al_install_keyboard();     
    al_install_mouse();        

    display = al_create_display(SCREEN_WIDTH, SCREEN_HEIGHT);
    al_set_window_title(display, "遊戲"); 

    font = al_load_ttf_font("assets/font/JasonHandwriting3.ttf", 20, 0); 
    if (!font) {
        fprintf(stderr, "載入 TTF 字體失敗。將使用內建字體。\n");
        font = al_create_builtin_font(); 
        if (!font) {fprintf(stderr, "創建內建字體失敗。\n"); exit(-1);}
    }

    event_queue = al_create_event_queue();
    timer = al_create_timer(1.0 / FPS); 

    al_register_event_source(event_queue, al_get_display_event_source(display));    
    al_register_event_source(event_queue, al_get_keyboard_event_source());   
    al_register_event_source(event_queue, al_get_mouse_event_source());      
    al_register_event_source(event_queue, al_get_timer_event_source(timer));     

    for (int i = 0; i < ALLEGRO_KEY_MAX; i++) keys[i] = false;

    load_game_assets(); // Moved asset loading to graphics.c
    
    init_player();                
    init_player_knife();
    init_bosses_by_archetype();            
    init_projectiles();                    
    init_menu_buttons();                   
    init_growth_buttons();
    game_phase = MENU;                     

    al_start_timer(timer); 
}

/**
 * @brief 關閉遊戲系統並釋放已分配的資源。
 */
void shutdown_game_systems_and_assets() {
    destroy_game_assets(); // Moved asset destruction to graphics.c

    al_destroy_font(font);
    al_destroy_timer(timer);
    al_destroy_event_queue(event_queue);
    al_destroy_display(display);

    al_shutdown_font_addon();
    al_shutdown_ttf_addon();
    al_shutdown_primitives_addon();
    al_shutdown_image_addon();
    // al_uninstall_keyboard(); // Optional, Allegro handles shutdown
    // al_uninstall_mouse();    // Optional
}


/**
 * @brief 遊戲主函數。
 */
int main() {
    init_game_systems_and_assets(); 
    bool game_is_running = true;    
    bool needs_redraw = true;       

    while (game_is_running && game_phase != EXIT) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev); 

        if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
            keys[ev.keyboard.keycode] = true;
        } else if (ev.type == ALLEGRO_EVENT_KEY_UP) {
            keys[ev.keyboard.keycode] = false;
        }

        if (ev.timer.source == timer) {
            needs_redraw = true; 
            if (game_phase == BATTLE) { 
                player.v_x = 0; player.v_y = 0; 
                if (keys[ALLEGRO_KEY_W] || keys[ALLEGRO_KEY_UP]) player.v_y -= 1.0f;    
                if (keys[ALLEGRO_KEY_S] || keys[ALLEGRO_KEY_DOWN]) player.v_y += 1.0f;  
                if (keys[ALLEGRO_KEY_A] || keys[ALLEGRO_KEY_LEFT]) player.v_x -= 1.0f;  
                if (keys[ALLEGRO_KEY_D] || keys[ALLEGRO_KEY_RIGHT]) player.v_x += 1.0f; 

                if (player.v_x != 0 || player.v_y != 0) {
                    float magnitude = sqrtf(player.v_x * player.v_x + player.v_y * player.v_y); 
                    player.v_x = (player.v_x / magnitude) * player.speed; 
                    player.v_y = (player.v_y / magnitude) * player.speed;
                }
                
                if (player.normal_attack_cooldown_timer > 0) {
                    player.normal_attack_cooldown_timer--;
                }

                update_player_character(); 
                update_player_knife();
                for (int i = 0; i < MAX_BOSSES; ++i) { 
                    update_boss_character(&bosses[i]); 
                }
                update_active_projectiles(); 
                update_game_camera();        

                if (player.hp <= 0) { 
                    printf("遊戲結束 - 你被擊敗了！\n");
                    game_phase = MENU; 
                    for (int i = 0; i < 3; ++i) { menu_buttons[i].is_hovered = false; }
                }
                bool all_bosses_defeated_this_round = true; 
                for (int i = 0; i < MAX_BOSSES; ++i) {
                    if (bosses[i].is_alive) { 
                        all_bosses_defeated_this_round = false; 
                        break; 
                    }
                }
                if (all_bosses_defeated_this_round && MAX_BOSSES > 0) { // Ensure there were bosses to defeat
                    printf("勝利 - 所有 Boss 都被擊敗了！\n");
                    int money_earned = 500 * MAX_BOSSES; 
                    player.money += money_earned; 
                    printf("玩家獲得了 %d 金幣！現在總共有 %d 金幣。\n", money_earned, player.money);
                    game_phase = MENU; 
                    for (int i = 0; i < 3; ++i) { menu_buttons[i].is_hovered = false; }
                }
            }
            // Update logic for MINIGAME_FLOWER
            if (game_phase == MINIGAME_FLOWER) {
                update_minigame_flower();
            }
        } 
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) { 
            game_is_running = false; 
        } 
        else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) { 
             if (game_phase == BATTLE) { 
                float player_screen_center_x = SCREEN_WIDTH / 2.0f;
                float player_screen_center_y = SCREEN_HEIGHT / 2.0f;
                player.facing_angle = atan2(ev.mouse.y - player_screen_center_y, ev.mouse.x - player_screen_center_x); 
            } else if (game_phase == MENU) { 
                 handle_main_menu_input(ev);
            } else if (game_phase == GROWTH) {
                 handle_growth_screen_input(ev);
            } else if (game_phase == MINIGAME_FLOWER) { // Added input handling for MINIGAME_FLOWER
                handle_minigame_flower_input(ev);
            }
        }
        else { 
            switch (game_phase) {
                case MENU: handle_main_menu_input(ev); break;
                case GROWTH: handle_growth_screen_input(ev); break;
                case BATTLE: handle_battle_scene_input_actions(ev); break;
                case MINIGAME_FLOWER: // Added event handling for MINIGAME_FLOWER
                    handle_minigame_flower_input(ev);
                    break;
                default: break; // Should not happen
            }
        }

        if (needs_redraw && al_is_event_queue_empty(event_queue)) {
            needs_redraw = false; 
            switch (game_phase) {
                case MENU: render_main_menu(); break;
                case GROWTH: render_growth_screen(); break;
                case BATTLE: render_battle_scene(); break;
                case MINIGAME_FLOWER: // Added rendering for MINIGAME_FLOWER
                    render_minigame_flower();
                    break;
                default: break; // Should not happen
            }
            al_flip_display(); 
        }
    }

    shutdown_game_systems_and_assets(); 
    return 0;
}