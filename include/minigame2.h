#ifndef MINIGAME2_H
#define MINIGAME2_H

#include <allegro5/allegro.h>
#include "types.h" // For Button, GamePhase, ALLEGRO_EVENT



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
// 初始化、畫面渲染、輸入處理、更新函式宣告
void init_minigame2(void);
void render_minigame2(void);
void handle_minigame2_input(ALLEGRO_EVENT ev);
void update_minigame2(void);

#endif // MINIGAME2_H
