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
CXX    := $(DEVKITPPC)/bin/powerpc-eabi-g++
SRCS := $(wildcard src/*/*.c)
CXXSRCS := $(wildcard src/game/*.cpp)   # Recast & Detour pathfinding (C++)
HDRS := $(wildcard src/*/*.h)
CFLAGS  := -O2 -ffast-math -fno-math-errno -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -std=gnu11 \
           -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
           -D__WIIU__ -D__WUT__ -I$(WUT_ROOT)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections -L$(WUT_ROOT)/lib
LIBS    := -lwut -lm
CXXFLAGS := -O2 -ffast-math -fno-math-errno -w -std=gnu++17 -fno-exceptions -fno-rtti -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections -D__WIIU__ -D__WUT__ -I$(WUT_ROOT)/include -Isrc
NAVOBJS := $(CXXSRCS:src/game/%.cpp=build/%.o)
build/%.o: src/game/%.cpp $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

all: $(TARGET).wuhb

# `make tour` builds dragonsbeyond-tour.rpx: the game playing itself, for the emulator screenshots in CI
tour: $(TARGET)-tour.rpx
$(TARGET)-tour.elf: $(SRCS) $(HDRS) $(NAVOBJS)
	$(CC) $(CFLAGS) -DDB_TOUR -c $(SRCS)
	$(CXX) $(CFLAGS) $(notdir $(SRCS:.c=.o)) $(NAVOBJS) $(LDFLAGS) $(LIBS) -o $@
	rm -f $(notdir $(SRCS:.c=.o))
$(TARGET)-tour.rpx: $(TARGET)-tour.elf
	$(DEVKITPRO)/tools/bin/elf2rpl $< $@

$(TARGET).elf: $(SRCS) $(HDRS) $(NAVOBJS)
	$(CC) $(CFLAGS) -c $(SRCS)
	$(CXX) $(CFLAGS) $(notdir $(SRCS:.c=.o)) $(NAVOBJS) $(LDFLAGS) $(LIBS) -o $@
	rm -f $(notdir $(SRCS:.c=.o))

$(TARGET).rpx: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/elf2rpl $< $@

$(TARGET).wuhb: $(TARGET).rpx content/hello.txt content/icon.png assets/splash_tv.png assets/splash_drc.png
	$(DEVKITPRO)/tools/bin/wuhbtool --content content --name "Dragons Beyond" \
		--short-name "Dragons Beyond" --author "Sean and the designer" --icon content/icon.png --tv-image assets/splash_tv.png --drc-image assets/splash_drc.png $< $@

# PC checks (no graphics): the dragon logic, and the whole game with a renderer that draws nothing
TESTFLAGS := -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-misleading-indentation -fsanitize=address,undefined -DKK_CONTENT_DIR='"content/"' -Isrc
test: navt.a
	gcc $(TESTFLAGS) tests/dragon_test.c src/game/world.c src/game/dragon.c navt.a -lstdc++ -lm -o dragon_test && ./dragon_test
	gcc $(TESTFLAGS) tests/boss_test.c src/game/boss.c src/game/world.c navt.a -lstdc++ -lm -o boss_test && ./boss_test
	gcc $(TESTFLAGS) tests/game_test.c tests/gfx_stub.c src/game/game.c src/game/world.c src/game/dragon.c src/game/anim3d.c src/game/props.c src/game/boss.c src/render/assets.c src/audio/sfx.c src/audio/music.c navt.a -lstdc++ -lm -lpthread -o game_test && ASAN_OPTIONS=detect_leaks=0 ./game_test
	gcc $(TESTFLAGS) tests/coop_test.c tests/gfx_stub.c src/game/game.c src/game/world.c src/game/dragon.c src/game/anim3d.c src/game/props.c src/game/boss.c src/render/assets.c src/audio/sfx.c src/audio/music.c navt.a -lstdc++ -lm -lpthread -o coop_test && ASAN_OPTIONS=detect_leaks=0 ./coop_test
	gcc $(TESTFLAGS) tests/sky_test.c tests/gfx_stub.c src/game/game.c src/game/world.c src/game/dragon.c src/game/anim3d.c src/game/props.c src/game/boss.c src/render/assets.c src/audio/sfx.c src/audio/music.c navt.a -lstdc++ -lm -lpthread -o sky_test && ASAN_OPTIONS=detect_leaks=0 ./sky_test
	gcc $(TESTFLAGS) -DDB_TOUR tests/tour_test.c tests/gfx_stub.c src/game/game.c src/game/world.c src/game/dragon.c src/game/anim3d.c src/game/props.c src/game/boss.c src/render/assets.c src/audio/sfx.c src/audio/music.c navt.a -lstdc++ -lm -lpthread -o tour_test && ASAN_OPTIONS=detect_leaks=0 ./tour_test

navt.a: $(wildcard src/game/*.cpp) src/game/nav.h src/game/world.h
	for f in src/game/*.cpp; do g++ -O1 -g -w -std=gnu++17 -fno-exceptions -fsanitize=address,undefined -Isrc -c $$f -o $$(basename $$f .cpp)_t.o || exit 1; done; ar rcs $@ *_t.o; rm -f *_t.o

clean:
	rm -f $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb $(TARGET)-tour.elf $(TARGET)-tour.rpx dragon_test game_test boss_test coop_test sky_test tour_test navt.a
	rm -rf build
.PHONY: all clean test tour
