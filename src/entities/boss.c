#include "include/boss.h"
#include "include/globals.h"    // For bosses array, player, font (used in boss_render)
#include "include/config.h"     // For boss related constants, FPS, SCREEN_WIDTH, SCREEN_HEIGHT etc.
#include "include/projectile.h" // For spawn_projectile
#include "include/utils.h"      // For calculate_distance_between_points
#include "include/boss_attack.h"  // For boss_evaluate_and_execute_action
#include "include/asset_manager.h" // For load_bitmap_once
#include <stdio.h>      // For printf, fprintf, stderr
#include <stdlib.h>     // For rand
#include <math.h>       // For M_PI, atan2, cos, sin
#include <allegro5/allegro_primitives.h> // For al_draw_filled_circle, al_draw_scaled_rotated_bitmap
#include <allegro5/allegro_image.h> // For al_get_bitmap_width/height (though often included with allegro.h or primitives.h)

/**
 * 設定 Boss 的屬性、圖像資源和特定行為參數。
 */
void configure_boss_stats_and_assets(Boss* b, BossArchetype archetype, int difficulty_tier, int boss_id_for_cooldown_randomness) {
    b->max_hp = 60 + difficulty_tier * 30;
    b->strength = 10 + difficulty_tier * 4;
    b->speed = 1.8f + difficulty_tier * 0.05f;
    b->defense = 7 + difficulty_tier * 2;
    b->magic = 10 + difficulty_tier * 2;
    b->ranged_special_projectile_type = PROJ_TYPE_FIRE;

    switch (archetype) {
        case BOSS_TYPE_TANK:
            b->max_hp = (int)(b->max_hp * 2.5f);
            b->strength = (int)(b->strength * 1.0f);
            b->speed *= 0.75f;
            b->defense += 12 + difficulty_tier * 4;
            b->sprite_asset = load_bitmap_once("assets/image/boss1.png");
            if (!b->sprite_asset) { fprintf(stderr, "Failed to load sprite for boss type TANK\n"); }
            b->target_display_width = BOSS1_TARGET_WIDTH * 1.20f;
            b->target_display_height = BOSS1_TARGET_HEIGHT * 1.20f;
            if (!b->sprite_asset) { b->target_display_width = 70; b->target_display_height = 70;} // Keep fallback size if sprite fails
            b->ranged_special_ability_cooldown_timer = (rand() % (FPS * 2)) + BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN + (FPS * (boss_id_for_cooldown_randomness % 2));
            b->melee_primary_ability_cooldown_timer = (rand() % FPS) + BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN;
            break;
        case BOSS_TYPE_SKILLFUL:
            b->max_hp = (int)(b->max_hp * 1.1f);
            b->strength = (int)(b->strength * 0.75f);
            b->speed *= 1.1f;
            b->defense = (b->defense - 3 > 1) ? b->defense - 3 : 1;
            b->magic = (int)(b->magic * 1.5f);
            b->sprite_asset = load_bitmap_once("assets/image/boss2.png");
            if (!b->sprite_asset) { fprintf(stderr, "Failed to load sprite for boss type SKILLFUL\n"); }
            b->target_display_width = BOSS2_TARGET_WIDTH;
            b->target_display_height = BOSS2_TARGET_HEIGHT;
            if (!b->sprite_asset) { b->target_display_width = 40; b->target_display_height = 40;} // Keep fallback size if sprite fails
            b->ranged_special_ability_cooldown_timer = (rand() % (FPS / 2)) + (int)(BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN * 0.35f) + (FPS/2 * (boss_id_for_cooldown_randomness % 2));
            b->melee_primary_ability_cooldown_timer = (rand() % FPS) + (int)(BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN * 1.35f);
            break;
        case BOSS_TYPE_BERSERKER:
            b->max_hp = (int)(b->max_hp * 1.5f);
            b->strength = (int)(b->strength * 2.2f);
            b->speed *= 0.50f;
            b->defense += 1 + difficulty_tier;
            b->sprite_asset = load_bitmap_once("assets/image/boss3.png");
            if (!b->sprite_asset) { fprintf(stderr, "Failed to load sprite for boss type BERSERKER\n"); }
            // Simplified target_display_width/height based on instructions.
            if (b->sprite_asset) {
                b->target_display_width = BOSS3_TARGET_WIDTH;
                b->target_display_height = BOSS3_TARGET_HEIGHT;
            } else {
                b->target_display_width = 60; // Default fallback size
                b->target_display_height = 60; // Default fallback size
            }
            b->ranged_special_ability_cooldown_timer = (rand() % (FPS * 2)) + (int)(BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN * 1.5f) + (FPS * (boss_id_for_cooldown_randomness % 2));
            b->melee_primary_ability_cooldown_timer = (rand() % (FPS / 2)) + (int)(BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN * 0.55f);
            break;
    }
    b->hp = b->max_hp;
    b->collision_radius = b->target_display_width / 2.0f;
}

/**
 * 根據預設的原型數量初始化所有 Boss。
 */
void init_bosses_by_archetype() {
    const int num_tanks = 3;
    const int num_skillful = 1;
    const int num_berserkers = 2;
    int current_boss_idx = 0;
    float initial_x_pos = 200.0f;
    float x_spacing = 170.0f;
    float initial_y_pos = 150.0f;
    float y_spacing_per_row = 120.0f;
    int bosses_per_row = 3;

    for (int i = 0; i < num_tanks; ++i) {
        if (current_boss_idx >= MAX_BOSSES) break;
        Boss* b = &bosses[current_boss_idx];
        b->id = current_boss_idx;
        b->archetype = BOSS_TYPE_TANK;
        int difficulty_tier = current_boss_idx / bosses_per_row;
        configure_boss_stats_and_assets(b, BOSS_TYPE_TANK, difficulty_tier, b->id);
        b->x = initial_x_pos + (current_boss_idx % bosses_per_row) * x_spacing;
        b->y = initial_y_pos + (current_boss_idx / bosses_per_row) * y_spacing_per_row;
        b->v_x = 0.0f; b->v_y = 0.0f; b->facing_angle = M_PI / 2.0; b->is_alive = true;
        b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY;
        current_boss_idx++;
    }

    for (int i = 0; i < num_skillful; ++i) {
        if (current_boss_idx >= MAX_BOSSES) break;
        Boss* b = &bosses[current_boss_idx];
        b->id = current_boss_idx;
        b->archetype = BOSS_TYPE_SKILLFUL;
        int difficulty_tier = current_boss_idx / bosses_per_row;
        configure_boss_stats_and_assets(b, BOSS_TYPE_SKILLFUL, difficulty_tier, b->id);
        b->x = initial_x_pos + (current_boss_idx % bosses_per_row) * x_spacing;
        b->y = initial_y_pos + (current_boss_idx / bosses_per_row) * y_spacing_per_row;
        b->v_x = 0.0f; b->v_y = 0.0f; b->facing_angle = M_PI / 2.0; b->is_alive = true;
        b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY;
        current_boss_idx++;
    }

    for (int i = 0; i < num_berserkers; ++i) {
        if (current_boss_idx >= MAX_BOSSES) break;
        Boss* b = &bosses[current_boss_idx];
        b->id = current_boss_idx;
        b->archetype = BOSS_TYPE_BERSERKER;
        int difficulty_tier = current_boss_idx / bosses_per_row;
        configure_boss_stats_and_assets(b, BOSS_TYPE_BERSERKER, difficulty_tier, b->id);
        b->x = initial_x_pos + (current_boss_idx % bosses_per_row) * x_spacing;
        b->y = initial_y_pos + (current_boss_idx / bosses_per_row) * y_spacing_per_row;
        b->v_x = 0.0f; b->v_y = 0.0f; b->facing_angle = M_PI / 2.0; b->is_alive = true;
        b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY;
        current_boss_idx++;
    }
    
    for (int i = current_boss_idx; i < MAX_BOSSES; ++i) {
        bosses[i].is_alive = false;
    }
}



/**
 * 更新特定 Boss 角色的狀態。
 */
void update_boss_character(Boss* b) {
    if (!b->is_alive) { 
        b->v_x = 0; b->v_y = 0; 
        return;
    }

    if (b->ranged_special_ability_cooldown_timer > 0) b->ranged_special_ability_cooldown_timer--;
    if (b->melee_primary_ability_cooldown_timer > 0) b->melee_primary_ability_cooldown_timer--;

    float dist_to_player = calculate_distance_between_points(b->x, b->y, player.x, player.y); 
    b->v_x = 0; b->v_y = 0; 

    float engagement_distance_factor = 0.75f; 
    float base_engagement_range = BOSS_MELEE_PRIMARY_BASE_RANGE * (b->archetype == BOSS_TYPE_BERSERKER ? 1.1f : 1.0f); 
    float desired_distance = base_engagement_range * engagement_distance_factor;
                                   
    if (dist_to_player > 800 && b->archetype != BOSS_TYPE_SKILLFUL) { 
        // Stay put
    } else if (dist_to_player > desired_distance + b->collision_radius + PLAYER_SPRITE_SIZE) { 
        float angle_to_player = atan2(player.y - b->y, player.x - b->x); 
        b->facing_angle = angle_to_player; 
        b->v_x = cos(angle_to_player) * b->speed; 
        b->v_y = sin(angle_to_player) * b->speed; 
    } else if (b->archetype == BOSS_TYPE_SKILLFUL && dist_to_player < desired_distance * 0.8f && dist_to_player > 100) { 
        // Skillful boss tries to maintain some distance
        float angle_to_player = atan2(player.y - b->y, player.x - b->x); 
        b->facing_angle = angle_to_player; 
        b->v_x = -cos(angle_to_player) * b->speed * 0.6f; 
        b->v_y = -sin(angle_to_player) * b->speed * 0.6f;
    }

    b->x += b->v_x; 
    b->y += b->v_y; 

    boss_evaluate_and_execute_action(b); 

}

void boss_render(Boss* b, float camera_x, float camera_y) {
    // This function is called only if b->is_alive is true, so no need to check here.

    float boss_screen_x = b->x - camera_x;
    float boss_screen_y = b->y - camera_y;

    if (b->sprite_asset) {
        float src_w = al_get_bitmap_width(b->sprite_asset);
        float src_h = al_get_bitmap_height(b->sprite_asset);
        if (src_w > 0 && src_h > 0) {
            float scale_x = b->target_display_width / src_w;
            float scale_y = b->target_display_height / src_h;
            al_draw_scaled_rotated_bitmap(b->sprite_asset,
                                        src_w / 2.0f, src_h / 2.0f,
                                        boss_screen_x, boss_screen_y,
                                        scale_x, scale_y,
                                        b->facing_angle,
                                        0);
        } else { // Fallback for invalid sprite dimensions
            al_draw_filled_circle(boss_screen_x, boss_screen_y, b->collision_radius, al_map_rgb(255, 0, 255)); // Magenta circle
        }
    } else { // Fallback if sprite_asset is NULL
        ALLEGRO_COLOR boss_fallback_color;
        switch (b->archetype) {
            case BOSS_TYPE_TANK: boss_fallback_color = al_map_rgb(100, 100, 220); break;
            case BOSS_TYPE_SKILLFUL: boss_fallback_color = al_map_rgb(220, 100, 220); break;
            case BOSS_TYPE_BERSERKER: boss_fallback_color = al_map_rgb(220, 60, 60); break;
            default: boss_fallback_color = al_map_rgb(180, 180, 180); break;
        }
        al_draw_filled_circle(boss_screen_x, boss_screen_y, b->collision_radius, boss_fallback_color);
    }

    // Text rendering for boss stats (uses global `font`)
    // Ensure `font` is accessible (e.g., via "globals.h")
    // `b->target_display_height` is part of the Boss struct.
    float text_y_offset = b->target_display_height / 2.0f + 7;
    const char* archetype_str_label = "";
    switch (b->archetype) {
        case BOSS_TYPE_TANK: archetype_str_label = "坦克"; break;
        case BOSS_TYPE_SKILLFUL: archetype_str_label = "法師"; break;
        case BOSS_TYPE_BERSERKER: archetype_str_label = "猛獸"; break;
    }
    if (font) { // Check if font is loaded
        al_draw_textf(font, al_map_rgb(255, 255, 255), boss_screen_x, boss_screen_y - text_y_offset - 40, ALLEGRO_ALIGN_CENTER, "ID:%d %s", b->id, archetype_str_label);
        al_draw_textf(font, al_map_rgb(255, 255, 255), boss_screen_x, boss_screen_y - text_y_offset - 20, ALLEGRO_ALIGN_CENTER, "HP: %d/%d", b->hp, b->max_hp);
        al_draw_textf(font, al_map_rgb(200, 200, 200), boss_screen_x, boss_screen_y - text_y_offset, ALLEGRO_ALIGN_CENTER, "力:%d 防:%d 魔:%d 速:%.1f", b->strength, b->defense, b->magic, b->speed);
    }
}