#include "player_attack.h"
#include "asset_manager.h" // Added
#include "globals.h"   // For player, player_knife, bosses (camera_x, camera_y are params now)
#include "config.h"    // For various player and skill constants, KNIFE_SPRITE_WIDTH, KNIFE_SPRITE_HEIGHT
#include "projectile.h"// For spawn_projectile
#include "utils.h"     // For calculate_distance_between_points, get_knife_path_point
#include "effect.h"     // For pawn_warning_circle, spawn_claw_slash, spawn_floating_text
#include <stdio.h>     // For printf, stderr
#include <math.h>      // For cos, sin, fmin
#include <allegro5/allegro_primitives.h> // Added for drawing functions
#include <allegro5/allegro_image.h> // For al_get_bitmap_width/height

/**
 * 初始化玩家刀子攻擊的狀態。
 */
void init_player_knife() {
    player_knife.active = false;
    player_knife.path_progress_timer = 0.0f;
    for (int i = 0; i < MAX_BOSSES; ++i) {
        player_knife.hit_bosses_this_swing[i] = false;
    }
    player_knife.sprite = load_bitmap_once("assets/image/knife.png");
    if (!player_knife.sprite) {
        fprintf(stderr, "Failed to load knife sprite in init_player_knife!\n");
    }
}

void player_attack_render_knife(PlayerKnifeState* p_knife, float camera_x, float camera_y) {
    if (!p_knife->active || !p_knife->sprite) {
        return;
    }

    float knife_screen_x = p_knife->x - camera_x;
    float knife_screen_y = p_knife->y - camera_y;

    float src_w = al_get_bitmap_width(p_knife->sprite);
    float src_h = al_get_bitmap_height(p_knife->sprite);

    if (src_w > 0 && src_h > 0) {
        float scale_x = (float)KNIFE_SPRITE_WIDTH / src_w;
        float scale_y = (float)KNIFE_SPRITE_HEIGHT / src_h;

        al_draw_scaled_rotated_bitmap(p_knife->sprite,
                                      src_w / 2.0f, src_h / 2.0f,
                                      knife_screen_x, knife_screen_y,
                                      scale_x, scale_y,
                                      p_knife->angle,
                                      0);
    } else {
        // Fallback if sprite is invalid (e.g., dimensions are 0)
        // KNIFE_SPRITE_WIDTH and KNIFE_SPRITE_HEIGHT are from config.h
        al_draw_filled_rectangle(knife_screen_x - KNIFE_SPRITE_WIDTH / 4.0f, knife_screen_y - KNIFE_SPRITE_HEIGHT / 4.0f,
                                 knife_screen_x + KNIFE_SPRITE_WIDTH / 4.0f, knife_screen_y + KNIFE_SPRITE_HEIGHT / 4.0f,
                                 al_map_rgb(180, 180, 180));
    }
}

/**
 * 玩家執行普通攻擊 (現在是啟動刀子攻擊)。
 */
void player_perform_normal_attack() {
    if (player_knife.active || player.normal_attack_cooldown_timer > 0 || player.state!= STATE_NORMAL) {
        return;
    }
    player_knife.active = true;
    player_knife.path_progress_timer = 0.0f;
    player_knife.owner_start_x = player.x;
    player_knife.owner_start_y = player.y;
    player_knife.owner_start_facing_angle = player.facing_angle;
    for (int i = 0; i < MAX_BOSSES; ++i) {
        player_knife.hit_bosses_this_swing[i] = false;
    }
    player.normal_attack_cooldown_timer = PLAYER_NORMAL_ATTACK_COOLDOWN;
}
/**
 * 玩家使用完美防禦技能。
 */
int player_is_hit() {
    if(player.state == STATE_DEFENSE) {
        if(player.defense_timer>=0 && player.defense_timer < 10) {
            player_use_perfect_defense();
        } else {
            spawn_floating_text(player.x + 15, player.y - 25, "防禦", al_map_rgba(0, 0, 200, 250));
        }
        return 0;
    }
    return 1;
};

void player_use_perfect_defense() {
    if (player.learned_skills[SKILL_PREFECT_DEFENSE].cooldown_timers > 0) { 
        printf("Perfect defense is colling down (%ds)\n", player.learned_skills[SKILL_PREFECT_DEFENSE].cooldown_timers/FPS + 1);
        spawn_floating_text(player.x + 15, player.y - 25, "防禦", al_map_rgba(0, 0, 200, 250));
        return;
    }
    spawn_floating_text(player.x + 15, player.y - 25, "完美防禦", al_map_rgba(155, 0, 200, 250));
    player.learned_skills[SKILL_PREFECT_DEFENSE].duration_timers = 3 * FPS;
    player.learned_skills[SKILL_PREFECT_DEFENSE].cooldown_timers = 8 * FPS;
    printf("player triger perfect defense!\n");
}

/**
 * 玩家使用水彈攻擊技能。
 */
void player_use_water_attack() {
    if (player.learned_skills[SKILL_WATER_ATTACK].cooldown_timers > 0) { 
        printf("水彈攻擊冷卻中 (%ds)\n", player.learned_skills[SKILL_WATER_ATTACK].cooldown_timers/FPS + 1);
        return;
    }
    player.learned_skills[SKILL_WATER_ATTACK].cooldown_timers = PLAYER_WATER_SKILL_COOLDOWN; 
    float projectile_target_x = player.x + cos(player.facing_angle) * 1000.0f;
    float projectile_target_y = player.y + sin(player.facing_angle) * 1000.0f;
    spawn_projectile(player.x, player.y, projectile_target_x, projectile_target_y,
                      OWNER_PLAYER, PROJ_TYPE_WATER, PLAYER_WATER_PROJECTILE_DAMAGE + player.magic, 
                      PLAYER_WATER_PROJECTILE_SPEED, PLAYER_WATER_PROJECTILE_LIFESPAN, -1);
    printf("玩家施放水彈攻擊！\n");
}

/**
 * 玩家使用冰錐術技能。
 */
void player_use_ice_shard() {
    if (player.learned_skills[SKILL_ICE_SHARD].cooldown_timers > 0) { 
        printf("冰錐術冷卻中 (%ds)\n", player.learned_skills[SKILL_ICE_SHARD].cooldown_timers/FPS + 1);
        return;
    }
    player.learned_skills[SKILL_ICE_SHARD].cooldown_timers = PLAYER_ICE_SKILL_COOLDOWN; 
    float projectile_target_x = player.x + cos(player.facing_angle) * 1000.0f;
    float projectile_target_y = player.y + sin(player.facing_angle) * 1000.0f;
    spawn_projectile(player.x, player.y, projectile_target_x, projectile_target_y,
                      OWNER_PLAYER, PROJ_TYPE_ICE, PLAYER_ICE_PROJECTILE_DAMAGE + player.magic, 
                      PLAYER_ICE_PROJECTILE_SPEED, PLAYER_ICE_PROJECTILE_LIFESPAN, -1);
    printf("玩家施放冰錐術！\n");
}

/**
 * 玩家使用閃電鏈技能。
 */
void player_use_lightning_bolt() {
    if (player.learned_skills[SKILL_LIGHTNING_BOLT].cooldown_timers > 0) { 
        printf("閃電鏈冷卻中 (%ds)\n", player.learned_skills[SKILL_LIGHTNING_BOLT].cooldown_timers/FPS + 1);
        return;
    }
    player.learned_skills[SKILL_LIGHTNING_BOLT].cooldown_timers = PLAYER_LIGHTNING_SKILL_COOLDOWN; 
    bool hit_any = false; 
    for (int i = 0; i < MAX_BOSSES; ++i) {
        if (bosses[i].is_alive) { 
            float dist = calculate_distance_between_points(player.x, player.y, bosses[i].x, bosses[i].y); 
            if (dist < PLAYER_LIGHTNING_RANGE) { 
                int damage_dealt = PLAYER_LIGHTNING_DAMAGE + player.magic; 
                bosses[i].hp -= damage_dealt; 
                printf("閃電鏈命中 Boss %d (原型 %d)！ Boss HP: %d/%d\n", bosses[i].id, bosses[i].archetype, bosses[i].hp, bosses[i].max_hp);
                hit_any = true;
                if (bosses[i].hp <= 0) { 
                    bosses[i].is_alive = false; 
                    printf("Boss %d (原型 %d) 被閃電鏈擊敗！\n", bosses[i].id, bosses[i].archetype);
                }
            }
        }
    }
    if (!hit_any) printf("閃電鏈：範圍內沒有 Boss！\n");
}

/**
 * 玩家使用治療術技能。
 */
void player_use_heal() {
    if (player.learned_skills[SKILL_HEAL].cooldown_timers > 0) { 
        printf("治療術冷卻中 (%ds)\n", player.learned_skills[SKILL_HEAL].cooldown_timers/FPS + 1);
        return;
    }
    player.learned_skills[SKILL_HEAL].cooldown_timers = PLAYER_HEAL_SKILL_COOLDOWN; 
    int old_hp = player.hp;
    player.hp += PLAYER_HEAL_AMOUNT + player.magic * 2; 
    if (player.hp > player.max_hp) player.hp = player.max_hp; 
    printf("玩家治療了 %d 點生命！ (%d -> %d)\n", player.hp - old_hp, old_hp, player.hp);
}

/**
 * 玩家使用火球術技能。
 */
void player_use_fireball() {
    if (player.learned_skills[SKILL_FIREBALL].cooldown_timers > 0) { 
        printf("火球術冷卻中 (%ds)\n", player.learned_skills[SKILL_FIREBALL].cooldown_timers/FPS + 1);
        return;
    }
    player.learned_skills[SKILL_FIREBALL].cooldown_timers = PLAYER_FIREBALL_SKILL_COOLDOWN; 
    float projectile_target_x = player.x + cos(player.facing_angle) * 1000.0f;
    float projectile_target_y = player.y + sin(player.facing_angle) * 1000.0f;
    spawn_projectile(player.x, player.y, projectile_target_x, projectile_target_y,
                      OWNER_PLAYER, PROJ_TYPE_PLAYER_FIREBALL, PLAYER_FIREBALL_DAMAGE + player.magic, 
                      PLAYER_FIREBALL_SPEED, PLAYER_FIREBALL_LIFESPAN, -1);
    printf("玩家施放火球術！\n");
}

/**
 * 更新玩家刀子攻擊的狀態。
 */
void update_player_knife() {
    if (!player_knife.active) {
        return;
    }

    player_knife.path_progress_timer += 1.0f;

    if (player_knife.path_progress_timer >= KNIFE_ATTACK_DURATION) {
        player_knife.active = false;
        return;
    }

    float progress = player_knife.path_progress_timer / KNIFE_ATTACK_DURATION; 
    float local_x, local_y, local_path_angle;
    get_knife_path_point(progress, &local_x, &local_y, &local_path_angle);

    float cos_a = cos(player_knife.owner_start_facing_angle);
    float sin_a = sin(player_knife.owner_start_facing_angle);

    float world_offset_x = local_x * cos_a - local_y * sin_a;
    float world_offset_y = local_x * sin_a + local_y * cos_a;

    player_knife.x = player_knife.owner_start_x + world_offset_x;
    player_knife.y = player_knife.owner_start_y + world_offset_y;
    player_knife.angle = player_knife.owner_start_facing_angle + local_path_angle;

    float knife_collision_radius = fmin(KNIFE_SPRITE_WIDTH, KNIFE_SPRITE_HEIGHT) / 3.0f;

    for (int i = 0; i < MAX_BOSSES; ++i) {
        if (bosses[i].is_alive && !player_knife.hit_bosses_this_swing[i]) {
            float dist = calculate_distance_between_points(player_knife.x, player_knife.y, bosses[i].x, bosses[i].y);
            if (dist < bosses[i].collision_radius + knife_collision_radius) {
                int damage_dealt = KNIFE_DAMAGE_BASE + player.strength - bosses[i].defense;
                if (damage_dealt < 1) damage_dealt = 1;
                
                bosses[i].hp -= damage_dealt;
                player_knife.hit_bosses_this_swing[i] = true; 

                printf("刀子擊中 Boss %d (原型 %d)！造成 %d 傷害。Boss HP: %d/%d\n",
                       bosses[i].id, bosses[i].archetype, damage_dealt, bosses[i].hp, bosses[i].max_hp);

                if (bosses[i].hp <= 0) {
                    bosses[i].is_alive = false;
                    printf("Boss %d (原型 %d) 被刀子擊敗！\n", bosses[i].id, bosses[i].archetype);
                }
            }
        }
    }
}

void update_player_skill() {
    if(player.learned_skills[SKILL_PREFECT_DEFENSE].duration_timers > 0) {
        battle_speed_multiplier = 0.1f; // 時空減速的速度
        player.speed = player.max_speed/battle_speed_multiplier;
        // TODO: maybe some effets.
        spawn_warning_circle(player.x, player.y, 500, 
                            al_map_rgba(150, 0, 150, 5) , 2);
        if(player.learned_skills[SKILL_PREFECT_DEFENSE].duration_timers == 1) {
            battle_speed_multiplier = 1.0f;
            player.speed = player.max_speed;
        }
    }
}
