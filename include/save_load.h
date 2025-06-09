#ifndef SAVE_LOAD_H
#define SAVE_LOAD_H

#include <stdint.h> // For uint32_t

// Declare the current version of the save file structure
extern const uint32_t SAVE_FILE_VERSION;

void save_game_progress(const char* filename);
int load_game_progress(const char* filename); // Added return type for status

#endif // SAVE_LOAD_H
