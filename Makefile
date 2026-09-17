CROSS  := arm-none-eabi-
CC     := $(CROSS)gcc
CFLAGS := -mcpu=cortex-m3 -mthumb -O2 -ffreestanding -fno-builtin \
          -nostdlib -Wall -Wextra
LDFLAGS := -mcpu=cortex-m3 -mthumb -nostdlib -T linker.ld \
           -Wl,--build-id=none -Wl,-Map,build/minios.map

SRCS := src/start.S src/uart.c src/task.c src/kernel.c
OBJS := $(patsubst src/%.c,build/%.o,$(filter %.c,$(SRCS))) \
        build/start.o

build/minios.elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

build/start.o: src/start.S
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.c src/kernel.h
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

$(OBJS): | build

run: build/minios.elf
	qemu-system-arm -M lm3s6965evb -nographic -monitor none \
		-serial stdio -kernel build/minios.elf

test: build/minios.elf
	bash test.sh

clean:
	rm -rf build

.PHONY: run test clean
