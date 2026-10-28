# Ankle Trainer 7-Button UI Controller Library (`interface_button`)

A standalone, non-blocking 7-button finite state machine (FSM) and hardware abstraction layer (HAL) driver library designed for the **Ankle Trainer** rehabilitation system on **STM32 microcontrollers** (configured by default for **STM32F401RE**).

This library features **non-blocking 20 ms hardware debouncing**, **auto-repeat on long press during standby**, and **6 medical-grade safety & fool-proofing (Poka-Yoke) interlocks**. It automatically enables RCC port clocks and configures GPIO pull-up inputs at runtime—requiring **zero manual GPIO configuration in STM32CubeMX**.

---

## 1. File Structure & Integration

Copy the two source files directly into your STM32CubeIDE / Keil MDK / CLion project directories:

| File Name | Target Directory | Description |
|---|---|---|
| `interface_button.h` | `Core/Inc/interface_button.h` | Data structures, enums, macro definitions, and public API declarations |
| `interface_button.c` | `Core/Src/interface_button.c` | Runtime GPIO initialization, non-blocking debouncer, and safety FSM implementation |

> **Cross-Series Compatibility**: By default, `interface_button.h` includes `"stm32f4xx_hal.h"`. To use this library on other STM32 families (e.g., STM32F1, G4, H7), simply replace `#include "stm32f4xx_hal.h"` with `#include "main.h"`.

---

## 2. Hardware Wiring & Recommended Pinout (Nucleo-F401RE)

* **Trigger Logic**: Active-Low (Triggered when pressed).
* **Wiring Method**: `AnkleUI_Init()` automatically configures all 7 pins as **internal pull-up inputs (`GPIO_PULLUP`)**. Simply connect **one terminal of each push button to the designated STM32 GPIO pin, and the other terminal directly to `GND`**. External pull-up resistors are not required, and any unconnected pins will safely remain pulled high (`3.3V`) without floating noise.

Below is the recommended conflict-free pin assignment on the **STM32 Nucleo-F401RE** morpho headers (**CN7** & **CN10**):

| Array Index Enum (`AnkleBtnIndex_t`) | Button Function | Recommended Pin | Nucleo Morpho Header |
|---|---|---|---|
| `BTN_MODE_TOGGLE` (`0`) | Toggle Training Mode (`Mode 1` ↔ `Mode 2`) | `PC10` | **CN7 Pin 1** |
| `BTN_AXIS_TOGGLE` (`1`) | Toggle Movement Axis (`Roll` ↔ `Pitch`) | `PC11` | **CN7 Pin 2** |
| `BTN_ANGLE_INC` (`2`) | Increase Target Angle (`Angle +`, `-30°` to `+30°`) | `PC12` | **CN7 Pin 3** |
| `BTN_ANGLE_DEC` (`3`) | Decrease Target Angle (`Angle -`, `-30°` to `+30°`) | `PD2` | **CN7 Pin 4** |
| `BTN_STRENGTH_INC` (`4`) | Increase Target Strength (`Strength +`, `0` to `50 kg`) | `PC8` | **CN10 Pin 2** |
| `BTN_STRENGTH_DEC` (`5`) | Decrease Target Strength (`Strength -`, `0` to `50 kg`) | `PC6` | **CN10 Pin 4** |
| `BTN_START_RESET` (`6`) | Start / Emergency Stop & Reset (`Start` ↔ `Stop/Reset`) | `PC5` | **CN10 Pin 6** |

---

## 3. Quick Start Guide (`main.c` Example)

No GPIO setup is needed in `.ioc` (STM32CubeMX). Initialize the pin array and poll `AnkleUI_Update()` inside your main loop:

```c
#include "main.h"
#include "interface_button.h"

/* 1. Declare the global controller instance */
AnkleController_t ankle_ctrl;

/* 2. Define the 7-button pin mapping array (customizable to any GPIO port/pin) */
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

    /* 3. Initialize buttons and controller (angle_step = 1 deg, strength_step = 1 kg) */
    AnkleUI_Init(&ankle_ctrl, my_button_pins, 1, 1);

    while (1) {
        /* 4. Non-blocking scan: returns true ONLY when state parameters actually change */
        if (AnkleUI_Update(&ankle_ctrl)) {
            // Read updated states for UI rendering or motor control loop:
            // ankle_ctrl.is_running  (bool: true = STARTED, false = STOPPED/RESET)
            // ankle_ctrl.mode        (TRAINING_MODE_1 or TRAINING_MODE_2)
            // ankle_ctrl.axis        (AXIS_ROLL or AXIS_PITCH)
            // ankle_ctrl.angle       (int16_t: -30 to +30 deg)
            // ankle_ctrl.strength    (int16_t: 0 to 50 kg)
        }
    }
}
```

---

## 4. State Machine Logic & 6 Safety Interlocks (Poka-Yoke)

To ensure patient safety during motorized ankle rehabilitation, the library enforces six hardware and software interlocks:

| Safety Mechanism | Functional Description |
|---|---|
| **1. Emergency Stop Preemption (`BTN_START_RESET`)** | `BTN_START_RESET` is evaluated with the highest priority. Pressing it while running immediately sets `is_running = false`, resets `angle` and `strength` to `0`, and discards any other button events within the same scan cycle. |
| **2. Standby Auto-Repeat vs. Runtime Surge Protection** | **Standby (`STOPPED`)**: Holding an increment/decrement button for `500 ms` triggers rapid auto-repeat every `100 ms`.<br>**Runtime (`STARTED`)**: Auto-repeat is strictly disabled (`is_repeat` ignored); only single-step fine adjustments are permitted to prevent sudden torque or angle surges. |
| **3. Runtime Mode & Axis Lockout** | While `is_running == true`, `BTN_MODE_TOGGLE` and `BTN_AXIS_TOGGLE` are locked out. Furthermore, transitioning from `STOPPED` to `STARTED` is blocked if any adjustment button (`0`–`5`) is simultaneously held down. |
| **4. Mode 2 Angle Lock & Auto-Zeroing** | In `Mode 2` (Strength-only mode), `Angle +` and `Angle -` inputs are ignored. Switching from `Mode 1` to `Mode 2` automatically zeroes `angle` (`0°`) to prevent unintended joint deflection. |
| **5. Opposing Button Mutual Exclusion** | If opposing buttons (`Angle +` & `Angle -`, or `Strength +` & `Strength -`) are pressed simultaneously, both commands are canceled out and frozen to prevent value jitter. |
| **6. Power-On Stuck-Key Guard (`armed` Lock)** | If a pin reads low (`GND`) at power-up (e.g., a jammed button or a shorted wire), its internal `armed` flag is set to `false`. It will never trigger a single press or auto-repeat runaway until the pin is released (`HIGH`) for at least `20 ms`. |

---

## 5. Public APIs & Configurable Macros

### Public Functions
* `void AnkleUI_Init(AnkleController_t *ctrl, const AnkleBtnPin_t pin_array[BTN_COUNT], int16_t angle_step, int16_t strength_step)`
  Enables GPIO clocks, configures all 7 pins as `GPIO_PULLUP` inputs, performs power-on stuck-key checks, and initializes default controller states.
* `void AnkleUI_SetSteps(AnkleController_t *ctrl, int16_t angle_step, int16_t strength_step)`
  Dynamically updates the step size for angle and strength adjustments at runtime.
* `bool AnkleUI_Update(AnkleController_t *ctrl)`
  Non-blocking two-phase scan function to be called inside `while(1)`. Uses `HAL_GetTick()` and returns `true` only when a state variable has genuinely changed.

### Configurable Macros (`interface_button.h`)
* `ANKLE_ANGLE_MIN` / `ANKLE_ANGLE_MAX`: Target angle bounds (default: `-30` / `+30`)
* `ANKLE_STRENGTH_MIN` / `ANKLE_STRENGTH_MAX`: Target strength bounds (default: `0` / `50`)
* `BTN_DEBOUNCE_MS`: Debounce filter window (default: `20U` ms)
* `BTN_LONG_PRESS_START_MS`: Hold duration required to start auto-repeat (default: `500U` ms)
* `BTN_REPEAT_INTERVAL_MS`: Interval between auto-repeat increments (default: `100U` ms)
# Interface_Button
