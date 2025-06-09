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
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>

#include "include/config.h"
#include "include/types.h"
#include "include/globals.h"
#include "include/player.h"
#include "include/boss.h"
#include "include/projectile.h"
#include "include/game_state.h"
#include "include/battle_manager.h"
#include "include/escape_gate.h"
#include "include/floor.h"
#include "include/asset_manager.h"
#include "include/player_attack.h"
#include "include/minigame1.h"
#include "include/minigame2.h"
#include "include/lottery.h"
#include "include/backpack.h"
#include "include/tutorial_page.h"
#include "include/player_skill_select.h"
#include "include/save_load.h"

// --- 背景音樂相關全域變數 ---
ALLEGRO_SAMPLE* bgm_sample = NULL;
ALLEGRO_SAMPLE_INSTANCE* bgm_instance = NULL;

#define BATTLE_BGM_COUNT 4
#define THEME_BGM_COUNT 2

// 戰鬥音樂播放清單
const char* battle_bgm_files[BATTLE_BGM_COUNT] = {
    "assets/sound/battle_song1.ogg",
    "assets/sound/battle1.ogg",
    "assets/sound/battle_song2.ogg",
    "assets/sound/battle2.ogg"
};
// 主題音樂播放清單
const char* theme_bgm_files[THEME_BGM_COUNT] = {
    "assets/sound/theme1.ogg",
    "assets/sound/theme2.ogg"
};

// 用於記錄各播放清單目前播放到哪一首
int battle_bgm_index = 0;
int theme_bgm_index = 0;

// 用於記錄每首音樂的播放進度（單位：秒）
double battle_bgm_pos[BATTLE_BGM_COUNT] = {0.0};
double theme_bgm_pos[THEME_BGM_COUNT] = {0.0};

// 記錄當前正在播放的音樂類型 (0: 戰鬥, 1: 主題, -1: 無)
int current_bgm_type = -1;

/**
 * @brief 從指定秒數開始播放背景音樂。
 *
 * @param filename 音樂檔案路徑。
 * @param pos_sec 要開始播放的時間點（秒）。
 */
void play_bgm_with_pos(const char* filename, double pos_sec) {
    // 停止並銷毀舊的實例和樣本，防止記憶體洩漏
    if (bgm_instance) {
        al_stop_sample_instance(bgm_instance);
        al_destroy_sample_instance(bgm_instance);
        bgm_instance = NULL;
    }
    if (bgm_sample) {
        al_destroy_sample(bgm_sample);
        bgm_sample = NULL;
    }

    // 載入新的音樂檔
    bgm_sample = al_load_sample(filename);
    if (!bgm_sample) {
        fprintf(stderr, "錯誤: 無法載入音樂檔案: %s\n", filename);
        return;
    }

    bgm_instance = al_create_sample_instance(bgm_sample);
    if (!bgm_instance) {
        fprintf(stderr, "錯誤: 無法創建音樂實例: %s\n", filename);
        al_destroy_sample(bgm_sample);
        bgm_sample = NULL;
        return;
    }

    al_attach_sample_instance_to_mixer(bgm_instance, al_get_default_mixer());
    // 設定為只播放一次，這樣我們才能在播放完畢後手動控制下一首
    al_set_sample_instance_playmode(bgm_instance, ALLEGRO_PLAYMODE_ONCE);
    al_set_sample_instance_gain(bgm_instance, 1.0f);

    // 【關鍵】實現從指定位置繼續播放
    // Allegro 5 支援跳轉播放！我們需要將秒數轉換回 Allegro 的樣本位置。
    // 公式: 樣本位置 = 秒數 * 取樣頻率
    if (pos_sec > 0.0) {
        unsigned int start_pos_samples = (unsigned int)(pos_sec * al_get_sample_frequency(bgm_sample));
        al_set_sample_instance_position(bgm_instance, start_pos_samples);
    }
    
    // 開始播放
    al_play_sample_instance(bgm_instance);
}

/**
 * @brief 停止當前播放的背景音樂。
 */
void stop_bgm() {
    if (bgm_instance) {
        al_stop_sample_instance(bgm_instance);
    }
}

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
    if (!display) {
        fprintf(stderr, "FATAL: Failed to create display!\n");
        exit(1); // Exit if display creation fails
    }
    al_set_window_title(display, "遊戲"); 

    font = al_load_ttf_font("assets/font/JasonHandwriting3.ttf", 30, 0); 
    if (!font) {
        fprintf(stderr, "FATAL: Failed to load font assets/font/JasonHandwriting3.ttf!\n");
        // Depending on how critical font is, might exit or just warn
        // For now, let's assume it's critical for basic UI
        al_destroy_display(display);
        exit(1);
    }

    event_queue = al_create_event_queue();
    if (!event_queue) {
        fprintf(stderr, "FATAL: Failed to create event_queue!\n");
        al_destroy_font(font);
        al_destroy_display(display);
        exit(1);
    }
    timer = al_create_timer(1.0 / FPS); 
    if (!timer) {
        fprintf(stderr, "FATAL: Failed to create timer!\n");
        al_destroy_event_queue(event_queue);
        al_destroy_font(font);
        al_destroy_display(display);
        exit(1);
    }

    al_register_event_source(event_queue, al_get_display_event_source(display));    
    al_register_event_source(event_queue, al_get_keyboard_event_source());   
    al_register_event_source(event_queue, al_get_mouse_event_source());      
    al_register_event_source(event_queue, al_get_timer_event_source(timer));     
    
    al_install_audio();
    al_init_acodec_addon();
    al_reserve_samples(10); // 增加保留樣本數以防萬一

    for (int i = 0; i < ALLEGRO_KEY_MAX; i++) keys[i] = false;

    init_asset_manager();
    init_floor();
    init_player();                
    init_player_knife();
    init_bosses_by_archetype();            
    init_projectiles();                    
    init_menu_buttons();                   
    init_growth_buttons();
    init_escape_gate();
    init_tutorial_page();
    init_player_skill_select();
    game_phase = MENU;                     

    al_start_timer(timer); 
}

/**
 * 關閉遊戲系統並釋放已分配的資源。
 */
void shutdown_game_systems_and_assets() {
    shutdown_asset_manager();

    al_destroy_font(font);
    al_destroy_timer(timer);
    al_destroy_event_queue(event_queue);
    al_destroy_display(display);

    al_shutdown_font_addon();
    al_shutdown_ttf_addon();
    al_shutdown_primitives_addon();
    al_shutdown_image_addon();

    // 確保音樂資源被完全釋放
    if (bgm_instance) {
        al_destroy_sample_instance(bgm_instance);
        bgm_instance = NULL;
    }
    if (bgm_sample) {
        al_destroy_sample(bgm_sample);
        bgm_sample = NULL;
    }
    al_uninstall_audio();
}

/**
 * 遊戲主函數。
 */
int main() {
    // The rest of the original main function will not be reached in this test setup.
    GamePhase last_phase = -1; // 用來偵測遊戲狀態是否改變
    init_game_systems_and_assets(); 

    // Attempt to load game progress
    if (load_game_progress("savegame.dat")) {
        // Game state is loaded. Now, relink assets that were nullified during save.
        // Assumes init_asset_manager() (called within init_game_systems_and_assets)
        // has already populated the global bitmap pointers.

        // Relink player sprite
        // TODO: Ensure player_sprite_asset is the correct one if player can have multiple sprites.
        // For now, assume player_sprite_asset is the general sprite.
        if (player_sprite_asset) { // Check if asset manager loaded it
             player.sprite = player_sprite_asset;
        } else {
            fprintf(stderr, "Warning: player_sprite_asset is NULL after init_asset_manager.\n");
        }

        // Relink boss sprites
        for (int i = 0; i < MAX_BOSSES; i++) {
            // Only relink for active/valid bosses, though bosses array is saved/loaded fully.
            // The sprite_asset field in the loaded bosses[i] struct is NULL.
            if (bosses[i].max_hp > 0) { // A simple check if the boss slot was ever used/initialized
                switch (bosses[i].archetype) {
                    case BOSS_TYPE_TANK:
                        if (boss_archetype_tank_sprite_asset)
                            bosses[i].sprite_asset = boss_archetype_tank_sprite_asset;
                        break;
                    case BOSS_TYPE_SKILLFUL:
                        if (boss_archetype_skillful_sprite_asset)
                            bosses[i].sprite_asset = boss_archetype_skillful_sprite_asset;
                        break;
                    case BOSS_TYPE_BERSERKER:
                        if (boss_archetype_berserker_sprite_asset)
                            bosses[i].sprite_asset = boss_archetype_berserker_sprite_asset;
                        break;
                    default:
                        // If new archetypes are added and not handled here, their sprites won't be relinked.
                        // Consider a warning or default sprite.
                        bosses[i].sprite_asset = NULL; // Explicitly NULL if no match
                        break;
                }
                if (bosses[i].sprite_asset == NULL) {
                    // Optional: Log if a known archetype didn't get a sprite linked
                    // fprintf(stderr, "Warning: Boss %d (type %d) sprite_asset is NULL after load relink.\n", i, bosses[i].archetype);
                }
            }
        }

        // For backpack items, their 'item.image' field was nulled.
        // The rendering code for the backpack should use item ID or item.image_path
        // to get the correct bitmap from the asset manager at render time.
        // Thus, no explicit relinking is done here for player_backpack[i].item.image.

        // Note: If game_phase was loaded, the game will resume in that phase.
        // Ensure all necessary initializations for that phase are compatible with a loaded state.
        // For example, if loaded into BATTLE, battle manager might need re-init with loaded data.
        // The current design re-initializes most things in init_game_systems_and_assets,
        // then load_game_progress overwrites data. Music BGM will also restart based on the loaded game_phase.

        fprintf(stdout, "Game progress loaded successfully from savegame.dat.\n");

    } else {
        // No save file found, or error during load. Game starts fresh.
        // The state from init_game_systems_and_assets() is used.
        fprintf(stdout, "No save file found or error during load. Starting new game.\n");
    }

    bool game_is_running = true;    
    bool needs_redraw = true;       
    init_minigame2();

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
            
            // 各遊戲狀態的邏輯更新
            switch(game_phase) {
                case BATTLE:
                    battle_manager_handle_input(&ev, keys);
                    battle_manager_update();
                    break;
                // case MINIGAME1: update_minigame1(); break; // Temporarily disabled for save/load testing
                case MINIGAME1: break; // Fallthrough or handle differently if needed
                case MINIGAME2: update_minigame2(); break;
                case LOTTERY: update_lottery(); break;
                case BACKPACK: update_backpack(); break;
                default: break; // 其他狀態可能沒有持續更新的邏輯
            }
           
            // 【音樂處理】檢查當前歌曲是否自然播放完畢，若是則自動換下一首
            if (bgm_instance && !al_get_sample_instance_playing(bgm_instance)) {
                if (current_bgm_type == 0) { // 戰鬥音樂播完了
                    battle_bgm_pos[battle_bgm_index] = 0.0; // 將已播完的歌曲位置重設為0
                    battle_bgm_index = (battle_bgm_index + 1) % BATTLE_BGM_COUNT;
                    // 下一首歌永遠從頭開始播放
                    play_bgm_with_pos(battle_bgm_files[battle_bgm_index], 0.0);
                } else if (current_bgm_type == 1) { // 主題音樂播完了
                    theme_bgm_pos[theme_bgm_index] = 0.0; // 將已播完的歌曲位置重設為0
                    theme_bgm_index = (theme_bgm_index + 1) % THEME_BGM_COUNT;
                    // 下一首歌永遠從頭開始播放
                    play_bgm_with_pos(theme_bgm_files[theme_bgm_index], 0.0);
                }
            }
        } 
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            printf("Saving game progress before exiting...\n"); // For debugging
            save_game_progress("savegame.dat");
            game_is_running = false; 
        } 
        else { // 處理滑鼠點擊、單次按鍵等非持續性事件
            switch (game_phase) {
                case MENU: handle_main_menu_input(ev); break;
                case GROWTH: handle_growth_screen_input(ev); break;
                case BATTLE: battle_manager_handle_input(&ev, keys); break;
                // case MINIGAME1: handle_minigame1_input(ev); break; // Temporarily disabled for save/load testing
                case MINIGAME1: break; // Fallthrough or handle differently if needed
                case MINIGAME2: handle_minigame2_input(ev); break;
                case LOTTERY: handle_lottery_input(ev); break;
                case BACKPACK: handle_backpack_input(ev); break;
                case TUTORIAL: handle_tutorial_page_input(ev); break;
                case EQUIPMENT: handle_player_skill_select_input(&ev); break;
                default: break;
            }
        }

        // 【音樂處理】當遊戲狀態改變時，處理音樂切換
        if (game_phase != last_phase) {
            // 根據新的遊戲狀態，決定目標音樂類型
            int target_bgm_type = -1; // 預設無音樂
            switch (game_phase) {
                case BATTLE:
                    target_bgm_type = 0; // 戰鬥音樂
                    break;
                case MINIGAME1:
                    target_bgm_type = -1; // 小遊戲1無音樂
                    break;
                // 所有其他狀態都使用主題音樂
                case MENU:
                case GROWTH:
                case MINIGAME2:
                case LOTTERY:
                case BACKPACK:
                case TUTORIAL:
                case EQUIPMENT:
                default:
                    target_bgm_type = 1; // 主題音樂
                    break;
            }

            // 只有當目標音樂類型與目前不同時，才需要執行切換動作
            if (target_bgm_type != current_bgm_type) {
                // 1. 切換前，先儲存目前正在播放音樂的進度
                if (bgm_instance && bgm_sample && al_get_sample_instance_playing(bgm_instance)) {
                    // 公式: 秒數 = 當前樣本位置 / 取樣頻率
                    double current_pos_sec = (double)al_get_sample_instance_position(bgm_instance) / al_get_sample_frequency(bgm_sample);
                    if (current_bgm_type == 0) {
                        battle_bgm_pos[battle_bgm_index] = current_pos_sec;
                    } else if (current_bgm_type == 1) {
                        theme_bgm_pos[theme_bgm_index] = current_pos_sec;
                    }
                }
                
                // 2. 更新當前的音樂類型
                current_bgm_type = target_bgm_type;

                // 3. 根據新的類型，播放對應的音樂（並從上次儲存的位置繼續）
                if (current_bgm_type == 0) { // 切換到戰鬥音樂
                    play_bgm_with_pos(battle_bgm_files[battle_bgm_index], battle_bgm_pos[battle_bgm_index]);
                } else if (current_bgm_type == 1) { // 切換到主題音樂
                    play_bgm_with_pos(theme_bgm_files[theme_bgm_index], theme_bgm_pos[theme_bgm_index]);
                } else { // 切換到無音樂
                    stop_bgm();
                }
            }
            
            last_phase = game_phase; // 更新最後的狀態，防止此區塊重複執行
        }


        if (needs_redraw && al_is_event_queue_empty(event_queue)) {
            needs_redraw = false; 
            switch (game_phase) {
                case MENU: render_main_menu(); break;
                case GROWTH: render_growth_screen(); break;
                case BATTLE: battle_manager_render(); break;
                // case MINIGAME1: render_minigame1(); break; // Temporarily disabled for save/load testing
                case MINIGAME1: break; // Fallthrough or handle differently if needed
                case MINIGAME2: render_minigame2(); break;
                case LOTTERY: render_lottery(); break;
                case BACKPACK: render_backpack(); break;
                case TUTORIAL: render_tutorial_page(); break;
                case EQUIPMENT: render_player_skill_select(); break;
                default: break;
            }
            al_flip_display(); 
        }
    }

    shutdown_game_systems_and_assets(); 
    return 0;
}