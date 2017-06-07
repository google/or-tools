# Top level declarations
help:
	@echo Please define target:
	@echo "  - C++: cc test_cc clean_cc"
	@echo "  - Python: python test_python clean_python"
	@echo "  - Java: java test_java clean_java"
	@echo "  - .NET: csharp test_csharp clean_csharp "
	@echo "  - all: all test clean"

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

.PHONY : python cc java csharp sat third_party_check
all: third_party_check cc java python csharp
	@echo Or-tools have been built for $(BUILT_LANGUAGES)
clean: clean_cc clean_java clean_python clean_csharp clean_compat

# Read version.
include $(OR_ROOT)Version.txt

# We try to detect the platform.
include $(OR_ROOT)makefiles/Makefile.port
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
include $(OR_ROOT)makefiles/Makefile.csharp.mk
include $(OR_ROOT)makefiles/Makefile.archive.mk

# Include test
include $(OR_ROOT)makefiles/Makefile.test

# Finally include user makefile if it exists
-include $(OR_ROOT)Makefile.user

#check if "make third_party" have been run or not
third_party_check:
ifeq ($(wildcard dependencies/install/include/gflags/gflags.h),)
	@echo "One of the third party files was not found! did you run 'make third_party'?" && exit 1
endif

print-%  : ; @echo $* = $($*)
