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
#include <allegro5/allegro.h> // For ALLEGRO_MOUSE_STATE

ALLEGRO_MOUSE_STATE mouse_state;

float get_angle_to_mouse(float x, float y) {
    // float target_x = camera_x + SCREEN_WIDTH / 2.0f;  // 玩家中心的世界座標
    // float target_y = camera_y + SCREEN_HEIGHT / 2.0f;

    int mouse_x, mouse_y;
    al_get_mouse_state(&mouse_state);
    mouse_x = mouse_state.x;
    mouse_y = mouse_state.y;

    float world_mouse_x = camera_x + mouse_x;
    float world_mouse_y = camera_y + mouse_y;

    return atan2(world_mouse_y - y, world_mouse_x - x);
}
void get_clamped_target_position(float player_x, float player_y, float max_range, float* out_x, float* out_y) {
    ALLEGRO_MOUSE_STATE mouse;
    al_get_mouse_state(&mouse);

    float mouse_screen_x = mouse.x;
    float mouse_screen_y = mouse.y;

    // 假設玩家在畫面中央，將滑鼠座標轉成地圖座標
    float target_x = player_x + (mouse_screen_x - SCREEN_WIDTH / 2.0f);
    float target_y = player_y + (mouse_screen_y - SCREEN_HEIGHT / 2.0f);

    // 計算距離與方向
    float dx = target_x - player_x;
    float dy = target_y - player_y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist > max_range) {
        // 超出範圍：拉回最遠距離的邊界點
        float scale = max_range / dist;
        dx *= scale;
        dy *= scale;
    }

    *out_x = player_x + dx;
    *out_y = player_y + dy;
}
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
    if(player.state == STATE_CHARGING) {
        spawn_floating_text(player.x + 15, player.y - 25, "中斷施法", al_map_rgba(0, 150, 200, 250));
        // TODO: 中斷
        player.state = STATE_NORMAL;
        switch(player.current_skill_index) {
            case SKILL_FOCUS:
                player.learned_skills[SKILL_FOCUS].charge_timers = 0;
            case SKILL_CHARGE_BEAM:
                player.learned_skills[SKILL_CHARGE_BEAM].charge_timers = 0;
        }
    }
    if(player.state == STATE_DEFENSE) {
        if(player.mp < player.max_mp - 10) player.mp += 10; // 回復魔力
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
 * 玩家使用元素疾步技能。
 */
void player_use_element_dash() {
    if (player.learned_skills[SKILL_ELEMENT_DASH].cooldown_timers > 0) return;
    if(player.mp < 30) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
        return;
    }
    player.mp -= 30;

    player.learned_skills[SKILL_ELEMENT_DASH].duration_timers = 1 * FPS;
    player.learned_skills[SKILL_ELEMENT_DASH].cooldown_timers = 12 * FPS;
}
void using_element_dash() {
    player.speed = player.max_speed * 5.0f;

    spawn_projectile(50, player.x, player.y, player.x, player.y, OWNER_PLAYER, PROJ_TYPE_DASH, 2, 2, 10, -1);
    // spawn_element_dash_trail(player.x, player.y,al_map_rgba(0, 50, 50, 150));
    // reset
    if(player.learned_skills[SKILL_ELEMENT_DASH].duration_timers == 1){
        player.speed = player.max_speed;
        if(player.state != STATE_STUN) player.state = STATE_NORMAL;
    }
}
/**
 * 玩家使用蓄力元素槍技能。
 */
void player_start_charge_beam() {
    if(player.learned_skills[SKILL_CHARGE_BEAM].cooldown_timers > 0) return;
    // spawn_charge_beam_charge(player.x, player.y);
    player.state = STATE_CHARGING;
    player.learned_skills[SKILL_CHARGE_BEAM].charge_timers = 0; // charge time
}

void player_release_charge_beam() {
    if(player.learned_skills[SKILL_CHARGE_BEAM].charge_timers > 300) player.learned_skills[SKILL_CHARGE_BEAM].charge_timers = 300;
    int charge_level = player.learned_skills[SKILL_CHARGE_BEAM].charge_timers;
    float power = 10 + charge_level/5;
    spawn_projectile(charge_level/5, player.x, player.y, player.x + cos(player.facing_angle)*1000, player.y + sin(player.facing_angle)*1000,
                     OWNER_PLAYER, PROJ_TYPE_PLAYER_FIREBALL, power, 15 + charge_level/30, 60 + charge_level, -1);
    player.state = STATE_NORMAL;
    player.learned_skills[SKILL_CHARGE_BEAM].cooldown_timers = 6 * FPS;
}

void player_end_charge_beam() {
    if(player.learned_skills[SKILL_CHARGE_BEAM].charge_timers == 0) return;
    if(player.state != STATE_STUN) player.state = STATE_NORMAL;
    
    player_release_charge_beam();
    player.learned_skills[SKILL_CHARGE_BEAM].charge_timers = 0; // reset tcharge time
    player.learned_skills[SKILL_CHARGE_BEAM].cooldown_timers = 5 * FPS;
}
/**
 * 玩家使用元素集中技能。
 */
void player_start_focus() {
    if(player.learned_skills[SKILL_CHARGE_BEAM].cooldown_timers > 0) return;

    player.state = STATE_CHARGING;
    player.learned_skills[SKILL_FOCUS].charge_timers = 0; // charge time
}

void player_end_focus() {
    if(player.state != STATE_STUN) player.state = STATE_NORMAL;
    player.learned_skills[SKILL_FOCUS].variable_1 = player.magic * (float)player.learned_skills[SKILL_FOCUS].charge_timers/300;
    player.learned_skills[SKILL_FOCUS].duration_timers = player.learned_skills[SKILL_FOCUS].charge_timers;
    player.learned_skills[SKILL_FOCUS].charge_timers = 0; // reset tcharge time
    player.learned_skills[SKILL_FOCUS].cooldown_timers = 5 * FPS;
}
void player_using_focus() {
    if(player.learned_skills[SKILL_FOCUS].duration_timers > 200 && battle_speed_multiplier > 0.7f) {
        battle_speed_multiplier = 0.7f; // 時空減速的速度
        player.speed = player.max_speed/battle_speed_multiplier;
    }
    if(player.learned_skills[SKILL_FOCUS].duration_timers == 200) {
        battle_speed_multiplier = 1.0f;
        player.speed = player.max_speed;
    }
    // 加速冷卻
    for (int i = 0; i < MAX_PLAYER_SKILLS; ++i) {
        if (player.learned_skills[i].cooldown_timers > 0) {
            player.learned_skills[i].cooldown_timers -= battle_speed_multiplier;
        }
    }
    // TODO: maybe some effects;
    spawn_focus_aura(player.x, player.y);
    
    if(player.learned_skills[SKILL_FOCUS].duration_timers == 1) {
        player.learned_skills[SKILL_FOCUS].variable_1 = 0;
    }
}

/**
 * 玩家使用水晶球技能。
 */
void player_use_arcane_orb() {
    if (player.learned_skills[SKILL_ARCANE_ORB].cooldown_timers > 0) return;
    if(player.mp < 80) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
        return;
    }
    player.mp -= 80;
    int range = 100;
    get_clamped_target_position(player.x, player.y, range,
                                &player.learned_skills[SKILL_ARCANE_ORB].x, &player.learned_skills[SKILL_ARCANE_ORB].y);
    
    player.learned_skills[SKILL_ARCANE_ORB].duration_timers = 1 * FPS; // 間隔 5 秒
    player.learned_skills[SKILL_ARCANE_ORB].variable_1 = 8;

    player.learned_skills[SKILL_ARCANE_ORB].cooldown_timers = player.learned_skills[SKILL_RUNE_IMPLOSION].duration_timers * player.learned_skills[SKILL_ARCANE_ORB].variable_1;
    player.learned_skills[SKILL_ARCANE_ORB].cooldown_timers += 5 * FPS;
}
void arcane_orb_normal_attack() {
    float x = player.learned_skills[SKILL_ARCANE_ORB].x;
    float y = player.learned_skills[SKILL_ARCANE_ORB].y;
    float angle = get_angle_to_mouse(x,y);

    float projectile_target_x = x + cos(angle) * 1000.0f;
    float projectile_target_y = y + sin(angle) * 1000.0f;

    spawn_projectile(-1, x, y, projectile_target_x, projectile_target_y,
                        OWNER_PLAYER, PROJ_TYPE_WATER, 5 + player.magic/2, 
                        PLAYER_WATER_PROJECTILE_SPEED, PLAYER_WATER_PROJECTILE_LIFESPAN, -1);
}
void arcane_orb_skill_attack() {
    float x = player.learned_skills[SKILL_ARCANE_ORB].x;
    float y = player.learned_skills[SKILL_ARCANE_ORB].y;
    float angle[8] = {atan2(1,0),atan2(1,1),atan2(0,1),atan2(-1,1),
                    atan2(-1,0),atan2(-1,-1),atan2(0,-1),atan2(1,-1)};

    for(int i=0; i<8; i++) {
        float projectile_target_x = x + cos(angle[i]) * 1000.0f;
        float projectile_target_y = y + sin(angle[i]) * 1000.0f;

        spawn_projectile(-1, x, y, projectile_target_x, projectile_target_y,
                            OWNER_PLAYER, PROJ_TYPE_ICE, 5 + player.magic/4, 
                            PLAYER_ICE_PROJECTILE_SPEED, PLAYER_ICE_PROJECTILE_LIFESPAN, -1);
    }
}
void using_arcane_orb() {
    float x = player.learned_skills[SKILL_ARCANE_ORB].x;
    float y = player.learned_skills[SKILL_ARCANE_ORB].y;
    spawn_warning_circle(x, y, 25, al_map_rgb(0, 0, 250), 2);
    spawn_orb_summon_glow(x, y);

    if( player.learned_skills[SKILL_ARCANE_ORB].duration_timers == 1
    && player.learned_skills[SKILL_ARCANE_ORB].variable_1 > 0) 
    {
        if(player.learned_skills[SKILL_ARCANE_ORB].variable_1%2 == 0) {
            arcane_orb_normal_attack();
            printf("arcane orb normal attack\n");
        }
        if(player.learned_skills[SKILL_ARCANE_ORB].variable_1%2 == 1) {
            arcane_orb_skill_attack();
            printf("arcane orb skill attack\n");
        }
        player.learned_skills[SKILL_ARCANE_ORB].variable_1 --;
        player.learned_skills[SKILL_ARCANE_ORB].duration_timers = 1* FPS;
    }  
}
/**
 * 玩家使用符文爆破技能。
 */
void player_use_rune_implosion() {
    if (player.learned_skills[SKILL_RUNE_IMPLOSION].cooldown_timers > 0) return;
    if(player.mp < 20) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
        return;
    }
    int range = 300;
    player.mp -= 20;
    get_clamped_target_position(player.x, player.y, range,
                                &player.learned_skills[SKILL_RUNE_IMPLOSION].x, &player.learned_skills[SKILL_RUNE_IMPLOSION].y);

    
    player.learned_skills[SKILL_RUNE_IMPLOSION].duration_timers = 3 * FPS; //  三秒後引爆
    player.learned_skills[SKILL_RUNE_IMPLOSION].cooldown_timers = 5 * FPS;
}
void release_rune_implosion() {
    int r = 50;
    int x = player.learned_skills[SKILL_RUNE_IMPLOSION].x;
    int y = player.learned_skills[SKILL_RUNE_IMPLOSION].y;
    int power = 30 + player.magic * 2;

    spawn_projectile(r, x, y, x, y,
                     OWNER_PLAYER, PROJ_TYPE_PLAYER_FIREBALL, power, 0, 5, -1);
    spawn_warning_circle(x, y, r, al_map_rgb(1, 1, 1), 2 * FPS);
    // TODO: 一些特效
}
void using_rune_implosion() {
    int r = 50;
    int x = player.learned_skills[SKILL_RUNE_IMPLOSION].x;
    int y = player.learned_skills[SKILL_RUNE_IMPLOSION].y;
    spawn_warning_circle(x, y, r, al_map_rgba(50, 50, 50, 50), 2);
    spawn_rune_circle(x, y, r, al_map_rgba(200, 100, 0, 200));
    if(player.learned_skills[SKILL_RUNE_IMPLOSION].duration_timers == 1) release_rune_implosion();
}
/**
 * 玩家使用水彈攻擊技能。
 */
void player_use_water_attack() {
    float projectile_target_x = player.x + cos(player.facing_angle) * 1000.0f;
    float projectile_target_y = player.y + sin(player.facing_angle) * 1000.0f;
    spawn_projectile(-1, player.x, player.y, projectile_target_x, projectile_target_y,
                      OWNER_PLAYER, PROJ_TYPE_WATER, PLAYER_WATER_PROJECTILE_DAMAGE + player.magic, 
                      PLAYER_WATER_PROJECTILE_SPEED, PLAYER_WATER_PROJECTILE_LIFESPAN, -1);
    printf("玩家施放水彈攻擊！\n");
}

/**
 * 玩家使用冰錐術技能。
 */
void player_use_ice_shard() {
    float projectile_target_x = player.x + cos(player.facing_angle) * 1000.0f;
    float projectile_target_y = player.y + sin(player.facing_angle) * 1000.0f;
    spawn_projectile(-1, player.x, player.y, projectile_target_x, projectile_target_y,
                      OWNER_PLAYER, PROJ_TYPE_ICE, PLAYER_ICE_PROJECTILE_DAMAGE + player.magic, 
                      PLAYER_ICE_PROJECTILE_SPEED, PLAYER_ICE_PROJECTILE_LIFESPAN, -1);
    printf("玩家施放冰錐術！\n");
}

/**
 * 玩家使用火球術技能。
 */
void player_use_fireball() {
    float projectile_target_x = player.x + cos(player.facing_angle) * 1000.0f;
    float projectile_target_y = player.y + sin(player.facing_angle) * 1000.0f;
    spawn_projectile(-1, player.x, player.y, projectile_target_x, projectile_target_y,
                      OWNER_PLAYER, PROJ_TYPE_PLAYER_FIREBALL, PLAYER_FIREBALL_DAMAGE + player.magic, 
                      PLAYER_FIREBALL_SPEED, PLAYER_FIREBALL_LIFESPAN, -1);
    printf("玩家施放火球術！\n");
}
/**
 * 玩家使用元素彈技能。
 */
void player_use_element_ball() {
    if(player.learned_skills[SKILL_ELEMENT_BALL].cooldown_timers > 0) return;
    if(player.mp < 10) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
        return;
    }
    player.mp -= 10;
    int types[4] = {PROJ_TYPE_PLAYER_FIREBALL, PROJ_TYPE_WATER, PROJ_TYPE_ICE};
    player.learned_skills[SKILL_ELEMENT_BALL].cooldown_timers = 1.5f * FPS; 
    player.learned_skills[SKILL_ELEMENT_BALL].variable_1 ++;
    if(player.learned_skills[SKILL_ELEMENT_BALL].variable_1 >= 4) player.learned_skills[SKILL_ELEMENT_BALL].variable_1 = 0;
    switch (types[player.learned_skills[SKILL_ELEMENT_BALL].variable_1])
    {
    case PROJ_TYPE_PLAYER_FIREBALL:
        player_use_fireball();
        break;
    case PROJ_TYPE_WATER:
        player_use_water_attack();
        break;
    case PROJ_TYPE_ICE:
        player_use_ice_shard();
        break;
    
    default:
        break;
    }
}
/**
 * 玩家使用閃電鏈技能。
 */
void player_use_lightning_bolt() {
    if (player.learned_skills[SKILL_LIGHTNING_BOLT].cooldown_timers > 0) { 
        printf("閃電鏈冷卻中 (%ds)\n", player.learned_skills[SKILL_LIGHTNING_BOLT].cooldown_timers/FPS + 1);
        return;
    }
    if(player.mp < 15) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
        return;
    }
    player.mp -= 15;
    
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
    if(player.mp < 10) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
        return;
    }
    player.mp -= 10;
    
    char heal_text[16];
    player.learned_skills[SKILL_HEAL].cooldown_timers = PLAYER_HEAL_SKILL_COOLDOWN; 
    int old_hp = player.hp;
    player.hp += PLAYER_HEAL_AMOUNT + player.magic * 2; 
    if (player.hp > player.max_hp) player.hp = player.max_hp; 
    printf("玩家治療了 %d 點生命！ (%d -> %d)\n", player.hp - old_hp, old_hp, player.hp);
    // 顯示特效
    snprintf(heal_text, sizeof(heal_text), "%.0d", player.hp - old_hp); // 無小數位，%.1f 顯示1位小數也可
    spawn_floating_text(player.x + 15, player.y - 25, heal_text, al_map_rgba(0, 255, 0, 250));
}

/**
 * 玩家使用治療術技能。
 */
void player_use_reflect_barrier() {
    if (player.learned_skills[SKILL_REFLECT_BARRIER].cooldown_timers > 0) { 
        printf("反轉結界冷卻中 (%ds)\n", player.learned_skills[SKILL_REFLECT_BARRIER].cooldown_timers/FPS + 1);
        return;
    }
    if(player.mp < 80) {
        spawn_floating_text(player.x + 15, player.y - 25, "魔力不足", al_map_rgb(0, 100, 255));
        return;
    }
    player.mp -= 80;
    int range = 50;
    get_clamped_target_position(player.x, player.y, range,
                                &player.learned_skills[SKILL_REFLECT_BARRIER].x, &player.learned_skills[SKILL_REFLECT_BARRIER].y);
    
    player.learned_skills[SKILL_REFLECT_BARRIER].cooldown_timers = 8 * FPS;
    player.learned_skills[SKILL_REFLECT_BARRIER].duration_timers = 3 * FPS; 
}

void using_reflect_barrier() {
    int r = 100;
    int x = player.learned_skills[SKILL_REFLECT_BARRIER].x;
    int y = player.learned_skills[SKILL_REFLECT_BARRIER].y;
    spawn_rune_circle(x, y, r, al_map_rgba(200, 100, 255, 200));

    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].owner == OWNER_BOSS && projectiles[i].active) { 

            if (calculate_distance_between_points(projectiles[i].x, projectiles[i].y, x, y) < r + projectiles[i].radius) {
                projectiles[i].v_x *= -1;
                projectiles[i].v_y *= -1;
                projectiles[i].lifespan += 3 *2.5f;
                projectiles[i].owner = OWNER_PLAYER;

                // 反轉特效
                spawn_floating_text(projectiles[i].x, projectiles[i].y, "反轉", al_map_rgba(250, 0, 250, 250));
            }
        }
    }
    
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
                int damage_dealt = KNIFE_DAMAGE_BASE + player.strength;
                if(bosses[i].current_ability_in_use == BOSS_ABILITY_SKILL){
                    spawn_floating_text(bosses[i].x + 15, bosses[i].y - 25, "防禦破除", al_map_rgb(250, 100, 0));
                }else
                {
                    damage_dealt -= bosses[i].defense;
                }
                
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
    if(player.learned_skills[SKILL_ARCANE_ORB].duration_timers > 0) {
        using_arcane_orb();
    }
    if(player.learned_skills[SKILL_ELEMENT_DASH].duration_timers > 0) {
        using_element_dash();
    }
    if(player.learned_skills[SKILL_RUNE_IMPLOSION].duration_timers > 0) {
        using_rune_implosion();
    }
    if(player.learned_skills[SKILL_FOCUS].duration_timers > 0) {
        player_using_focus();
    }
    if(player.learned_skills[SKILL_REFLECT_BARRIER].duration_timers > 0) {
        using_reflect_barrier();
    }

    if(player.mp < player.max_mp - 1 && (int)battle_time%6 == 0 && player.state == STATE_NORMAL) player.mp += 1 * battle_speed_multiplier; // 回復魔力
}
