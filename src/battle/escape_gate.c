#include "escape_gate.h"
#include "globals.h"
#include "player.h"
#include "config.h"
#include <allegro5/allegro_primitives.h>
#include <math.h>
#include "asset_manager.h"

ALLEGRO_BITMAP* gate_img = NULL;

// 新增：控制是否顯示羅盤


/**
 * 初始化逃生門的狀態。
 */
void init_escape_gate() {
    // 定義逃生門可生成的範圍
    const float MIN_X = -500;
    const float MAX_X = 500;
    const float MIN_Y = -500;
    const float MAX_Y = 500;
    
    // 在範圍內隨機生成位置
    escape_gate.x = MIN_X + (rand() / (float)RAND_MAX) * (MAX_X - MIN_X);
    escape_gate.y = MIN_Y + (rand() / (float)RAND_MAX) * (MAX_Y - MIN_Y);
    
    escape_gate.width = 80;
    escape_gate.height = 100;
    escape_gate.is_active = true;
    escape_gate.is_counting_down = false;
    escape_gate.countdown_frames = 300; // 3秒倒數 (假設60FPS)
    gate_img = load_bitmap_once("assets/image/escape_gate.png");
    show_compass = false; // 預設顯示羅盤
}

/**
 * 更新逃生門的邏輯，包括偵測玩家碰撞與倒數邏輯。
 */
void update_escape_gate() {
    if (!escape_gate.is_active) return;

    float px = player.x;
    float py = player.y;

    bool player_in_gate =
        (px >= escape_gate.x && px <= escape_gate.x + escape_gate.width &&
         py >= escape_gate.y && py <= escape_gate.y + escape_gate.height);

    if (player_in_gate) {
        if (!escape_gate.is_counting_down) {
            escape_gate.is_counting_down = true;
            escape_gate.countdown_frames = 300;
        } else {
            escape_gate.countdown_frames--;
            if (escape_gate.countdown_frames <= 0) {
                // 逃離成功，切換回養成畫面
                escape_gate.is_active = false;
                escape_gate.is_counting_down = false;
                escape_gate.countdown_frames = 0;

                game_phase = GROWTH;
                day_time = 1;
                current_day++;

                camera_x = player.x - SCREEN_WIDTH / 2;
                camera_y = player.y - SCREEN_HEIGHT / 2;
            }
        }
    } else {
        // 玩家離開門範圍，中止倒數
        escape_gate.is_counting_down = false;
    }
}

/**
 * 繪製逃生門與倒數提示。
 */
void render_escape_gate(ALLEGRO_FONT* font) {
    if (!escape_gate.is_active) return;

    float screen_x = escape_gate.x - camera_x;
    float screen_y = escape_gate.y - camera_y;


    // 計算門在螢幕上的位置
    float w = escape_gate.width;
    float h = escape_gate.height;

    al_draw_scaled_bitmap(
        gate_img,
        0, 0, al_get_bitmap_width(gate_img), al_get_bitmap_height(gate_img),
        screen_x, screen_y, w, h,
        0
    );

    // 只在 show_compass 為 true 時顯示羅盤
    if (show_compass) {
        float compass_margin = 40.0f;
        float compass_radius = 50.0f; // 羅盤半徑
        float center_x = SCREEN_WIDTH / 2.0f;
        float center_y = SCREEN_HEIGHT - compass_margin - compass_radius;

        // 算出逃生門相對於螢幕中心的方向角
        float gate_screen_x = escape_gate.x - camera_x + escape_gate.width / 2;
        float gate_screen_y = escape_gate.y - camera_y + escape_gate.height / 2;
        float screen_center_x = SCREEN_WIDTH / 2.0f;
        float screen_center_y = SCREEN_HEIGHT / 2.0f;
        float angle = atan2f(gate_screen_y - screen_center_y, gate_screen_x - screen_center_x);

        // 1) 畫羅盤外圈
        al_draw_circle(center_x, center_y, compass_radius, al_map_rgb(200, 180, 120), 3.0f);

        // 2) 畫羅盤刻度（N、E、S、W）
        const char* dirs[4] = { "N", "E", "S", "W" };
        for (int i = 0; i < 4; i++) {
            float a = ALLEGRO_PI/2 * i;  // 0, 90, 180, 270 度
            float x1 = center_x + cosf(a) * (compass_radius - 5);
            float y1 = center_y + sinf(a) * (compass_radius - 5);
            float x2 = center_x + cosf(a) * (compass_radius + 5);
            float y2 = center_y + sinf(a) * (compass_radius + 5);
            al_draw_line(x1, y1, x2, y2, al_map_rgb(200, 180, 120), 2.0f);
            al_draw_textf(font, al_map_rgb(200, 180, 120),
                center_x + cosf(a) * (compass_radius + 15),
                center_y + sinf(a) * (compass_radius + 15) - 8,
                ALLEGRO_ALIGN_CENTER, "%s", dirs[i]);
        }

        // 3) 畫指向逃生門的羅盤箭頭
        float arrow_len = compass_radius - 15;
        float tip_x = center_x + cosf(angle) * arrow_len;
        float tip_y = center_y + sinf(angle) * arrow_len;
        float shaft_w = 6.0f;
        float nx = -sinf(angle), ny = cosf(angle);

        // 箭桿四邊形
        float shaft_pts[4][2] = {
            { center_x + nx * (shaft_w/2), center_y + ny * (shaft_w/2) },
            { center_x - nx * (shaft_w/2), center_y - ny * (shaft_w/2) },
            { tip_x - cosf(angle)*10 - nx*(shaft_w/2), tip_y - sinf(angle)*10 - ny*(shaft_w/2) },
            { tip_x - cosf(angle)*10 + nx*(shaft_w/2), tip_y - sinf(angle)*10 + ny*(shaft_w/2) }
        };
        al_draw_filled_polygon((const float*)shaft_pts, 4, al_map_rgb(180, 30, 30));

        // 箭頭三角形
        float head_pts[3][2] = {
            { tip_x, tip_y },
            { tip_x - cosf(angle)*10 - nx*12, tip_y - sinf(angle)*10 - ny*12 },
            { tip_x - cosf(angle)*10 + nx*12, tip_y - sinf(angle)*10 + ny*12 }
        };
        al_draw_filled_triangle(
            head_pts[0][0], head_pts[0][1],
            head_pts[1][0], head_pts[1][1],
            head_pts[2][0], head_pts[2][1],
            al_map_rgb(180, 30, 30)
        );
    }

    if (escape_gate.is_counting_down) {
        int bar_width = 60;
        int bar_height = 10;
        int max_frames = 300;
        float ratio = (float)escape_gate.countdown_frames / max_frames;
        float filled_width = ratio * bar_width;

        int seconds_left = escape_gate.countdown_frames / 60;
        al_draw_textf(font, al_map_rgb(255, 0, 0),
            screen_x + escape_gate.width / 2,
            screen_y - 20,
            ALLEGRO_ALIGN_CENTER,
            "逃離中：%d", seconds_left);

         // 外框
        al_draw_rectangle(
            screen_x + 10,
            screen_y - 30,
            screen_x + 10 + bar_width,
            screen_y - 30 + bar_height,
            al_map_rgb(255, 255, 255),
            1.5f
        );

        // 內填充
        al_draw_filled_rectangle(
            screen_x + 10,
            screen_y - 30,
            screen_x + 10 + filled_width,
            screen_y - 30 + bar_height,
            al_map_rgb(255, 255, 0)
        );
    }
}
