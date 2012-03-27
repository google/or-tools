# Top level declarations
help:
	@echo Please define target:
	@echo "  - constraint programming: cplibs cpexe pycp javacp csharpcp csharpexe"
	@echo "  - mathematical programming: lplibs lpexe pylp javalp csharplp"
	@echo "  - algorithms: algorithmslibs pyalgorithms javaalgorithms csharpalgorithms"
	@echo "  - graph: graphlibs pygraph javagraph csharpgraph"
	@echo "  - tests: test test_cc test_python test_java test_csharp"
	@echo "  - cleaning: clean clean_csharp"

OR_TOOLS_VERSION = 1.0.0
ifeq ($(TOP),)
  MAKEFILE_DIR=makefiles
else
  MAKEFILE_DIR=$(TOP)/makefiles
endif

.PHONY : python cc java csharp
all: cc java python csharp
clean: clean_cc clean_java clean_python clean_csharp

# First, we try to detect the platform.
include $(MAKEFILE_DIR)/Makefile.port

# We include predefined variables
include $(MAKEFILE_DIR)/Makefile.def

# Then we overwrite the local ones if the Makefile.local file exists.
-include Makefile.local

# Then include specific system commands and definitions
include $(MAKEFILE_DIR)/Makefile.$(SYSTEM)

# Rules to fetch and build third party dependencies.
include $(MAKEFILE_DIR)/Makefile.third_party.$(SYSTEM)

# Include .mk files.
include $(MAKEFILE_DIR)/Makefile.cpp.mk
include $(MAKEFILE_DIR)/Makefile.python.mk
include $(MAKEFILE_DIR)/Makefile.java.mk
include $(MAKEFILE_DIR)/Makefile.csharp.mk

# Include test
include $(MAKEFILE_DIR)/Makefile.test.$(SYSTEM)

# Finally include user makefile if it exists
-include Makefile.user
