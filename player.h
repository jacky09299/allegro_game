#ifndef PLAYER_H
#define PLAYER_H

#include "types.h" // For Player, PlayerSkillIdentifier, PlayerKnifeState

void init_player(void);
void init_player_knife(void);
void update_player_character(void);
void update_player_knife(void);

void player_perform_normal_attack(void);
void player_use_water_attack(void);
void player_use_ice_shard(void);
void player_use_lightning_bolt(void);
void player_use_heal(void);
void player_use_fireball(void);

void update_game_camera(void); // Camera is tied to player

#endif // PLAYER_H