#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <allegro5/allegro.h>
//#include <allegro5/allegro_event.h> // For ALLEGRO_EVENT
#include "types.h" // For Player, PlayerSkillIdentifier, PlayerKnifeState

void init_player(void);
void update_player_character(void);

void update_game_camera(void); // 相機永遠在角色上方，玩家會固定在視窗中間

// New rendering function
void player_render(Player* p, float camera_x, float camera_y);

// New consolidated input handling function
void player_handle_input(Player* p, ALLEGRO_EVENT* ev, bool keys[]);

#endif // PLAYER_H
