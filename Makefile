# First, we include predefined variables
include Makefile.def

# Then we overwrite the local ones if the Makefile.local file exists.
-include Makefile.local

# Then include specific unix commands and definitions
include Makefile.unix

# Include build files.
include Makefile.build.cpp
include Makefile.build.python
include Makefile.build.java

# Finally include user makefile if it exists
-include Makefile.user
