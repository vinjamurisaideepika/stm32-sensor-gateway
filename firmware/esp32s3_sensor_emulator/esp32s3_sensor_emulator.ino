/**
 * ESP32-S3 "virtual sensor" emulator for the STM32 Embedded Sensor Gateway.
 *
 * Because we don't have real I2C/SPI sensor modules, the ESP32-S3 pretends
 * to be two of them at the same time:
 *
 *   1. I2C slave @ 0x48 : "environment sensor" (temperature + humidity)
 *   2. SPI slave        : "accelerometer" (X/Y/Z in milli-g)
 *
 * The STM32 talks to it with ordinary HAL calls, exactly like real chips.
 *
 * Board : ESP32-S3-DevKitC-1 (N8R8)
 * Core  : Arduino-ESP32 3.x (Boards Manager: "esp32" by Espressif)
 * Plug the USB cable into the port labelled "UART" (Serial Monitor 115200).
 *
 * Wiring (see docs/wiring.md):
 *   ESP32-S3 GPIO8  (SDA)  <-> STM32 PB9 (D14)   + 4.7k pull-up to 3V3
 *   ESP32-S3 GPIO9  (SCL)  <-> STM32 PB8 (D15)   + 4.7k pull-up to 3V3
 *   ESP32-S3 GPIO10 (CS)   <-  STM32 PB12
 *   ESP32-S3 GPIO12 (SCK)  <-  STM32 PB13
 *   ESP32-S3 GPIO13 (MISO) ->  STM32 PB14
 *   ESP32-S3 GPIO11 (MOSI) <-  STM32 PB15
 *   GND                    <-> GND  (always connect the grounds!)
 */
#include <Arduino.h>
#include <Wire.h>
#include "driver/spi_slave.h"

// ----------------------------------------------------------------- pins
static const int PIN_I2C_SDA = 8;
static const int PIN_I2C_SCL = 9;
static const int PIN_SPI_CS = 10;
static const int PIN_SPI_MOSI = 11;
static const int PIN_SPI_SCK = 12;
static const int PIN_SPI_MISO = 13;

// ------------------------------------------------------- I2C register map
static const uint8_t I2C_ADDR = 0x48;
static const uint8_t REG_TEMP_H = 0x00;  // int16, centi-degrees C
static const uint8_t REG_HUM_H = 0x02;   // uint16, centi-percent
static const uint8_t REG_COUNTER = 0x04;
static const uint8_t REG_WHO_AM_I = 0x0F;
static const uint8_t REG_MAP_SIZE = 16;

static uint8_t regs[REG_MAP_SIZE];      // the emulated registers
static volatile uint8_t regPointer = 0;  // set by the master's first byte
static portMUX_TYPE regLock = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t i2cReads = 0;

// ------------------------------------------------------------ SPI frame
static const size_t SPI_FRAME_LEN = 9;
static const uint8_t SPI_HEADER = 0xA5;
static uint8_t spiTx[16];
static uint8_t spiRx[16];
static spi_slave_transaction_t spiTrans;
static uint8_t spiSeq = 0;
static uint32_t spiFrames = 0;

// ================================================================== I2C
// Called when the master WRITES to us. For HAL_I2C_Mem_Read() the master
// first writes one byte: the register address it wants to read from.
void onI2cReceive(int len) {
  if (len > 0) {
    uint8_t reg = Wire.read();
    regPointer = (reg < REG_MAP_SIZE) ? reg : 0;
  }
  while (Wire.available()) Wire.read();  // ignore any extra bytes
}

// Called when the master READS from us: send bytes starting at regPointer.
void onI2cRequest() {
  uint8_t out[REG_MAP_SIZE];
  uint8_t start = regPointer;
  uint8_t n = REG_MAP_SIZE - start;
  portENTER_CRITICAL(&regLock);
  memcpy(out, &regs[start], n);
  portEXIT_CRITICAL(&regLock);
  Wire.write(out, n);
  i2cReads++;
}

static void updateI2cRegisters(float t) {
  // Fake but realistic-looking signals: slow sine waves plus a little noise.
  int16_t temp = (int16_t)(2300 + 300 * sinf(t / 20.0f) + random(-5, 6));  // ~23 C
  uint16_t hum = (uint16_t)(4500 + 800 * sinf(t / 35.0f) + random(-10, 11)); // ~45 %

  portENTER_CRITICAL(&regLock);
  regs[REG_TEMP_H] = (uint8_t)(temp >> 8);
  regs[REG_TEMP_H + 1] = (uint8_t)(temp & 0xFF);
  regs[REG_HUM_H] = (uint8_t)(hum >> 8);
  regs[REG_HUM_H + 1] = (uint8_t)(hum & 0xFF);
  regs[REG_COUNTER]++;
  portEXIT_CRITICAL(&regLock);
}

// ================================================================== SPI
// An SPI slave cannot "think" while the master is clocking: the reply must
// be loaded BEFORE the master pulls CS low. So we always keep one
// transaction queued, holding the latest accelerometer frame.
static void buildSpiFrame(float t) {
  // Gravity vector slowly rotating around the X axis (a board being tilted).
  int16_t ax = (int16_t)(50 * sinf(t * 2.0f));
  int16_t ay = (int16_t)(1000 * sinf(t / 4.0f));
  int16_t az = (int16_t)(1000 * cosf(t / 4.0f));

  spiTx[0] = SPI_HEADER;
  spiTx[1] = spiSeq++;
  spiTx[2] = ax >> 8; spiTx[3] = ax & 0xFF;
  spiTx[4] = ay >> 8; spiTx[5] = ay & 0xFF;
  spiTx[6] = az >> 8; spiTx[7] = az & 0xFF;
  uint8_t x = 0;
  for (size_t i = 0; i < SPI_FRAME_LEN - 1; i++) x ^= spiTx[i];
  spiTx[8] = x;  // checksum
}

static void queueSpiTransaction() {
  memset(&spiTrans, 0, sizeof spiTrans);
  spiTrans.length = SPI_FRAME_LEN * 8;  // in bits
  spiTrans.tx_buffer = spiTx;
  spiTrans.rx_buffer = spiRx;
  ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &spiTrans, portMAX_DELAY));
}

static void setupSpiSlave() {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_SPI_MOSI;
  bus.miso_io_num = PIN_SPI_MISO;
  bus.sclk_io_num = PIN_SPI_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;

  spi_slave_interface_config_t slv = {};
  slv.mode = 0;  // CPOL=0, CPHA=0 -> must match the STM32 (SPI mode 0)
  slv.spics_io_num = PIN_SPI_CS;
  slv.queue_size = 1;

  // Pull-ups keep CS/SCK quiet if the STM32 is unplugged (no rogue frames).
  gpio_set_pull_mode((gpio_num_t)PIN_SPI_CS, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)PIN_SPI_SCK, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)PIN_SPI_MOSI, GPIO_PULLUP_ONLY);
  ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &bus, &slv, SPI_DMA_DISABLED));
}

// ================================================================ setup
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-S3 virtual sensor emulator ===");

  regs[REG_WHO_AM_I] = 0xA5;
  updateI2cRegisters(0);

  Wire.onReceive(onI2cReceive);
  Wire.onRequest(onI2cRequest);
  if (!Wire.begin(I2C_ADDR, PIN_I2C_SDA, PIN_I2C_SCL, 100000)) {
    Serial.println("ERROR: I2C slave init failed");
  } else {
    Serial.printf("I2C slave ready at 0x%02X (SDA=%d SCL=%d)\n", I2C_ADDR, PIN_I2C_SDA, PIN_I2C_SCL);
  }

  setupSpiSlave();
  buildSpiFrame(0);
  queueSpiTransaction();
  Serial.println("SPI slave ready (mode 0, 9-byte frames)");
}

// ================================================================= loop
void loop() {
  static uint32_t lastUpdate = 0, lastPrint = 0;
  float t = millis() / 1000.0f;

  // 1) Refresh the I2C "measurement" twice per second.
  if (millis() - lastUpdate >= 500) {
    lastUpdate = millis();
    updateI2cRegisters(t);
  }

  // 2) Did the master finish an SPI transaction? Then queue a fresh frame.
  spi_slave_transaction_t *done;
  if (spi_slave_get_trans_result(SPI2_HOST, &done, 0) == ESP_OK) {
    spiFrames++;
    buildSpiFrame(t);
    queueSpiTransaction();
    rgbLedWrite(RGB_BUILTIN, 0, 8, 0);  // short green flash per SPI frame
  }

  // 3) Status line once per second.
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    Serial.printf("[%6.1fs] I2C reads=%lu  SPI frames=%lu  counter=%u\n",
                  t, (unsigned long)i2cReads, (unsigned long)spiFrames, regs[REG_COUNTER]);
  }
}
