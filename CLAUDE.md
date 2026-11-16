# STM32 Embedded Sensor Gateway - context for Claude Code

NUCLEO-F446RE firmware (STM32 HAL, STM32CubeIDE) that reads an ADC (internal
temp sensor, VREFINT, diode temperature sensor on PA0, divider on PA1), an
I2C sensor and an SPI sensor (both emulated by an ESP32-S3), with a
TIM6-driven scheduler, EXTI button and an interrupt-driven UART console.

## Layout
- `firmware/stm32/` - STM32CubeIDE project. Our code: `Core/Src/app/`, `Core/Inc/app/`.
  CubeMX-generated files outside `app/` must only be edited inside `USER CODE` blocks.
- `firmware/esp32s3_sensor_emulator/` - Arduino sketch (I2C slave 0x48 + SPI slave).
- `tools/ci/` - command-line build used by GitHub Actions (`make` after fetching HAL).
- `docs/` - wiring, CubeMX settings, logic-analyser captures.

## Conventions
- C11, no dynamic memory, no floats in printf (fixed-point: mV, centi-degrees).
- ISRs only set flags / copy bytes; work happens in `app_loop()`.

## Git rules for Claude Code
- Commit messages: Conventional Commits, `type(scope): summary` + short bullet body.
- One logical change per commit. Never commit build output, `Debug/`, `.DS_Store`,
  model weights (`*.pt`, `*.onnx`, `*.tflite`) or anything secret.
- Never rewrite history, never `git push --force`, never change git config.
- If the checks in `ship.sh` fail, stop and explain the failure instead of committing.
