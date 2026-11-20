# Renku Mobile: the phone shell and its input_server add-ons.
#
# On Linux it cross-builds with the toolchain from third_party (see README);
# on Renku/Haiku it builds natively.
#   make            build/$(ARCH)/MobileShell, build/$(ARCH)/renku_virtual_keyboard,
#                   build/$(ARCH)/renku_touch
#   make package    build/renku_mobile-<version>-$(ARCH).hpkg
#   make test       the swipe decoder on the host, then the shell in QEMU

ROOT := $(abspath .)
ARCH ?= x86_64
VERSION := 0.2.0
OUT := build/$(ARCH)

HEADERS := $(ROOT)/third_party/renku-src/headers
override CXXFLAGS += -O2 -Wall -std=c++17 -I$(HEADERS)/private \
	-I$(HEADERS)/private/app -I$(HEADERS)/private/interface

ifeq ($(shell uname -s),Haiku)
CXX ?= g++
INPUT_SERVER := /boot/system/servers/input_server
else
CROSS := $(ROOT)/third_party/renku-src/generated.$(ARCH)/cross-tools-$(ARCH)/bin/$(ARCH)-unknown-haiku-
SYSROOT := $(ROOT)/build/sysroot-$(ARCH)/system
CXX := $(CROSS)g++
# System headers from haiku_devel, after gcc's own: libstdc++'s <cstdlib>
# does #include_next <stdlib.h>.
DEVEL_HEADERS := $(SYSROOT)/develop/headers
override CXXFLAGS += $(addprefix -idirafter ,$(DEVEL_HEADERS)/posix \
	$(DEVEL_HEADERS) $(DEVEL_HEADERS)/os $(wildcard $(DEVEL_HEADERS)/os/*/) \
	$(wildcard $(DEVEL_HEADERS)/os/add-ons/*/))
override LDFLAGS += -B$(SYSROOT)/develop/lib -L$(SYSROOT)/develop/lib \
	-L$(SYSROOT)/lib -Wl,-rpath-link=$(SYSROOT)/lib
INPUT_SERVER := $(SYSROOT)/servers/input_server
endif

# whisper.cpp for the keyboard's dictation (scripts/build-whisper.sh)
WHISPER := build/whisper-$(ARCH)
WHISPER_INCLUDES := -I$(ROOT)/third_party/whisper.cpp/include \
	-I$(ROOT)/third_party/whisper.cpp/ggml/include
WHISPER_LIBS := $(WHISPER)/src/libwhisper.a $(WHISPER)/ggml/src/libggml.a \
	$(WHISPER)/ggml/src/libggml-cpu.a $(WHISPER)/ggml/src/libggml-base.a

SHELL_SOURCES := $(wildcard shell/src/*.cpp)
SHELL_OBJECTS := $(patsubst shell/src/%.cpp,$(OUT)/shell/%.o,$(SHELL_SOURCES))

all: $(OUT)/MobileShell $(OUT)/renku_virtual_keyboard $(OUT)/renku_touch

$(OUT)/shell/%.o: shell/src/%.cpp shell/src/*.h vkbd/VirtualKeyboard.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(WHISPER_INCLUDES) -c $< -o $@

$(OUT)/shell/VoiceInput.o: $(WHISPER)/src/libwhisper.a

$(WHISPER)/src/libwhisper.a:
	sh scripts/build-whisper.sh

$(OUT)/MobileShell: $(SHELL_OBJECTS) $(WHISPER)/src/libwhisper.a
	$(CXX) $(LDFLAGS) -o $@ $(SHELL_OBJECTS) $(WHISPER_LIBS) -lbe -lbnetapi \
		-lnetwork -lmedia

$(OUT)/renku_virtual_keyboard: vkbd/VirtualKeyboardDevice.cpp vkbd/VirtualKeyboard.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -shared $(LDFLAGS) -o $@ $< -lbe $(INPUT_SERVER)

$(OUT)/renku_touch: touch/TouchFilter.cpp vkbd/VirtualKeyboard.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -shared $(LDFLAGS) -o $@ $< -lbe $(INPUT_SERVER)

package: all
	sh packaging/build-hpkg.sh $(ARCH) $(VERSION)

test:
	@mkdir -p build/host
	c++ -O2 -std=c++17 -o build/host/swipe-test tests/swipe-test.cpp \
		shell/src/SwipeDecoder.cpp
	build/host/swipe-test
	sh tests/run.sh

clean:
	rm -rf build/$(ARCH)/shell $(OUT)/MobileShell $(OUT)/renku_virtual_keyboard \
		$(OUT)/renku_touch

.PHONY: all package test clean
