#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro.h>
#include "globals.h"
#include "lottery.h"
#include "types.h"
#include "backpack.h"
#include "asset_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

// --- 定義抽獎費用 ---
#define LOTTERY_COST 300

// --- 抽獎界面元素 ---
static Button draw_button_ui; // UI 按鈕
static Button open_backpack_button_ui;
static LotteryItemDefinition last_drawn_item; // 用於顯示上次抽中的物品
static bool show_drawn_item_feedback = false;
static double drawn_item_feedback_end_time = 0;
#define DRAWN_ITEM_DISPLAY_DURATION 2.5 // 抽中物品顯示持續時間 (秒)

// --- Helper: 繪製按鈕 (可放入共用 utils.c) ---
void draw_ui_button(Button* btn, ALLEGRO_FONT* btn_font) {
    ALLEGRO_COLOR current_color = btn->is_hovered ? btn->hover_color : btn->color;
    al_draw_filled_rectangle(btn->x, btn->y, btn->x + btn->width, btn->y + btn->height, current_color);
    if (btn_font && btn->text) {
        al_draw_text(btn_font, btn->text_color,
                     btn->x + btn->width / 2,
                     btn->y + (btn->height - al_get_font_ascent(btn_font)) / 2,
                     ALLEGRO_ALIGN_CENTRE, btn->text);
    }
}

// --- Helper: 檢查滑鼠是否在按鈕上 (可放入共用 utils.c) ---
bool is_mouse_on_button(Button* btn, float mouse_x, float mouse_y) {
    return (mouse_x >= btn->x && mouse_x <= btn->x + btn->width &&
            mouse_y >= btn->y && mouse_y <= btn->y + btn->height);
}


void init_lottery(void) {
    srand(time(NULL)); // 初始化隨機數種子

    // 初始化獎池物品
    // 物品1
    lottery_prize_pool[0].id = 1001; // 自定義ID
    //lottery.sprite = load_bitmap_once();
    strcpy(lottery_prize_pool[0].name, "回復藥水");
    lottery_prize_pool[0].image_path = "assets/image/item1.png"; // 使用您提供的圖片名稱
    lottery_prize_pool[0].image = al_load_bitmap(lottery_prize_pool[0].image_path);
    if (!lottery_prize_pool[0].image) {
        fprintf(stderr, "Failed to load lottery item image: %s\n", lottery_prize_pool[0].image_path);
        // 可以載入一個預設的"圖片遺失"圖片
    }

    // 物品2
    lottery_prize_pool[1].id = 1002; // 自定義ID
    strcpy(lottery_prize_pool[1].name, "力量提升");
    lottery_prize_pool[1].image_path = "assets/image/item2.png"; // 使用您提供的圖片名稱
    lottery_prize_pool[1].image = al_load_bitmap(lottery_prize_pool[1].image_path);
    if (!lottery_prize_pool[1].image) {
        fprintf(stderr, "Failed to load lottery item image: %s\n", lottery_prize_pool[1].image_path);
    }

    // 物品3
    lottery_prize_pool[2].id = 1003; // 自定義ID
    strcpy(lottery_prize_pool[2].name, "魔力提升");
    lottery_prize_pool[2].image_path = "assets/image/item3.png"; // 使用您提供的圖片名稱
    lottery_prize_pool[2].image = al_load_bitmap(lottery_prize_pool[2].image_path);
    if (!lottery_prize_pool[2].image) {
        fprintf(stderr, "Failed to load lottery item image: %s\n", lottery_prize_pool[2].image_path);
    }

    // 物品4
    lottery_prize_pool[3].id = 1004; // 自定義ID
    strcpy(lottery_prize_pool[3].name, "速度提升");
    lottery_prize_pool[3].image_path = "assets/image/item4.png"; // 使用您提供的圖片名稱
    lottery_prize_pool[3].image = al_load_bitmap(lottery_prize_pool[3].image_path);
    if (!lottery_prize_pool[3].image) {
        fprintf(stderr, "Failed to load lottery item image: %s\n", lottery_prize_pool[3].image_path);
    }

    // 初始化抽獎按鈕 (UI Button)
    draw_button_ui.x = 650;
    draw_button_ui.y = 450;
    draw_button_ui.width = 200;
    draw_button_ui.height = 50;
    draw_button_ui.text = "抽獎!";
    draw_button_ui.color = al_map_rgb(0, 180, 0);
    draw_button_ui.hover_color = al_map_rgb(50, 220, 50);
    draw_button_ui.text_color = al_map_rgb(255, 255, 255);
    draw_button_ui.is_hovered = false;
    // draw_button_ui.action_phase 不直接用於改變 game_phase，而是觸發抽獎動作
/*
    // 初始化打開背包按鈕 (UI Button)
    open_backpack_button_ui.x = 1100;
    open_backpack_button_ui.y = 50;
    open_backpack_button_ui.width = 180;
    open_backpack_button_ui.height = 40;
    open_backpack_button_ui.text = "查看背包 (B)";
    open_backpack_button_ui.color = al_map_rgb(100, 100, 200);
    open_backpack_button_ui.hover_color = al_map_rgb(130, 130, 230);
    open_backpack_button_ui.text_color = al_map_rgb(255, 255, 255);
    open_backpack_button_ui.is_hovered = false;
    open_backpack_button_ui.action_phase = BACKPACK; // 點擊此按鈕切換到背包階段
*/
    show_drawn_item_feedback = false;
    printf("Lottery initialized with %d prizes.\n", MAX_LOTTERY_PRIZES);
}


void render_lottery(void) {
    al_clear_to_color(al_map_rgb(50, 50, 70));
    // 繪製標題與金錢
    if (font) { // 假設 globals.h 中的 font 已被載入
        al_draw_text(font, al_map_rgb(255, 215, 0), 750, 50, ALLEGRO_ALIGN_CENTRE, "幸運抽獎");
        al_draw_textf(font, al_map_rgb(255, 255, 255), 750, 90, ALLEGRO_ALIGN_CENTRE, "持有金錢: %d G", player.money);
    }

    // 繪製按鈕
    draw_ui_button(&draw_button_ui, font);
    draw_ui_button(&open_backpack_button_ui, font);

    // 顯示抽獎費用
    if (font) {
        al_draw_textf(font, al_map_rgb(255, 255, 0),
                     draw_button_ui.x + draw_button_ui.width / 2,
                     draw_button_ui.y + draw_button_ui.height + 5,
                     ALLEGRO_ALIGN_CENTRE, "費用: %d G", LOTTERY_COST);
    }


    // 顯示抽中的物品回饋
    if (show_drawn_item_feedback && last_drawn_item.image) {
        float img_w = al_get_bitmap_width(last_drawn_item.image);
        float img_h = al_get_bitmap_height(last_drawn_item.image);
        float display_scale = 0.7f;
        float disp_x = 750 - (img_w * display_scale / 2);
        float disp_y = 250 - (img_h * display_scale / 2);

        al_draw_scaled_bitmap(last_drawn_item.image,
                              0, 0, img_w, img_h,
                              disp_x, disp_y, img_w * display_scale, img_h * display_scale,
                              0);
        if (font) {
            al_draw_textf(font, al_map_rgb(255, 255, 255),
                          750, disp_y + img_h * display_scale - 100,
                          ALLEGRO_ALIGN_CENTRE, "獲得: %s!", last_drawn_item.name);
        }
    }

    // 提示文字
    if (font) {
        al_draw_text(font, al_map_rgb(180, 180, 180), 10, 850, ALLEGRO_ALIGN_LEFT, "按 ESC 返回養成畫面");
    }
}

void handle_lottery_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GROWTH;
            show_drawn_item_feedback = false; // 離開時清除回饋
        }
/*
        if (ev.keyboard.keycode == ALLEGRO_KEY_B) { // 按 B 打開背包
            game_phase = BACKPACK;
            show_drawn_item_feedback = false;
        }
*/
    }else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
        draw_button_ui.is_hovered = is_mouse_on_button(&draw_button_ui, ev.mouse.x, ev.mouse.y);
        open_backpack_button_ui.is_hovered = is_mouse_on_button(&open_backpack_button_ui, ev.mouse.x, ev.mouse.y);
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev.mouse.button == 1) { // 左鍵點擊
            if (draw_button_ui.is_hovered) {
                // 檢查是否有足夠的金錢
                if (player.money >= LOTTERY_COST) {
                    player.money -= LOTTERY_COST; // 扣除金錢

                    // 執行抽獎
                    int random_index = rand() % MAX_LOTTERY_PRIZES;
                    last_drawn_item = lottery_prize_pool[random_index]; // 複製物品資訊

                    add_item_to_backpack(last_drawn_item); // 添加到背包

                    show_drawn_item_feedback = true;
                    drawn_item_feedback_end_time = al_get_time() + DRAWN_ITEM_DISPLAY_DURATION;

                    printf("抽中了: %s\n", last_drawn_item.name);
                } else {
                    printf("金錢不足! 需要 %d G 才能抽獎。\n", LOTTERY_COST);
                }
            }
            if (open_backpack_button_ui.is_hovered) {
                game_phase = open_backpack_button_ui.action_phase; // 切換到背包
                show_drawn_item_feedback = false;
            }
        }
    }
}

void update_lottery(void) {
    if (show_drawn_item_feedback && al_get_time() > drawn_item_feedback_end_time) {
        show_drawn_item_feedback = false;
    }
}