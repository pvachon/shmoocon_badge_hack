TARGET=shmoo
OBJ=shmoo.o memchr.o

SDKREV=ESP8266_NONOS_SDK_V2.0.0_16_08_10

CROSS_COMPILE=xtensa-lx106-elf-
OFLAGS=-O2 -g
SDKDIR=$(HOME)/src/esp-open-sdk/$(SDKREV)

CC = $(CROSS_COMPILE)gcc
SDK_INCLUDES=-I$(SDKDIR)/include -I$(SDKDIR)/driver_lib/include

CFLAGS = -I. -mlongcalls $(OFLAGS) $(SDK_INCLUDES) -std=c99 -DICACHE_FLASH
LIBS=-lmain -lnet80211 -lwpa -llwip -lpp -lphy -ldriver
LDLIBS = -nostdlib -Wl,-EL -Wl,--start-group $(LIBS) -Wl,--end-group -lgcc
LDFLAGS = -Teagle.app.v6.ld


$(TARGET)-0x00000.bin: $(TARGET)
	esptool.py elf2image $^

$(TARGET): $(OBJ)

flash: $(TARGET)-0x00000.bin
	esptool.py --baud 576000 write_flash 0 $(TARGET)-0x00000.bin 0x10000 $(TARGET)-0x10000.bin

clean:
	rm -f $(TARGET) $(OBJ) $(TARGET)-0x00000.bin $(TARGET)-0x10000.bin

.PHONY: clean flash
