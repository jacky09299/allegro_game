#include "player_skill_select.h"
#include "player_attack.h"
#include "globals.h"
#include "player.h"
#include "config.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h>
#include "asset_manager.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h> // For rand()
#include <time.h>   // For srand()

// --- UI Layout Definitions ---
#define SKILL_LIBRARY_ROWS 3
#define SKILL_LIBRARY_COLS 5
#define NUM_DISPLAY_SKILLS 10

// Positions and sizes
#define EQUIPPED_SLOT_X 150
#define EQUIPPED_SLOT_Y 100
#define EQUIPPED_SLOT_SIZE 100
#define EQUIPPED_SLOT_PADDING 10

#define LIBRARY_GRID_X 150
#define LIBRARY_GRID_Y 300
#define LIBRARY_SLOT_SIZE 150
#define LIBRARY_SLOT_PADDING 10
#define TEXT_HEIGHT 30

#define PLAYER_PREVIEW_BOX_X (SCREEN_WIDTH - 350)
#define PLAYER_PREVIEW_BOX_Y 100
#define PLAYER_PREVIEW_BOX_WIDTH 300
#define PLAYER_PREVIEW_BOX_HEIGHT 300
#define PLAYER_PREVIEW_PADDING 10
#define PLAYER_IMAGE_PATH "assets/image/player.png"

SkillIdentifier player_skill_group[MAX_EQUIPPED_SKILLS];
static ALLEGRO_BITMAP* skill_bitmaps[MAX_PLAYER_SKILLS];
static ALLEGRO_BITMAP* player_image = NULL;
static SkillIdentifier available_library_skills[NUM_DISPLAY_SKILLS];
static bool is_dragging = false;
static SkillIdentifier dragged_skill = SKILL_NONE;
static int dragged_skill_original_slot = -1;
static float drag_x = 0, drag_y = 0;

static ALLEGRO_BITMAP* voronoi_overlay = NULL;
static bool voronoi_needs_update = true; 

const char* skill_icon_filenames[MAX_PLAYER_SKILLS] = {
    NULL,
    "assets/image/skill1.png", "assets/image/skill2.png", "assets/image/skill3.png",
    "assets/image/skill4.png", "assets/image/skill5.png", "assets/image/skill6.png",
    "assets/image/skill7.png", "assets/image/skill8.png", "assets/image/skill9.png", 
    "assets/image/skill9.png"
};

const char* skill_names[] = {
    "無", "閃電鏈", "治癒", "元素彈",
    "元素衝刺", "蓄力槍", "集中", "水晶浮球", "符文爆破", "完美防禦" , "反轉結界"
    };

static void generate_voronoi_overlay();

void init_player_skill_select() {
    srand(time(NULL)); // 初始化隨機數種子，讓每次的 Voronoi 圖案都不同

    if (!al_is_image_addon_initialized()) {
        al_init_image_addon();
    }
    player_image = load_bitmap_once("assets/image/player.png");

    for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
        player_skill_group[i] = SKILL_NONE;
    }

    for (int i = SKILL_NONE + 1; i < MAX_PLAYER_SKILLS; ++i) {
        if (skill_icon_filenames[i] != NULL) {
            skill_bitmaps[i] = load_bitmap_once(skill_icon_filenames[i]);
        } else {
            skill_bitmaps[i] = NULL;
        }
    }

    available_library_skills[0] = SKILL_LIGHTNING_BOLT;
    available_library_skills[1] = SKILL_HEAL;
    available_library_skills[2] = SKILL_ELEMENT_BALL;
    available_library_skills[3] = SKILL_ELEMENT_DASH; //元素衝刺
    available_library_skills[4] = SKILL_CHARGE_BEAM; //充能元素槍
    available_library_skills[5] = SKILL_FOCUS; //集中
    available_library_skills[6] = SKILL_ARCANE_ORB; //水晶浮球四射
    available_library_skills[7] = SKILL_RUNE_IMPLOSION; //符文爆破
    available_library_skills[8] = SKILL_REFLECT_BARRIER; //反轉結界
    available_library_skills[9] = SKILL_PREFECT_DEFENSE; //完美防禦
    


    is_dragging = false;
    dragged_skill = SKILL_NONE;
    dragged_skill_original_slot = -1;

    voronoi_overlay = NULL;
    voronoi_needs_update = true; 

}


void handle_player_skill_select_input(ALLEGRO_EVENT* ev) {
    if (!ev) return;

    if (ev->type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GROWTH;
            return;
        }
    }

    bool skills_changed = false;

    if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        float mouse_x = ev->mouse.x;
        float mouse_y = ev->mouse.y;

        // 右鍵卸載技能
        if (ev->mouse.button == 2) {
            for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
                float slot_x = EQUIPPED_SLOT_X + i * (EQUIPPED_SLOT_SIZE + EQUIPPED_SLOT_PADDING);
                float slot_y = EQUIPPED_SLOT_Y;
                if (mouse_x >= slot_x && mouse_x <= slot_x + EQUIPPED_SLOT_SIZE && mouse_y >= slot_y && mouse_y <= slot_y + EQUIPPED_SLOT_SIZE) {
                    if (player_skill_group[i] != SKILL_NONE) {
                        player_skill_group[i] = SKILL_NONE;
                        skills_changed = true;
                        break;
                    }
                }
            }
        }

        // 左鍵點擊或開始拖曳
        if (ev->mouse.button == 1) {
            // 從技能庫點擊裝備
            for (int i = 0; i < NUM_DISPLAY_SKILLS; ++i) {
                int row = i / SKILL_LIBRARY_COLS;
                int col = i % SKILL_LIBRARY_COLS;
                float cell_x = LIBRARY_GRID_X + col * (LIBRARY_SLOT_SIZE + LIBRARY_SLOT_PADDING);
                float cell_y = LIBRARY_GRID_Y + row * (LIBRARY_SLOT_SIZE + LIBRARY_SLOT_PADDING + TEXT_HEIGHT);
                if (mouse_x >= cell_x && mouse_x <= cell_x + LIBRARY_SLOT_SIZE && mouse_y >= cell_y && mouse_y <= cell_y + LIBRARY_SLOT_SIZE) {
                    SkillIdentifier clicked_skill_id = available_library_skills[i];
                    if (clicked_skill_id == SKILL_NONE) continue;

                    bool already_equipped = false;
                    for (int j = 0; j < MAX_EQUIPPED_SKILLS; ++j) {
                        if (player_skill_group[j] == clicked_skill_id) {
                            already_equipped = true;
                            break;
                        }
                    }
                    if (!already_equipped) {
                        for (int j = 0; j < MAX_EQUIPPED_SKILLS; ++j) {
                            if (player_skill_group[j] == SKILL_NONE) {
                                player_skill_group[j] = clicked_skill_id;
                                skills_changed = true;
                                break;
                            }
                        }
                    }
                    if (skills_changed) break;
                }
            }
            if (skills_changed) goto end_input_handling;

            // 從已裝備區開始拖曳
            for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
                float slot_x = EQUIPPED_SLOT_X + i * (EQUIPPED_SLOT_SIZE + EQUIPPED_SLOT_PADDING);
                float slot_y = EQUIPPED_SLOT_Y;
                if (mouse_x >= slot_x && mouse_x <= slot_x + EQUIPPED_SLOT_SIZE && mouse_y >= slot_y && mouse_y <= slot_y + EQUIPPED_SLOT_SIZE) {
                    if (player_skill_group[i] != SKILL_NONE) {
                        is_dragging = true;
                        dragged_skill = player_skill_group[i];
                        dragged_skill_original_slot = i;
                        drag_x = mouse_x;
                        drag_y = mouse_y;
                        player_skill_group[i] = SKILL_NONE; // 暫時移除以顯示拖曳效果
                        skills_changed = true; // 拖曳開始，預覽圖應更新
                        break;
                    }
                }
            }
        }
    } else if (ev->type == ALLEGRO_EVENT_MOUSE_AXES) {
        if (is_dragging) {
            drag_x = ev->mouse.x;
            drag_y = ev->mouse.y;
        }
    } else if (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
        if (ev->mouse.button == 1 && is_dragging) {
            float mouse_x = ev->mouse.x;
            float mouse_y = ev->mouse.y;
            int target_slot_index = -1;

            // 檢查是否放置在某個已裝備欄位上
            for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
                float slot_x = EQUIPPED_SLOT_X + i * (EQUIPPED_SLOT_SIZE + EQUIPPED_SLOT_PADDING);
                float slot_y = EQUIPPED_SLOT_Y;
                if (mouse_x >= slot_x && mouse_x <= slot_x + EQUIPPED_SLOT_SIZE && mouse_y >= slot_y && mouse_y <= slot_y + EQUIPPED_SLOT_SIZE) {
                    target_slot_index = i;
                    break;
                }
            }

            if (target_slot_index != -1) { // 放置在有效欄位
                SkillIdentifier skill_already_in_target_slot = player_skill_group[target_slot_index];
                player_skill_group[target_slot_index] = dragged_skill; // 放置新技能
                // 如果目標欄位原本有技能，則與原欄位交換
                if (skill_already_in_target_slot != SKILL_NONE && target_slot_index != dragged_skill_original_slot) {
                    player_skill_group[dragged_skill_original_slot] = skill_already_in_target_slot;
                }
            } else { // 放置在無效區域，返回原位
                player_skill_group[dragged_skill_original_slot] = dragged_skill;
            }
            
            skills_changed = true;
            is_dragging = false;
            dragged_skill = SKILL_NONE;
            dragged_skill_original_slot = -1;
        }
    }

end_input_handling:
    if (skills_changed) {
        voronoi_needs_update = true;
    }
}

ALLEGRO_COLOR blend(ALLEGRO_COLOR base, ALLEGRO_COLOR overlay, float alpha) {
    float r1, g1, b1, a1;
    float r2, g2, b2, a2;
    al_unmap_rgba_f(base, &r1, &g1, &b1, &a1);
    al_unmap_rgba_f(overlay, &r2, &g2, &b2, &a2);

    float r = r1 * (1 - alpha) + r2 * alpha;
    float g = g1 * (1 - alpha) + g2 * alpha;
    float b = b1 * (1 - alpha) + b2 * alpha;
    float a = a1; // 保持原本主要圖的 alpha

    return al_map_rgba_f(r, g, b, a);
}


static void generate_voronoi_overlay() {
    if (voronoi_overlay) {
        al_destroy_bitmap(voronoi_overlay);
        voronoi_overlay = NULL;
    }

    int equipped_skill_count = 0;
    SkillIdentifier equipped_ids[MAX_EQUIPPED_SKILLS];
    for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
        if (player_skill_group[i] != SKILL_NONE) {
            equipped_ids[equipped_skill_count++] = player_skill_group[i];
        }
    }

    if (equipped_skill_count == 0) {
        return; // 沒有技能，不需要疊圖
    }

    const int OVERLAY_WIDTH = PLAYER_PREVIEW_BOX_WIDTH - 2 * PLAYER_PREVIEW_PADDING;
    const int OVERLAY_HEIGHT = PLAYER_PREVIEW_BOX_HEIGHT - 2 * PLAYER_PREVIEW_PADDING;

    voronoi_overlay = al_create_bitmap(OVERLAY_WIDTH, OVERLAY_HEIGHT);

    float seed_points[MAX_EQUIPPED_SKILLS][2];
    for (int i = 0; i < equipped_skill_count; i++) {
        seed_points[i][0] = rand() % OVERLAY_WIDTH;
        seed_points[i][1] = rand() % OVERLAY_HEIGHT;
    }

    ALLEGRO_BITMAP* temp_skill_bitmaps[MAX_EQUIPPED_SKILLS] = {NULL};
    ALLEGRO_BITMAP* old_target = al_get_target_bitmap();
    for (int i = 0; i < equipped_skill_count; ++i) {
        SkillIdentifier skill_id = equipped_ids[i];
        if (skill_bitmaps[skill_id] != NULL) {
            temp_skill_bitmaps[i] = al_create_bitmap(OVERLAY_WIDTH, OVERLAY_HEIGHT);
            if (temp_skill_bitmaps[i]) {
                al_set_target_bitmap(temp_skill_bitmaps[i]);
                al_clear_to_color(al_map_rgba(0, 0, 0, 0));
                al_draw_scaled_bitmap(skill_bitmaps[skill_id], 0, 0, al_get_bitmap_width(skill_bitmaps[skill_id]), al_get_bitmap_height(skill_bitmaps[skill_id]), 0, 0, OVERLAY_WIDTH, OVERLAY_HEIGHT, 0);
            }
        }
    }
    al_set_target_bitmap(old_target);

    al_set_target_bitmap(voronoi_overlay);
    al_lock_bitmap(voronoi_overlay, ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_WRITEONLY);
    for (int i = 0; i < equipped_skill_count; ++i) {
        if (temp_skill_bitmaps[i]) al_lock_bitmap(temp_skill_bitmaps[i], ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_READONLY);
    }

    for (int y = 0; y < OVERLAY_HEIGHT; y++) {
        for (int x = 0; x < OVERLAY_WIDTH; x++) {
            float min_dist_sq = -1.0f;
            int closest_seed_index = -1;
            for (int i = 0; i < equipped_skill_count; i++) {
                float dx = x - seed_points[i][0];
                float dy = y - seed_points[i][1];
                float dist_sq = dx * dx + dy * dy;
                if (closest_seed_index == -1 || dist_sq < min_dist_sq) {
                    min_dist_sq = dist_sq;
                    closest_seed_index = i;
                }
            }
            float alpha = 0.05f;
            if (closest_seed_index != -1 && temp_skill_bitmaps[closest_seed_index]) {
                ALLEGRO_COLOR final_color = al_get_pixel(temp_skill_bitmaps[closest_seed_index], x, y);

                for (int j = 0; j < equipped_skill_count; ++j) {
                    if (j == closest_seed_index || !temp_skill_bitmaps[j]) continue;

                    ALLEGRO_COLOR overlay_color = al_get_pixel(temp_skill_bitmaps[j], x, y);
                    final_color = blend(final_color, overlay_color, alpha);
                }

                al_put_pixel(x, y, final_color);
            }
        }
    }

    al_unlock_bitmap(voronoi_overlay);
    for (int i = 0; i < equipped_skill_count; ++i) {
        if (temp_skill_bitmaps[i]) al_unlock_bitmap(temp_skill_bitmaps[i]);
    }
    
    al_set_target_bitmap(old_target);

    for (int i = 0; i < equipped_skill_count; ++i) {
        if (temp_skill_bitmaps[i]) al_destroy_bitmap(temp_skill_bitmaps[i]);
    }
}


void render_player_skill_select() {
    al_draw_filled_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, al_map_rgba(0, 0, 0, 150));
    ALLEGRO_COLOR slot_color = al_map_rgb(50, 50, 50);
    ALLEGRO_COLOR border_color = al_map_rgb(150, 150, 150);
    al_draw_text(font, al_map_rgb(255, 255, 0), EQUIPPED_SLOT_X, EQUIPPED_SLOT_Y - TEXT_HEIGHT - 10, 0, "技能組:");
    al_draw_text(font, al_map_rgb(0, 255, 255), LIBRARY_GRID_X, LIBRARY_GRID_Y - TEXT_HEIGHT - 10, 0, "技能庫:");
    
    // --- 操作說明 ---
    const char* op1 = "左鍵：從技能庫裝備/拖曳技能";
    const char* op2 = "右鍵：卸載技能 | ESC：返回  ";
    float op_y = SCREEN_HEIGHT - 3 * TEXT_HEIGHT - 30;
    al_draw_text(font, al_map_rgb(255,255,255), SCREEN_WIDTH - 20, op_y, ALLEGRO_ALIGN_RIGHT, op1);
    al_draw_text(font, al_map_rgb(255,255,255), SCREEN_WIDTH - 20, op_y + TEXT_HEIGHT + 5, ALLEGRO_ALIGN_RIGHT, op2);

    for (int i = 0; i < MAX_EQUIPPED_SKILLS; ++i) {
        float slot_x = EQUIPPED_SLOT_X + i * (EQUIPPED_SLOT_SIZE + EQUIPPED_SLOT_PADDING);
        float slot_y = EQUIPPED_SLOT_Y;
        al_draw_filled_rectangle(slot_x, slot_y, slot_x + EQUIPPED_SLOT_SIZE, slot_y + EQUIPPED_SLOT_SIZE, slot_color);
        SkillIdentifier skill_id = player_skill_group[i];
        if (skill_id != SKILL_NONE && skill_bitmaps[skill_id] != NULL) {
            al_draw_scaled_bitmap(skill_bitmaps[skill_id], 0, 0, al_get_bitmap_width(skill_bitmaps[skill_id]), al_get_bitmap_height(skill_bitmaps[skill_id]), slot_x, slot_y, EQUIPPED_SLOT_SIZE, EQUIPPED_SLOT_SIZE, 0);
        }
        al_draw_rectangle(slot_x, slot_y, slot_x + EQUIPPED_SLOT_SIZE, slot_y + EQUIPPED_SLOT_SIZE, border_color, 2.0);
    }

    for (int i = 0; i < NUM_DISPLAY_SKILLS; ++i) {
        int row = i / SKILL_LIBRARY_COLS;
        int col = i % SKILL_LIBRARY_COLS;
        float cell_x = LIBRARY_GRID_X + col * (LIBRARY_SLOT_SIZE + LIBRARY_SLOT_PADDING);
        float cell_y = LIBRARY_GRID_Y + row * (LIBRARY_SLOT_SIZE + LIBRARY_SLOT_PADDING + TEXT_HEIGHT);
        al_draw_filled_rectangle(cell_x, cell_y, cell_x + LIBRARY_SLOT_SIZE, cell_y + LIBRARY_SLOT_SIZE, slot_color);
        SkillIdentifier skill_id = available_library_skills[i];
        if (skill_id != SKILL_NONE && skill_bitmaps[skill_id] != NULL) {
            al_draw_scaled_bitmap(skill_bitmaps[skill_id], 0, 0, al_get_bitmap_width(skill_bitmaps[skill_id]), al_get_bitmap_height(skill_bitmaps[skill_id]), cell_x, cell_y, LIBRARY_SLOT_SIZE, LIBRARY_SLOT_SIZE, 0);
        }
        al_draw_rectangle(cell_x, cell_y, cell_x + LIBRARY_SLOT_SIZE, cell_y + LIBRARY_SLOT_SIZE, border_color, 2.0);
        al_draw_rectangle(cell_x, cell_y + LIBRARY_SLOT_SIZE, cell_x + LIBRARY_SLOT_SIZE, cell_y + LIBRARY_SLOT_SIZE + TEXT_HEIGHT, border_color, 2.0);
        al_draw_filled_rectangle(cell_x, cell_y + LIBRARY_SLOT_SIZE, cell_x + LIBRARY_SLOT_SIZE, cell_y + LIBRARY_SLOT_SIZE + TEXT_HEIGHT, slot_color);
        al_draw_text(font, al_map_rgb(255, 255, 255), cell_x + LIBRARY_SLOT_SIZE / 2.0, cell_y + LIBRARY_SLOT_SIZE + (TEXT_HEIGHT - al_get_font_line_height(font)) / 2.0, ALLEGRO_ALIGN_CENTER, skill_names[skill_id]);

    }

    if (is_dragging && dragged_skill != SKILL_NONE && skill_bitmaps[dragged_skill] != NULL) {
        float bmp_x = drag_x - EQUIPPED_SLOT_SIZE / 2.0;
        float bmp_y = drag_y - EQUIPPED_SLOT_SIZE / 2.0;
        al_draw_scaled_bitmap(skill_bitmaps[dragged_skill], 0, 0, al_get_bitmap_width(skill_bitmaps[dragged_skill]), al_get_bitmap_height(skill_bitmaps[dragged_skill]), bmp_x, bmp_y, EQUIPPED_SLOT_SIZE, EQUIPPED_SLOT_SIZE, 0);
    }

    al_draw_filled_rectangle(PLAYER_PREVIEW_BOX_X, PLAYER_PREVIEW_BOX_Y, PLAYER_PREVIEW_BOX_X + PLAYER_PREVIEW_BOX_WIDTH, PLAYER_PREVIEW_BOX_Y + PLAYER_PREVIEW_BOX_HEIGHT, al_map_rgb(40, 40, 40));

    if (voronoi_needs_update) {
        generate_voronoi_overlay();
        voronoi_needs_update = false;
    }
    if (voronoi_overlay) {
        al_draw_bitmap(voronoi_overlay, PLAYER_PREVIEW_BOX_X + PLAYER_PREVIEW_PADDING, PLAYER_PREVIEW_BOX_Y + PLAYER_PREVIEW_PADDING, 0);
    }

    if (player_image) {
        al_draw_scaled_bitmap(
            player_image,
            0, 0, al_get_bitmap_width(player_image), al_get_bitmap_height(player_image),
            PLAYER_PREVIEW_BOX_X + PLAYER_PREVIEW_PADDING,
            PLAYER_PREVIEW_BOX_Y + PLAYER_PREVIEW_PADDING,
            PLAYER_PREVIEW_BOX_WIDTH - 2 * PLAYER_PREVIEW_PADDING,
            PLAYER_PREVIEW_BOX_HEIGHT - 2 * PLAYER_PREVIEW_PADDING,
            0
        );
    }

    al_draw_rectangle(PLAYER_PREVIEW_BOX_X, PLAYER_PREVIEW_BOX_Y, PLAYER_PREVIEW_BOX_X + PLAYER_PREVIEW_BOX_WIDTH, PLAYER_PREVIEW_BOX_Y + PLAYER_PREVIEW_BOX_HEIGHT, al_map_rgb(120, 120, 120), 2.0);
}

void update_player_skill_select() {
    // 目前沒有需要每幀更新的邏輯
}

