#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h> // For al_load_bitmap related functions

// Initializes the asset manager. Call this once at the start.
// (May not be strictly necessary if using static initialization for the manager's internal state,
// but good practice for explicit control).
void init_asset_manager(void);

// Loads a bitmap from the given path.
// If the bitmap has already been loaded, returns a pointer to the existing bitmap.
// Otherwise, loads the bitmap, stores it for future requests, and returns a pointer to it.
// Returns NULL if loading fails.
ALLEGRO_BITMAP* load_bitmap_once(const char* path);

// Destroys all bitmaps loaded by the asset manager.
// Call this once at the end of the game to free resources.
void destroy_all_loaded_bitmaps(void);

// Alias for destroy_all_loaded_bitmaps for symmetry with init.
void shutdown_asset_manager(void);

#endif // ASSET_MANAGER_H
