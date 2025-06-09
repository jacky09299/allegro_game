#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h> 
#include "globals.h"
#include "backpack.h"
#include "types.h"
#include <stdio.h>
#include <stdbool.h>
#include "escape_gate.h"

// --- UI 元素 ---
static Button skill_select_button;

// --- Helper: 繪製按鈕 (可放入共用 utils.c 以供多個檔案使用) ---
static void draw_ui_button(Button* btn, ALLEGRO_FONT* btn_font) {
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
static bool is_mouse_on_button(Button* btn, float mouse_x, float mouse_y) {
    return (mouse_x >= btn->x && mouse_x <= btn->x + btn->width &&
            mouse_y >= btn->y && mouse_y <= btn->y + btn->height);
}

void init_backpack(void) {
    // 初始化 "前往技能裝備" 按鈕
    skill_select_button.x = SCREEN_WIDTH - 250; // 放置在右上角
    skill_select_button.y = 30;
    skill_select_button.width = 220;
    skill_select_button.height = 40;
    skill_select_button.text = "技能裝備";
    skill_select_button.action_phase = EQUIPMENT; // 按下後切換到裝備階段
    skill_select_button.color = al_map_rgb(150, 100, 200);
    skill_select_button.hover_color = al_map_rgb(180, 130, 230);
    skill_select_button.text_color = al_map_rgb(255, 255, 255);
    skill_select_button.is_hovered = false;
    printf("Backpack UI button initialized.\n");
}

void render_backpack(void) {
    al_clear_to_color(al_map_rgb(50, 50, 70));
    if (font) {
        al_draw_text(font, al_map_rgb(220, 220, 200), 750, 30, ALLEGRO_ALIGN_CENTRE, "我的背包");
    }

    // 繪製前往技能裝備的按鈕
    draw_ui_button(&skill_select_button, font);

    if (backpack_item_count == 0) {
        if (font) {
            al_draw_text(font, al_map_rgb(180, 180, 180), 750, 300, ALLEGRO_ALIGN_CENTRE, "背包是空的...");
        }
    } else {
        int items_per_row = 5;
        float slot_size = 200;
        float padding = 20;
        float start_x = 200;
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

        if (growth_message_timer > 0) {
            al_draw_text(font, al_map_rgb(255, 255, 0),
                        SCREEN_WIDTH / 2, 100,
                        ALLEGRO_ALIGN_CENTER, growth_message);
            growth_message_timer--;
        }
    }

    if (font) {
        al_draw_text(font, al_map_rgb(180, 180, 180), 10, 850, ALLEGRO_ALIGN_LEFT, "按 ESC 返回養成畫面，按 B 返回戰鬥畫面");
    }
}

// 幫助函式：從背包陣列中移除一個物品
static void remove_item_from_backpack(int index) {
    if (index < 0 || index >= backpack_item_count) return;

    // 將後續物品往前移動以填補空位
    for (int i = index; i < backpack_item_count - 1; ++i) {
        player_backpack[i] = player_backpack[i + 1];
    }
    backpack_item_count--;

    // (可選) 清理最後一個現在未使用的槽位
    player_backpack[backpack_item_count].quantity = 0;
    player_backpack[backpack_item_count].item.id = -1;
}

void handle_backpack_input(ALLEGRO_EVENT ev) {
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            game_phase = GROWTH;
        }
    //    else if (ev.keyboard.keycode == ALLEGRO_KEY_E && game_phase != BATTLE) {
     //       game_phase = EQUIPMENT;
      //  }
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES && game_phase != BATTLE) {
        // 更新按鈕的懸停狀態
        skill_select_button.is_hovered = is_mouse_on_button(&skill_select_button, ev.mouse.x, ev.mouse.y); 
    }else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev.mouse.button == 1) { // 處理左鍵點擊
            // 複製渲染時的佈局邏輯來找到被點擊的物品
            if (skill_select_button.is_hovered) {
                game_phase = skill_select_button.action_phase; // 切換到 EQUIPMENT 階段
                return; // 切換階段後直接返回
            }
            int items_per_row = 5;
            float slot_size = 200;
            float padding = 20;
            float start_x = 200;
            float start_y = 300;

            for (int i = 0; i < backpack_item_count; ++i) {
                int row = i / items_per_row;
                int col = i % items_per_row;
                float current_x = start_x + col * (slot_size + padding);
                float current_y = start_y + row * (slot_size + padding);

                // 檢查點擊是否在此物品格的範圍內
                if (ev.mouse.x >= current_x && ev.mouse.x <= current_x + slot_size &&
                    ev.mouse.y >= current_y && ev.mouse.y <= current_y + slot_size) {
                    
                    int item_id = player_backpack[i].item.id;
                    bool item_was_used = true; // 預設物品會被使用

                    // 根據物品ID應用效果
                    switch (item_id) {
                        case 1001: // 回復藥水
                            if (player.hp >= player.max_hp) {
                                player.max_hp += 10;
                            } else {
                                player.hp = player.max_hp; // 完全回復生命值
                            }
                            break;
                        case 1002: // 力量提升
                            player.strength += 1;
                            break;
                        case 1003: // 魔力提升
                            if (player.mp >= player.max_mp) {
                                player.max_mp += 5;
                                player.magic += 1;
                            } else {
                                player.mp = player.max_mp; // 完全回復魔力值
                                player.magic += 1;
                            }
                            break;
                        case 1004: // 速度提升
                            player.max_speed += 0.1;
                            break;
                        case 1005: // 指南針
                            if(show_compass == true || game_phase != BATTLE) {
                                item_was_used = false;
                                snprintf(growth_message, sizeof(growth_message), "目前無法使用該道具");
                                growth_message_timer = 180; // 顯示訊息約 3 秒（假設 60 FPS）
                            }  
                            show_compass = true;
                            break;
                        case 1006: // 花
                            player.money += 3000;
                            snprintf(growth_message, sizeof(growth_message), "獲得%d元", 3000);
                                    growth_message_timer = 180; // 顯示訊息約 3 秒（假設 60 FPS）
                            break;
                        case 1007: // 惡魔花
                        {
                            int hp_loss = player.max_hp * 0.2;
                            player.max_hp -= hp_loss;
                            player.strength += hp_loss/100;
                            if(player.hp > player.max_hp) player.hp = player.max_hp;
                            snprintf(growth_message, sizeof(growth_message), "失去%d2點血量，獲得%d點力量", hp_loss, hp_loss/100);
                                    growth_message_timer = 180; // 顯示訊息約 3 秒（假設 60 FPS）
                            break;
                        }
                            
                        default:
                            item_was_used = false; // 此物品沒有定義用途
                            break;
                    }

                    // 如果物品被成功使用，則減少其數量
                    if (item_was_used) {
                        player_backpack[i].quantity--;
                        printf("已使用 %s。剩餘數量: %d\n", player_backpack[i].item.name, player_backpack[i].quantity);

                        // 如果數量歸零，則從背包中移除
                        if (player_backpack[i].quantity <= 0) {
                            remove_item_from_backpack(i);
                            break; // 退出循環，因為陣列已被修改
                        }
                    }
                }
            }
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