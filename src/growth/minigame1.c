#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro.h> // For al_get_time, ALLEGRO_PI
#include <stdio.h>
#include <math.h>   // For fmaxf, fminf, cos, sin, fabs
#include <stdlib.h> // For rand, abs (for int)
#include <string.h> // For memset, memcpy (if needed)
#include <time.h>


#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include "globals.h"
#include "minigame1.h"
#include "types.h"

// Audio Recording Constants
#define AUDIO_BUFFER_SIZE (44100 * 2 * 35)
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 1
#define AUDIO_BITS_PER_SAMPLE 16

// Real-time Feedback Constants
#define WAVEFORM_AREA_X 50
#define WAVEFORM_AREA_Y 100 // Adjusted Y to make space for new text
#define WAVEFORM_AREA_WIDTH (SCREEN_WIDTH - 100)
#define WAVEFORM_AREA_HEIGHT 100
#define REALTIME_ANALYSIS_WINDOW_MS 50
#define WAVEFORM_DISPLAY_DURATION_SEC 0.5f

// Singing Validation Constants
#define MIN_RECORDING_DURATION_SECONDS 1.0f // Min 按鈕間時間
#define REQUIRED_SINGING_PROPORTION (1.0f / 3.0f) // 唱歌音量達標時間佔按鈕間時間的比例
const double SINGING_TOTAL_TIME_THRESHOLD = 20.0; // 唱歌總時間 (actual_end - actual_start) 需大於20秒
const double SINGING_DETECTION_MIN_STREAK_DURATION = 0.2; // 持續0.2秒達threshold才算有效歌唱片段

// Relative Volume Constants
#define NOISE_CALC_WINDOW_SAMPLES (AUDIO_SAMPLE_RATE / 5) // Not actively used in current logic but kept for structure
#define RMS_ANALYSIS_WINDOW_SAMPLES (AUDIO_SAMPLE_RATE / 20) // 50ms window for current volume and singing check
#define SINGING_RMS_THRESHOLD_MULTIPLIER 3.0f // Current RMS must be X times background RMS
#define MIN_ABSOLUTE_RMS_FOR_SINGING 150.0f    // Minimum RMS to be considered singing (prevents silence being loud vs silence)
#define INITIAL_BACKGROUND_RMS_GUESS 200.0f   // Initial guess for background RMS


// Static global variables for audio recording
static HWAVEIN hWaveIn = NULL;
static WAVEHDR waveHdr;
static char* pWaveBuffer = NULL;
// allegro_recording_start_time is essentially time_start_button_press for elapsed calculation
static double allegro_recording_start_time = 0.0;


static bool isActuallyRecording = false;
static bool displayPleaseSingMessage = false;
static float overall_button_to_button_duration = 0.0f; // Renamed from audioLengthSeconds

// Static global variables for real-time feedback & relative volume
static float current_recording_elapsed_time_sec = 0.0f;
static bool current_volume_is_loud_enough = false; // Based on relative volume now
static float current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;
// Removed unused noise_calc_buffer_idx and noise_calc_buffer_filled, recent_samples_for_noise_calc

static bool force_audio_too_short_for_test = false; // For debugging

static MinigameFlowerPlant flower_plant;
static Button minigame_buttons[NUM_MINIGAME1_BUTTONS];
static bool seed_planted = false;
static bool is_singing = false;
static const int songs_to_flower = 8;
static bool minigame_srand_called = false;

// New static global variables for detailed singing timing as per request
static double time_start_button_press = 0.0;    // User clicks "Start Singing"
static double time_end_button_press = 0.0;      // User clicks "Finish Singing"
static double actual_singing_start_time = -1.0;          // First time audio threshold met for 0.2s (start of that 0.2s)
static double current_actual_singing_end_time = -1.0;    // Continuously updated end of the latest 0.2s singing streak
static double locked_successful_singing_end_time = -1.0; // If core requirements met, this locks the successful singing end time
static bool has_achieved_lockable_success = false;       // Flag if core singing requirements (e.g. total singing > 20s) met during recording

// Internal state for detecting singing streaks
static bool internal_was_loud_last_frame = false;
static double internal_current_loud_streak_began_at_time = 0.0;


static void prepare_audio_recording(void);
static void start_actual_audio_recording(void);
static bool stop_actual_audio_recording(void);
static void cleanup_audio_recording(void);
static float calculate_rms(const short* samples, int count);


void init_minigame1(void) {
    if (!minigame_srand_called) {
        srand(time(NULL));
        minigame_srand_called = true;
    }
    if (!al_is_system_installed()) {
        al_init();
    }

    force_audio_too_short_for_test = false;

    cleanup_audio_recording();
    prepare_audio_recording();

    is_singing = false;

    displayPleaseSingMessage = false;
    overall_button_to_button_duration = 0.0f;

    current_recording_elapsed_time_sec = 0.0f;
    current_volume_is_loud_enough = false;
    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS;

    // Reset new timing variables
    time_start_button_press = 0.0;
    time_end_button_press = 0.0;
    actual_singing_start_time = -1.0;
    current_actual_singing_end_time = -1.0;
    locked_successful_singing_end_time = -1.0;
    has_achieved_lockable_success = false;

    internal_was_loud_last_frame = false;
    internal_current_loud_streak_began_at_time = 0.0;
    allegro_recording_start_time = 0.0;


    printf("DEBUG: Minigame Flower initialized/reset.\n");

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
    // Print RMS for very short windows (for debug, but avoid flooding)
    if (count == RMS_ANALYSIS_WINDOW_SAMPLES) {
        printf("[DEBUG][RMS] calc_rms: count=%d, rms=%.2f, first_sample=%d\n", count, rms, samples[0]);
    }
    return rms;
}


void render_minigame1(void) {
    al_clear_to_color(al_map_rgb(50, 50, 70));
    char display_text_buffer[256]; // General buffer for formatted text
    float base_text_y = WAVEFORM_AREA_Y - 70; // Start Y for timing info

    // Display main recording/singing times
    if (is_singing) {
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


        // Existing elapsed time and RMS display
        sprintf(display_text_buffer, "已錄製: %.1f s (背景RMS: %.0f)", current_recording_elapsed_time_sec, current_background_rms);
        al_draw_text(font, al_map_rgb(255, 255, 255), WAVEFORM_AREA_X, base_text_y + (has_achieved_lockable_success ? 40 : 20), 0, display_text_buffer);

        ALLEGRO_COLOR vol_indicator_box_color = current_volume_is_loud_enough ? al_map_rgb(0, 200, 0) : al_map_rgb(200, 0, 0);
        const char* vol_text = current_volume_is_loud_enough ? "音量達標!" : "音量不足";
        float vol_text_width = al_get_text_width(font, vol_text);
        float vol_indicator_x_start = WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH - vol_text_width - 20;
        al_draw_filled_rectangle(vol_indicator_x_start, base_text_y + (has_achieved_lockable_success ? 40 : 20),
                                 WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH, base_text_y + (has_achieved_lockable_success ? 40 : 20) + 20, vol_indicator_box_color);
        al_draw_text(font, al_map_rgb(0,0,0), vol_indicator_x_start + 5, base_text_y + (has_achieved_lockable_success ? 40 : 20) + (20-al_get_font_line_height(font))/2.0f, 0, vol_text);

    } else if (time_end_button_press > 0.0 && time_start_button_press > 0.0) { // After singing finished and was started
        char actual_start_str[20], final_actual_end_str[20];
        if (actual_singing_start_time < 0.0) strcpy(actual_start_str, "N/A");
        else sprintf(actual_start_str, "%.2fs", actual_singing_start_time);

        double displayed_end_time = has_achieved_lockable_success ? locked_successful_singing_end_time : current_actual_singing_end_time;
        if (displayed_end_time < 0.0) strcpy(final_actual_end_str, "N/A");
        else sprintf(final_actual_end_str, "%.2fs", displayed_end_time);

        sprintf(display_text_buffer, "按鈕間: %.2fs 至 %.2fs | 實際演唱: %s 至 %s",
                time_start_button_press, time_end_button_press, actual_start_str, final_actual_end_str);

        float text_y_pos = minigame_buttons[0].y - 60; // Position above buttons
        al_draw_text(font, al_map_rgb(220, 220, 180), SCREEN_WIDTH / 2.0f, text_y_pos, ALLEGRO_ALIGN_CENTER, display_text_buffer);
         if(has_achieved_lockable_success){ // Display if success was due to lock
            if(displayPleaseSingMessage == false) { // Only if it passed validation
                 al_draw_text(font, al_map_rgb(180, 255, 180), SCREEN_WIDTH / 2.0f, text_y_pos + 20, ALLEGRO_ALIGN_CENTER, "核心演唱已達標並通過!");
            }
        }
    }


    if (is_singing && isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL) {
        // --- DEBUG: Print buffer info for waveform ---
        DWORD bytes_per_sample = AUDIO_BITS_PER_SAMPLE / 8;
        DWORD total_samples_recorded_so_far = waveHdr.dwBytesRecorded / bytes_per_sample;
        printf("[DEBUG][Waveform] dwBytesRecorded=%lu, total_samples=%lu\n", waveHdr.dwBytesRecorded, total_samples_recorded_so_far);

        int num_samples_in_waveform_timespan = (int)(AUDIO_SAMPLE_RATE * WAVEFORM_DISPLAY_DURATION_SEC);

        if (total_samples_recorded_so_far > 1 && num_samples_in_waveform_timespan > 0 && WAVEFORM_AREA_WIDTH > 1) {
            short* all_samples = (short*)pWaveBuffer;
            // Print first 5 samples for debug
            printf("[DEBUG][Waveform] First 5 samples: ");
            for (int i = 0; i < 5 && i < (int)total_samples_recorded_so_far; ++i) {
                printf("%d ", all_samples[i]);
            }
            printf("\n");

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

    } else if (is_singing) {
        al_draw_text(font, al_map_rgb(200,200,200), WAVEFORM_AREA_X + WAVEFORM_AREA_WIDTH/2.0f, WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT/2.0f - al_get_font_line_height(font)/2.0f, ALLEGRO_ALIGN_CENTER, "等待麥克風...");
    }

    float plant_base_y = WAVEFORM_AREA_Y + WAVEFORM_AREA_HEIGHT + SCREEN_HEIGHT * 0.20f;
    if (plant_base_y > SCREEN_HEIGHT * 0.75f) plant_base_y = SCREEN_HEIGHT * 0.75f;
    float plant_x = SCREEN_WIDTH / 2.0f;
    float stem_height = 20 + flower_plant.growth_stage * 15;

    // Plant drawing logic (condensed for brevity, same as before)
    if (!seed_planted) { /* ... */ } else { /* ... */ }
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


    if (displayPleaseSingMessage) {
        char validation_msg[128];
        // Updated message to reflect all conditions
        sprintf(validation_msg, "按鈕間時間>%.0fs, 唱歌總時間>%.0fs, (若未鎖定)音量比例>%.0f%%",
                MIN_RECORDING_DURATION_SECONDS, SINGING_TOTAL_TIME_THRESHOLD, REQUIRED_SINGING_PROPORTION * 100.0f);
        float msg_y_pos = minigame_buttons[0].y - 30;
        if (time_end_button_press > 0.0 && time_start_button_press > 0.0) { // if final stats are shown
             msg_y_pos -= (has_achieved_lockable_success && !displayPleaseSingMessage ? 40 : 20); // Make more space if stats/lock info shown
        }
        al_draw_text(font, al_map_rgb(255, 100, 100), SCREEN_WIDTH / 2.0f, msg_y_pos, ALLEGRO_ALIGN_CENTER, validation_msg);
         if (has_achieved_lockable_success && time_end_button_press > 0.0 && displayPleaseSingMessage) { // If it was locked but still failed
            al_draw_text(font, al_map_rgb(255,150,150), SCREEN_WIDTH/2.0f, msg_y_pos + 15, ALLEGRO_ALIGN_CENTER, "(核心曾達標但其他條件如按鈕時長未滿足)");
        }
    }

    // --- Draw Buttons --- (condensed for brevity, same as before)
    if (!seed_planted) { Button* b = &minigame_buttons[0]; al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color); al_draw_text(font, b->text_color, b->x + b->width / 2.0f, b->y + (b->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b->text); }
    if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower) { Button* b = &minigame_buttons[1]; al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color); al_draw_text(font, b->text_color, b->x + b->width / 2.0f, b->y + (b->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b->text); }
    else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower) { Button* b = &minigame_buttons[5]; al_draw_filled_rectangle(b->x, b->y, b->x + b->width, b->y + b->height, b->is_hovered ? b->hover_color : b->color); al_draw_text(font, b->text_color, b->x + b->width / 2.0f, b->y + (b->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b->text); }
    if (is_singing) { Button* b_restart = &minigame_buttons[2]; al_draw_filled_rectangle(b_restart->x, b_restart->y, b_restart->x + b_restart->width, b_restart->y + b_restart->height, b_restart->is_hovered ? b_restart->hover_color : b_restart->color); al_draw_text(font, b_restart->text_color, b_restart->x + b_restart->width / 2.0f, b_restart->y + (b_restart->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_restart->text); Button* b_finish = &minigame_buttons[3]; al_draw_filled_rectangle(b_finish->x, b_finish->y, b_finish->x + b_finish->width, b_finish->y + b_finish->height, b_finish->is_hovered ? b_finish->hover_color : b_finish->color); al_draw_text(font, b_finish->text_color, b_finish->x + b_finish->width / 2.0f, b_finish->y + (b_finish->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_finish->text); }
    Button* b_exit = &minigame_buttons[4]; al_draw_filled_rectangle(b_exit->x, b_exit->y, b_exit->x + b_exit->width, b_exit->y + b_exit->height, b_exit->is_hovered ? b_exit->hover_color : b_exit->color); al_draw_text(font, b_exit->text_color, b_exit->x + b_exit->width / 2.0f, b_exit->y + (b_exit->height / 2.0f) - (al_get_font_line_height(font) / 2.0f), ALLEGRO_ALIGN_CENTER, b_exit->text);


    al_draw_text(font, al_map_rgb(220, 220, 255), SCREEN_WIDTH / 2.0f, 30, ALLEGRO_ALIGN_CENTER, "唱歌種花小遊戲");
    if (seed_planted) {
        al_draw_textf(font, al_map_rgb(200, 200, 200), SCREEN_WIDTH - 20, 30, ALLEGRO_ALIGN_RIGHT, "已唱歌曲: %d / %d", flower_plant.songs_sung, songs_to_flower);
    }
}

void handle_minigame1_input(ALLEGRO_EVENT ev) {
    // Input handling logic (condensed for brevity, same as before for button hover/clicks)
    if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) { /* ... */
        for (int i = 0; i < NUM_MINIGAME1_BUTTONS; ++i) { minigame_buttons[i].is_hovered = false; }
        if (!seed_planted && ev.mouse.x >= minigame_buttons[0].x && ev.mouse.x <= minigame_buttons[0].x + minigame_buttons[0].width && ev.mouse.y >= minigame_buttons[0].y && ev.mouse.y <= minigame_buttons[0].y + minigame_buttons[0].height) { minigame_buttons[0].is_hovered = true;}
        else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower && ev.mouse.x >= minigame_buttons[1].x && ev.mouse.x <= minigame_buttons[1].x + minigame_buttons[1].width && ev.mouse.y >= minigame_buttons[1].y && ev.mouse.y <= minigame_buttons[1].y + minigame_buttons[1].height) { minigame_buttons[1].is_hovered = true; }
        else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower && ev.mouse.x >= minigame_buttons[5].x && ev.mouse.x <= minigame_buttons[5].x + minigame_buttons[5].width && ev.mouse.y >= minigame_buttons[5].y && ev.mouse.y <= minigame_buttons[5].y + minigame_buttons[5].height) { minigame_buttons[5].is_hovered = true; }
        else if (is_singing) { if (ev.mouse.x >= minigame_buttons[2].x && ev.mouse.x <= minigame_buttons[2].x + minigame_buttons[2].width && ev.mouse.y >= minigame_buttons[2].y && ev.mouse.y <= minigame_buttons[2].y + minigame_buttons[2].height) { minigame_buttons[2].is_hovered = true; } if (ev.mouse.x >= minigame_buttons[3].x && ev.mouse.x <= minigame_buttons[3].x + minigame_buttons[3].width && ev.mouse.y >= minigame_buttons[3].y && ev.mouse.y <= minigame_buttons[3].y + minigame_buttons[3].height) { minigame_buttons[3].is_hovered = true; } }
        if (ev.mouse.x >= minigame_buttons[4].x && ev.mouse.x <= minigame_buttons[4].x + minigame_buttons[4].width && ev.mouse.y >= minigame_buttons[4].y && ev.mouse.y <= minigame_buttons[4].y + minigame_buttons[4].height) { minigame_buttons[4].is_hovered = true; }
    }
    else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev.mouse.button == 1) {
            bool button_clicked_this_event = false;
            if (!seed_planted && minigame_buttons[0].is_hovered) { // Plant Seed
                seed_planted = true; is_singing = false;
                flower_plant.songs_sung = 0; flower_plant.growth_stage = 0;
                displayPleaseSingMessage = false; button_clicked_this_event = true;
                time_start_button_press = 0.0; // Reset for next session's display logic
                time_end_button_press = 0.0;
                has_achieved_lockable_success = false; // Crucial reset
            }
            else if (seed_planted && !is_singing && flower_plant.growth_stage < songs_to_flower && minigame_buttons[1].is_hovered) { // Start Singing
                is_singing = true; start_actual_audio_recording(); // This resets all timing vars for the new session
                printf("Minigame: Start Singing button clicked.\n"); button_clicked_this_event = true;
            }
            else if (is_singing) {
                if (minigame_buttons[2].is_hovered) { // Restart singing (while already singing)
                    // Stop current recording abruptly, then start a new one
                    if (hWaveIn && isActuallyRecording) {
                        waveInReset(hWaveIn); // Stop data flow
                        if (waveHdr.dwFlags & WHDR_PREPARED) {
                            waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)); // Unprepare old header
                        }
                    }
                    start_actual_audio_recording(); // Re-initializes everything for a fresh start
                    printf("Minigame: Restart singing button clicked.\n"); button_clicked_this_event = true;
                }
                else if (minigame_buttons[3].is_hovered) { // Finish Singing
                    is_singing = false; // Set this first
                    bool sound_was_valid = stop_actual_audio_recording(); // Perform all checks
                    printf("Minigame: Finish Singing button clicked.\n");
                    if (sound_was_valid) {
                        if (flower_plant.songs_sung < songs_to_flower) { flower_plant.songs_sung++; }
                        flower_plant.growth_stage = flower_plant.songs_sung;
                        if(flower_plant.growth_stage == songs_to_flower){ flower_plant.which_flower = (rand() % 2); }
                        printf("DEBUG: Valid song recorded, growth updated.\n");
                    } else {
                        printf("DEBUG: Invalid sound detected. Song not counted.\n");
                    }
                    // Reset live feedback vars, but keep final times for display until next "Start Singing"
                    current_recording_elapsed_time_sec = 0.0f;
                    current_volume_is_loud_enough = false;
                    button_clicked_this_event = true;
                }
            }
            else if (seed_planted && !is_singing && flower_plant.growth_stage >= songs_to_flower && minigame_buttons[5].is_hovered) { // Harvest
                if (flower_plant.which_flower == 0) { player.item_quantities[0]++; printf("DEBUG: Harvested a Regular flower! Total: %d\n", player.item_quantities[0]); }
                else { player.item_quantities[1]++; printf("DEBUG: Harvested a Devil flower. Total: %d\n", player.item_quantities[1]); }
                flower_plant.songs_sung = 0; flower_plant.growth_stage = 0;
                seed_planted = false; displayPleaseSingMessage = false; button_clicked_this_event = true;
                time_start_button_press = 0.0; // Reset for next session's display logic
                time_end_button_press = 0.0;
                has_achieved_lockable_success = false; // Crucial reset
            }
            if (minigame_buttons[4].is_hovered) { // Exit Minigame
                if(is_singing && isActuallyRecording && hWaveIn) {
                    // If exiting while singing, capture end time but don't validate
                    time_end_button_press = al_get_time();
                    waveInReset(hWaveIn);
                }
                is_singing = false; // Ensure is_singing is false before cleanup
                cleanup_audio_recording();
                game_phase = GROWTH;
                button_clicked_this_event = true;
            }
            if (button_clicked_this_event) { for (int i = 0; i < NUM_MINIGAME1_BUTTONS; ++i) { minigame_buttons[i].is_hovered = false; } }
        }
    }
    else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) { // Exit Minigame
            if(is_singing && isActuallyRecording && hWaveIn) {
                time_end_button_press = al_get_time();
                waveInReset(hWaveIn);
            }
            is_singing = false; // Ensure is_singing is false before cleanup
            cleanup_audio_recording();
            game_phase = GROWTH;
        }
    }
}

void update_minigame1(void) {
    if (is_singing && isActuallyRecording && hWaveIn != NULL && pWaveBuffer != NULL) {
        double current_time_for_update = al_get_time();
        current_recording_elapsed_time_sec = (float)(current_time_for_update - allegro_recording_start_time);

        DWORD bytes_per_sample = AUDIO_BITS_PER_SAMPLE / 8;
        DWORD total_samples_in_buffer_now = waveHdr.dwBytesRecorded / bytes_per_sample;

        short* live_samples_ptr = (short*)pWaveBuffer;

        printf("[DEBUG][Update] is_singing=%d, isActuallyRecording=%d, hWaveIn=%p, pWaveBuffer=%p, total_samples_in_buffer_now=%lu, elapsed=%.2f\n",
            is_singing, isActuallyRecording, hWaveIn, pWaveBuffer, total_samples_in_buffer_now, current_recording_elapsed_time_sec);

        // --- Background Noise Calculation ---
        if (total_samples_in_buffer_now >= RMS_ANALYSIS_WINDOW_SAMPLES) {
            const short* latest_samples_for_noise = live_samples_ptr + (total_samples_in_buffer_now - RMS_ANALYSIS_WINDOW_SAMPLES);
            float latest_rms = calculate_rms(latest_samples_for_noise, RMS_ANALYSIS_WINDOW_SAMPLES);
            printf("[DEBUG][Update] BG RMS: latest_rms=%.2f, current_background_rms=%.2f\n", latest_rms, current_background_rms);
            if (!current_volume_is_loud_enough || current_background_rms < 50) {
                if (latest_rms < current_background_rms * 1.5) { // Only update if not drastically louder
                    current_background_rms = (current_background_rms * 0.99f) + (latest_rms * 0.01f);
                    if (current_background_rms < 50) current_background_rms = 50; // Floor for background
                }
            }
        }

        // --- Current Volume Check (Singing Detection for feedback) ---
        if (total_samples_in_buffer_now >= RMS_ANALYSIS_WINDOW_SAMPLES) {
            const short* current_analysis_samples = live_samples_ptr + (total_samples_in_buffer_now - RMS_ANALYSIS_WINDOW_SAMPLES);
            float current_rms = calculate_rms(current_analysis_samples, RMS_ANALYSIS_WINDOW_SAMPLES);
            printf("[DEBUG][Update] Current RMS: %.2f, BG RMS: %.2f, Threshold: %.2f, AbsoluteMin: %.2f\n",
                current_rms, current_background_rms, current_background_rms * SINGING_RMS_THRESHOLD_MULTIPLIER, MIN_ABSOLUTE_RMS_FOR_SINGING);

            if (current_rms > current_background_rms * SINGING_RMS_THRESHOLD_MULTIPLIER &&
                current_rms > MIN_ABSOLUTE_RMS_FOR_SINGING) {
                if (!current_volume_is_loud_enough)
                    printf("[DEBUG][Update] Volume just became loud enough!\n");
                current_volume_is_loud_enough = true;
            } else {
                if (current_volume_is_loud_enough)
                    printf("[DEBUG][Update] Volume just dropped below threshold.\n");
                current_volume_is_loud_enough = false;
            }
        } else {
            current_volume_is_loud_enough = false;
        }

        // --- Update actual_singing_start_time and current_actual_singing_end_time ---
        if (current_volume_is_loud_enough) {
            if (!internal_was_loud_last_frame) { // Rising edge: sound just became loud
                printf("[DEBUG][Update] Loud streak started at %.2f\n", current_time_for_update);
                internal_current_loud_streak_began_at_time = current_time_for_update;
            }
            double current_loud_duration = current_time_for_update - internal_current_loud_streak_began_at_time;

            if (current_loud_duration >= SINGING_DETECTION_MIN_STREAK_DURATION) {
                if (actual_singing_start_time < 0.0) {
                    printf("[DEBUG][Update] actual_singing_start_time set to %.2f\n", internal_current_loud_streak_began_at_time);
                    actual_singing_start_time = internal_current_loud_streak_began_at_time;
                }
                current_actual_singing_end_time = current_time_for_update;
                printf("[DEBUG][Update] current_actual_singing_end_time updated to %.2f\n", current_actual_singing_end_time);
            }
            internal_was_loud_last_frame = true;
        } else {
            if (internal_was_loud_last_frame)
                printf("[DEBUG][Update] Loud streak ended at %.2f\n", current_time_for_update);
            internal_was_loud_last_frame = false;
        }

        // --- Check for lockable success ---
        if (!has_achieved_lockable_success) {
            if (actual_singing_start_time > 0.0 && current_actual_singing_end_time > 0.0) {
                double current_singing_span = current_actual_singing_end_time - actual_singing_start_time;
                if (current_singing_span > SINGING_TOTAL_TIME_THRESHOLD) {
                    has_achieved_lockable_success = true;
                    locked_successful_singing_end_time = current_actual_singing_end_time;
                    printf("[DEBUG][Update] Core singing requirements MET and LOCKED at %.2fs (Singing span: %.2fs)\n",
                        locked_successful_singing_end_time, current_singing_span);
                }
            }
        }
    } else {
        printf("[DEBUG][Update] Not recording or not singing. is_singing=%d, isActuallyRecording=%d, hWaveIn=%p, pWaveBuffer=%p\n",
            is_singing, isActuallyRecording, hWaveIn, pWaveBuffer);
        current_volume_is_loud_enough = false;
    }
}


static void prepare_audio_recording(void) {
    // Same as before
    if (pWaveBuffer == NULL) { pWaveBuffer = (char*)malloc(AUDIO_BUFFER_SIZE); if (pWaveBuffer == NULL) { fprintf(stderr, "Fatal Error: Failed to allocate audio buffer.\n"); if (hWaveIn != NULL) { waveInClose(hWaveIn); hWaveIn = NULL; } return; } }
    WAVEFORMATEX wfx; wfx.wFormatTag = WAVE_FORMAT_PCM; wfx.nChannels = AUDIO_CHANNELS; wfx.nSamplesPerSec = AUDIO_SAMPLE_RATE; wfx.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8); wfx.nBlockAlign = AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8); wfx.wBitsPerSample = AUDIO_BITS_PER_SAMPLE; wfx.cbSize = 0;
    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) { char error_text[256]; waveInGetErrorTextA(result, error_text, sizeof(error_text)); fprintf(stderr, "Error: Could not open audio recording device (Error %u: %s).\nPlease ensure a microphone is connected and enabled.\n", result, error_text); if (pWaveBuffer) { free(pWaveBuffer); pWaveBuffer = NULL; } hWaveIn = NULL; return; }
    printf("DEBUG: Windows Audio recording prepared. Device opened.\n");
}

static void start_actual_audio_recording(void) {
    // Same as before, with new variable resets
    if (hWaveIn == NULL) {
        prepare_audio_recording();
        if (hWaveIn == NULL) {
            fprintf(stderr, "Audio system not ready. Cannot start recording.\n");
            is_singing = false;
            isActuallyRecording = false;
            return;
        }
    }

    // If a previous recording was prematurely stopped (e.g. restart), ensure header is clean
    if (waveHdr.dwFlags & WHDR_PREPARED) {
        waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    }

    ZeroMemory(&waveHdr, sizeof(WAVEHDR));
    waveHdr.lpData = pWaveBuffer;
    waveHdr.dwBufferLength = AUDIO_BUFFER_SIZE;
    waveHdr.dwFlags = 0;

    ZeroMemory(pWaveBuffer, AUDIO_BUFFER_SIZE); // Clear buffer for new recording

    MMRESULT prep_res = waveInPrepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    if (prep_res != MMSYSERR_NOERROR) { fprintf(stderr, "Error preparing wave header: %u\n", prep_res); is_singing = false; isActuallyRecording = false; return; }
    MMRESULT add_res = waveInAddBuffer(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    if (add_res != MMSYSERR_NOERROR) { fprintf(stderr, "Error adding wave buffer: %u\n", add_res); waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)); is_singing = false; isActuallyRecording = false; return; }
    MMRESULT start_res = waveInStart(hWaveIn);
    if (start_res != MMSYSERR_NOERROR) { fprintf(stderr, "Error starting wave input: %u\n", start_res); waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)); is_singing = false; isActuallyRecording = false; return; }

    isActuallyRecording = true;
    allegro_recording_start_time = al_get_time(); // This is the button press time
    time_start_button_press = allegro_recording_start_time; // Store it explicitly

    // Reset other timing variables for the new recording session
    time_end_button_press = 0.0; // Will be set when stop_actual_audio_recording is called
    actual_singing_start_time = -1.0;
    current_actual_singing_end_time = -1.0;
    locked_successful_singing_end_time = -1.0;
    has_achieved_lockable_success = false; // Crucial reset
    internal_was_loud_last_frame = false;
    internal_current_loud_streak_began_at_time = 0.0;

    displayPleaseSingMessage = false;
    overall_button_to_button_duration = 0.0f; // Reset duration

    current_recording_elapsed_time_sec = 0.0f;
    current_volume_is_loud_enough = false;
    current_background_rms = INITIAL_BACKGROUND_RMS_GUESS; // Reset background estimate

    printf("[DEBUG][Audio] start_actual_audio_recording() called. hWaveIn=%p\n", hWaveIn);
    printf("[DEBUG][Audio] isActuallyRecording=%d, allegro_recording_start_time=%.2f, time_start_button_press=%.2f\n",
        isActuallyRecording, allegro_recording_start_time, time_start_button_press);
}

static bool stop_actual_audio_recording(void) {
    if (!isActuallyRecording || hWaveIn == NULL) {
        printf("[DEBUG][Audio] stop_actual_audio_recording() called but not recording. isActuallyRecording=%d, hWaveIn=%p\n", isActuallyRecording, hWaveIn);
        isActuallyRecording = false;
        return false;
    }

    double current_stop_time = al_get_time();
    time_end_button_press = current_stop_time; // Set this first
    overall_button_to_button_duration = (float)(time_end_button_press - time_start_button_press);

    MMRESULT reset_res = waveInReset(hWaveIn); // Stop data flow into the buffer
    if (reset_res != MMSYSERR_NOERROR) {
        fprintf(stderr, "Error resetting wave input: %u\n", reset_res);
    }
    // waveHdr.dwBytesRecorded should now be stable for analysis
    isActuallyRecording = false; // Set this after stopping and before extensive processing

    printf("DEBUG: Windows Recording stopped at %.2f. Total button-to-button duration: %.2f seconds.\n", time_end_button_press, overall_button_to_button_duration);
    printf("DEBUG: Bytes recorded: %lu. ActualSingStart: %.2f, CurrentActualSingEnd (at stop): %.2f, LockedEnd: %.2f, LockAchieved: %s\n",
           waveHdr.dwBytesRecorded, actual_singing_start_time, current_actual_singing_end_time,
           locked_successful_singing_end_time, has_achieved_lockable_success ? "Yes" : "No");

    bool validationSuccess = true;
    displayPleaseSingMessage = false;

    if (force_audio_too_short_for_test) { // For testing
        overall_button_to_button_duration = 0.5f; // Simulate short recording
        printf("DEBUG: FORCED BUTTON-TO-BUTTON DURATION TOO SHORT (%.2f sec).\n", overall_button_to_button_duration);
    }

    // --- VALIDATION LOGIC ---

    // Condition 1: 按鈕間時間 (Button-to-button duration)
    if (overall_button_to_button_duration < MIN_RECORDING_DURATION_SECONDS) {
        printf("DEBUG FAIL: Button-to-button duration (%.2fs) < min required (%.1fs).\n", overall_button_to_button_duration, MIN_RECORDING_DURATION_SECONDS);
        validationSuccess = false;
    }

    // Analyze recorded audio for total_time_above_rms_threshold (needed for proportion if not locked)
    short* samples = (short*)pWaveBuffer;
    DWORD num_samples_actually_recorded = waveHdr.dwBytesRecorded / (AUDIO_BITS_PER_SAMPLE / 8);
    float total_time_above_rms_threshold = 0.0f;

    if (validationSuccess && num_samples_actually_recorded == 0) { // Check only if not already failed
        printf("DEBUG FAIL: No audio data recorded (dwBytesRecorded is 0 after stop).\n");
        validationSuccess = false;
    } else if (num_samples_actually_recorded > 0) { // Only analyze if there's data
        long loud_frame_count = 0;
        float persistent_background_rms = INITIAL_BACKGROUND_RMS_GUESS; // Re-estimate for the whole recording
        const int frame_size = RMS_ANALYSIS_WINDOW_SAMPLES;

        for (DWORD frame_start_sample = 0; (frame_start_sample + frame_size) <= num_samples_actually_recorded; frame_start_sample += frame_size) {
            const short* current_frame_samples = samples + frame_start_sample;
            float frame_rms = calculate_rms(current_frame_samples, frame_size);

            if (frame_rms < persistent_background_rms * 1.2f && frame_rms < MIN_ABSOLUTE_RMS_FOR_SINGING * 1.5f) {
                 persistent_background_rms = (persistent_background_rms * 0.95f) + (frame_rms * 0.05f);
                 if(persistent_background_rms < 50) persistent_background_rms = 50;
            }
            if (frame_rms > persistent_background_rms * SINGING_RMS_THRESHOLD_MULTIPLIER &&
                frame_rms > MIN_ABSOLUTE_RMS_FOR_SINGING) {
                loud_frame_count++;
            }
        }
        total_time_above_rms_threshold = (float)loud_frame_count * frame_size / AUDIO_SAMPLE_RATE;
        printf("DEBUG: [Validation] Full recording - Est. BG RMS: %.1f. Loud frames: %ld. Total time loud: %.2f s\n",
               persistent_background_rms, loud_frame_count, total_time_above_rms_threshold);
    }


    // Condition 2: Core singing requirements (唱歌總時間 & potentially proportion)
    if (validationSuccess) { // Only proceed if Condition 1 (button duration) passed & data exists
        if (has_achieved_lockable_success) {
            // Player met "唱歌總時間 > 20s" requirement DURING recording.
            // Use the locked_successful_singing_end_time. Proportion check is WAIVED.
            double locked_singing_duration = 0.0;
            if (actual_singing_start_time > 0.0 && locked_successful_singing_end_time > 0.0 && locked_successful_singing_end_time > actual_singing_start_time) {
                locked_singing_duration = locked_successful_singing_end_time - actual_singing_start_time;
            }

            // The primary check for lockable success was already "> SINGING_TOTAL_TIME_THRESHOLD" in update.
            // Here, we just confirm it's still valid (it should be).
            if (locked_singing_duration <= SINGING_TOTAL_TIME_THRESHOLD) {
                // This would indicate a logic error if has_achieved_lockable_success is true
                // but the duration isn't met.
                printf("DEBUG FAIL (Locked Path Error): Locked singing duration (%.2fs) unexpectedly <= threshold (%.1fs).\n", locked_singing_duration, SINGING_TOTAL_TIME_THRESHOLD);
                validationSuccess = false;
            } else {
                printf("DEBUG PASS (Locked Path): Core singing requirements met via lock. Locked singing duration: %.2fs. Proportion check waived.\n", locked_singing_duration);
                // validationSuccess remains true
            }
        } else {
            // Lock was NOT achieved. Apply standard checks for 唱歌總時間 and 比例.
            double final_actual_singing_duration = 0.0;
            // Use current_actual_singing_end_time, which was the value at the moment of stopping recording.
            if (actual_singing_start_time > 0.0 && current_actual_singing_end_time > 0.0 && current_actual_singing_end_time > actual_singing_start_time) {
                final_actual_singing_duration = current_actual_singing_end_time - actual_singing_start_time;
            }

            // Check 2a: 唱歌總時間 (actual_end - actual_start)
            if (final_actual_singing_duration <= SINGING_TOTAL_TIME_THRESHOLD) {
                printf("DEBUG FAIL (Not Locked): Actual singing duration (%.2fs) <= threshold (%.1fs).\n", final_actual_singing_duration, SINGING_TOTAL_TIME_THRESHOLD);
                validationSuccess = false;
            }

            // Check 2b: 比例 (Proportion of loud time vs button-to-button time)
            // This check is only performed if previous checks (including 2a for non-locked) passed.
            if (validationSuccess) { // If still valid after 2a
                if (overall_button_to_button_duration > 0) { // Avoid division by zero
                    float singing_proportion = total_time_above_rms_threshold / overall_button_to_button_duration;
                    if (singing_proportion < REQUIRED_SINGING_PROPORTION) {
                        printf("DEBUG FAIL (Not Locked): Singing proportion (%.2f / %.2f = %.3f) < required (%.3f).\n",
                               total_time_above_rms_threshold, overall_button_to_button_duration, singing_proportion, REQUIRED_SINGING_PROPORTION);
                        validationSuccess = false;
                    }
                } else if (total_time_above_rms_threshold > 0) {
                     printf("DEBUG FAIL (Not Locked): Button duration is 0 but loud time > 0. Invalid state.\n");
                     validationSuccess = false;
                } else { // Both overall_button_to_button_duration and total_time_above_rms_threshold are 0 (or overall is <=0)
                    // If MIN_RECORDING_DURATION_SECONDS was > 0, it would have failed earlier.
                    // If MIN_RECORDING_DURATION_SECONDS is 0, and proportion is required, this must fail.
                    if (REQUIRED_SINGING_PROPORTION > 0.0f) {
                        printf("DEBUG FAIL (Not Locked): Zero button duration and/or zero loud time, proportion fails if required.\n");
                        validationSuccess = false;
                    }
                }
            }

            if (validationSuccess && !has_achieved_lockable_success) {
                 printf("DEBUG PASS (Not Locked): All standard requirements met.\n");
            }
        }
    }


    if (!validationSuccess) {
        displayPleaseSingMessage = true;
        printf("DEBUG: Overall Audio validation FAILED.\n");
    } else {
        printf("DEBUG: Overall Audio validation SUCCEEDED.\n");
        displayPleaseSingMessage = false; // Ensure message is cleared on success
    }

    // Unprepare header must be done AFTER all data from pWaveBuffer is used.
    if (waveHdr.dwFlags & WHDR_PREPARED) {
        MMRESULT unprep_res = waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
        if (unprep_res != MMSYSERR_NOERROR && unprep_res != WAVERR_UNPREPARED) { // WAVERR_UNPREPARED is ok if already unprepared
             fprintf(stderr, "Error unpreparing wave header: %u\n", unprep_res);
        }
    }
    // waveHdr.dwFlags should now not have WHDR_PREPARED
    // Reset waveHdr.dwBytesRecorded for safety, though it will be zeroed on next prep.
    // waveHdr.dwBytesRecorded = 0;


    return validationSuccess;
}


static void cleanup_audio_recording(void) {
    // Same as before
    printf("DEBUG: Windows cleanup_audio_recording() called.\n");
    if (isActuallyRecording && hWaveIn != NULL) { // Should not happen if logic is correct, stop_actual_audio_recording should set isActuallyRecording to false
        waveInReset(hWaveIn);
        isActuallyRecording = false;
    }
    if (hWaveIn != NULL) {
        // It's possible waveHdr was prepared but not added/started if an error occurred mid-prepare.
        // Or if stop_actual_audio_recording didn't complete unprepare.
        if (waveHdr.dwFlags & WHDR_PREPARED) {
            MMRESULT res_unprep = waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
            if (res_unprep != MMSYSERR_NOERROR && res_unprep != WAVERR_UNPREPARED) {
                fprintf(stderr, "Error unpreparing wave header during cleanup: %u\n", res_unprep);
            }
        }
        MMRESULT close_res = waveInClose(hWaveIn);
        if (close_res != MMSYSERR_NOERROR) {
            // Common error is STILL_PLAYING if waveInReset wasn't effective or called.
            // Or if another thread is somehow accessing it (not the case here).
            fprintf(stderr, "Error closing wave input device: %u\n", close_res);
        }
        hWaveIn = NULL;
    }
    if (pWaveBuffer != NULL) {
        free(pWaveBuffer);
        pWaveBuffer = NULL;
    }
    // Explicitly clear waveHdr structure after freeing its buffer and closing device
    ZeroMemory(&waveHdr, sizeof(WAVEHDR));
}