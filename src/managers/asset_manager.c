#include "asset_manager.h"
#include "floor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <allegro5/allegro_image.h>

// 定義一個結構來儲存 bitmap 的資訊
typedef struct {
    char* path;
    ALLEGRO_BITMAP* bitmap;
} LoadedBitmapInfo;

// 儲存已載入 bitmap 的內部資料
static LoadedBitmapInfo* loaded_bitmaps = NULL;
static int num_loaded_bitmaps = 0;
static int capacity_loaded_bitmaps = 0;

#define INITIAL_BITMAP_CAPACITY 100

void init_asset_manager(void) {
    loaded_bitmaps = (LoadedBitmapInfo*)malloc(sizeof(LoadedBitmapInfo) * INITIAL_BITMAP_CAPACITY);
    num_loaded_bitmaps = 0;
    capacity_loaded_bitmaps = INITIAL_BITMAP_CAPACITY;
    printf("AssetManager: 初始化完成。\n");
}

ALLEGRO_BITMAP* load_bitmap_once(const char* path) {
    for (int i = 0; i < num_loaded_bitmaps; ++i) {
        if (strcmp(loaded_bitmaps[i].path, path) == 0) {
            return loaded_bitmaps[i].bitmap;
        }
    }

    ALLEGRO_BITMAP* new_bitmap = al_load_bitmap(path);

    if (num_loaded_bitmaps >= capacity_loaded_bitmaps) {
        int new_capacity = capacity_loaded_bitmaps * 2;
        loaded_bitmaps = (LoadedBitmapInfo*)realloc(loaded_bitmaps, sizeof(LoadedBitmapInfo) * new_capacity);
        capacity_loaded_bitmaps = new_capacity;
    }

    char* path_copy = strdup(path);
    loaded_bitmaps[num_loaded_bitmaps].path = path_copy;
    loaded_bitmaps[num_loaded_bitmaps].bitmap = new_bitmap;
    num_loaded_bitmaps++;

    return new_bitmap;
}

void destroy_all_loaded_bitmaps(void) {
    for (int i = 0; i < num_loaded_bitmaps; ++i) {
        al_destroy_bitmap(loaded_bitmaps[i].bitmap);
        free(loaded_bitmaps[i].path);
    }
    free(loaded_bitmaps);
    loaded_bitmaps = NULL;
    num_loaded_bitmaps = 0;
    capacity_loaded_bitmaps = 0;
}

void shutdown_asset_manager(void) {
    destroy_all_loaded_bitmaps();
    destroy_floor_texture();
}
