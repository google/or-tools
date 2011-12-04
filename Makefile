# Top level declarations
help:
	@echo Please define target:
	@echo "  - constraint programming: cplibs cpexe pycp javacp"
	@echo "  - mathematical programming: lplibs lpexe pylp javalp"
	@echo "  - algorithms: algorithmslibs pyalgorithms javaalgorithms"
	@echo "  - graph: graphlibs pygraph javagraph"
	@echo "  - misc: clean"

all: cplibs cpexe pycp javacp algorithmslibs pyalgorithms javaalgorithms graphlibs pygraph javagraph lplibs lpexe pylp javalp

# First, we try to detect the platform.
include Makefiles/Makefile.port

# We include predefined variables
include Makefiles/Makefile.def

# Then we overwrite the local ones if the Makefile.local file exists.
-include Makefile.local

# Then include specific system commands and definitions
include Makefiles/Makefile.$(SYSTEM)

# Rules to fetch and build third party dependencies.
include Makefiles/Makefile.third_party.$(SYSTEM)

# Include .mk files.
include Makefiles/Makefile.cpp.mk
include Makefiles/Makefile.python.mk
include Makefiles/Makefile.java.mk

# Finally include user makefile if it exists
-include Makefile.user
