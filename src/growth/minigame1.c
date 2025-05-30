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
// #define AUDIO_BUFFER_SIZE (44100 * 2 * 35) // 音訊緩衝區大小 (44.1kHz * 16位元單聲道 * 35秒) - REPLACED
#define AUDIO_SAMPLE_RATE 44100            // 音訊取樣率
#define AUDIO_CHANNELS 1                   // 音訊聲道數
#define AUDIO_BITS_PER_SAMPLE 16           // 每樣本位元數

const double MIN_TOTAL_SESSION_DURATION_FOR_GROWTH = 30.0; // New constant for plant growth (changed to const double)

// 新的音訊緩衝常數
#define NUM_AUDIO_BUFFERS 3               // 使用3個緩衝區進行三重緩衝
#define AUDIO_CHUNK_DURATION_MS 100       // 每次處理 100 毫秒的音訊
#define AUDIO_CHUNK_SAMPLES (AUDIO_SAMPLE_RATE * AUDIO_CHUNK_DURATION_MS / 1000) // 每塊音訊的樣本數
#define SINGLE_BUFFER_SIZE (AUDIO_CHUNK_SAMPLES * (AUDIO_BITS_PER_SAMPLE / 8) * AUDIO_CHANNELS) // 單一緩衝區的大小（位元組）
#define WAVEFORM_DISPLAY_SAMPLES (AUDIO_SAMPLE_RATE * 1) // 波形顯示緩衝區的樣本數 (例如 1 秒)


// 即時回饋常數
#define WAVEFORM_AREA_X 50                                  // 波形顯示區域 X 座標
#define WAVEFORM_AREA_Y (100 + 60)                          // 波形顯示區域 Y 座標 (向下移動 60 像素)
#define WAVEFORM_AREA_WIDTH (SCREEN_WIDTH - 100)            // 波形顯示區域寬度
#define WAVEFORM_AREA_HEIGHT 100                            // 波形顯示區域高度
#define REALTIME_ANALYSIS_WINDOW_MS 50                      // 即時分析窗口毫秒數 (未使用於目前RMS計算)
#define WAVEFORM_DISPLAY_DURATION_SEC 0.5f                  // 波形顯示持續時間 (秒)

// 歌唱驗證常數
// #define MIN_RECORDING_DURATION_SECONDS 1.0f                 // 按鈕按下到放開的最短時間 - OBSOLETE
#define REQUIRED_SINGING_PROPORTION (1.0f / 3.0f)           // 歌唱音量達標時間佔按鈕間時間的比例
const double SINGING_TOTAL_TIME_THRESHOLD = 20.0;           // 歌唱總時間 (t_singing_loud_duration) 需大於20秒 for singing_is_successful_realtime
const double SINGING_DETECTION_MIN_STREAK_DURATION = 0.2;   // 持續0.2秒音量達閾值才算有效歌唱片段

// 相對音量常數
#define NOISE_CALC_WINDOW_SAMPLES (AUDIO_SAMPLE_RATE / 5)   // 噪音計算窗口樣本數 (目前邏輯未使用，保留結構)
#define RMS_ANALYSIS_WINDOW_SAMPLES (AUDIO_SAMPLE_RATE / 20) // 50毫秒窗口，用於目前音量和歌唱檢查
#define SINGING_RMS_THRESHOLD_MULTIPLIER 3.0f               // 目前 RMS 必須是背景 RMS 的 X 倍
#define MIN_ABSOLUTE_RMS_FOR_SINGING 150.0f                 // 被視為歌唱的最低 RMS (防止靜音環境下的小聲誤判為大聲)
#define INITIAL_BACKGROUND_RMS_GUESS 200.0f                 // 背景 RMS 的初始猜測值


// 音訊錄製相關的靜態全域變數
static HWAVEIN hWaveIn = NULL;                              // Windows 音訊輸入裝置控制代碼
// static WAVEHDR waveHdr;                                     // Windows 音訊緩衝區標頭 - REPLACED
// static char* pWaveBuffer = NULL;                            // 指向音訊緩衝區的指標 - REPLACED
static WAVEHDR waveHdrs[NUM_AUDIO_BUFFERS];                 // Windows 音訊緩衝區標頭陣列
static char* pAudioBuffers[NUM_AUDIO_BUFFERS];              // 指向音訊緩衝區的指標陣列
static int current_buffer_idx = 0;                          // 目前正在被錄音裝置填充的緩衝區索引
static volatile int processed_buffer_idx = -1;             // 最新已處理緩衝區的索引 (可選，或由事件管理)
static short* waveform_buffer = NULL;                       // 波形顯示緩衝區
static int waveform_buffer_size_samples;                    // 波形顯示緩衝區的大小 (樣本數)
static int waveform_buffer_write_pos = 0;                   // 波形顯示緩衝區的寫入位置 (循環)

static double allegro_recording_start_time = 0.0;           // Allegro 開始錄製的時間點 (按下按鈕後，或倒數計時結束後)

static bool isActuallyRecording = false;                    // 是否真的正在錄音 (硬體層面)
static bool displayPleaseSingMessage = false;               // 是否顯示「請唱歌」提示訊息
// static float overall_button_to_button_duration = 0.0f;   // 按鈕按下到放開的總時長 - FULLY REMOVED
// static int local_last_processed_idx = -1; // Declaration moved to file scope near other static globals

// 即時回饋與相對音量相關的靜態全域變數
static float current_recording_elapsed_time_sec = 0.0f;     // 目前錄製已過時間 (秒)
static bool current_volume_is_loud_enough = false;          // 目前音量是否足夠大 (基於相對音量)
static float current_background_rms = INITIAL_BACKGROUND_RMS_GUESS; // 目前背景 RMS

// static bool force_audio_too_short_for_test = false;         // 用於除錯，強制音訊過短 - FULLY REMOVED
static int local_last_processed_idx = -1; // Moved here, to file scope

static MinigameFlowerPlant flower_plant;                    // 小遊戲花朵植物結構
static Button minigame_buttons[NUM_MINIGAME1_BUTTONS];      // 小遊戲按鈕陣列
static bool seed_planted = false;                           // 種子是否已種下
static bool is_singing = false;                             // 是否處於「唱歌」狀態 (邏輯層面，倒數計時後才為 true)
static const int songs_to_flower = 8;                       // 種出花朵需要的歌曲數量
static bool minigame_srand_called = false;                  // srand 是否已在小遊戲中呼叫過

// 新增的詳細歌唱計時靜態全域變數
static double time_start_button_press = 0.0;                // 使用者點擊「開始唱歌」的確切時間 (倒數後) - this corresponds to t1 / allegro_recording_start_time
// static double time_end_button_press = 0.0;                  // 使用者點擊「完成歌唱」的時間 - FULLY REMOVED (t4 is used)
// static double actual_singing_start_time = -1.0;             // 音訊首次達到閾值並持續0.2秒的時間點 (該0.2秒的開始) - REPLACED by t2_singing_start_time
// static double current_actual_singing_end_time = -1.0;       // 持續更新的最新0.2秒歌唱片段的結束時間 - REPLACED by t_singing_loud_duration logic
// static double locked_successful_singing_end_time = -1.0;    // 如果核心需求滿足，此變數鎖定成功的歌唱結束時間 - REMOVING, direct validation
// static bool has_achieved_lockable_success = false;          // 標記在錄製期間是否已滿足核心歌唱需求 - REMOVING, direct validation

static double t1_countdown_finish_time = 0.0;               // t1: 時間點：倒數計時結束，正式錄音開始
static double t2_singing_start_time = -1.0;                 // t2: 時間點：首次偵測到持續0.2秒的歌唱音量 (-1 表示尚未偵測到)
static double t_singing_loud_duration = 0.0;                // t_singing: 總計：從t2開始後，音量持續在閾值以上的總時長

// // 偵測歌唱片段的內部狀態 (Old ones, to be replaced)
// static bool internal_was_loud_last_frame = false;           // 內部：上一幀音量是否夠大 - REPLACED by is_currently_loud_for_streak
// static double internal_current_loud_streak_began_at_time = 0.0; // 內部：目前大聲片段開始的時間 - REPLACED by current_loud_streak_start_time

// New streak detection variables
static double current_loud_streak_start_time = -1.0;        // 追蹤目前連續大聲片段的開始時間 (-1 表示不在連續大聲片段中)
static bool is_currently_loud_for_streak = false;           // 目前的音訊塊是否達到大聲標準 (用於連續判斷)
static bool singing_is_successful_realtime = false;         // 即時歌唱成功標誌
static double t4_complete_button_press_time = 0.0;          // t4: 時間點：使用者點擊「完成歌唱」按鈕

// 321 倒數動畫相關變數
static bool is_in_countdown_animation = false;              // 是否正在進行321倒數動畫
static double countdown_start_time = 0.0;                   // 倒數動畫開始時間
static int countdown_value = 3;                             // 目前倒數顯示的數字
static bool background_rms_calculation_locked = false;      // 背景音量計算是否已完成並鎖定
static float accumulated_rms_sum = 0.0f;                    // 用於計算倒數期間平均 RMS 的累加和
static int rms_samples_count = 0;                           // 用於計算倒數期間平均 RMS 的樣本計數

// 新增：用於倒數計時的大字型
static ALLEGRO_FONT *large_font = NULL;


static void prepare_audio_recording(void);                  // 準備音訊錄製
static void start_actual_audio_recording(void);             // 開始實際的音訊錄製
static bool stop_actual_audio_recording(void);              // 停止實際的音訊錄製並進行驗證
static void cleanup_audio_recording(void);                  // 清理音訊錄製資源
static float calculate_rms(const short* samples, int count); // 計算 RMS 值

// 新增的音訊回呼函式原型
static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);


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

    // force_audio_too_short_for_test = false; // Obsolete

    cleanup_audio_recording(); // 清理任何先前的錄製資源 (包括 large_font)

    is_singing = false;
    is_in_countdown_animation = false;
    background_rms_calculation_locked = true;

    displayPleaseSingMessage = false;
    // overall_button_to_button_duration = 0.0f; // Obsolete (already commented, confirm removal of declaration)

    current_recording_elapsed_time_sec = 0.0f;
    current_volume_is_loud_enough = false;
    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;

    time_start_button_press = 0.0; // This is reset when singing starts (becomes t1)
    // time_end_button_press = 0.0; // Obsolete (already commented, confirm removal of declaration)
    // actual_singing_start_time = -1.0; // Replaced
    // current_actual_singing_end_time = -1.0; // Replaced
    // locked_successful_singing_end_time = -1.0; // Removed
    // has_achieved_lockable_success = false; // Removed

    t1_countdown_finish_time = 0.0;
    t2_singing_start_time = -1.0;
    t_singing_loud_duration = 0.0;
    current_loud_streak_start_time = -1.0;
    is_currently_loud_for_streak = false;

    // internal_was_loud_last_frame = false; // Replaced
    // internal_current_loud_streak_began_at_time = 0.0; // Replaced
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
    if (is_singing && !is_in_countdown_animation) { 
        char t1_str[20], t2_str[20];
        if (t1_countdown_finish_time <= 0.0) strcpy(t1_str, "N/A"); else sprintf(t1_str, "%.2f", t1_countdown_finish_time);
        if (t2_singing_start_time < 0.0) strcpy(t2_str, "N/A"); else sprintf(t2_str, "%.2f", t2_singing_start_time);

        sprintf(display_text_buffer, "t1:%ss t2:%ss ts:%.2fs L:%s Rec:%.1fs OK:%s",
                t1_str, t2_str, t_singing_loud_duration,
                current_volume_is_loud_enough ? "Y" : "N",
                current_recording_elapsed_time_sec,
                singing_is_successful_realtime ? "YES!" : "No "); // Added space for clarity
        al_draw_text(font, al_map_rgb(220, 220, 180), WAVEFORM_AREA_X, base_text_y, 0, display_text_buffer);

        sprintf(display_text_buffer, "(BgRMS:%.0f CurTime:%.2fs)", current_background_rms, al_get_time());
        al_draw_text(font, al_map_rgb(255, 255, 255), WAVEFORM_AREA_X, base_text_y + 20, 0, display_text_buffer);

        ALLEGRO_COLOR vol_indicator_box_color = current_volume_is_loud_enough ? al_map_rgb(0, 200, 0) : al_map_rgb(200, 0, 0);
        const char* vol_text = current_volume_is_loud_enough ? "音量達標!" : "音量不足";
        float vol_text_width_val = al_get_text_width(font, vol_text); 
        float vol_indicator_x_start = WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH - vol_text_width_val - 20;
        al_draw_filled_rectangle(vol_indicator_x_start, base_text_y + 20, 
                                 WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH, base_text_y + 20 + 20, vol_indicator_box_color);
        al_draw_text(font, al_map_rgb(0,0,0), vol_indicator_x_start + 5, base_text_y + 20 + (20-al_get_font_line_height(font))/2.0f, 0, vol_text);

    } else if (t4_complete_button_press_time > 0.0 && t1_countdown_finish_time > 0.0 && !is_in_countdown_animation) { // After "Complete Singing" is pressed
        char t1_disp_str[20], t2_disp_str[20], t4_disp_str[20];
        sprintf(t1_disp_str, "%.2f", t1_countdown_finish_time);
        if (t2_singing_start_time < 0.0) strcpy(t2_disp_str, "N/A"); else sprintf(t2_disp_str, "%.2f", t2_singing_start_time);
        sprintf(t4_disp_str, "%.2f", t4_complete_button_press_time);

        sprintf(display_text_buffer, "Session: t1 %ss to t4 %ss (Total: %.2fs)",
                t1_disp_str, t4_disp_str, (t4_complete_button_press_time - t1_countdown_finish_time));
        al_draw_text(font, al_map_rgb(220,220,180), SCREEN_WIDTH / 2.0f, base_text_y, ALLEGRO_ALIGN_CENTER, display_text_buffer);
        
        sprintf(display_text_buffer, "Singing: t2 %ss, Duration t_singing: %.2fs. Success: %s",
                t2_disp_str, t_singing_loud_duration, singing_is_successful_realtime ? "YES" : "NO");
        al_draw_text(font, al_map_rgb(220,220,180), SCREEN_WIDTH / 2.0f, base_text_y + 20, ALLEGRO_ALIGN_CENTER, display_text_buffer);

    // } else if (time_end_button_press > 0.0 && time_start_button_press > 0.0 && !is_in_countdown_animation) {  // OBSOLETE BLOCK
        // Fallback for other post-singing scenarios if t4 is not yet set (e.g. if stopped by ESC)
        // This part might become less relevant with the t4 logic.
        // char t1_disp_str[20], t2_disp_str[20];
        // if (t1_countdown_finish_time <= 0.0) strcpy(t1_disp_str, "N/A"); else sprintf(t1_disp_str, "%.2fs", t1_countdown_finish_time);
        // if (t2_singing_start_time < 0.0) strcpy(t2_disp_str, "N/A"); else sprintf(t2_disp_str, "%.2fs", t2_singing_start_time);

        // sprintf(display_text_buffer, "Button: %.2f-%.2f | t1: %s, t2: %s, t_singing: %.2fs",
        //         time_start_button_press, time_end_button_press, 
        //         t1_disp_str, t2_disp_str, t_singing_loud_duration);
        // float text_y_pos = minigame_buttons[0].y - 60 + y_offset; 
        // al_draw_text(font, al_map_rgb(220, 220, 180), SCREEN_WIDTH / 2.0f, text_y_pos, ALLEGRO_ALIGN_CENTER, display_text_buffer);
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
    // else if (is_singing && isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL) { // OLD condition
    else if (is_singing && !is_in_countdown_animation && isActuallyRecording && hWaveIn != NULL && waveform_buffer != NULL && waveform_buffer_size_samples > 0) {
        // DWORD bytes_per_sample = AUDIO_BITS_PER_SAMPLE / 8; // Not needed directly for drawing
        // DWORD total_samples_recorded_so_far = waveHdr.dwBytesRecorded / bytes_per_sample; // Not applicable anymore
        int num_samples_to_display = waveform_buffer_size_samples; // We display the whole waveform_buffer

        if (num_samples_to_display > 1 && WAVEFORM_AREA_WIDTH > 1) {
            // short* all_samples = (short*)pWaveBuffer; // Changed to waveform_buffer
            short* display_samples = waveform_buffer;
            float prev_x_pos = WAVEFORM_AREA_X;
            
            // The waveform_buffer is circular. waveform_buffer_write_pos is the next spot to write.
            // So, the oldest sample is at waveform_buffer_write_pos (if buffer is full).
            // The newest sample is at (waveform_buffer_write_pos - 1 + waveform_buffer_size_samples) % waveform_buffer_size_samples.
            // We want to draw from oldest to newest.

            // Create a temporary linear copy for easier drawing if the display would wrap around.
            // This avoids complex logic in the drawing loop.
            short temp_display_copy[waveform_buffer_size_samples];
            int current_read_pos = waveform_buffer_write_pos;
            for(int i=0; i<num_samples_to_display; ++i) {
                temp_display_copy[i] = display_samples[current_read_pos];
                current_read_pos = (current_read_pos + 1) % waveform_buffer_size_samples;
            }
            // Now temp_display_copy contains the waveform data in linear order (oldest to newest)

            if (num_samples_to_display > 1) { // samples_available_in_window changed to num_samples_to_display
                short first_sample_val = temp_display_copy[0]; // all_samples[start_buffer_idx] changed
                float prev_y_pos = WAVEFORM_AREA_Y + (WAVEFORM_AREA_HEIGHT / 2.0f) -
                                   (first_sample_val / 32767.0f) * (WAVEFORM_AREA_HEIGHT / 2.0f);
                prev_y_pos = fmaxf(WAVEFORM_AREA_Y + 1.0f, fminf(WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT - 1.0f, prev_y_pos));

                for (int screen_pixel_x = 1; screen_pixel_x < WAVEFORM_AREA_WIDTH; ++screen_pixel_x) {
                    float proportion = (WAVEFORM_AREA_WIDTH > 1) ? ((float)screen_pixel_x / (WAVEFORM_AREA_WIDTH - 1)) : 0.0f;
                    int sample_idx_in_display_copy = (int)(proportion * (num_samples_to_display - 1));
                    // int current_sample_idx_in_buffer = start_buffer_idx + sample_offset_in_selected_window; // Old logic

                    // if(current_sample_idx_in_buffer >= total_samples_recorded_so_far) current_sample_idx_in_buffer = total_samples_recorded_so_far -1; // Old
                    // if(current_sample_idx_in_buffer < 0) current_sample_idx_in_buffer = 0; // Old

                    short sample_value = temp_display_copy[sample_idx_in_display_copy]; // all_samples changed to temp_display_copy
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
    if (displayPleaseSingMessage && !is_in_countdown_animation && t4_complete_button_press_time > 0.0) { 
        char failure_reason[128] = "Plant growth conditions not met.";
        bool conditionA = (t4_complete_button_press_time - t1_countdown_finish_time) > MIN_TOTAL_SESSION_DURATION_FOR_GROWTH;
        
        if (!conditionA) {
            sprintf(failure_reason, "Session too short (must be > %.0fs). Current: %.1fs", 
                    MIN_TOTAL_SESSION_DURATION_FOR_GROWTH, 
                    (t4_complete_button_press_time - t1_countdown_finish_time));
        } else if (!singing_is_successful_realtime) {
            // More specific reasons for singing_is_successful_realtime failure could be added if tracked
            strcpy(failure_reason, "Singing quality criteria not met during session.");
        }

        float msg_y_pos = minigame_buttons[0].y - 40 + y_offset; 
        al_draw_text(font, al_map_rgb(255, 100, 100), SCREEN_WIDTH / 2.0f, msg_y_pos, ALLEGRO_ALIGN_CENTER, failure_reason);
    } else if (displayPleaseSingMessage && !is_in_countdown_animation && !(t4_complete_button_press_time > 0.0) ) {
        // Generic message if displayPleaseSingMessage is true but not from a completion attempt (e.g. old logic path)
        char validation_msg[128];
        sprintf(validation_msg, "Please try singing clearly for a longer duration.");
        float msg_y_pos = minigame_buttons[0].y - 40 + y_offset;
        al_draw_text(font, al_map_rgb(255, 100, 100), SCREEN_WIDTH / 2.0f, msg_y_pos, ALLEGRO_ALIGN_CENTER, validation_msg);
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
                    time_start_button_press = 0.0; // Reset, will be set to t1
                    // time_end_button_press = 0.0; // Obsolete
                    // has_achieved_lockable_success = false; // Obsolete (already commented out)
                }
                else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower && minigame_buttons[1].is_hovered) {
                    is_in_countdown_animation = true;
                    countdown_start_time = al_get_time();
                    countdown_value = 3;
                    background_rms_calculation_locked = false; // 解鎖背景音量計算
                    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS; // 重設背景音量猜測
                    accumulated_rms_sum = 0.0f; // 初始化 RMS 累加器
                    rms_samples_count = 0;      // 初始化 RMS 計數器
                    
                    // 清理先前的計時和狀態 for a fresh start
                    // actual_singing_start_time = -1.0; // Replaced
                    // current_actual_singing_end_time = -1.0; // Replaced
                    // locked_successful_singing_end_time = -1.0; // Removed
                    // has_achieved_lockable_success = false; // Removed
                    t1_countdown_finish_time = 0.0;
                    t2_singing_start_time = -1.0;
                    t_singing_loud_duration = 0.0;
                    current_loud_streak_start_time = -1.0;
                    is_currently_loud_for_streak = false;

                    // internal_was_loud_last_frame = false; // Replaced
                    // internal_current_loud_streak_began_at_time = 0.0; // Replaced
                    displayPleaseSingMessage = false;
                    current_recording_elapsed_time_sec = 0.0f;

                    start_actual_audio_recording(); 
                    printf("小遊戲: 開始唱歌按鈕點擊，進入倒數計時。\n");
                    button_clicked_this_event = true;
                }
                else if (is_singing) { // 正在唱歌時 (倒數已結束)
                    if (minigame_buttons[2].is_hovered) { // 點擊 "重新開始"
                        // Stop current recording first
                        if(isActuallyRecording && hWaveIn) {
                            stop_actual_audio_recording(); // This will set isActuallyRecording to false and unprepare headers
                        }
                        is_singing = false; // Explicitly set singing to false before countdown

                        // Enter countdown again
                        is_in_countdown_animation = true; 
                        countdown_start_time = al_get_time();
                        countdown_value = 3;
                        background_rms_calculation_locked = false; 
                        current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
                        accumulated_rms_sum = 0.0f; // 初始化 RMS 累加器
                        rms_samples_count = 0;      // 初始化 RMS 計數器
                        
                        // Clear timing and state variables
                        // actual_singing_start_time = -1.0; // Replaced
                        // current_actual_singing_end_time = -1.0; // Replaced
                        // locked_successful_singing_end_time = -1.0; // Removed
                        // has_achieved_lockable_success = false; // Removed
                        t1_countdown_finish_time = 0.0;
                        t2_singing_start_time = -1.0;
                        t_singing_loud_duration = 0.0;
                        current_loud_streak_start_time = -1.0;
                        is_currently_loud_for_streak = false;
                        // internal_was_loud_last_frame = false; // Replaced
                        // internal_current_loud_streak_began_at_time = 0.0; // Replaced
                        current_recording_elapsed_time_sec = 0.0f;

                        start_actual_audio_recording(); 
                        printf("小遊戲: 重新開始唱歌按鈕點擊，進入倒數計時。\n");
                        button_clicked_this_event = true;
                    }
                    else if (minigame_buttons[3].is_hovered) { // 點擊 "完成歌唱"
                        // bool sound_was_valid = stop_actual_audio_recording(); // This was an error from merge, stop_actual_audio_recording is called once
                        t4_complete_button_press_time = al_get_time();
                        bool plant_should_grow = stop_actual_audio_recording(); // stop_actual_audio_recording now returns the growth condition
                        is_singing = false; 
                        
                        printf("小遊戲: 完成歌唱按鈕點擊 (t4: %.2f).\n", t4_complete_button_press_time);

                        if (plant_should_grow) {
                            if (flower_plant.songs_sung < songs_to_flower) { flower_plant.songs_sung++; }
                            flower_plant.growth_stage = flower_plant.songs_sung;
                            if(flower_plant.growth_stage == songs_to_flower){ flower_plant.which_flower = (rand() % 2); }
                            printf("DEBUG: Plant growth conditions met! Plant grows.\n");
                            displayPleaseSingMessage = false;
                        } else {
                            printf("DEBUG: Plant growth conditions NOT met.\n");
                            displayPleaseSingMessage = true; // Message will be shown by render logic
                        }
                        current_recording_elapsed_time_sec = 0.0f; // Reset for display
                        current_volume_is_loud_enough = false;     // Reset for display
                        // t1, t2, t_singing, singing_is_successful_realtime will be reset by next "Start Singing"
                        button_clicked_this_event = true;
                    }
                }
                else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower && minigame_buttons[5].is_hovered) {
                    if (flower_plant.which_flower == 0) { player.item_quantities[0]++; printf("DEBUG: 採收了一朵普通花! 總數: %d\n", player.item_quantities[0]); }
                    else { player.item_quantities[1]++; printf("DEBUG: 採收了一朵惡魔花. 總數: %d\n", player.item_quantities[1]); }
                    flower_plant.songs_sung = 0; flower_plant.growth_stage = 0;
                    seed_planted = false; displayPleaseSingMessage = false; button_clicked_this_event = true;
                    time_start_button_press = 0.0; // Reset t1 related marker
                    // time_end_button_press = 0.0; // Obsolete
                    t1_countdown_finish_time = 0.0; 
                    t2_singing_start_time = -1.0;
                    t_singing_loud_duration = 0.0;
                    t4_complete_button_press_time = 0.0;
                    singing_is_successful_realtime = false;
                }
            }
            if (minigame_buttons[4].is_hovered) {
                if((is_singing || is_in_countdown_animation) && isActuallyRecording && hWaveIn) {
                    // waveInReset(hWaveIn); // cleanup_audio_recording or stop_actual_audio_recording will handle
                    stop_actual_audio_recording(); 
                }
                is_singing = false; 
                is_in_countdown_animation = false;
                cleanup_audio_recording(); // Full cleanup
                game_phase = GROWTH;
                button_clicked_this_event = true;
            }
            if (button_clicked_this_event) { for (int i = 0; i < NUM_MINIGAME1_BUTTONS; ++i) { minigame_buttons[i].is_hovered = false; } }
        }
    }
    else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            if((is_singing || is_in_countdown_animation) && isActuallyRecording && hWaveIn) {
                // waveInReset(hWaveIn); // cleanup_audio_recording or stop_actual_audio_recording will handle
                stop_actual_audio_recording();
            }
            is_singing = false; 
            is_in_countdown_animation = false;
            cleanup_audio_recording();
            game_phase = GROWTH;
        }
    }
}

void update_minigame1(void) {
    // double current_time_for_update = al_get_time(); // Unused variable
    // static int local_last_processed_idx = -1; // Moved to file scope
    double current_time = al_get_time(); // Use a consistent time for this update cycle

    if (is_in_countdown_animation) {
        double elapsed_countdown_time = current_time - countdown_start_time;
        if (elapsed_countdown_time < 1.0) countdown_value = 3;
        else if (elapsed_countdown_time < 2.0) countdown_value = 2;
        else if (elapsed_countdown_time < 3.0) countdown_value = 1;
        else { // 倒數結束
            is_in_countdown_animation = false;
            is_singing = true; 

            // Finalize background RMS calculation
            if (rms_samples_count > 0) {
                current_background_rms = accumulated_rms_sum / rms_samples_count;
            } else {
                current_background_rms = INITIAL_BACKGROUND_RMS_GUESS; 
                printf("警告: 倒數期間未收集到 RMS 樣本，使用預設背景 RMS。\n");
            }
            if (current_background_rms < 50) current_background_rms = 50; 
            background_rms_calculation_locked = true; 
            accumulated_rms_sum = 0.0f; 
            rms_samples_count = 0;

            // Record t1 and reset singing detection variables
            t1_countdown_finish_time = current_time;
            allegro_recording_start_time = current_time; // Keep this for overall duration if needed elsewhere
            time_start_button_press = current_time;      // Corresponds to t1

            t2_singing_start_time = -1.0;
            t_singing_loud_duration = 0.0;
            current_loud_streak_start_time = -1.0;
            is_currently_loud_for_streak = false;
            current_volume_is_loud_enough = false; 
            singing_is_successful_realtime = false; // Reset real-time success flag
            
            // Comment out old state variables reset here as they are globally replaced/commented
            // actual_singing_start_time = -1.0;
            // current_actual_singing_end_time = -1.0;
            // locked_successful_singing_end_time = -1.0;
            // has_achieved_lockable_success = false;
            // internal_was_loud_last_frame = false;
            // internal_current_loud_streak_began_at_time = 0.0;
            
            current_recording_elapsed_time_sec = 0.0f; // Reset elapsed time
            displayPleaseSingMessage = false;
            local_last_processed_idx = -1; 

            printf("DEBUG: Countdown finished. t1=%.2f. Background RMS locked: %.0f. Singing started.\n", t1_countdown_finish_time, current_background_rms);
        }

        // Background RMS calculation during countdown
        // if (isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL && !background_rms_calculation_locked) { // OLD
        if (isActuallyRecording && hWaveIn != NULL && !background_rms_calculation_locked) {
            // Check if a new buffer has been filled by the callback
            if (processed_buffer_idx != local_last_processed_idx && processed_buffer_idx != -1) {
                int buffer_to_analyze = processed_buffer_idx;
                local_last_processed_idx = buffer_to_analyze; // Mark as processed by this function for this cycle

                short* samples_from_chunk = (short*)waveHdrs[buffer_to_analyze].lpData;
                int samples_in_chunk = waveHdrs[buffer_to_analyze].dwBytesRecorded / (AUDIO_BITS_PER_SAMPLE / 8);

                if (samples_in_chunk > AUDIO_CHUNK_SAMPLES / 2) { // Only process if significant data
                    float latest_rms = calculate_rms(samples_from_chunk, samples_in_chunk);
                    // current_background_rms = (current_background_rms * 0.90f) + (latest_rms * 0.10f); // OLD AVERAGING REMOVED
                    accumulated_rms_sum += latest_rms;
                    rms_samples_count++;
                    // if (current_background_rms < 50) current_background_rms = 50; // Clamping moved to end of countdown
                     // printf("DEBUG: Countdown RMS update from buffer %d, latest_rms=%.1f, accumulated_sum=%.1f, count=%d\n", buffer_to_analyze, latest_rms, accumulated_rms_sum, rms_samples_count);
                }
            }
        }
    }
    // else if (is_singing && isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL) { // OLD
    else if (is_singing && isActuallyRecording && hWaveIn != NULL) { 
        current_recording_elapsed_time_sec = (float)(current_time - t1_countdown_finish_time); // Elapsed time since t1

        if (processed_buffer_idx != local_last_processed_idx && processed_buffer_idx != -1) {
            int current_chunk_idx = processed_buffer_idx;
            local_last_processed_idx = current_chunk_idx; 

            short* samples_in_chunk = (short*)waveHdrs[current_chunk_idx].lpData;
            int num_samples_in_chunk = waveHdrs[current_chunk_idx].dwBytesRecorded / (AUDIO_BITS_PER_SAMPLE / 8);
            bool chunk_is_loud = false;

            if (num_samples_in_chunk > 0) {
                float current_rms_val = calculate_rms(samples_in_chunk, num_samples_in_chunk);
                chunk_is_loud = (current_rms_val > current_background_rms * SINGING_RMS_THRESHOLD_MULTIPLIER &&
                                 current_rms_val > MIN_ABSOLUTE_RMS_FOR_SINGING);
                current_volume_is_loud_enough = chunk_is_loud; // For UI
            } else {
                current_volume_is_loud_enough = false; // No data, so not loud
            }

            // Detect t2 (Singing Start)
            if (t2_singing_start_time < 0) { // t2 not yet found
                if (chunk_is_loud) {
                    if (current_loud_streak_start_time < 0) { // Start of a new potential streak
                        // Approximate start of this chunk's time
                        current_loud_streak_start_time = current_time - (AUDIO_CHUNK_DURATION_MS / 1000.0); 
                    }
                    // Check if current streak meets minimum duration for t2
                    if ((current_time - current_loud_streak_start_time) >= SINGING_DETECTION_MIN_STREAK_DURATION) {
                        t2_singing_start_time = current_loud_streak_start_time;
                        t_singing_loud_duration = SINGING_DETECTION_MIN_STREAK_DURATION; // Initialize with the streak duration
                        printf("DEBUG: t2 detected at %.2fs. Initial t_singing_loud_duration: %.2fs\n", t2_singing_start_time, t_singing_loud_duration);
                    }
                } else { // Chunk is not loud, break any current streak
                    current_loud_streak_start_time = -1.0;
                }
            } else { // t2 has already been found, now accumulate t_singing
                if (chunk_is_loud) {
                    // Add this chunk's duration to t_singing_loud_duration
                    // This handles continuous singing after t2 is established.
                    // The initial SINGING_DETECTION_MIN_STREAK_DURATION is already in t_singing_loud_duration.
                    // We only add if this chunk is loud AND it's *not* the one that just completed the t2 streak.
                    // To simplify, if t2 was set in a *previous* chunk's processing, this chunk (if loud) adds its full duration.
                    // If t2 was set *by this current chunk* making the streak long enough, its duration is already accounted for.
                    // A slightly more precise way:
                    // If this is the first loud chunk AFTER t2 was established (or the one that established it)
                    // ensure t_singing_loud_duration reflects time since t2.
                    // The current logic: t_singing_loud_duration gets MIN_STREAK when t2 is set.
                    // For subsequent loud chunks, we add their duration.
                    // This seems correct.
                     t_singing_loud_duration += (AUDIO_CHUNK_DURATION_MS / 1000.0);
                }
            }
            is_currently_loud_for_streak = chunk_is_loud; // Update for next iteration's streak logic

            // Real-time success check
            if (t2_singing_start_time >= 0 && !singing_is_successful_realtime) {
                double t3_current_time = current_time; // al_get_time() called at start of update_minigame1
                double duration_since_t2 = t3_current_time - t2_singing_start_time;

                bool condition1_duration_met = (duration_since_t2 > SINGING_TOTAL_TIME_THRESHOLD);
                bool condition2_proportion_met = false;
                if (duration_since_t2 > 0.001) { // Avoid division by zero, ensure some time has passed
                    condition2_proportion_met = (t_singing_loud_duration / duration_since_t2) > REQUIRED_SINGING_PROPORTION;
                }

                if (condition1_duration_met && condition2_proportion_met) {
                    singing_is_successful_realtime = true;
                    printf("DEBUG: Real-time singing success achieved! duration_since_t2: %.2f, t_singing: %.2f, proportion: %.2f\n",
                           duration_since_t2, t_singing_loud_duration, t_singing_loud_duration / duration_since_t2);
                }
            }

        } // End of new chunk processing
        
        // Old logic based on internal_was_loud_last_frame and has_achieved_lockable_success is removed.
        // The new t2_singing_start_time and t_singing_loud_duration are the primary outputs.

    } else if (!is_in_countdown_animation) { // Not singing, not in countdown
        current_volume_is_loud_enough = false;
        // Reset streak variables if not actively singing (e.g., after stop, or before first start)
        current_loud_streak_start_time = -1.0;
        is_currently_loud_for_streak = false;
        // singing_is_successful_realtime is reset when singing starts (countdown finishes)
    }
}

static void prepare_audio_recording(void) {
    // 初始化所有 pAudioBuffers 為 NULL 以便安全清理
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        pAudioBuffers[i] = NULL;
    }
    waveform_buffer = NULL; 

    // 分配多個音訊緩衝區
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        pAudioBuffers[i] = (char*)malloc(SINGLE_BUFFER_SIZE);
        if (pAudioBuffers[i] == NULL) {
            fprintf(stderr, "嚴重錯誤：無法分配音訊緩衝區 %d。\n", i);
            // 清理已分配的緩衝區
            for (int j = 0; j < i; ++j) { // 只釋放已成功分配的
                if(pAudioBuffers[j]) free(pAudioBuffers[j]);
                pAudioBuffers[j] = NULL;
            }
            // 不需要關閉 hWaveIn，因為此時它還未開啟
            return; // 從函數返回，表示準備失敗
        }
        ZeroMemory(pAudioBuffers[i], SINGLE_BUFFER_SIZE); // 初始化緩衝區
        printf("DEBUG: Audio buffer %d allocated (%d bytes).\n", i, (int)SINGLE_BUFFER_SIZE); // Corrected %lu to %d and cast
    }

    // 分配波形顯示緩衝區
    waveform_buffer_size_samples = WAVEFORM_DISPLAY_SAMPLES;
    waveform_buffer = (short*)malloc(waveform_buffer_size_samples * sizeof(short));
    if (waveform_buffer == NULL) {
        fprintf(stderr, "嚴重錯誤：無法分配波形顯示緩衝區。\n");
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) { // 清理所有音訊塊緩衝區
            if (pAudioBuffers[i]) {
                free(pAudioBuffers[i]);
                pAudioBuffers[i] = NULL;
            }
        }
        // 不需要關閉 hWaveIn
        return; // 準備失敗
    }
    ZeroMemory(waveform_buffer, waveform_buffer_size_samples * sizeof(short));
    waveform_buffer_write_pos = 0;
    printf("DEBUG: Waveform buffer allocated (%d samples, %lu bytes).\n", waveform_buffer_size_samples, (unsigned long)((size_t)waveform_buffer_size_samples * sizeof(short))); // Changed %zu to %llu and cast, NOW FIXED

    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM; 
    wfx.nChannels = AUDIO_CHANNELS; 
    wfx.nSamplesPerSec = AUDIO_SAMPLE_RATE;
    wfx.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8);
    wfx.nBlockAlign = AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8); 
    wfx.wBitsPerSample = AUDIO_BITS_PER_SAMPLE; 
    wfx.cbSize = 0;

    // 修改 waveInOpen 以使用回呼函式
    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)waveInProc, (DWORD_PTR)NULL, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        char error_text[256]; 
        waveInGetErrorTextA(result, error_text, sizeof(error_text));
        fprintf(stderr, "錯誤：無法開啟音訊錄製裝置 (錯誤 %u: %s)。\n請確認麥克風已連接並啟用。\n", result, error_text);
        // 清理已分配的記憶體
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
            if (pAudioBuffers[i]) { free(pAudioBuffers[i]); pAudioBuffers[i] = NULL; }
        }
        if (waveform_buffer) { free(waveform_buffer); waveform_buffer = NULL; }
        hWaveIn = NULL; // 確保 hWaveIn 為 NULL 表示開啟失敗
        return; // 準備失敗
    }
    printf("DEBUG: Windows 音訊錄製已準備。裝置已開啟 (使用回呼)。hWaveIn = %p\n", hWaveIn);
}


// waveInProc 回呼函式實現
static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    switch (uMsg) {
        case WIM_OPEN:
            printf("DEBUG: waveInProc - WIM_OPEN received. hWaveIn = %p\n", hwi);
            // 通常不需要做特別處理，除非要基於 dwInstance 做事
            break;
        case WIM_DATA:
        {
            WAVEHDR* pDoneHeader = (WAVEHDR*)dwParam1;
            // printf("DEBUG: waveInProc - WIM_DATA. Header Addr: %p, Bytes: %lu, Flags: %lu\n", pDoneHeader, pDoneHeader->dwBytesRecorded, pDoneHeader->dwFlags);

            if (pDoneHeader->dwBytesRecorded > 0) {
                int filled_buffer_idx = -1;
                for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
                    if (&waveHdrs[i] == pDoneHeader) {
                        filled_buffer_idx = i;
                        break;
                    }
                }

                if (filled_buffer_idx != -1) {
                    // 標記此緩衝區已準備好被主循環處理
                    // 為了避免與主循環的 processed_buffer_idx 產生競爭，
                    // 這裡可以考慮使用更安全的機制，例如原子操作或訊息隊列。
                    // 簡單起見，我們直接設定 volatile 變數。
                    processed_buffer_idx = filled_buffer_idx;
                    // printf("DEBUG: waveInProc - Buffer %d is ready for processing (processed_buffer_idx set to %d).\n", filled_buffer_idx, processed_buffer_idx);


                    // 將數據複製到 waveform_buffer (循環)
                    if (waveform_buffer != NULL && waveform_buffer_size_samples > 0) {
                        int samples_in_chunk = pDoneHeader->dwBytesRecorded / (AUDIO_BITS_PER_SAMPLE / 8);
                        short* chunk_samples = (short*)pDoneHeader->lpData;
                        for (int i = 0; i < samples_in_chunk; ++i) {
                            waveform_buffer[waveform_buffer_write_pos] = chunk_samples[i];
                            waveform_buffer_write_pos = (waveform_buffer_write_pos + 1) % waveform_buffer_size_samples;
                        }
                        // printf("DEBUG: waveInProc - Copied %d samples to waveform_buffer. write_pos now %d\n", samples_in_chunk, waveform_buffer_write_pos);
                    }
                } else {
                    fprintf(stderr, "waveInProc Error: WIM_DATA received for an unknown WAVEHDR!\n");
                }
            } else {
                // printf("DEBUG: waveInProc - WIM_DATA received with 0 bytes recorded. Header Addr: %p\n", pDoneHeader);
            }

            // 重新將此緩衝區加入佇列，除非錄製已停止
            // isActuallyRecording 應該由主線程控制，並在呼叫 waveInReset/Stop 之前設定為 false
            if (isActuallyRecording && hWaveIn != NULL) {
                 // 在重新加入前，確保 dwFlags 是乾淨的 (waveInAddBuffer 要求 dwFlags 為 0)
                 // pDoneHeader->dwFlags = 0; // waveInPrepareHeader 應該已處理，但再次確認無害
                 // dwBufferLength 應該保持不變
                MMRESULT add_res = waveInAddBuffer(hwi, pDoneHeader, sizeof(WAVEHDR));
                if (add_res != MMSYSERR_NOERROR) {
                    char error_text[256];
                    waveInGetErrorTextA(add_res, error_text, sizeof(error_text));
                    fprintf(stderr, "waveInProc: waveInAddBuffer error %u: %s. Header Addr: %p\n", add_res, error_text, pDoneHeader);
                    // 嚴重的錯誤，可能需要通知主線程停止錄製
                    isActuallyRecording = false; // 嘗試停止進一步的錄製
                } else {
                    // printf("DEBUG: waveInProc - Buffer %p re-added to queue.\n", pDoneHeader);
                }
            } else {
                 // printf("DEBUG: waveInProc - Not re-adding buffer %p because isActuallyRecording is false or hWaveIn is NULL.\n", pDoneHeader);
            }
            break;
        }
        case WIM_CLOSE:
            printf("DEBUG: waveInProc - WIM_CLOSE received. hWaveIn = %p\n", hwi);
            // 裝置已關閉。通常不需要在此做清理，因為 waveInClose 的呼叫者會處理。
            // isActuallyRecording 應該已經是 false。
            break;
        default:
            // printf("DEBUG: waveInProc - Unknown message %u\n", uMsg);
            break;
    }
}


static void start_actual_audio_recording(void) {
    if (hWaveIn == NULL) {
        // prepare_audio_recording should have been called and hWaveIn would be NULL if it failed.
        // This might be called if prepare_audio_recording was called in init, and device was not opened before.
        // Defensive call to prepare_audio_recording if hWaveIn is NULL for some reason.
        printf("DEBUG: start_actual_audio_recording - hWaveIn is NULL, calling prepare_audio_recording.\n");
        prepare_audio_recording(); 
        if (hWaveIn == NULL) { // Check again if prepare_audio_recording succeeded
            fprintf(stderr, "音訊系統未就緒。無法開始錄製。\n");
            is_singing = false; 
            is_in_countdown_animation = false; 
            isActuallyRecording = false;
            return;
        }
    }

    // isActuallyRecording should be true here to allow waveInAddBuffer in callback
    isActuallyRecording = true; 
    current_buffer_idx = 0;
    processed_buffer_idx = -1; // Reset processed buffer index
    local_last_processed_idx = -1; // Reset for update_minigame1's tracking
    waveform_buffer_write_pos = 0; // Reset waveform buffer write position
    if(waveform_buffer && waveform_buffer_size_samples > 0) { // Check waveform_buffer_size_samples too
      ZeroMemory(waveform_buffer, waveform_buffer_size_samples * sizeof(short)); // Clear old waveform data
    }


    // Prepare and add all buffers
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        // If header was prepared from a previous recording and not cleaned up properly (should not happen in normal flow)
        if (waveHdrs[i].dwFlags & WHDR_PREPARED) {
            printf("警告: Buffer %d was already prepared in start_actual_audio_recording. Unpreparing.\n", i);
            MMRESULT unprep_res = waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
            if (unprep_res != MMSYSERR_NOERROR && unprep_res != WAVERR_UNPREPARED) {
                fprintf(stderr, "start_actual_audio_recording: waveInUnprepareHeader error for buffer %d: %u\n", i, unprep_res);
            }
        }

        ZeroMemory(&waveHdrs[i], sizeof(WAVEHDR)); // Clear the header structure
        waveHdrs[i].lpData = pAudioBuffers[i];
        waveHdrs[i].dwBufferLength = SINGLE_BUFFER_SIZE;
        waveHdrs[i].dwFlags = 0; // IMPORTANT: flags must be 0 before calling waveInPrepareHeader / waveInAddBuffer
        // ZeroMemory(pAudioBuffers[i], SINGLE_BUFFER_SIZE); // Ensure buffer is clean (already done in prepare_audio_recording)

        MMRESULT prep_res = waveInPrepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        if (prep_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(prep_res, err_text, 256);
            fprintf(stderr, "準備 wave 標頭 %d 錯誤: %u (%s)\n", i, prep_res, err_text);
            isActuallyRecording = false; // Set back to false as we cannot start
            // Clean up headers prepared so far
            for (int j = 0; j < i; ++j) { // Only unprepare those successfully prepared
                if (waveHdrs[j].dwFlags & WHDR_PREPARED) waveInUnprepareHeader(hWaveIn, &waveHdrs[j], sizeof(WAVEHDR));
            }
            return;
        }

        MMRESULT add_res = waveInAddBuffer(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        if (add_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(add_res, err_text, 256);
            fprintf(stderr, "新增 wave 緩衝區 %d 錯誤: %u (%s)\n", i, add_res, err_text);
            isActuallyRecording = false; // Set back to false
            // Clean up all headers that were successfully prepared (including this one as prepare was successful)
            for (int j = 0; j <= i; ++j) { // up to and including current index i
                 if (waveHdrs[j].dwFlags & WHDR_PREPARED) waveInUnprepareHeader(hWaveIn, &waveHdrs[j], sizeof(WAVEHDR));
            }
            return;
        }
        printf("DEBUG: Buffer %d (%p) prepared and added to queue.\n", i, &waveHdrs[i]);
    }

    MMRESULT start_res = waveInStart(hWaveIn);
    if (start_res != MMSYSERR_NOERROR) {
        char err_text[256]; waveInGetErrorTextA(start_res, err_text, 256);
        fprintf(stderr, "開始 wave 輸入錯誤: %u (%s)\n", start_res, err_text);
        isActuallyRecording = false; // Set back to false
        // Clean up all prepared headers
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
            if (waveHdrs[i].dwFlags & WHDR_PREPARED) waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        }
        return;
    }

    // isActuallyRecording was set true at the beginning of this function.
    printf("[DEBUG][Audio] start_actual_audio_recording() SUCCEEDED. hWaveIn=%p\n", hWaveIn);
    printf("[DEBUG][Audio] isActuallyRecording=%d. current_buffer_idx=%d. processed_buffer_idx=%d.\n", isActuallyRecording, current_buffer_idx, processed_buffer_idx);
    printf("[DEBUG][Audio] Waiting for countdown to finish to set singing start time.\n");
}

static bool stop_actual_audio_recording(void) {
    if (!isActuallyRecording || hWaveIn == NULL) {
        printf("[DEBUG][Audio] stop_actual_audio_recording() called but not actually recording or hWaveIn is NULL. isActuallyRecording=%d, hWaveIn=%p\n", isActuallyRecording, hWaveIn);
        return false; 
    }

    printf("[DEBUG][Audio] stop_actual_audio_recording() called. hWaveIn=%p\n", hWaveIn);
    // t4_complete_button_press_time is set in handle_minigame1_input just before this.

    // 1. Set isActuallyRecording to false. This prevents waveInProc from re-adding buffers.
    isActuallyRecording = false; 
    printf("[DEBUG][Audio] isActuallyRecording set to false for stop.\n");

    // 2. Call waveInReset.
    // This stops recording and marks all pending buffers as done (MM_WIM_DONE),
    // returning them to the application via MM_WIM_DATA.
    // waveInProc will be called for each buffer.
    MMRESULT reset_res = waveInReset(hWaveIn); 
    if (reset_res != MMSYSERR_NOERROR) { 
        char err_text[256]; waveInGetErrorTextA(reset_res, err_text, 256);
        fprintf(stderr, "重置 wave 輸入錯誤: %u (%s)\n", reset_res, err_text); 
        // Continue with cleanup even if reset fails
    } else {
        printf("DEBUG: waveInReset called successfully. Pending buffers should now be returned via WIM_DATA.\n");
    }
    
    // waveInStop is technically not needed as waveInReset already stops input.
    // MMRESULT stop_res = waveInStop(hWaveIn);
    // if (stop_res != MMSYSERR_NOERROR) { fprintf(stderr, "停止 wave 輸入錯誤: %u\n", stop_res); }

    // At this point, waveInProc should have handled all WIM_DATA messages for buffers returned by waveInReset.
    // The main loop's update_minigame1 should pick up the last processed_buffer_idx and process it.
    
    printf("DEBUG: Windows 錄製停止於 t4=%.2f.\n", t4_complete_button_press_time);
    printf("DEBUG: t1: %.2f, t2: %.2f, t_singing_loud_duration: %.2f\n",
           t1_countdown_finish_time, t2_singing_start_time, t_singing_loud_duration);
    
    // displayPleaseSingMessage is handled in handle_minigame1_input based on this function's return.

    // New Plant Growth Validation Logic
    bool conditionA_total_duration = false;
    double session_duration = 0.0;

    if (t1_countdown_finish_time > 0 && t4_complete_button_press_time > t1_countdown_finish_time) {
        session_duration = t4_complete_button_press_time - t1_countdown_finish_time;
        conditionA_total_duration = (session_duration > MIN_TOTAL_SESSION_DURATION_FOR_GROWTH);
    }
    
    bool conditionB_singing_quality = singing_is_successful_realtime;
    bool plant_should_grow = conditionA_total_duration && conditionB_singing_quality;

    printf("DEBUG stop_actual_audio_recording: t1=%.2f, t4=%.2f, SessionDuration=%.2f (Req: >%.1f) -> ConditionA: %s\n",
        t1_countdown_finish_time, t4_complete_button_press_time, 
        session_duration, MIN_TOTAL_SESSION_DURATION_FOR_GROWTH,
        conditionA_total_duration ? "Met" : "Not Met");
    printf("DEBUG stop_actual_audio_recording: singing_is_successful_realtime -> ConditionB: %s\n", 
        conditionB_singing_quality ? "Met" : "Not Met");
    printf("DEBUG stop_actual_audio_recording: Final Plant should grow: %s\n", plant_should_grow ? "YES" : "NO");

    // Old validation logic removed.
    // force_audio_too_short_for_test is not used in this new path.

    // 4. Unprepare all headers
    // After waveInReset, all buffers are returned via WIM_DATA (and waveInProc won't re-add them as isActuallyRecording is false).
    // It's now safe to unprepare them.
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        if (waveHdrs[i].dwFlags & WHDR_PREPARED) {
            MMRESULT unprep_res = waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
            if (unprep_res != MMSYSERR_NOERROR) {
                 // WAVERR_UNPREPARED means it wasn't prepared or already unprepared. Usually not an issue here.
                if (unprep_res != WAVERR_UNPREPARED) { // Log only if it's a different error
                     char err_text[256]; waveInGetErrorTextA(unprep_res, err_text, 256);
                     fprintf(stderr, "停止時取消準備 wave 標頭 %d (%p) 錯誤: %u (%s)\n", i, &waveHdrs[i], unprep_res, err_text);
                }
            } else {
                // printf("DEBUG: Buffer %d (%p) unprepared successfully during stop.\n", i, &waveHdrs[i]);
            }
        } else {
            // printf("DEBUG: Buffer %d (%p) was not prepared during stop, no need to unprepare.\n", i, &waveHdrs[i]);
        }
    }
    
    // isActuallyRecording was set to false before waveInReset.
    // hWaveIn is typically closed in cleanup_audio_recording, unless decided to close here.
    // For consistency, let cleanup_audio_recording handle waveInClose.
    
    return plant_should_grow; // Return the new growth condition
}

static void cleanup_audio_recording(void) {
    printf("DEBUG: Windows cleanup_audio_recording() 已呼叫。isActuallyRecording=%d, hWaveIn=%p\n", isActuallyRecording, hWaveIn);
    
    // 1. If still recording, stop it (though normal flow should call stop_actual_audio_recording first)
    if (isActuallyRecording && hWaveIn != NULL) { // Check isActuallyRecording as well
        printf("DEBUG: cleanup_audio_recording - Still recording, calling waveInReset.\n");
        isActuallyRecording = false; // Prevent callback from re-adding buffers
        MMRESULT reset_res = waveInReset(hWaveIn); 
        if (reset_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(reset_res, err_text, 256);
            fprintf(stderr, "清理期間重置 wave 輸入錯誤: %u (%s)\n", reset_res, err_text);
        }
    } else {
        isActuallyRecording = false; // Ensure it's false
    }

    // 2. Unprepare all headers and close device
    if (hWaveIn != NULL) {
        printf("DEBUG: cleanup_audio_recording - Unpreparing headers and closing device.\n");
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
            if (waveHdrs[i].dwFlags & WHDR_PREPARED) { 
                MMRESULT res_unprep = waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
                if (res_unprep != MMSYSERR_NOERROR && res_unprep != WAVERR_UNPREPARED) {
                    char err_text[256]; waveInGetErrorTextA(res_unprep, err_text, 256);
                    fprintf(stderr, "清理期間取消準備 wave 標頭 %d (%p) 錯誤: %u (%s)\n", i, &waveHdrs[i], res_unprep, err_text);
                }
            }
            ZeroMemory(&waveHdrs[i], sizeof(WAVEHDR)); // Also zero out the header structure itself
        }
        
        MMRESULT close_res = waveInClose(hWaveIn); 
        if (close_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(close_res, err_text, 256);
            fprintf(stderr, "關閉 wave 輸入裝置錯誤: %u (%s)\n", close_res, err_text);
        }
        hWaveIn = NULL; // Mark device as closed
    }

    // 3. Free all allocated buffers
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        if (pAudioBuffers[i] != NULL) {
            free(pAudioBuffers[i]);
            pAudioBuffers[i] = NULL;
            // printf("DEBUG: Freed pAudioBuffers[%d]\n", i);
        }
    }
    
    if (waveform_buffer != NULL) {
        free(waveform_buffer);
        waveform_buffer = NULL;
        waveform_buffer_size_samples = 0;
        waveform_buffer_write_pos = 0;
        // printf("DEBUG: Freed waveform_buffer\n");
    }

    // pWaveBuffer and waveHdr (single old ones) are removed, so no cleanup for them.

    // 清理 large_font (如果已載入)
    if (large_font) {
        printf("DEBUG: 正在銷毀 large_font。\n");
        al_destroy_font(large_font);
        large_font = NULL;
    }
}