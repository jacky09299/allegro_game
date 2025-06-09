#include "boss_attack.h"
#include "globals.h"    // For player, bosses array
#include "config.h"     // For boss related constants
#include "projectile.h" // For spawn_projectile
#include "utils.h"      // For calculate_distance_between_points
#include "effect.h"     
#include <stdio.h>      // For printf
#include <stdlib.h>     // For rand
#include <math.h>       // For M_PI, atan2, cos, sin (though M_PI, atan2, cos, sin might not be strictly needed if only action evaluation is here)

#include <allegro5/allegro_primitives.h> // 用於 al_draw_filled_circle, al_draw_line, al_clear_to_color 等
#include <allegro5/allegro_color.h> // 用於 al_map_rgb

// 生成警示圈
// WarningCircle 表示技能即將發動的預警圈，alpha 代表透明度（0 完全透明，255 完全不透明）
void spawn_warning_circle(float x, float y, float radius, ALLEGRO_COLOR color, int duration) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i].type = EFFECT_WARNING_CIRCLE;
            effects[i].x = x;
            effects[i].y = y;
            effects[i].radius = radius;
            effects[i].color = color;
            effects[i].timer = duration;
            effects[i].active = true;
            break;
        }
    }
}
// 生成爪擊特效
void spawn_claw_slash(float x, float y, float angle) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i].type = EFFECT_CLAW_SLASH;
            effects[i].x = x;
            effects[i].y = y;
            effects[i].angle = angle;
            effects[i].timer = 0.3 * 60; // 約 18 幀
            effects[i].active = true;
            break;
        }
    }
}
// 生成漂浮文字
void spawn_floating_text(float x, float y, const char* content, ALLEGRO_COLOR color) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i].type = EFFECT_FLOATING_TEXT;
            effects[i].x = x;
            effects[i].y = y;
            effects[i].vx = 0;
            effects[i].vy = -0.4f;
            effects[i].timer = 2 * FPS;
            effects[i].color = color;
            strncpy(effects[i].text, content, sizeof(effects[i].text));
            effects[i].active = true;
            break;
        }
    }
}
// 生成curse link
void spawn_curse_link(float x, float y, int duration) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i].type = EFFECT_CURSE_LINK;
            effects[i].x = x;
            effects[i].y = y;
            effects[i].timer = duration; // 約 18 幀
            effects[i].active = true;
            break;
        }
    }
}

void update_effects() {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) continue;

        effects[i].x += effects[i].vx;
        effects[i].y += effects[i].vy;
        effects[i].timer-= battle_speed_multiplier;
        if (effects[i].timer <= 0) effects[i].active = false;
    }
}

// 繪製 boss 連結效果
// not finished yet
void draw_curse_link(Effect *e) {
    float pulse = sin(battle_time * 10.0f);  // 10Hz 閃爍
    int alpha = 120 + pulse * 60;            // 在 60~180 間變動

    float dx = e->x - player.x;
    float dy = e->y - player.y;
    float angle = atan2f(dy, dx);
    float wave = sin(battle_time * 15.0f) * 3.0f; // 震動幅度

    // 起點與終點（加上抖動）
    float px = player.x + cosf(angle + ALLEGRO_PI / 2) * wave;
    float py = player.y + sinf(angle + ALLEGRO_PI / 2) * wave;
    float bx = e->x + cosf(angle - ALLEGRO_PI / 2) * wave;
    float by = e->y + sinf(angle - ALLEGRO_PI / 2) * wave;

    // 投影到螢幕
    float pxs = px - camera_x, pys = py - camera_y;
    float bxs = bx - camera_x, bys = by - camera_y;

    // 畫線
    al_draw_line(pxs, pys, bxs, bys,
        al_map_rgba(180, 0, 255, alpha), 3.5f);
}

// 繪製Boss 爪擊效果
void draw_claw(Effect *e) {
    float offset[3] = { -0.2f, 0.0f, 0.2f };
    float length[3] = {35, 40, 35};
    float claw_screen_x = e->x - camera_x;
    float claw_screen_y = e->y - camera_y;

    // 計算爪擊方向向量（沿著攻擊角度）
    float dir_x = cosf(e->angle);
    float dir_y = sinf(e->angle);

    // 垂直向量（法線方向），用來左右平移
    float perp_x = -dir_y;
    float perp_y = dir_x;

    float ratio = (e->timer / (0.3 * FPS));

    for (int i = 0; i < 3; ++i) {
        float offset_scale = offset[i] * length[i];

        // 爪痕起點（中心點 + 偏移）
        float px = claw_screen_x + perp_x * offset_scale;
        float py = claw_screen_y + perp_y * offset_scale;

        float end_x = px + dir_x * length[i];
        float end_y = py + dir_y * length[i];

        al_draw_line(px, py, end_x, end_y,
                    al_map_rgba(255, 0, 0, 200 * ratio), 4.0f);
    }

}

// 殘影效果
void spawn_element_dash_trail(float x, float y, ALLEGRO_COLOR color) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i] = (Effect){
                .type = EFFECT_DASH_TRAIL,
                .x = x, .y = y,
                .timer = 15,
                .color = color,
                .active = true
            };
            break;
        }
    }
}

void draw_element_dash_trail(Effect* e) {
    float alpha = (float)e->timer / 15;
    ALLEGRO_COLOR faded = al_map_rgba_f(e->color.r, e->color.g, e->color.b, alpha);
    al_draw_filled_circle(e->x - camera_x, e->y - camera_y, 12, faded);
}

// 蓄力環效果
void spawn_charge_beam_charge(float x, float y) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i] = (Effect){
                .type = EFFECT_CHARGE_BEAM_RING,
                .x = x, .y = y,
                .radius = 0,
                .timer = 30,
                .active = true
            };
            break;
        }
    }
}

void draw_charge_beam_ring(Effect* e) {
    float progress = 1.0f - (float)e->timer / 30.0f;
    float radius = 20 + 20 * progress;
    al_draw_circle(e->x - camera_x, e->y - camera_y, radius, al_map_rgba(100, 200, 255, 180), 2.0f);
}

void spawn_focus_aura(float x, float y) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i] = (Effect){
                .type = EFFECT_FOCUS_AURA,
                .x = x, .y = y,
                .timer = 60,
                .active = true
            };
            break;
        }
    }
}

void draw_focus_aura(Effect* e) {
    // float t = (60 - e->timer) / 60.0f;
    float angle = battle_time * 6;
    for (int i = 0; i < 6; ++i) {
        float a = angle + i * ALLEGRO_PI / 3;
        float dx = cosf(a) * 40, dy = sinf(a) * 40;
        al_draw_filled_circle(e->x + dx - camera_x, e->y + dy - camera_y, 40, al_map_rgba(100, 255, 100, 150));
    }
}

void spawn_orb_summon_glow(float x, float y) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i] = (Effect){
                .type = EFFECT_ORB_SUMMON_GLOW,
                .x = x, .y = y,
                .timer = 25,
                .active = true
            };
            break;
        }
    }
}

void draw_orb_summon_glow(Effect* e) {
    float r = 30 + sin(battle_time * 10) * 5;
    al_draw_circle(e->x - camera_x, e->y - camera_y, r, al_map_rgba(100, 100, 255, 180), 2);
}

void spawn_rune_circle(float x, float y, int radius, ALLEGRO_COLOR color) {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) {
            effects[i] = (Effect){
                .type = EFFECT_RUNE_CIRCLE,
                .x = x, .y = y,
                .timer = 60,
                .active = true,
                .radius = radius,
                .color = color
            };
            break;
        }
    }
}

void draw_rune_circle(Effect* e) {
    float progress = 1.0f - (float)e->timer / 60.0f;
    float radius = e->radius + 10 * sin(progress * ALLEGRO_PI);
    al_draw_circle(e->x - camera_x, e->y - camera_y, radius, e->color, 2);

    // 五芒星
    for (int i = 0; i < 5; ++i) {
        float angle1 = ALLEGRO_PI * 2 * i / 5;
        float angle2 = ALLEGRO_PI * 2 * ((i + 2) % 5) / 5;
        float x1 = e->x + cos(angle1) * radius;
        float y1 = e->y + sin(angle1) * radius;
        float x2 = e->x + cos(angle2) * radius;
        float y2 = e->y + sin(angle2) * radius;
        al_draw_line(x1 - camera_x, y1 - camera_y, x2 - camera_x, y2 - camera_y, e->color, 1.5f);
    }
}

void render_effects_back() {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) continue;
        Effect* e = &effects[i];

        switch (e->type) {
            case EFFECT_FLOATING_TEXT:
                al_draw_text(font, e->color, e->x - camera_x, e->y - camera_y, ALLEGRO_ALIGN_CENTER, e->text);
                break;
            case EFFECT_WARNING_CIRCLE:
                al_draw_filled_circle(e->x - camera_x, e->y - camera_y, e->radius, e->color);
                break;
            case EFFECT_DASH_TRAIL:
                draw_element_dash_trail(e);
                break;
            case EFFECT_RUNE_CIRCLE:
                draw_rune_circle(e);
                break;
            case EFFECT_CHARGE_BEAM_RING:
                draw_charge_beam_ring(e);
                break;
            default: break;
        }
    }
}

void render_effects_front() {
    for (int i = 0; i < MAX_EFFECTS; ++i) {
        if (!effects[i].active) continue;
        Effect* e = &effects[i];

        switch (e->type) {
            case EFFECT_CLAW_SLASH:
                // 可抽成獨立函式畫三線
                draw_claw(e);
                break;
            case EFFECT_CURSE_LINK:
                draw_curse_link(e);
                break;
            case EFFECT_ORB_SUMMON_GLOW:
                draw_orb_summon_glow(e);
                break;
            default: break;
        }
    }
}