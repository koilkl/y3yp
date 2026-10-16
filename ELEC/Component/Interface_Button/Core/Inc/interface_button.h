#ifndef INTERFACE_BUTTON_H
#define INTERFACE_BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== 参数范围与消抖时间宏定义 ==================== */
#define ANKLE_ANGLE_MIN          (-30)   // Mode 1 最小目标角度: -30°
#define ANKLE_ANGLE_MAX          (30)    // Mode 1 最大目标角度: +30°
#define ANKLE_STRENGTH_MIN       (0)     // 最小力量/阻力负荷: 0 kg (完全卸力状态)
#define ANKLE_STRENGTH_MAX       (50)    // 最大力量/阻力负荷: 50 kg

#define BTN_DEBOUNCE_MS          (20U)   // 硬件消抖时间: 20 ms
#define BTN_LONG_PRESS_START_MS  (500U)  // 待机状态下长按满 500 ms 启动连续快调
#define BTN_REPEAT_INTERVAL_MS   (100U)  // 长按连续快调步进间隔: 100 ms (10 Hz)

/* ==================== 7个按钮在数组中的固定下标 ==================== */
typedef enum {
    BTN_MODE_TOGGLE = 0,  // [0] 切换训练模式: Mode 1 (被动抗阻) <-> Mode 2 (主动踩踏)
    BTN_AXIS_TOGGLE,      // [1] 切换运动轴: Roll (左右侧倾) <-> Pitch (前后俯仰)
    BTN_ANGLE_INC,        // [2] Angle + (仅在 Mode 1 下设定踏板目标角度；Mode 2 下踏板自由移动无需按钮控制)
    BTN_ANGLE_DEC,        // [3] Angle - (仅在 Mode 1 下设定踏板目标角度；Mode 2 下踏板自由移动无需按钮控制)
    BTN_STRENGTH_INC,     // [4] Strength + (增加踏板施加的力或阻力，Mode 1 & 2 均有效)
    BTN_STRENGTH_DEC,     // [5] Strength - (减小踏板施加的力或阻力，Mode 1 & 2 均有效)
    BTN_START_RESET,      // [6] Start / Reset 切换键: 启动加载力矩 <-> 紧急停止并卸除所有踏板力矩
    BTN_COUNT             // 按钮总数 = 7
} AnkleBtnIndex_t;

/* ==================== 运动轴与训练模式枚举 ==================== */
typedef enum {
    AXIS_ROLL = 0,        // Roll (Side-to-side / 左右侧倾)
    AXIS_PITCH = 1        // Pitch (Front-to-back / 前后俯仰)
} AnkleAxis_t;

typedef enum {
    TRAINING_MODE_1 = 0,  // Mode 1: 用户抵抗踏板主动施加的力 (可调 Target Angle 与 Strength)
    TRAINING_MODE_2 = 1   // Mode 2: 用户主动发力移动踏板 (角度随用户自由运动，无需按钮设定角度，仅调 Strength)
} AnkleMode_t;

/* ==================== 单个引脚硬件定义结构体 ==================== */
typedef struct {
    GPIO_TypeDef *port;   // GPIO 端口，例如 GPIOC
    uint16_t      pin;    // GPIO 引脚号，例如 GPIO_PIN_10
} AnkleBtnPin_t;

/* ==================== 单个按键内部消抖与防呆状态机 ==================== */
typedef struct {
    bool     raw_state;        // 当前采样原始状态 (true = 按下)
    bool     stable_state;     // 消抖后的稳定状态 (true = 按下)
    bool     armed;            // 上电防卡死保险：确保上电时未处于短路/压死状态，松开过一次才允许触发
    uint32_t last_change_tick; // 上次电平变化的时间戳
    uint32_t last_repeat_tick; // 上次触发长按连发的时间戳
} AnkleBtnFilter_t;

/* ==================== 踝关节训练器全局控制句柄 ==================== */
typedef struct {
    // 硬件引脚数组 (7个按钮) 与消抖滤波器
    AnkleBtnPin_t    pins[BTN_COUNT];
    AnkleBtnFilter_t filters[BTN_COUNT];

    // 业务状态数据 (供 UI 显示和电机控制器读取)
    AnkleAxis_t      axis;           // 当前运动轴: AXIS_ROLL 或 AXIS_PITCH
    AnkleMode_t      mode;           // 当前模式: TRAINING_MODE_1 (抗阻) 或 TRAINING_MODE_2 (主动移动)
    int16_t          angle;          // Mode 1 下的目标角度: [-30°, +30°] (Mode 2 下踏板自由移动，此值不限制踏板)
    int16_t          strength;       // 目标力量/阻力负荷: [0 kg, 50 kg] (Reset 时立即卸力归 0)
    bool             is_running;     // 力矩加载状态: true = Started (施力中), false = Reset (已卸力待机)

    // 可配置步长
    int16_t          angle_step;     // 每次按键角度变化步长 (如 1° 或 5°)
    int16_t          strength_step;  // 每次按键力量变化步长 (如 1 kg 或 2 kg)

    // 状态更新标志位 (方便外部判断是否需要刷新 UI 或发送串口数据)
    bool             state_changed;
} AnkleController_t;

/* ==================== 对外 API 接口函数 ==================== */

/**
 * @brief 初始化 7 个按键的 GPIO 引脚及控制器初始参数
 * @param ctrl          指向控制器句柄的指针
 * @param pin_array     长度为 7 的引脚定义数组 (AnkleBtnPin_t[7])
 * @param angle_step    角度加减步长 (若传入 <=0 则默认为 1)
 * @param strength_step 力量加减步长 (若传入 <=0 则默认为 1)
 */
void AnkleUI_Init(AnkleController_t *ctrl,
                  const AnkleBtnPin_t pin_array[BTN_COUNT],
                  int16_t angle_step,
                  int16_t strength_step);

/**
 * @brief 动态修改角度与力量的步长
 */
void AnkleUI_SetSteps(AnkleController_t *ctrl, int16_t angle_step, int16_t strength_step);

/**
 * @brief 非阻塞按键扫描与防呆状态更新函数 (在 main 的 while(1) 中直接调用即可)
 * @param ctrl 指向控制器句柄的指针
 * @return true 如果本次扫描有按键触发导致业务状态真正改变；false 状态未改变
 */
bool AnkleUI_Update(AnkleController_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_BUTTON_H */
