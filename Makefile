# Top level declarations
.PHONY: help
help: help_all

.PHONY: all
all: build_all

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
# INC_DIR is like OR_ROOT, but with a default of '.' instead of
# empty.  It is used for instance in include directives (-I.).
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

# We try to detect the platform.
include $(OR_ROOT)makefiles/Makefile.port.mk
OR_ROOT_FULL=$(OR_TOOLS_TOP)

# Load local variables
include $(OR_ROOT)Makefile.local

# Change the file extensions to increase diff tool friendliness.
# Then include specific system commands and definitions
include $(OR_ROOT)makefiles/Makefile.$(SYSTEM).mk

# Rules to fetch and build third party dependencies.
include $(OR_ROOT)makefiles/Makefile.third_party.$(SYSTEM).mk

# Include .mk files.
include $(OR_ROOT)makefiles/Makefile.cpp.mk
include $(OR_ROOT)makefiles/Makefile.python.mk
include $(OR_ROOT)makefiles/Makefile.java.mk
include $(OR_ROOT)makefiles/Makefile.dotnet.mk
# include $(OR_ROOT)makefiles/Makefile.csharp.mk
# include $(OR_ROOT)makefiles/Makefile.fsharp.mk
include $(OR_ROOT)makefiles/Makefile.archive.mk
include $(OR_ROOT)makefiles/Makefile.install.mk

# Include test
include $(OR_ROOT)makefiles/Makefile.test.mk

# Finally include user makefile if it exists
-include $(OR_ROOT)Makefile.user

.PHONY: help_usage
help_usage:
	@echo Use one of the following targets:
	@echo help, help_all:	Print this help.
	@echo all:	Build OR-Tools for all available languages \(need a call to \"make third_party\" first\).
	@echo test, test_all:	Test OR-Tools for all available languages.
	@echo clean, clean_all:	Clean output from previous build for all available languages \(won\'t clean third party\).
	@echo detect, detect_all:	Show variables used to build OR-Tools for all available languages.
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

.PHONY: help_all
help_all: help_usage help_third_party help_cc help_python help_java help_dotnet help_archive

.PHONY: build_all
build_all: cc python java dotnet
	@echo Or-tools have been built for $(BUILT_LANGUAGES)

.PHONY: test_all
test_all: test_cc test_python test_java test_dotnet
	@echo Or-tools have been built and tested for $(BUILT_LANGUAGES)

.PHONY: clean_all
clean_all: clean_cc clean_python clean_java clean_dotnet clean_compat clean_archive
	-$(DELREC) $(BIN_DIR)
	-$(DELREC) $(LIB_DIR)
	-$(DELREC) $(OBJ_DIR)
	-$(DELREC) $(GEN_PATH)
	@echo Or-tools have been cleaned for $(BUILT_LANGUAGES)

.PHONY: detect_all
detect_all: detect_port detect_third_party detect_cc detect_python detect_java detect_dotnet detect_archive

print-%  : ; @echo $* = $($*)
