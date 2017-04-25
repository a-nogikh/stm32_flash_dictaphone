# http://www.st.com/internet/com/TECHNICAL_RESOURCES/TECHNICAL_LITERATURE/APPLICATION_NOTE/DM00040802.pdf


TARGET:=MEDIA_intFLASH
TOOLCHAIN_BASE:=/home/alexander/gcc-arm-none-eabi-6-2017-q1-update
TOOLCHAIN_PATH:=$(TOOLCHAIN_BASE)/bin/
TOOLCHAIN_PREFIX:=arm-none-eabi
OPTLVL:=3 # Optimization level, can be [0, 1, 2, 3, s].

#PROJECT_NAME=$(notdir $(lastword $(CURDIR)))
TOP:=$(shell readlink -f "../..")
DISCOVERY:=$(TOP)/Utilities/STM32F4-Discovery
STMLIB:=$(TOP)/Libraries
STD_PERIPH:=$(STMLIB)/STM32F4xx_StdPeriph_Driver
STARTUP:=$(STMLIB)/CMSIS/ST/STM32F4xx/Source/Templates/gcc_ride7
LINKER_SCRIPT:=$(CURDIR)/stm32f4.ld

INCLUDE=-I$(CURDIR)
INCLUDE+=-I$(CURDIR)/src
INCLUDE+=-I$(STMLIB)/CMSIS/ST/STM32F4xx/Include
INCLUDE+=-I$(STMLIB)/CMSIS/Include
INCLUDE+=-I$(TOP)/Utilities/Third_Party/fat_fs/inc
INCLUDE+=-I$(STD_PERIPH)/inc
INCLUDE+=-I$(DISCOVERY)

# vpath is used so object files are written to the current directory instead
# of the same directory as their source files
vpath %.c $(CURDIR)/src \
          $(DISCOVERY) $(STD_PERIPH)/src 
vpath %.s $(STARTUP)

ASRC=startup_stm32f4xx.s

# Project Source Files
SRC+=main.c
SRC+=recorder.c
SRC+=system_stm32f4xx.c
SRC+=stm32f4xx_it.c

# Discovery Source Files
SRC+=stm32f4_discovery.c
SRC+=stm32f4_discovery_lis302dl.c
SRC+=stm32f4_discovery_audio_codec.c

# Standard Peripheral Source Files
SRC+=stm32f4xx_syscfg.c
SRC+=misc.c
SRC+=stm32f4xx_adc.c
SRC+=stm32f4xx_dac.c
SRC+=stm32f4xx_dma.c
SRC+=stm32f4xx_exti.c
SRC+=stm32f4xx_flash.c
SRC+=stm32f4xx_gpio.c
SRC+=stm32f4xx_i2c.c
SRC+=stm32f4xx_rcc.c
SRC+=stm32f4xx_spi.c
SRC+=stm32f4xx_tim.c

CDEFS=-DUSE_STDPERIPH_DRIVER
CDEFS+=-DSTM32F4XX -D__FPU_PRESENT="1" -D__FPU_USED="1" -D__VFP_FP__


MCUFLAGS=-mthumb -mlittle-endian  -mfloat-abi=softfp -mfpu=fpv5-sp-d16 -march=armv7-m -mthumb-interwork 
#MCUFLAGS=-mcpu=cortex-m4 -mfpu=vfpv4-sp-d16 -mfloat-abi=hard
COMMONFLAGS=-O$(OPTLVL) -g -Wall -Werror  -Wno-misleading-indentation -Wno-unused-variable
CFLAGS=$(COMMONFLAGS) $(MCUFLAGS) $(INCLUDE) $(CDEFS) 

LIBDIR=$(DISCOVERY)

# libraries to link to
LDLIBS=-lm
LDLIBS+=-lPDMFilter_GCC
#LDLIBS += -lpdm_filter

LDFLAGS=$(COMMONFLAGS) -v -fno-exceptions -ffunction-sections -fdata-sections \
        -nostartfiles -nostdlib -L$(TOOLCHAIN_BASE)/lib/gcc/arm-none-eabi/6.3.1/thumb/v7-m/  -Wl,--gc-sections,-T$(LINKER_SCRIPT),-Map=my.map 

#####
#####

OBJ = $(SRC:%.c=%.o) $(ASRC:%.s=%.o)

CC=$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)-gcc
LD=$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)-gcc
OBJCOPY=$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)-objcopy
AS=$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)-as
AR=$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)-ar
GDB=$(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)-gdb


all: $(OBJ)
	$(CC) -o $(TARGET).elf $(LDFLAGS) $(OBJ)	-L$(LIBDIR) $(LDLIBS) -lgcc
	$(OBJCOPY) -O ihex   $(TARGET).elf $(TARGET).hex
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

.PHONY: clean

clean:
	rm -f $(OBJ)
	rm -f $(TARGET).elf
	rm -f $(TARGET).hex
	rm -f $(TARGET).bin
