# Let's discover something about where we run
ifeq "$(SHELL)" "cmd.exe"
  SYSTEM=win
else
  ifeq "$(SHELL)" "sh.exe"
    SYSTEM=win
  else
    SYSTEM=unix
  endif
endif

# First, we include predefined variables
include Makefile.def

# Then we overwrite the local ones if the Makefile.local file exists.
-include Makefile.local

# Then include specific system commands and definitions
include Makefile.$(SYSTEM)

# Include .mk files.
include Makefile.cpp.mk
include Makefile.python.mk
include Makefile.java.mk

# Finally include user makefile if it exists
-include Makefile.user
