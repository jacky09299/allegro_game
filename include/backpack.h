#ifndef BACKPACK_H
#define BACKPACK_H

#include <allegro5/allegro.h> // For ALLEGRO_EVENT

void init_backpack(void);
void render_backpack(void);
void handle_backpack_input(ALLEGRO_EVENT ev);
void update_backpack(void);
void add_item_to_backpack(LotteryItemDefinition item_to_add);

#endif // BACKPACK_H
