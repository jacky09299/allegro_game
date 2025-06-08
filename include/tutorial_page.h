#ifndef TUTORIAL_PAGE_H
#define TUTORIAL_PAGE_H

#include <allegro5/allegro.h>


// 宣告給 main.c 使用的四個主要函式
void init_tutorial_page(void);
void render_tutorial_page(void);
void handle_tutorial_page_input(ALLEGRO_EVENT ev);
void update_tutorial_page(void);

#endif // TUTORIAL_PAGE_H