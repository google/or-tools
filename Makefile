# Top level declarations
help:
	@echo Please define target:
	@echo "  - constraint programming: cplibs cpexe pycp javacp"
	@echo "  - mathematical programming: lplibs lpexe pylp javalp"
	@echo "  - algorithms: algorithmslibs pyalgorithms javaalgorithms"
	@echo "  - graph: graphlibs pygraph javagraph"
	@echo "  - .NET on windows: csharp csharpcp csharplp csharpalgorithms csharpgraph csharpexe"
	@echo "  - misc: clean cleancsharp"

.PHONY : python cc java
cc: cplibs cpexe algorithmslibs graphlibs lplibs lpexe
java: javacp javaalgorithms javagraph javalp
python: pycp pyalgorithms pygraph pylp
all: cc java python

# First, we try to detect the platform.
include makefiles/Makefile.port

# We include predefined variables
include makefiles/Makefile.def

# Then we overwrite the local ones if the Makefile.local file exists.
-include Makefile.local

# Then include specific system commands and definitions
include makefiles/Makefile.$(SYSTEM)

# Rules to fetch and build third party dependencies.
include makefiles/Makefile.third_party.$(SYSTEM)

# Include .mk files.
include makefiles/Makefile.cpp.mk
include makefiles/Makefile.python.mk
include makefiles/Makefile.java.mk
ifeq ("$(SYSTEM)","win")
include makefiles/Makefile.csharp.mk
endif

# Include test
include makefiles/Makefile.test.$(SYSTEM)

# Finally include user makefile if it exists
-include Makefile.user
