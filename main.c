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

#define VIDEO_FRAME_COUNT 238
#define VIDEO_FPS 30.0
ALLEGRO_BITMAP* video_frames[VIDEO_FRAME_COUNT];

void load_video_frames() {
    char filename[256];
    for (int i = 0; i < VIDEO_FRAME_COUNT; ++i) {
        snprintf(filename, sizeof(filename), "assets/video_frames/frame_%03d.png", i);
        video_frames[i] = al_load_bitmap(filename);
        if (!video_frames[i]) {
            fprintf(stderr, "無法載入 %s\n", filename);
        }
    }
}

void play_video_frames() {
    double start_time = al_get_time();
    while (al_get_time() - start_time < VIDEO_FRAME_COUNT / VIDEO_FPS) {
        double elapsed = al_get_time() - start_time;
        int frame_idx = (int)(elapsed * VIDEO_FPS);
        if (frame_idx >= VIDEO_FRAME_COUNT) break;

        if (video_frames[frame_idx]) {
            al_draw_scaled_bitmap(
                video_frames[frame_idx],
                0, 0,
                al_get_bitmap_width(video_frames[frame_idx]),
                al_get_bitmap_height(video_frames[frame_idx]),
                0, 0,
                SCREEN_WIDTH, SCREEN_HEIGHT,
                0
            );
            al_flip_display();
        }
    }
}

// 新版本：一點進去就開始播，沒有 Loading 畫面
void play_intro_video_from_frames() {
    char filename[256];
    double frame_duration = 1.0 / VIDEO_FPS*2;
    ALLEGRO_BITMAP* current_frame = NULL;

    // 直接進入播放迴圈
    for (int i = 0; i < VIDEO_FRAME_COUNT; i+=3) {
        double frame_start_time = al_get_time();

        // 處理事件，允許用戶在播放時關閉視窗或按 ESC 跳過
        ALLEGRO_EVENT ev;
        // 使用 al_get_next_event 而非 al_wait_for_event，這樣不會阻塞播放流程
        while (al_get_next_event(event_queue, &ev)) {
            if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                game_phase = EXIT;
                return; // 提前結束函式
            }
            if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                printf("影片已跳過。\n");
                return; // 提前結束函式
            }
        }
        // 清空事件佇列後，清空畫面，準備繪製新的一幀
        al_clear_to_color(al_map_rgb(0, 0, 0));

        // 1. 載入當前這一幀的圖片
        snprintf(filename, sizeof(filename), "assets/video_frames/frame_%03d.png", i);
        current_frame = al_load_bitmap(filename);

        if (current_frame) {
            // 2. 繪製這一幀
            al_draw_scaled_bitmap(
                current_frame,
                0, 0,
                al_get_bitmap_width(current_frame),
                al_get_bitmap_height(current_frame),
                0, 0,
                SCREEN_WIDTH, SCREEN_HEIGHT,
                0
            );
            
            // 3. 銷毀點陣圖，釋放記憶體
            al_destroy_bitmap(current_frame);
        } else {
            fprintf(stderr, "警告: 無法載入影格 %s，將顯示黑畫面。\n", filename);
            // 如果載入失敗，因為前面已經 clear_to_color，這一幀會是黑的
        }

        // 4. 顯示畫面
        al_flip_display();

        // 5. 控制播放速度
        double time_spent = al_get_time() - frame_start_time;
        if (time_spent < frame_duration) {
            al_rest(frame_duration - time_spent);
        }
    }
}


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
 * 停止當前播放的背景音樂。
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
    al_set_window_title(display, "遊戲"); 

    font = al_load_ttf_font("assets/font/JasonHandwriting3.ttf", 30, 0); 

    event_queue = al_create_event_queue();
    timer = al_create_timer(1.0 / FPS); 

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
    GamePhase last_phase = -1; // 用來偵測遊戲狀態是否改變
    init_game_systems_and_assets(); 
    
    // 載入影片畫面
    play_intro_video_from_frames();

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
                case MINIGAME1: update_minigame1(); break;
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
            game_is_running = false; 
        } 
        else { // 處理滑鼠點擊、單次按鍵等非持續性事件
            switch (game_phase) {
                case MENU: handle_main_menu_input(ev); break;
                case GROWTH: handle_growth_screen_input(ev); break;
                case BATTLE: battle_manager_handle_input(&ev, keys); break;
                case MINIGAME1: handle_minigame1_input(ev); break;
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
                case MINIGAME1: render_minigame1(); break;
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