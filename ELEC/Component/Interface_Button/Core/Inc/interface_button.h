/**
 ******************************************************************************
 * @file    interface_button.h
 * @brief   踝关节训练器：2颗加减力按钮 + 三态拨动开关(ON-OFF-ON) + 变力状态灯 + 6珠灯条
 ******************************************************************************
 */

#ifndef INTERFACE_BUTTON_H
#define INTERFACE_BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== 1. 输入通道定义 (2个按钮 + 拨动开关左右2路 = 4路输入) ==================== */
#define INPUT_COUNT 4

typedef enum {
    BTN_UP      = 0,   // 加力按钮 (+0.5 kg) -> PC12
    BTN_DOWN    = 1,   // 减力按钮 (-0.5 kg) -> PD2
    SW_POS_LEFT = 2,   // 三态拨动开关：左档引脚 -> PC10
    SW_POS_RIGHT= 3    // 三态拨动开关：右档引脚 -> PC11
} AnkleInputChannel_t;

/* ==================== 2. 三态拨动开关状态枚举 ==================== */
typedef enum {
    SW_STATE_CENTER_OFF = 0,  // 中间档：两边都不通电 (OFF / 待机)
    SW_STATE_LEFT_ON    = 1,  // 左档：左边通电 (Mode 1)
    SW_STATE_RIGHT_ON   = 2   // 右档：右边通电 (Mode 2)
} ToggleSwitchState_t;

/* ==================== 3. 引脚结构体与简写宏 ==================== */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} AnkleBtnPin_t;

#define PC10  (AnkleBtnPin_t){GPIOC, GPIO_PIN_10}
#define PC11  (AnkleBtnPin_t){GPIOC, GPIO_PIN_11}
#define PC12  (AnkleBtnPin_t){GPIOC, GPIO_PIN_12}
#define PD2   (AnkleBtnPin_t){GPIOD, GPIO_PIN_2}

/* ==================== 4. LED 与 6珠灯条硬件引脚分配 ==================== */
// 1) 常亮电源灯：PA5 (板载绿灯 LD2 / 正面母座 D13)
#define LED_POWER_PORT    GPIOA
#define LED_POWER_PIN     GPIO_PIN_5

// 2) 力改变时的闪烁状态灯：PA7 (正面母座 D11 / CN10 Pin 15)
#define LED_STATUS_PORT   GPIOA
#define LED_STATUS_PIN    GPIO_PIN_7
#define STATUS_LED_ON_MS  150          // 每次力改变时点亮 150ms

// 3) 6颗 WS2812B 幻彩灯条数据脚：PA6 (正面母座 D12 / CN10 Pin 13)
#define WS_PORT           GPIOA
#define WS_PIN            GPIO_PIN_6
#define NUM_LEDS          6

/* ==================== 5. 系统主控状态结构体 ==================== */
typedef struct {
    uint8_t             raw_value;           // 内部定点数：0 ~ 20 (对应 0.0 ~ 10.0 kg，步进 0.5 kg)
    uint8_t             min_raw;             // 最小定点值：0  (0.0 kg)
    uint8_t             max_raw;             // 最大定点值：20 (10.0 kg)
    uint8_t             display_percentage;  // 百分比进度：0 ~ 100%
    uint8_t             active_leds;         // 当前点亮的灯条灯珠数：0 ~ 6
    ToggleSwitchState_t sw_state;            // 三态拨动开关当前档位 (CENTER_OFF / LEFT_ON / RIGHT_ON)
    bool                btn_conflict;        // 防呆警告1：UP 和 DOWN 按钮同时按下
    bool                sw_fault;            // 防呆警告2：拨动开关左右两路同时短路导通异常
    bool                ui_updated;          // 状态改变标志位 (用于触发串口打印)
} AnkleCtrl_t;

/* ==================== 6. 外部接口函数 ==================== */
void AnkleUI_Init(const AnkleBtnPin_t* pin_config);
void AnkleUI_Update(AnkleCtrl_t* ctrl);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_BUTTON_H */
