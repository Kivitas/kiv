# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 kivitas
# Licensed under the GNU GPL v3 or later; see the LICENSE file.
#
# kiv build: Rust frontend, C++ runtime, and an x86-64 JIT on Linux/x86-64.
# Run `make help` for targets and variables.

# ---------------------------------------------------------------- user knobs
# MODE: release | debug | asan.  V=1: echo full commands.  WERROR=1: warnings are errors.
MODE    ?= release
V       ?= 0
WERROR  ?= 0
PREFIX  ?= /usr/local
DESTDIR ?=

CXX     ?= g++
CARGO   ?= cargo

# ------------------------------------------------------------ output control
ifeq ($(V),1)
  Q    :=
  ECHO := @:
else
  Q    := @
  ECHO := @echo
endif

# ------------------------------------------------------------ mode settings
# Each mode gets its own object directory, so switching modes never mixes
# optimized and debug objects.
ifeq ($(MODE),release)
  OPTFLAGS     := -O2 -DNDEBUG
  SANFLAGS     :=
  CARGO_PROFILE := release
else ifeq ($(MODE),debug)
  OPTFLAGS     := -O0 -g
  SANFLAGS     :=
  CARGO_PROFILE := debug
else ifeq ($(MODE),asan)
  OPTFLAGS     := -O1 -g -fno-omit-frame-pointer
  SANFLAGS     := -fsanitize=address,undefined
  CARGO_PROFILE := debug
else
  $(error unknown MODE '$(MODE)' (use release, debug or asan))
endif

BUILD  := build/$(MODE)
TARGET := $(BUILD)/kiv

# ------------------------------------------------------------ platform
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# ------------------------------------------------------------ C++ flags
CXXSTD   := -std=c++17
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast
ifeq ($(WERROR),1)
  WARNINGS += -Werror
endif
CXXFLAGS ?=
CXXFLAGS += $(OPTFLAGS) $(SANFLAGS) $(CXXSTD) $(WARNINGS)
CPPFLAGS += -Iinclude -Iruntime -Ijit
LDLIBS   := -lpthread -ldl -lm

# ------------------------------------------------------------ sources
RUNTIME_SRC := runtime/heap.cpp runtime/vm.cpp
MAIN_SRC    := src/main.cpp
# jit.cpp is always built; it selects the real JIT or a stub with #if.
JIT_SRC     := jit/jit.cpp
JIT_ASM     :=

# The JIT is x86-64 and uses mmap/mprotect, so only Linux/x86-64 enables it.
# Other platforms build the interpreter alone; --jit then reports it is unavailable.
ifeq ($(UNAME_S)/$(UNAME_M),Linux/x86_64)
  CPPFLAGS += -DKIV_ENABLE_JIT=1
  JIT_ASM  := jit/trampoline_x86_64.S
endif

CXX_SRCS := $(RUNTIME_SRC) $(MAIN_SRC) $(JIT_SRC)
OBJS     := $(patsubst %.cpp,$(BUILD)/%.o,$(CXX_SRCS)) \
            $(patsubst %.S,$(BUILD)/%.o,$(JIT_ASM))
DEPS     := $(patsubst %.cpp,$(BUILD)/%.d,$(CXX_SRCS))

RUST_DIR   := frontend
RUST_OUT   := $(RUST_DIR)/target/$(CARGO_PROFILE)
RUST_LIB   := $(RUST_OUT)/libkiv_frontend.a
RUST_SRCS  := $(shell find $(RUST_DIR)/src -name '*.rs' 2>/dev/null) $(RUST_DIR)/Cargo.toml $(RUST_DIR)/Cargo.lock
RUST_FLAGS := $(if $(filter release,$(CARGO_PROFILE)),--release,)

# ------------------------------------------------------------ targets
.DEFAULT_GOAL := all
.PHONY: all test test-rust test-examples install uninstall clean distclean help

all: $(TARGET)  ## Build the kiv binary (default)

$(TARGET): $(OBJS) $(RUST_LIB)
	$(ECHO) "  LD      $@"
	$(Q)$(CXX) $(CXXFLAGS) $(SANFLAGS) -o $@ $(OBJS) $(RUST_LIB) $(LDLIBS)

$(RUST_LIB): $(RUST_SRCS)
	$(ECHO) "  CARGO   frontend ($(CARGO_PROFILE))"
	@command -v $(CARGO) >/dev/null 2>&1 || { echo "error: '$(CARGO)' not found; install Rust from https://rustup.rs or set CARGO=/path/to/cargo" >&2; exit 1; }
	$(Q)cd $(RUST_DIR) && $(CARGO) build --locked $(RUST_FLAGS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(ECHO) "  CXX     $<"
	$(Q)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(ECHO) "  AS      $<"
	$(Q)$(CXX) $(CPPFLAGS) $(SANFLAGS) -c $< -o $@

-include $(DEPS)

test: test-rust test-examples  ## Run all tests (cargo tests + example scripts)

test-rust:  ## Run the Rust frontend unit tests
	$(ECHO) "  TEST    frontend"
	$(Q)cd $(RUST_DIR) && $(CARGO) test --locked

test-examples: $(TARGET)  ## Run the example scripts against the built binary
	$(Q)sh tests/run_examples.sh $(TARGET)

install: $(TARGET)  ## Install kiv into $(PREFIX)/bin (override with PREFIX= or DESTDIR=)
	@if [ "$(MODE)" != release ]; then echo "warning: installing a $(MODE) build" >&2; fi
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 0755 $(TARGET) "$(DESTDIR)$(PREFIX)/bin/kiv"

uninstall:  ## Remove the installed binary
	rm -f "$(DESTDIR)$(PREFIX)/bin/kiv"

clean:  ## Remove C++ build products for every mode
	rm -rf build

distclean: clean  ## Also remove the Rust build output
	cd $(RUST_DIR) && $(CARGO) clean

help:  ## Show targets and variables
	@echo "kiv build targets:"
	@grep -hE '^[a-zA-Z_-]+:.*## ' $(MAKEFILE_LIST) | sed -E 's/:[^#]*## /\t/' | awk -F'\t' '{printf "  %-15s %s\n", $$1, $$2}'
	@echo ""
	@echo "variables (current value in brackets):"
	@echo "  MODE=release|debug|asan  [$(MODE)]"
	@echo "  V=1                      echo full commands [$(V)]"
	@echo "  WERROR=1                 warnings are errors [$(WERROR)]"
	@echo "  CXX=, CARGO=, PREFIX=, DESTDIR=  [$(CXX), $(CARGO), $(PREFIX), $(DESTDIR)]"
