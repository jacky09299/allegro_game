#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h> 
#include "globals.h"
#include "backpack.h"
#include "types.h"
#include <stdio.h>
#include <stdbool.h>
/*
// 靜態旗標，用於追蹤背包是否已經被初始化過
static bool g_backpack_initialized_once = false;

void init_backpack(void) {
     // 只有當背包從未被初始化過時，才執行清空操作
    if (!g_backpack_initialized_once) {
        for (int i = 0; i < MAX_BACKPACK_SLOTS; ++i) { //
            player_backpack[i].quantity = 0; //
            player_backpack[i].item.id = -1; // 表示空槽位
        }
        backpack_item_count = 0; // 背包中還沒有物品
        g_backpack_initialized_once = true; // 設定旗標，表示已經初始化過
        printf("Backpack initialized for the first time.\n"); //
    } else {
        printf("Backpack already initialized. Skipping reset.\n");
    }
} 
*/
void render_backpack(void) {
    al_clear_to_color(al_map_rgb(50, 50, 70));
    if (font) {
        al_draw_text(font, al_map_rgb(220, 220, 200), 750, 30, ALLEGRO_ALIGN_CENTRE, "我的背包");
    }

    if (backpack_item_count == 0) {
        if (font) {
            al_draw_text(font, al_map_rgb(180, 180, 180), 750, 300, ALLEGRO_ALIGN_CENTRE, "背包是空的...");
        }
    } else {
        int items_per_row = 5;
        float slot_size = 200;
        float padding = 20;
        float start_x = 350;
        float start_y = 300;
        float text_offset_y = slot_size - 25; // 文字在格子底部的位置

        for (int i = 0; i < backpack_item_count; ++i) {
            if (player_backpack[i].quantity > 0) { // 只顯示有數量的物品
                int row = i / items_per_row;
                int col = i % items_per_row;

                float current_x = start_x + col * (slot_size + padding);
                float current_y = start_y + row * (slot_size + padding);

                // 繪製格子背景 (可選)
                al_draw_filled_rectangle(current_x, current_y, current_x + slot_size, current_y + slot_size, al_map_rgb(30,50,30));
                al_draw_rectangle(current_x, current_y, current_x + slot_size, current_y + slot_size, al_map_rgb(70,90,70), 1.5);


                // 繪製物品圖片
                if (player_backpack[i].item.image) {
                    float img_w = al_get_bitmap_width(player_backpack[i].item.image);
                    float img_h = al_get_bitmap_height(player_backpack[i].item.image);
                    float scale = (slot_size - 30) / (img_w > img_h ? img_w : img_h); // 縮放以適應格子
                    float display_w = img_w * scale;
                    float display_h = img_h * scale;
                    float img_disp_x = current_x + (slot_size - display_w) / 2;
                    float img_disp_y = current_y + (slot_size - display_h - 20) / 2; // 向上移動一點給文字留空間

                    al_draw_scaled_bitmap(player_backpack[i].item.image,
                                          0, 0, img_w, img_h,
                                          img_disp_x, img_disp_y, display_w, display_h, 0);
                }

                // 繪製物品名稱和數量
                if (font) {
                    char item_text[100];
                    snprintf(item_text, sizeof(item_text), "%s x%d", player_backpack[i].item.name, player_backpack[i].quantity);
                    al_draw_text(font, al_map_rgb(200, 200, 200),
                                 current_x + slot_size / 2, current_y + text_offset_y,
                                 ALLEGRO_ALIGN_CENTRE, item_text);
                }
            }
        }
    }

    if (font) {
        al_draw_text(font, al_map_rgb(180, 180, 180), 10, 850, ALLEGRO_ALIGN_LEFT, "按 ESC 返回養成畫面");
    }
}

void handle_backpack_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GROWTH;
        }
    }
}

void update_backpack(void) {
} 

void add_item_to_backpack(LotteryItemDefinition item_to_add) {
    // 檢查背包中是否已存在該物品 (通過 ID)
    for (int i = 0; i < backpack_item_count; ++i) {
        if (player_backpack[i].item.id == item_to_add.id) {
            player_backpack[i].quantity++;
            printf("Increased quantity of %s to %d\n", item_to_add.name, player_backpack[i].quantity);
            return;
        }
    }

    // 如果物品不存在且背包還有空間存放新品種
    if (backpack_item_count < MAX_BACKPACK_SLOTS) {
        player_backpack[backpack_item_count].item = item_to_add; // 複製物品定義 (包含圖片指針)
        player_backpack[backpack_item_count].quantity = 1;
        backpack_item_count++;
        printf("Added new item %s to backpack. Total unique items: %d\n", item_to_add.name, backpack_item_count);
    } else {
        printf("Backpack is full for new types of items! Cannot add %s.\n", item_to_add.name);
        // 可以在螢幕上顯示提示
    }
}