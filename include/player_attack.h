#ifndef PLAYER_ATTACK_H
#define PLAYER_ATTACK_H

#include <stdbool.h>
#include <allegro5/allegro.h>
#include "types.h"

// Function declarations for player attacks
int player_is_hit(void);
void player_use_perfect_defense(void);
void player_defense_start(void);
void player_defense_end();
void player_perform_normal_attack(void);
void player_use_lightning_bolt(void);
void player_use_heal(void);
void update_player_knife(void);
void init_player_knife(void);
void update_player_skill(void);
void player_use_element_dash(void);
void player_start_charge_beam(void);
void player_end_charge_beam(void);
void player_use_arcane_orb(void);
void player_use_rune_implosion(void);
void player_use_element_ball(void);
void player_use_reflect_barrier(void);
void player_start_focus(void);
void player_end_focus(void);


// New function to render the player's knife
void player_attack_render_knife(PlayerKnifeState* p_knife, float camera_x, float camera_y);

#endif // PLAYER_ATTACK_H
