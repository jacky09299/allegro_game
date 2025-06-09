#ifndef PLAYER_SKILL_SELECT_H
#define PLAYER_SKILL_SELECT_H

#include <allegro5/allegro.h>
#include "types.h" // Added for SkillIdentifier

// Functions for the equipment screen
void init_player_skill_select();
void handle_player_skill_select_input(ALLEGRO_EVENT* ev);
void render_player_skill_select(); // Removed x, y parameters as UI is likely fixed
void update_player_skill_select();

// Helper or external functions (review if they are still needed for this specific screen)
// For now, keep them, but they might not be used by the equipment screen itself.
void player_switch_skill();
void player_use_selected_skill();
int get_selected_skill_index();
void player_skill_add(int skill_type);
void player_skill_remove(int skill_type);
void player_skill_swap(int idx1, int idx2);
int  player_skill_get_selected();
void player_skill_next();
void player_skill_prev();

#endif // PLAYER_SKILL_SELECT_H
