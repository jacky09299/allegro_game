#ifndef MINIGAME_FLOWER_H
#define MINIGAME_FLOWER_H

#include <allegro5/allegro.h>
#include "types.h" // For Button, GamePhase, ALLEGRO_EVENT

#define NUM_MINIGAME_FLOWER_BUTTONS 6 // Plant Seed, Start, Restart, Finish, Exit, Harvest

// Structure to hold the state of the flower in the minigame
typedef struct {
    int songs_sung;
    int growth_stage; // 0: seed, 1-7: growing stages, 8: flowered
} MinigameFlowerPlant;

// Function declarations for the flower minigame
void init_minigame_flower(void);
void render_minigame_flower(void);
void handle_minigame_flower_input(ALLEGRO_EVENT ev);
void update_minigame_flower(void);

#endif // MINIGAME_FLOWER_H
