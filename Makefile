# PowerA PC HQ for macOS - Makefile

SHELL := /usr/bin/env bash

# Detect Homebrew prefix (Apple Silicon: /opt/homebrew, Intel: /usr/local)
ifeq ($(shell test -d /opt/homebrew && echo 1),1)
    BREW_PREFIX ?= /opt/homebrew
else ifeq ($(shell test -d /usr/local && echo 1),1)
    BREW_PREFIX ?= /usr/local
else
    BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null || echo "/opt/homebrew")
endif

CC ?= clang
CXX ?= clang++
CFLAGS ?= -O3 -Wall -I./driver -I$(BREW_PREFIX)/include -I$(BREW_PREFIX)/include/hidapi
CXXFLAGS ?= -std=c++17 -O3 -Wall -I$(BREW_PREFIX)/include -I$(BREW_PREFIX)/include/hidapi
LDFLAGS ?= -L$(BREW_PREFIX)/lib -lhidapi -framework IOKit -framework CoreFoundation

.PHONY: all app dmg install restart tools clean test help

all: app

app:
	@echo "Building PowerA PC HQ application bundle..."
	@./scripts/build_bundle.sh

dmg:
	@echo "Building standalone macOS DMG installer..."
	@./scripts/build_dmg.sh

install:
	@echo "Installing PowerA PC HQ application and driver subsystem..."
	@./scripts/install_driver.sh

restart:
	@echo "Restarting background driver subsystem..."
	@./scripts/restart_driver.sh

tools:
	@echo "Compiling CLI diagnostics and mapping tools..."
	@mkdir -p bin
	$(CC) $(CFLAGS) $(LDFLAGS) tools/powera_active_listener.c -o bin/powera_active_listener
	$(CC) $(CFLAGS) $(LDFLAGS) tools/interactive_tester.c -o bin/interactive_tester
	$(CC) $(CFLAGS) $(LDFLAGS) tools/diag_live.c -o bin/diag_live
	$(CC) $(CFLAGS) $(LDFLAGS) tools/rumble_protocol_isolator.c -o bin/rumble_isolator
	@echo "Tools successfully compiled to bin/"

clean:
	@echo "Cleaning build and distribution artifacts..."
	@rm -rf build dist bin *.o *.log *.err
	@echo "Clean completed."

test:
	@echo "Testing build prerequisites and header availability..."
	@test -d $(BREW_PREFIX) || (echo "Warning: Homebrew not found at $(BREW_PREFIX)" && exit 1)
	@test -f $(BREW_PREFIX)/include/hidapi/hidapi.h -o -f $(BREW_PREFIX)/include/hidapi.h || (echo "Error: hidapi not found. Run 'brew install hidapi'" && exit 1)
	@echo "All build prerequisites met."

help:
	@echo "PowerA PC HQ for macOS - Available Targets:"
	@echo "  make (or make all) - Build native driver, tools, and .app bundle"
	@echo "  make app           - Build 'PowerA PC HQ.app' bundle"
	@echo "  make dmg           - Build standalone drag-and-drop DMG installer"
	@echo "  make tools         - Compile CLI hardware analysis & test utilities"
	@echo "  make install       - Install application and driver launchd service"
	@echo "  make restart       - Restart the background driver daemon"
	@echo "  make test          - Verify build environment and prerequisites"
	@echo "  make clean         - Remove build/, dist/, and bin/ directories"
