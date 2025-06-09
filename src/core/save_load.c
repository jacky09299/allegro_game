#include "save_load.h"
#include <stdio.h>
#include <string.h> // For memcpy if needed, though direct assignment is used
#include "globals.h" // For accessing global game state variables
#include "types.h"   // For Player, Boss, etc. definitions

// Define the current version of the save file structure
const uint32_t SAVE_FILE_VERSION = 1;

void save_game_progress(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (f == NULL) {
        fprintf(stderr, "Error opening file for saving: %s\n", filename);
        // In a real game, might set a global error flag or message
        return;
    }

    // Write the version number first
    if (fwrite(&SAVE_FILE_VERSION, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "Error writing save file version to %s\n", filename);
        fclose(f);
        return;
    }

    // 1. Player player
    Player player_to_save = player;
    player_to_save.sprite = NULL;
    // Ensure all skills' potentially dynamic data is handled if any (currently skills are structs)
    fwrite(&player_to_save, sizeof(Player), 1, f);

    // 2. Boss bosses[MAX_BOSSES]
    Boss bosses_to_save[MAX_BOSSES];
    for (int i = 0; i < MAX_BOSSES; ++i) {
        bosses_to_save[i] = bosses[i];
        bosses_to_save[i].sprite_asset = NULL;
        // Ensure all skills' potentially dynamic data is handled
    }
    fwrite(bosses_to_save, sizeof(Boss), MAX_BOSSES, f);

    // 3. BossArchetype boss_spawn_queue[MAX_BOSSES]
    fwrite(boss_spawn_queue, sizeof(BossArchetype), MAX_BOSSES, f);

    // 4. int boss_queue_index
    fwrite(&boss_queue_index, sizeof(int), 1, f);

    // 5. BackpackSlot player_backpack[MAX_BACKPACK_SLOTS]
    BackpackSlot backpack_to_save[MAX_BACKPACK_SLOTS];
    for (int i = 0; i < MAX_BACKPACK_SLOTS; ++i) {
        backpack_to_save[i] = player_backpack[i];
        if (backpack_to_save[i].item.image != NULL) { // Only nullify if it was set
            backpack_to_save[i].item.image = NULL;
        }
        // image_path (const char*) in LotteryItemDefinition is assumed to be a string literal
        // or managed elsewhere if dynamic. If it's just a path, it's fine.
    }
    fwrite(backpack_to_save, sizeof(BackpackSlot), MAX_BACKPACK_SLOTS, f);

    // 6. int backpack_item_count
    fwrite(&backpack_item_count, sizeof(int), 1, f);

    // 7. int current_day
    fwrite(&current_day, sizeof(int), 1, f);

    // 8. int day_time
    fwrite(&day_time, sizeof(int), 1, f);

    // 9. double battle_time
    fwrite(&battle_time, sizeof(double), 1, f);

    // 10. EscapeGate escape_gate
    fwrite(&escape_gate, sizeof(EscapeGate), 1, f);

    // 11. GamePhase game_phase
    fwrite(&game_phase, sizeof(GamePhase), 1, f);

    // 12. Player global player stats from player.c
    // This was a misinterpretation in planning. 'player' is the global Player struct.
    // Other stats like money, hp are members of 'player'.

    if (ferror(f)) {
        fprintf(stderr, "Error writing to save file: %s\n", filename);
    }

    fclose(f);
    // For debugging, a message can be useful, but avoid stdout in library-like code
    // printf("Game progress saved to %s.\n", filename);
}

int load_game_progress(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        // This is not necessarily an error, just means no save file exists.
        // The game can start fresh.
        // A message to stderr could be uncommented for debugging if needed.
        // fprintf(stderr, "Info: No save file found: %s. Starting a new game.\n", filename);
        return 0; // 0 indicates no save file or failure to load
    }

    uint32_t file_version;
    size_t read_count; // Already exists in current load_game_progress
    bool load_error_occurred = false; // Keep this for subsequent LOAD_DATA checks

    // Read the version number first
    read_count = fread(&file_version, sizeof(uint32_t), 1, f);
    if (read_count != 1) {
        fprintf(stderr, "Error: Could not read save file version from %s. File might be too short or corrupted.\n", filename);
        fclose(f);
        return 0; // Indicate failure
    }

    if (file_version != SAVE_FILE_VERSION) {
        fprintf(stderr, "Error: Incompatible save file version in %s. Expected version %u, but found version %u.\n",
                filename, SAVE_FILE_VERSION, file_version);
        fclose(f);
        return 0; // Indicate failure (version mismatch)
    }
    // If version matches, proceed to load the rest...

    // Helper macro to reduce boilerplate for fread checks
    #define LOAD_DATA(ptr, size, count) \
        if (!load_error_occurred) { \
            read_count = fread(ptr, size, count, f); \
            if (read_count != (count)) { \
                load_error_occurred = true; \
            } \
        }

    // 1. Player player
    LOAD_DATA(&player, sizeof(Player), 1);
    // Player sprites and other non-serialized assets will be NULL or default.
    // They should be re-initialized by asset loading functions after successful load.

    // 2. Boss bosses[MAX_BOSSES]
    LOAD_DATA(bosses, sizeof(Boss), MAX_BOSSES);
    // Boss sprites will be NULL.

    // 3. BossArchetype boss_spawn_queue[MAX_BOSSES]
    LOAD_DATA(boss_spawn_queue, sizeof(BossArchetype), MAX_BOSSES);

    // 4. int boss_queue_index
    LOAD_DATA(&boss_queue_index, sizeof(int), 1);

    // 5. BackpackSlot player_backpack[MAX_BACKPACK_SLOTS]
    LOAD_DATA(player_backpack, sizeof(BackpackSlot), MAX_BACKPACK_SLOTS);
    // Backpack item images will be NULL.

    // 6. int backpack_item_count
    LOAD_DATA(&backpack_item_count, sizeof(int), 1);

    // 7. int current_day
    LOAD_DATA(&current_day, sizeof(int), 1);

    // 8. int day_time
    LOAD_DATA(&day_time, sizeof(int), 1);

    // 9. double battle_time
    LOAD_DATA(&battle_time, sizeof(double), 1);

    // 10. EscapeGate escape_gate
    LOAD_DATA(&escape_gate, sizeof(EscapeGate), 1);

    // 11. GamePhase game_phase
    LOAD_DATA(&game_phase, sizeof(GamePhase), 1);

    // Check for read errors after all attempts
    if (ferror(f) || load_error_occurred) {
        fprintf(stderr, "Error: Save file format error or incomplete data in %s.\n", filename);
        fclose(f);
        // To prevent using partially loaded data, it might be best to trigger a clean game init here.
        // For now, returning 0 will signal main to init fresh.
        return 0; // Indicate failure
    }

    fclose(f);
    // For debugging:
    // printf("Game progress loaded from %s.\n", filename);
    // After successful load, ensure assets are consistent with loaded state.
    // For example, player.sprite and boss[i].sprite_asset are NULL here.
    // The existing init_asset_manager(), init_player(), init_bosses_by_archetype()
    // might need to be called again or modified to handle this.
    // This will be addressed in the integration step.
    return 1; // 1 indicates success
}
