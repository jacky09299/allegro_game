#ifndef BATTLE_MANAGER_H
#define BATTLE_MANAGER_H

// Intentionally left blank for now, or add common includes if obvious
// For example:
// #include "globals.h" // Might be needed by functions defined here later
// #include "player.h"  // Might be needed
// #include "boss.h"    // Might be needed
#include <allegro5/allegro.h> // For ALLEGRO_EVENT
#include <stdbool.h> // for bool

void start_new_battle(void);
void manage_battle_state(void);

// Initializes resources or states specific to the battle manager itself, if any.
// For now, start_new_battle() handles per-battle setup. This is a placeholder if needed later.
// void battle_manager_init(void);

// Handles all input events during the BATTLE game phase.
void battle_manager_handle_input(ALLEGRO_EVENT* ev, bool keys[]);

// Updates all game logic for the BATTLE game phase.
// This includes player, bosses, projectiles, camera, and checks win/loss conditions.
void battle_manager_update(void);

// Renders everything for the BATTLE game phase.
// This includes the floor, player, bosses, projectiles, knife, UI, and escape gate.
void battle_manager_render(void);

#endif /* BATTLE_MANAGER_H */
