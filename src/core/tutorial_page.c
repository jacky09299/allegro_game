#include <stdio.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>

// 包含你的專案標頭檔以使用全域變數和設定
#include "config.h"
#include "types.h"
#include "globals.h"
#include "tutorial_page.h" // 包含自己的標頭檔

#define TOTAL_PAGES 5
/*
game_state.c要改成
menu_buttons[1] = (Button){
        center_x - button_width / 2, SCREEN_HEIGHT / 2.0f,
        button_width, button_height, "遊戲手冊", TUTORIAL,
        al_map_rgb(70, 170, 70), al_map_rgb(100, 220, 100), al_map_rgb(255, 255, 255), false
    };
main.c加入
標頭檔加入-> #include "include/tutorial_page.h"
init_game_systems_and_assets()加入-> init_tutorial_page();
處理input的switch加入-> case TUTORIAL: handle_tutorial_page_input(ev); break;
處理render的switch加入-> case TUTORIAL: render_tutorial_page(); break;

*/


// 資源 (只為標題建立一個新字型)
static ALLEGRO_FONT* font_tutorial_title = NULL;

// 狀態
static int current_page = 0;

// 頁面內容
static const char* page_titles[TOTAL_PAGES];
static const char* page_contents[TOTAL_PAGES];

// UI 按鈕與互動 (這些是教學頁面內部的按鈕，與主選單無關)
typedef struct {
    float x, y, w, h;
    const char* text;
    bool is_hovered;
} TutorialButton;

static TutorialButton btn_prev, btn_next, btn_back;

// 檢查滑鼠是否在矩形區域內
static bool is_mouse_in_rect(float mx, float my, float x, float y, float w, float h) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

// --- 主要函式 ---

void init_tutorial_page(void) {
    printf("Initializing tutorial page...\n");

    // 1. 載入標題字型 (內文將使用你已載入的全域 font)
    font_tutorial_title = al_load_ttf_font("assets/font/JasonHandwriting3.ttf", 48, 0); 
    if (!font_tutorial_title) {
        fprintf(stderr, "Could not load title font for tutorial page.\n");
    }

    // 2. 初始化狀態
    current_page = 0;

    // 3. 設定每一頁的內容
    page_titles[0] = "遊戲簡介與流程";
    page_contents[0] = 
        "【遊戲簡介】\n"
        "本遊戲是一款「養成 × 戰鬥」模擬遊戲。玩家將體驗養成族群、\n"
        "資源管理、個體繁殖，並於回合後期進入動作戰鬥關卡，\n"
        "以養成成果挑戰強力 Boss。\n\n"
        "【遊戲流程】\n"
        "1. 啟動遊戲 → 進入主選單\n"
        "2. 養成階段 (早/中/晚) → 進行資源收集、繁殖等\n"
        "3. 戰鬥階段 → 操作角色挑戰 Boss\n"
        "4. 戰鬥結束 → 返回下一天的養成階段";
    page_titles[1] = "玩法詳解 - 養成階段";
    page_contents[1] =
        "【族群管理】\n"
        "- 每個個體擁有品質值、快樂值、成年狀態等屬性。\n"
        "- 透過繁殖產生新成員，品質值會依據父母屬性影響。\n"
        "- 個體有成長時間、繁殖冷卻等機制。\n\n"
        "【資源與道具】\n"
        "- 可參加抽獎小遊戲獲得隨機道具或金幣。\n"
        "- 按下 B 鍵可開關背包，查看已獲得的道具。\n\n"
        "【小遊戲】\n"
        "- 包含種花、抽獎、管理人口等。\n"
        "- 介面皆有明確按鈕提示，操作直覺。";
    page_titles[2] = "玩法詳解 - 戰鬥階段";
    page_contents[2] =
        "【玩家操作】\n"
        "- 使用鍵盤方向鍵移動角色。\n"
        "- 按下攻擊鍵 (如左鍵/空白鍵) 發動攻擊。\n"
        "- 按 B 切換背包，ESC 暫停戰鬥。\n\n"
        "【戰鬥目標】\n"
        "- 擊敗 Boss 取得獎勵，或從逃脫門離開。\n"
        "- 戰場上會出現多種投射物與敵人彈幕，需靈活閃避。\n\n"
        "【Boss 系統】\n"
        "- 不同 Boss 擁有獨立血量、攻擊模式與移動邏輯。\n"
        "- 隨天數推進 Boss 強度會提升。";
    page_titles[3] = "操作說明";
    page_contents[3] =
        "【主選單】\n"
        "  ↑/↓：移動選項\n"
        "  Enter：確認選擇\n\n"
        "【遊戲內通用】\n"
        "  ↑/↓/←/→：移動角色\n"
        "  Space/滑鼠左鍵：攻擊或互動\n"
        "  B：開啟/關閉背包\n"
        "  ESC：暫停/返回上層\n\n"
        "【養成/小遊戲】\n"
        "  主要使用滑鼠點擊畫面上的按鈕進行操作。";
    page_titles[4] = "進階提示與結語";
    page_contents[4] =
        "【進階提示】\n"
        "- 合理安排繁殖與資源分配，提升族群品質。\n"
        "- 道具可在戰鬥時使用，能大幅提升生存能力。\n"
        "- 善用每日三時段的小遊戲，以獲得隱藏獎勵。\n"
        "- 戰鬥失敗可重試，也可策略性撤退以保存實力。\n\n"
        "【結語】\n"
        "祝您遊戲愉快，挑戰極限！";

    // 4. 設定教學頁面內部導覽按鈕的位置和文字
    btn_prev = (TutorialButton){100, SCREEN_HEIGHT - 80, 150, 50, "[ 上一頁 ]", false};
    btn_next = (TutorialButton){SCREEN_WIDTH - 250, SCREEN_HEIGHT - 80, 150, 50, "[ 下一頁 ]", false};
    btn_back = (TutorialButton){SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 80, 200, 50, "[ 返回主選單 ]", false};
}

void render_tutorial_page(void) {
    al_clear_to_color(al_map_rgb(20, 20, 40));

    // 繪製標題
    al_draw_text(font_tutorial_title, al_map_rgb(255, 255, 0), SCREEN_WIDTH / 2, 30, ALLEGRO_ALIGN_CENTER, page_titles[current_page]);
    al_draw_line(50, 100, SCREEN_WIDTH - 50, 100, al_map_rgb(100, 100, 120), 2);
    
    // 使用全域的 `font` 來繪製內文 (調整行高以匹配你的字型)
    al_draw_multiline_text(font, al_map_rgb(230, 230, 230), 80, 140, SCREEN_WIDTH - 160, al_get_font_line_height(font) + 8, 0, page_contents[current_page]);

    // 繪製頁碼指示
    char page_str[16];
    snprintf(page_str, 16, "第 %d / %d 頁", current_page + 1, TOTAL_PAGES);
    al_draw_text(font, al_map_rgb(150, 150, 150), SCREEN_WIDTH / 2, SCREEN_HEIGHT - 120, ALLEGRO_ALIGN_CENTER, page_str);

    // 繪製導覽按鈕
    TutorialButton* buttons[] = {&btn_prev, &btn_next, &btn_back};
    for (int i = 0; i < 3; ++i) {
        TutorialButton* btn = buttons[i];
        if ((i == 0 && current_page == 0) || (i == 1 && current_page == TOTAL_PAGES - 1)) continue;

        ALLEGRO_COLOR bg_color = btn->is_hovered ? al_map_rgb(100, 100, 180) : al_map_rgb(50, 50, 90);
        al_draw_filled_rectangle(btn->x, btn->y, btn->x + btn->w, btn->y + btn->h, bg_color);
        al_draw_rectangle(btn->x, btn->y, btn->x + btn->w, btn->y + btn->h, al_map_rgb(150, 150, 220), 2.0);
        al_draw_text(font, al_map_rgb(255, 255, 255), btn->x + btn->w / 2, btn->y + (btn->h - al_get_font_ascent(font)) / 2, ALLEGRO_ALIGN_CENTER, btn->text);
    }
}

void handle_tutorial_page_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        switch (ev.keyboard.keycode) {
            case ALLEGRO_KEY_LEFT:
                if (current_page > 0) current_page--;
                break;
            case ALLEGRO_KEY_RIGHT:
                if (current_page < TOTAL_PAGES - 1) current_page++;
                break;
            case ALLEGRO_KEY_ESCAPE:
                game_phase = MENU; // 直接修改全域遊戲狀態以返回主選單
                // 重置主選單按鈕的懸停狀態，避免返回時按鈕仍是高亮
                for (int i = 0; i < 3; ++i) menu_buttons[i].is_hovered = false;
                break;
        }
    } 
    else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
        float mx = ev.mouse.x;
        float my = ev.mouse.y;
        btn_prev.is_hovered = is_mouse_in_rect(mx, my, btn_prev.x, btn_prev.y, btn_prev.w, btn_prev.h);
        btn_next.is_hovered = is_mouse_in_rect(mx, my, btn_next.x, btn_next.y, btn_next.w, btn_next.h);
        btn_back.is_hovered = is_mouse_in_rect(mx, my, btn_back.x, btn_back.y, btn_back.w, btn_back.h);
    }
    else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev.mouse.button == 1) { // 左鍵點擊
            if (btn_prev.is_hovered && current_page > 0) current_page--;
            if (btn_next.is_hovered && current_page < TOTAL_PAGES - 1) current_page++;
            if (btn_back.is_hovered) {
                game_phase = MENU; // 直接修改全域遊戲狀態
                for (int i = 0; i < 3; ++i) menu_buttons[i].is_hovered = false;
            }
        }
    }
}