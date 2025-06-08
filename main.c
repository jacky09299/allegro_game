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

#include "include/config.h"
#include "include/types.h"
#include "include/globals.h"
#include "include/player.h"
#include "include/boss.h"
#include "include/projectile.h"
#include "include/game_state.h"
#include "include/battle_manager.h"
#include "include/escape_gate.h"
#include "include/floor.h" // Added for init_floor
#include "include/asset_manager.h" // Added for asset_manager functions
#include "include/player_attack.h" // Added
#include "include/minigame1.h"
#include "include/minigame2.h"
#include "include/lottery.h"
#include "include/backpack.h"
#include "include/tutorial_page.h"
#include "include/player_skill_select.h" // For player skill management

/**
 * 初始化遊戲所需的 Allegro 系統、資源和遊戲物件。
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

    font = al_load_ttf_font("assets/font/JasonHandwriting3.ttf", 30, 0); 

    event_queue = al_create_event_queue();
    timer = al_create_timer(1.0 / FPS); 

    al_register_event_source(event_queue, al_get_display_event_source(display));    
    al_register_event_source(event_queue, al_get_keyboard_event_source());   
    al_register_event_source(event_queue, al_get_mouse_event_source());      
    al_register_event_source(event_queue, al_get_timer_event_source(timer));     

    for (int i = 0; i < ALLEGRO_KEY_MAX; i++) keys[i] = false;

    // load_game_assets(); // This will be removed as per instructions
    
    init_asset_manager(); // New call
    init_floor();         // New call
    init_player();                
    init_player_knife();
    init_bosses_by_archetype();            
    init_projectiles();                    
    init_menu_buttons();                   
    init_growth_buttons();
    init_escape_gate();
    init_tutorial_page();
    game_phase = MENU;                     

    al_start_timer(timer); 
}

/**
 * 關閉遊戲系統並釋放已分配的資源。
 */
void shutdown_game_systems_and_assets() {
    // destroy_game_assets(); // This will be removed as per instructions
    shutdown_asset_manager(); // New call

    al_destroy_font(font);
    al_destroy_timer(timer);
    al_destroy_event_queue(event_queue);
    al_destroy_display(display);

    al_shutdown_font_addon();
    al_shutdown_ttf_addon();
    al_shutdown_primitives_addon();
    al_shutdown_image_addon();
}


/**
 * 遊戲主函數。
 */
int main() {
    init_game_systems_and_assets(); 
    bool game_is_running = true;    
    bool needs_redraw = true;       
    init_minigame2();

    while (game_is_running && game_phase != EXIT) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev); 

        if (ev.type == ALLEGRO_EVENT_KEY_DOWN) { //按鍵列表中被按下的按鍵設為true
            keys[ev.keyboard.keycode] = true;
        } else if (ev.type == ALLEGRO_EVENT_KEY_UP) { //按鍵列表中被按下的按鍵設為false
            keys[ev.keyboard.keycode] = false;
        }

        //每 1/FPS 秒執行一次
        if (ev.timer.source == timer) { 
            update_minigame2();
            needs_redraw = true; 
            if (game_phase == BATTLE) {
                // battle_manager_update() now calls update_player_knife() internally.
                // battle_manager_handle_input() handles movement based on `keys` array.
                battle_manager_handle_input(&ev, keys); // Handles continuous key states for movement
                battle_manager_update(); 
            }
            if (game_phase == MINIGAME1) {
                update_minigame1();
            }
            if (game_phase == MINIGAME2) {
                //update_minigame2();
            }
            if (game_phase == LOTTERY) {
                update_lottery();
            }
            if (game_phase == BACKPACK) {
                update_backpack();
            }
        } 
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) { //按視窗叉叉，關閉遊戲
            game_is_running = false; 
        } 
        // For other event types (mouse, non-timer key events), dispatch to phase-specific handlers
        else { 
            switch (game_phase) {
                case MENU: handle_main_menu_input(ev); break;
                case GROWTH: handle_growth_screen_input(ev); break;
                case BATTLE: 
                    battle_manager_handle_input(&ev, keys); // Process discrete input like aiming, actions
                    break;
                case MINIGAME1: handle_minigame1_input(ev); break;
                case MINIGAME2: handle_minigame2_input(ev); break;
                case LOTTERY: handle_lottery_input(ev); break;
                case BACKPACK: handle_backpack_input(ev); break;
                case TUTORIAL: handle_tutorial_page_input(ev); break;
                case EQUIPMENT: 
                    handle_player_skill_select_input(&ev); // Handle skill selection input
                    break;
                default: break;
            }
        }

        if (needs_redraw && al_is_event_queue_empty(event_queue)) {
            needs_redraw = false; 
            switch (game_phase) {
                case MENU: render_main_menu(); break;
                case GROWTH: render_growth_screen(); break;
                case BATTLE: 
                    battle_manager_render(); // Single call for all battle rendering
                    break;
                case MINIGAME1: render_minigame1(); break;
                case MINIGAME2: render_minigame2(); break;
                case LOTTERY: render_lottery(); break;
                case BACKPACK: render_backpack(); break;
                case TUTORIAL: render_tutorial_page(); break;
                case EQUIPMENT: 
                    render_player_skill_select(); // Render skill selection UI
                    break;
                default: break;
            }
            al_flip_display(); 
        }
    }

    shutdown_game_systems_and_assets(); 
    return 0;
}