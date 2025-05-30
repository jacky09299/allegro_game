#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro.h> // 用於 al_get_time, ALLEGRO_PI
#include <stdio.h>
#include <math.h>   // 用於 fmaxf, fminf, cos, sin, fabs
#include <stdlib.h> // 用於 rand, abs (整數)
#include <string.h> // 用於 memset, memcpy (如果需要)
#include <time.h>


#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include "globals.h" // 假設 font, SCREEN_WIDTH, SCREEN_HEIGHT 等在此定義或 extern
#include "minigame1.h"
#include "types.h"   // 假設 Button, MinigameFlowerPlant 等在此定義

// 音訊錄製常數
#define AUDIO_BUFFER_SIZE (44100 * 2 * 35) // 音訊緩衝區大小 (44.1kHz * 16位元單聲道 * 35秒)
#define AUDIO_SAMPLE_RATE 44100            // 音訊取樣率
#define AUDIO_CHANNELS 1                   // 音訊聲道數
#define AUDIO_BITS_PER_SAMPLE 16           // 每樣本位元數

// 即時回饋常數
#define WAVEFORM_AREA_X 50                                  // 波形顯示區域 X 座標
#define WAVEFORM_AREA_Y (100 + 60)                          // 波形顯示區域 Y 座標 (向下移動 60 像素)
#define WAVEFORM_AREA_WIDTH (SCREEN_WIDTH - 100)            // 波形顯示區域寬度
#define WAVEFORM_AREA_HEIGHT 100                            // 波形顯示區域高度
#define REALTIME_ANALYSIS_WINDOW_MS 50                      // 即時分析窗口毫秒數 (未使用於目前RMS計算)
#define WAVEFORM_DISPLAY_DURATION_SEC 0.5f                  // 波形顯示持續時間 (秒)

// 歌唱驗證常數
#define MIN_RECORDING_DURATION_SECONDS 1.0f                 // 按鈕按下到放開的最短時間
#define REQUIRED_SINGING_PROPORTION (1.0f / 3.0f)           // 歌唱音量達標時間佔按鈕間時間的比例
const double SINGING_TOTAL_TIME_THRESHOLD = 20.0;           // 歌唱總時間 (actual_end - actual_start) 需大於20秒
const double SINGING_DETECTION_MIN_STREAK_DURATION = 0.2;   // 持續0.2秒音量達閾值才算有效歌唱片段

// 相對音量常數
#define NOISE_CALC_WINDOW_SAMPLES (AUDIO_SAMPLE_RATE / 5)   // 噪音計算窗口樣本數 (目前邏輯未使用，保留結構)
#define RMS_ANALYSIS_WINDOW_SAMPLES (AUDIO_SAMPLE_RATE / 20) // 50毫秒窗口，用於目前音量和歌唱檢查
#define SINGING_RMS_THRESHOLD_MULTIPLIER 3.0f               // 目前 RMS 必須是背景 RMS 的 X 倍
#define MIN_ABSOLUTE_RMS_FOR_SINGING 150.0f                 // 被視為歌唱的最低 RMS (防止靜音環境下的小聲誤判為大聲)
#define INITIAL_BACKGROUND_RMS_GUESS 200.0f                 // 背景 RMS 的初始猜測值


// 音訊錄製相關的靜態全域變數
static HWAVEIN hWaveIn = NULL;                              // Windows 音訊輸入裝置控制代碼
static WAVEHDR waveHdr;                                     // Windows 音訊緩衝區標頭
static char* pWaveBuffer = NULL;                            // 指向音訊緩衝區的指標
static double allegro_recording_start_time = 0.0;           // Allegro 開始錄製的時間點 (按下按鈕後，或倒數計時結束後)

static bool isActuallyRecording = false;                    // 是否真的正在錄音 (硬體層面)
static bool displayPleaseSingMessage = false;               // 是否顯示「請唱歌」提示訊息
static float overall_button_to_button_duration = 0.0f;      // 按鈕按下到放開的總時長

// 即時回饋與相對音量相關的靜態全域變數
static float current_recording_elapsed_time_sec = 0.0f;     // 目前錄製已過時間 (秒)
static bool current_volume_is_loud_enough = false;          // 目前音量是否足夠大 (基於相對音量)
static float current_background_rms = INITIAL_BACKGROUND_RMS_GUESS; // 目前背景 RMS

static bool force_audio_too_short_for_test = false;         // 用於除錯，強制音訊過短

static MinigameFlowerPlant flower_plant;                    // 小遊戲花朵植物結構
static Button minigame_buttons[NUM_MINIGAME1_BUTTONS];      // 小遊戲按鈕陣列
static bool seed_planted = false;                           // 種子是否已種下
static bool is_singing = false;                             // 是否處於「唱歌」狀態 (邏輯層面，倒數計時後才為 true)
static const int songs_to_flower = 8;                       // 種出花朵需要的歌曲數量
static bool minigame_srand_called = false;                  // srand 是否已在小遊戲中呼叫過

// 新增的詳細歌唱計時靜態全域變數
static double time_start_button_press = 0.0;                // 使用者點擊「開始唱歌」的確切時間 (倒數後)
static double time_end_button_press = 0.0;                  // 使用者點擊「完成歌唱」的時間
static double actual_singing_start_time = -1.0;             // 音訊首次達到閾值並持續0.2秒的時間點 (該0.2秒的開始)
static double current_actual_singing_end_time = -1.0;       // 持續更新的最新0.2秒歌唱片段的結束時間
static double locked_successful_singing_end_time = -1.0;    // 如果核心需求滿足，此變數鎖定成功的歌唱結束時間
static bool has_achieved_lockable_success = false;          // 標記在錄製期間是否已滿足核心歌唱需求

// 偵測歌唱片段的內部狀態
static bool internal_was_loud_last_frame = false;           // 內部：上一幀音量是否夠大
static double internal_current_loud_streak_began_at_time = 0.0; // 內部：目前大聲片段開始的時間

// 321 倒數動畫相關變數
static bool is_in_countdown_animation = false;              // 是否正在進行321倒數動畫
static double countdown_start_time = 0.0;                   // 倒數動畫開始時間
static int countdown_value = 3;                             // 目前倒數顯示的數字
static bool background_rms_calculation_locked = false;      // 背景音量計算是否已完成並鎖定

// 新增：用於倒數計時的大字型
static ALLEGRO_FONT *large_font = NULL;


static void prepare_audio_recording(void);                  // 準備音訊錄製
static void start_actual_audio_recording(void);             // 開始實際的音訊錄製
static bool stop_actual_audio_recording(void);              // 停止實際的音訊錄製並進行驗證
static void cleanup_audio_recording(void);                  // 清理音訊錄製資源
static float calculate_rms(const short* samples, int count); // 計算 RMS 值


void init_minigame1(void) {
    if (!minigame_srand_called) {
        srand(time(NULL));
        minigame_srand_called = true;
    }
    if (!al_is_system_installed()) {
        if (!al_init()) {
            fprintf(stderr, "錯誤：無法初始化 Allegro 系統。\n");
            // 嚴重錯誤，可能需要退出
            return;
        }
    }

    // 確保字型和TTF插件已初始化 (通常在主程式開始時做一次即可)
    if (!al_is_font_addon_initialized()) {
        if (!al_init_font_addon()) {
            fprintf(stderr, "錯誤：無法初始化字型插件。\n");
            // 處理錯誤，可能需要退出遊戲
        }
    }
    if (!al_is_ttf_addon_initialized()) {
        if (!al_init_ttf_addon()) {
            fprintf(stderr, "錯誤：無法初始化TTF字型插件。\n");
            // 處理錯誤
        }
    }

    force_audio_too_short_for_test = false;

    cleanup_audio_recording(); // 清理任何先前的錄製資源 (包括 large_font)

    is_singing = false;
    is_in_countdown_animation = false;
    background_rms_calculation_locked = true;

    displayPleaseSingMessage = false;
    overall_button_to_button_duration = 0.0f;

    current_recording_elapsed_time_sec = 0.0f;
    current_volume_is_loud_enough = false;
    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;

    time_start_button_press = 0.0;
    time_end_button_press = 0.0;
    actual_singing_start_time = -1.0;
    current_actual_singing_end_time = -1.0;
    locked_successful_singing_end_time = -1.0;
    has_achieved_lockable_success = false;

    internal_was_loud_last_frame = false;
    internal_current_loud_streak_began_at_time = 0.0;
    allegro_recording_start_time = 0.0;

    // 載入用於倒數的大字型
    // cleanup_audio_recording 中已包含銷毀 large_font 的邏輯，
    // 因此此處無需檢查 large_font 是否已存在。
    // *** 重要：請將 "pirulen.ttf" 替換為你實際的字型檔案名稱和路徑 ***
    // 字型大小 72 是一個範例，你可以調整
    // 如果你沒有 pirulen.ttf，可以使用其他如 "arial.ttf" 等常見字型
    large_font = al_load_ttf_font("pirulen.ttf", 72, 0);
    if (!large_font) {
        fprintf(stderr, "警告：無法載入大型倒數字型 (例如 pirulen.ttf, 大小 72)。將使用預設字型。\n");
        // 如果載入失敗，render 函數中會回退到使用 font
    }


    printf("DEBUG: 小遊戲花朵已初始化/重置。\n");

    float button_width = 200;
    float button_height = 50;
    float center_x = SCREEN_WIDTH / 2.0f - button_width / 2.0f;
    float main_action_buttons_y = SCREEN_HEIGHT - 80;

    minigame_buttons[0] = (Button){center_x, main_action_buttons_y, button_width, button_height, "種下種子", MINIGAME1, al_map_rgb(70, 170, 70), al_map_rgb(100, 200, 100), al_map_rgb(255, 255, 255), false};
    minigame_buttons[1] = (Button){center_x, main_action_buttons_y, button_width, button_height, "開始唱歌", MINIGAME1, al_map_rgb(70, 70, 170), al_map_rgb(100, 100, 200), al_map_rgb(255, 255, 255), false};
    minigame_buttons[2] = (Button){center_x - button_width / 2 - 10, main_action_buttons_y, button_width, button_height, "重新開始", MINIGAME1, al_map_rgb(170, 70, 70), al_map_rgb(200, 100, 100), al_map_rgb(255, 255, 255), false};
    minigame_buttons[3] = (Button){center_x + button_width / 2 + 10, main_action_buttons_y, button_width, button_height, "完成歌唱", MINIGAME1, al_map_rgb(70, 170, 170), al_map_rgb(100, 200, 200), al_map_rgb(255, 255, 255), false};

    minigame_buttons[4].x = SCREEN_WIDTH - 150 - 20;
    minigame_buttons[4].y = SCREEN_HEIGHT - 40 - 20;
    minigame_buttons[4].width = 150;
    minigame_buttons[4].height = 40;
    minigame_buttons[4].text = "離開小遊戲";
    minigame_buttons[4].action_phase = GROWTH;
    minigame_buttons[4].color = al_map_rgb(100, 100, 100);
    minigame_buttons[4].hover_color = al_map_rgb(130, 130, 130);
    minigame_buttons[4].text_color = al_map_rgb(255, 255, 255);
    minigame_buttons[4].is_hovered = false;

    minigame_buttons[5] = (Button){center_x, main_action_buttons_y, button_width, button_height, "採收", MINIGAME1, al_map_rgb(200, 150, 50), al_map_rgb(230, 180, 80), al_map_rgb(255, 255, 255), false};
}

static float calculate_rms(const short* samples, int count) {
    if (count == 0) return 0.0f;
    double sum_sq = 0.0;
    for (int i = 0; i < count; ++i) {
        sum_sq += (double)samples[i] * samples[i];
    }
    float rms = (float)sqrt(sum_sq / count);
    return rms;
}


void render_minigame1(void) {
    al_clear_to_color(al_map_rgb(50, 50, 70)); // 清除畫面為深藍灰色
    char display_text_buffer[256]; // 用於格式化文字的通用緩衝區
    float y_offset = 60; // 統一向下移動的距離
    float base_text_y = WAVEFORM_AREA_Y - 70 + y_offset; // 計時資訊的起始 Y 座標

    // 顯示主要的錄製/歌唱時間
    if (is_singing && !is_in_countdown_animation) { // 僅在唱歌且非倒數時顯示
        char actual_start_str[20], current_actual_end_str[20], locked_end_str[20];
        if (actual_singing_start_time < 0.0) strcpy(actual_start_str, "N/A");
        else sprintf(actual_start_str, "%.2fs", actual_singing_start_time);

        if (current_actual_singing_end_time < 0.0) strcpy(current_actual_end_str, "N/A");
        else sprintf(current_actual_end_str, "%.2fs", current_actual_singing_end_time);

        if (locked_successful_singing_end_time < 0.0) strcpy(locked_end_str, "N/A");
        else sprintf(locked_end_str, "%.2fs", locked_successful_singing_end_time);

        sprintf(display_text_buffer, "按鈕按下: %.2fs | 實際開始: %s | 實際結束(動態): %s (當前: %.2fs)",
                time_start_button_press, actual_start_str, current_actual_end_str, al_get_time());
        al_draw_text(font, al_map_rgb(220, 220, 180), WAVEFORM_AREA_X, base_text_y, 0, display_text_buffer);

        if(has_achieved_lockable_success){
            sprintf(display_text_buffer, "已鎖定結束: %s. 核心演唱已達標!", locked_end_str);
            al_draw_text(font, al_map_rgb(180, 255, 180), WAVEFORM_AREA_X, base_text_y + 20, 0, display_text_buffer);
        }

        // 現有的已錄製時間和 RMS 顯示
        sprintf(display_text_buffer, "已錄製: %.1f s (背景RMS: %.0f)", current_recording_elapsed_time_sec, current_background_rms);
        al_draw_text(font, al_map_rgb(255, 255, 255), WAVEFORM_AREA_X, base_text_y + (has_achieved_lockable_success ? 40 : 20), 0, display_text_buffer);

        ALLEGRO_COLOR vol_indicator_box_color = current_volume_is_loud_enough ? al_map_rgb(0, 200, 0) : al_map_rgb(200, 0, 0);
        const char* vol_text = current_volume_is_loud_enough ? "音量達標!" : "音量不足";
        float vol_text_width_val = al_get_text_width(font, vol_text); // 避免與函數參數重名
        float vol_indicator_x_start = WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH - vol_text_width_val - 20;
        al_draw_filled_rectangle(vol_indicator_x_start, base_text_y + (has_achieved_lockable_success ? 40 : 20),
                                 WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH, base_text_y + (has_achieved_lockable_success ? 40 : 20) + 20, vol_indicator_box_color);
        al_draw_text(font, al_map_rgb(0,0,0), vol_indicator_x_start + 5, base_text_y + (has_achieved_lockable_success ? 40 : 20) + (20-al_get_font_line_height(font))/2.0f, 0, vol_text);

    } else if (time_end_button_press > 0.0 && time_start_button_press > 0.0 && !is_in_countdown_animation) { // 歌唱完成後且已開始過 (非倒數時)
        char actual_start_str[20], final_actual_end_str[20];
        if (actual_singing_start_time < 0.0) strcpy(actual_start_str, "N/A");
        else sprintf(actual_start_str, "%.2fs", actual_singing_start_time);

        double displayed_end_time = has_achieved_lockable_success ? locked_successful_singing_end_time : current_actual_singing_end_time;
        if (displayed_end_time < 0.0) strcpy(final_actual_end_str, "N/A");
        else sprintf(final_actual_end_str, "%.2fs", displayed_end_time);

        sprintf(display_text_buffer, "按鈕間: %.2fs 至 %.2fs | 實際演唱: %s 至 %s",
                time_start_button_press, time_end_button_press, actual_start_str, final_actual_end_str);

        float text_y_pos = minigame_buttons[0].y - 60 + y_offset; // 按鈕上方的顯示位置
        al_draw_text(font, al_map_rgb(220, 220, 180), SCREEN_WIDTH / 2.0f, text_y_pos, ALLEGRO_ALIGN_CENTER, display_text_buffer);
         if(has_achieved_lockable_success){ // 如果成功是由於鎖定
            if(displayPleaseSingMessage == false) { // 僅在通過驗證時顯示
                 al_draw_text(font, al_map_rgb(180, 255, 180), SCREEN_WIDTH / 2.0f, text_y_pos + 20, ALLEGRO_ALIGN_CENTER, "核心演唱已達標並通過!");
            }
        }
    }

    // 321 倒數動畫繪製
    if (is_in_countdown_animation) {
        sprintf(display_text_buffer, "%d", countdown_value);
        // 如果 large_font 成功載入，則使用它；否則回退到普通的 font
        ALLEGRO_FONT *font_for_countdown = large_font ? large_font : font;
        float countdown_text_height = al_get_font_line_height(font_for_countdown);

        al_draw_text(font_for_countdown, al_map_rgb(255, 255, 0), // 黃色大號數字
                     SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - countdown_text_height / 2.0f,
                     ALLEGRO_ALIGN_CENTER, display_text_buffer);
        al_draw_text(font, al_map_rgb(200,200,200), SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f + countdown_text_height, ALLEGRO_ALIGN_CENTER, "計算背景音量中...");
    }
    // 波形繪製 (僅在唱歌且非倒數時)
    else if (is_singing && isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL) {
        DWORD bytes_per_sample = AUDIO_BITS_PER_SAMPLE / 8;
        DWORD total_samples_recorded_so_far = waveHdr.dwBytesRecorded / bytes_per_sample;
        int num_samples_in_waveform_timespan = (int)(AUDIO_SAMPLE_RATE * WAVEFORM_DISPLAY_DURATION_SEC);

        if (total_samples_recorded_so_far > 1 && num_samples_in_waveform_timespan > 0 && WAVEFORM_AREA_WIDTH > 1) {
            short* all_samples = (short*)pWaveBuffer;
            float prev_x_pos = WAVEFORM_AREA_X;
            int start_buffer_idx = (total_samples_recorded_so_far > num_samples_in_waveform_timespan) ?
                                   (total_samples_recorded_so_far - num_samples_in_waveform_timespan) : 0;
            int samples_available_in_window = total_samples_recorded_so_far - start_buffer_idx;

            if (samples_available_in_window > 1) {
                short first_sample_val = all_samples[start_buffer_idx];
                float prev_y_pos = WAVEFORM_AREA_Y + (WAVEFORM_AREA_HEIGHT / 2.0f) -
                                   (first_sample_val / 32767.0f) * (WAVEFORM_AREA_HEIGHT / 2.0f);
                prev_y_pos = fmaxf(WAVEFORM_AREA_Y + 1.0f, fminf(WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT - 1.0f, prev_y_pos));

                for (int screen_pixel_x = 1; screen_pixel_x < WAVEFORM_AREA_WIDTH; ++screen_pixel_x) {
                    float proportion = (WAVEFORM_AREA_WIDTH > 1) ? ((float)screen_pixel_x / (WAVEFORM_AREA_WIDTH - 1)) : 0.0f;
                    int sample_offset_in_selected_window = (int)(proportion * (samples_available_in_window - 1));
                    int current_sample_idx_in_buffer = start_buffer_idx + sample_offset_in_selected_window;

                    if(current_sample_idx_in_buffer >= total_samples_recorded_so_far) current_sample_idx_in_buffer = total_samples_recorded_so_far -1;
                    if(current_sample_idx_in_buffer < 0) current_sample_idx_in_buffer = 0;

                    short sample_value = all_samples[current_sample_idx_in_buffer];
                    float current_x_pos = WAVEFORM_AREA_X + screen_pixel_x;
                    float current_y_pos = WAVEFORM_AREA_Y + (WAVEFORM_AREA_HEIGHT / 2.0f) -
                                       (sample_value / 32767.0f) * (WAVEFORM_AREA_HEIGHT / 2.0f);
                    current_y_pos = fmaxf(WAVEFORM_AREA_Y + 1.0f, fminf(WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT - 1.0f, current_y_pos));

                    al_draw_line(prev_x_pos, prev_y_pos, current_x_pos, current_y_pos, al_map_rgb(100, 200, 255), 1.0f);

                    prev_x_pos = current_x_pos;
                    prev_y_pos = current_y_pos;
                }
            }
        }
    } else if (is_singing && !is_in_countdown_animation) { // 正在唱歌但硬體未就緒 (非倒數時)
        al_draw_text(font, al_map_rgb(200,200,200), WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH/2.0f, WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT/2.0f - al_get_font_line_height(font)/2.0f, ALLEGRO_ALIGN_CENTER, "等待麥克風...");
    }

    // 植物繪製邏輯
    float plant_base_y = WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT + SCREEN_HEIGHT * 0.20f + y_offset;
    if (plant_base_y > SCREEN_HEIGHT * 0.75f) plant_base_y = SCREEN_HEIGHT * 0.75f;
    float plant_x = SCREEN_WIDTH / 2.0f;
    float stem_height = 20 + flower_plant.growth_stage * 15;

    if (!seed_planted) {
        al_draw_text(font, al_map_rgb(255, 255, 255), plant_x, plant_base_y - 50, ALLEGRO_ALIGN_CENTER, "請先種下種子");
    } else {
        if (flower_plant.growth_stage == 0) {
            al_draw_filled_circle(plant_x, plant_base_y, 10, al_map_rgb(139, 69, 19));
        } else {
            al_draw_filled_rectangle(plant_x - 4, plant_base_y - stem_height, plant_x + 4, plant_base_y, al_map_rgb(0, 128, 0));
            if (flower_plant.growth_stage >= 2) {
                 al_draw_filled_triangle(plant_x + 4, plant_base_y - stem_height * 0.3f,
                                        plant_x + 4, plant_base_y - stem_height * 0.3f - 15,
                                        plant_x + 4 + 25, plant_base_y - stem_height * 0.3f - 7,
                                        al_map_rgb(34, 139, 34));
            }
            if (flower_plant.growth_stage >= 3) {
                 al_draw_filled_triangle(plant_x - 4, plant_base_y - stem_height * 0.5f,
                                        plant_x - 4, plant_base_y - stem_height * 0.5f - 15,
                                        plant_x - 4 - 25, plant_base_y - stem_height * 0.5f - 7,
                                        al_map_rgb(34, 139, 34));
            }
             if (flower_plant.growth_stage >= 4) {
                 al_draw_filled_triangle(plant_x + 4, plant_base_y - stem_height * 0.7f,
                                        plant_x + 4, plant_base_y - stem_height * 0.7f - 15,
                                        plant_x + 4 + 20, plant_base_y - stem_height * 0.7f - 7,
                                        al_map_rgb(34, 139, 34));
            }
            if (flower_plant.growth_stage >= songs_to_flower) {
                for (int i = 0; i < 6; ++i) {
                    float angle = (ALLEGRO_PI * 2.0f / 6.0f) * i;
                    if(flower_plant.which_flower == 0){
                        al_draw_filled_circle(plant_x + cos(angle) * 20,
                                              (plant_base_y - stem_height - 25) + sin(angle) * 20,
                                              12, al_map_rgb(238, 255, 0));
                    } else {
                        al_draw_filled_circle(plant_x + cos(angle) * 20,
                                              (plant_base_y - stem_height - 25) + sin(angle) * 20,
                                              12, al_map_rgb(162, 22, 212));
                    }
                }
                if(flower_plant.which_flower == 0){
                    al_draw_filled_circle(plant_x, plant_base_y - stem_height - 25, 18, al_map_rgb(255, 136, 0));
                } else {
                    al_draw_filled_circle(plant_x, plant_base_y - stem_height - 25, 18, al_map_rgb(0, 0, 0));
                }
                al_draw_text(font, al_map_rgb(255, 215, 0), plant_x, plant_base_y - stem_height - 70, ALLEGRO_ALIGN_CENTER, "開花了!");
            }
        }
    }

    // 顯示驗證失敗訊息
    if (displayPleaseSingMessage && !is_in_countdown_animation) { // 非倒數時才顯示
        char validation_msg[128];
        sprintf(validation_msg, "按鈕間時間>%.0fs, 唱歌總時間>%.0fs, (若未鎖定)音量比例>%.0f%%",
                MIN_RECORDING_DURATION_SECONDS, SINGING_TOTAL_TIME_THRESHOLD, REQUIRED_SINGING_PROPORTION * 100.0f);
        float msg_y_pos = minigame_buttons[0].y - 30 + y_offset;
        if (time_end_button_press > 0.0 && time_start_button_press > 0.0) {
             msg_y_pos -= (has_achieved_lockable_success && !displayPleaseSingMessage ? 40 : 20);
        }
        al_draw_text(font, al_map_rgb(255, 100, 100), SCREEN_WIDTH / 2.0f, msg_y_pos, ALLEGRO_ALIGN_CENTER, validation_msg);
         if (has_achieved_lockable_success && time_end_button_press > 0.0 && displayPleaseSingMessage) {
            al_draw_text(font, al_map_rgb(255,150,150), SCREEN_WIDTH/2.0f, msg_y_pos + 15, ALLEGRO_ALIGN_CENTER, "(核心曾達標但其他條件如按鈕時長未滿足)");
        }
    }

    // --- 繪製按鈕 ---
    // 只有在非倒數動畫時才顯示主要操作按鈕
    if (!is_in_countdown_animation) {
        if (!seed_planted) {
            Button* b = &minigame_buttons[0];
            al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color);
            al_draw_text(font, b->text_color, b->x + b->width / 2.0f, b->y + (b->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b->text);
        }
        if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower) {
            Button* b = &minigame_buttons[1];
            al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color);
            al_draw_text(font, b->text_color, b->x + b->width / 2.0f, b->y + (b->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b->text);
        } else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower) {
            Button* b = &minigame_buttons[5]; // 採收按鈕
            al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color);
            al_draw_text(font, b->text_color, b->x + b->width / 2.0f, b->y + (b->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b->text);
        }
        if (is_singing) { // 唱歌時顯示「重新開始」和「完成歌唱」
            Button* b_restart = &minigame_buttons[2];
            al_draw_filled_rectangle(b_restart->x, b_restart->y, b_restart->x + b_restart->width, b_restart->y + b_restart->height, b_restart->is_hovered ? b_restart->hover_color : b_restart->color);
            al_draw_text(font, b_restart->text_color, b_restart->x + b_restart->width / 2.0f, b_restart->y + (b_restart->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_restart->text);
            Button* b_finish = &minigame_buttons[3];
            al_draw_filled_rectangle(b_finish->x, b_finish->y, b_finish->x + b_finish->width, b_finish->y + b_finish->height, b_finish->is_hovered ? b_finish->hover_color : b_finish->color);
            al_draw_text(font, b_finish->text_color, b_finish->x + b_finish->width / 2.0f, b_finish->y + (b_finish->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_finish->text);
        }
    }
    // 離開按鈕總是顯示
    Button* b_exit = &minigame_buttons[4];
    al_draw_filled_rectangle(b_exit->x, b_exit->y, b_exit->x + b_exit->width, b_exit->y + b_exit->height, b_exit->is_hovered ? b_exit->hover_color : b_exit->color);
    al_draw_text(font, b_exit->text_color, b_exit->x + b_exit->width / 2.0f, b_exit->y + (b_exit->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_exit->text);


    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2.0f, 30, ALLEGRO_ALIGN_CENTER, "唱歌種花小遊戲");
    if (seed_planted) {
        al_draw_textf(font, al_map_rgb(200, 200, 200), SCREEN_WIDTH - 20, 30, ALLEGRO_ALIGN_RIGHT, "已唱歌曲: %d / %d", flower_plant.songs_sung, songs_to_flower);
    }
}

void handle_minigame1_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
        for (int i = 0; i < NUM_MINIGAME1_BUTTONS; ++i) { minigame_buttons[i].is_hovered = false; }
        if (!is_in_countdown_animation) {
            if (!seed_planted && ev.mouse.x >= minigame_buttons[0].x && ev.mouse.x <= minigame_buttons[0].x + minigame_buttons[0].width && ev.mouse.y >= minigame_buttons[0].y && ev.mouse.y <= minigame_buttons[0].y + minigame_buttons[0].height) { minigame_buttons[0].is_hovered = true;}
            else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower && ev.mouse.x >= minigame_buttons[1].x && ev.mouse.x <= minigame_buttons[1].x + minigame_buttons[1].width && ev.mouse.y >= minigame_buttons[1].y && ev.mouse.y <= minigame_buttons[1].y + minigame_buttons[1].height) { minigame_buttons[1].is_hovered = true; }
            else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower && ev.mouse.x >= minigame_buttons[5].x && ev.mouse.x <= minigame_buttons[5].x + minigame_buttons[5].width && ev.mouse.y >= minigame_buttons[5].y && ev.mouse.y <= minigame_buttons[5].y + minigame_buttons[5].height) { minigame_buttons[5].is_hovered = true; }
            else if (is_singing) {
                if (ev.mouse.x >= minigame_buttons[2].x && ev.mouse.x <= minigame_buttons[2].x + minigame_buttons[2].width && ev.mouse.y >= minigame_buttons[2].y && ev.mouse.y <= minigame_buttons[2].y + minigame_buttons[2].height) { minigame_buttons[2].is_hovered = true; }
                if (ev.mouse.x >= minigame_buttons[3].x && ev.mouse.x <= minigame_buttons[3].x + minigame_buttons[3].width && ev.mouse.y >= minigame_buttons[3].y && ev.mouse.y <= minigame_buttons[3].y + minigame_buttons[3].height) { minigame_buttons[3].is_hovered = true; }
            }
        }
        if (ev.mouse.x >= minigame_buttons[4].x && ev.mouse.x <= minigame_buttons[4].x + minigame_buttons[4].width && ev.mouse.y >= minigame_buttons[4].y && ev.mouse.y <= minigame_buttons[4].y + minigame_buttons[4].height) { minigame_buttons[4].is_hovered = true; }
    }
    else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev.mouse.button == 1) {
            bool button_clicked_this_event = false;
            if (!is_in_countdown_animation) {
                if (!seed_planted && minigame_buttons[0].is_hovered) {
                    seed_planted = true; is_singing = false;
                    flower_plant.songs_sung = 0; flower_plant.growth_stage = 0;
                    displayPleaseSingMessage = false; button_clicked_this_event = true;
                    time_start_button_press = 0.0; time_end_button_press = 0.0;
                    has_achieved_lockable_success = false;
                }
                else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower && minigame_buttons[1].is_hovered) {
                    is_in_countdown_animation = true;
                    countdown_start_time = al_get_time();
                    countdown_value = 3;
                    background_rms_calculation_locked = false;
                    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
                    start_actual_audio_recording();
                    printf("小遊戲: 開始唱歌按鈕點擊，進入倒數計時。\n");
                    button_clicked_this_event = true;
                }
                else if (is_singing) {
                    if (minigame_buttons[2].is_hovered) {
                        if (hWaveIn && isActuallyRecording) {
                            waveInReset(hWaveIn);
                            if (waveHdr.dwFlags & WHDR_PREPARED) {
                                waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
                            }
                        }
                        is_in_countdown_animation = true;
                        countdown_start_time = al_get_time();
                        countdown_value = 3;
                        background_rms_calculation_locked = false;
                        current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
                        start_actual_audio_recording();
                        printf("小遊戲: 重新開始唱歌按鈕點擊，進入倒數計時。\n");
                        button_clicked_this_event = true;
                    }
                    else if (minigame_buttons[3].is_hovered) {
                        is_singing = false;
                        bool sound_was_valid = stop_actual_audio_recording();
                        printf("小遊戲: 完成歌唱按鈕點擊。\n");
                        if (sound_was_valid) {
                            if (flower_plant.songs_sung < songs_to_flower) { flower_plant.songs_sung++; }
                            flower_plant.growth_stage = flower_plant.songs_sung;
                            if(flower_plant.growth_stage == songs_to_flower){ flower_plant.which_flower = (rand() % 2); }
                            printf("DEBUG: 有效歌曲已錄製，成長已更新。\n");
                        } else {
                            printf("DEBUG: 偵測到無效聲音。歌曲未計數。\n");
                        }
                        current_recording_elapsed_time_sec = 0.0f;
                        current_volume_is_loud_enough = false;
                        button_clicked_this_event = true;
                    }
                }
                else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower && minigame_buttons[5].is_hovered) {
                    if (flower_plant.which_flower == 0) { player.item_quantities[0]++; printf("DEBUG: 採收了一朵普通花! 總數: %d\n", player.item_quantities[0]); }
                    else { player.item_quantities[1]++; printf("DEBUG: 採收了一朵惡魔花. 總數: %d\n", player.item_quantities[1]); }
                    flower_plant.songs_sung = 0; flower_plant.growth_stage = 0;
                    seed_planted = false; displayPleaseSingMessage = false; button_clicked_this_event = true;
                    time_start_button_press = 0.0; time_end_button_press = 0.0;
                    has_achieved_lockable_success = false;
                }
            }
            if (minigame_buttons[4].is_hovered) {
                if((is_singing || is_in_countdown_animation) && isActuallyRecording && hWaveIn) {
                    waveInReset(hWaveIn);
                }
                is_singing = false; is_in_countdown_animation = false;
                cleanup_audio_recording();
                game_phase = GROWTH;
                button_clicked_this_event = true;
            }
            if (button_clicked_this_event) { for (int i = 0; i < NUM_MINIGAME1_BUTTONS; ++i) { minigame_buttons[i].is_hovered = false; } }
        }
    }
    else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            if((is_singing || is_in_countdown_animation) && isActuallyRecording && hWaveIn) {
                waveInReset(hWaveIn);
            }
            is_singing = false; is_in_countdown_animation = false;
            cleanup_audio_recording();
            game_phase = GROWTH;
        }
    }
}

void update_minigame1(void) {
    double current_time_for_update = al_get_time();

    if (is_in_countdown_animation) {
        double elapsed_countdown_time = current_time_for_update - countdown_start_time;
        if (elapsed_countdown_time < 1.0) countdown_value = 3;
        else if (elapsed_countdown_time < 2.0) countdown_value = 2;
        else if (elapsed_countdown_time < 3.0) countdown_value = 1;
        else { // 倒數結束
            is_in_countdown_animation = false;
            background_rms_calculation_locked = true; // 鎖定背景音量
            is_singing = true; // 正式開始唱歌

            allegro_recording_start_time = current_time_for_update;
            time_start_button_press = allegro_recording_start_time;

            current_recording_elapsed_time_sec = 0.0f;
            actual_singing_start_time = -1.0;
            current_actual_singing_end_time = -1.0;
            locked_successful_singing_end_time = -1.0;
            has_achieved_lockable_success = false;
            internal_was_loud_last_frame = false;
            internal_current_loud_streak_began_at_time = 0.0;
            displayPleaseSingMessage = false;

            printf("DEBUG: 倒數結束。背景 RMS 已鎖定為 %.0f。正式開始唱歌。\n", current_background_rms);
        }

        if (isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL && !background_rms_calculation_locked) {
            DWORD bytes_per_sample = AUDIO_BITS_PER_SAMPLE / 8;
            DWORD total_samples_in_buffer_now = waveHdr.dwBytesRecorded / bytes_per_sample;
            short* live_samples_ptr = (short*)pWaveBuffer;

            if (total_samples_in_buffer_now >= RMS_ANALYSIS_WINDOW_SAMPLES) {
                const short* latest_samples_for_noise = live_samples_ptr + (total_samples_in_buffer_now - RMS_ANALYSIS_WINDOW_SAMPLES);
                float latest_rms = calculate_rms(latest_samples_for_noise, RMS_ANALYSIS_WINDOW_SAMPLES);
                current_background_rms = (current_background_rms * 0.90f) + (latest_rms * 0.10f);
                if (current_background_rms < 50) current_background_rms = 50;
            }
        }
    }
    else if (is_singing && isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL) {
        current_recording_elapsed_time_sec = (float)(current_time_for_update - allegro_recording_start_time);
        DWORD bytes_per_sample = AUDIO_BITS_PER_SAMPLE / 8;
        DWORD total_samples_in_buffer_now = waveHdr.dwBytesRecorded / bytes_per_sample;
        short* live_samples_ptr = (short*)pWaveBuffer;

        if (total_samples_in_buffer_now >= RMS_ANALYSIS_WINDOW_SAMPLES) {
            const short* current_analysis_samples = live_samples_ptr + (total_samples_in_buffer_now - RMS_ANALYSIS_WINDOW_SAMPLES);
            float current_rms_val = calculate_rms(current_analysis_samples, RMS_ANALYSIS_WINDOW_SAMPLES); // 避免與全域變數重名
            if (current_rms_val > current_background_rms * SINGING_RMS_THRESHOLD_MULTIPLIER &&
                current_rms_val > MIN_ABSOLUTE_RMS_FOR_SINGING) {
                if (!current_volume_is_loud_enough)
                    printf("[DEBUG][UpdateSinging] 音量剛好足夠!\n");
                current_volume_is_loud_enough = true;
            } else {
                if (current_volume_is_loud_enough)
                    printf("[DEBUG][UpdateSinging] 音量剛降到閾值以下。\n");
                current_volume_is_loud_enough = false;
            }
        } else {
            current_volume_is_loud_enough = false;
        }

        if (current_volume_is_loud_enough) {
            if (!internal_was_loud_last_frame) {
                internal_current_loud_streak_began_at_time = current_time_for_update;
            }
            double current_loud_duration = current_time_for_update - internal_current_loud_streak_began_at_time;
            if (current_loud_duration >= SINGING_DETECTION_MIN_STREAK_DURATION) {
                if (actual_singing_start_time < 0.0) {
                    actual_singing_start_time = internal_current_loud_streak_began_at_time;
                }
                current_actual_singing_end_time = current_time_for_update;
            }
            internal_was_loud_last_frame = true;
        } else {
            internal_was_loud_last_frame = false;
        }

        if (!has_achieved_lockable_success) {
            if (actual_singing_start_time > 0.0 && current_actual_singing_end_time > 0.0) {
                double current_singing_span = current_actual_singing_end_time - actual_singing_start_time;
                if (current_singing_span > SINGING_TOTAL_TIME_THRESHOLD) {
                    has_achieved_lockable_success = true;
                    locked_successful_singing_end_time = current_actual_singing_end_time;
                    printf("[DEBUG][UpdateSinging] 核心歌唱需求已滿足並於 %.2fs 鎖定 (歌唱時長: %.2fs)\n",
                        locked_successful_singing_end_time, current_singing_span);
                }
            }
        }
    } else if (!is_in_countdown_animation) {
        current_volume_is_loud_enough = false;
    }
}

static void prepare_audio_recording(void) {
    if (pWaveBuffer == NULL) {
        pWaveBuffer = (char*)malloc(AUDIO_BUFFER_SIZE);
        if (pWaveBuffer == NULL) {
            fprintf(stderr, "嚴重錯誤：無法分配音訊緩衝區。\n");
            if (hWaveIn != NULL) { waveInClose(hWaveIn); hWaveIn = NULL; }
            return;
        }
    }
    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM; wfx.nChannels = AUDIO_CHANNELS; wfx.nSamplesPerSec = AUDIO_SAMPLE_RATE;
    wfx.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8);
    wfx.nBlockAlign = AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8); wfx.wBitsPerSample = AUDIO_BITS_PER_SAMPLE; wfx.cbSize = 0;
    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        char error_text[256]; waveInGetErrorTextA(result, error_text, sizeof(error_text));
        fprintf(stderr, "錯誤：無法開啟音訊錄製裝置 (錯誤 %u: %s)。\n請確認麥克風已連接並啟用。\n", result, error_text);
        if (pWaveBuffer) { free(pWaveBuffer); pWaveBuffer = NULL; }
        hWaveIn = NULL; return;
    }
    printf("DEBUG: Windows 音訊錄製已準備。裝置已開啟。\n");
}

static void start_actual_audio_recording(void) {
    if (hWaveIn == NULL) {
        prepare_audio_recording();
        if (hWaveIn == NULL) {
            fprintf(stderr, "音訊系統未就緒。無法開始錄製。\n");
            is_singing = false; is_in_countdown_animation = false; isActuallyRecording = false;
            return;
        }
    }
    if (waveHdr.dwFlags & WHDR_PREPARED) { // 如果標頭已準備，先取消準備
        waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    }
    ZeroMemory(&waveHdr, sizeof(WAVEHDR)); waveHdr.lpData = pWaveBuffer;
    waveHdr.dwBufferLength = AUDIO_BUFFER_SIZE; waveHdr.dwFlags = 0;
    ZeroMemory(pWaveBuffer, AUDIO_BUFFER_SIZE); // 清空緩衝區
    MMRESULT prep_res = waveInPrepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    if (prep_res != MMSYSERR_NOERROR) { fprintf(stderr, "準備 wave 標頭錯誤: %u\n", prep_res); is_singing = false; is_in_countdown_animation = false; isActuallyRecording = false; return; }
    MMRESULT add_res = waveInAddBuffer(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    if (add_res != MMSYSERR_NOERROR) { fprintf(stderr, "新增 wave 緩衝區錯誤: %u\n", add_res); waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)); is_singing = false; is_in_countdown_animation = false; isActuallyRecording = false; return; }
    MMRESULT start_res = waveInStart(hWaveIn);
    if (start_res != MMSYSERR_NOERROR) { fprintf(stderr, "開始 wave 輸入錯誤: %u\n", start_res); waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)); is_singing = false; is_in_countdown_animation = false; isActuallyRecording = false; return; }
    isActuallyRecording = true;
    printf("[DEBUG][Audio] start_actual_audio_recording() 已呼叫。hWaveIn=%p\n", hWaveIn);
    printf("[DEBUG][Audio] isActuallyRecording=%d。等待倒數計時完成以設定歌唱開始時間。\n", isActuallyRecording);
}

static bool stop_actual_audio_recording(void) {
    if (!isActuallyRecording || hWaveIn == NULL) {
        printf("[DEBUG][Audio] stop_actual_audio_recording() 被呼叫但未在錄製。isActuallyRecording=%d, hWaveIn=%p\n", isActuallyRecording, hWaveIn);
        isActuallyRecording = false; is_singing = false; is_in_countdown_animation = false;
        return false;
    }
    double current_stop_time = al_get_time();
    time_end_button_press = current_stop_time;
    overall_button_to_button_duration = (float)(time_end_button_press - time_start_button_press);
    MMRESULT reset_res = waveInReset(hWaveIn); // 停止資料流入
    if (reset_res != MMSYSERR_NOERROR) { fprintf(stderr, "重置 wave 輸入錯誤: %u\n", reset_res); }
    isActuallyRecording = false; // 設定為未在錄製
    printf("DEBUG: Windows 錄製停止於 %.2f。總按鈕間時長 (倒數後): %.2f 秒。\n", time_end_button_press, overall_button_to_button_duration);
    printf("DEBUG: 錄製位元組: %lu。實際歌唱開始: %.2f, 目前實際歌唱結束 (停止時): %.2f, 鎖定結束: %.2f, 是否鎖定: %s\n",
           waveHdr.dwBytesRecorded, actual_singing_start_time, current_actual_singing_end_time,
           locked_successful_singing_end_time, has_achieved_lockable_success ? "是" : "否");
    bool validationSuccess = true;
    displayPleaseSingMessage = false;
    if (force_audio_too_short_for_test) {
        overall_button_to_button_duration = 0.5f;
        printf("DEBUG: 強制按鈕間時長過短 (%.2f 秒)。\n", overall_button_to_button_duration);
    }
    if (overall_button_to_button_duration < MIN_RECORDING_DURATION_SECONDS) {
        printf("DEBUG 失敗: 按鈕間時長 (%.2fs) < 最低要求 (%.1fs)。\n", overall_button_to_button_duration, MIN_RECORDING_DURATION_SECONDS);
        validationSuccess = false;
    }
    short* samples = (short*)pWaveBuffer;
    DWORD num_samples_actually_recorded = waveHdr.dwBytesRecorded / (AUDIO_BITS_PER_SAMPLE / 8);
    float total_time_above_rms_threshold = 0.0f;
    if (validationSuccess && num_samples_actually_recorded == 0) {
        printf("DEBUG 失敗: 未錄製到音訊資料 (停止後 dwBytesRecorded 為 0)。\n");
        validationSuccess = false;
    } else if (num_samples_actually_recorded > 0) { // 僅在有資料時分析
        long loud_frame_count = 0;
        float persistent_background_rms_for_validation = current_background_rms; // 使用已鎖定的背景RMS
        const int frame_size = RMS_ANALYSIS_WINDOW_SAMPLES;
        for (DWORD frame_start_sample = 0; (frame_start_sample + frame_size) <= num_samples_actually_recorded; frame_start_sample += frame_size) {
            const short* current_frame_samples = samples + frame_start_sample;
            float frame_rms_val = calculate_rms(current_frame_samples, frame_size);
            if (frame_rms_val > persistent_background_rms_for_validation * SINGING_RMS_THRESHOLD_MULTIPLIER &&
                frame_rms_val > MIN_ABSOLUTE_RMS_FOR_SINGING) {
                loud_frame_count++;
            }
        }
        total_time_above_rms_threshold = (float)loud_frame_count * frame_size / AUDIO_SAMPLE_RATE;
        printf("DEBUG: [驗證] 完整錄製 - 使用鎖定背景 RMS: %.1f。大聲幀數: %ld。總大聲時間: %.2f 秒\n",
               persistent_background_rms_for_validation, loud_frame_count, total_time_above_rms_threshold);
    }
    if (validationSuccess) {
        if (has_achieved_lockable_success) {
            double locked_singing_duration = 0.0;
            if (actual_singing_start_time > 0.0 && locked_successful_singing_end_time > 0.0 && locked_successful_singing_end_time > actual_singing_start_time) {
                locked_singing_duration = locked_successful_singing_end_time - actual_singing_start_time;
            }
            if (locked_singing_duration <= SINGING_TOTAL_TIME_THRESHOLD) {
                printf("DEBUG 失敗 (鎖定路徑錯誤): 鎖定歌唱時長 (%.2fs) 未預期地 <= 閾值 (%.1fs)。\n", locked_singing_duration, SINGING_TOTAL_TIME_THRESHOLD);
                validationSuccess = false;
            } else {
                printf("DEBUG 通過 (鎖定路徑): 核心歌唱需求透過鎖定滿足。鎖定歌唱時長: %.2fs。豁免比例檢查。\n", locked_singing_duration);
            }
        } else { // 未鎖定
            double final_actual_singing_duration = 0.0;
            if (actual_singing_start_time > 0.0 && current_actual_singing_end_time > 0.0 && current_actual_singing_end_time > actual_singing_start_time) {
                final_actual_singing_duration = current_actual_singing_end_time - actual_singing_start_time;
            }
            if (final_actual_singing_duration <= SINGING_TOTAL_TIME_THRESHOLD) {
                printf("DEBUG 失敗 (未鎖定): 實際歌唱時長 (%.2fs) <= 閾值 (%.1fs)。\n", final_actual_singing_duration, SINGING_TOTAL_TIME_THRESHOLD);
                validationSuccess = false;
            }
            if (validationSuccess) { // 如果在 2a 後仍然有效
                if (overall_button_to_button_duration > 0) {
                    float singing_proportion = total_time_above_rms_threshold / overall_button_to_button_duration;
                    if (singing_proportion < REQUIRED_SINGING_PROPORTION) {
                        printf("DEBUG 失敗 (未鎖定): 歌唱比例 (%.2f / %.2f = %.3f) < 要求 (%.3f)。\n",
                               total_time_above_rms_threshold, overall_button_to_button_duration, singing_proportion, REQUIRED_SINGING_PROPORTION);
                        validationSuccess = false;
                    }
                } else if (total_time_above_rms_threshold > 0) {
                     printf("DEBUG 失敗 (未鎖定): 按鈕時長為 0 但大聲時間 > 0。無效狀態。\n");
                     validationSuccess = false;
                } else {
                    if (REQUIRED_SINGING_PROPORTION > 0.0f) { // 如果要求比例，但時長和聲音都為0
                        printf("DEBUG 失敗 (未鎖定): 按鈕時長和/或大聲時間為零，若需要比例則失敗。\n");
                        validationSuccess = false;
                    }
                }
            }
            if (validationSuccess && !has_achieved_lockable_success) {
                 printf("DEBUG 通過 (未鎖定): 所有標準需求均已滿足。\n");
            }
        }
    }
    if (!validationSuccess) {
        displayPleaseSingMessage = true;
        printf("DEBUG: 整體音訊驗證失敗。\n");
    } else {
        printf("DEBUG: 整體音訊驗證成功。\n");
        displayPleaseSingMessage = false;
    }
    if (waveHdr.dwFlags & WHDR_PREPARED) { // 確保取消準備標頭
        MMRESULT unprep_res = waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
        if (unprep_res != MMSYSERR_NOERROR && unprep_res != WAVERR_UNPREPARED) {
             fprintf(stderr, "取消準備 wave 標頭錯誤: %u\n", unprep_res);
        }
    }
    return validationSuccess;
}

static void cleanup_audio_recording(void) {
    printf("DEBUG: Windows cleanup_audio_recording() 已呼叫。\n");
    if (isActuallyRecording && hWaveIn != NULL) {
        waveInReset(hWaveIn); // 停止任何正在進行的錄製
        isActuallyRecording = false;
    }
    if (hWaveIn != NULL) {
        if (waveHdr.dwFlags & WHDR_PREPARED) { // 確保標頭已取消準備
            MMRESULT res_unprep = waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
            if (res_unprep != MMSYSERR_NOERROR && res_unprep != WAVERR_UNPREPARED) {
                fprintf(stderr, "清理期間取消準備 wave 標頭錯誤: %u\n", res_unprep);
            }
        }
        MMRESULT close_res = waveInClose(hWaveIn); // 關閉裝置
        if (close_res != MMSYSERR_NOERROR) {
            fprintf(stderr, "關閉 wave 輸入裝置錯誤: %u\n", close_res);
        }
        hWaveIn = NULL;
    }
    if (pWaveBuffer != NULL) {
        free(pWaveBuffer);
        pWaveBuffer = NULL;
    }
    ZeroMemory(&waveHdr, sizeof(WAVEHDR)); // 清空標頭結構

    // 清理 large_font (如果已載入)
    if (large_font) {
        printf("DEBUG: 正在銷毀 large_font。\n");
        al_destroy_font(large_font);
        large_font = NULL;
    }
}