#include "projectile.h"
#include "globals.h" // For projectiles array, player, bosses
#include "utils.h"   // For calculate_distance_between_points
#include "config.h"  // For MAX_PROJECTILES, PROJECTILE_RADIUS, PLAYER_SPRITE_SIZE
#include <stdio.h>   // For printf
#include <math.h>    // For atan2, cos, sin
#include "effect.h"     // For spawn_floating_text
#include "player_attack.h" // For player_is_hit

#include <allegro5/allegro_primitives.h> // 用於 al_draw_filled_circle, al_draw_line, al_clear_to_color 等

char dmg_text[16];

/**
 * @brief 初始化投射物陣列。
 */
void init_projectiles() {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        projectiles[i].active = false;
    }
}

/**
 * @brief 在投射物陣列中尋找一個未被使用的欄位。
 */
int find_inactive_projectile_slot() {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (!projectiles[i].active) { 
            return i; 
        }
    }
    return -1; 
}

/**
 * @brief 生成一個新的投射物。
 */
void spawn_projectile(int radius, float origin_x, float origin_y, float target_x, float target_y, ProjectileOwner owner_type, ProjectileType proj_type, int base_damage, float travel_speed, int active_lifespan_frames, int owner_entity_id) {
    int slot = find_inactive_projectile_slot(); 
    if (slot == -1) return; 
    if(radius == -1) radius = PROJECTILE_RADIUS;
    projectiles[slot].active = true;        
    projectiles[slot].x = origin_x;         
    projectiles[slot].y = origin_y;
    projectiles[slot].radius = radius;         
    projectiles[slot].owner = owner_type;   
    projectiles[slot].type = proj_type;     
    projectiles[slot].damage = base_damage;
    projectiles[slot].lifespan = active_lifespan_frames; 
    projectiles[slot].owner_id = owner_entity_id;
    if(owner_type == OWNER_PLAYER && player.learned_skills[SKILL_FOCUS].variable_1 > 0) {
        projectiles[slot].damage += player.learned_skills[SKILL_FOCUS].variable_1;
        projectiles[slot].radius += player.learned_skills[SKILL_FOCUS].variable_1;
        projectiles[slot].lifespan += player.learned_skills[SKILL_FOCUS].variable_1;
        spawn_floating_text(origin_x + 15, origin_y - 25, "專注一擊", al_map_rgba(155, 0, 200, 250));
    }
    

    float angle_rad = atan2(target_y - origin_y, target_x - origin_x); 
    projectiles[slot].v_x = cos(angle_rad) * travel_speed; 
    projectiles[slot].v_y = sin(angle_rad) * travel_speed; 
}

/**
 * @brief 更新所有活動中投射物的狀態。
 */
void update_active_projectiles() {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].active) { 
            projectiles[i].x += projectiles[i].v_x * battle_speed_multiplier; 
            projectiles[i].y += projectiles[i].v_y * battle_speed_multiplier; 
            projectiles[i].lifespan-= battle_speed_multiplier;             

            if (projectiles[i].lifespan <= 0) { 
                projectiles[i].active = false;  
                continue; 
            }

            // 傷害判定
            if (projectiles[i].owner == OWNER_PLAYER) { 
                for (int j = 0; j < MAX_BOSSES; ++j) {
                    if (bosses[j].is_alive && 
                        calculate_distance_between_points(projectiles[i].x, projectiles[i].y, bosses[j].x, bosses[j].y) < bosses[j].collision_radius + + projectiles[i].radius) {
                        
                        int damage_dealt = projectiles[i].damage;
                        if(bosses[j].current_ability_in_use == BOSS_ABILITY_SKILL){
                            spawn_floating_text(bosses[j].x + 15, bosses[j].y - 25, "防禦破除", al_map_rgb(250, 100, 0));
                        }else
                        {
                            damage_dealt -= bosses[i].defense;
                        }
                        if (damage_dealt < 1 && projectiles[i].damage > 0) damage_dealt = 1; 
                        else if (projectiles[i].damage <= 0) damage_dealt = 0; 

                        if (damage_dealt > 0) bosses[j].hp -= damage_dealt; 
                        if(player.mp < player.max_mp - damage_dealt) player.mp += damage_dealt; // 回復魔力
                        
                        printf("玩家投射物 (傷害: %d, 類型: %d) 命中 Boss %d (原型 %d)！ Boss HP: %d/%d\n", projectiles[i].damage, projectiles[i].type, bosses[j].id, bosses[j].archetype, bosses[j].hp, bosses[j].max_hp);
                        projectiles[i].active = false; 
                        if (bosses[j].hp <= 0) { 
                            bosses[j].is_alive = false;
                            printf("Boss %d (原型 %d) 被投射物擊敗！\n", bosses[j].id, bosses[j].archetype);
                        }
                        break; 
                    }
                }
            } else if (projectiles[i].owner == OWNER_BOSS) { 
                // int proj_size_indicator = 0;                        //用於修正投射物大小
                // if(projectiles[i].type == PROJ_TYPE_BIG_EARTHBALL) proj_size_indicator = 20;
                if (calculate_distance_between_points(projectiles[i].x, projectiles[i].y, player.x, player.y) < PLAYER_SPRITE_SIZE + projectiles[i].radius) {
                    // 傷害計算
                    if(player_is_hit()) {
                        int damage_taken = projectiles[i].damage;
                    if (damage_taken < 0) damage_taken = 0;
                    player.hp -= damage_taken; 
                    
                    // 傷害特效
                    snprintf(dmg_text, sizeof(dmg_text), "%.0d", damage_taken); // 無小數位，%.1f 顯示1位小數也可
                    spawn_floating_text(player.x + 25, player.y - 15, dmg_text, al_map_rgba(255, 0, 0, 250));
                    
                    //printf("玩家被 Boss %d 的投射物 (傷害: %d, 類型: %d) 擊中！玩家 HP: %d\n", projectiles[i].owner_id, projectiles[i].damage, projectiles[i].type, player.hp);
                    }
                    projectiles[i].active = false; 
                }
            }
        }
    }
}

void render_active_projectiles() {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].active) {
            float proj_screen_x = projectiles[i].x - camera_x;
            float proj_screen_y = projectiles[i].y - camera_y;
            ALLEGRO_COLOR proj_color;
            switch (projectiles[i].type) {
                case PROJ_TYPE_WATER: proj_color = al_map_rgb(0, 150, 255); break;
                case PROJ_TYPE_FIRE: proj_color = al_map_rgb(255, 100, 0); break;
                case PROJ_TYPE_ICE: proj_color = al_map_rgb(150, 255, 255); break;
                case PROJ_TYPE_PLAYER_FIREBALL: proj_color = al_map_rgb(255, 50, 50); break;
                case PROJ_TYPE_GENERIC:
                case PROJ_TYPE_BIG_EARTHBALL: proj_color = al_map_rgb(160, 82, 45); break;
                case PROJ_TYPE_DASH: proj_color = al_map_rgba(5, 5, 100, 50); break;
                default: proj_color = al_map_rgb(200, 200, 200); break;
            }
            al_draw_filled_circle(proj_screen_x, proj_screen_y, projectiles[i].radius, proj_color);
            switch (projectiles[i].type)
            {
            case PROJ_TYPE_ICE:
                al_draw_circle(proj_screen_x, proj_screen_y, projectiles[i].radius + 2, al_map_rgb(255, 255, 255), 1.0f);
                break;
            case PROJ_TYPE_PLAYER_FIREBALL:
                al_draw_circle(proj_screen_x, proj_screen_y, projectiles[i].radius + 1, al_map_rgb(255, 255, 255), 1.0f);
                break;
            
            default:
                break;
            }
        }
    }
}