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
  TOUCH := touch
  ifeq ($(OS),Linux)
  endif # ifeq($(OS),Linux)
  ifeq ($(OS),Darwin) # Assume Mac Os X
  endif # ifeq($(OS),Darwin)
  # Set UNIX_PYTHON_VER to the version number of the Python installation on your computer that you wish to use with or-tools.
  # Example : UNIX_PYTHON_VER=3.10
  UNIX_PYTHON_VER ?= 3
  PYTHON_BIN := $(shell command -v python$(UNIX_PYTHON_VER) 2>/dev/null)
  # Set this variable to use it as PYTHONPATH.
  UNIX_PYTHONPATH ?=
  ifneq ($(UNIX_PYTHONPATH),)
  SET_PYTHONPATH = @PYTHONPATH=$(UNIX_PYTHONPATH)
  endif
endif # SYSTEM == unix

# Windows specific part.
ifeq ($(SYSTEM),win)
  S := \\
  WHICH := bin$Swhich.exe
  TOUCH := bin$Stouch.exe
  # set this variable to the full path to your Python installation
  PYTHON_BIN := $(shell $(WHICH) python 2> NUL)
  # Set this variable to use it as PYTHONPATH
  WINDOWS_PYTHONPATH ?=
  ifneq ($(WINDOWS_PYTHONPATH),)
  SET_PYTHONPATH = @set PYTHONPATH=$(WINDOWS_PYTHONPATH) &&
  endif
endif # SYSTEM == win

.PHONY: all
all: detect

.PHONY: detect
detect:
	@echo VERSION = $(VERSION)
	@echo SYSTEM = $(SYSTEM)
	@echo OS = $(OS)
	@echo SHELL = $(SHELL)
	@echo PYTHON_BIN = $(PYTHON_BIN)
	@echo SOURCE = $(SOURCE)
	@echo SOURCE_PATH = $(SOURCE_PATH)
	@echo SOURCE_NAME = $(SOURCE_NAME)
	@echo SOURCE_SUFFIX = $(SOURCE_SUFFIX)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

python.log:
	"$(PYTHON_BIN)" -m pip install --user "$(firstword $(wildcard ortools*.whl))"
	@$(TOUCH) $@

##############
##  SOURCE  ##
##############
.PHONY: run # Run a Python program.

# Check SOURCE argument
SOURCE_SUFFIX := $(suffix $(SOURCE))
# will contain “/any/path/foo.cc” on unix and “\\any\\path\\foo.cc” on windows
SOURCE_PATH := $(subst /,$S,$(SOURCE))
SOURCE_NAME := $(basename $(notdir $(SOURCE)))
ifeq ($(SOURCE),) # Those rules will be used if SOURCE is empty

run:
	$(error no source file provided, the "$@" target must be used like so: \
 make $@ SOURCE=relative/path/to/filename.extension)

else #ifeq SOURCE

run: $(SOURCE) | python.log
	$(SET_PYTHONPATH) $(PYTHON_BIN) examples$S$(SOURCE_NAME).py $(ARGS)

endif #ifeq SOURCE

#############
##  Tests  ##
#############
.PHONY: test
test:
	$(MAKE) run SOURCE=examples/basic_example.py
	$(MAKE) run SOURCE=examples/tsp.py
	$(MAKE) run SOURCE=examples/vrp.py

############
##  MISC  ##
############
# Include user makefile
-include Makefile.user

print-% : ; @echo $* = $($*)
