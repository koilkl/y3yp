# 踝关节训练器 加力控制界面库 (`interface_button`)

适用于 **STM32 (HAL 库，默认适配 STM32F401RE / Nucleo-F401RE)** 的踝关节训练器（Ankle Trainer）力控界面驱动库，独立、全非阻塞设计。

本库完整驱动加力控制面板：

* **2 颗加减力按钮** —— 目标力 `0.0` – `10.0 kg`，步进 `0.5 kg`
* **1 个三态拨动开关（ON-OFF-ON）** —— 训练模式选择（`Mode 1` / `OFF` / `Mode 2`）
* **1 颗变力状态灯** —— 每次力改变时闪烁 `150 ms`
* **6 珠 WS2812B 幻彩灯条** —— 绿→黄→橙→红 渐变力值进度条

内置 **20 ms 硬件消抖**、**长按连发**（按住 `500 ms` 后每 `150 ms` 连发一次）、**3 重防呆（Poka-Yoke）安全互锁**、**开机灯条跑马灯自检**，以及 **纯 GPIO 位脉冲（bit-bang）WS2812B 时序**（无需定时器/PWM/DMA）。所有 GPIO 均在运行时自动配置 —— **STM32CubeMX 中零 GPIO 配置，开箱即用**。

---

## 1. 文件清单与目录结构

将以下两个文件直接复制到你的 STM32CubeIDE / Keil MDK / CLion 工程目录中即可：

| 文件名 | 放置路径 | 说明 |
|---|---|---|
| `interface_button.h` | `Core/Inc/interface_button.h` | 引脚宏、数据结构、枚举与对外 API 声明 |
| `interface_button.c` | `Core/Src/interface_button.c` | GPIO 运行时初始化、非阻塞消抖、WS2812B 驱动与安全逻辑 |

> **跨型号兼容提示**：本库默认包含 `#include "stm32f4xx_hal.h"`。若在 STM32F1/G4/H7 等其他系列上使用，只需将 `interface_button.h` 中的该行改为 `#include "main.h"` 即可。

---

## 2. 硬件接线规范与推荐引脚 (Nucleo-F401RE)

* **触发方式**：低电平有效（接 `GND` 触发）。
* **接线方法**：`AnkleUI_Init()` 内部已自动开启 4 路输入的 **内部上拉电阻（`GPIO_PULLUP`）**。每个按钮/开关引脚的一端接对应的 STM32 GPIO，另一端直接接 `GND`，无需外接上拉电阻。

### 2.1 输入通道（2 颗按钮 + 拨动开关左右 2 路）

| 通道枚举 (`AnkleInputChannel_t`) | 功能 | 引脚 |
|---|---|---|
| `BTN_UP` (`0`) | 加力（`+0.5 kg`） | `PC12` |
| `BTN_DOWN` (`1`) | 减力（`-0.5 kg`） | `PD2` |
| `SW_POS_LEFT` (`2`) | 拨动开关**左档**（`Mode 1`） | `PC10` |
| `SW_POS_RIGHT` (`3`) | 拨动开关**右档**（`Mode 2`） | `PC11` |

> 三态拨动开关（ON-OFF-ON）的 **公共端接 `GND`**。左档 → `SW_STATE_LEFT_ON`（Mode 1），中间档 → `SW_STATE_CENTER_OFF`（待机），右档 → `SW_STATE_RIGHT_ON`（Mode 2）。本库只负责跟踪并上报该状态，各模式的具体行为由应用程序决定。

### 2.2 输出信号（电源灯、状态灯、灯条）

| 信号 | 引脚 | 接线位置 | 行为 |
|---|---|---|---|
| 常亮电源灯 | `PA5` | 板载 LD2 / `D13` | 初始化后常亮 |
| 变力状态灯 | `PA7` | `D11` / CN10 Pin 15 | 每次力改变时闪烁 `150 ms` |
| 灯条数据脚（主） | `PA6` | `D12` / CN10 Pin 13 | 6 珠灯条 `DIN` |
| 灯条数据脚（备用） | `PB6` | `D10` | 与 PA6 完全同步的同源信号 |

> `PA6` 与 `PB6` 输出**完全相同**的位流 —— 灯条 `DIN` 接任意一个均可。

---

## 3. 快速上手（`main.c` 示例）

`.ioc`（STM32CubeMX）中无需任何 GPIO 配置。定义引脚数组并在主循环中轮询 `AnkleUI_Update()`：

```c
#include "main.h"
#include "interface_button.h"

/* 1. 定义 4 路输入引脚映射（2 颗按钮 + 拨动开关左右 2 路） */
const AnkleBtnPin_t my_pins[INPUT_COUNT] = {
    [BTN_UP]       = PC12,
    [BTN_DOWN]     = PD2,
    [SW_POS_LEFT]  = PC10,
    [SW_POS_RIGHT] = PC11
};

/* 2. 声明控制器状态（定点数 0..20 对应 0.0..10.0 kg） */
AnkleCtrl_t ankle_ctrl = { .max_raw = 20 };

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 3. 初始化：开时钟、配置上拉与 LED 输出、
          执行卡键检测与开机灯条跑马灯自检 */
    AnkleUI_Init(my_pins);

    while (1) {
        /* 4. 非阻塞扫描（内部使用 HAL_GetTick()） */
        AnkleUI_Update(&ankle_ctrl);

        /* 5. 处理库标记的状态变化 */
        if (ankle_ctrl.ui_updated) {
            // ankle_ctrl.raw_value          (uint8_t: 0..20 → 0.0..10.0 kg，步进 0.5)
            // ankle_ctrl.display_percentage (uint8_t: 0..100 %)
            // ankle_ctrl.active_leds        (uint8_t: 0..6)
            // ankle_ctrl.sw_state           (SW_STATE_CENTER_OFF / LEFT_ON / RIGHT_ON)
            // ankle_ctrl.btn_conflict       (bool: 加减力按钮同时按下)
            // ankle_ctrl.sw_fault           (bool: 拨动开关左右两路同时导通)
            ankle_ctrl.ui_updated = false;   // 处理完毕后清标志
        }
        HAL_Delay(1);
    }
}
```

示例 `main.c` 还会在每次状态变化时向 **UART2（115200 8N1）** 打印一行状态，并在防呆互锁触发时打印一次性 `[POKA-YOKE]` 报警。

---

## 4. 行为细节

### 4.1 按钮与长按连发

* 按钮为 **边沿触发**：每次干净按下，力值步进 `±0.5 kg`。
* **长按连发**：按住按钮 `500 ms` 后，每 `150 ms` 自动连发一次。
* 力值被钳位在 `min_raw`（`0.0 kg`）与 `max_raw`（`10.0 kg`）之间，到顶/到底后饱和，不会回绕。

### 4.2 三态拨动开关

开关为 **电平触发**（自锁触点）。档位同样经过 20 ms 消抖后上报为 `sw_state`：

| 档位 | `sw_state` | 含义 |
|---|---|---|
| 左档 | `SW_STATE_LEFT_ON` | Mode 1 |
| 中间档 | `SW_STATE_CENTER_OFF` | 待机 / OFF |
| 右档 | `SW_STATE_RIGHT_ON` | Mode 2 |

与旧版 7 按键设计不同，本库 **不再对按钮施加任何模式锁定** —— 任何档位下均可调整力值，模式语义完全由应用程序定义。

### 4.3 WS2812B 灯条

6 珠灯条作为力值比例进度条，颜色采用 **GRB** 顺序：

| 灯珠 | 颜色 | 力值区间 |
|---|---|---|
| 第 1 颗 | 纯绿色 | 0.5 – 1.5 kg |
| 第 2 颗 | 草绿色 | 2.0 – 3.0 kg |
| 第 3 颗 | 亮黄色 | 3.5 – 5.0 kg |
| 第 4 颗 | 亮橙色 | 5.5 – 6.5 kg |
| 第 5 颗 | 深橙红 | 7.0 – 8.5 kg |
| 第 6 颗 | 纯红色 | 9.0 – 10.0 kg |

* **0.0 kg 待机**：第 1 颗灯珠常亮微弱冰蓝色（`B = 40`），一眼即可确认灯条工作正常。
* **开机自检**：`AnkleUI_Init()` 时 6 颗灯珠跑马灯扫描一遍（约 0.4 秒），随后回到待机状态。
* **时序**（基于 DWT 周期计数器位脉冲，`__attribute__((optimize("O2")))`）：
  * `T0H ≈ 350 ns`、`T1H ≈ 800 ns`、单 bit 周期 ≈ `1250 ns`
  * 复位锁存保持 `500 µs` 纯低电平 —— 确保新版 WS2812B 100% 锁存亮起
  * 每帧约关中断 `680 µs`（6 珠 × 24 bit × 1.25 µs + 500 µs 复位），不占用定时器/PWM/DMA，灯条可接任意 GPIO

### 4.4 防呆安全互锁（Poka-Yoke）

| 防护 | 说明 |
|---|---|
| **1. 加减力互斥锁定** | `BTN_UP` 与 `BTN_DOWN` 同时按下时，力值被冻结（`btn_conflict = true`），直到松开其中一个才恢复 —— 防止数值抖动。 |
| **2. 开关短路异常** | 拨动开关左右两路同时导通（接线故障）时，自动回落为 `SW_STATE_CENTER_OFF` 并置位 `sw_fault = true` —— 失效即安全。 |
| **3. 开机卡键保护** | 上电时按钮若被按住（读数为 `LOW`，如按钮卡死或线路短路），其内部 `armed` 标志被清除，直到松开（`HIGH`）≥ `20 ms` 后才会解除，期间不会产生单击或连发。 |

---

## 5. 对外 API 与可配置宏

### 对外函数

* `void AnkleUI_Init(const AnkleBtnPin_t* pin_config)`
  开启 GPIO 时钟，将 4 路输入配置为 `GPIO_PULLUP`，初始化电源灯/状态灯/灯条数据脚，执行卡键检测与开机跑马灯自检。
* `void AnkleUI_Update(AnkleCtrl_t* ctrl)`
  非阻塞扫描函数，在 `while(1)` 中调用（内部使用 `HAL_GetTick()`）。当力值、开关档位或故障标志发生变化时置位 `ctrl->ui_updated`。

### 控制器状态（`AnkleCtrl_t`，见 `interface_button.h`）

| 字段 | 类型 | 说明 |
|---|---|---|
| `raw_value` | `uint8_t` | 内部定点力值：`0..20` → `0.0..10.0 kg`（步进 `0.5`） |
| `min_raw` / `max_raw` | `uint8_t` | 力值上下限（默认 `0` / `20`） |
| `display_percentage` | `uint8_t` | 力值进度 `0..100 %` |
| `active_leds` | `uint8_t` | 当前点亮灯珠数 `0..6` |
| `sw_state` | `ToggleSwitchState_t` | `SW_STATE_CENTER_OFF` / `SW_STATE_LEFT_ON` / `SW_STATE_RIGHT_ON` |
| `btn_conflict` | `bool` | 防呆 1：加减力按钮同时按下 |
| `sw_fault` | `bool` | 防呆 2：开关左右两路同时导通 |
| `ui_updated` | `bool` | 状态改变标志位（处理完毕后清掉） |

### 引脚简写宏（`interface_button.h`）

`PC10`、`PC11`、`PC12`、`PD2` —— 展开为 `(AnkleBtnPin_t){GPIOx, GPIO_PIN_x}`，让引脚数组写法更直观。

### 可配置宏

| 宏 | 位置 | 默认值 | 说明 |
|---|---|---|---|
| `DEBOUNCE_DELAY_MS` | `interface_button.c` | `20` | 消抖滤波窗口 |
| `AUTO_REPEAT_DELAY_MS` | `interface_button.c` | `500` | 长按连发启动阈值 |
| `AUTO_REPEAT_RATE_MS` | `interface_button.c` | `150` | 连发间隔 |
| `STATUS_LED_ON_MS` | `interface_button.h` | `150` | 状态灯闪烁时长 |
| `NUM_LEDS` | `interface_button.h` | `6` | 灯条灯珠数量 |
