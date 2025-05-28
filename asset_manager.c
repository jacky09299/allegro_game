#include "asset_manager.h"
#include "floor.h" // For destroy_floor_texture
#include <stdio.h>  // For fprintf, stderr, printf (debugging)
#include <string.h> // For strcmp, strdup
#include <stdlib.h> // For free, malloc, realloc (if using dynamic array)
#include <allegro5/allegro_image.h> // Required for al_load_bitmap, al_destroy_bitmap

// Define a structure to hold bitmap info
typedef struct {
    char* path;
    ALLEGRO_BITMAP* bitmap;
} LoadedBitmapInfo;

// Internal storage for loaded bitmaps
// Using a dynamic array for simplicity. A hash map could be more efficient for many assets.
static LoadedBitmapInfo* loaded_bitmaps = NULL;
static int num_loaded_bitmaps = 0;
static int capacity_loaded_bitmaps = 0; // Capacity of the dynamic array

// Maximum number of unique bitmaps expected (for fixed-size array, if preferred over dynamic)
// For dynamic, this can be an initial capacity.
#define INITIAL_BITMAP_CAPACITY 20

void init_asset_manager(void) {
    // Initialize the dynamic array
    loaded_bitmaps = (LoadedBitmapInfo*)malloc(sizeof(LoadedBitmapInfo) * INITIAL_BITMAP_CAPACITY);
    if (!loaded_bitmaps) {
        fprintf(stderr, "AssetManager: Failed to allocate memory for loaded_bitmaps array.\n");
        // Handle error appropriately, maybe exit or set a flag
        num_loaded_bitmaps = 0;
        capacity_loaded_bitmaps = 0;
        return;
    }
    num_loaded_bitmaps = 0;
    capacity_loaded_bitmaps = INITIAL_BITMAP_CAPACITY;
    printf("AssetManager: Initialized.\n");
}

ALLEGRO_BITMAP* load_bitmap_once(const char* path) {
    if (!path) {
        fprintf(stderr, "AssetManager: load_bitmap_once called with NULL path.\n");
        return NULL;
    }

    // Check if already loaded
    for (int i = 0; i < num_loaded_bitmaps; ++i) {
        if (loaded_bitmaps[i].path && strcmp(loaded_bitmaps[i].path, path) == 0) {
            // printf("AssetManager: Returning existing bitmap for %s\n", path);
            return loaded_bitmaps[i].bitmap;
        }
    }

    // Not loaded, try to load it
    ALLEGRO_BITMAP* new_bitmap = al_load_bitmap(path);
    if (!new_bitmap) {
        fprintf(stderr, "AssetManager: Failed to load bitmap: %s\n", path);
        return NULL;
    }
    // printf("AssetManager: Successfully loaded new bitmap: %s\n", path);

    // Add to our list
    // Check if capacity needs to be increased
    if (num_loaded_bitmaps >= capacity_loaded_bitmaps) {
        int new_capacity = capacity_loaded_bitmaps == 0 ? INITIAL_BITMAP_CAPACITY : capacity_loaded_bitmaps * 2;
        LoadedBitmapInfo* new_array = (LoadedBitmapInfo*)realloc(loaded_bitmaps, sizeof(LoadedBitmapInfo) * new_capacity);
        if (!new_array) {
            fprintf(stderr, "AssetManager: Failed to reallocate memory for loaded_bitmaps array. Cannot store new bitmap: %s\n", path);
            al_destroy_bitmap(new_bitmap); // Clean up the newly loaded bitmap as we can't store it
            return NULL; // Or return the bitmap but warn it's not managed
        }
        loaded_bitmaps = new_array;
        capacity_loaded_bitmaps = new_capacity;
        // printf("AssetManager: Increased capacity to %d\n", new_capacity);
    }

    // Store the new bitmap info
    // strdup allocates memory for the path string, which needs to be freed later
    char* path_copy = strdup(path);
    if (!path_copy) {
        fprintf(stderr, "AssetManager: Failed to duplicate path string for %s. Cannot store new bitmap.\n", path);
        al_destroy_bitmap(new_bitmap);
        return NULL;
    }

    loaded_bitmaps[num_loaded_bitmaps].path = path_copy;
    loaded_bitmaps[num_loaded_bitmaps].bitmap = new_bitmap;
    num_loaded_bitmaps++;

    return new_bitmap;
}

void destroy_all_loaded_bitmaps(void) {
    if (!loaded_bitmaps) return;

    // printf("AssetManager: Destroying %d loaded bitmaps...\n", num_loaded_bitmaps);
    for (int i = 0; i < num_loaded_bitmaps; ++i) {
        if (loaded_bitmaps[i].bitmap) {
            al_destroy_bitmap(loaded_bitmaps[i].bitmap);
            loaded_bitmaps[i].bitmap = NULL; // Good practice
        }
        if (loaded_bitmaps[i].path) {
            free(loaded_bitmaps[i].path); // Free the duplicated path string
            loaded_bitmaps[i].path = NULL;   // Good practice
        }
    }
    free(loaded_bitmaps);
    loaded_bitmaps = NULL;
    num_loaded_bitmaps = 0;
    capacity_loaded_bitmaps = 0;
    printf("AssetManager: All bitmaps destroyed and memory freed.\n");
}

void shutdown_asset_manager(void) {
    destroy_all_loaded_bitmaps();
    destroy_floor_texture();      // New call
    printf("AssetManager: Shutdown complete (including floor).\n"); // Modified log
}
