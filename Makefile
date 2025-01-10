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

# Top level declarations
.PHONY: help
help: help_all

.PHONY: detect
detect: detect_all

.PHONY: all
all: build_all

.PHONY: check
check: check_all

.PHONY: test
test: test_all

.PHONY: archive
archive: archive_all

.PHONY: clean
clean: clean_all

# OR_ROOT is the minimal prefix to define the root of or-tools, if we
# are compiling in the or-tools root, it is empty. Otherwise, it is
# $(OR_TOOLS_TOP)/ or $(OR_TOOLS_TOP)\\ depending on the platform. It
# contains the trailing separator if not empty.
#
# OR_ROOT_FULL is always the complete path to or-tools. It is useful
# to store path informations inside libraries for instance.
ifeq ($(OR_TOOLS_TOP),)
  OR_ROOT =
else
  ifeq ($(OS), Windows_NT)
    OR_ROOT = $(OR_TOOLS_TOP)\\
  else
    OR_ROOT = $(OR_TOOLS_TOP)/
  endif
endif

# Delete all implicit rules to speed up makefile
.SUFFIXES:
# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

# Keep all intermediate files
# ToDo: try to remove it later
.SECONDARY:

# Read version.
ifeq ($(OR_TOOLS_MAJOR)$(OR_TOOLS_MINOR),)
  include $(OR_ROOT)Version.txt
endif

# We try to detect the platform, and load system specific macros.
include $(OR_ROOT)makefiles/Makefile.port.mk
OR_ROOT_FULL=$(OR_TOOLS_TOP)

# Check SOURCE argument
SOURCE_SUFFIX = $(suffix $(SOURCE))
# will contain “/any/path/foo.cc” on unix and “\\any\\path\\foo.cc” on windows
SOURCE_PATH = $(subst /,$S,$(SOURCE))
SOURCE_NAME= $(basename $(notdir $(SOURCE)))
ifeq ($(SOURCE),) # Those rules will be used if SOURCE is empty
.PHONY: build run
build run:
	$(error no source file provided, the "$@" target must be used like so: \
 make $@ SOURCE=relative/path/to/filename.extension)
else
ifeq (,$(wildcard $(SOURCE)))
$(error File "$(SOURCE)" does not exist !)
endif
endif

# Include .mk files.
BUILT_LANGUAGES = C++
ifeq ($(BUILD_PYTHON),ON)
BUILT_LANGUAGES +=, Python$(PYTHON_VERSION)
endif
ifeq ($(BUILD_JAVA),ON)
BUILT_LANGUAGES +=, Java
endif
ifeq ($(BUILD_DOTNET),ON)
BUILT_LANGUAGES +=, .Net (6.0)
endif

include $(OR_ROOT)makefiles/Makefile.cpp.mk
include $(OR_ROOT)makefiles/Makefile.dotnet.mk
include $(OR_ROOT)makefiles/Makefile.java.mk
include $(OR_ROOT)makefiles/Makefile.python.mk
include $(OR_ROOT)makefiles/Makefile.archive.mk
ifneq ($(PLATFORM),WIN64)
include $(OR_ROOT)makefiles/Makefile.doc.mk
else
# Remove some rules on windows
help_doc:

endif

.PHONY: help_usage
help_usage:
	@echo Use one of the following targets:
	@echo help, help_all:	Print this help.
	@echo all:	Build OR-Tools for all available languages.
	@echo check, check_all:	Check OR-Tools for all available languages.
	@echo test, test_all:	Test OR-Tools for all available languages.
	@echo clean, clean_all:	Clean output from previous build for all available languages \(won\'t clean third party\).
	@echo detect, detect_all:	Show variables used to build OR-Tools for all available languages.
ifeq ($(PLATFORM),WIN64)
	@echo off & echo(
else
	@echo
endif

.PHONY: help_all
help_all: help_usage help_cpp help_dotnet help_java help_python help_archive help_doc

.PHONY: check_all
check_all: check_cpp check_dotnet check_java check_python
	@echo Or-tools has been built and checked for "$(BUILT_LANGUAGES)"

# Commands to build/clean all languages.
.PHONY: compile
compile:
	cmake --build dependencies --target install --config $(BUILD_TYPE) -j $(JOBS) -v

.PHONY: build_all
build_all:
	$(MAKE) third_party BUILD_DOTNET=ON BUILD_JAVA=ON BUILD_PYTHON=ON
	cmake --build $(BUILD_DIR) --target install --config $(BUILD_TYPE) -j $(JOBS) -v
	@echo Or-tools has been built for "$(BUILT_LANGUAGES)"

.PHONY: test_all
test_all: test_cpp test_dotnet test_java test_python
	@echo Or-tools have been built and tested for "$(BUILT_LANGUAGES)"

.PHONY: archive_all
archive_all: archive_cpp archive_dotnet archive_java archive_python archive_data
	@echo Or-tools has been built and archived for "$(BUILT_LANGUAGES)"

.PHONY: test_archive_all
test_archive_all: test_archive_cpp test_archive_dotnet test_archive_java test_archive_python
	@echo Or-tools archives have been checked for "$(BUILT_LANGUAGES)"

.PHONY: clean_all
clean_all: clean_cpp clean_dotnet clean_java clean_python clean_archive
	@echo Or-Tools has been cleaned for "$(BUILT_LANGUAGES)"

.PHONY: detect_all
detect_all: detect_port detect_cpp detect_dotnet detect_java detect_python detect_archive
	@echo SOURCE = $(SOURCE)
	@echo SOURCE_PATH = $(SOURCE_PATH)
	@echo SOURCE_NAME = $(SOURCE_NAME)
	@echo SOURCE_SUFFIX = $(SOURCE_SUFFIX)
ifeq ($(PLATFORM),WIN64)
	@echo off & echo(
else
	@echo
endif

print-%  : ; @echo $* = \'$($*)\'

.PHONY: FORCE
FORCE:
