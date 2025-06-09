#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_color.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "globals.h"
#include "minigame2.h"

// --- 常數定義 ---
static double CHILD_MATURATION_TIME = 10.0;
static const double ASEXUAL_CHANCE_PER_SEC = 0.005;
static const float INITIAL_QUALITY = 1.0f;
static const float MAX_QUALITY = 2.0f;
static const float MIN_QUALITY = 0.1f;
static const float EAT_ATK_FACTOR = 0.5f;
static const float HAPPINESS_FACTOR = 0.5f;
static const float PARENT_FACTOR = 0.9f;
static const float ASEXUAL_PENALTY = 0.9f;
static double BREEDING_DURATION = 5.0; // 繁殖耗時

// --- 動態陣列儲存族群 (Population) ---
Person population[MAX_POPULATION_LIMIT];
size_t pop_count = 0;
int next_id = 0;
float temp_atk = 0.0f;
float leader_point_x = 800.0f, leader_point_y = 800.0f;


// --- 繁殖佇列 (Breeding Queue) ---
static int breeding_queue[2000];
static size_t breeding_queue_count = 0;

static bool is_breeding_in_progress = false;
static double current_breeding_start_time = 0.0;
static int current_breeding_parent_id = -1;

// --- Minigame 全域變數 ---
static int last_clicked_id = -1; // 用於顯示最後點擊的個體資訊
static double game_time = 0.0, last_update = 0.0;

static bool is_dragging_selection_circle = false;
static float selection_drag_start_x = 0, selection_drag_start_y = 0;
static float current_mouse_drag_x = 0, current_mouse_drag_y = 0;


// --- 空間分割網格的常數與資料結構 ---
#define CELL_SIZE 50.0f // 網格單元大小，可根據交互範圍調整
#define GRID_WIDTH 15
#define GRID_HEIGHT 9

static double last_leader_point_change_time = 0.0;

// 為了效能，我們用一個大陣列模擬動態列表，而不是真的用鏈結串列
// 'grid' 儲存每個網格單元的資訊：它在 'cell_content' 中的起始索引和個數
typedef struct {
    int start_index;
    int count;
} GridCell;

static GridCell grid[(int)GRID_WIDTH + 1][(int)GRID_HEIGHT + 1];
// 'cell_content' 是一個大陣列，連續儲存所有網格中的個體索引
static int cell_content[2000];
// 'next_in_cell' 用於在填充網格時串連同一個單元格中的個體
static int next_in_cell[2000];

// --- 其他優化用常數 ---
static const float TIME_STEP = 0.016f; // 假設更新頻率約為 60fps
static const float DAMPING = 0.1f; // 阻尼係數
static const float INTERACTION_FORCE_FACTOR = -0.01f;
static const float RANDOM_VELOCITY_CHANGE_CHANCE = 0.01f; // 原來的 1% 機率

const char* lastNames[] = {
    "李", "王", "張", "劉", "陳", "楊", "黃", "吳", "林", "趙",
    "周", "徐", "孫", "馬", "朱", "胡", "郭", "何", "高", "林",
    "羅", "鄭", "梁", "謝", "宋", "唐", "許", "鄧", "韓", "曹",
    "曾", "彭", "蕭", "蔡", "潘", "田", "袁", "馮", "邱", "侯",
    "白", "江", "阮", "薛", "呂", "魏", "鐘", "盧", "蔣", "汪",
    "歐陽", "司馬", "諸葛", "上官", "司徒", "夏侯", "南宮", "公孫", "宇文", "慕容"
};
const char* firstNames1[] = {
    "志", "欣", "庭", "宇", "柏", "嘉", "思", "昀", "育", "承",
    "哲", "瑞", "子", "宥", "孟", "昱", "亭", "書", "均", "泓",
    "予", "奕", "芯", "語", "芝", "采", "妍", "婉", "怡", "芸",
    "宸", "睿", "筠", "渝", "珊", "潔", "彤", "懿", "浩", "郁",
    "品", "佳", "宗", "昇", "旻", "祐", "博", "騰", "中", "冠",
    "宏", "煒", "晨", "凱", "誠", "明", "謙", "晏", "玟", "韋",
    "晉", "威", "洋", "東", "杰", "穎", "璇", "瑜", "逸", "潤",
    "曉", "辰", "澤", "斌", "恩", "毅", "朗", "萱", "嫻", "霈",
    "嬿", "湘", "妤", "淇", "芷", "蓉", "鈺", "麟", "詠", "雅",
    "皓", "融", "翰", "馳", "翎", "晟", "瑋", "堯", "弘", "頤",
    "誼", "寧", "榆", "致", "邦", "睞", "尚", "煥", "軒", "騰",
    "潼", "琦", "倩", "芮", "安", "恩", "政", "詠", "芷", "微",
    "維", "澔", "銘", "純", "悠", "芸", "翔", "鴻", "昊", "諾",
    "歆", "泓", "翎", "恒", "鈞", "楚", "雲", "聖", "倫", "禹",
    "弘", "靖", "瑜", "熙", "語", "芯", "蘊", "韶", "梓", "榮",
    "嵐", "楷", "璿", "涵", "宛", "竹", "清", "慧", "曦", "煒",
    "衍", "錦", "寰", "馥", "喬", "璟", "宥", "璇", "嶸", "柏",
    "箴", "翰", "岳", "恩", "瑱", "璨", "芯", "曜", "熠", "漪",
    "弈", "豐", "瀅", "潁", "俞", "晨", "珂", "苓", "琬", "韞"
};
const char* firstNames2[] = {
    "豪", "恩", "軒", "霖", "潔", "倫", "儀", "涵", "翔", "甯",
    "澤", "瑜", "彥", "瑋", "珮", "庭", "翰", "甫", "蘭", "萱",
    "安", "晴", "茹", "鈺", "芬", "靜", "惠", "彤", "芸", "芝",
    "昕", "伶", "婕", "昀", "璇", "妍", "儒", "凡", "馨", "佳",
    "瑄", "珊", "詠", "霓", "羽", "璟", "嫻", "芊", "皓", "淇",
    "齊", "毅", "謙", "翎", "萌", "雅", "彧", "瀚", "弘", "濬",
    "苓", "蕙", "琬", "媛", "霏", "曦", "漾", "婉", "菁", "馥",
    "羿", "翊", "朔", "辰", "衍", "雯", "琳", "恬", "蘊", "沛",
    "朗", "彬", "邦", "怡", "驊", "清", "青", "煒", "煦", "航",
    "華", "頤", "豫", "仁", "勻", "詩", "昭", "正", "超", "聿",
    "誠", "隆", "峻", "宸", "濤", "旭", "睿", "澔", "愷", "致",
    "淳", "承", "偉", "裕", "暉", "雋", "辰", "澤", "佑", "昱",
    "晨", "喬", "岳", "璇", "翰", "豪", "騰", "璿", "鈞", "潤",
    "騫", "霖", "炘", "謙", "嶸", "懿", "睿", "嵐", "懷", "銘",
    "諺", "斌", "燁", "弘", "權", "澈", "灝", "燦", "雲", "愉",
    "俞", "綸", "樂", "翊", "辰", "謠", "楚", "孝", "竣", "竺",
    "臻", "翾", "悠", "黛", "靜", "弦", "濯", "璞", "諠", "謹"
};


void assign_random_name(char *dest) {
    // 複姓的起始索引
    int single_last_count = 50; // 前50個是單姓
    int total_last_count = sizeof(lastNames) / sizeof(lastNames[0]);
    int complex_last_count = total_last_count - single_last_count;

    int use_complex = (rand() % 100 == 0) ? 1 : 0; // 約10%機率選複姓
    int lastIndex;
    if (use_complex && complex_last_count > 0) {
        lastIndex = single_last_count + rand() % complex_last_count;
    } else {
        lastIndex = rand() % single_last_count;
    }

    int first1Index = rand() % (sizeof(firstNames1) / sizeof(firstNames1[0]));
    int first2Index = rand() % (sizeof(firstNames2) / sizeof(firstNames2[0]));

    const char* last = lastNames[lastIndex];
    const char* first1 = firstNames1[first1Index];
    const char* first2 = firstNames2[first2Index];

    // 單名機率 1%，否則雙名
    int nameLen = (rand() % 100 == 0) ? 1 : 2;

    if (nameLen == 1) {
        snprintf(dest, 16, "%s%s", last, first1);
    } else {
        snprintf(dest, 16, "%s%s%s", last, first1, first2);
    }
}


/*
    * 初始化一個 Person 結構體
    * p: 指向 Person 結構體的指標
    * id: 個體的唯一識別碼
    * t: 出生時間
    * q: 個體的品質值
    * adult: 是否為成年個體
*/
void person_init(Person *p, int id, double t, float q, bool adult) {
    p->id = id;
    p->birth_time = t;
    p->quality = q;
    p->is_adult = adult;
    p->is_selected_for_queue = false;
    p->last_reproduction_time = adult ? (t - CHILD_MATURATION_TIME - BREEDING_DURATION -1.0): t;
    p->happiness = 0.0f; // 初始快樂值為 0
    p->x = 100 + rand() % (SCREEN_WIDTH - 200);
    p->y = 100 + rand() % (SCREEN_HEIGHT - 200);
    p->vx = 0.0f; // 初始速度向量為 0
    p->vy = 0.0f; // 初始速度向量為 0
    p->ax = 0.0f; // 初始加速度向量為 0
    p->ay = 0.0f; // 初始加速度向量為 0
    assign_random_name(p->name); // 姓名分配
}

void clean_atk() {
    temp_atk = 0.0f;
}

/*
    * 檢查個體是否免於選擇變更
    * p: 指向 Person 結構體的指標
    * 返回 true 如果個體在繁殖中或在繁殖佇列中，否則返回 false
*/
static bool is_person_immune_to_selection_change(const Person* p) {
    if (is_breeding_in_progress && current_breeding_parent_id == p->id) {
        return true;
    }
    for (size_t i = 0; i < breeding_queue_count; ++i) {
        if (breeding_queue[i] == p->id) {
            return true;
        }
    }
    return false;
}

/*
    * 從 population 中移除一個人
    * idx: 要移除的人的索引
    * 如果 idx 超出範圍，則不執行任何操作
    * 如果被移除的人正在繁殖中，則取消繁殖並清除相關狀態
*/
static void remove_person(size_t idx) {
    if (idx >= pop_count) return;

    int removed_id = population[idx].id;

    bool removed_was_breeding = (is_breeding_in_progress && current_breeding_parent_id == removed_id);

    for (size_t i = 0; i < breeding_queue_count; ) {
        if (breeding_queue[i] == removed_id) {
            if (i == 0 && removed_was_breeding) {
                 is_breeding_in_progress = false;
                 current_breeding_parent_id = -1;
            }
            memmove(&breeding_queue[i], &breeding_queue[i+1], (breeding_queue_count - i - 1) * sizeof(int));
            breeding_queue_count--;
        } else {
            i++;
        }
    }
    
     if (removed_was_breeding && current_breeding_parent_id == removed_id) { 
        is_breeding_in_progress = false;
        current_breeding_parent_id = -1;
        
    }

    memmove(&population[idx], &population[idx+1], (pop_count - idx - 1) * sizeof(Person));
    pop_count--;

    if (last_clicked_id == removed_id) last_clicked_id = -1;
}

/*
    * 從 population 中隨機淘汰一個人
    * 根據每個人的品質計算淘汰權重，權重越低被淘汰的機率越高
    * 如果所有人的權重總和為 0，則隨機移除一個人
*/
static void cull_one_person() {
    if (pop_count == 0) return;

    double total_cull_weight = 0.0;
    for (size_t i = 0; i < pop_count; ++i) {
        double weight = (MAX_QUALITY + 1.0f) - population[i].quality;
        if (weight < 0.1) weight = 0.1; 
        total_cull_weight += weight;
    }

    if (total_cull_weight <= 0.0) {
        size_t fallback_idx = rand() % pop_count;
        remove_person(fallback_idx);
        return;
    }

    double random_pick = ((double)rand() / RAND_MAX) * total_cull_weight;
    double current_sum = 0.0;
    size_t person_to_remove_idx = pop_count - 1;

    for (size_t i = 0; i < pop_count; ++i) {
        double weight = (MAX_QUALITY + 1.0f) - population[i].quality;
        if (weight < 0.1) weight = 0.1; 
        current_sum += weight;
        if (random_pick <= current_sum) {
            person_to_remove_idx = i;
            break;
        }
    }
    remove_person(person_to_remove_idx);
}

/*
    * 添加一個新的人到 population 中
    * quality: 新人的品質值
    * is_initial_adult: 是否為初始成年個體
    * 如果達到最大人口限制，則先淘汰一個人
    * 如果無法確保容量，則不執行任何操作
*/
static void add_person(float quality, bool is_initial_adult) {
    if (MAX_POPULATION_LIMIT == 0) {
        return; 
    }

    if (pop_count >= MAX_POPULATION_LIMIT) {
        cull_one_person(); 
    }

    if (pop_count < MAX_POPULATION_LIMIT) {
        // if (!ensure_cap(pop_count + 1)) return; 
        person_init(&population[pop_count], next_id++, game_time, quality, is_initial_adult);
        pop_count++;
    }
}

/*
    * 根據 ID 查找 population 中的人
    * id: 要查找的人的 ID
    * 返回該人的索引，如果未找到則返回 -1
*/
static int find_by_id(int id) {
    for (size_t i = 0; i < pop_count; i++) {
        if (population[i].id == id) return (int)i;
    }
    return -1;
}
/*
    * 計算自上次繁殖以來的時間
    * p: 指向 Person 結構體的指標
    * 返回自上次繁殖以來的時間差，如果個體不是成年則返回 0.0
*/
static double time_since_repro(const Person *p) {
    if (!p->is_adult) return 0.0;
    double diff = game_time - p->last_reproduction_time;
    return diff > 0.0 ? diff : 0.0;
}
/*
    * 將一個人添加到繁殖佇列
    * person_id: 要添加的人的 ID
    * 如果該人不存在或已經免於選擇變更，則不執行任何操作
    * 如果無法確保繁殖佇列容量，則不執行任何操作
*/
static void add_to_breeding_queue(int person_id) {
    int person_idx = find_by_id(person_id);
    if (person_idx == -1) {
        return;
    }
    if (is_person_immune_to_selection_change(&population[person_idx])) {
         return;
    }
    // if (!ensure_breeding_queue_cap(breeding_queue_count + 1)) return;
    if (breeding_queue_count >= 2000) return;
    breeding_queue[breeding_queue_count++] = person_id;
}
/*
    * 從繁殖佇列的前面移除一個人
    * 如果繁殖佇列為空，則不執行任何操作
    * 如果有繁殖正在進行，則取消繁殖並清除相關狀態
*/
static void remove_from_breeding_queue_front() {
    if (breeding_queue_count == 0) return;
    memmove(&breeding_queue[0], &breeding_queue[1], (breeding_queue_count - 1) * sizeof(int));
    breeding_queue_count--;
}
/*
    * 初始化 minigame2 的全域變數和狀態
    * 釋放之前的 population 和 breeding_queue 記憶體
    * 添加初始人口到 population 中
*/

void update_behavior(){
    // --- 階段 0: 重置所有個體的加速度 ---
    for (size_t i = 0; i < pop_count; i++) {
        population[i].ax = 0.0f;
        population[i].ay = 0.0f;
    }

    // --- 階段 1: 填充空間分割網格 (Spatial Grid) ---
    // 清空網格計數器
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            grid[x][y].count = 0;
            grid[x][y].start_index = -1; // -1 表示為空
        }
    }
    
    // 遍歷所有個體，將它們放入對應的網格
    for (size_t i = 0; i < pop_count; i++) {
        Person* p = &population[i];
        int cell_x = p->x / CELL_SIZE;
        int cell_y = p->y / CELL_SIZE;

        // 邊界檢查
        if (cell_x < 0) cell_x = 0;
        if (cell_x >= GRID_WIDTH) cell_x = GRID_WIDTH - 1;
        if (cell_y < 0) cell_y = 0;
        if (cell_y >= GRID_HEIGHT) cell_y = GRID_HEIGHT - 1;

        // 使用 "next_in_cell" 暫時建立一個單向鏈結串列來將同網格的個體串起來
        // 這種方法比直接操作陣列更簡單，最後再展平
        next_in_cell[i] = grid[cell_x][cell_y].start_index;
        grid[cell_x][cell_y].start_index = i;
        grid[cell_x][cell_y].count++;
    }
    
    // 將鏈結串列結構 "展平" 到 cell_content 連續陣列中，以提高快取命中率
    int current_pos = 0;
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            int original_start = grid[x][y].start_index;
            grid[x][y].start_index = current_pos; // 更新為在 cell_content 中的真實起始位置
            
            int head = original_start;
            while(head != -1) {
                cell_content[current_pos++] = head;
                head = next_in_cell[head];
            }
        }
    }


    // --- 階段 2: 使用網格計算個體間的作用力 ---
    for (size_t i = 0; i < pop_count; i++) {
        Person* p = &population[i];
        int cell_x = p->x / CELL_SIZE;
        int cell_y = p->y / CELL_SIZE;
        
        // 遍歷周圍 3x3 的網格 (包含自身所在的網格)
        for (int nx = cell_x - 3; nx <= cell_x + 3; nx++) {
            for (int ny = cell_y - 3; ny <= cell_y + 3; ny++) {
                // 邊界檢查
                if (nx < 0 || nx >= GRID_WIDTH || ny < 0 || ny >= GRID_HEIGHT) {
                    continue;
                }

                // 獲取該鄰近網格的內容
                GridCell neighbor_cell = grid[nx][ny];
                int start = neighbor_cell.start_index;
                int end = start + neighbor_cell.count;

                // 遍歷網格中的所有個體
                for (int k = start; k < end; k++) {
                    size_t j = cell_content[k];
                    
                    // 關鍵：避免重複計算 (i 與 j) 和自我計算 (i 與 i)
                    // 原本的 j = i + 1 隱含了這個邏輯，這裡我們必須明確加上
                    if (i >= j) {
                        continue;
                    }
                    
                    Person* other = &population[j];

                    float dx = other->x - p->x;
                    float dy = other->y - p->y;
                    float dist_sq = dx * dx + dy * dy + 0.0001f;
                    
                    // 次要優化：如果距離太遠，可以提前跳過，但目前演算法已經很快，此步可選
                    float max_interaction_dist_sq = CELL_SIZE * CELL_SIZE * 10; // e.g.
                    if (dist_sq > max_interaction_dist_sq) continue;

                    // 次要優化：只計算一次 sqrt
                    float dist = sqrtf(dist_sq);
                    float d_quality = p->quality - other->quality;

                    float force = INTERACTION_FORCE_FACTOR * (dist - d_quality * 10000 - 50.0f);
                    
                    // 次要優化：避免重複除法
                    float force_over_dist = force / dist;
                    float force_x = force_over_dist * dx;
                    float force_y = force_over_dist * dy;

                    p->ax -= force_x;
                    p->ay -= force_y;
                    other->ax += force_x;
                    other->ay += force_y;
                }
            }
        }
    }

    // --- 階段 3: 更新每個個體的位置和速度 ---
    for (size_t i = 0; i < pop_count; i++) {
        Person *p = &population[i];
        
        // 隨機擾動
        if (((float)rand() / RAND_MAX) < RANDOM_VELOCITY_CHANGE_CHANCE/p->happiness) {
            p->vx += ((float)rand() / RAND_MAX) * 40.0f - 20.0f;
            p->vy += ((float)rand() / RAND_MAX) * 40.0f - 20.0f;
        }
        
        float leader_dx = leader_point_x - p->x;
        float leader_dy = leader_point_y - p->y;
        float dist = sqrtf(leader_dx * leader_dx + leader_dy * leader_dy) + 0.0001f;
        // 彈簧力：F = k * (d - L)
        const float SPRING_K = 0.8f;    // 彈簧常數，可調整
        const float SPRING_L = 500.0f;   // 彈簧原長
        float spring_force = SPRING_K * (dist - SPRING_L*(1.0f - p->quality * 0.5f)); // 品質影響彈簧原長
        p->ax += p->quality * spring_force * leader_dx / dist;
        p->ay += p->quality * spring_force * leader_dy / dist;

        // 套用阻尼 (Friction/Damping)
        p->ax -= DAMPING * p->vx;
        p->ay -= DAMPING * p->vy;
        
        // 更新速度
        p->vx += p->ax * TIME_STEP;
        p->vy += p->ay * TIME_STEP;
        
        // 更新位置
        p->x += p->vx * TIME_STEP;
        p->y += p->vy * TIME_STEP;
        
        // 邊界限制
        if (p->x < 15.0f) { p->x = 15.0f; p->vx *= -0.5f; } // 碰撞後反彈
        if (p->y < 15.0f) { p->y = 15.0f; p->vy *= -0.5f; }
        if (p->x > SCREEN_WIDTH - 15.0f) { p->x = SCREEN_WIDTH - 15.0f; p->vx *= -0.5f; }
        if (p->y > SCREEN_HEIGHT - 15.0f) { p->y = SCREEN_HEIGHT - 15.0f; p->vy *= -0.5f; }
    }
}

void init_minigame2(void) {
    srand((unsigned int)time(NULL));

    // if (population) { free(population); population = NULL; }
    pop_count = 0; // pop_cap = 0;
    // if (breeding_queue) { free(breeding_queue); breeding_queue = NULL; }
    breeding_queue_count = 0; // breeding_queue_cap = 0;

    is_breeding_in_progress = false;
    current_breeding_start_time = 0.0;
    current_breeding_parent_id = -1;
    next_id = 0;
    last_clicked_id = -1;
    temp_atk = 0.0f;
    is_dragging_selection_circle = false;
    selection_drag_start_x = 0; selection_drag_start_y = 0;
    current_mouse_drag_x = 0; current_mouse_drag_y = 0;

    double now = al_get_time();
    game_time = now; last_update = now;

    size_t initial_pop_target = 3;
    if (MAX_POPULATION_LIMIT > 0 && initial_pop_target > MAX_POPULATION_LIMIT) {
        initial_pop_target = MAX_POPULATION_LIMIT;
    } else if (MAX_POPULATION_LIMIT == 0) {
        initial_pop_target = 0;
    }

    for (size_t i = 0; i < initial_pop_target; ++i) {
        add_person(INITIAL_QUALITY * (1.0f + (float)(rand() % 50 - 25)/100.0f) , true);
    }
}

void render_minigame2(void) {
    al_clear_to_color(al_map_rgb(50, 50, 70));
    size_t adults = 0, children = 0;

    for (size_t i = 0; i < pop_count; i++) {
        const Person *p = &population[i];
        ALLEGRO_COLOR c;
        float r = p->is_adult ? 15.0f : 10.0f;

        if (!p->is_adult) {
            c = al_map_rgb(0, 0, 0);  // 小孩全黑
            children++;
        } else {
            float q_norm = (p->quality - MIN_QUALITY) / (MAX_QUALITY - MIN_QUALITY);
            q_norm = fminf(fmaxf(q_norm, 0.0f), 1.0f);  // 限定在 0~1

            float hue = 0.83f - 0.83f * q_norm;  // 品質越高 hue 越小（紅）
            float sat = 0.7f;
            float val = 0.3f + 0.7f * q_norm;  // 更劇烈變化，最低 0.3 → 最高 1.0


            // HSV to RGB conversion
            float r_f = 0, g_f = 0, b_f = 0;
            float h = hue * 6.0f;
            int hi = (int)h;
            float f = h - hi;
            float p = val * (1.0f - sat);
            float q = val * (1.0f - sat * f);
            float t = val * (1.0f - sat * (1.0f - f));
            switch (hi % 6) {
                case 0: r_f = val; g_f = t; b_f = p; break;
                case 1: r_f = q; g_f = val; b_f = p; break;
                case 2: r_f = p; g_f = val; b_f = t; break;
                case 3: r_f = p; g_f = q; b_f = val; break;
                case 4: r_f = t; g_f = p; b_f = val; break;
                case 5: r_f = val; g_f = p; b_f = q; break;
            }

            unsigned char r = (unsigned char)(r_f * 255);
            unsigned char g = (unsigned char)(g_f * 255);
            unsigned char b = (unsigned char)(b_f * 255);
            c = al_map_rgb(r, g, b);
            adults++;
        }

        al_draw_filled_circle(p->x, p->y, r, c);

        if (is_person_immune_to_selection_change(p)) { 
            if (is_breeding_in_progress && current_breeding_parent_id == p->id) {
                al_draw_circle(p->x, p->y, r + 5, al_map_rgb(255, 0, 255), 2.5f); 
            } else {
                al_draw_circle(p->x, p->y, r + 3, al_map_rgb(0, 255, 255), 2.0f); 
            }
        } else if (p->is_selected_for_queue) {
            al_draw_circle(p->x, p->y, r + 3, al_map_rgb(255, 255, 0), 2.0f); 
        }

    }

    if (is_dragging_selection_circle && font) { 
        float radius = sqrt(pow(current_mouse_drag_x - selection_drag_start_x, 2) + pow(current_mouse_drag_y - selection_drag_start_y, 2));
        if (radius > 1.0f) { 
             al_draw_circle(selection_drag_start_x, selection_drag_start_y, radius, al_map_rgba(200, 200, 255, 100), 2.0f);
        }
    }


    if (font) {
        al_draw_textf(font, al_map_rgb(255,255,255), 10, 10, 0, "人口數: %I64u/%d (成人:%I64u, 兒童:%I64u)", pop_count, MAX_POPULATION_LIMIT, adults, children);
        al_draw_textf(font, al_map_rgb(255,255,255), 10, 30, 0, "增長戰鬥力: %d", (int)temp_atk);
        
        

        al_draw_text(font, al_map_rgb(200,200,200), 10, SCREEN_HEIGHT - 120, 0, "左鍵點選: 選擇/取消選擇個體");
        al_draw_text(font, al_map_rgb(200,200,200), 10, SCREEN_HEIGHT - 100, 0, "左鍵框選: 選擇個體");
        al_draw_text(font, al_map_rgb(200,200,200), 10, SCREEN_HEIGHT - 80, 0, "右鍵: 取消所有選取");
        al_draw_text(font, al_map_rgb(200,200,200), 10, SCREEN_HEIGHT - 60, 0, "Q: 加入繁殖列 | E: 吃掉");
        al_draw_text(font, al_map_rgb(200,200,200), 10, SCREEN_HEIGHT - 40, 0, "C: 清空繁殖列 | ESC: 退出");


        if (last_clicked_id != -1) {
            int idx = find_by_id(last_clicked_id);
            if (idx != -1) {
                const Person *p = &population[idx];
                al_draw_textf(font, al_map_rgb(255,255,0), SCREEN_WIDTH - 350, 10, 0, "姓名: %s", p->name);
                al_draw_textf(font, al_map_rgb(255,255,0), SCREEN_WIDTH - 350, 40, 0, "品質: %.2f", p->quality);
                al_draw_textf(font, al_map_rgb(255,255,0), SCREEN_WIDTH - 350, 70, 0, "幸福感: %.2f", p->happiness);
            }
        }
    }

    if (last_clicked_id != -1) {
        int idx = find_by_id(last_clicked_id);
        if (idx != -1) {
            const Person *p = &population[idx];
            al_draw_textf(font, al_map_rgb(255,255,0), SCREEN_WIDTH - 350, 10, 0, "姓名: %s", p->name);
            al_draw_textf(font, al_map_rgb(255,255,0), SCREEN_WIDTH - 350, 40, 0, "品質: %.2f", p->quality);
            al_draw_textf(font, al_map_rgb(255,255,0), SCREEN_WIDTH - 350, 70, 0, "幸福感: %.2f", p->happiness);
        }
    }


    // --- 品質顏色條 ---
    const int bar_width = 28;
    const int bar_height = 240;
    const int bar_x = SCREEN_WIDTH - bar_width - 20;
    const int bar_y = 60;
    for (int i = 0; i < bar_height; ++i) {
        float t = (float)i / (bar_height - 1); // 0~1
        float quality = MIN_QUALITY + t * (MAX_QUALITY - MIN_QUALITY);

        float q_norm = (quality - MIN_QUALITY) / (MAX_QUALITY - MIN_QUALITY);
        float hue = 0.83f - 0.83f * q_norm;
        float sat = 0.7f;
        float val = 0.3f + 0.7f * q_norm;

        float r_f = 0, g_f = 0, b_f = 0;
        float h = hue * 6.0f;
        int hi = (int)h;
        float f = h - hi;
        float p = val * (1.0f - sat);
        float qv = val * (1.0f - sat * f);
        float t_ = val * (1.0f - sat * (1.0f - f));
        switch (hi % 6) {
            case 0: r_f = val; g_f = t_; b_f = p; break;
            case 1: r_f = qv; g_f = val; b_f = p; break;
            case 2: r_f = p; g_f = val; b_f = t_; break;
            case 3: r_f = p; g_f = qv; b_f = val; break;
            case 4: r_f = t_; g_f = p; b_f = val; break;
            case 5: r_f = val; g_f = p; b_f = qv; break;
        }
        ALLEGRO_COLOR c = al_map_rgb_f(r_f, g_f, b_f);
        al_draw_filled_rectangle(bar_x, bar_y + bar_height - 1 - i, bar_x + bar_width, bar_y + bar_height - i, c);
    }
    // 邊框
    al_draw_rectangle(bar_x, bar_y, bar_x + bar_width, bar_y + bar_height, al_map_rgb(255,255,255), 2.0f);

    // 標示最大/最小品質值
    al_draw_textf(font, al_map_rgb(255,255,255), bar_x - 8, bar_y - 8, ALLEGRO_ALIGN_RIGHT, "%.1f", MAX_QUALITY);
    al_draw_textf(font, al_map_rgb(255,255,255), bar_x - 8, bar_y + bar_height - 16, ALLEGRO_ALIGN_RIGHT, "%.1f", MIN_QUALITY);
    al_draw_text(font, al_map_rgb(255,255,255), bar_x - 20, bar_y + bar_height + 8, 0, "品質");
    
}

void handle_minigame2_input(ALLEGRO_EVENT ev) {
    bool clicked_on_person_this_event = false;
    if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        switch (ev.keyboard.keycode) {
            case ALLEGRO_KEY_ESCAPE:
                game_phase = GROWTH;
                break;
            case ALLEGRO_KEY_Q:
                for (size_t i = 0; i < pop_count; ++i) {
                    if (population[i].is_selected_for_queue && population[i].is_adult &&
                        !is_person_immune_to_selection_change(&population[i])) { 
                        add_to_breeding_queue(population[i].id);
                        population[i].is_selected_for_queue = false; 
                    }
                }
                break;
            case ALLEGRO_KEY_E: {
                float total_atk_boost_session = 0.0f;
                bool any_eaten_this_press = false;

                for (int i = (int)pop_count - 1; i >= 0; i--) { 
                    if (population[i].is_selected_for_queue && population[i].is_adult) {
                        if (is_person_immune_to_selection_change(&population[i])) {
                            population[i].is_selected_for_queue = false; 
                            continue;
                        }
                        total_atk_boost_session += pow(10, 2.0f * population[i].quality * EAT_ATK_FACTOR - 3.0f);
                        any_eaten_this_press = true;
                        
                        remove_person(i); 
                    } else if (population[i].is_selected_for_queue && !population[i].is_adult) {
                        population[i].is_selected_for_queue = false;
                    }
                }
                if (any_eaten_this_press) {
                    temp_atk += total_atk_boost_session;
                }
                break;
            }
            case ALLEGRO_KEY_C:
                {
                    int breeding_now_id = -1;
                    if (is_breeding_in_progress && breeding_queue_count > 0) {
                        breeding_now_id = breeding_queue[0]; 
                    }

                    for(size_t i=0; i < pop_count; ++i) {
                        if (population[i].is_selected_for_queue) {
                            bool in_q = false;
                            for(size_t q_idx=0; q_idx < breeding_queue_count; ++q_idx) {
                                if(breeding_queue[q_idx] == population[i].id) {in_q=true; break;}
                            }
                            if(!in_q) population[i].is_selected_for_queue = false;
                        }
                    }
                    
                    if (breeding_now_id != -1) {
                        breeding_queue[0] = breeding_now_id;
                        breeding_queue_count = 1;
                    } else {
                        breeding_queue_count = 0;
                    }
                }
                break;
            // 金手指：按 G 鍵讓繁殖時間變 0，長大時間變 1
            case ALLEGRO_KEY_G:
                BREEDING_DURATION = 0.0;
                CHILD_MATURATION_TIME = 1.0;
                printf("Cheat activated: BREEDING_DURATION=0, CHILD_MATURATION_TIME=1\n");
                break;

        }
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES) {
        if (is_dragging_selection_circle) {
            current_mouse_drag_x = ev.mouse.x;
            current_mouse_drag_y = ev.mouse.y;
        }
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev.mouse.button == 1) {
            // 先檢查是否點到個體
            for (size_t i = 0; i < pop_count; i++) {
                float dx = ev.mouse.x - population[i].x;
                float dy = ev.mouse.y - population[i].y;
                float r_person = population[i].is_adult ? 15.0f : 10.0f;
                if (dx*dx + dy*dy < r_person*r_person) { 
                    clicked_on_person_this_event = true;
                    if (population[i].is_adult) {
                        if (!is_person_immune_to_selection_change(&population[i])) {
                            population[i].is_selected_for_queue = !population[i].is_selected_for_queue;
                        }
                    }
                    last_clicked_id = population[i].id;
                    // 不要在這裡設 is_dragging_selection_circle = false;
                    break; 
                }
            }
            // 無論有沒有點到個體，都可以開始 drag selection circle
            is_dragging_selection_circle = true;
            selection_drag_start_x = ev.mouse.x;
            selection_drag_start_y = ev.mouse.y;
            current_mouse_drag_x = ev.mouse.x; 
            current_mouse_drag_y = ev.mouse.y;
        } else if (ev.mouse.button == 2) { 
            for (size_t i = 0; i < pop_count; i++) {
                if (!is_person_immune_to_selection_change(&population[i])) {
                    if (population[i].is_selected_for_queue) {
                        population[i].is_selected_for_queue = false;
                    }
                }
            }
            is_dragging_selection_circle = false; 
        }
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
        if (ev.mouse.button == 1) {
            if (is_dragging_selection_circle) { 
                is_dragging_selection_circle = false;
                float final_radius_sq = pow(ev.mouse.x - selection_drag_start_x, 2) + pow(ev.mouse.y - selection_drag_start_y, 2);
                float final_radius = sqrt(final_radius_sq);

                if (final_radius >= 5.0f || clicked_on_person_this_event) { // 如果半徑大於 5.0f，則選擇在圓形內的成年個體
                    int newly_selected_count = 0;
                    for (size_t i = 0; i < pop_count; ++i) {
                        Person* p = &population[i];
                        if (p->is_adult && !is_person_immune_to_selection_change(p)) {
                            float dist_to_center_sq = pow(p->x - selection_drag_start_x, 2) + pow(p->y - selection_drag_start_y, 2);
                            if (dist_to_center_sq <= final_radius_sq) {
                                if (!p->is_selected_for_queue) { 
                                    p->is_selected_for_queue = true;
                                    newly_selected_count++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void update_minigame2(void) {
    static int last_game_phase = -1;
    double now = al_get_time();
    double dt = now - last_update;
    if (last_game_phase != BATTLE && game_phase == BATTLE && temp_atk > 0.0f) {
        player.strength += (int)temp_atk;
    }
    else if (last_game_phase == BATTLE && game_phase != BATTLE && temp_atk > 0.0f) {
        player.strength -= (int)temp_atk;
        clean_atk();
    }
    last_game_phase = game_phase;

    if (dt <= 0) return; 
    if (dt > 0.1) dt = 0.1; 
    game_time = now;

    // --- 領導點每分鐘有1/10機率隨機變換 ---
    if (game_time - last_leader_point_change_time >= 10.0) {
        if ((rand() % 10) == 0) { // 1/10 機率
            leader_point_x = 50 + rand() % (SCREEN_WIDTH - 100);
            leader_point_y = 50 + rand() % (SCREEN_HEIGHT - 100);
        }
        last_leader_point_change_time = game_time;
    }

    // 更新人口狀態
    for (size_t i = 0; i < pop_count; i++) {
        Person *p = &population[i];
        double parent_time_since_repro = time_since_repro(p);
        p->happiness = 1-exp( -(float)parent_time_since_repro*0.01);
        if (!p->is_adult && (game_time - p->birth_time >= CHILD_MATURATION_TIME)) {
            p->is_adult = true;
            p->last_reproduction_time = game_time - BREEDING_DURATION -1.0; 
        }
    }

    update_behavior();

    size_t current_pop_count_for_asexual_loop = pop_count; // 使用當前人口數量，避免在迴圈中修改時出現問題
    // 處理無性繁殖
    for (size_t i = 0; i < current_pop_count_for_asexual_loop; i++) {
        if (i >= pop_count) break; 

        if (population[i].is_adult) {
            if ((double)rand() / RAND_MAX < ASEXUAL_CHANCE_PER_SEC * dt) {
                int parent_id_for_log = population[i].id; 
                // 檢查個體是否免於選擇變更
                if (is_person_immune_to_selection_change(&population[i])) {
                    continue; 
                }
                int temp_parent_idx_check = find_by_id(parent_id_for_log);
                if (temp_parent_idx_check == -1 || !population[temp_parent_idx_check].is_adult) continue; 

                float parent_quality = population[temp_parent_idx_check].quality;

                
                float happiness_val = population[temp_parent_idx_check].happiness;
                float child_quality = parent_quality * PARENT_FACTOR * ASEXUAL_PENALTY + // 無性繁殖的品質值會有一定的懲罰
                                      happiness_val * (HAPPINESS_FACTOR * 0.5f) * ((float)rand() / RAND_MAX) + 
                                      ((float)rand() / RAND_MAX) * 0.4f - 0.2f; // 無性繁殖的快樂因子較低
                
                child_quality = fmaxf(MIN_QUALITY, fminf(MAX_QUALITY, child_quality)); // 確保品質值在合理範圍內
                
                add_person(child_quality, false); // 添加新個體到人口中，其中false表示這是兒童狀態

                // 更新父母的最後繁殖時間
                int parent_idx_after_add = find_by_id(parent_id_for_log);
                if(parent_idx_after_add != -1) {
                     population[parent_idx_after_add].last_reproduction_time = game_time;
                }
            }
        }
    }
    // 處理繁殖佇列
    if (is_breeding_in_progress) {
        if (game_time - current_breeding_start_time >= BREEDING_DURATION) {
            int parent_idx = find_by_id(current_breeding_parent_id); 

            if (parent_idx != -1 && population[parent_idx].is_adult) { 
                float parent_quality = population[parent_idx].quality;

                float happiness_val = population[parent_idx].happiness;
                float child_quality = parent_quality * PARENT_FACTOR + 
                                      happiness_val * HAPPINESS_FACTOR * ((float)rand() / RAND_MAX) +
                                      ((float)rand() / RAND_MAX) * 0.4f - 0.2f;
                child_quality = fmaxf(MIN_QUALITY, fminf(MAX_QUALITY, child_quality));
                
                add_person(child_quality, false);
                
                parent_idx = find_by_id(current_breeding_parent_id); 
                if (parent_idx != -1) { 
                    population[parent_idx].last_reproduction_time = game_time;
                }
            }
            remove_from_breeding_queue_front(); 
            is_breeding_in_progress = false;
            current_breeding_parent_id = -1;
        }
    }
    // 處理繁殖佇列中的下一個個體
    if (!is_breeding_in_progress && breeding_queue_count > 0) {
        int next_parent_id_to_breed = breeding_queue[0];
        int next_parent_idx = find_by_id(next_parent_id_to_breed);

        if (next_parent_idx != -1 && population[next_parent_idx].is_adult) {
            if (time_since_repro(&population[next_parent_idx]) >= BREEDING_DURATION + 0.1) {
                is_breeding_in_progress = true;
                current_breeding_start_time = game_time;
                current_breeding_parent_id = next_parent_id_to_breed;
            } 
        } else {
            remove_from_breeding_queue_front();
        }
    }

    last_update = now;
}