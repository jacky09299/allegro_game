#ifndef MINIGAME1_H
#define MINIGAME1_H

#include <allegro5/allegro.h>
#include "types.h" // For Button, GamePhase, ALLEGRO_EVENT

#define NUM_MINIGAME1_BUTTONS 6 // Plant Seed, Start, Restart, Finish, Exit, Harvest

// 初始化、畫面渲染、輸入處理、更新函式宣告
void init_minigame1(void);
void render_minigame1(void);
void handle_minigame1_input(ALLEGRO_EVENT ev);
void update_minigame1(void);

#endif // MINIGAME1_H
