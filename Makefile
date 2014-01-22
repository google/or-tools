# Top level declarations
help:
	@echo Please define target:
	@echo "  - C++: cc test_cc clean_cc"
	@echo "  - Python: python test_python clean_python"
	@echo "  - Java: java test_java clean_java"
	@echo "  - .NET: csharp test_csharp clean_csharp "
	@echo "  - all: all test clean"

OR_TOOLS_VERSION = 1.0.0

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
  ifeq "$(SHELL)" "cmd.exe"
    OR_ROOT = $(OR_TOOLS_TOP)\\
  else
    ifeq "$(SHELL)" "sh.exe"
      OR_ROOT = $(OR_TOOLS_TOP)\\
    else
      OR_ROOT = $(OR_TOOLS_TOP)/
    endif
  endif
endif

.PHONY : python cc java csharp sat
all: cc java python csharp
clean: clean_cc clean_java clean_python clean_csharp clean_compat

# First, we try to detect the platform.
include $(OR_ROOT)makefiles/Makefile.port
OR_ROOT_FULL=$(OR_TOOLS_TOP)

# We include predefined variables
include $(OR_ROOT)makefiles/Makefile.def

# Then we overwrite the local ones if the Makefile.local file exists.
-include $(OR_ROOT)Makefile.local

# Then include specific system commands and definitions
include $(OR_ROOT)makefiles/Makefile.$(SYSTEM)

# Rules to fetch and build third party dependencies.
include $(OR_ROOT)makefiles/Makefile.third_party.$(SYSTEM)

# Include .mk files.
include $(OR_ROOT)makefiles/Makefile.cpp.mk
include $(OR_ROOT)makefiles/Makefile.python.mk
include $(OR_ROOT)makefiles/Makefile.java.mk
include $(OR_ROOT)makefiles/Makefile.csharp.mk
include $(OR_ROOT)makefiles/Makefile.doc.mk

# Include test
include $(OR_ROOT)makefiles/Makefile.test.$(SYSTEM)

# Finally include user makefile if it exists
-include $(OR_ROOT)Makefile.user
