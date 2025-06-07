#ifndef BOSS_H
#define BOSS_H

#include "types.h" // For Boss, BossArchetype
#include <allegro5/allegro.h> // For ALLEGRO_BITMAP, though likely included via types.h

void configure_boss_stats_and_assets(Boss* b, BossArchetype archetype, int difficulty_tier, int boss_id_for_cooldown_randomness);
void init_bosses_by_archetype(void);
void update_boss_character(Boss* b);
void boss_render(Boss* b, float camera_x, float camera_y);

#endif // BOSS_H
