# Ankle Trainer Force-Control UI Library (`interface_button`)

A standalone, non-blocking UI driver library for the **Ankle Trainer** rehabilitation system on **STM32 microcontrollers** (configured by default for **STM32F401RE / Nucleo-F401RE**).

The library drives the complete force-selection panel:

* **2 push buttons** — Increase / Decrease target force (`0.0` – `10.0 kg`, step `0.5 kg`)
* **1 three-position toggle switch** (ON-OFF-ON) — Training mode selection (`Mode 1` / `OFF` / `Mode 2`)
* **1 status LED** — flashes `150 ms` on every force change
* **6-LED WS2812B strip** — rainbow gradient force gauge (green → yellow → orange → red)

It features **non-blocking 20 ms hardware debouncing**, **auto-repeat on long press** (hold `500 ms`, then repeat every `150 ms`), **3 Poka-Yoke (fool-proofing) safety interlocks**, a **power-on LED-strip self-test**, and **bit-banged WS2812B timing** (no timer/PWM/DMA required). All GPIO is configured at runtime — **zero manual GPIO setup in STM32CubeMX**.

---

## 1. File Structure & Integration

Copy the two source files directly into your STM32CubeIDE / Keil MDK / CLion project directories:

| File Name | Target Directory | Description |
|---|---|---|
| `interface_button.h` | `Core/Inc/interface_button.h` | Pin macros, data structures, enums, and public API declarations |
| `interface_button.c` | `Core/Src/interface_button.c` | Runtime GPIO init, non-blocking debouncer, WS2812B driver, and safety logic |

> **Cross-Series Compatibility**: By default, `interface_button.h` includes `"stm32f4xx_hal.h"`. To use this library on other STM32 families (e.g., STM32F1, G4, H7), simply replace `#include "stm32f4xx_hal.h"` with `#include "main.h"`.

---

## 2. Hardware Wiring & Recommended Pinout (Nucleo-F401RE)

* **Trigger Logic**: Active-Low (triggered when pulled to `GND`).
* **Wiring Method**: `AnkleUI_Init()` automatically configures all 4 input pins as **internal pull-up inputs (`GPIO_PULLUP`)**. Connect one terminal of each button / switch pole to the designated STM32 GPIO pin, and the other terminal directly to `GND`. External pull-up resistors are not required.

### 2.1 Inputs (2 buttons + both poles of the ON-OFF-ON switch)

| Channel Enum (`AnkleInputChannel_t`) | Function | Pin |
|---|---|---|
| `BTN_UP` (`0`) | Increase force (`+0.5 kg`) | `PC12` |
| `BTN_DOWN` (`1`) | Decrease force (`-0.5 kg`) | `PD2` |
| `SW_POS_LEFT` (`2`) | Toggle switch **left** pole (`Mode 1`) | `PC10` |
| `SW_POS_RIGHT` (`3`) | Toggle switch **right** pole (`Mode 2`) | `PC11` |

> The 3-position ON-OFF-ON switch has its **common terminal wired to `GND`**. Left position → `SW_STATE_LEFT_ON` (`Mode 1`), center → `SW_STATE_CENTER_OFF` (standby), right → `SW_STATE_RIGHT_ON` (`Mode 2`). The library only tracks and reports this state — the application decides what each mode does.

### 2.2 Outputs (power LED, status LED, LED strip)

| Signal | Pin | Connector | Behavior |
|---|---|---|---|
| Power LED | `PA5` | Board LD2 / `D13` | Always on after init |
| Status LED | `PA7` | `D11` / CN10 Pin 15 | Flashes `150 ms` on every force change |
| WS2812B data (main) | `PA6` | `D12` / CN10 Pin 13 | 6-LED strip data line |
| WS2812B data (backup) | `PB6` | `D10` | Identical synchronized signal (spare connector) |

> `PA6` and `PB6` always carry the **same** bitstream — connect the strip's `DIN` to either one.

---

## 3. Quick Start Guide (`main.c` Example)

No GPIO setup is needed in `.ioc` (STM32CubeMX). Define the pin array and poll `AnkleUI_Update()` inside your main loop:

```c
#include "main.h"
#include "interface_button.h"

/* 1. Define the 4-input pin mapping (2 buttons + 2 switch poles) */
const AnkleBtnPin_t my_pins[INPUT_COUNT] = {
    [BTN_UP]       = PC12,
    [BTN_DOWN]     = PD2,
    [SW_POS_LEFT]  = PC10,
    [SW_POS_RIGHT] = PC11
};

/* 2. Declare the controller state (0..20 fixed point = 0.0..10.0 kg) */
AnkleCtrl_t ankle_ctrl = { .max_raw = 20 };

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 3. Init: enables clocks, configures pull-ups & LED outputs,
          runs the stuck-key check and the power-on strip self-test */
    AnkleUI_Init(my_pins);

    while (1) {
        /* 4. Non-blocking scan (uses HAL_GetTick()) */
        AnkleUI_Update(&ankle_ctrl);

        /* 5. Handle state changes flagged by the library */
        if (ankle_ctrl.ui_updated) {
            // ankle_ctrl.raw_value          (uint8_t: 0..20 → 0.0..10.0 kg, step 0.5)
            // ankle_ctrl.display_percentage (uint8_t: 0..100 %)
            // ankle_ctrl.active_leds        (uint8_t: 0..6)
            // ankle_ctrl.sw_state           (SW_STATE_CENTER_OFF / LEFT_ON / RIGHT_ON)
            // ankle_ctrl.btn_conflict       (bool: UP & DOWN held together)
            // ankle_ctrl.sw_fault           (bool: both switch poles active)
            ankle_ctrl.ui_updated = false;   // clear after handling
        }
        HAL_Delay(1);
    }
}
```

The demo `main.c` additionally prints a status line to **UART2 (115200 8N1)** on every change, plus one-shot `[POKA-YOKE]` warnings when a safety interlock trips.

---

## 4. Behavior Details

### 4.1 Buttons & Auto-Repeat

* Buttons are **edge-triggered**: each clean press steps the force by `±0.5 kg`.
* **Auto-repeat**: holding a button for `500 ms` triggers rapid repeat every `150 ms`.
* Force is clamped to `min_raw` (`0.0 kg`) and `max_raw` (`10.0 kg`) — saturated at the bounds, no wraparound.

### 4.2 Toggle Switch

The switch is **level-sensitive** (maintained contact). Its position is sampled through the same 20 ms debouncer and reported as `sw_state`:

| Position | `sw_state` | Meaning |
|---|---|---|
| Left | `SW_STATE_LEFT_ON` | Mode 1 |
| Center | `SW_STATE_CENTER_OFF` | Standby / OFF |
| Right | `SW_STATE_RIGHT_ON` | Mode 2 |

Unlike the previous 7-button design, the library applies **no mode-specific lockouts** to the buttons — force adjustment works in any switch position; mode semantics belong to the application.

### 4.3 WS2812B LED Strip

The 6-LED strip is a proportional force gauge. Colors use **GRB** ordering:

| LED | Color | Force Range |
|---|---|---|
| 1 | Pure green | 0.5 – 1.5 kg |
| 2 | Grass green | 2.0 – 3.0 kg |
| 3 | Bright yellow | 3.5 – 5.0 kg |
| 4 | Bright orange | 5.5 – 6.5 kg |
| 5 | Deep orange-red | 7.0 – 8.5 kg |
| 6 | Pure red | 9.0 – 10.0 kg |

* **0.0 kg standby**: LED 1 stays lit with a faint ice-blue glow (`B = 40`) so you can confirm the strip is alive at a glance.
* **Power-on self-test**: on `AnkleUI_Init()`, a chase animation sweeps across all 6 LEDs (~0.4 s), then returns to standby.
* **Timing** (bit-banged via the DWT cycle counter, `__attribute__((optimize("O2")))`):
  * `T0H ≈ 350 ns`, `T1H ≈ 800 ns`, bit period ≈ `1250 ns`
  * `500 µs` low latch-reset — guarantees the new-generation WS2812B latches 100% reliably
  * Interrupts are disabled for ~680 µs per frame (6 LEDs × 24 bits × 1.25 µs + 500 µs reset) — no timers, PWM, or DMA used, so the strip works on any GPIO pin.

### 4.4 Safety Interlocks (Poka-Yoke)

| Protection | Description |
|---|---|
| **1. UP/DOWN Conflict Lock** | If `BTN_UP` and `BTN_DOWN` are held simultaneously, the force value is frozen (`btn_conflict = true`) until one button is released — prevents value jitter. |
| **2. Switch Short-Circuit Fault** | If both switch poles read active at once (wiring fault), the library falls back to `SW_STATE_CENTER_OFF` and raises `sw_fault = true` — fails safe. |
| **3. Power-On Stuck-Key Guard** | If a button is held (`LOW`) at power-up (jammed button or shorted wire), its internal `armed` flag is cleared. It cannot press or auto-repeat until released (`HIGH`) for ≥ `20 ms`. |

---

## 5. Public APIs & Configurable Macros

### Public Functions

* `void AnkleUI_Init(const AnkleBtnPin_t* pin_config)`
  Enables GPIO clocks, configures all 4 inputs as `GPIO_PULLUP`, initializes the power/status LEDs and the strip data pins, performs the stuck-key check, and runs the power-on self-test.
* `void AnkleUI_Update(AnkleCtrl_t* ctrl)`
  Non-blocking scan function to be called inside `while(1)`. Uses `HAL_GetTick()`. Sets `ctrl->ui_updated = true` whenever the force value, switch state, or a fault flag changes.

### Controller State (`AnkleCtrl_t`, in `interface_button.h`)

| Field | Type | Description |
|---|---|---|
| `raw_value` | `uint8_t` | Internal fixed-point force: `0..20` → `0.0..10.0 kg` (step `0.5`) |
| `min_raw` / `max_raw` | `uint8_t` | Force bounds (default `0` / `20`) |
| `display_percentage` | `uint8_t` | Force progress `0..100 %` |
| `active_leds` | `uint8_t` | Lit strip LEDs `0..6` |
| `sw_state` | `ToggleSwitchState_t` | `SW_STATE_CENTER_OFF` / `SW_STATE_LEFT_ON` / `SW_STATE_RIGHT_ON` |
| `btn_conflict` | `bool` | Poka-Yoke 1: UP & DOWN held together |
| `sw_fault` | `bool` | Poka-Yoke 2: both switch poles active |
| `ui_updated` | `bool` | Change flag (clear after handling) |

### Pin Shorthand Macros (`interface_button.h`)

`PC10`, `PC11`, `PC12`, `PD2` — expand to `(AnkleBtnPin_t){GPIOx, GPIO_PIN_x}` for readable pin arrays.

### Configurable Macros

| Macro | Location | Default | Description |
|---|---|---|---|
| `DEBOUNCE_DELAY_MS` | `interface_button.c` | `20` | Debounce filter window |
| `AUTO_REPEAT_DELAY_MS` | `interface_button.c` | `500` | Hold time before auto-repeat starts |
| `AUTO_REPEAT_RATE_MS` | `interface_button.c` | `150` | Auto-repeat interval |
| `STATUS_LED_ON_MS` | `interface_button.h` | `150` | Status LED flash duration |
| `NUM_LEDS` | `interface_button.h` | `6` | Strip LED count |
