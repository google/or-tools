# Top level declarations
.PHONY: help
help: help_all

.PHONY: all
all: build_all

.PHONY: check
check: check_all

.PHONY: test
test: test_all

.PHONY: clean
clean: clean_all

.PHONY: detect
detect: detect_all

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
include $(OR_ROOT)Version.txt

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
include $(OR_ROOT)makefiles/Makefile.python.mk
include $(OR_ROOT)makefiles/Makefile.java.mk
include $(OR_ROOT)makefiles/Makefile.dotnet.mk
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
help_all: help_usage help_cc help_python help_java help_dotnet help_archive help_doc

# Commands to build/clean all languages.
.PHONY: compile
compile:
	cmake --build dependencies --target install --config $(BUILD_TYPE) -j $(JOBS) -v

.PHONY: build_all
build_all:
	$(MAKE) third_party BUILD_PYTHON=ON BUILD_JAVA=ON BUILD_DOTNET=ON
	cmake --build $(BUILD_DIR) --target install --config $(BUILD_TYPE) -j $(JOBS) -v
	@echo Or-tools has been built for "$(BUILT_LANGUAGES)"

.PHONY: check_all
check_all: check_cpp check_python check_java check_dotnet
	@echo Or-tools has been built and checked for "$(BUILT_LANGUAGES)"

.PHONY: test_all
test_all: test_cpp test_python test_java test_dotnet
	@echo Or-tools have been built and tested for "$(BUILT_LANGUAGES)"

.PHONY: clean_all
clean_all: clean_cpp clean_python clean_java clean_dotnet clean_archive
	@echo Or-Tools has been cleaned for "$(BUILT_LANGUAGES)"

.PHONY: detect_all
detect_all: detect_port detect_cpp detect_python detect_java detect_dotnet detect_archive
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
