#include "boss_attack.h"
#include "globals.h"    // For player, bosses array
#include "config.h"     // For boss related constants
#include "projectile.h" // For spawn_projectile
#include "utils.h"      // For calculate_distance_between_points
#include <stdio.h>      // For printf
#include <stdlib.h>     // For rand
#include <math.h>       // For M_PI, atan2, cos, sin (though M_PI, atan2, cos, sin might not be strictly needed if only action evaluation is here)

/**
 * Boss 評估當前狀況並執行相應的動作。
 */
void boss_evaluate_and_execute_action(Boss* b) {
    if (!b->is_alive) return; 

    float dist_to_player = calculate_distance_between_points(b->x, b->y, player.x, player.y); 
    bool attempt_ranged_special = false; 
    int ranged_special_usage_chance = 20; 

    if (b->archetype == BOSS_TYPE_SKILLFUL) {
        ranged_special_usage_chance = 55; 
    } else if (b->archetype == BOSS_TYPE_BERSERKER) {
        ranged_special_usage_chance = 10; 
    }

    if ((rand() % 100) < ranged_special_usage_chance) { 
        attempt_ranged_special = true;
    }

    if (b->ranged_special_ability_cooldown_timer <= 0 && dist_to_player < 600 && attempt_ranged_special) { 
        b->current_ability_in_use = BOSS_ABILITY_RANGED_SPECIAL; 
        
        int current_ranged_special_damage = BOSS_RANGED_SPECIAL_BASE_DAMAGE + b->magic; 
        if (b->archetype == BOSS_TYPE_SKILLFUL) {
            current_ranged_special_damage = (int)(current_ranged_special_damage * 1.4f); 
        } else if (b->archetype == BOSS_TYPE_BERSERKER) {
            current_ranged_special_damage = (int)(current_ranged_special_damage * 0.7f); 
        }

        spawn_projectile(b->x, b->y, player.x, player.y, 
                          OWNER_BOSS, b->ranged_special_projectile_type, current_ranged_special_damage,
                          BOSS_RANGED_SPECIAL_PROJECTILE_BASE_SPEED, BOSS_RANGED_SPECIAL_PROJECTILE_BASE_LIFESPAN, b->id);
        
        if (b->archetype == BOSS_TYPE_SKILLFUL) {
            b->ranged_special_ability_cooldown_timer = (int)(BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN * 0.45f); 
        } else if (b->archetype == BOSS_TYPE_BERSERKER) {
            b->ranged_special_ability_cooldown_timer = (int)(BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN * 1.6f); 
        } else { 
            b->ranged_special_ability_cooldown_timer = BOSS_RANGED_SPECIAL_ABILITY_BASE_COOLDOWN;
        }
        printf("Boss %d (原型 %d) 使用遠程特殊技能 (類型: %d)！傷害: %d\n", b->id, b->archetype, b->ranged_special_projectile_type, current_ranged_special_damage);
        b->melee_primary_ability_cooldown_timer += FPS * 0.25; 
        return; 
    }

    float effective_melee_range = BOSS_MELEE_PRIMARY_BASE_RANGE;
     if (b->archetype == BOSS_TYPE_BERSERKER) {
        effective_melee_range *= 1.1f; 
    }

    if (b->melee_primary_ability_cooldown_timer <= 0 && dist_to_player < (effective_melee_range + b->collision_radius + PLAYER_SPRITE_SIZE)) { 
        b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY; 
        int damage_dealt = b->strength; 
        player.hp -= damage_dealt; 
        printf("Boss %d (原型 %d) 使用近戰主技能！玩家 HP: %d\n", b->id, b->archetype, player.hp);

        if (b->archetype == BOSS_TYPE_BERSERKER) {
            b->melee_primary_ability_cooldown_timer = (int)(BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN * 0.50f); 
        } else if (b->archetype == BOSS_TYPE_SKILLFUL) {
            b->melee_primary_ability_cooldown_timer = (int)(BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN * 1.45f); 
        } else { 
            b->melee_primary_ability_cooldown_timer = BOSS_MELEE_PRIMARY_ABILITY_BASE_COOLDOWN;
        }
        return; 
    }
    b->current_ability_in_use = BOSS_ABILITY_MELEE_PRIMARY; // Default action if nothing else
}
