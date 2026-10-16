# Ankle Trainer 7-Button UI Controller Library (`interface_button`)

适用于 **STM32 (HAL 库，默认适配 STM32F401RE)** 的踝关节训练器（Ankle Trainer）7 按键状态机与硬件抽象层驱动库。

本库采用 **全非阻塞（Non-blocking）设计**，内置 **20ms 硬件级消抖**、**待机长按连发** 以及 **6 重医疗康复器械级防呆与安全互锁机制**，无需在 STM32CubeMX 中手动配置 GPIO 引脚，开箱即用。

---

## 1. 文件清单与目录结构

将以下两个文件直接复制到你的 STM32CubeIDE / Keil 工程目录中即可：

| 文件名 | 放置路径 | 说明 |
|---|---|---|
| `interface_button.h` | `Core/Inc/interface_button.h` | 数据结构、枚举、宏配置与对外 API 声明 |
| `interface_button.c` | `Core/Src/interface_button.c` | GPIO 自动初始化、非阻塞消抖与安全状态机实现 |

> **跨型号兼容提示**：本库默认包含 `#include "stm32f4xx_hal.h"`。若在 STM32F1/G4/H7 等其他系列上使用，只需将 `interface_button.h` 第 9 行改为 `#include "main.h"` 即可。

---

## 2. 硬件接线规范与推荐引脚 (Nucleo-F401RE)

* **触发方式**：按下触发（低电平有效）。
* **接线方法**：函数内部已自动开启 STM32 内部上拉电阻（`GPIO_PULLUP`）。**每个按钮的一端接对应的 STM32 GPIO 引脚，另一端直接接 `GND` 即可**（无需外接上拉电阻；未接按钮的悬空引脚会自动保持高电平，不会产生误触发）。

以下为在 **Nucleo-F401RE** 上推荐的无冲突引脚组合（位于 CN7 与 CN10 外侧排针）：

| 数组下标枚举 (`AnkleBtnIndex_t`) | 按钮功能说明 | 推荐引脚 | Nucleo 排针位置 |
|---|---|---|---|
| `BTN_MODE_TOGGLE` (`0`) | 切换训练模式 (`Mode 1` ↔ `Mode 2`) | `PC10` | **CN7 Pin 1** |
| `BTN_AXIS_TOGGLE` (`1`) | 切换运动轴 (`Roll` ↔ `Pitch`) | `PC11` | **CN7 Pin 2** |
| `BTN_ANGLE_INC` (`2`) | 角度增加 (`Angle +`，范围 `-30° ~ +30°`) | `PC12` | **CN7 Pin 3** |
| `BTN_ANGLE_DEC` (`3`) | 角度减小 (`Angle -`，范围 `-30° ~ +30°`) | `PD2` | **CN7 Pin 4** |
| `BTN_STRENGTH_INC` (`4`) | 力量增加 (`Strength +`，范围 `0 ~ 50 kg`) | `PC8` | **CN10 Pin 2** |
| `BTN_STRENGTH_DEC` (`5`) | 力量减小 (`Strength -`，范围 `0 ~ 50 kg`) | `PC6` | **CN10 Pin 4** |
| `BTN_START_RESET` (`6`) | 启动 / 急停复位 (`Start` ↔ `Stop/Reset`) | `PC5` | **CN10 Pin 6** |

---

## 3. 快速上手指南 (`main.c` 集成示例)

无需在 STM32CubeMX 中配置上述 7 个 GPIO 引脚，`AnkleUI_Init()` 会自动识别端口并开启对应的 RCC 时钟。

```c
#include "main.h"
#include "interface_button.h"

/* 1. 定义全局控制器实例 */
AnkleController_t ankle_ctrl;

/* 2. 定义 7 个按钮的引脚映射数组 (可根据实际 PCB 走线自由修改端口和引脚) */
const AnkleBtnPin_t my_button_pins[BTN_COUNT] = {
    [BTN_MODE_TOGGLE]  = {GPIOC, GPIO_PIN_10},
    [BTN_AXIS_TOGGLE]  = {GPIOC, GPIO_PIN_11},
    [BTN_ANGLE_INC]    = {GPIOC, GPIO_PIN_12},
    [BTN_ANGLE_DEC]    = {GPIOD, GPIO_PIN_2},
    [BTN_STRENGTH_INC] = {GPIOC, GPIO_PIN_8},
    [BTN_STRENGTH_DEC] = {GPIOC, GPIO_PIN_6},
    [BTN_START_RESET]  = {GPIOC, GPIO_PIN_5}
};

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 3. 初始化按键GPIO与控制器 (参数：句柄指针, 引脚数组, 角度步长=1°, 力量步长=1kg) */
    AnkleUI_Init(&ankle_ctrl, my_button_pins, 1, 1);

    while (1) {
        /* 4. 非阻塞扫描：仅在状态真正发生改变时返回 true */
        if (AnkleUI_Update(&ankle_ctrl)) {
            // 在此处读取最新状态用于刷新 UI 或更新电机目标：
            // ankle_ctrl.is_running  (bool: true=运行中, false=已停止/复位)
            // ankle_ctrl.mode        (TRAINING_MODE_1 或 TRAINING_MODE_2)
            // ankle_ctrl.axis        (AXIS_ROLL 或 AXIS_PITCH)
            // ankle_ctrl.angle       (int16_t: -30 ~ +30)
            // ankle_ctrl.strength    (int16_t: 0 ~ 50)
        }
    }
}
```

---

## 4. 状态机业务逻辑与 6 重安全防呆机制

为了保障人体踝关节训练安全，本库在底层强制执行以下防呆互锁规则：

| 机制名称 | 具体行为描述 |
|---|---|
| **1. 急停最高抢占权 (`BTN_START_RESET`)** | 每次扫描优先检测 `Start/Reset` 键。在运行状态下按下该键，立即切断 `is_running` 并将 `angle` 和 `strength` 归零，同时丢弃同一周期内其他按键的任何触发。 |
| **2. 待机长按连发 vs 运行中防暴冲** | **待机时 (`STOPPED`)**：按住加减键满 `500ms` 后自动以 `100ms` 间隔快速连续加减；<br>**运行中 (`STARTED`)**：自动禁用长按连发，仅允许单次点按微调，严防电机力矩或角度瞬间暴增。 |
| **3. 运行中模式/运动轴锁死** | 当 `is_running == true` 时，`Mode` 和 `Axis` 切换按钮被彻底锁定，防止电机运动中途发生反向突变；同时启动瞬间若检测到有其他调节键被按住，拒绝启动。 |
| **4. Mode 2 角度锁定与自动归零** | 在 `Mode 2` 下只能修改 `Strength`，`Angle +` 与 `Angle -` 自动失效；且从 `Mode 1` 切入 `Mode 2` 瞬间，`angle` 自动安全归零 (`0°`)。 |
| **5. 正反方向键互斥锁** | 若用户同时按下 `Angle +` 与 `Angle -`（或同时按下 `Strength +` 与 `Strength -`），两边指令同时冻结忽略，数值保持稳定不抖动。 |
| **6. 上电防短路/卡死保险 (`armed` 锁)** | 若上电瞬间某个引脚已处于低电平（如按钮被异物压住或接线短路到 GND），该按钮会被挂上 `armed = false` 保险锁，绝不触发单按或长按狂飙，直到引脚恢复高电平满 `20ms` 后才自动解锁。 |

---

## 5. API 接口与可调宏参数说明

### 对外函数接口
* `void AnkleUI_Init(AnkleController_t *ctrl, const AnkleBtnPin_t pin_array[BTN_COUNT], int16_t angle_step, int16_t strength_step)`
  初始化 7 个 GPIO 引脚（自动开启时钟并配置为 `GPIO_PULLUP`）及默认状态。
* `void AnkleUI_SetSteps(AnkleController_t *ctrl, int16_t angle_step, int16_t strength_step)`
  在程序运行过程中动态修改角度或力量的单次加减步长。
* `bool AnkleUI_Update(AnkleController_t *ctrl)`
  非阻塞状态更新函数，建议在主循环 `while(1)` 中直接调用。仅当业务参数实际发生变化时返回 `true`。

### 可调时间与阈值宏（位于 `interface_button.h`）
* `ANKLE_ANGLE_MIN` / `ANKLE_ANGLE_MAX`：角度上下限（默认 `-30` / `+30`）
* `ANKLE_STRENGTH_MIN` / `ANKLE_STRENGTH_MAX`：力量上下限（默认 `0` / `50`）
* `BTN_DEBOUNCE_MS`：按键消抖时间（默认 `20U` ms）
* `BTN_LONG_PRESS_START_MS`：触发长按连发的按住时间阈值（默认 `500U` ms）
* `BTN_REPEAT_INTERVAL_MS`：长按连发的步进时间间隔（默认 `100U` ms）
