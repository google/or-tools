#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard. In that case, please tell us -----
#  ----- about it. -----

# Unix specific definitions
LIB_PREFIX = lib
DEP_BIN_DIR = $(OR_ROOT)dependencies/install/bin
# C++ relevant directory
INC_DIR = $(OR_ROOT).
SRC_DIR = $(OR_ROOT).
GEN_DIR = $(OR_ROOT)ortools/gen
GEN_PATH = $(subst /,$S,$(GEN_DIR))
OBJ_DIR = $(OR_ROOT)objs
LIB_DIR = $(OR_ROOT)lib
BIN_DIR = $(OR_ROOT)bin
TEST_DIR  = $(OR_ROOT)examples/tests
TEST_PATH = $(subst /,$S,$(TEST_DIR))
CC_EX_DIR  = $(OR_ROOT)examples/cpp
CC_EX_PATH = $(subst /,$S,$(CC_EX_DIR))
FZ_EX_DIR  = $(OR_ROOT)examples/flatzinc
FZ_EX_PATH = $(subst /,$S,$(FZ_EX_DIR))
# Python relevant directory
PYTHON_EX_DIR  = $(OR_ROOT)examples/python
PYTHON_EX_PATH = $(subst /,$S,$(PYTHON_EX_DIR))
# Java relevant directory
CLASS_DIR = $(OR_ROOT)classes
JAVA_EX_DIR  = $(OR_ROOT)examples/java
JAVA_EX_PATH = $(subst /,$S,$(JAVA_EX_DIR))
# .Net relevant directory
PACKAGE_DIR = $(OR_ROOT)packages
DOTNET_EX_DIR  = $(OR_ROOT)examples/dotnet
DOTNET_EX_PATH = $(subst /,$S,$(DOTNET_EX_DIR))
# Contrib examples directory
CONTRIB_EX_DIR = $(OR_ROOT)examples/contrib
CONTRIB_EX_PATH = $(subst /,$S,$(CONTRIB_EX_DIR))

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
MVN_BIN := $(shell $(WHICH) mvn)

# This is needed to find python.h
PYTHON_VERSION = $(UNIX_PYTHON_VER)
MAJOR_PYTHON_VERSION = $(shell python$(UNIX_PYTHON_VER) -c "from sys import version_info as v; print (str(v[0]))")
MINOR_PYTHON_VERSION = $(shell python$(UNIX_PYTHON_VER) -c "from sys import version_info as v; print (str(v[1]))")

PATH_TO_PYTHON_LIB = $(shell python$(UNIX_PYTHON_VER) -c 'import sysconfig; print (sysconfig.get_paths()["stdlib"])')
PATH_TO_PYTHON_INCLUDE = $(shell python$(UNIX_PYTHON_VER) -c 'import sysconfig; print (sysconfig.get_paths()["platinclude"])')
PYTHON_INC = -I$(PATH_TO_PYTHON_INCLUDE) -I$(PATH_TO_PYTHON_LIB) $(ADD_PYTHON_INC)

PYTHON_INC += $(shell pkg-config --cflags python$(MAJOR_PYTHON_VERSION) 2> /dev/null)
#PYTHON_LNK += $(shell pkg-config --libs   python$(MAJOR_PYTHON_VERSION) 2> /dev/null)

#ifeq ("${PYTHON_LNK}","")
#PYTHON_LNK = "-lpython${UNIX_PYTHON_VERSION}"
#endif

# This is needed to find GLPK include files.
ifdef UNIX_GLPK_DIR
  GLPK_INC = -I$(UNIX_GLPK_DIR)/include -DUSE_GLPK
  GLPK_SWIG = $(GLPK_INC)
endif
ifdef UNIX_CPLEX_DIR
  CPLEX_INC = -I$(UNIX_CPLEX_DIR)/cplex/include -DUSE_CPLEX
  CPLEX_SWIG = $(CPLEX_INC)
endif
ifdef UNIX_XPRESS_DIR
  XPRESS_INC = -I$(UNIX_XPRESS_DIR)/include -DUSE_XPRESS -DXPRESS_PATH="$(UNIX_XPRESS_DIR)"
  XPRESS_SWIG = $(XPRESS_INC)
endif

ifeq ($(PLATFORM),LINUX)
  SWIG_INC = -DSWIGWORDSIZE64
else
  SWIG_INC =
endif
SWIG_INC += \
 -DUSE_GLOP -DUSE_BOP -DABSL_MUST_USE_RESULT \
 $(GLPK_SWIG) $(CPLEX_SWIG) $(XPRESS_INC)

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
  JAVA_INC = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
  JAVAC_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/javac)
  JAVA_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/java)
  JAR_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/jar)
  JNI_LIB_EXT = so

  SWIG_PYTHON_LIB_SUFFIX = so
  SWIG_DOTNET_LIB_SUFFIX = so
  LINK_CMD = $(DYNAMIC_LD)
  PRE_LIB = -L$(OR_ROOT_FULL)/lib -l
  POST_LIB =
  LINK_FLAGS = \
 -Wl,-rpath,'$$ORIGIN' \
 -Wl,-rpath,'$$ORIGIN/../lib' \
 -Wl,-rpath,'$$ORIGIN/../dependencies/install/lib64' \
 -Wl,-rpath,'$$ORIGIN/../dependencies/install/lib'
  PYTHON_LDFLAGS = \
 -Wl,-rpath,'$$ORIGIN' \
 -Wl,-rpath,'$$ORIGIN/../../ortools' \
 -Wl,-rpath,'$$ORIGIN/../../ortools/.libs' \
 -Wl,-rpath,'$$ORIGIN/../../../../lib' \
 -Wl,-rpath,'$$ORIGIN/../../../../dependencies/install/lib64' \
 -Wl,-rpath,'$$ORIGIN/../../../../dependencies/install/lib'
endif  # ifeq ($(PLATFORM),LINUX)
ifeq ($(PLATFORM),MACOSX)
  MAC_VERSION = -mmacosx-version-min=$(MAC_MIN_VERSION)
  CCC = clang++ -fPIC -std=c++17  $(MAC_VERSION) -stdlib=libc++
  DYNAMIC_LD = clang++ -dynamiclib -undefined dynamic_lookup \
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
  JAVA_INC = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/darwin
  JAVAC_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/javac)
  JAVA_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/java)
  JAR_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/jar)
  JNI_LIB_EXT = jnilib

  SWIG_PYTHON_LIB_SUFFIX = so# To overcome a bug in Mac OS X loader.
  SWIG_DOTNET_LIB_SUFFIX = dylib
  LINK_CMD = clang++ -dynamiclib \
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
  PYTHON_LDFLAGS = \
 -Wl,-rpath,@loader_path \
 -Wl,-rpath,@loader_path/../../ortools \
 -Wl,-rpath,@loader_path/../../ortools/.libs \
 -Wl,-rpath,@loader_path/../../../../lib \
 -Wl,-rpath,@loader_path/../../../../dependencies/install/lib
endif # ifeq ($(PLATFORM),MACOSX)

DEPENDENCIES_INC = -I$(INC_DIR) -I$(GEN_DIR) \
 -Wno-deprecated -DUSE_GLOP -DUSE_BOP \
 $(GLPK_INC) $(CPLEX_INC) $(XPRESS_INC)

CFLAGS = $(DEBUG) $(DEPENDENCIES_INC) -DOR_TOOLS_MAJOR=$(OR_TOOLS_MAJOR) -DOR_TOOLS_MINOR=$(OR_TOOLS_MINOR)
JNIFLAGS = $(JNIDEBUG) $(DEPENDENCIES_INC)
LDFLAGS += $(ZLIB_LNK) $(SYS_LNK) $(LINK_FLAGS)
DEPENDENCIES_LNK = $(GLPK_LNK) $(CPLEX_LNK) $(XPRESS_LNK)

OR_TOOLS_LNK = $(PRE_LIB)ortools$(POST_LIB)
OR_TOOLS_LDFLAGS = $(ZLIB_LNK) $(SYS_LNK) $(LINK_FLAGS)
