#include "globals.h" 

// Allegro 系統全域變數
ALLEGRO_DISPLAY* display = NULL;
ALLEGRO_FONT* font = NULL;
ALLEGRO_EVENT_QUEUE* event_queue = NULL;
ALLEGRO_TIMER* timer = NULL;

//養成階段提示文字系統全域變數
char growth_message[128] = "";
int growth_message_timer = 0;

// 遊戲物件全域變數
Player player; 
Boss bosses[MAX_BOSSES]; 
BossArchetype boss_spawn_queue[BOSS_POOL_SIZE];
int boss_queue_index = BOSS_POOL_SIZE; // 表示目前池已空，下次要重洗
Projectile projectiles[MAX_PROJECTILES];
PlayerKnifeState player_knife; 
EscapeGate escape_gate;
float battle_speed_multiplier = 1.0f;
Effect effects[MAX_EFFECTS];

// 遊戲狀態全域變數
GamePhase game_phase; 
Button menu_buttons[3]; 
Button growth_buttons[MAX_GROWTH_BUTTONS]; 

int current_day = 1;
int day_time = 1;
double battle_time;

// 輸入與攝影機全域變數
bool keys[ALLEGRO_KEY_MAX]; // Will be initialized in init_game_systems_and_assets()
float camera_x = 0, camera_y = 0;

// 圖像資源全域變數
ALLEGRO_BITMAP *player_sprite_asset = NULL;
ALLEGRO_BITMAP *boss_archetype_tank_sprite_asset = NULL;
ALLEGRO_BITMAP *boss_archetype_skillful_sprite_asset = NULL;
ALLEGRO_BITMAP *boss_archetype_berserker_sprite_asset = NULL;
ALLEGRO_BITMAP *knife_sprite_asset = NULL;

LotteryItemDefinition lottery_prize_pool[MAX_LOTTERY_PRIZES];
BackpackSlot player_backpack[MAX_BACKPACK_SLOTS];
int backpack_item_count = 0; // 初始化為0