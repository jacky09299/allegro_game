#ifndef EFFECT_H
#define EFFECT_H

#include "types.h" // For Boss struct

// Function declaration for effects
void spawn_warning_circle(float x, float y, float radius, ALLEGRO_COLOR color, int duration);
void spawn_claw_slash(float x, float y, float angle);
void spawn_floating_text(float x, float y, const char* content, ALLEGRO_COLOR color);
void spawn_curse_link(float x, float y, int duration);
void update_effects();
void render_effects_back();
void render_effects_front(); 

#endif // BOSS_ATTACK_H
