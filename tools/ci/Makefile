# Command-line build used by GitHub Actions to prove the firmware compiles.
# (Day to day you build in STM32CubeIDE; see docs/cubemx-config.md.)
#
#   make HAL_DIR=... CMSIS_DEVICE_DIR=... CMSIS_CORE_DIR=...
#
# Override APP_DIR to reuse this Makefile for another project.
HAL_DIR          ?= vendor/stm32f4xx_hal_driver
CMSIS_DEVICE_DIR ?= vendor/cmsis_device_f4
CMSIS_CORE_DIR   ?= vendor/CMSIS_5/CMSIS/Core
APP_DIR          ?= ../../firmware/stm32/Core
HAL_MODULES      ?= hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_dma hal_pwr hal_pwr_ex \
                    hal_flash hal_flash_ex hal_adc hal_adc_ex hal_i2c hal_spi hal_tim \
                    hal_tim_ex hal_uart hal_exti
BUILD := build
TARGET := $(BUILD)/firmware.elf

CC := arm-none-eabi-gcc
MCU := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS := $(MCU) -std=c11 -Os -g -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
          -DSTM32F446xx -DUSE_HAL_DRIVER \
          -IInc -I$(APP_DIR)/Inc -I$(HAL_DIR)/Inc \
          -I$(CMSIS_DEVICE_DIR)/Include -I$(CMSIS_CORE_DIR)/Include
LDFLAGS := $(MCU) -TSTM32F446RETX_FLASH.ld -Wl,--gc-sections -Wl,-Map=$(BUILD)/firmware.map \
           --specs=nano.specs --specs=nosys.specs -Wl,--print-memory-usage

APP_SRCS := $(wildcard $(APP_DIR)/Src/app/*.c)
HAL_SRCS := $(foreach m,$(HAL_MODULES),$(HAL_DIR)/Src/stm32f4xx_$(m).c)
SRCS := $(wildcard Src/*.c) $(APP_SRCS) $(HAL_SRCS) \
        $(CMSIS_DEVICE_DIR)/Source/Templates/system_stm32f4xx.c
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(SRCS))) $(BUILD)/startup.o
vpath %.c $(sort $(dir $(SRCS)))

all: $(TARGET)

# HAL sources are third-party: don't fail the build on their warnings.
$(BUILD)/stm32f4xx_%.o $(BUILD)/system_stm32f4xx.o: CFLAGS := $(filter-out -Werror -Wextra,$(CFLAGS))

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup.o: $(CMSIS_DEVICE_DIR)/Source/Templates/gcc/startup_stm32f446xx.s | $(BUILD)
	$(CC) $(MCU) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	arm-none-eabi-size $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
.PHONY: all clean
