#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <allegro5/allegro.h> // For ALLEGRO_COLOR
#include "config.h" // For MAX_PLAYER_SKILLS, MAX_BOSSES

#define MAX_LOTTERY_PRIZES 5 // 我們有五種抽獎獎品
#define MAX_BACKPACK_SLOTS 10 // 背包中最多可存放的不同物品種類

#define MAX_WARNING_CIRCLES 10 // 最多警示圈上限

#define MAX_FLOATING_TEXT 50 // 漂浮文字上限

#define MAX_EFFECTS 100

// 遊戲階段枚舉
typedef enum {
    MENU,       // 主選單階段
    EXIT,       // 退出遊戲
    GROWTH,     // 養成階段
    MINIGAME1,  // 小遊戲1
    MINIGAME2,  // 小遊戲2
    LOTTERY,      // 抽獎
    BACKPACK,   // 背包
    BATTLE,      // 戰鬥階段
    TUTORIAL,    // 教學階段
    EQUIPMENT // 裝備階段
    
} GamePhase;

#define MAX_SKILLS 21
#define MAX_BOSS_SKILLS 3                   // BOSS 技能數量上限
#define MAX_BOSS_PRIMARY 2
#define MAX_PLAYER_SKILLS 16                 // 玩家最大技能數量
// 技能標識符枚舉
typedef enum {
    SKILL_NONE,             // 無技能
    SKILL_LIGHTNING_BOLT,   // 閃電鏈
    SKILL_HEAL,             // 治療術
    SKILL_ELEMENT_BALL,     // 元素彈

    SKILL_ELEMENT_DASH,
    SKILL_CHARGE_BEAM,
    SKILL_FOCUS,
    SKILL_ARCANE_ORB,
    SKILL_RUNE_IMPLOSION,
    SKILL_REFLECT_BARRIER,

    SKILL_ELEMENTAL_SCATTER,
    SKILL_ELEMENTAL_BLAST,
    SKILL_BIG_HEAL,
    SKILL_RAPID_SHOOT,

    SKILL_ELEMENTAL_COUNTER, // 元素反擊 new
    SKILL_PREFECT_DEFENSE,   // 完美防禦

    BOSS_SKILL_1,  // Boss 特殊技能 1
    BOSS_SKILL_2,  // Boss 特殊技能 2
    BOSS_SKILL_3,  // Boss 特殊技能 3
    BOSS_RANGE_PRIMARY, // Boss 遠程基礎技能
    BOSS_MELEE_PRIMARY  // Boss 近戰基礎技能
} SkillIdentifier;

// BOSS狀態枚舉
typedef enum{
    BOSS_ABILITY_MELEE_PRIMARY, // Boss 近戰攻擊
    BOSS_ABILITY_RANGED_PRIMARY, // Boss 遠程攻擊
    BOSS_ABILITY_SKILL,  // Boss 特殊技能
}BossAbilityIdentifier;

#define MAX_STATE 6
// 玩家狀態枚舉
typedef enum {
    STATE_NORMAL,             // 一般狀態
    STATE_STUN,               // 暈眩狀態
    STATE_SLOWNESS,           // 緩速狀態
    STATE_DEFENSE,            // 防禦狀態
    STATE_CHARGING,           // 充能狀態
    STATE_USING_SKILL         // 充能狀態
} PlayerStateIdentifier;

// 玩家背包裡的道具
typedef enum {
    ITEM_HEALTH_POTION,
    ITEM_BOMB
} ItemType;


// Boss 原型枚舉
typedef enum {
    BOSS_TYPE_TANK,         // 坦克型 Boss
    BOSS_TYPE_SKILLFUL,     // 技巧型 Boss
    BOSS_TYPE_BERSERKER     // 狂戰型 Boss
} BossArchetype;

// 投射物擁有者枚舉
typedef enum {
    OWNER_PLAYER,           // 投射物屬於玩家
    OWNER_BOSS              // 投射物屬於 Boss
} ProjectileOwner;

// 投射物類型枚舉
typedef enum {
    PROJ_TYPE_GENERIC,          // 通用類型 (預設或未使用)
    PROJ_TYPE_NONE,             // 無屬性投射物
    PROJ_TYPE_WATER,            // 水系投射物
    PROJ_TYPE_FIRE,             // 火系投射物 (通常為 Boss)
    PROJ_TYPE_ICE,              // 冰系投射物
    PROJ_TYPE_PLAYER_FIREBALL,   // 玩家火球術
    PROJ_TYPE_BIG_EARTHBALL,     // 大土球
    PROJ_TYPE_DASH               // 元素衝刺
} ProjectileType;

// 戰鬥特效枚舉
typedef enum {
    EFFECT_FLOATING_TEXT,
    EFFECT_WARNING_CIRCLE,
    EFFECT_CLAW_SLASH,
    EFFECT_EXPLOSION,
    EFFECT_CURSE_LINK,
    EFFECT_DASH_TRAIL,
    EFFECT_CHARGE_BEAM_RING,
    EFFECT_FOCUS_AURA,
    EFFECT_ORB_SUMMON_GLOW,
    EFFECT_RUNE_CIRCLE
    // ... 其他類型
} EffectType;

// 投射物結構
typedef struct {
    float x, y;                 // 投射物當前位置 (x, y 座標)
    float v_x, v_y;             // 投射物速度向量 (x, y 分量)
    int radius;                 // 投射物半徑
    bool active;                // 投射物是否啟用 (是否在畫面中移動和碰撞)
    ProjectileOwner owner;      // 投射物擁有者 (玩家或 Boss)
    ProjectileType type;        // 投射物類型 (影響外觀、效果等)
    int damage;                 // 投射物造成的傷害值
    int lifespan;               // 投射物剩餘壽命 (幀數)，為0時消失
    int owner_id;               // 若擁有者是 Boss，則為 Boss 的 ID
} Projectile;

// SKILL structure
typedef struct {
    SkillIdentifier type;
    bool learned;
    int cooldown_timers;
    int duration_timers;
    int charge_timers;
    int variable_1;
    float x, y;
    //SKILL* next;
} SKILL;

// 玩家結構
typedef struct {
    int hp;                     // 當前生命值
    int max_hp;                 // 最大生命值
    int mp;                     // 當前魔力值
    int max_mp;                 // 最大魔力值
    int strength;               // 力量 (影響物理傷害)
    int magic;                  // 魔力 (影響技能傷害或效果)
    float max_speed;            // 最大移動速度
    float speed;                // 當前移動速度
    int money;                  // 金錢
    float x, y;                 // 玩家當前位置 (x, y 座標)
    float v_x, v_y;             // 玩家當前速度向量 (用於移動計算)
    float facing_angle;         // 玩家面朝角度 (弧度)
    int defense_timer;           // 玩家防禦時間計時器 (幀)
    PlayerStateIdentifier state;    // 玩家當前狀態
    int effect_timers[MAX_STATE];
    SKILL learned_skills[MAX_SKILLS]; // 已學習的技能列表
    //SKILL* current_skill;
    int current_skill_index;
    int normal_attack_cooldown_timer; // 普通攻擊冷卻計時器
    int item_quantities[NUM_ITEMS]; //記錄每種道具的數量
    ALLEGRO_BITMAP* sprite; // Player's sprite
} Player;

// Boss 結構
typedef struct {
    int id;                     // Boss 的唯一標識符
    BossArchetype archetype;    // Boss 的原型 (坦克、技巧、狂戰)
    int hp;                     // 當前生命值
    int max_hp;                 // 最大生命值
    int strength;               // 力量 (影響物理傷害)
    BossAbilityIdentifier current_ability_in_use; // 當前正在使用的技能
    int defense;                // 防禦力 (減免受到的物理傷害)
    int magic;                  // 魔力 (影響技能傷害或效果)
    float speed;                // 移動速度
    float x, y;                 // Boss 當前位置 (x, y 座標)
    float v_x, v_y;             // Boss 當前速度向量 (用於移動計算)
    float facing_angle;         // Boss 面朝角度 (弧度)
    // int ranged_special_ability_cooldown_timer; // 遠程特殊技能冷卻計時器 (幀)
    // int melee_primary_ability_cooldown_timer;  // 近戰主技能冷卻計時器 (幀)
    SKILL learned_skills[MAX_SKILLS];
    ProjectileType ranged_special_projectile_type; // 遠程特殊技能的投射物類型
    bool is_alive;              // Boss 是否存活
    bool is_dashing;            // Boss 是否在衝刺
    ALLEGRO_BITMAP *sprite_asset; // Boss 的圖像資源
    float target_display_width; // Boss 圖像的目標顯示寬度
    float target_display_height;// Boss 圖像的目標顯示高度
    float collision_radius;     // Boss 的碰撞半徑
} Boss;

// 按鈕結構 (用於主選單和養成畫面)
typedef struct {
    float x, y, width, height;  // 按鈕的位置和尺寸
    const char* text;           // 按鈕上顯示的文字
    GamePhase action_phase;     // 按下按鈕後轉換到的遊戲階段 (或用於標識按鈕類型)
    ALLEGRO_COLOR color;        // 按鈕正常狀態顏色
    ALLEGRO_COLOR hover_color;  // 按鈕滑鼠懸停時顏色
    ALLEGRO_COLOR text_color;   // 按鈕文字顏色
    bool is_hovered;            // 滑鼠是否懸停在按鈕上
} Button;

// 玩家刀子攻擊狀態結構
typedef struct {
    bool active;                // 是否正在進行攻擊
    float x, y;                 // 刀子目前在世界座標系中的位置
    float angle;                // 刀子目前的旋轉角度 (世界座標系)
    float path_progress_timer;  // 路徑進度計時器 (0 到 KNIFE_ATTACK_DURATION)
    
    // 攻擊發起時玩家的狀態，用於計算路徑的基準點
    float owner_start_x;
    float owner_start_y;
    float owner_start_facing_angle;

    // 追蹤在此次揮砍中已擊中的 Boss，避免重複傷害
    bool hit_bosses_this_swing[MAX_BOSSES]; 
    ALLEGRO_BITMAP* sprite; // <-- NEW FIELD
} PlayerKnifeState;

//逃跑門結構
typedef struct {
    float x, y;
    float width, height;
    bool is_active;
    bool is_counting_down;
    int countdown_frames; // 倒數用的 frame 數
} EscapeGate;

// 
typedef struct {
    int id;                         // 物品的唯一ID
    char name[50];                  // 物品名稱
    ALLEGRO_BITMAP *image;          // 物品圖片
    const char* image_path;         // 圖片檔案路徑 (用於載入)
} LotteryItemDefinition;

// 物品格結構
typedef struct {
    LotteryItemDefinition item;     // 槽位中的物品定義
    int quantity;                   // 該物品的數量
} BackpackSlot;

// 戰鬥特效結構
typedef struct {
    EffectType type;
    float x, y;
    float vx, vy;

    int timer;
    bool active;

    // 通用屬性
    float radius;
    float angle;
    ALLEGRO_COLOR color;
    char text[32]; // 用於漂字類
    float alpha;

    // 進階：可加 union 或 void* userdata 擴充
} Effect;

typedef struct {
    int songs_sung;
    int growth_stage; // 0: seed, 1-7: growing stages, 8: flowered
    int which_flower;
} MinigameFlowerPlant;

typedef struct {
    int id;
    bool is_adult;
    bool is_selected_for_queue; // 是否被選中準備加入繁殖佇列
    double birth_time;
    double last_reproduction_time;
    float quality;
    float happiness; // 快樂值，基於父母的繁殖時間計算
    char name[32]; // 姓名，使用字元陣列以便於分配和顯示
    // For rendering and selection
    float x, y;
    float vx, vy; // 速度向量，未使用但保留以便未來擴展
    float ax, ay; // 加速度向量，未使用但保留以便未來擴展
    bool is_selected;
} Person;

#endif // TYPES_H