# Let's discover something about where we run
ifeq "$(SHELL)" "cmd.exe"
SYSTEM=win
OS=windows
else
ifeq "$(SHELL)" "sh.exe"
SYSTEM=win
OS=windows
else
SYSTEM=unix
OS=$(shell uname -s)
endif
endif

# First, we include predefined variables
include Makefile.def

# Then we overwrite the local ones if the Makefile.local file exists.
-include Makefile.local

# Then include specific system commands and definitions
include Makefile.$(SYSTEM)

# Include build files.
include Makefile.build.cpp
include Makefile.build.python
include Makefile.build.java

# Finally include user makefile if it exists
-include Makefile.user
