/**
 ******************************************************************************
 * @file    interface_button.c
 * @brief   含开机灯条自检 + 0kg蓝光待机指示 + 500us强锁存 + D12(PA6)/D10(PB6)双输出
 ******************************************************************************
 */

#include "interface_button.h"

typedef struct {
    AnkleBtnPin_t pin_cfg;
    GPIO_PinState current_reading;
    GPIO_PinState stable_state;
    uint32_t      last_debounce_ms;
    uint32_t      press_start_ms;
    uint32_t      last_repeat_ms;
    bool          is_active;
    bool          armed;
} InputInternal_t;

static InputInternal_t inputs[INPUT_COUNT];

static bool     status_led_active   = false;
static uint32_t status_led_start_ms = 0;

static ToggleSwitchState_t last_sw_state = SW_STATE_CENTER_OFF;
static bool                first_sw_sync = true;

#define DEBOUNCE_DELAY_MS     20
#define AUTO_REPEAT_DELAY_MS  500
#define AUTO_REPEAT_RATE_MS   150

// 备用灯条信号脚：PB6 (正面母座 D10)，与 PA6 (正面母座 D12) 同步输出！
#define WS_BACKUP_PORT        GPIOB
#define WS_BACKUP_PIN         GPIO_PIN_6

static uint8_t rgb_data[NUM_LEDS][3];

// 6 颗灯珠渐变色谱：[G, R, B]
static const uint8_t COLOR_MAP[NUM_LEDS][3] = {
    {150, 0,   0},   // 第 1 颗：纯绿色 (0.5 ~ 1.5 kg)
    {130, 40,  0},   // 第 2 颗：草绿色 (2.0 ~ 3.0 kg)
    {110, 100, 0},   // 第 3 颗：亮黄色 (3.5 ~ 5.0 kg)
    {60,  150, 0},   // 第 4 颗：亮橙色 (5.5 ~ 6.5 kg)
    {20,  150, 0},   // 第 5 颗：深橙红 (7.0 ~ 8.5 kg)
    {0,   150, 0}    // 第 6 颗：纯红色 (9.0 ~ 10.0 kg)
};

static void Enable_GPIO_Clock(GPIO_TypeDef* port) {
    if (port == GPIOA)      __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
}

/* ==================== 强制 O2 + DWT 硬件时序 + 500us 强锁存复位 ==================== */
__attribute__((optimize("O2"))) static void WS2812_Show(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    const uint32_t clk_mhz    = SystemCoreClock / 1000000UL;
    const uint32_t t0h_cycles = (clk_mhz * 350) / 1000;   // 0码高电平 ~350ns
    const uint32_t t1h_cycles = (clk_mhz * 800) / 1000;   // 1码高电平 ~800ns
    const uint32_t tot_cycles = (clk_mhz * 1250) / 1000;  // 单bit总周期 ~1250ns
    const uint32_t rst_cycles = clk_mhz * 500;            // 500us 强锁存低电平复位

    __disable_irq();

    for (int i = 0; i < NUM_LEDS; i++) {
        for (int c = 0; c < 3; c++) {
            uint8_t byte = rgb_data[i][c];
            for (int b = 7; b >= 0; b--) {
                uint32_t start_cycle = DWT->CYCCNT;

                // 同时拉高 PA6 (D12) 和 PB6 (D10)
                WS_PORT->BSRR        = WS_PIN;
                WS_BACKUP_PORT->BSRR = WS_BACKUP_PIN;

                uint32_t high_wait = ((byte >> b) & 0x01) ? t1h_cycles : t0h_cycles;
                while ((DWT->CYCCNT - start_cycle) < high_wait) {}

                // 同时拉低 PA6 (D12) 和 PB6 (D10)
                WS_PORT->BSRR        = ((uint32_t)WS_PIN << 16);
                WS_BACKUP_PORT->BSRR = ((uint32_t)WS_BACKUP_PIN << 16);

                while ((DWT->CYCCNT - start_cycle) < tot_cycles) {}
            }
        }
    }

    // 关键修复：保持 >500us 纯低电平，确保新版 WS2812B 100% 锁存亮起！
    uint32_t rst_start = DWT->CYCCNT;
    WS_PORT->BSRR        = ((uint32_t)WS_PIN << 16);
    WS_BACKUP_PORT->BSRR = ((uint32_t)WS_BACKUP_PIN << 16);
    while ((DWT->CYCCNT - rst_start) < rst_cycles) {}

    __enable_irq();
}

static void WS2812_SetLED(uint8_t index, uint8_t g, uint8_t r, uint8_t b) {
    if (index >= NUM_LEDS) return;
    rgb_data[index][0] = g;
    rgb_data[index][1] = r;
    rgb_data[index][2] = b;
}

static uint8_t WS2812_UpdateStrip(uint8_t raw_value) {
    uint8_t num_lit = 0;
    if (raw_value > 0) {
        num_lit = (uint8_t)(((uint16_t)raw_value * NUM_LEDS + 10) / 20);
        if (num_lit > NUM_LEDS) num_lit = NUM_LEDS;
    }

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        if (i < num_lit) {
            WS2812_SetLED(i, COLOR_MAP[i][0], COLOR_MAP[i][1], COLOR_MAP[i][2]);
        } else {
            WS2812_SetLED(i, 0, 0, 0);
        }
    }

    // 当力为 0.0 kg 时，让第 1 颗灯珠亮微弱的待机冰蓝色 (B=40)，一眼确认灯条工作正常！
    if (raw_value == 0) {
        WS2812_SetLED(0, 0, 0, 40);
    }

    WS2812_Show();
    return num_lit;
}

static bool Step_ForceValue(AnkleCtrl_t* ctrl, int8_t dir) {
    if (dir > 0) {
        if (ctrl->raw_value < ctrl->max_raw) {
            ctrl->raw_value++;
            return true;
        }
    } else if (dir < 0) {
        if (ctrl->raw_value > ctrl->min_raw) {
            ctrl->raw_value--;
            return true;
        }
    }
    return false;
}

static void Trigger_StatusLED(uint32_t now) {
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_SET);
    status_led_active   = true;
    status_led_start_ms = now;
}

void AnkleUI_Init(const AnkleBtnPin_t* pin_config) {
    uint32_t now = HAL_GetTick();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 1. 初始化 4 路输入引脚 (PULLUP)
    for (int i = 0; i < INPUT_COUNT; i++) {
        inputs[i].pin_cfg = pin_config[i];
        Enable_GPIO_Clock(inputs[i].pin_cfg.port);

        GPIO_InitStruct.Pin   = inputs[i].pin_cfg.pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(inputs[i].pin_cfg.port, &GPIO_InitStruct);

        GPIO_PinState init_pin = HAL_GPIO_ReadPin(inputs[i].pin_cfg.port, inputs[i].pin_cfg.pin);
        inputs[i].current_reading  = init_pin;
        inputs[i].stable_state     = init_pin;
        inputs[i].last_debounce_ms = now;
        inputs[i].press_start_ms   = 0;
        inputs[i].last_repeat_ms   = 0;

        if (i == BTN_UP || i == BTN_DOWN) {
            inputs[i].is_active = false;
            inputs[i].armed     = (init_pin == GPIO_PIN_SET);
        } else {
            inputs[i].is_active = (init_pin == GPIO_PIN_RESET);
            inputs[i].armed     = true;
        }
    }

    // 2. 初始化常亮电源灯 PA5 (LD2) 與 变力状态灯 PA7 (D11)
    Enable_GPIO_Clock(LED_POWER_PORT);
    GPIO_InitStruct.Pin   = LED_POWER_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_POWER_PORT, &GPIO_InitStruct);

    Enable_GPIO_Clock(LED_STATUS_PORT);
    GPIO_InitStruct.Pin   = LED_STATUS_PIN;
    HAL_GPIO_Init(LED_STATUS_PORT, &GPIO_InitStruct);

    // 3. 初始化灯条主引脚 PA6 (正面母座 D12) 与 备用引脚 PB6 (正面母座 D10)
    Enable_GPIO_Clock(WS_PORT);
    GPIO_InitStruct.Pin   = WS_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(WS_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WS_PORT, WS_PIN, GPIO_PIN_RESET);

    Enable_GPIO_Clock(WS_BACKUP_PORT);
    GPIO_InitStruct.Pin   = WS_BACKUP_PIN;
    HAL_GPIO_Init(WS_BACKUP_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WS_BACKUP_PORT, WS_BACKUP_PIN, GPIO_PIN_RESET);
    HAL_Delay(5);

    // 4. 点亮电源灯，并执行开机 6 颗灯珠跑马灯自检 (持续约 0.4 秒)
    HAL_GPIO_WritePin(LED_POWER_PORT, LED_POWER_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);

    for (uint8_t step = 2; step <= 20; step += 4) {
        WS2812_UpdateStrip(step);
        HAL_Delay(60);
    }
    // 自检完毕，回到 0.0 kg 待机状态 (第 1 颗灯常亮微弱冰蓝光)
    WS2812_UpdateStrip(0);
}

void AnkleUI_Update(AnkleCtrl_t* ctrl) {
    uint32_t now = HAL_GetTick();
    bool force_changed = false;
    bool single_edge_trig[2] = {false, false};

    if (ctrl->max_raw == 0) ctrl->max_raw = 20;
    if (ctrl->raw_value > ctrl->max_raw) ctrl->raw_value = ctrl->max_raw;

    if (status_led_active && ((now - status_led_start_ms) >= STATUS_LED_ON_MS)) {
        HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);
        status_led_active = false;
    }

    for (int i = 0; i < INPUT_COUNT; i++) {
        GPIO_PinState raw_pin = HAL_GPIO_ReadPin(inputs[i].pin_cfg.port, inputs[i].pin_cfg.pin);

        if (raw_pin != inputs[i].current_reading) {
            inputs[i].last_debounce_ms = now;
            inputs[i].current_reading  = raw_pin;
        }

        if ((now - inputs[i].last_debounce_ms) >= DEBOUNCE_DELAY_MS) {
            if (!inputs[i].armed) {
                if (raw_pin == GPIO_PIN_SET) {
                    inputs[i].armed        = true;
                    inputs[i].stable_state = GPIO_PIN_SET;
                    inputs[i].is_active    = false;
                }
                continue;
            }

            if (raw_pin != inputs[i].stable_state) {
                inputs[i].stable_state = raw_pin;

                if (inputs[i].stable_state == GPIO_PIN_RESET) {
                    inputs[i].is_active      = true;
                    inputs[i].press_start_ms = now;
                    inputs[i].last_repeat_ms = now;
                    if (i == BTN_UP || i == BTN_DOWN) {
                        single_edge_trig[i] = true;
                    }
                } else {
                    inputs[i].is_active      = false;
                    inputs[i].press_start_ms = 0;
                    inputs[i].last_repeat_ms = 0;
                }
            }
        }
    }

    bool left_on  = inputs[SW_POS_LEFT].is_active;
    bool right_on = inputs[SW_POS_RIGHT].is_active;

    ToggleSwitchState_t new_sw_state = SW_STATE_CENTER_OFF;
    if (left_on && right_on) {
        ctrl->sw_fault = true;
        new_sw_state   = SW_STATE_CENTER_OFF;
    } else {
        ctrl->sw_fault = false;
        if (left_on) {
            new_sw_state = SW_STATE_LEFT_ON;
        } else if (right_on) {
            new_sw_state = SW_STATE_RIGHT_ON;
        } else {
            new_sw_state = SW_STATE_CENTER_OFF;
        }
    }

    if (first_sw_sync || (new_sw_state != last_sw_state)) {
        ctrl->sw_state   = new_sw_state;
        last_sw_state    = new_sw_state;
        first_sw_sync    = false;
        ctrl->ui_updated = true;
    }

    if (inputs[BTN_UP].is_active && inputs[BTN_DOWN].is_active) {
        inputs[BTN_UP].press_start_ms   = now;
        inputs[BTN_UP].last_repeat_ms   = now;
        inputs[BTN_DOWN].press_start_ms = now;
        inputs[BTN_DOWN].last_repeat_ms = now;
        ctrl->btn_conflict              = true;
        return;
    }
    ctrl->btn_conflict = false;

    for (int i = 0; i < 2; i++) {
        int8_t dir = (i == BTN_UP) ? +1 : -1;

        if (single_edge_trig[i]) {
            if (Step_ForceValue(ctrl, dir)) {
                force_changed = true;
            }
        } else if (inputs[i].is_active) {
            if ((now - inputs[i].press_start_ms) >= AUTO_REPEAT_DELAY_MS) {
                if ((now - inputs[i].last_repeat_ms) >= AUTO_REPEAT_RATE_MS) {
                    inputs[i].last_repeat_ms = now;
                    if (Step_ForceValue(ctrl, dir)) {
                        force_changed = true;
                    }
                }
            }
        }
    }

    if (force_changed) {
        Trigger_StatusLED(now);
        ctrl->display_percentage = (uint8_t)(((uint16_t)ctrl->raw_value * 100) / ctrl->max_raw);
        ctrl->active_leds        = WS2812_UpdateStrip(ctrl->raw_value);
        ctrl->ui_updated         = true;
    }
}
