#ifndef GLOBALS_H
#define GLOBALS_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include "types.h" // For Player, Boss, Projectile, GamePhase, Button, PlayerKnifeState, etc.
#include "config.h" // For MAX_BOSSES, MAX_PROJECTILES, ALLEGRO_KEY_MAX, MAX_GROWTH_BUTTONS

// Allegro 系統全域變數
extern ALLEGRO_DISPLAY* display;
extern ALLEGRO_FONT* font;
extern ALLEGRO_EVENT_QUEUE* event_queue;
extern ALLEGRO_TIMER* timer;

//養成階段提示文字系統全域變數
extern char growth_message[128];
extern int growth_message_timer;

// 遊戲物件全域變數
extern Player player;
extern Boss bosses[MAX_BOSSES];
extern BossArchetype boss_spawn_queue[MAX_BOSSES];
extern int boss_queue_index;
extern Projectile projectiles[MAX_PROJECTILES];
extern PlayerKnifeState player_knife;
extern EscapeGate escape_gate;
extern float battle_speed_multiplier;
extern Effect effects[MAX_EFFECTS];

// 遊戲狀態全域變數
extern GamePhase game_phase;
extern Button menu_buttons[3]; // 3 is based on original code
extern Button growth_buttons[MAX_GROWTH_BUTTONS];

extern int current_day;
extern int day_time;
extern double battle_time;

// 輸入與攝影機全域變數
extern bool keys[ALLEGRO_KEY_MAX];
extern float camera_x, camera_y;

// 圖像資源全域變數
extern ALLEGRO_BITMAP *player_sprite_asset;
extern ALLEGRO_BITMAP *boss_archetype_tank_sprite_asset;
extern ALLEGRO_BITMAP *boss_archetype_skillful_sprite_asset;
extern ALLEGRO_BITMAP *boss_archetype_berserker_sprite_asset;
extern ALLEGRO_BITMAP *knife_sprite_asset;

extern LotteryItemDefinition lottery_prize_pool[MAX_LOTTERY_PRIZES]; // 抽獎獎池
extern BackpackSlot player_backpack[MAX_BACKPACK_SLOTS];             // 玩家背包
extern int backpack_item_count;

#endif // GLOBALS_H