# ⚡ ELEC — Electronics & Firmware

All electronics and embedded firmware for the **Actuation Belt Trainer**.

> The two firmware projects are **git submodules** pointing at their own repositories. Clone this repo with:
> ```bash
> git clone --recursive https://github.com/koilkl/y3yp.git
> ```
> If you already cloned without `--recursive`, run `git submodule update --init --recursive`.

## Layout

| Path | Description |
|------|-------------|
| [`Version-1.0/Year_3_first_prototype`](https://github.com/yuu033/Year_3_first_prototype) | Full first-prototype firmware (STM32F103C6T6): CAN motor control (C620 ESCs), IMU feedback (BMI270 + Modbus IMU), PID control loop |
| [`Component/Incliometer`](https://github.com/koilkl/Incliometer) | Standalone inclinometer data-acquisition project (STM32F401RETx, Modbus-RTU) used to validate pedal angle sensing |

## System Context

Both projects are STM32CubeMX-generated HAL projects built with CMake (`arm-none-eabi` GCC + Ninja). Together they implement the control loop described in the main [README](../readme.md): the MCU reads the pedal angle from an IMU, runs a PID controller, and drives the M3508/C620 motors over CAN to apply programmable resistance.

## Related

- Mechanical design: [`../MECH/`](../MECH/README.md)
- Presentations & demo: [`../DOCUMENT/`](../DOCUMENT/README.md)
