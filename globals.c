#include "globals.h" // This will bring in the extern declarations and types

// Allegro 系統全域變數
ALLEGRO_DISPLAY* display = NULL;
ALLEGRO_FONT* font = NULL;
ALLEGRO_EVENT_QUEUE* event_queue = NULL;
ALLEGRO_TIMER* timer = NULL;

// 遊戲物件全域變數
Player player; // Will be initialized by init_player()
Boss bosses[MAX_BOSSES]; // Will be initialized by init_bosses_by_archetype()
Projectile projectiles[MAX_PROJECTILES]; // Will be initialized by init_projectiles()
PlayerKnifeState player_knife; // Will be initialized by init_player_knife()

// 遊戲狀態全域變數
GamePhase game_phase; // Will be set in init_game_systems_and_assets()
Button menu_buttons[3]; // Will be initialized by init_menu_buttons()
Button growth_buttons[MAX_GROWTH_BUTTONS]; // Will be initialized by init_growth_buttons()

// 輸入與攝影機全域變數
bool keys[ALLEGRO_KEY_MAX]; // Will be initialized in init_game_systems_and_assets()
float camera_x = 0, camera_y = 0;

// 圖像資源全域變數
ALLEGRO_BITMAP *background_texture = NULL;
ALLEGRO_BITMAP *player_sprite_asset = NULL;
ALLEGRO_BITMAP *boss_archetype_tank_sprite_asset = NULL;
ALLEGRO_BITMAP *boss_archetype_skillful_sprite_asset = NULL;
ALLEGRO_BITMAP *boss_archetype_berserker_sprite_asset = NULL;
ALLEGRO_BITMAP *knife_sprite_asset = NULL;