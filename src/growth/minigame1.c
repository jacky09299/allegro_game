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
#include "backpack.h" // 假設 add_item_to_backpack 函數在此定義

// 音訊錄製常數
#define AUDIO_SAMPLE_RATE 44100            // 音訊取樣率
#define AUDIO_CHANNELS 1                   // 音訊聲道數
#define AUDIO_BITS_PER_SAMPLE 16           // 每樣本位元數

const double MIN_TOTAL_SESSION_DURATION_FOR_GROWTH = 30.0;

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
#define WAVEFORM_DISPLAY_DURATION_SEC 0.5f                  // 波形顯示持續時間 (秒)

// 歌唱驗證常數
#define REQUIRED_SINGING_PROPORTION (2.0f / 3.0f)           // 歌唱音量達標時間佔按鈕間時間的比例
const double SINGING_TOTAL_TIME_THRESHOLD = 20.0;           // 歌唱總時間 (t_singing_loud_duration) 需大於20秒 for singing_is_successful_realtime
const double SINGING_DETECTION_MIN_STREAK_DURATION = 0.2;   // 持續0.2秒音量達閾值才算有效歌唱片段

// 相對音量常數
#define SINGING_THRESHOLD_RELATIVE_MULTIPLIER 0.0f // 要求 RMS 是背景的 X 倍
#define SINGING_THRESHOLD_ABSOLUTE_ADDITION 20.0f  // 要求 RMS 比背景高 X
#define MIN_ABSOLUTE_SINGING_RMS_FLOOR 100.0f     // 最低歌唱 RMS 不得低於此值
#define INITIAL_BACKGROUND_RMS_GUESS 20.0f       // 背景 RMS 的初始猜測值 (若倒數時未測到或過低則使用)

// 音訊錄製相關的靜態全域變數
static HWAVEIN hWaveIn = NULL;
static WAVEHDR waveHdrs[NUM_AUDIO_BUFFERS];
static char* pAudioBuffers[NUM_AUDIO_BUFFERS];
static int current_buffer_idx = 0;
static volatile int processed_buffer_idx = -1;
static short* waveform_buffer = NULL;
static int waveform_buffer_size_samples;
static int waveform_buffer_write_pos = 0;

// ---- NEW ----
// 用於從 waveInProc 傳遞 dwBytesRecorded 到主線程
static DWORD g_last_recorded_bytes[NUM_AUDIO_BUFFERS];
// ---- END NEW ----

static double allegro_recording_start_time = 0.0;

static bool isActuallyRecording = false;
static bool displayPleaseSingMessage = false;

// 即時回饋與相對音量相關的靜態全域變數
static float current_recording_elapsed_time_sec = 0.0f;
static bool current_volume_is_loud_enough = false;
static float current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
static float g_last_calculated_singing_threshold = MIN_ABSOLUTE_SINGING_RMS_FLOOR; // 用於顯示計算出的閾值

static int local_last_processed_idx = -1;

static MinigameFlowerPlant flower_plant;
static Button minigame_buttons[NUM_MINIGAME1_BUTTONS];
static bool seed_planted = false;
static bool is_singing = false;
static const int songs_to_flower = 8;
static bool minigame_srand_called = false;

static LotteryItemDefinition flower[2];

// 新增的詳細歌唱計時靜態全域變數
static double time_start_button_press = 0.0;
static double t1_countdown_finish_time = 0.0;
static double t2_singing_start_time = -1.0;
static double t_singing_loud_duration = 0.0;

// New streak detection variables
static double current_loud_streak_start_time = -1.0;
static bool is_currently_loud_for_streak = false;
static bool singing_is_successful_realtime = false;
static double t4_complete_button_press_time = 0.0;

// 321 倒數動畫相關變數
static bool is_in_countdown_animation = false;
static double countdown_start_time = 0.0;
static int countdown_value = 3;
static bool background_rms_calculation_locked = false;
static float accumulated_rms_sum = 0.0f;
static int rms_samples_count = 0;

static ALLEGRO_FONT *large_font = NULL;


static void prepare_audio_recording(void);
static void start_actual_audio_recording(void);
static bool stop_actual_audio_recording(void);
static void cleanup_audio_recording(void);
static float calculate_rms(const short* samples, int count);
static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);


void init_minigame1(void) {
    if (!minigame_srand_called) {
        srand(time(NULL));
        minigame_srand_called = true;
    }

    cleanup_audio_recording();

    is_singing = false;
    is_in_countdown_animation = false;
    background_rms_calculation_locked = true;

    displayPleaseSingMessage = false;
    current_recording_elapsed_time_sec = 0.0f;
    current_volume_is_loud_enough = false;
    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
    g_last_calculated_singing_threshold = MIN_ABSOLUTE_SINGING_RMS_FLOOR; // Reset threshold display

    time_start_button_press = 0.0;
    t1_countdown_finish_time = 0.0;
    t2_singing_start_time = -1.0;
    t_singing_loud_duration = 0.0;
    current_loud_streak_start_time = -1.0;
    is_currently_loud_for_streak = false;
    allegro_recording_start_time = 0.0;

    large_font = al_load_ttf_font("assets/font/JasonHandwriting3.ttf", 72, 0);

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
    flower[0].id = 1006; // 自定義ID
    //lottery.sprite = load_bitmap_once();
    strcpy(flower[0].name, "花");
    flower[0].image_path = "assets/image/flower.png"; // 使用您提供的圖片名稱
    flower[0].image = al_load_bitmap(flower[0].image_path);
    if (!flower[0].image) {
        fprintf(stderr, "Failed to load lottery item image: %s\n", flower[0].image_path);
        // 可以載入一個預設的"圖片遺失"圖片
    }

    flower[1].id = 1007; // 自定義ID
    //lottery.sprite = load_bitmap_once();
    strcpy(flower[1].name, "惡魔花");
    flower[1].image_path = "assets/image/devil_flower.png"; // 使用您提供的圖片名稱
    flower[1].image = al_load_bitmap(flower[1].image_path);
    if (!flower[1].image) {
        fprintf(stderr, "Failed to load lottery item image: %s\n", flower[1].image_path);
        // 可以載入一個預設的"圖片遺失"圖片
    }
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
    al_clear_to_color(al_map_rgb(50, 50, 70));
    char display_text_buffer[256];
    float y_offset = 60;
    float base_text_y = WAVEFORM_AREA_Y - 70 + y_offset;

    if (is_singing && !is_in_countdown_animation) {
        char t1_str[20], t2_str[20];
        if (t1_countdown_finish_time <= 0.0) strcpy(t1_str, "N/A"); else sprintf(t1_str, "%.2f", t1_countdown_finish_time);
        if (t2_singing_start_time < 0.0) strcpy(t2_str, "N/A"); else sprintf(t2_str, "%.2f", t2_singing_start_time);
        
        sprintf(display_text_buffer, "已唱了:%.1fs",
                current_recording_elapsed_time_sec);
        al_draw_text(font, al_map_rgb(220, 220, 180), WAVEFORM_AREA_X, base_text_y, 0, display_text_buffer);


        //ALLEGRO_COLOR vol_indicator_box_color = current_volume_is_loud_enough ? al_map_rgb(0, 200, 0) : al_map_rgb(200, 0, 0);
        //const char* vol_text = current_volume_is_loud_enough ? "音量達標!" : "音量不足";
        //float vol_text_width_val = al_get_text_width(font, vol_text);
        //float vol_indicator_x_start = WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH - vol_text_width_val - 20;
        //al_draw_filled_rectangle(vol_indicator_x_start, base_text_y + 20,
        //                         WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH, base_text_y + 20 + 20, vol_indicator_box_color);
        //al_draw_text(font, al_map_rgb(0,0,0), vol_indicator_x_start + 5, base_text_y + 20 + (20-al_get_font_line_height(font))/2.0f, 0, vol_text);

    } else if (t4_complete_button_press_time > 0.0 && t1_countdown_finish_time > 0.0 && !is_in_countdown_animation) {
    }

    if (is_in_countdown_animation) {
        sprintf(display_text_buffer, "%d", countdown_value);
        ALLEGRO_FONT *font_for_countdown = large_font ? large_font : font;
        float countdown_text_height = al_get_font_line_height(font_for_countdown);

        al_draw_text(font_for_countdown, al_map_rgb(255, 255, 0),
                     SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - countdown_text_height / 2.0f + 200,
                     ALLEGRO_ALIGN_CENTER, display_text_buffer);
    }
    else if (is_singing && !is_in_countdown_animation && isActuallyRecording && hWaveIn != NULL && waveform_buffer != NULL && waveform_buffer_size_samples > 0) {
        int num_samples_to_display = waveform_buffer_size_samples;

        if (num_samples_to_display > 1 && WAVEFORM_AREA_WIDTH > 1) {
            short* display_samples = waveform_buffer;
            float prev_x_pos = WAVEFORM_AREA_X;

            short temp_display_copy[WAVEFORM_DISPLAY_SAMPLES]; // Use defined size
            int current_read_pos = waveform_buffer_write_pos;
            for(int i=0; i<num_samples_to_display; ++i) {
                int read_idx = (waveform_buffer_write_pos - num_samples_to_display + i + waveform_buffer_size_samples) % waveform_buffer_size_samples;
                temp_display_copy[i] = display_samples[read_idx];
            }
            // Re-implementing your original display copy logic for clarity:
            current_read_pos = waveform_buffer_write_pos; // Start reading from where the next data would be written (oldest data in buffer if full)
            for(int i = 0; i < num_samples_to_display; ++i) {
                int read_idx = (current_read_pos + i) % waveform_buffer_size_samples;
                 if (i < WAVEFORM_DISPLAY_SAMPLES) { // Boundary check for temp_display_copy
                    temp_display_copy[i] = display_samples[read_idx];
                 }
            }


            if (num_samples_to_display > 1) {
                short first_sample_val = temp_display_copy[0];
                float prev_y_pos = WAVEFORM_AREA_Y + (WAVEFORM_AREA_HEIGHT / 2.0f) -
                                   (first_sample_val / 32767.0f) * (WAVEFORM_AREA_HEIGHT / 2.0f);
                prev_y_pos = fmaxf(WAVEFORM_AREA_Y + 1.0f, fminf(WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT - 1.0f, prev_y_pos));

                for (int screen_pixel_x = 1; screen_pixel_x < WAVEFORM_AREA_WIDTH; ++screen_pixel_x) {
                    float proportion = (WAVEFORM_AREA_WIDTH > 1) ? ((float)screen_pixel_x / (WAVEFORM_AREA_WIDTH - 1)) : 0.0f;
                    int sample_idx_in_display_copy = (int)(proportion * (num_samples_to_display - 1));
                     if(sample_idx_in_display_copy >= WAVEFORM_DISPLAY_SAMPLES) sample_idx_in_display_copy = WAVEFORM_DISPLAY_SAMPLES -1; // Boundary check

                    short sample_value = temp_display_copy[sample_idx_in_display_copy];
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
    } else if (is_singing && !is_in_countdown_animation) {
        al_draw_text(font, al_map_rgb(200,200,200), WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH/2.0f, WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT/2.0f - al_get_font_line_height(font)/2.0f, ALLEGRO_ALIGN_CENTER, "等待麥克風...");
    }

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

    if (displayPleaseSingMessage && !is_in_countdown_animation && t4_complete_button_press_time > 0.0) {
        char failure_reason[128] = "Plant growth conditions not met.";
        bool conditionA = (t4_complete_button_press_time - t1_countdown_finish_time) > MIN_TOTAL_SESSION_DURATION_FOR_GROWTH;

        if (!conditionA) {
            sprintf(failure_reason, "唱歌時長要大於%.0f秒 (你唱了 %.1f秒)",
                    MIN_TOTAL_SESSION_DURATION_FOR_GROWTH,
                    (t4_complete_button_press_time - t1_countdown_finish_time));
        } else if (!singing_is_successful_realtime) {
            sprintf(failure_reason, "小花感受不到你唱歌的熱情，請再試一次。");
        }

        float msg_y_pos = (SCREEN_HEIGHT / 2.0f + minigame_buttons[0].y) / 2.0f;
        al_draw_text(font, al_map_rgb(255, 100, 100), SCREEN_WIDTH / 2.0f, msg_y_pos, ALLEGRO_ALIGN_CENTER, failure_reason);
    }

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
            Button* b = &minigame_buttons[5];
            al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color);
            al_draw_text(font, b->text_color, b->x + b->width / 2.0f, b->y + (b->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b->text);
        }
        if (is_singing) {
            Button* b_restart = &minigame_buttons[2];
            al_draw_filled_rectangle(b_restart->x, b_restart->y, b_restart->x + b_restart->width, b_restart->y + b_restart->height, b_restart->is_hovered ? b_restart->hover_color : b_restart->color);
            al_draw_text(font, b_restart->text_color, b_restart->x + b_restart->width / 2.0f, b_restart->y + (b_restart->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_restart->text);
            Button* b_finish = &minigame_buttons[3];
            al_draw_filled_rectangle(b_finish->x, b_finish->y, b_finish->x + b_finish->width, b_finish->y + b_finish->height, b_finish->is_hovered ? b_finish->hover_color : b_finish->color);
            al_draw_text(font, b_finish->text_color, b_finish->x + b_finish->width / 2.0f, b_finish->y + (b_finish->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_finish->text);
        }
    }
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
                    time_start_button_press = 0.0;
                }
                else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower && minigame_buttons[1].is_hovered) {
                    is_in_countdown_animation = true;
                    countdown_start_time = al_get_time();
                    countdown_value = 3;
                    background_rms_calculation_locked = false;
                    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
                    g_last_calculated_singing_threshold = MIN_ABSOLUTE_SINGING_RMS_FLOOR; // Reset display
                    accumulated_rms_sum = 0.0f;
                    rms_samples_count = 0;

                    t1_countdown_finish_time = 0.0;
                    t2_singing_start_time = -1.0;
                    t_singing_loud_duration = 0.0;
                    current_loud_streak_start_time = -1.0;
                    is_currently_loud_for_streak = false;
                    displayPleaseSingMessage = false;
                    current_recording_elapsed_time_sec = 0.0f;

                    start_actual_audio_recording();
                    printf("小遊戲: 開始唱歌按鈕點擊，進入倒數計時。\n");
                    button_clicked_this_event = true;
                }
                else if (is_singing) {
                    if (minigame_buttons[2].is_hovered) {
                        if(isActuallyRecording && hWaveIn) {
                            stop_actual_audio_recording(); // Stop previous before restarting
                        }
                        is_singing = false; // This will be set to true after countdown

                        is_in_countdown_animation = true;
                        countdown_start_time = al_get_time();
                        countdown_value = 3;
                        background_rms_calculation_locked = false;
                        current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
                        g_last_calculated_singing_threshold = MIN_ABSOLUTE_SINGING_RMS_FLOOR; // Reset display
                        accumulated_rms_sum = 0.0f;
                        rms_samples_count = 0;

                        t1_countdown_finish_time = 0.0;
                        t2_singing_start_time = -1.0;
                        t_singing_loud_duration = 0.0;
                        current_loud_streak_start_time = -1.0;
                        is_currently_loud_for_streak = false;
                        current_recording_elapsed_time_sec = 0.0f;

                        start_actual_audio_recording(); // Start new recording for countdown
                        printf("小遊戲: 重新開始唱歌按鈕點擊，進入倒數計時。\n");
                        button_clicked_this_event = true;
                    }
                    else if (minigame_buttons[3].is_hovered) {
                        t4_complete_button_press_time = al_get_time();
                        bool plant_should_grow = stop_actual_audio_recording();
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
                            displayPleaseSingMessage = true;
                        }
                        current_recording_elapsed_time_sec = 0.0f;
                        current_volume_is_loud_enough = false;
                        button_clicked_this_event = true;
                    }
                }
                else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower && minigame_buttons[5].is_hovered) {
                    if (flower_plant.which_flower == 0) { 
                         add_item_to_backpack(flower[0]);
                        printf("DEBUG: 採收了一朵普通花! 總數: %d\n", player.item_quantities[0]); }
                    else { 
                         add_item_to_backpack(flower[1]);
                        printf("DEBUG: 採收了一朵惡魔花. 總數: %d\n", player.item_quantities[1]); 
                    }
                    flower_plant.songs_sung = 0; flower_plant.growth_stage = 0;
                    seed_planted = false; displayPleaseSingMessage = false; button_clicked_this_event = true;
                    time_start_button_press = 0.0;
                    t1_countdown_finish_time = 0.0;
                    t2_singing_start_time = -1.0;
                    t_singing_loud_duration = 0.0;
                    t4_complete_button_press_time = 0.0;
                    singing_is_successful_realtime = false;
                }
            }
            if (minigame_buttons[4].is_hovered) {
                if((is_singing || is_in_countdown_animation) && isActuallyRecording && hWaveIn) {
                    stop_actual_audio_recording();
                }
                is_singing = false;
                is_in_countdown_animation = false;
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
                stop_actual_audio_recording();
            }
            is_singing = false;
            is_in_countdown_animation = false;
            cleanup_audio_recording();
            game_phase = GROWTH;
        }
        // 金手指: 按G鍵直接讓花長大
        else if (ev.keyboard.keycode == ALLEGRO_KEY_G && seed_planted) {
            if (flower_plant.growth_stage < songs_to_flower) {
                flower_plant.songs_sung++;
                flower_plant.growth_stage = flower_plant.songs_sung;
                if (flower_plant.growth_stage >= songs_to_flower) {
                    flower_plant.which_flower = (rand() % 2);
                    printf("DEBUG: Cheat activated! Flower bloomed!\n");
                }
            }
        }
    }
}

void update_minigame1(void) {
    double current_time = al_get_time();

    if (is_in_countdown_animation) {
        double elapsed_countdown_time = current_time - countdown_start_time;
        if (elapsed_countdown_time < 1.0) countdown_value = 3;
        else if (elapsed_countdown_time < 2.0) countdown_value = 2;
        else if (elapsed_countdown_time < 3.0) countdown_value = 1;
        else { // Countdown finished
            is_in_countdown_animation = false;
            is_singing = true; // Now officially singing

            if (rms_samples_count > 0) {
                current_background_rms = accumulated_rms_sum / rms_samples_count;
                if (current_background_rms < 5.0f) {
                    printf("警告: 計算出的背景 RMS (%.1f) 過低，使用預設值 %.1f 代替。\n", current_background_rms, INITIAL_BACKGROUND_RMS_GUESS);
                    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
                }
            } else {
                current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
                printf("警告: 倒數期間未收集到 RMS 樣本，使用預設背景 RMS (%.1f)。\n", INITIAL_BACKGROUND_RMS_GUESS);
            }
            background_rms_calculation_locked = true; // Lock background RMS calculation
            accumulated_rms_sum = 0.0f; // Reset for any future use (though not used after this)
            rms_samples_count = 0;      // Reset

            t1_countdown_finish_time = current_time;
            allegro_recording_start_time = current_time; // Might be redundant if t1 is used
            time_start_button_press = current_time; // Might be redundant

            t2_singing_start_time = -1.0;
            t_singing_loud_duration = 0.0;
            current_loud_streak_start_time = -1.0;
            is_currently_loud_for_streak = false;
            current_volume_is_loud_enough = false;
            singing_is_successful_realtime = false;

            current_recording_elapsed_time_sec = 0.0f;
            displayPleaseSingMessage = false;
            // local_last_processed_idx = -1; // Already reset when starting recording

            float threshold1_relative_init = current_background_rms * SINGING_THRESHOLD_RELATIVE_MULTIPLIER;
            float threshold2_absolute_add_init = current_background_rms * 0.0f + SINGING_THRESHOLD_ABSOLUTE_ADDITION;
            g_last_calculated_singing_threshold = fmaxf(MIN_ABSOLUTE_SINGING_RMS_FLOOR, fmaxf(threshold1_relative_init, threshold2_absolute_add_init));

            printf("DEBUG: Countdown finished. t1=%.2f. Background RMS locked: %.0f (Initial Est. Threshold: %.0f). Singing started.\n",
                   t1_countdown_finish_time, current_background_rms, g_last_calculated_singing_threshold);
        }

        // Process audio for background RMS calculation during countdown
        if (isActuallyRecording && hWaveIn != NULL && !background_rms_calculation_locked) {
            if (processed_buffer_idx != local_last_processed_idx && processed_buffer_idx != -1) {
                int buffer_to_analyze = processed_buffer_idx;

                short* samples_from_chunk = (short*)waveHdrs[buffer_to_analyze].lpData;
                DWORD recorded_bytes = g_last_recorded_bytes[buffer_to_analyze]; // Use saved byte count
                int samples_in_chunk = recorded_bytes / (AUDIO_BITS_PER_SAMPLE / 8);

                // Your existing printf for countdown debugging:
                // printf("DEBUG: Processed buffer %d, latest_rms: %.2f\n", buffer_to_analyze, latest_rms);
                // We can add one for sample count if needed:
                // printf("DEBUG Countdown: Buf %d, Bytes %lu, Samples %d\n", buffer_to_analyze, (unsigned long)recorded_bytes, samples_in_chunk);


                if (samples_in_chunk > AUDIO_CHUNK_SAMPLES / 2) { // Check if enough samples
                    float latest_rms = calculate_rms(samples_from_chunk, samples_in_chunk);
                    printf("DEBUG Countdown RMS: Buf %d, Samples %d, RMS %.2f\n", buffer_to_analyze, samples_in_chunk, latest_rms);
                    accumulated_rms_sum += latest_rms;
                    rms_samples_count++;
                }
                local_last_processed_idx = buffer_to_analyze; // Mark as processed by main thread
            }
        }
    }
    else if (is_singing && isActuallyRecording && hWaveIn != NULL) { // Singing phase
        // printf("DEBUG: Processing audio buffers during singing...\n"); // Your existing debug
        current_recording_elapsed_time_sec = (float)(current_time - t1_countdown_finish_time);

        if (processed_buffer_idx != local_last_processed_idx && processed_buffer_idx != -1) {
            // printf("DEBUG: Processing buffer %d (last processed: %d)\n", processed_buffer_idx, local_last_processed_idx); // Your existing debug
            int current_chunk_idx = processed_buffer_idx;

            short* audio_samples_in_chunk = (short*)waveHdrs[current_chunk_idx].lpData;
            DWORD recorded_bytes_for_singing = g_last_recorded_bytes[current_chunk_idx]; // Use saved byte count
            int num_actual_samples_in_chunk = recorded_bytes_for_singing / (AUDIO_BITS_PER_SAMPLE / 8);
            bool chunk_is_loud = false;

            // Your existing debug printf for sample count:
            printf("num_samples_in_chunk: %d (from %lu bytes), Expected AUDIO_CHUNK_SAMPLES: %d\n",
                   num_actual_samples_in_chunk, (unsigned long)recorded_bytes_for_singing, AUDIO_CHUNK_SAMPLES);

            if (num_actual_samples_in_chunk > 0) {
                float current_rms_val = calculate_rms(audio_samples_in_chunk, num_actual_samples_in_chunk);
                 // Your existing debug printf for RMS:
                printf("current_rms_val: %.2f, current_background_rms: %.2f, Threshold: %.2f\n",
                       current_rms_val, current_background_rms, g_last_calculated_singing_threshold);


                float threshold1_relative = current_background_rms * SINGING_THRESHOLD_RELATIVE_MULTIPLIER;
                float threshold2_absolute_add = current_background_rms * 0.0f + SINGING_THRESHOLD_ABSOLUTE_ADDITION;
                float singing_threshold = fmaxf(MIN_ABSOLUTE_SINGING_RMS_FLOOR,
                                                fmaxf(threshold1_relative, threshold2_absolute_add));
                g_last_calculated_singing_threshold = singing_threshold;

                chunk_is_loud = (current_rms_val > singing_threshold);
                current_volume_is_loud_enough = chunk_is_loud;
            } else {
                current_volume_is_loud_enough = false;
                 // printf("DEBUG Singing: Buffer %d had 0 samples (from g_last_recorded_bytes = %lu)\n", current_chunk_idx, (unsigned long)recorded_bytes_for_singing);
            }

            // t2 (singing start time) detection logic
            if (t2_singing_start_time < 0) { // If t2 has not been set yet
                if (chunk_is_loud) {
                    if (current_loud_streak_start_time < 0) { // Start of a new potential streak
                        current_loud_streak_start_time = current_time - (AUDIO_CHUNK_DURATION_MS / 1000.0); // Estimate start time of this chunk
                    }
                    if ((current_time - current_loud_streak_start_time) >= SINGING_DETECTION_MIN_STREAK_DURATION) {
                        t2_singing_start_time = current_loud_streak_start_time; // Valid streak, set t2
                        t_singing_loud_duration = (current_time - t2_singing_start_time); // Accumulate duration for this streak
                        printf("DEBUG: t2 detected at %.2fs. Initial t_singing_loud_duration: %.2fs\n", t2_singing_start_time, t_singing_loud_duration);
                    }
                } else { // Chunk is not loud, reset streak
                    current_loud_streak_start_time = -1.0;
                }
            } else { // t2 has been set, accumulate loud duration
                if (chunk_is_loud) {
                     t_singing_loud_duration += (AUDIO_CHUNK_DURATION_MS / 1000.0); // Add duration of this chunk
                }
            }
            is_currently_loud_for_streak = chunk_is_loud; // For other logic if needed

            // Real-time success check
            if (t2_singing_start_time >= 0 && !singing_is_successful_realtime) {
                double t3_current_time = current_time; // Or more accurately, the time this chunk represents
                double duration_since_t2 = t3_current_time - t2_singing_start_time;

                bool condition1_duration_met = (duration_since_t2 > SINGING_TOTAL_TIME_THRESHOLD);
                bool condition2_proportion_met = false;
                if (duration_since_t2 > 0.001) { // Avoid division by zero
                    condition2_proportion_met = (t_singing_loud_duration / duration_since_t2) > REQUIRED_SINGING_PROPORTION;
                }

                if (condition1_duration_met && condition2_proportion_met) {
                    singing_is_successful_realtime = true;
                    printf("DEBUG: Real-time singing success achieved! duration_since_t2: %.2f, t_singing: %.2f, proportion: %.2f\n",
                           duration_since_t2, t_singing_loud_duration, (duration_since_t2 > 0 ? t_singing_loud_duration / duration_since_t2 : 0));
                }
            }
            local_last_processed_idx = current_chunk_idx; // Mark as processed by main thread
        }
    } else if (!is_in_countdown_animation) { // Not singing, not in countdown (e.g., after stopping)
        current_volume_is_loud_enough = false;
        current_loud_streak_start_time = -1.0;
        is_currently_loud_for_streak = false;
    }
}

static void prepare_audio_recording(void) {
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        pAudioBuffers[i] = NULL;
    }
    waveform_buffer = NULL;

    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        pAudioBuffers[i] = (char*)malloc(SINGLE_BUFFER_SIZE);
        if (pAudioBuffers[i] == NULL) {
            fprintf(stderr, "嚴重錯誤：無法分配音訊緩衝區 %d。\n", i);
            for (int j = 0; j < i; ++j) {
                if(pAudioBuffers[j]) free(pAudioBuffers[j]);
                pAudioBuffers[j] = NULL;
            }
            return;
        }
        ZeroMemory(pAudioBuffers[i], SINGLE_BUFFER_SIZE);
        printf("DEBUG: Audio buffer %d allocated (%lu bytes).\n", i, (unsigned long)SINGLE_BUFFER_SIZE);
    }

    waveform_buffer_size_samples = WAVEFORM_DISPLAY_SAMPLES;
    waveform_buffer = (short*)malloc(waveform_buffer_size_samples * sizeof(short));
    if (waveform_buffer == NULL) {
        fprintf(stderr, "嚴重錯誤：無法分配波形顯示緩衝區。\n");
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
            if (pAudioBuffers[i]) {
                free(pAudioBuffers[i]);
                pAudioBuffers[i] = NULL;
            }
        }
        return;
    }
    ZeroMemory(waveform_buffer, waveform_buffer_size_samples * sizeof(short));
    waveform_buffer_write_pos = 0;
    printf("DEBUG: Waveform buffer allocated (%d samples, %lu bytes).\n", waveform_buffer_size_samples, (unsigned long)((size_t)waveform_buffer_size_samples * sizeof(short)));

    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = AUDIO_CHANNELS;
    wfx.nSamplesPerSec = AUDIO_SAMPLE_RATE;
    wfx.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8);
    wfx.nBlockAlign = AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8);
    wfx.wBitsPerSample = AUDIO_BITS_PER_SAMPLE;
    wfx.cbSize = 0;

    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)waveInProc, (DWORD_PTR)NULL, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        char error_text[256];
        waveInGetErrorTextA(result, error_text, sizeof(error_text));
        fprintf(stderr, "錯誤：無法開啟音訊錄製裝置 (錯誤 %u: %s)。\n請確認麥克風已連接並啟用。\n", result, error_text);
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
            if (pAudioBuffers[i]) { free(pAudioBuffers[i]); pAudioBuffers[i] = NULL; }
        }
        if (waveform_buffer) { free(waveform_buffer); waveform_buffer = NULL; }
        hWaveIn = NULL;
        return;
    }
    printf("DEBUG: Windows 音訊錄製已準備。裝置已開啟 (使用回呼)。hWaveIn = %p\n", hWaveIn);
}

static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    switch (uMsg) {
        case WIM_OPEN:
            // printf("DEBUG: waveInProc - WIM_OPEN received. hWaveIn = %p\n", hwi);
            break;
        case WIM_DATA:
        {
            WAVEHDR* pDoneHeader = (WAVEHDR*)dwParam1;
            int filled_buffer_idx = -1;

            // Find which buffer is done
            for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
                if (&waveHdrs[i] == pDoneHeader) {
                    filled_buffer_idx = i;
                    break;
                }
            }

            if (filled_buffer_idx != -1) {
                // Store the actual recorded bytes for the main thread BEFORE signaling
                g_last_recorded_bytes[filled_buffer_idx] = pDoneHeader->dwBytesRecorded;
                // printf("waveInProc: Buffer %d, dwBytesRecorded = %lu, saved.\n", filled_buffer_idx, (unsigned long)pDoneHeader->dwBytesRecorded);


                if (pDoneHeader->dwBytesRecorded > 0) {
                    // Copy to waveform display buffer
                    if (waveform_buffer != NULL && waveform_buffer_size_samples > 0) {
                        int samples_in_chunk_for_waveform = pDoneHeader->dwBytesRecorded / (AUDIO_BITS_PER_SAMPLE / 8);
                        short* chunk_samples = (short*)pDoneHeader->lpData;
                        for (int i = 0; i < samples_in_chunk_for_waveform; ++i) {
                            waveform_buffer[waveform_buffer_write_pos] = chunk_samples[i];
                            waveform_buffer_write_pos = (waveform_buffer_write_pos + 1) % waveform_buffer_size_samples;
                        }
                    }
                }
                // Now signal the main thread that this buffer is ready for processing
                processed_buffer_idx = filled_buffer_idx;

            } else {
                fprintf(stderr, "waveInProc Error: WIM_DATA received for an unknown WAVEHDR! Addr: %p\n", pDoneHeader);
            }

            // Re-add the buffer to the input queue if still recording
            if (isActuallyRecording && hWaveIn != NULL) {
                // It's important that pDoneHeader is one of our waveHdrs.
                // If filled_buffer_idx was -1, re-adding pDoneHeader might be problematic,
                // but waveInAddBuffer expects the same header that was returned.
                MMRESULT add_res = waveInAddBuffer(hwi, pDoneHeader, sizeof(WAVEHDR));
                if (add_res != MMSYSERR_NOERROR) {
                    char error_text[256];
                    waveInGetErrorTextA(add_res, error_text, sizeof(error_text));
                    fprintf(stderr, "waveInProc: waveInAddBuffer error %u: %s. Header Addr: %p\n", add_res, error_text, pDoneHeader);
                    isActuallyRecording = false; // Critical error, stop recording
                }
            }
            break;
        }
        case WIM_CLOSE:
            // printf("DEBUG: waveInProc - WIM_CLOSE received. hWaveIn = %p\n", hwi);
            break;
        default:
            break;
    }
}


static void start_actual_audio_recording(void) {
    if (hWaveIn == NULL) {
        printf("DEBUG: start_actual_audio_recording - hWaveIn is NULL, calling prepare_audio_recording.\n");
        prepare_audio_recording();
        if (hWaveIn == NULL) {
            fprintf(stderr, "音訊系統未就緒。無法開始錄製。\n");
            is_singing = false;
            is_in_countdown_animation = false;
            isActuallyRecording = false;
            return;
        }
    }

    // ---- NEW ----
    // Initialize the array for storing recorded bytes for each buffer
    memset(g_last_recorded_bytes, 0, sizeof(g_last_recorded_bytes));
    // ---- END NEW ----

    isActuallyRecording = true;
    current_buffer_idx = 0;
    processed_buffer_idx = -1; // No buffer processed yet
    local_last_processed_idx = -1; // Main thread hasn't processed anything from this session yet
    waveform_buffer_write_pos = 0;
    if(waveform_buffer && waveform_buffer_size_samples > 0) {
      ZeroMemory(waveform_buffer, waveform_buffer_size_samples * sizeof(short));
    }


    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        if (waveHdrs[i].dwFlags & WHDR_PREPARED) {
            MMRESULT unprep_res = waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
            if (unprep_res != MMSYSERR_NOERROR && unprep_res != WAVERR_UNPREPARED) { // WAVERR_UNPREPARED is not an error here
                 char err_text[256]; waveInGetErrorTextA(unprep_res, err_text, 256);
                fprintf(stderr, "start_actual_audio_recording: waveInUnprepareHeader error for buffer %d: %u (%s)\n", i, unprep_res, err_text);
            }
        }

        ZeroMemory(&waveHdrs[i], sizeof(WAVEHDR));
        waveHdrs[i].lpData = pAudioBuffers[i];
        waveHdrs[i].dwBufferLength = SINGLE_BUFFER_SIZE;
        waveHdrs[i].dwFlags = 0; // Clear flags

        MMRESULT prep_res = waveInPrepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        if (prep_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(prep_res, err_text, 256);
            fprintf(stderr, "準備 wave 標頭 %d 錯誤: %u (%s)\n", i, prep_res, err_text);
            isActuallyRecording = false;
            for (int j = 0; j < i; ++j) { // Unprepare those already prepared
                if (waveHdrs[j].dwFlags & WHDR_PREPARED) waveInUnprepareHeader(hWaveIn, &waveHdrs[j], sizeof(WAVEHDR));
            }
            return;
        }

        MMRESULT add_res = waveInAddBuffer(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        if (add_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(add_res, err_text, 256);
            fprintf(stderr, "新增 wave 緩衝區 %d 錯誤: %u (%s)\n", i, add_res, err_text);
            isActuallyRecording = false;
            for (int j = 0; j <= i; ++j) { // Unprepare all including current one if prepared
                 if (waveHdrs[j].dwFlags & WHDR_PREPARED) waveInUnprepareHeader(hWaveIn, &waveHdrs[j], sizeof(WAVEHDR));
            }
            return;
        }
    }

    MMRESULT start_res = waveInStart(hWaveIn);
    if (start_res != MMSYSERR_NOERROR) {
        char err_text[256]; waveInGetErrorTextA(start_res, err_text, 256);
        fprintf(stderr, "開始 wave 輸入錯誤: %u (%s)\n", start_res, err_text);
        isActuallyRecording = false;
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) { // Unprepare all buffers
            if (waveHdrs[i].dwFlags & WHDR_PREPARED) waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        }
        return;
    }

    printf("[DEBUG][Audio] start_actual_audio_recording() SUCCEEDED. hWaveIn=%p\n", hWaveIn);
}

static bool stop_actual_audio_recording(void) {
    if (!isActuallyRecording || hWaveIn == NULL) {
        return false;
    }

    isActuallyRecording = false; // Signal waveInProc to stop adding buffers

    MMRESULT reset_res = waveInReset(hWaveIn); // Stop recording and return all buffers
    if (reset_res != MMSYSERR_NOERROR) {
        char err_text[256]; waveInGetErrorTextA(reset_res, err_text, 256);
        fprintf(stderr, "重置 wave 輸入錯誤: %u (%s)\n", reset_res, err_text);
    }
    // After waveInReset, any pending buffers are returned to the application via WIM_DATA
    // with the WHDR_DONE flag set and dwBytesRecorded updated.
    // Our waveInProc will handle them and set g_last_recorded_bytes.
    // The main loop in update_minigame1 should continue to process these final buffers.

    // The cleanup of waveHdrs (unprepare) should ideally happen after all buffers are confirmed done.
    // However, waveInClose will implicitly unprepare them if still prepared.
    // For robustness, unprepare them here explicitly. This should be safe after waveInReset.
    // It might be better to do this in cleanup_audio_recording or ensure all WIM_DATA have been processed.
    // For now, let's keep unprepare here as it was.
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        if (waveHdrs[i].dwFlags & WHDR_PREPARED) { // Check if it's still prepared
            // It's possible that waveInReset already marked them as unprepared or returned them.
            // It's safer to check WHDR_DONE before unpreparing, but waveInUnprepareHeader should handle it.
            MMRESULT unprep_res = waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
            if (unprep_res != MMSYSERR_NOERROR && unprep_res != WAVERR_UNPREPARED) {
                 char err_text[256]; waveInGetErrorTextA(unprep_res, err_text, 256);
                 fprintf(stderr, "停止時取消準備 wave 標頭 %d (%p) 錯誤: %u (%s)\n", i, &waveHdrs[i], unprep_res, err_text);
            }
        }
    }
    // Note: Device (hWaveIn) is not closed here, it's closed in cleanup_audio_recording.

    printf("DEBUG: Windows 錄製停止於 t4=%.2f.\n", t4_complete_button_press_time);
    printf("DEBUG: t1: %.2f, t2: %.2f, t_singing_loud_duration: %.2f\n",
           t1_countdown_finish_time, t2_singing_start_time, t_singing_loud_duration);

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

    return plant_should_grow;
}

static void cleanup_audio_recording(void) {
    if (isActuallyRecording && hWaveIn != NULL) {
        isActuallyRecording = false; // Ensure waveInProc stops queueing
        MMRESULT reset_res = waveInReset(hWaveIn);
        if (reset_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(reset_res, err_text, 256);
            fprintf(stderr, "清理期間重置 wave 輸入錯誤: %u (%s)\n", reset_res, err_text);
        }
    } else {
        isActuallyRecording = false; // Defensive
    }

    if (hWaveIn != NULL) {
        // Unprepare all headers
        for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
            if (waveHdrs[i].dwFlags & WHDR_PREPARED) {
                MMRESULT res_unprep = waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
                // WAVERR_UNPREPARED is not an error if it was already unprepared or never prepared properly.
                if (res_unprep != MMSYSERR_NOERROR && res_unprep != WAVERR_UNPREPARED) {
                    char err_text[256]; waveInGetErrorTextA(res_unprep, err_text, 256);
                    fprintf(stderr, "清理期間取消準備 wave 標頭 %d (%p) 錯誤: %u (%s)\n", i, &waveHdrs[i], res_unprep, err_text);
                }
            }
            // ZeroMemory(&waveHdrs[i], sizeof(WAVEHDR)); // Not strictly necessary here as buffers are freed.
        }

        MMRESULT close_res = waveInClose(hWaveIn);
        if (close_res != MMSYSERR_NOERROR) {
            char err_text[256]; waveInGetErrorTextA(close_res, err_text, 256);
            fprintf(stderr, "關閉 wave 輸入裝置錯誤: %u (%s)\n", close_res, err_text);
        }
        hWaveIn = NULL;
    }

    // Free allocated audio buffers
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        if (pAudioBuffers[i] != NULL) {
            free(pAudioBuffers[i]);
            pAudioBuffers[i] = NULL;
        }
    }

    // Free waveform display buffer
    if (waveform_buffer != NULL) {
        free(waveform_buffer);
        waveform_buffer = NULL;
        waveform_buffer_size_samples = 0;
        waveform_buffer_write_pos = 0;
    }

    if (large_font) {
        al_destroy_font(large_font);
        large_font = NULL;
    }
    // Reset related static variables
    processed_buffer_idx = -1;
    local_last_processed_idx = -1;
    memset(g_last_recorded_bytes, 0, sizeof(g_last_recorded_bytes));

    printf("DEBUG: Audio recording resources cleaned up.\n");
}