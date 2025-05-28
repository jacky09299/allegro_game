#ifndef PLAYER_H
#define PLAYER_H

#include "types.h" // For Player, PlayerSkillIdentifier, PlayerKnifeState
#include <allegro5/allegro.h> // For ALLEGRO_EVENT

void init_player(void);
void init_player_knife(void);
void update_player_character(void);
void update_player_knife(void);

// New functions for handling player input in battle
void handle_player_battle_movement(bool keys[]);
void handle_player_battle_aim(int mouse_x, int mouse_y);
void player_process_battle_input_event(ALLEGRO_EVENT ev);

void player_perform_normal_attack(void);
void player_use_water_attack(void);
void player_use_ice_shard(void);
void player_use_lightning_bolt(void);
void player_use_heal(void);
void player_use_fireball(void);

void update_game_camera(void); // 相機永遠在角色上方，玩家會固定在視窗中間

#endif // PLAYER_H
