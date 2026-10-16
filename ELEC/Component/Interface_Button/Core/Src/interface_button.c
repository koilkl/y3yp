#include "interface_button.h"

/* 内部辅助函数：根据传入的 GPIO_TypeDef 自动使能对应的 RCC 时钟 */
static void AnkleUI_EnablePortClock(GPIO_TypeDef *port) {
    if (port == GPIOA)      { __HAL_RCC_GPIOA_CLK_ENABLE(); }
    else if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); }
    else if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); }
    else if (port == GPIOD) { __HAL_RCC_GPIOD_CLK_ENABLE(); }
    else if (port == GPIOE) { __HAL_RCC_GPIOE_CLK_ENABLE(); }
    else if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); }
}

/* 内部辅助函数：数值上下限限幅 */
static int16_t Clamp_i16(int16_t val, int16_t min_val, int16_t max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

/* 内部辅助函数：处理单个按钮的触发动作 (含防呆与卸力保护逻辑) */
static void AnkleUI_ExecuteAction(AnkleController_t *ctrl, AnkleBtnIndex_t btn_idx, bool is_repeat) {
    /* 【防呆 1：运行施力期间禁止长按连发暴冲】
     * 当处于 STARTED (踏板已加载力矩) 状态时，禁止任何按键的长按连发，
     * 仅允许单次点按微调，防止踏板力矩或目标角度瞬间暴增伤及脚踝 */
    if (ctrl->is_running && is_repeat) {
        return;
    }

    switch (btn_idx) {
        case BTN_START_RESET:
            // 同一个按钮切换 Start (加载力矩) / Reset (卸除力矩)，不支持长按连发
            if (!is_repeat) {
                if (!ctrl->is_running) {
                    // 安全启动检查：如果按下 Start 瞬间还有其他调节键被按住，拒绝启动（防误触）
                    for (uint8_t i = 0; i < BTN_START_RESET; i++) {
                        if (ctrl->filters[i].stable_state) {
                            return;
                        }
                    }
                    ctrl->is_running = true; // 启动训练，开始按设定模式施力
                } else {
                    // 【最高优先级 Reset：立即卸除踏板上的所有力】
                    // 无论在 Mode 1 还是 Mode 2 下，按下 Reset 立即停止输出并将力量清零 (0 kg)
                    ctrl->is_running = false;
                    ctrl->strength   = 0;
                    ctrl->angle      = 0;
                }
            }
            break;

        case BTN_MODE_TOGGLE:
            // Mode 切换：不支持长按连发，且运行施力期间严禁切换模式
            if (!is_repeat && !ctrl->is_running) {
                if (ctrl->mode == TRAINING_MODE_1) {
                    // 切入 Mode 2 (用户主动发力踩动踏板)：
                    // 踏板角度随用户自由运动，无需按钮预设角度，因此将预设值清零
                    ctrl->mode  = TRAINING_MODE_2;
                    ctrl->angle = 0;
                } else {
                    // 切回 Mode 1 (用户抵抗踏板主动施加的力)
                    ctrl->mode  = TRAINING_MODE_1;
                }
            }
            break;

        case BTN_AXIS_TOGGLE:
            // Axis 切换：不支持长按连发，且运行施力期间严禁切换运动轴
            if (!is_repeat && !ctrl->is_running) {
                ctrl->axis = (ctrl->axis == AXIS_ROLL) ? AXIS_PITCH : AXIS_ROLL;
            }
            break;

        case BTN_ANGLE_INC:
            // 【防呆 2：正反键互斥】若 Angle - 同时被按住，直接忽略
            if (ctrl->filters[BTN_ANGLE_DEC].stable_state) {
                return;
            }
            // 仅在 Mode 1 (用户抵抗踏板施力) 下需要通过按钮设定目标角度；
            // 在 Mode 2 下由用户主动踩动踏板，按钮控制被旁路 (Bypassed)
            if (ctrl->mode == TRAINING_MODE_1) {
                ctrl->angle = Clamp_i16(ctrl->angle + ctrl->angle_step, ANKLE_ANGLE_MIN, ANKLE_ANGLE_MAX);
            }
            break;

        case BTN_ANGLE_DEC:
            // 【防呆 2：正反键互斥】若 Angle + 同时被按住，直接忽略
            if (ctrl->filters[BTN_ANGLE_INC].stable_state) {
                return;
            }
            // 仅在 Mode 1 下通过按钮设定目标角度；Mode 2 下忽略角度按钮
            if (ctrl->mode == TRAINING_MODE_1) {
                ctrl->angle = Clamp_i16(ctrl->angle - ctrl->angle_step, ANKLE_ANGLE_MIN, ANKLE_ANGLE_MAX);
            }
            break;

        case BTN_STRENGTH_INC:
            // 【防呆 2：正反键互斥】若 Strength - 同时被按住，直接忽略
            if (ctrl->filters[BTN_STRENGTH_DEC].stable_state) {
                return;
            }
            // Mode 1 (踏板主动施力大小) 与 Mode 2 (踏板阻力大小) 均支持调节 Strength
            ctrl->strength = Clamp_i16(ctrl->strength + ctrl->strength_step, ANKLE_STRENGTH_MIN, ANKLE_STRENGTH_MAX);
            break;

        case BTN_STRENGTH_DEC:
            // 【防呆 2：正反键互斥】若 Strength + 同时被按住，直接忽略
            if (ctrl->filters[BTN_STRENGTH_INC].stable_state) {
                return;
            }
            ctrl->strength = Clamp_i16(ctrl->strength - ctrl->strength_step, ANKLE_STRENGTH_MIN, ANKLE_STRENGTH_MAX);
            break;

        default:
            break;
    }
}

void AnkleUI_Init(AnkleController_t *ctrl,
                  const AnkleBtnPin_t pin_array[BTN_COUNT],
                  int16_t angle_step,
                  int16_t strength_step) {
    if (ctrl == NULL || pin_array == NULL) return;

    uint32_t now = HAL_GetTick();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        ctrl->pins[i] = pin_array[i];

        // 1. 自动开启对应 GPIO 端口时钟
        AnkleUI_EnablePortClock(ctrl->pins[i].port);

        // 2. 配置为内部上拉输入模式 (按钮另一端接 GND，按下为低电平)
        GPIO_InitStruct.Pin  = ctrl->pins[i].pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(ctrl->pins[i].port, &GPIO_InitStruct);

        // 3. 【防呆 3：上电防短路/卡死自检 (armed 锁)】
        // 若上电瞬间引脚已处于低电平，挂上 armed = false 保险锁，直到松开满 20ms 后才解锁
        bool initial_pressed = (HAL_GPIO_ReadPin(ctrl->pins[i].port, ctrl->pins[i].pin) == GPIO_PIN_RESET);
        ctrl->filters[i].raw_state        = initial_pressed;
        ctrl->filters[i].stable_state     = initial_pressed;
        ctrl->filters[i].armed            = !initial_pressed;
        ctrl->filters[i].last_change_tick = now;
        ctrl->filters[i].last_repeat_tick = now;
    }

    // 4. 初始化业务状态与步长 (初始处于完全卸力待机状态)
    ctrl->axis          = AXIS_ROLL;
    ctrl->mode          = TRAINING_MODE_1;
    ctrl->angle         = 0;
    ctrl->strength      = 0;
    ctrl->is_running    = false;
    ctrl->angle_step    = (angle_step > 0) ? angle_step : 1;
    ctrl->strength_step = (strength_step > 0) ? strength_step : 1;
    ctrl->state_changed = true;
}

void AnkleUI_SetSteps(AnkleController_t *ctrl, int16_t angle_step, int16_t strength_step) {
    if (ctrl == NULL) return;
    if (angle_step > 0)    ctrl->angle_step    = angle_step;
    if (strength_step > 0) ctrl->strength_step = strength_step;
}

bool AnkleUI_Update(AnkleController_t *ctrl) {
    if (ctrl == NULL) return false;

    uint32_t now = HAL_GetTick();

    // 记录本轮扫描前的状态快照，只有数据真正改变才返回 true
    AnkleAxis_t prev_axis     = ctrl->axis;
    AnkleMode_t prev_mode     = ctrl->mode;
    int16_t     prev_angle    = ctrl->angle;
    int16_t     prev_strength = ctrl->strength;
    bool        prev_running  = ctrl->is_running;

    // 第一阶段：全盘采样并更新所有 7 个按键的物理消抖状态
    bool single_trigger[BTN_COUNT] = {false};
    bool repeat_trigger[BTN_COUNT] = {false};

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        bool current_pressed = (HAL_GPIO_ReadPin(ctrl->pins[i].port, ctrl->pins[i].pin) == GPIO_PIN_RESET);
        AnkleBtnFilter_t *flt = &ctrl->filters[i];

        if (current_pressed != flt->raw_state) {
            flt->raw_state = current_pressed;
            flt->last_change_tick = now;
        }

        if ((now - flt->last_change_tick) >= BTN_DEBOUNCE_MS) {
            if (flt->stable_state != flt->raw_state) {
                flt->stable_state = flt->raw_state;

                if (flt->stable_state) {
                    // 刚稳定按下：只有已解锁 (armed == true) 的正常按钮才允许触发
                    if (flt->armed) {
                        flt->last_repeat_tick = now;
                        single_trigger[i] = true;
                    }
                } else {
                    // 刚稳定松开满 20ms：自动解除上电锁定
                    flt->armed = true;
                }
            } else if (flt->stable_state && flt->armed) {
                // 持续稳定按下且已解锁：检查是否达到 500ms 长按连发条件
                if ((now - flt->last_change_tick) >= BTN_LONG_PRESS_START_MS) {
                    if ((now - flt->last_repeat_tick) >= BTN_REPEAT_INTERVAL_MS) {
                        flt->last_repeat_tick = now;
                        repeat_trigger[i] = true;
                    }
                }
            }
        }
    }

    // 第二阶段：【最高优先级：检查 Start / Reset 卸力键】
    if (single_trigger[BTN_START_RESET]) {
        AnkleUI_ExecuteAction(ctrl, BTN_START_RESET, false);
        // 如果刚才这一下触发了 Reset 卸力 (is_running 变成了 false)，直接丢弃本轮其他所有按钮的触发
        if (!ctrl->is_running) {
            ctrl->state_changed = (ctrl->axis != prev_axis) || (ctrl->mode != prev_mode) ||
                                  (ctrl->angle != prev_angle) || (ctrl->strength != prev_strength) ||
                                  (ctrl->is_running != prev_running);
            return ctrl->state_changed;
        }
    }

    // 第三阶段：按顺序处理其余 6 个调节按钮 (0 ~ 5)
    for (uint8_t i = 0; i < BTN_START_RESET; i++) {
        if (single_trigger[i]) {
            AnkleUI_ExecuteAction(ctrl, (AnkleBtnIndex_t)i, false);
        } else if (repeat_trigger[i]) {
            AnkleUI_ExecuteAction(ctrl, (AnkleBtnIndex_t)i, true);
        }
    }

    // 第四阶段：严格比对业务数据是否真正发生了变化
    ctrl->state_changed = (ctrl->axis       != prev_axis)     ||
                          (ctrl->mode       != prev_mode)     ||
                          (ctrl->angle      != prev_angle)    ||
                          (ctrl->strength   != prev_strength) ||
                          (ctrl->is_running != prev_running);

    return ctrl->state_changed;
}
