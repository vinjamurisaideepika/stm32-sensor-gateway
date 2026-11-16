# STM32CubeMX configuration (inside STM32CubeIDE)

Create the project: **File → New → STM32 Project → Board Selector →
NUCLEO-F446RE**, name it `stm32` and save it in `firmware/` (so the project is
`firmware/stm32/`). Answer **Yes** to "Initialize all peripherals with their
default Mode?". This already sets up USART2 (ST-LINK virtual COM port),
LD2 (PA5) and B1 (PC13).

Then change the settings below in the `.ioc` file and press **Ctrl+S**
(on a Mac **Cmd+S**) to generate the code.

## Clock Configuration tab
* Type **84** into the **HCLK (MHz)** box and press Enter. CubeMX solves the
  PLL for you. Result: SYSCLK 84 MHz, APB1 42 MHz, APB1 timer clock 84 MHz.

## Pinout & Configuration tab

| Peripheral | Setting | Value |
|---|---|---|
| **ADC1** | IN0, IN1 | tick both (PA0, PA1) |
| | Temperature Sensor Channel | tick |
| | Vrefint Channel | tick |
| | Scan / Continuous mode | Disabled / Disabled |
| | Resolution | 12 bits |
| **I2C1** | Mode | I2C |
| | Pins | click **PB8 → I2C1_SCL** and **PB9 → I2C1_SDA** in the chip view (the default may be PB6/PB7) |
| | Speed | Standard mode, 100 kHz |
| **SPI2** | Mode | Full-Duplex Master, Hardware NSS **Disable** |
| | Pins | **PB13** SCK, **PB14** MISO, **PB15** MOSI (click them if CubeMX picked others) |
| | Data size / First bit | 8 bits / MSB first |
| | Prescaler | **32** → 1.3125 Mbit/s |
| | CPOL / CPHA | Low / 1 Edge (SPI mode 0) |
| **PB12** | GPIO_Output, label **SPI2_CS**, initial level **High** | user label is exactly `SPI2_CS` |
| **TIM6** | Activated | tick |
| | Prescaler / Counter Period | **8399** / **999** → 10 Hz |
| | NVIC | tick **TIM6 global interrupt, DAC1 and DAC2 underrun error** |
| **USART2** | Already 115200 8N1 | NVIC: tick **USART2 global interrupt** |
| **GPIO → NVIC** | EXTI line[15:10] interrupts | tick (button B1) |
| **SYS** | Timebase Source | leave **SysTick** |

## Project Manager tab
* Code Generator: tick **"Generate peripheral initialization as a pair of
  '.c/.h' files per peripheral"** - optional, but tidier.

## Add our code
1. Copy `Core/Inc/app/` and `Core/Src/app/` from this repository into the
   generated project (they are already there if you generated *inside* the
   cloned repository).
2. In `Core/Src/main.c`:

```c
/* USER CODE BEGIN Includes */
#include "app/app.h"
/* USER CODE END Includes */
...
  /* USER CODE BEGIN 2 */
  app_init();
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    app_loop();
  }
  /* USER CODE END 3 */
```

3. Build (hammer icon), flash (green Run arrow), and open a serial terminal at
   115200 baud on the ST-LINK port (`screen /dev/tty.usbmodem* 115200` on macOS;
   quit with Ctrl+A then K).

Code you write between `USER CODE BEGIN`/`END` survives regeneration; anything
else in generated files is overwritten.
