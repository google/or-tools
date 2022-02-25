#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard. In that case, please tell us -----
#  ----- about it. -----

# Unix specific definitions
LIB_PREFIX = lib
DEP_BIN_DIR = $(OR_ROOT)bin
# C++ relevant directory
BUILD_DIR = $(OR_ROOT)dependencies
INC_DIR = $(OR_ROOT)include
SRC_DIR = $(OR_ROOT).
GEN_DIR = $(OR_ROOT)ortools/gen
GEN_PATH = $(subst /,$S,$(GEN_DIR))
OBJ_DIR = $(OR_ROOT)objs
LIB_DIR = $(OR_ROOT)lib
BIN_DIR = $(OR_ROOT)bin
FZ_EX_DIR  = $(OR_ROOT)examples/flatzinc
FZ_EX_PATH = $(subst /,$S,$(FZ_EX_DIR))
# C++ relevant directory
CC_EX_DIR  = $(OR_ROOT)examples/cpp
CC_GEN_DIR  = $(GEN_DIR)/examples/cpp
CC_EX_PATH = $(subst /,$S,$(CC_EX_DIR))
CC_GEN_PATH = $(subst /,$S,$(CC_GEN_DIR))
# Python relevant directory
PYTHON_EX_DIR  = $(OR_ROOT)examples/python
PYTHON_EX_PATH = $(subst /,$S,$(PYTHON_EX_DIR))
# Java relevant directory
CLASS_DIR = $(OR_ROOT)classes
JAVA_EX_DIR  = $(OR_ROOT)examples/java
JAVA_EX_PATH = $(subst /,$S,$(JAVA_EX_DIR))
# .Net relevant directory
DOTNET_EX_DIR  = $(OR_ROOT)examples/dotnet
DOTNET_EX_PATH = $(subst /,$S,$(DOTNET_EX_DIR))
# Contrib examples directory
CONTRIB_EX_DIR = $(OR_ROOT)examples/contrib
CONTRIB_EX_PATH = $(subst /,$S,$(CONTRIB_EX_DIR))
# Test examples directory
TEST_DIR  = $(OR_ROOT)examples/tests
TEST_PATH = $(subst /,$S,$(TEST_DIR))

O = o
ifeq ($(PLATFORM),LINUX)
L = so
else # MACOS
L = dylib
endif
E =
J = .jar
D = .dll
PDB = .pdb
EXP = .exp
ARCHIVE_EXT = .tar.gz
FZ_EXE = fzn-or-tools$E
LD_OUT = -o # need the space.
OBJ_OUT = -o # need the space
EXE_OUT = -o # need the space
S = /
CMDSEP = ;
CPSEP = :

COPY = cp
COPYREC = cp -R
DEL = rm -f
DELREC = rm -rf
GREP = grep
MKDIR = mkdir
MKDIR_P = mkdir -p
RENAME = mv
SED = sed
TAR = tar
TOUCH = touch
WHICH = which

CMAKE := $(shell $(WHICH) cmake)
ifeq ($(CMAKE),)
$(error Please add "cmake" to your PATH)
endif

ifdef UNIX_CPLEX_DIR
  CPLEX_INC = -I$(UNIX_CPLEX_DIR)/cplex/include -DUSE_CPLEX
endif
ifdef UNIX_XPRESS_DIR
  XPRESS_INC = -I$(UNIX_XPRESS_DIR)/include -DUSE_XPRESS -DXPRESS_PATH="$(UNIX_XPRESS_DIR)"
endif

# Compilation flags
DEBUG = -O4 -DNDEBUG
JNIDEBUG = -O1 -DNDEBUG

ifeq ($(PLATFORM),LINUX)
  CCC = g++ -fPIC -std=c++17 -fwrapv
  DYNAMIC_LD = g++ -shared
  DYNAMIC_LDFLAGS = -Wl,-rpath,\"\\\$$\$$ORIGIN\"

  # This is needed to find libz.a
  ZLIB_LNK = -lz
  ifdef UNIX_GLPK_DIR
  GLPK_LNK = $(UNIX_GLPK_DIR)/lib/libglpk.a
  endif
  ifdef UNIX_CPLEX_DIR
    CPLEX_LNK = \
    -L$(UNIX_CPLEX_DIR)/cplex/lib/x86-64_linux/static_pic -lcplex \
    -lm -lpthread -ldl
  endif
  ifdef UNIX_XPRESS_DIR
    XPRESS_LNK = -L$(UNIX_XPRESS_DIR)/lib -lxprs -lxprl
  endif

  SYS_LNK = -lrt -lpthread -Wl,--no-as-needed -ldl
  JNI_LIB_EXT = so

  LINK_CMD = $(DYNAMIC_LD)
  PRE_LIB = -L$(OR_ROOT_FULL)/lib -l
  POST_LIB =
  LINK_FLAGS = \
 -Wl,-rpath,'$$ORIGIN' \
 -Wl,-rpath,'$$ORIGIN/../lib'

endif  # ifeq ($(PLATFORM),LINUX)
ifeq ($(PLATFORM),MACOSX)
  MAC_VERSION = -mmacosx-version-min=$(MAC_MIN_VERSION)
  CCC = clang++ -fPIC -std=c++17  $(MAC_VERSION) -stdlib=libc++
  DYNAMIC_LD = clang++ -dynamiclib -undefined dynamic_lookup \
  $(MAC_VERSION) \
 -Wl,-search_paths_first \
 -Wl,-headerpad_max_install_names \
 -current_version $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR) \
 -compatibility_version $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR)
  DYNAMIC_LDFLAGS = -Wl,-rpath,\"@loader_path\"

  ZLIB_LNK = -lz
  ifdef UNIX_GLPK_DIR
    GLPK_LNK = $(UNIX_GLPK_DIR)/lib/libglpk.a
  endif
  ifdef UNIX_CPLEX_DIR
    CPLEX_LNK = \
 -force_load $(UNIX_CPLEX_DIR)/cplex/lib/x86-64_osx/static_pic/libcplex.a \
 -lm -lpthread -framework CoreFoundation -framework IOKit
  endif
  ifdef UNIX_XPRESS_DIR
    XPRESS_LNK = -Wl,-rpath,$(UNIX_XPRESS_DIR)/lib -L$(UNIX_XPRESS_DIR)/lib -lxprs -lxprl
  endif
  SYS_LNK =
  SET_COMPILER = CXX="$(CCC)"
  SET_COIN_OPT = OPT_CXXFLAGS="-O1 -DNDEBUG" OPT_CFLAGS="-O1 -DNDEBUG"
  JNI_LIB_EXT = dylib

  LINK_CMD = clang++ -dynamiclib $(MAC_VERSION) \
 -Wl,-search_paths_first \
 -Wl,-headerpad_max_install_names \
 -current_version $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR) \
 -compatibility_version $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR)
  PRE_LIB = -L$(OR_ROOT)lib -l
  POST_LIB =
  LINK_FLAGS = \
 -framework CoreFoundation \
 -Wl,-rpath,@loader_path \
 -Wl,-rpath,@loader_path/../lib \
 -Wl,-rpath,@loader_path/../dependencies/install/lib
  LDFLAGS = -install_name @rpath/$(LIB_PREFIX)ortools.$L #
endif # ifeq ($(PLATFORM),MACOSX)

DEPENDENCIES_INC = -I$(INC_DIR) -I$(SRC_DIR) -I$(GEN_DIR) \
 -Wno-deprecated -DUSE_GLOP -DUSE_BOP \
 $(GLPK_INC) $(CPLEX_INC) $(XPRESS_INC)

CFLAGS = $(DEBUG) $(DEPENDENCIES_INC) -DOR_TOOLS_MAJOR=$(OR_TOOLS_MAJOR) -DOR_TOOLS_MINOR=$(OR_TOOLS_MINOR) -DOR_TOOLS_PATCH=$(GIT_REVISION)
JNIFLAGS = $(JNIDEBUG) $(DEPENDENCIES_INC)
LDFLAGS += $(ZLIB_LNK) $(SYS_LNK) $(LINK_FLAGS)
DEPENDENCIES_LNK = $(GLPK_LNK) $(CPLEX_LNK) $(XPRESS_LNK)

OR_TOOLS_LNK = $(PRE_LIB)ortools$(POST_LIB)
OR_TOOLS_LDFLAGS = $(ZLIB_LNK) $(SYS_LNK) $(LINK_FLAGS)

# language targets

BUILD_PYTHON ?= ON
ifeq ($(PYTHON_VERSION),)
BUILD_PYTHON = OFF
endif

BUILD_JAVA ?= ON
JAVAC_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/javac)
JAVA_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/java)
JAR_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/jar)
MVN_BIN := $(shell $(WHICH) mvn)
ifndef JAVAC_BIN
BUILD_JAVA = OFF
endif
ifndef JAR_BIN
BUILD_JAVA = OFF
endif
ifndef JAVA_BIN
BUILD_JAVA = OFF
endif
ifndef MVN_BIN
BUILD_JAVA = OFF
endif

BUILD_DOTNET ?= ON
DOTNET_BIN := $(shell $(WHICH) dotnet 2> /dev/null)
ifndef DOTNET_BIN
BUILD_DOTNET=OFF
endif

.PHONY detect_languages:
detect_languages:
	@echo BUILD_PYTHON = $(BUILD_PYTHON)
	@echo BUILD_JAVA = $(BUILD_JAVA)
	@echo BUILD_DOTNET = $(BUILD_DOTNET)
