# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

VERSION := @PROJECT_VERSION@
# Let's discover something about where we run
ifeq ($(OS),Windows_NT)
OS = Windows
endif
ifeq ($(OS),Windows)
SYSTEM = win
else
SYSTEM = unix
endif

.SECONDARY:

# Unix specific part.
ifeq ($(SYSTEM),unix)
  OS := $(shell uname -s)
  S := /
  ifeq ($(OS),Linux)
  endif # ifeq($(OS),Linux)
  ifeq ($(OS),Darwin) # Assume Mac Os X
  endif # ifeq($(OS),Darwin)
endif # SYSTEM == unix

# Windows specific part.
ifeq ($(SYSTEM),win)
  S := \\
endif # SYSTEM == win

BUILD_TYPE ?= Release

.PHONY: all
all: detect

.PHONY: detect
detect:
	@echo VERSION = $(VERSION)
	@echo SYSTEM = $(SYSTEM)
	@echo OS = $(OS)
	@echo SHELL = $(SHELL)
	@echo SOURCE = $(SOURCE)
	@echo SOURCE_PATH = $(SOURCE_PATH)
	@echo SOURCE_NAME = $(SOURCE_NAME)
	@echo SOURCE_SUFFIX = $(SOURCE_SUFFIX)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

##############
##  SOURCE  ##
##############
.PHONY: build # Build a C++ program.
.PHONY: run # Run a C++ program.

# Check SOURCE argument
SOURCE_SUFFIX := $(suffix $(SOURCE))
# will contain “/any/path/foo.cc” on unix and “\\any\\path\\foo.cc” on windows
SOURCE_PATH := $(subst /,$S,$(SOURCE))
SOURCE_NAME := $(basename $(notdir $(SOURCE)))
ifeq ($(SOURCE),) # Those rules will be used if SOURCE is empty

build run:
	$(error no source file provided, the "$@" target must be used like so: \
 make $@ SOURCE=relative/path/to/filename.extension)

else #ifeq SOURCE

ifeq (,$(wildcard $(SOURCE)))
$(error File "$(SOURCE)" does not exist !)
endif #ifeq SOURCE exist
build: $(SOURCE) examples/$(SOURCE_NAME)/CMakeLists.txt
	cd examples$S$(SOURCE_NAME) && \
 cmake \
 -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_PREFIX_PATH=$(CURDIR)
	cd examples$S$(SOURCE_NAME) && \
 cmake \
 --build build \
 --config $(BUILD_TYPE) \
 -v
run: build
ifeq ($(SYSTEM),win)
	cd examples$S$(SOURCE_NAME) && \
 cmake \
 --build build --config $(BUILD_TYPE) \
 --target RUN_TESTS -v
else #ifeq win
	cd examples$S$(SOURCE_NAME) && \
 cmake \
 --build build --config $(BUILD_TYPE) \
 --target test -v
endif #ifeq win

endif #ifeq SOURCE

#############
##  Tests  ##
#############
.PHONY: test
test:
	$(MAKE) run SOURCE=examples/basic_example/basic_example.cc
	$(MAKE) run SOURCE=examples/linear_programming/linear_programming.cc
	$(MAKE) run SOURCE=examples/integer_programming/integer_programming.cc
	$(MAKE) run SOURCE=examples/simple_knapsack_program/simple_knapsack_program.cc
	$(MAKE) run SOURCE=examples/simple_max_flow_program/simple_max_flow_program.cc
	$(MAKE) run SOURCE=examples/simple_min_cost_flow_program/simple_min_cost_flow_program.cc
	$(MAKE) run SOURCE=examples/simple_lp_program/simple_lp_program.cc
	$(MAKE) run SOURCE=examples/simple_mip_program/simple_mip_program.cc
	$(MAKE) run SOURCE=examples/simple_sat_program/simple_sat_program.cc
	$(MAKE) run SOURCE=examples/simple_ls_program/simple_ls_program.cc
	$(MAKE) run SOURCE=examples/tsp/tsp.cc
	$(MAKE) run SOURCE=examples/vrp/vrp.cc
	$(MAKE) run SOURCE=examples/nurses_cp/nurses_cp.cc
	$(MAKE) run SOURCE=examples/nurses_sat/nurses_sat.cc
	$(MAKE) run SOURCE=examples/minimal_jobshop_cp/minimal_jobshop_cp.cc

############
##  MISC  ##
############
# Include user makefile
-include Makefile.user

print-%	: ; @echo $* = $($*)
