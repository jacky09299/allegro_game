#ifndef BACKPACK_H
#define BACKPACK_H

#include <allegro5/allegro.h> // For ALLEGRO_EVENT

// Forward declaration if needed, or include relevant headers like "types.h" for GamePhase
// For now, we only need ALLEGRO_EVENT for handle_backpack_screen_input.
// render_backpack_screen will use globals like font, player, etc., which are typically available via "globals.h"
// but "globals.h" itself might include "types.h".
// Let's assume "game_state.h" or "main.c" will include "globals.h" before "backpack.h" if it's not included here.

/**
 * @brief Renders the backpack screen.
 */
void render_backpack_screen(void);

/**
 * @brief Handles input events for the backpack screen.
 * @param ev The Allegro event to process.
 */
void handle_backpack_screen_input(ALLEGRO_EVENT ev);

#endif // BACKPACK_H
