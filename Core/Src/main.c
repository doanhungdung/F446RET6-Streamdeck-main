/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_hid.h"
#include "lvgl.h"
#include "lcd_port.h"
#include "ui.h"                /* SquareLine: ui_init(), ui_Screen1..4, ui_Roller1, ui_Arc1, ... */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum { EB_NONE = 0, EB_SINGLE, EB_DOUBLE, EB_LONG } enc_evt_t;

typedef enum { SCR_HOME = 0, SCR_MENU, SCR_THEME, SCR_BRIGHT, SCR_GAME } screen_t;

typedef struct { lv_obj_t **btn; screen_t target; } menu_item_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ----- Nút nhấn PC0..PC5 ---------------------------------------------------- */
#define NUM_BUTTONS         6
#define BTN_NONE            0xFF   /* giá trị "không có nút nào đang nhấn"            */
#define BTN_DEBOUNCE_MS     20     /* trạng thái phải ổn định ngần này ms mới chấp nhận */

/* ----- Encoder TIM3 + nút nhấn PC8 ------------------------------------------ */
#define ENCODER_DIV         4      /* số xung TIM3 ứng với 1 nấc xoay */

#define EB_DEBOUNCE_MS      30
#define EB_DBL_MS           350    /* cửa sổ chờ lần nhấn thứ 2 */
#define EB_LONG_MS          800    /* ngưỡng nhấn giữ           */

/* ----- USB HID: modifier của keyboard report -------------------------------- */
#define MOD_CTRL            0x01
#define MOD_SHIFT           0x02
#define MOD_ALT             0x04
#define MOD_GUI             0x08   /* phím Windows */

/* ----- USB HID: keycode (USB HID usage) của các phím đang dùng -------------- */
#define KEY_A               0x04
#define KEY_C               0x06
#define KEY_D               0x07
#define KEY_E               0x08
#define KEY_L               0x0F
#define KEY_M               0x10
#define KEY_R               0x15
#define KEY_S               0x16
#define KEY_U               0x18
#define KEY_V               0x19
#define KEY_ENTER           0x28

/* ----- USB HID: Report ID và bit của Consumer report ------------------------ */
#define REPORT_ID_KEYBOARD  0x01
#define REPORT_ID_CONSUMER  0x02

#define VOL_INC_BIT         (1 << 0)
#define VOL_DEC_BIT         (1 << 1)
#define VOL_MUTE_BIT        (1 << 2)
#define BRIGHT_INC_BIT      (1 << 3)
#define BRIGHT_DEC_BIT      (1 << 4)

/* ----- USB HID: thời gian ---------------------------------------------------- */
#define HID_READY_TIMEOUT_MS    20  /* timeout chờ USB rảnh, tránh treo vô hạn nếu USB rớt */
#define HID_KEY_HOLD_MS         10  /* giữ phím đủ lâu để Windows nhận combo               */
#define HID_CONSUMER_HOLD_MS    2
#define START_MENU_WAIT_MS      500 /* chờ Start hiện ra                                   */
#define START_SEARCH_WAIT_MS    400 /* chờ Windows tìm ra kết quả                          */

/* ----- Giao diện -------------------------------------------------------------- */
/* Thời gian hiệu ứng trượt giữa các màn hình (SquareLine dùng 100 ms).
 * Nếu bị giật do tốc độ SPI thì thử tăng lên 200. */
#define SCREEN_ANIM_MS      100

#define WALLPAPER_START     2      /* wallpaper lúc khởi động (2 = NORMAL / logo) */

/* Mỗi nấc xoay ở màn BRIGHTNESS làm cung Arc1 thay đổi bấy nhiêu (chỉ để hiển thị,
 * độ sáng thật của máy tính điều khiển bằng phím HID nên không đọc lại được) */
#define ARC_STEP            10
#define ARC_UPDATE_MIN_MS   130    /* khoảng cách tối thiểu giữa 2 lần đổi arc, thử 60-150 ms */

/* ----- Game "Catch": hứng vật rơi bằng cách xoay encoder ---------------------
 *  Màn hình 240x240 chia thành lưới 8 cột. Hàng trên cùng dành cho điểm số,
 *  còn lại 7 hàng để chơi. Người chơi "U" ở hàng cuối, vật "o" rơi từ trên xuống.
 *  Hứng được thì +1 điểm và rơi nhanh hơn; hụt thì thua.
 */
#define GAME_COLS           8
#define GAME_ROWS           7
#define GAME_CELL           30     /* mỗi ô 30x30 px -> 8 cột = 240 px       */
#define GAME_TOP            30     /* hàng chữ điểm số nằm ở y = 0..29       */
#define GAME_START_MS       450    /* thời gian rơi 1 hàng lúc mới bắt đầu   */
#define GAME_MIN_MS         150    /* nhanh nhất                             */
#define GAME_SPEEDUP_MS     10     /* mỗi lần hứng được, rơi nhanh hơn ngần này */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* Số phần tử của mảng */
#define ARRAY_SIZE(a)       (sizeof(a) / sizeof((a)[0]))
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* hspi1 / hdma_spi1_tx KHÔNG dùng nữa: SPI1 + DMA2_Stream3 được cấu hình và
 * điều khiển trực tiếp bằng thanh ghi trong lcd_port.c (spi1_init(), v.v.) */

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;

uint8_t HID_Buffer[8] = {0};    /* giữ lại phòng file khác có extern */

/* --- 6 nút PC0..PC5 --- */
static uint8_t  btn_last     = BTN_NONE;   /* trạng thái ỔN ĐỊNH (đã qua debounce) */
static uint8_t  btn_raw_last = BTN_NONE;   /* lần đọc thô gần nhất                 */
static uint32_t btn_raw_tick = 0;          /* thời điểm lần đọc thô đổi gần nhất   */

/* --- Encoder --- */
static int16_t  enc_last_count = 0;

/* --- Trạng thái giao diện chung --- */
static screen_t    screen      = SCR_HOME;
static uint8_t     menu_index  = 0;
static uint8_t     wp_index    = WALLPAPER_START;
static const char *home_status = "Volume";
static uint32_t    arc_last_update = 0;

/* --- Game --- */
static uint8_t  game_player_col;
static uint8_t  game_item_col;
static uint8_t  game_item_row;
static uint16_t game_score;
static uint32_t game_period;       /* ms cho mỗi lần rơi 1 hàng */
static uint32_t game_last_tick;
static uint8_t  game_over;

/* --- Đối tượng LVGL do code tạo (các screen SquareLine đã có sẵn: ui_Screen1..4) --- */
static lv_obj_t *home_scr;         /* màn HOME                     */
static lv_obj_t *home_bg;          /* ảnh wallpaper, nằm dưới cùng */
static lv_obj_t *home_hint;        /* dòng trạng thái nhỏ ở dưới   */
static lv_obj_t *game_title;       /* điểm số                      */
static lv_obj_t *game_msg;         /* "Hoc lai!!!" khi thua        */
static lv_obj_t *game_player;      /* "U" trong game               */
static lv_obj_t *game_item;        /* "o" trong game               */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
static void    Buttons_Init(void);
static uint8_t Buttons_Scan(void);
static void    UI_Render(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ============================================================================
 *  1. USB HID (bàn phím + phím media)
 * ========================================================================== */

/* Chờ USB rảnh trước khi gửi report tiếp theo */
static void HID_WaitReady(void)
{
    USBD_HID_HandleTypeDef *hhid;
    uint32_t deadline = HAL_GetTick() + HID_READY_TIMEOUT_MS;

    do {
        hhid = (USBD_HID_HandleTypeDef *)hUsbDeviceFS.pClassDataCmsit[0];
    } while (hhid != NULL && hhid->state != USBD_HID_IDLE && HAL_GetTick() < deadline);
}

/* Gửi 1 tổ hợp phím (nhấn rồi nhả). Bấm 1 phím thường thì truyền mod = 0. */
static void HID_Keyboard_Combo(uint8_t mod, uint8_t keycode)
{
    uint8_t report[9] = {0};
    report[0] = REPORT_ID_KEYBOARD;
    report[1] = mod;
    report[3] = keycode;

    HID_WaitReady();
    USBD_HID_SendReport(&hUsbDeviceFS, report, sizeof(report));

    HAL_Delay(HID_KEY_HOLD_MS);
    report[1] = 0;
    report[3] = 0;

    HID_WaitReady();
    USBD_HID_SendReport(&hUsbDeviceFS, report, sizeof(report));
}

/* Bấm 1 phím đơn (không có modifier) */
static void HID_Key(uint8_t keycode)
{
    HID_Keyboard_Combo(0, keycode);
}

/* Gửi 1 phím media (âm lượng / độ sáng): nhấn rồi nhả */
static void HID_Consumer_Press(uint8_t bitmask)
{
    uint8_t report[2];
    report[0] = REPORT_ID_CONSUMER;
    report[1] = bitmask;

    HID_WaitReady();
    USBD_HID_SendReport(&hUsbDeviceFS, report, sizeof(report));

    HAL_Delay(HID_CONSUMER_HOLD_MS);
    report[1] = 0;

    HID_WaitReady();
    USBD_HID_SendReport(&hUsbDeviceFS, report, sizeof(report));
}

/* ============================================================================
 *  2. 6 NÚT NHẤN PC0..PC5
 * ========================================================================== */

/**
  * @brief  Cấu hình PC0..PC5 làm ngõ vào, Pull-up.
  */
static void Buttons_Init(void)
{
    uint8_t pin;

    RCC->AHB1ENR |= (1 << 2);                     /* GPIOCEN = bit 2 */

    for (pin = 0; pin < NUM_BUTTONS; pin++)
    {
        /* MODER: mỗi chân 2 bit, vị trí = pin*2; Input = 00b */
        GPIOC->MODER &= ~(0x3 << (pin * 2));

        /* PUPDR: mỗi chân 2 bit; Pull-up = 01b (xóa về 00b rồi set bit thấp) */
        GPIOC->PUPDR &= ~(0x3 << (pin * 2));
        GPIOC->PUPDR |=  (0x1 << (pin * 2));
    }
}

/**
  * @brief  Quét 6 nút PC0..PC5, trả về chỉ số nút đang nhấn (0..5),
  *         hoặc BTN_NONE nếu không có nút nào.
  *         Nút nối GND khi nhấn (active LOW) nên IDR = 0 nghĩa là đang nhấn.
  *         Đây là giá trị THÔ (chưa debounce), việc chống nảy do Buttons_Handle() làm.
  */
static uint8_t Buttons_Scan(void)
{
    uint8_t pin;

    for (pin = 0; pin < NUM_BUTTONS; pin++)
    {
        if ((GPIOC->IDR & (1 << pin)) == 0)
        {
            return pin;
        }
    }
    return BTN_NONE;
}

/* Mở app Claude: Win -> gõ "claude" -> Enter */
static void Action_OpenClaudeApp(void)
{
    static const uint8_t claude_keys[] = { KEY_C, KEY_L, KEY_A, KEY_U, KEY_D, KEY_E };
    uint8_t i;

    HID_Keyboard_Combo(MOD_GUI, 0);             /* bấm Win: mở Start */
    HAL_Delay(START_MENU_WAIT_MS);

    for (i = 0; i < ARRAY_SIZE(claude_keys); i++)
    {
        HID_Key(claude_keys[i]);
    }

    HAL_Delay(START_SEARCH_WAIT_MS);
    HID_Key(KEY_ENTER);
}

/* Hành động của từng nút */
static void Button_Action(uint8_t idx)
{
    switch (idx)
    {
    case 0: HID_Keyboard_Combo(MOD_CTRL, KEY_C);             break;   /* PC0: Ctrl+C        */
    case 1: HID_Keyboard_Combo(MOD_CTRL, KEY_V);             break;   /* PC1: Ctrl+V        */
    case 2: HID_Keyboard_Combo(MOD_GUI | MOD_SHIFT, KEY_S);  break;   /* PC2: Win+Shift+S   */
    case 3: HID_Keyboard_Combo(MOD_CTRL, KEY_D);             break;   /* PC3: tắt mic       */
    case 4: Action_OpenClaudeApp();                          break;   /* PC4: mở app Claude */
    case 5: HID_Keyboard_Combo(MOD_GUI, KEY_L);              break;   /* PC5: Win+L (khóa máy) */
    default: break;
    }
}

/**
  * @brief  Xử lý 6 nút có DEBOUNCE, không chặn (non-blocking).
  *
  *         Chỉ chấp nhận trạng thái mới khi giá trị đọc thô giữ NGUYÊN không đổi
  *         liên tục >= BTN_DEBOUNCE_MS.
  *           - Đọc thô thay đổi -> ghi lại thời điểm, đếm lại từ đầu.
  *           - Ổn định đủ lâu và khác trạng thái đã chấp nhận -> cập nhật btn_last,
  *             nếu là sự kiện NHẤN thì gọi Button_Action() đúng 1 lần.
  *         Lọc nhiễu cả lúc nhấn lẫn lúc nhả, không lặp khi đang giữ.
  */
static void Buttons_Handle(void)
{
    uint8_t  raw = Buttons_Scan();
    uint32_t now = HAL_GetTick();

    if (raw != btn_raw_last)
    {
        btn_raw_last = raw;
        btn_raw_tick = now;
        return;
    }

    if ((now - btn_raw_tick >= BTN_DEBOUNCE_MS) && (raw != btn_last))
    {
        btn_last = raw;

        if (raw != BTN_NONE)
        {
            Button_Action(raw);
        }
    }
}

/* ============================================================================
 *  3. ENCODER (TIM3) + NÚT NHẤN ENCODER (PC8)
 *
 *  Nút nhấn encoder phân biệt 3 kiểu: ấn 1 lần / ấn 2 lần / ấn giữ
 *  (EB_SINGLE, EB_DOUBLE, EB_LONG). Mỗi kiểu làm gì tùy màn hình, xem mục 5.
 * ========================================================================== */

/* Trả về +1 / -1 khi xoay đủ 1 nấc, 0 nếu chưa đủ */
static int8_t Encoder_ReadStep(void)
{
    int16_t count = (int16_t)TIM3->CNT;         /* đọc thẳng thanh ghi đếm của TIM3 */
    int16_t diff  = count - enc_last_count;

    if (diff >= ENCODER_DIV)  { enc_last_count = count; return 1;  }
    if (diff <= -ENCODER_DIV) { enc_last_count = count; return -1; }
    return 0;
}

/* Máy trạng thái phân biệt ấn 1 lần / ấn 2 lần / ấn giữ của nút PC8 */
static enc_evt_t Encoder_ButtonEvent(void)
{
    enum { ST_IDLE = 0, ST_PRESS_1, ST_WAIT_2ND, ST_WAIT_RELEASE };

    static uint8_t  st = ST_IDLE;
    static uint32_t t  = 0;

    uint8_t  down = ((GPIOC->IDR & (1 << 8)) == 0);   /* PC8 (ENC_SW), active LOW */
    uint32_t now  = HAL_GetTick();

    switch (st)
    {
    case ST_IDLE:                                   /* rảnh */
        if (down) { st = ST_PRESS_1; t = now; }
        break;

    case ST_PRESS_1:                                /* đang nhấn lần 1 */
        if (!down)
        {
            st = (now - t >= EB_DEBOUNCE_MS) ? ST_WAIT_2ND : ST_IDLE;   /* nhả sớm quá = nhiễu */
            t  = now;
        }
        else if (now - t >= EB_LONG_MS)
        {
            st = ST_WAIT_RELEASE;
            return EB_LONG;
        }
        break;

    case ST_WAIT_2ND:                               /* chờ lần nhấn thứ 2 */
        if (now - t < EB_DEBOUNCE_MS) break;        /* bỏ qua nảy lúc nhả */
        if (down) { st = ST_WAIT_RELEASE; return EB_DOUBLE; }
        if (now - t >= EB_DBL_MS) { st = ST_IDLE; return EB_SINGLE; }
        break;

    case ST_WAIT_RELEASE:                           /* chờ nhả hẳn */
        if (!down) st = ST_IDLE;
        break;
    }
    return EB_NONE;
}

/* ============================================================================
 *  4. GIAO DIỆN: HOME (wallpaper, viết tay) + MENU / THEME / BRIGHT / GAME (SquareLine)
 *
 *  Màn hình SquareLine (giữ nguyên bản export):
 *      Screen1 = MENU   (Button1 = THEME, Button3 = BRIGHTNESS, Button4 = GAME)
 *      Screen2 = THEME  (Roller1: PIXEL / DORAEMON / NORMAL)  -> chọn wallpaper
 *      Screen3 = BRIGHT (Arc1)
 *      Screen4 = GAME   (trống, label game được tạo bằng code ở Game_Create())
 *
 *  Việc chuyển màn hình do state machine này làm (Screen_Go), không dùng event
 *  click của SquareLine (không có cảm ứng nên các event đó không bao giờ chạy).
 * ========================================================================== */

/* ----- Các mục trong MENU (SquareLine Screen1): thứ tự từ trên xuống dưới ---- */
static const menu_item_t menu_items[] = {
    { &ui_Button1, SCR_THEME  },   /* nút "THEME"      */
    { &ui_Button3, SCR_BRIGHT },   /* nút "BRIGHTNESS" */
    { &ui_Button4, SCR_GAME   },   /* nút "GAME"       */
};
#define MENU_COUNT  ARRAY_SIZE(menu_items)

/* ----- Wallpaper: THỨ TỰ PHẢI KHỚP với các mục của Roller1 trong SquareLine ---
 *  Roller1: "HCMUTE-PIXEL\nHCMUTE-DORAEMON\nHCMUTE-NORMAL"
 *  Muốn thêm ảnh: thêm dòng extern + 1 dòng trong bảng + 1 mục trong Roller1.
 */
extern const lv_image_dsc_t hcmute_logo;    /* hcmute_logo.c  */
extern const lv_image_dsc_t hcmute_pixel;   /* hcmute_pixel.c */
extern const lv_image_dsc_t hcmute_dore;    /* hcmute_dore.c  */

static const lv_image_dsc_t * const wallpapers[] = {
    &hcmute_pixel,     /* 0: HCMUTE-PIXEL    */
    &hcmute_dore,      /* 1: HCMUTE-DORAEMON */
    &hcmute_logo,      /* 2: HCMUTE-NORMAL   */
};
#define WALLPAPER_COUNT  ARRAY_SIZE(wallpapers)

/* ----- Tiện ích chung -------------------------------------------------------- */

/* Tăng / giảm chỉ số, quay vòng khi vượt biên */
static uint8_t Wrap(uint8_t index, int8_t step, uint8_t count)
{
    if (step > 0)
    {
        return (uint8_t)((index + 1) % count);
    }
    return (uint8_t)((index + count - 1) % count);
}

static void Show(lv_obj_t *obj, uint8_t visible)
{
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

/* ----- Wallpaper -------------------------------------------------------------
 *  Chỉ áp wallpaper đang chọn lên màn HOME. Screen2 (THEME) giữ nguyên nền
 *  do SquareLine thiết kế.
 */
static void Wallpaper_Apply(void)
{
    lv_image_set_src(home_bg, wallpapers[wp_index]);
}

/* ----- Màn HOME (wallpaper + 1 dòng trạng thái) ------------------------------ */
static void Home_Create(void)
{
    home_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(home_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(home_scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(home_scr, lv_color_white(), LV_PART_MAIN);

    /* Tạo ảnh nền TRƯỚC để nó nằm dưới label */
    home_bg = lv_image_create(home_scr);
    lv_obj_center(home_bg);

    /* Dòng trạng thái nhỏ ở dưới, nền đen mờ ôm sát chữ */
    home_hint = lv_label_create(home_scr);
    lv_obj_set_style_text_align(home_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(home_hint, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home_hint, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(home_hint, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(home_hint, 2, LV_PART_MAIN);
    lv_obj_set_style_text_font(home_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(home_hint, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_label_set_text(home_hint, home_status);
}

/* ----- Game ------------------------------------------------------------------ */

/* Tạo label cho game lên Screen4 của SquareLine (gọi sau ui_init()) */
static void Game_Create(void)
{
    lv_obj_set_style_bg_color(ui_Screen4, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Screen4, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_Screen4, lv_color_white(), LV_PART_MAIN);

    game_title  = lv_label_create(ui_Screen4);
    game_msg    = lv_label_create(ui_Screen4);
    game_player = lv_label_create(ui_Screen4);
    game_item   = lv_label_create(ui_Screen4);

    lv_obj_align(game_title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_align(game_msg,   LV_ALIGN_CENTER,  0, 0);
    lv_label_set_text(game_msg, "Hoc lai!!!");

    /* Người chơi và vật rơi: mỗi cái rộng đúng 1 ô để chữ tự canh giữa ô */
    lv_obj_set_width(game_player, GAME_CELL);
    lv_obj_set_width(game_item,   GAME_CELL);
    lv_obj_set_style_text_align(game_player, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_align(game_item,   LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(game_player, "U");
    lv_label_set_text(game_item,   "o");
}

static void Game_Spawn(void)
{
    game_item_col = (uint8_t)lv_rand(0, GAME_COLS - 1);
    game_item_row = 0;
}

static void Game_Start(void)
{
    game_player_col = GAME_COLS / 2;
    game_score      = 0;
    game_period     = GAME_START_MS;
    game_over       = 0;
    game_last_tick  = HAL_GetTick();
    Game_Spawn();
}

/* Xoay encoder: dịch người chơi sang trái / phải, không quay vòng */
static void Game_Move(int8_t step)
{
    if (game_over) return;

    if (step > 0 && game_player_col < GAME_COLS - 1) game_player_col++;
    if (step < 0 && game_player_col > 0)             game_player_col--;
}

/* Gọi liên tục trong vòng lặp chính: cho vật rơi mỗi khi hết game_period */
static void Game_Update(void)
{
    if (screen != SCR_GAME || game_over) return;
    if (HAL_GetTick() - game_last_tick < game_period) return;

    game_last_tick = HAL_GetTick();
    game_item_row++;

    if (game_item_row >= GAME_ROWS - 1)           /* vật đã chạm hàng của người chơi */
    {
        if (game_item_col == game_player_col)     /* hứng được */
        {
            game_score++;
            if (game_period > GAME_MIN_MS) game_period -= GAME_SPEEDUP_MS;
            Game_Spawn();
        }
        else                                      /* hụt -> thua */
        {
            game_over = 1;
        }
    }
    UI_Render();
}

static void Game_Render(void)
{
    lv_label_set_text_fmt(game_title, "Score: %d", (int)game_score);
    Show(game_msg, game_over);                    /* chỉ hiện khi thua */
    lv_obj_set_pos(game_player, game_player_col * GAME_CELL,
                   GAME_TOP + (GAME_ROWS - 1) * GAME_CELL);
    lv_obj_set_pos(game_item,   game_item_col * GAME_CELL,
                   GAME_TOP + game_item_row * GAME_CELL);
}

/* ----- MENU: nút đang chọn được đặt trạng thái FOCUSED (kiểu dáng do SquareLine) */
static void Menu_Render(void)
{
    uint8_t i;

    for (i = 0; i < MENU_COUNT; i++)
    {
        lv_obj_t *b = *menu_items[i].btn;

        if (i == menu_index) lv_obj_add_state(b, LV_STATE_FOCUSED);
        else                 lv_obj_remove_state(b, LV_STATE_FOCUSED);
    }
}

/* ----- Vẽ lại phần thay đổi của màn hình hiện tại ---------------------------- */
static void UI_Render(void)
{
    switch (screen)
    {
    case SCR_HOME: lv_label_set_text(home_hint, home_status); break;
    case SCR_MENU: Menu_Render();                             break;
    case SCR_GAME: Game_Render();                             break;
    default: break;   /* THEME / BRIGHT: roller và arc tự cập nhật trong OnRotate() */
    }
}

/* ----- Chuyển màn hình -------------------------------------------------------- */
static lv_obj_t *Screen_Object(screen_t s)
{
    switch (s)
    {
    case SCR_MENU:   return ui_Screen1;
    case SCR_THEME:  return ui_Screen2;
    case SCR_BRIGHT: return ui_Screen3;
    case SCR_GAME:   return ui_Screen4;
    default:         return home_scr;
    }
}

/* Đi "tiến" (số thứ tự màn hình tăng) thì trượt sang trái, quay lại thì trượt sang phải */
static void Screen_Go(screen_t next)
{
    screen_t  prev = screen;
    lv_obj_t *obj  = Screen_Object(next);

    screen = next;
    if (next == SCR_GAME) Game_Start();
    UI_Render();                              /* vẽ nội dung trước khi trượt */

    if (lv_screen_active() == obj) return;    /* ví dụ chơi lại game: không cần trượt */

    lv_screen_load_anim(obj,
                        (next > prev) ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                        SCREEN_ANIM_MS, 0, false);
}

/* Khởi tạo toàn bộ giao diện (gọi sau lcd_init) */
static void UI_Init(void)
{
    ui_init();                 /* SquareLine: tạo Screen1..4 (và tự load Screen1) */
    Home_Create();             /* HOME: wallpaper + chữ                           */
    Game_Create();             /* label game trên Screen4                         */

    lv_roller_set_selected(ui_Roller1, wp_index, LV_ANIM_OFF);
    lv_rand_set_seed(HAL_GetTick());
    Wallpaper_Apply();

    /* Lúc khởi động load thẳng HOME, không trượt */
    screen = SCR_HOME;
    lv_screen_load(home_scr);
    UI_Render();
}

/* ============================================================================
 *  5. XỬ LÝ SỰ KIỆN ENCODER
 *
 *    Màn hình    | Xoay                 | Ấn 1 lần            | Ấn 2 lần | Ấn giữ
 *    ------------+----------------------+---------------------+----------+---------
 *    HOME        | Volume +/-           | Mute                | vào MENU | -
 *    MENU        | chọn nút             | vào mục đang chọn   | -        | về HOME
 *    THEME       | đổi wallpaper        | xong (về MENU)      | -        | về MENU
 *    BRIGHTNESS  | chỉnh độ sáng (HID)  | xong (về MENU)      | -        | về MENU
 *    GAME        | di chuyển            | chơi lại (khi thua) | -        | về MENU
 * ========================================================================== */

/* ----- Nút nhấn encoder (1 lần / 2 lần / giữ) --------------------------------- */
static void OnButtonEvent(enc_evt_t e)
{
    if (e == EB_NONE) return;

    switch (screen)
    {
    case SCR_HOME:
        if (e == EB_SINGLE)
        {
            HID_Consumer_Press(VOL_MUTE_BIT);
            home_status = "Mute";
            UI_Render();
        }
        else if (e == EB_DOUBLE)
        {
            Screen_Go(SCR_MENU);
        }
        break;

    case SCR_MENU:
        if (e == EB_SINGLE)     Screen_Go(menu_items[menu_index].target);
        else if (e == EB_LONG)  Screen_Go(SCR_HOME);
        break;

    case SCR_THEME:
    case SCR_BRIGHT:
        if (e == EB_SINGLE || e == EB_LONG) Screen_Go(SCR_MENU);
        break;

    case SCR_GAME:
        if (e == EB_SINGLE && game_over)  Screen_Go(SCR_GAME);   /* chơi lại */
        else if (e == EB_LONG)            Screen_Go(SCR_MENU);
        break;
    }
}

/* ----- Xoay encoder ----------------------------------------------------------- */
static void OnRotate(int8_t step)
{
    switch (screen)
    {
    case SCR_HOME:
        HID_Consumer_Press(step > 0 ? VOL_INC_BIT : VOL_DEC_BIT);
        home_status = (step > 0) ? "Volume +" : "Volume -";
        break;

    case SCR_MENU:
        menu_index = Wrap(menu_index, step, (uint8_t)MENU_COUNT);
        break;

    case SCR_THEME:
        wp_index = Wrap(wp_index, step, (uint8_t)WALLPAPER_COUNT);
        lv_roller_set_selected(ui_Roller1, wp_index, LV_ANIM_ON);
        Wallpaper_Apply();                   /* đổi wallpaper HOME ngay */
        break;

    case SCR_BRIGHT:
    {
        uint32_t now = HAL_GetTick();

        /* Nếu chưa đủ thời gian thì bỏ qua nấc xoay này: không gửi phím, không đổi arc */
        if (now - arc_last_update >= ARC_UPDATE_MIN_MS)
        {
            arc_last_update = now;
            HID_Consumer_Press(step > 0 ? BRIGHT_INC_BIT : BRIGHT_DEC_BIT);
            _ui_arc_increment(ui_Arc1, (step > 0) ? ARC_STEP : -ARC_STEP);
        }
        break;
    }

    case SCR_GAME:
        Game_Move(step);
        break;
    }
    UI_Render();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  /* MX_SPI1_Init() đã bỏ: SPI1 được lcd_init() -> spi1_init() (trong lcd_port.c) cấu hình bằng thanh ghi */
  MX_USB_DEVICE_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  lcd_init();
  UI_Init();                           /* ui_init() của SquareLine + tạo màn HOME (wallpaper) và game */

  Buttons_Init();

  /* Bật encoder TIM3 bằng thanh ghi */
  TIM3->CCER |= (1 << 0);                /* CC1E: bật kênh 1 (PC6) */
  TIM3->CCER |= (1 << 4);                /* CC2E: bật kênh 2 (PC7) */
  TIM3->CR1  |= (1 << 0);                /* CEN : cho bộ đếm chạy  */
  enc_last_count = (int16_t)TIM3->CNT;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    int8_t step;

    lv_timer_handler();                    /* cập nhật màn hình LVGL          */

    Buttons_Handle();                      /* 6 nút PC0..PC5 (có debounce)    */
    OnButtonEvent(Encoder_ButtonEvent());  /* nút nhấn encoder                */

    step = Encoder_ReadStep();             /* xoay encoder                    */
    if (step != 0)
    {
      OnRotate(step);
    }

    Game_Update();                         /* vật rơi trong game (nếu đang chơi) */
    HAL_Delay(5);
  }
  /* USER CODE END 3 */
}

/**
  * @brief  Cấu hình xung nhịp hệ thống bằng thanh ghi
  *         (thay cho HAL_RCC_OscConfig + HAL_RCC_ClockConfig)
  *
  *         HSE -> PLL (M=4, N=90, P=2, Q=5, R=2) -> SYSCLK = 90 MHz
  *         APB1 = /2
  *         APB2 = /1
  *         Flash = 2 wait state, Voltage Scale 3
  */
void SystemClock_Config(void)
{
  /* ===== Bước 1: bật clock cho PWR và chọn Voltage Scale 3 ============== */
  RCC->APB1ENR |= (1 << 28);              /* PWREN = bit 28 */
  (void)RCC->APB1ENR;                   /* đọc lại 1 lần để clock kịp ổn định */

  PWR->CR &= ~(0x3 << 14);              /* xóa 2 bit VOS [15:14] */
  PWR->CR |=  (0x1 << 14);              /* VOS = 01b -> Scale 3  */

  /* ===== Bước 2: bật HSE (thạch anh ngoài) và chờ ổn định ================ */
  RCC->CR |= (1 << 16);                       /* HSEON */
  while ((RCC->CR & (1 << 17)) == 0);         /* chờ HSERDY */

  /* ===== Bước 3: cấu hình PLL (phải làm khi PLL đang tắt) ================ */
  RCC->PLLCFGR = (4  << 0)      /* PLLM   [5:0]   = 4                   */
               | (90 << 6)      /* PLLN   [14:6]  = 90                  */
               | (0  << 16)     /* PLLP   [17:16] = 00b -> chia 2       */
               | (1  << 22)     /* PLLSRC bit 22  = 1   -> nguồn là HSE */
               | (5  << 24)     /* PLLQ   [27:24] = 5                   */
               | (2  << 28);    /* PLLR   [30:28] = 2                   */

  /* ===== Bước 4: bật PLL và chờ khóa ===================================== */
  RCC->CR |= (1 << 24);                       /* PLLON */
  while ((RCC->CR & (1 << 25)) == 0);         /* chờ PLLRDY */

  /* ===== Bước 5: Flash latency = 2 wait state (PHẢI làm trước khi đổi SYSCLK) */
  FLASH->ACR &= ~(0xF << 0);                /* xóa LATENCY [3:0] */
  FLASH->ACR |=  (2   << 0);                /* LATENCY = 2       */
  while ((FLASH->ACR & 0xF) != 2);          /* đọc lại để chắc chắn đã nhận */

  /* ===== Bước 6: hệ số chia cho các bus (RCC->CFGR) ====================== */
  RCC->CFGR &= ~(0xF << 4);                 /* HPRE  [7:4]   = 0000b -> AHB  /1 */

  RCC->CFGR &= ~(0x7 << 10);                /* xóa PPRE1 [12:10]                */
  RCC->CFGR |=  (0x4 << 10);                /* PPRE1 = 100b -> APB1 /2          */

  RCC->CFGR &= ~(0x7 << 13);                /* PPRE2 [15:13] = 000b -> APB2 /1  */

  /* ===== Bước 7: chọn PLL làm nguồn SYSCLK =============================== */
  RCC->CFGR &= ~(0x3 << 0);                 /* xóa SW [1:0]                     */
  RCC->CFGR |=  (0x2 << 0);                 /* SW = 10b -> chọn PLL             */
  while ((RCC->CFGR & (0x3 << 2)) != (0x2 << 2));   /* chờ SWS [3:2] = 10b      */

  /* ===== Bước 8: tạo clock 48 MHz cho USB từ PLLSAI (khớp file .ioc) ======
   *  HSE 8 MHz / PLLSAIM(4) = 2 MHz -> x PLLSAIN(96) = 192 MHz -> / PLLSAIP(4) = 48 MHz
   *  PLLQ của PLL chính chỉ ra 180/5 = 36 MHz nên KHÔNG dùng được cho USB. */
  RCC->PLLSAICFGR &= ~((0x3F  << 0)         /* xóa PLLSAIM [5:0]   */
                     | (0x1FF << 6)         /* xóa PLLSAIN [14:6]  */
                     | (0x3   << 16)        /* xóa PLLSAIP [17:16] */
                     | (0xF   << 24));      /* xóa PLLSAIQ [27:24] */
  RCC->PLLSAICFGR |=  ((4  << 0)            /* PLLSAIM = 4                              */
                     | (96 << 6)            /* PLLSAIN = 96                             */
                     | (1  << 16)           /* PLLSAIP = 01b -> chia 4                  */
                     | (2  << 24));         /* PLLSAIQ = 2 (không dùng, giá trị hợp lệ) */

  RCC->DCKCFGR2 |= (1 << 27);                 /* CK48MSEL: USB lấy 48 MHz từ PLLSAI-P */

  RCC->CR |= (1 << 28);                       /* PLLSAION */
  while ((RCC->CR & (1 << 29)) == 0);         /* chờ PLLSAIRDY */

  /* ===== Bước 9: báo cho HAL biết tốc độ mới ============================= */
  /* HAL_Init() đã cấu hình SysTick theo HSI 16 MHz cũ. Nếu không cấu hình lại,
   * HAL_GetTick()/HAL_Delay() sẽ chạy sai và LVGL (dùng HAL_GetTick) cũng lệch theo. */
  SystemCoreClockUpdate();                  /* cập nhật biến SystemCoreClock */
  HAL_InitTick(TICK_INT_PRIORITY);          /* cấu hình lại SysTick = 1 ms   */
}

/* MX_SPI1_Init() đã bỏ hoàn toàn: xem spi1_init() trong lcd_port.c (thanh ghi) */

/**
  * @brief TIM3 Initialization Function (viết bằng thanh ghi)
  *        TIM3 chạy Encoder Mode TI1&TI2 (đếm cả 2 pha), ARR = 65535, lọc nhiễu = 10.
  *        Hàm này chỉ CẤU HÌNH; việc bật đếm nằm ở USER CODE 2 trong main().
  */
static void MX_TIM3_Init(void)
{
  /* ===== 1. Bật clock cho TIM3 (APB1) ==================================== */
  RCC->APB1ENR |= (1 << 1);               /* TIM3EN */
  (void)RCC->APB1ENR;

  /* ===== 2. Chân encoder: PC6 = TIM3_CH1, PC7 = TIM3_CH2 (AF2) =========== */
  /* Khớp file .ioc: PC6 -> TIM3_CH1, PC7 -> TIM3_CH2, không kéo lên/xuống.
   * Nếu module encoder không có điện trở kéo lên sẵn thì đổi PUPDR sang 01b.
   * (Clock GPIOC đã được bật trong MX_GPIO_Init) */
  /* MODER: 10b = Alternate Function */
  GPIOC->MODER   &= ~((0x3 << (6 * 2)) | (0x3 << (7 * 2)));
  GPIOC->MODER   |=  ((0x2 << (6 * 2)) | (0x2 << (7 * 2)));

  /* OTYPER: 0 = push-pull */
  GPIOC->OTYPER  &= ~((1 << 6) | (1 << 7));

  /* OSPEEDR: 00b = tốc độ thấp */
  GPIOC->OSPEEDR &= ~((0x3 << (6 * 2)) | (0x3 << (7 * 2)));

  /* PUPDR: 00b = không kéo */
  GPIOC->PUPDR   &= ~((0x3 << (6 * 2)) | (0x3 << (7 * 2)));

  /* AFR[0] (AFRL) điều khiển chân 0..7, mỗi chân 4 bit -> vị trí = pin * 4
   * AF2 = 0010b = TIM3 */
  GPIOC->AFR[0]  &= ~((0xF << (6 * 4)) | (0xF << (7 * 4)));
  GPIOC->AFR[0]  |=  ((0x2 << (6 * 4)) | (0x2 << (7 * 4)));

  /* ===== 3. Cấu hình phần đếm của TIM3 =================================== */
  TIM3->CR1 = 0;                        /* CKD = 00 (chia 1), ARPE = 0, đếm lên, chưa bật CEN */
  TIM3->CR2 = 0;                        /* MMS = 000: TRGO = Reset (không dùng master mode)   */
  TIM3->PSC = 0;                        /* Prescaler = 0     */
  TIM3->ARR = 65535;                    /* Period    = 65535 */

  /* ===== 4. Chế độ Encoder =============================================== */
  TIM3->SMCR = (0x3 << 0);              /* SMS [2:0] = 011b: Encoder mode 3 (đếm ở cả TI1 và TI2),
                                           MSM = 0 (Master/Slave tắt) */

  /* ===== 5. Kênh 1 và kênh 2 làm ngõ vào (Input Capture) ================= */
  TIM3->CCMR1 = (0x1 << 0)              /* CC1S   [1:0]   = 01b: IC1 nối với TI1 (direct) */
              | (0x0 << 2)              /* IC1PSC [3:2]   = 00b: không chia               */
              | (10  << 4)              /* IC1F   [7:4]   = 10 : bộ lọc nhiễu             */
              | (0x1 << 8)              /* CC2S   [9:8]   = 01b: IC2 nối với TI2 (direct) */
              | (0x0 << 10)             /* IC2PSC [11:10] = 00b: không chia               */
              | (10  << 12);            /* IC2F   [15:12] = 10 : bộ lọc nhiễu             */

  /* CCER: CC1P (bit 1), CC1NP (bit 3), CC2P (bit 5), CC2NP (bit 7) = 0 -> cạnh lên.
   * CC1E (bit 0) và CC2E (bit 4) để 0 ở đây, sẽ bật khi khởi động encoder. */
  TIM3->CCER = 0;

  /* ===== 6. Nạp PSC/ARR vào thanh ghi shadow, đưa bộ đếm về 0 ============ */
  TIM3->EGR = (1 << 0);                   /* UG: tạo update event */
  TIM3->CNT = 0;
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{
  RCC->AHB1ENR |= (1 << 22);              /* DMA2EN */
  (void)RCC->AHB1ENR;

  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}

/**
  * @brief GPIO Initialization Function (viết bằng thanh ghi)
  *
  *  PA2 (CS), PA3 (RST), PA4 (DC), PA6 : ngõ ra cho LCD
  *  PC8 (ENC_SW)                        : ngõ vào, pull-up (nút nhấn encoder)
  *  PC0..PC5 được cấu hình riêng trong Buttons_Init().
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* ===== 1. Bật clock các cổng GPIO (RCC->AHB1ENR) ====================== */
  RCC->AHB1ENR |= (1 << 0);               /* GPIOAEN */
  RCC->AHB1ENR |= (1 << 1);               /* GPIOBEN */
  RCC->AHB1ENR |= (1 << 2);               /* GPIOCEN */
  RCC->AHB1ENR |= (1 << 7);               /* GPIOHEN (chân thạch anh HSE) */
  (void)RCC->AHB1ENR;                   /* đọc lại để clock kịp ổn định */

  /* ===== 2. Ngõ ra: PA2 (CS), PA3 (RST), PA4 (DC), PA6 =================== */
  /* Mức ban đầu = 0 (làm TRƯỚC khi chuyển sang output để không bị glitch) */
  GPIOA->ODR &= ~((1 << 2) | (1 << 3) | (1 << 4) | (1 << 6));

  /* MODER = 01b (Output) */
  GPIOA->MODER   &= ~((0x3 << (2 * 2)) | (0x3 << (3 * 2)) | (0x3 << (4 * 2)) | (0x3 << (6 * 2)));
  GPIOA->MODER   |=  ((0x1 << (2 * 2)) | (0x1 << (3 * 2)) | (0x1 << (4 * 2)) | (0x1 << (6 * 2)));

  /* OTYPER = 0 (Push-pull) */
  GPIOA->OTYPER  &= ~((1 << 2) | (1 << 3) | (1 << 4) | (1 << 6));

  /* OSPEEDR = 00b (Low speed) */
  GPIOA->OSPEEDR &= ~((0x3 << (2 * 2)) | (0x3 << (3 * 2)) | (0x3 << (4 * 2)) | (0x3 << (6 * 2)));

  /* PUPDR = 00b (No pull) */
  GPIOA->PUPDR   &= ~((0x3 << (2 * 2)) | (0x3 << (3 * 2)) | (0x3 << (4 * 2)) | (0x3 << (6 * 2)));

  /* ===== 3. Ngõ vào: PC8 (ENC_SW - nút nhấn encoder), pull-up ============ */
  GPIOC->MODER &= ~(0x3 << (8 * 2));    /* MODER = 00b (Input) */
  GPIOC->PUPDR &= ~(0x3 << (8 * 2));    /* xóa 2 bit cũ        */
  GPIOC->PUPDR |=  (0x1 << (8 * 2));    /* PUPDR = 01b (Pull-up) */

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
