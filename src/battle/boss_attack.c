#include "boss_attack.h"
#include "globals.h"    // For player, bosses array
#include "config.h"     // For boss related constants
#include "projectile.h" // For spawn_projectile
#include "utils.h"      // For calculate_distance_between_points
#include "effect.h"     // For pawn_warning_circle, spawn_claw_slash, spawn_floating_text
#include "player_attack.h" // For player_is_hit
#include <stdio.h>      // For printf
#include <stdlib.h>     // For rand
#include <math.h>       // For M_PI, atan2, cos, sin (though M_PI, atan2, cos, sin might not be strictly needed if only action evaluation is here)

#include <allegro5/allegro_primitives.h> // 用於 al_draw_filled_circle, al_draw_line, al_clear_to_color 等
#include <allegro5/allegro_color.h> // 用於 al_map_rgb

char dmg_text[16];
/*
* Boss AI
*/
void update_tank_ai(Boss* b) {
    float dx = player.x - b->x;
    float dy = player.y - b->y;
    float dist = calculate_distance_between_points(b->x, b->y, player.x, player.y);
    //int current_ranged_special_damage = BOSS_RANGED_SPECIAL_BASE_DAMAGE + b->magic;
    int difficulty_tier = current_day / 3;
    float normal_spead = 1.8f + difficulty_tier * 0.05f;
    normal_spead *= 0.75f;
    

    float angle_to_player = atan2(player.y - b->y, player.x - b->x); 

    // 技能 1: 裂地震擊
    if (b->learned_skills[BOSS_SKILL_1].cooldown_timers <= 0 && dist < 150 && b->learned_skills[BOSS_SKILL_1].variable_1 == 0
    && b->current_ability_in_use == BOSS_ABILITY_MELEE_PRIMARY) {
        b->current_ability_in_use = BOSS_ABILITY_SKILL;
        // 1. 停下等待: 會隨著boss轉動但不移動
        b->speed = 0;
        b->learned_skills[BOSS_SKILL_1].cooldown_timers = 3 * FPS;
        b->learned_skills[BOSS_SKILL_1].variable_1 = 1;
        printf("Boss use Skill 1\n");
    }
    // 2. 顯示預警效果（紅色圓圈） 
    if (b->learned_skills[BOSS_SKILL_1].cooldown_timers > 0 && b->learned_skills[BOSS_SKILL_1].variable_1 == 1) {
        int r = 150; //調整技能大小範圍
        spawn_warning_circle(b->x +cos(angle_to_player) * r, b->y +sin(angle_to_player) * r, r, 
                            al_map_rgba(255, 0, 0, 50) , 0.1f * FPS);
    }
    // 3. 施放技能
    if (b->learned_skills[BOSS_SKILL_1].cooldown_timers <= 0 && b->learned_skills[BOSS_SKILL_1].variable_1 == 1) {
        int r = 145; //調整技能大小範圍
        spawn_warning_circle(b->x +cos(angle_to_player) * (r+5), b->y +sin(angle_to_player) * (r+5), r + 5, 
                            al_map_rgb(1, 1, 1), 5 * FPS);
        spawn_warning_circle(b->x +cos(angle_to_player) * r, b->y +sin(angle_to_player) * r, r, 
                            al_map_rgb(139, 69, 19), 1 * FPS);
        //al_draw_filled_circle(b->x +cos(angle_to_player) * r, b->y +sin(angle_to_player) * r, r, al_map_rgba(150, 200, 0, 250));
        //al_map_rgb(255, 100, 0)
        // 3. 判斷玩家是否在範圍內
        if(calculate_distance_between_points(b->x+cos(angle_to_player) * r, b->y +sin(angle_to_player) * r, player.x, player.y) < r) { //命中判定
            // 傷害計算
            if(player_is_hit()){
                int damage_dealt = b->strength + 50; 
                player.hp -= 5 * damage_dealt; // 高額傷害
                player.hp -= player.max_hp * 0.05f; // 百分比傷害
                //player.is_stunned = true;
                //player.stun_timer = 60; // 暈 1 秒
                // 傷害顯示
                float tolDamage = 2.5f * damage_dealt + player.max_hp * 0.05f;
                snprintf(dmg_text, sizeof(dmg_text), "%.0f", tolDamage); // 無小數位，%.1f 顯示1位小數也可
                spawn_floating_text(player.x + 15, player.y - 25, dmg_text, al_map_rgba(255, 0, 0, 250));
                // 暈眩效果
                player.effect_timers[STATE_STUN] = 2 * FPS;
                spawn_floating_text(player.x + 15, player.y - 25, "擊暈", al_map_rgba(200, 100, 255, 250));
            }   
        }
        b->speed = normal_spead;
        b->learned_skills[BOSS_SKILL_1].variable_1 = 0;
        b->learned_skills[BOSS_SKILL_1].cooldown_timers = 8 * FPS;
        b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY;
    }


    // 技能 2: 破城突擊（距離遠則衝刺）
    if (b->learned_skills[BOSS_SKILL_2].cooldown_timers <= 0 && dist > 500 && b->current_ability_in_use == BOSS_ABILITY_MELEE_PRIMARY) {
        b->current_ability_in_use = BOSS_ABILITY_SKILL;
        b->is_dashing = true;
        b->speed = player.max_speed * 2.0f;
        printf("Boss use Skill 2\n");
    }
    if(b->is_dashing) {
        spawn_warning_circle(player.x, player.y, 50, 
                            al_map_rgba(255, 0, 0, 50) , 2);
    }
    if(b->is_dashing && dist < 50){
        b->is_dashing = false;
        b->speed = normal_spead;
        
        // 傷害計算
        if(player_is_hit()) {
            int damage_dealt = b->strength * 5; 
            player.hp -= damage_dealt; 
            printf("Boss attacked player with skill 2! player HP: %d\n", player.hp);

            // 傷害顯示
            snprintf(dmg_text, sizeof(dmg_text), "%.0d", damage_dealt); // 無小數位，%.1f 顯示1位小數也可
            spawn_floating_text(player.x + 15, player.y - 25, dmg_text, al_map_rgba(255, 0, 0, 250));

            b->learned_skills[BOSS_SKILL_2].cooldown_timers = 15 * FPS;
            // TODO: 添加暈眩與特效
            player.effect_timers[STATE_STUN] = 1 * FPS;
            spawn_floating_text(player.x + 15, player.y - 25, "擊暈", al_map_rgba(200, 100, 255, 250));
        }

        b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY;
    }

    // 遠程攻擊: 巨石投擲
    if (b->learned_skills[BOSS_RANGE_PRIMARY].cooldown_timers  <= 0 && dist > 150
        && b->current_ability_in_use == BOSS_ABILITY_MELEE_PRIMARY
    ) {
        spawn_projectile(30, b->x, b->y, player.x, player.y,  //發射巨石
                        OWNER_BOSS, PROJ_TYPE_BIG_EARTHBALL, 20 + b->magic * 2,
                        BOSS_RANGED_SPECIAL_PROJECTILE_BASE_SPEED * 0.4f, BOSS_RANGED_SPECIAL_PROJECTILE_BASE_LIFESPAN * 2, b->id);
        // TODO: 停頓?
        b->learned_skills[BOSS_RANGE_PRIMARY].cooldown_timers  = 5 * FPS;
    }

    // 近戰攻擊:
    if (b->learned_skills[BOSS_MELEE_PRIMARY].cooldown_timers  <= 0 && dist < 150
    &&  b->current_ability_in_use == BOSS_ABILITY_MELEE_PRIMARY) {
        // TODO:
        spawn_projectile(50, b->x, b->y, player.x, player.y,  //發射巨石
                        OWNER_BOSS, PROJ_TYPE_BIG_EARTHBALL, 20 + b->strength,
                        1, 80, b->id);
        b->learned_skills[BOSS_MELEE_PRIMARY].cooldown_timers  = 2 * FPS;
    }

    // 基本面向
    b->facing_angle = atan2f(dy, dx);
}

// === 法師型 Boss：迷霧咒者 ===
void update_skillful_ai(Boss* b) {
    float dx = player.x - b->x;
    float dy = player.y - b->y;
    float dist = calculate_distance_between_points(b->x, b->y, player.x, player.y); 
    int current_ranged_special_damage = BOSS_RANGED_SPECIAL_BASE_DAMAGE + b->magic;

    // 技能 1: 咒靈彈幕
    if (b->learned_skills[BOSS_SKILL_1].cooldown_timers <= 0 && b->learned_skills[BOSS_SKILL_1].variable_1 == 0) {
        b->learned_skills[BOSS_SKILL_1].variable_1 = 10;
        b->learned_skills[BOSS_SKILL_1].cooldown_timers  = 0.0f;
    }
    if (b->learned_skills[BOSS_SKILL_1].variable_1 > 0 && b->learned_skills[BOSS_SKILL_1].cooldown_timers <= 0) {
        spawn_projectile(-1, b->x, b->y, player.x, player.y,  //發射咒彈
                        OWNER_BOSS, PROJ_TYPE_ICE, current_ranged_special_damage,
                        BOSS_RANGED_SPECIAL_PROJECTILE_BASE_SPEED * 1.5f, BOSS_RANGED_SPECIAL_PROJECTILE_BASE_LIFESPAN, b->id);
        b->learned_skills[BOSS_SKILL_1].variable_1--;
        if(b->learned_skills[BOSS_SKILL_1].variable_1 == 0) b->learned_skills[BOSS_SKILL_1].cooldown_timers  = 5 * FPS;
        else b->learned_skills[BOSS_SKILL_1].cooldown_timers  = 0.2 * FPS;
    }

    // 技能 2: 虛空閃現（逃跑）
    if (b->learned_skills[BOSS_SKILL_2].cooldown_timers <= 0 && b->hp < b->learned_skills[BOSS_SKILL_2].variable_1) {
        float angle = atan2f(dy, dx) + ALLEGRO_PI; // 相反方向
        float teleport_dist = 200.0f;
        b->x += cosf(angle) * teleport_dist;
        b->y += sinf(angle) * teleport_dist;
        b->learned_skills[BOSS_SKILL_2].cooldown_timers = 24 * FPS;
        b->learned_skills[BOSS_SKILL_2].variable_1 = b->hp;
        // TODO: 特效 & 無敵短時間
        b->learned_skills[BOSS_MELEE_PRIMARY].cooldown_timers = 0; // 重製近戰技能
        b->learned_skills[BOSS_SKILL_3].variable_1 = 0;             // 重製詛咒層數
    }
    if (b->hp != b->learned_skills[BOSS_SKILL_2].variable_1) {
        b->learned_skills[BOSS_SKILL_2].variable_1 = b->hp;
    }

    // 技能 3: 詛咒連結
    
    // 造成傷害、回血、減速
    if (b->learned_skills[BOSS_SKILL_3].variable_1 > 0 && b->learned_skills[BOSS_SKILL_3].cooldown_timers <= 0) {
        float angle = atan2f(dy, dx);
        float teleport_dist = dist - 1000;

        if(b->learned_skills[BOSS_SKILL_3].variable_1 > 10) {
            player.effect_timers[STATE_SLOWNESS] = 5 * FPS;
            b->x += cosf(angle) * teleport_dist;
            b->y += sinf(angle) * teleport_dist;
        }
        // 詛咒傷害
        int damage_dealt = b->learned_skills[BOSS_SKILL_3].variable_1 * 5 + b->magic;
        player.hp -= damage_dealt; 
        // 傷害顯示
        snprintf(dmg_text, sizeof(dmg_text), "詛咒傷害 %.0d", damage_dealt); // 無小數位，%.1f 顯示1位小數也可
        spawn_floating_text(player.x + 15, player.y - 25, dmg_text, al_map_rgba(255, 0, 200, 250));
        // 回血
        if(b->max_hp > b->hp + damage_dealt) b->hp += damage_dealt; 
        // 回血顯示
        snprintf(dmg_text, sizeof(dmg_text), "%.0d", damage_dealt); // 無小數位，%.1f 顯示1位小數也可
        spawn_floating_text(b->x + 15, b->y - 25, dmg_text, al_map_rgba(0, 255, 0, 250));
        printf("Boss demage player with skill 3!\n");
    }
    // 顯示連結
    if (b->learned_skills[BOSS_SKILL_3].variable_1 > 0) {
        //draw_curse_link(b); // not working yet.
        spawn_curse_link(b->x, b->y, 3);
    }
    // 等待、疊層
    if (b->learned_skills[BOSS_SKILL_3].cooldown_timers <= 0) {
        if(dist > 1000) {
            b->learned_skills[BOSS_SKILL_3].variable_1 ++;
            printf("Boss curse player with skill 3! level: %d\n", b->learned_skills[BOSS_SKILL_3].variable_1);
        }
        b->learned_skills[BOSS_SKILL_3].cooldown_timers = 5 * FPS;
    }
    
    // 遠程攻擊: 咒靈彈幕(慢速)
    if (b->learned_skills[BOSS_RANGE_PRIMARY].cooldown_timers <= 0 ) {
        spawn_projectile(-1, b->x, b->y, player.x, player.y,  //發射咒彈
                        OWNER_BOSS, PROJ_TYPE_ICE, current_ranged_special_damage,
                        BOSS_RANGED_SPECIAL_PROJECTILE_BASE_SPEED * 0.5f, BOSS_RANGED_SPECIAL_PROJECTILE_BASE_LIFESPAN, b->id);
        b->learned_skills[BOSS_RANGE_PRIMARY].cooldown_timers   = 0.5 * FPS;
    }

    // 近戰攻擊: 幽影漩渦（地面AOE）
    if (b->learned_skills[BOSS_MELEE_PRIMARY].cooldown_timers  <= 0 && dist <= 120) {
        b->learned_skills[BOSS_MELEE_PRIMARY].x = b->x;
        b->learned_skills[BOSS_MELEE_PRIMARY].y = b->y;
        b->learned_skills[BOSS_MELEE_PRIMARY].duration_timers = 5 * FPS;
        spawn_warning_circle(b->x, b->y, 120, 
                            al_map_rgba(0, 0, 150, 100) , 5 * FPS);
        // TODO: 召喚地面吸附 + 爆炸
        b->learned_skills[BOSS_MELEE_PRIMARY].cooldown_timers  = 10 * FPS;
    }
    if(b->learned_skills[BOSS_MELEE_PRIMARY].duration_timers > 0){
        int x = b->learned_skills[BOSS_MELEE_PRIMARY].x;
        int y = b->learned_skills[BOSS_MELEE_PRIMARY].y;
        if(calculate_distance_between_points(x,y,player.x,player.y) <= 120) {
            if(player.effect_timers[STATE_SLOWNESS] <= 0) {
                spawn_floating_text(player.x + 15, player.y - 25, "緩速", al_map_rgba(0, 200, 255, 250));}
            player.effect_timers[STATE_SLOWNESS] = 2 * FPS;
        }
    }

    b->facing_angle = atan2f(dy, dx);
}

// === 猛獸型 Boss：血爪獵影 ===
void update_berserker_ai(Boss* b) {
    float dx = player.x - b->x;
    float dy = player.y - b->y;
    float dist = calculate_distance_between_points(b->x, b->y, player.x, player.y);
    int difficulty_tier = current_day / 3;
    float normal_spead = 1.8f + difficulty_tier * 0.05f;
    normal_spead *= 1.5f;

    float angle_to_player = atan2(player.y - b->y, player.x - b->x); 

    // 技能 1: 撕咬連擊（近身）
    if (b->learned_skills[BOSS_SKILL_1].cooldown_timers <= 0 && dist < 300 && b->learned_skills[BOSS_SKILL_1].variable_1 <= 0) {
        b->current_ability_in_use = BOSS_ABILITY_SKILL; 
        b->speed = 0;
        // TODO: 飛撲快速 3 連擊 + 吸血
        b->learned_skills[BOSS_SKILL_1].variable_1 = 3;
        b->learned_skills[BOSS_SKILL_1].cooldown_timers = 0.5 * FPS; //前搖
    }
    if(b->learned_skills[BOSS_SKILL_1].cooldown_timers <= 0 && b->learned_skills[BOSS_SKILL_1].variable_1 > 0) { //衝刺判定
        b->is_dashing = true;
        b->speed = player.max_speed * 2.0f;
    }
    if(b->learned_skills[BOSS_SKILL_1].cooldown_timers <= 0 && b->learned_skills[BOSS_SKILL_1].variable_1 >= 0 &&
        dist < 50) { //命中判定
        spawn_claw_slash(player.x, player.y, angle_to_player);
        b->speed = 0;
        b->is_dashing = false;

        // 傷害計算
        if(player_is_hit()) {
            int damage_dealt = 10 + b->strength; 
            player.hp -= damage_dealt;
            if(b->learned_skills[BOSS_SKILL_3].variable_1 > 2) b->learned_skills[BOSS_SKILL_3].variable_1 -= 2;  // 命中降低怒氣值
            
            // 傷害特效
            snprintf(dmg_text, sizeof(dmg_text), "%.0d", damage_dealt); // 無小數位，%.1f 顯示1位小數也可
            spawn_floating_text(player.x + 15, player.y - 25, dmg_text, al_map_rgba(255, 0, 0, 250));
            printf("Boss attacked player with skill 1! player HP: %d\n", player.hp);

            // 吸血
            if(b->max_hp > b->hp + damage_dealt * 0.2f) b->hp += damage_dealt * 0.2f;
            
            // 吸血特效
            snprintf(dmg_text, sizeof(dmg_text), "%.0f", damage_dealt * 0.2f); // 無小數位，%.1f 顯示1位小數也可
            spawn_floating_text(b->x + 15, b->y - 25, dmg_text, al_map_rgba(0, 255, 0, 250));
        }
    
        b->learned_skills[BOSS_SKILL_1].variable_1--;

        if(b->learned_skills[BOSS_SKILL_1].variable_1 <= 0) { //終止設定
            b->speed = normal_spead;
            b->learned_skills[BOSS_SKILL_1].cooldown_timers  = 8 * FPS;
            b->is_dashing = false;
            b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY; 
        }
        else b->learned_skills[BOSS_SKILL_1].cooldown_timers  = 0.5 * FPS;
    }

    // 技能 2: 獵殺衝刺（中距離）
    if (b->learned_skills[BOSS_SKILL_2].cooldown_timers <= 0 && dist > 300 
        && dist < 1000 && b->current_ability_in_use == BOSS_ABILITY_MELEE_PRIMARY) {
        b->current_ability_in_use = BOSS_ABILITY_SKILL;
        b->is_dashing = true;
        b->speed = player.max_speed * 2.0f;
        printf("Boss use Skill 2\n");
    }
    if(b->is_dashing) {
        spawn_warning_circle(player.x, player.y, 50, 
                            al_map_rgba(255, 0, 0, 50) , 0.1f * FPS);
    }
    if(b->is_dashing && dist < 50){
        b->is_dashing = false;
        b->speed = normal_spead;
        
        // 傷害計算
        if(player_is_hit()) {
            int damage_dealt = 10 + b->strength * 2; 
            player.hp -= damage_dealt; 
            printf("Boss attacked player with skill 2! player HP: %d\n", player.hp);

            // 傷害顯示
            snprintf(dmg_text, sizeof(dmg_text), "%.0d", damage_dealt); // 無小數位，%.1f 顯示1位小數也可
            spawn_floating_text(player.x + 15, player.y - 25, dmg_text, al_map_rgba(255, 0, 0, 250));

            b->learned_skills[BOSS_SKILL_2].cooldown_timers = 15 * FPS;
            // TODO: 添加暈眩與特效
            player.effect_timers[STATE_STUN] = 1 * FPS;
            spawn_floating_text(player.x + 15, player.y - 25, "擊暈", al_map_rgba(200, 100, 255, 250));
        }
        b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY;
    }

    // 技能 3: 嗜血狂暴（怒氣觸發）
    if (b->learned_skills[BOSS_SKILL_3].variable_1 >= 100 && b->learned_skills[BOSS_SKILL_3].cooldown_timers <= 0) {
        b->learned_skills[BOSS_SKILL_3].variable_1 = 100;
        b->learned_skills[BOSS_SKILL_3].cooldown_timers = 999 * FPS;
        spawn_floating_text(b->x + 15, b->y - 25, "狂暴化", al_map_rgba(255, 0, 0, 250));
    }
    // 狂暴特效
    if (b->learned_skills[BOSS_SKILL_3].cooldown_timers >= 600) {
        b->speed = player.max_speed * 1.1f;
        spawn_warning_circle(b->x, b->y, 40, 
                            al_map_rgba(255, 0, 0, 150) , 0.1f * FPS);
        if(b->learned_skills[BOSS_SKILL_3].cooldown_timers % FPS == 0) b->learned_skills[BOSS_SKILL_3].variable_1--;
    }
    // 狀態重製
    if (b->learned_skills[BOSS_SKILL_3].variable_1 <= 0 && b->learned_skills[BOSS_SKILL_3].cooldown_timers >= 600) {
        b->speed = normal_spead;
        b->learned_skills[BOSS_SKILL_3].cooldown_timers = 1 * FPS;
    }
    // 被動：夜行者移動充能
    if (b->learned_skills[BOSS_SKILL_3].variable_1 < 100 && b->learned_skills[BOSS_SKILL_3].cooldown_timers <= 0) {
        // TODO: 狂暴 buff
        b->learned_skills[BOSS_SKILL_3].variable_1 += (int)(dist/100);
        if(b->learned_skills[BOSS_SKILL_3].variable_1 > 100) {
            b->learned_skills[BOSS_SKILL_3].variable_1 = 100;
        }
        b->learned_skills[BOSS_SKILL_3].cooldown_timers = 1 * FPS;
    }
    
    // 近戰攻擊: 撕咬
    if (b->learned_skills[BOSS_MELEE_PRIMARY].cooldown_timers  <= 0 && dist < 80 
        && b->current_ability_in_use == BOSS_ABILITY_MELEE_PRIMARY
    ) 
    {
        spawn_claw_slash(player.x, player.y, angle_to_player);
        // 傷害計算
        if(player_is_hit()) {
            int damage_dealt = 5 + b->strength; 
            player.hp -= damage_dealt;
            if(b->learned_skills[BOSS_SKILL_3].variable_1 > 3) b->learned_skills[BOSS_SKILL_3].variable_1 -= 3; // 命中降低怒氣值
            
            // 傷害特效
            snprintf(dmg_text, sizeof(dmg_text), "%.0d", damage_dealt); // 無小數位，%.1f 顯示1位小數也可
            spawn_floating_text(player.x + 15, player.y - 25, dmg_text, al_map_rgba(255, 0, 0, 250));
            printf("Boss attacked player with skill 1! player HP: %d\n", player.hp);

            if(b->learned_skills[BOSS_SKILL_3].cooldown_timers >= 600){
                // 吸血
                if(b->max_hp > b->hp + damage_dealt * 0.2f) b->hp += damage_dealt * 0.2f;

                // 吸血特效
                snprintf(dmg_text, sizeof(dmg_text), "%.0f", damage_dealt * 0.2f); // 無小數位，%.1f 顯示1位小數也可
                spawn_floating_text(b->x + 15, b->y - 25, dmg_text, al_map_rgba(0, 255, 0, 250));
            }
        }

        b->learned_skills[BOSS_MELEE_PRIMARY].cooldown_timers  = 0.5 * FPS;
    }

    b->facing_angle = atan2f(dy, dx);
}


/**
 * Boss 評估當前狀況並執行相應的動作。
 */
void boss_evaluate_and_execute_action(Boss* b) {
    if (!b->is_alive) return; 

    if (b->archetype == BOSS_TYPE_SKILLFUL) update_skillful_ai(b);
    if(b->archetype == BOSS_TYPE_TANK) update_tank_ai(b);
    if (b->archetype == BOSS_TYPE_BERSERKER) update_berserker_ai(b);

    // float dist_to_player = calculate_distance_between_points(b->x, b->y, player.x, player.y); 
    // bool attempt_ranged_special = false; 
    // int ranged_special_usage_chance = 20; 

    // if (b->archetype == BOSS_TYPE_SKILLFUL) {
    //     ranged_special_usage_chance = 55; 
    // } else if (b->archetype == BOSS_TYPE_BERSERKER) {
    //     ranged_special_usage_chance = 10; 
    // }

    // if ((rand() % 100) < ranged_special_usage_chance) { 
    //     attempt_ranged_special = true;
    // }

    // if (b->learned_skills[BOSS_RANGE_PRIMARY] <= 0 && dist_to_player < 600 && attempt_ranged_special) { 
    //     b->current_ability_in_use = BOSS_ABILITY_RANGED_SPECIAL; 
        
    //     int current_ranged_special_damage = BOSS_RANGED_SPECIAL_BASE_DAMAGE + b->magic; 
    //     if (b->archetype == BOSS_TYPE_SKILLFUL) {
    //         current_ranged_special_damage = (int)(current_ranged_special_damage * 1.4f); 
    //     } else if (b->archetype == BOSS_TYPE_BERSERKER) {
    //         current_ranged_special_damage = (int)(current_ranged_special_damage * 0.7f); 
    //     }

    //     spawn_projectile(b->x, b->y, player.x, player.y, 
    //                       OWNER_BOSS, b->ranged_special_projectile_type, current_ranged_special_damage,
    //                       BOSS_RANGED_SPECIAL_PROJECTILE_BASE_SPEED, BOSS_RANGED_SPECIAL_PROJECTILE_BASE_LIFESPAN, b->id);
        
    //     if (b->archetype == BOSS_TYPE_SKILLFUL) {
    //         b->learned_skills[BOSS_RANGE_PRIMARY] = (int)(BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN * 0.45f); 
    //     } else if (b->archetype == BOSS_TYPE_BERSERKER) {
    //         b->learned_skills[BOSS_RANGE_PRIMARY] = (int)(BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN * 1.6f); 
    //     } else { 
    //         b->learned_skills[BOSS_RANGE_PRIMARY] = BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN;
    //     }
    //     printf("Boss %d (原型 %d) 使用遠程特殊技能 (類型: %d)！傷害: %d\n", b->id, b->archetype, b->ranged_special_projectile_type, current_ranged_special_damage);
    //     b->learned_skills[BOSS_MELEE_PRIMARY] += FPS * 0.25; 
    //     return; 
    // }

    // float effective_melee_range = BOSS_MELEE_PRIMARY_BASE_RANGE;
    //  if (b->archetype == BOSS_TYPE_BERSERKER) {
    //     effective_melee_range *= 1.1f; 
    // }

    // if (b->learned_skills[BOSS_MELEE_PRIMARY] <= 0 && dist_to_player < (effective_melee_range + b->collision_radius + PLAYER_SPRITE_SIZE)) { 
    //     b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY; 
    //     int damage_dealt = b->strength; 
    //     player.hp -= damage_dealt; 
    //     printf("Boss %d (原型 %d) 使用近戰主技能！玩家 HP: %d\n", b->id, b->archetype, player.hp);

    //     if (b->archetype == BOSS_TYPE_BERSERKER) {
    //         b->learned_skills[BOSS_MELEE_PRIMARY] = (int)(BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN * 0.50f); 
    //     } else if (b->archetype == BOSS_TYPE_SKILLFUL) {
    //         b->learned_skills[BOSS_MELEE_PRIMARY] = (int)(BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN * 1.45f); 
    //     } else { 
    //         b->learned_skills[BOSS_MELEE_PRIMARY] = BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN;
    //     }
    //     return; 
    // }
    // b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY; // Default action if nothing else
}