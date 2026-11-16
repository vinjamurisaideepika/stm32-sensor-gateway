# Wiring

> **Golden rules:** unplug USB while you change wires. Connect **GND to GND**
> between every board. Never connect the 3V3 pins of the two boards together:
> each board is powered by its own USB cable.

## Pin map

| Signal | NUCLEO-F446RE pin | Where on the Nucleo | ESP32-S3-DevKitC-1 | Notes |
|---|---|---|---|---|
| I2C SCL | PB8 | Arduino header **D15** (CN5) | GPIO9 | 4.7 kΩ pull-up to 3V3 |
| I2C SDA | PB9 | Arduino header **D14** (CN5) | GPIO8 | 4.7 kΩ pull-up to 3V3 |
| SPI CS | PB12 | Morpho **CN10 pin 16** | GPIO10 | |
| SPI SCK | PB13 | Morpho **CN10 pin 30** | GPIO12 | |
| SPI MISO | PB14 | Morpho **CN10 pin 28** | GPIO13 | ESP32 → STM32 |
| SPI MOSI | PB15 | Morpho **CN10 pin 26** | GPIO11 | STM32 → ESP32 |
| Diode sensor | PA0 | Arduino header **A0** (CN8) | - | see circuit below |
| Voltage divider | PA1 | Arduino header **A1** (CN8) | - | see circuit below |
| Ground | GND | CN6 GND | GND | **mandatory** |
| 3.3 V | 3V3 | CN6 3V3 | - | for pull-ups, diode and divider only |

Morpho pin numbers come from ST user manual UM1724. Always double-check
the small labels printed next to the pins on your board.

## Circuits on the breadboard

```
 I2C pull-ups                 Diode temperature sensor        Voltage divider
 (one for each line)

 3V3 ──[4.7k]──┬── SDA        3V3 ──[10k]──┬──── PA0 (A0)     3V3 ──[10k]──┬── PA1 (A1)
               │                           │                               │
 3V3 ──[4.7k]──┼── SCL                 anode ▼ 1N4148             GND ──[10k]──┘
                                      cathode │      ┬ 100 nF
                                             GND     ┴ (optional,
                                                       PA0 to GND)
```

* **Diode sensor:** about 0.27 mA flows through the diode; its forward
  voltage (~0.6 V) drops by ~2 mV for every +1 °C. Touch the diode with a
  warm finger and watch the reading change. The 100 nF ceramic capacitor
  filters noise.
* **Divider:** two equal resistors give 3.3 V / 2 ≈ 1650 mV. Swap in other
  values to check your ADC maths: Vout = 3.3 V × R2 / (R1 + R2).

## Logic analyser hookup (8-channel USB analyser + PulseView)

| Channel | Signal | PulseView decoder |
|---|---|---|
| D0 | SCL (PB8) | I²C: SCL = D0, SDA = D1 |
| D1 | SDA (PB9) | |
| D2 | CS (PB12) | SPI: CLK = D3, MOSI = D4, MISO = D5, CS# = D2, mode 0 |
| D3 | SCK (PB13) | |
| D4 | MOSI (PB15) | |
| D5 | MISO (PB14) | |
| GND | GND | - |

Sample rate: at least **8 MHz** (SPI runs at 1.31 MHz). Capture 1 s, then add
the I²C and SPI decoders. Save screenshots into `docs/images/` for the README.
