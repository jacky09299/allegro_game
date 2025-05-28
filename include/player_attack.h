#ifndef PLAYER_ATTACK_H
#define PLAYER_ATTACK_H

#include <stdbool.h>
#include <allegro5/allegro.h>
#include "include/types.h"

// Function declarations for player attacks
void player_perform_normal_attack(void);
void player_use_water_attack(void);
void player_use_ice_shard(void);
void player_use_lightning_bolt(void);
void player_use_heal(void);
void player_use_fireball(void);
void update_player_knife(void);
void init_player_knife(void);

// New function to render the player's knife
void player_attack_render_knife(PlayerKnifeState* p_knife, float camera_x, float camera_y);

#endif // PLAYER_ATTACK_H
