# Dragons Beyond - Wii U build (devkitPro + wut). Produces dragonsbeyond.wuhb
.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
ifneq ($(MAKECMDGOALS),test)
$(error DEVKITPRO is not set. Install devkitPro and export DEVKITPRO=/opt/devkitpro)
endif
endif
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
TARGET := dragonsbeyond
CC     := $(DEVKITPPC)/bin/powerpc-eabi-gcc
SRCS := $(wildcard src/*/*.c)
HDRS := $(wildcard src/*/*.h)
CFLAGS  := -O2 -ffast-math -fno-math-errno -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -std=gnu11 \
           -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
           -D__WIIU__ -D__WUT__ -I$(WUT_ROOT)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections -L$(WUT_ROOT)/lib
LIBS    := -lwut -lm

all: $(TARGET).wuhb

# `make tour` builds dragonsbeyond-tour.rpx: the game playing itself, for the emulator screenshots in CI
tour: $(TARGET)-tour.rpx
$(TARGET)-tour.elf: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -DDB_TOUR $(SRCS) $(LDFLAGS) $(LIBS) -o $@
$(TARGET)-tour.rpx: $(TARGET)-tour.elf
	$(DEVKITPRO)/tools/bin/elf2rpl $< $@

$(TARGET).elf: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).rpx: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/elf2rpl $< $@

$(TARGET).wuhb: $(TARGET).rpx content/hello.txt content/icon.png assets/splash_tv.png assets/splash_drc.png
	$(DEVKITPRO)/tools/bin/wuhbtool --content content --name "Dragons Beyond" \
		--short-name "Dragons Beyond" --author "Sean and the designer" --icon content/icon.png --tv-image assets/splash_tv.png --drc-image assets/splash_drc.png $< $@

# PC checks (no graphics): the dragon logic, and the whole game with a renderer that draws nothing
TESTFLAGS := -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-misleading-indentation -fsanitize=address,undefined -DKK_CONTENT_DIR='"content/"' -Isrc
test:
	gcc $(TESTFLAGS) tests/dragon_test.c src/game/world.c src/game/dragon.c -lm -o dragon_test && ./dragon_test
	gcc $(TESTFLAGS) tests/boss_test.c src/game/boss.c src/game/world.c -lm -o boss_test && ./boss_test
	gcc $(TESTFLAGS) tests/game_test.c tests/gfx_stub.c src/game/game.c src/game/world.c src/game/dragon.c src/game/anim3d.c src/game/props.c src/game/boss.c src/render/assets.c src/audio/sfx.c src/audio/music.c -lm -lpthread -o game_test && ASAN_OPTIONS=detect_leaks=0 ./game_test
	gcc $(TESTFLAGS) tests/coop_test.c tests/gfx_stub.c src/game/game.c src/game/world.c src/game/dragon.c src/game/anim3d.c src/game/props.c src/game/boss.c src/render/assets.c src/audio/sfx.c src/audio/music.c -lm -lpthread -o coop_test && ASAN_OPTIONS=detect_leaks=0 ./coop_test
	gcc $(TESTFLAGS) tests/sky_test.c tests/gfx_stub.c src/game/game.c src/game/world.c src/game/dragon.c src/game/anim3d.c src/game/props.c src/game/boss.c src/render/assets.c src/audio/sfx.c src/audio/music.c -lm -lpthread -o sky_test && ASAN_OPTIONS=detect_leaks=0 ./sky_test

clean:
	rm -f $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb $(TARGET)-tour.elf $(TARGET)-tour.rpx dragon_test game_test boss_test coop_test sky_test
.PHONY: all clean test tour
