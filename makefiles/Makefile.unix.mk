#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard. In that case, please tell us -----
#  ----- about it. -----

# Unix specific definitions
PROTOBUF_DIR = $(UNIX_PROTOBUF_DIR)
SWIG_BINARY = $(UNIX_SWIG_BINARY)
MKDIR = mkdir
MKDIR_P = mkdir -p
COPY = cp
TOUCH = touch
LIB_PREFIX = lib
ifeq ($(PLATFORM),MACOSX)
# Keep the path in the lib as it is stored upon construction.
LIB_DIR = $(OR_ROOT_FULL)/lib
else # No need to keep the path in the lib, it is not stored there on linux.
LIB_DIR = $(OR_ROOT)lib
endif
BIN_DIR = $(OR_ROOT)bin
GEN_DIR = $(OR_ROOT)src/gen
OBJ_DIR = $(OR_ROOT)objs
SRC_DIR = $(OR_ROOT)src
EX_DIR  = $(OR_ROOT)examples
INC_DIR = $(OR_ROOT)src
DEP_BIN_DIR = $(OR_ROOT)dependencies/install/bin


O = o
E =
L=.a
DLL=.dll
PDB=.pdb
EXP=.exp
LDOUT = -o # need the space.
OBJ_OUT = -o #
EXE_OUT = -o #
DEL = rm -f
DELREC = rm -rf
RENAME = mv
S = /
CPSEP = :
SED = sed
ARCHIVE_EXT = .tar.gz
FZ_EXE = fzn-or-tools$E

# This is needed to find python.h
PYTHON_VERSION = $(UNIX_PYTHON_VER)

PATH_TO_PYTHON_LIB = $(shell python$(UNIX_PYTHON_VER) -c 'import sysconfig; print (sysconfig.get_paths()["stdlib"])')
PATH_TO_PYTHON_INCLUDE = $(shell python$(UNIX_PYTHON_VER) -c 'import sysconfig; print (sysconfig.get_paths()["platinclude"])')

PYTHON_INC = -I$(PATH_TO_PYTHON_INCLUDE) -I$(PATH_TO_PYTHON_LIB) $(ADD_PYTHON_INC)

ifeq ($(PLATFORM),LINUX)
MAJOR_PYTHON_VERSION = $(shell python$(UNIX_PYTHON_VER) -c "from sys import version_info as v; print (str(v[0]))")
PYTHON_INC += $(shell pkg-config --cflags --libs python$(MAJOR_PYTHON_VERSION) 2> /dev/null)
endif

# This is needed to find gflags/gflags.h
GFLAGS_INC = -I$(UNIX_GFLAGS_DIR)/include
# This is needed to find protocol buffers.
PROTOBUF_INC = -I$(UNIX_PROTOBUF_DIR)/include
# This is needed to find sparse hash containers.
SPARSEHASH_INC = -I$(UNIX_SPARSEHASH_DIR)/include

# Define UNIX_CLP_DIR if unset and if UNIX_CBC_DIR is set.
ifdef UNIX_CBC_DIR
  ifndef UNIX_CLP_DIR
    UNIX_CLP_DIR = $(UNIX_CBC_DIR)
  endif
endif
# This is needed to find Coin LP include files.
ifdef UNIX_CLP_DIR
  CLP_INC = -I$(UNIX_CLP_DIR)/include -DUSE_CLP
  CLP_SWIG = $(CLP_INC)
endif
# This is needed to find Coin Branch and Cut include files.
ifdef UNIX_CBC_DIR
  CBC_INC = -I$(UNIX_CBC_DIR)/include -DUSE_CBC
  CBC_SWIG = $(CBC_INC)
endif
# This is needed to find GLPK include files.
ifdef UNIX_GLPK_DIR
  GLPK_INC = -I$(UNIX_GLPK_DIR)/include -DUSE_GLPK
  GLPK_SWIG = $(GLPK_INC)
endif
# This is needed to find scip include files.
ifdef UNIX_SCIP_DIR
  SCIP_INC = -I$(UNIX_SCIP_DIR)/src -DUSE_SCIP
  SCIP_SWIG = $(SCIP_INC)
endif
# This is needed to find SULUM include files.
ifdef UNIX_SLM_DIR
  SLM_INC = -I$(UNIX_SLM_DIR)/header -DUSE_SLM
  SLM_SWIG = $(SLM_INC)
endif
ifdef UNIX_GUROBI_DIR
  GUROBI_INC = -I$(UNIX_GUROBI_DIR)/$(GUROBI_PLATFORM)/include -DUSE_GUROBI
  GUROBI_SWIG = $(GUROBI_INC)
endif
ifdef UNIX_CPLEX_DIR
  CPLEX_INC = -I$(UNIX_CPLEX_DIR)/cplex/include -DUSE_CPLEX
  CPLEX_SWIG = $(CPLEX_INC)
endif

SWIG_INC = $(GLPK_SWIG) $(CLP_SWIG) $(CBC_SWIG) $(SCIP_SWIG) $(SLM_SWIG) $(GUROBI_SWIG) $(CPLEX_SWIG) -DUSE_GLOP -DUSE_BOP

# Compilation flags
DEBUG = -O4 -DNDEBUG
JNIDEBUG = -O1 -DNDEBUG

# Check wether CBC/CLP need a coin subdir in library.
ifdef UNIX_CBC_DIR
  ifneq ($(wildcard $(UNIX_CBC_DIR)/lib/coin),)
    UNIX_CBC_COIN = /coin
  endif
endif
ifdef UNIX_CLP_DIR
  ifneq ($(wildcard $(UNIX_CLP_DIR)/lib/coin),)
    UNIX_CLP_COIN = /coin
  endif
endif

ifeq ($(PLATFORM),LINUX)
  CCC = g++ -fPIC -std=c++0x -fwrapv
  DYNAMIC_LD = g++ -shared
  CMAKE = cmake
  ifeq ($(PTRLENGTH),64)
    ARCH = -DARCH_K8
  else
    ARCH  =
  endif
  CSC = $(PATH_TO_CSHARP_COMPILER)
  MONO = LD_LIBRARY_PATH=$(LIB_DIR):$(LD_LIBRARY_PATH) mono

  # This is needed to find libgflags.a
  GFLAGS_LNK = $(UNIX_GFLAGS_DIR)/lib/libgflags.a
  # This is needed to find libz.a
  ZLIB_LNK = -lz
  # This is needed to find libprotobuf.a
  PROTOBUF_LNK = $(UNIX_PROTOBUF_DIR)/lib/libprotobuf.a
  ifdef UNIX_GLPK_DIR
  GLPK_LNK = $(UNIX_GLPK_DIR)/lib/libglpk.a
  endif
  ifdef UNIX_CLP_DIR
  CLP_LNK = $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libClp.a $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libCoinUtils.a
  endif
  ifdef UNIX_CBC_DIR
  CBC_LNK = $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libCbcSolver.a $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libCbc.a $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libCgl.a $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libOsi.a $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libOsiCbc.a $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libOsiClp.a
  endif
  ifdef UNIX_SCIP_DIR
    ifeq ($(PTRLENGTH),64)
      SCIP_ARCH = linux.x86_64.gnu.opt
    else
      SCIP_ARCH = linux.x86.gnu.opt
    endif
    SCIP_LNK = $(UNIX_SCIP_DIR)/lib/libscip.$(SCIP_ARCH).a $(UNIX_SCIP_DIR)/lib/libnlpi.cppad.$(SCIP_ARCH).a $(UNIX_SCIP_DIR)/lib/liblpispx.$(SCIP_ARCH).a $(UNIX_SCIP_DIR)/lib/libsoplex.$(SCIP_ARCH).a
  endif
  ifdef UNIX_SLM_DIR
    ifeq ($(PTRLENGTH),64)
      SLM_LNK = -Wl,-rpath $(UNIX_SLM_DIR)/linux64/bin/ -L$(UNIX_SLM_DIR)/linux64/bin/ -m64 -lc -ldl -lm -lpthread -lsulum$(UNIX_SULUM_VERSION)
    else
      SLM_LNK = -Wl,-rpath $(UNIX_SLM_DIR)/linux32/bin/ -L$(UNIX_SLM_DIR)/linux32/bin/ -m32 -lc -ldl -lm -lpthread -lsulum$(UNIX_SULUM_VERSION)
    endif
  endif
  ifdef UNIX_GUROBI_DIR
    ifeq ($(PTRLENGTH),64)
      GUROBI_LNK = -Wl,-rpath $(UNIX_GUROBI_DIR)/linux64/lib/ -L$(UNIX_GUROBI_DIR)/linux64/lib/ -m64 -lc -ldl -lm -lpthread -lgurobi$(GUROBI_LIB_VERSION)
    else
      GUROBI_LNK = -Wl,-rpath $(UNIX_GUROBI_DIR)/linux32/lib/ -L$(UNIX_GUROBI_DIR)/linux32/lib/ -m32 -lc -ldl -lm -lpthread -lgurobi$(GUROBI_LIB_VERSION)
    endif
  endif
  ifdef UNIX_CPLEX_DIR
    ifeq ($(PTRLENGTH),64)
      CPLEX_LNK = $(UNIX_CPLEX_DIR)/cplex/lib/x86-64_linux/static_pic/libcplex.a -lm -lpthread
    else
      CPLEX_LNK = $(UNIX_CPLEX_DIR)/cplex/lib/x86_linux/static_pic/libcplex.a -lm -lpthread
    endif
  endif
  SYS_LNK = -lrt -lpthread
  JAVA_INC = -I$(JDK_DIRECTORY)/include -I$(JDK_DIRECTORY)/include/linux
  JAVAC_BIN = $(JDK_DIRECTORY)/bin/javac
  JAVA_BIN = $(JDK_DIRECTORY)/bin/java
  JAR_BIN = $(JDK_DIRECTORY)/bin/jar
  JNI_LIB_EXT = so
  LIB_SUFFIX = so
  SWIG_LIB_SUFFIX = so
  LINK_CMD = g++ -shared
  LINK_PREFIX = -o # Need the space.
  PRE_LIB = -Wl,-rpath $(OR_ROOT_FULL)/lib -L$(OR_ROOT_FULL)/lib -l
  POST_LIB =
endif  # LINUX
ifeq ($(PLATFORM),MACOSX)
  MAC_VERSION = -mmacosx-version-min=$(MAC_MIN_VERSION)
  CCC = clang++ -fPIC -std=c++11  $(MAC_VERSION) -stdlib=libc++
  DYNAMIC_LD = ld -arch x86_64 -bundle -flat_namespace -undefined suppress -macosx_version_min $(MAC_MIN_VERSION) -lSystem -compatibility_version $(OR_TOOLS_SHORT_VERSION) -current_version $(OR_TOOLS_SHORT_VERSION)

  CMAKE := $(shell which cmake)
  ifeq ($(CMAKE),)
    CMAKE = /Applications/CMake.app/Contents/bin/cmake
  else
    $(warning "cmake found: $(CMAKE)")
  endif

  JNI_LIB_EXT = jnilib
  CSC = $(PATH_TO_CSHARP_COMPILER)
  MONO =  DYLD_FALLBACK_LIBRARY_PATH=$(LIB_DIR):$(DYLD_LIBRARY_PATH) mono64

  GFLAGS_LNK = $(UNIX_GFLAGS_DIR)/lib/libgflags.a
  ZLIB_LNK = -lz
  PROTOBUF_LNK = $(UNIX_PROTOBUF_DIR)/lib/libprotobuf.a
  ARCH = -DARCH_K8
  SYS_LNK =

  JAVA_INC = -I$(JDK_DIRECTORY) -I$(JDK_DIRECTORY)/darwin
  JAVAC_BIN = javac
  JAVA_BIN = java
  JAR_BIN = jar

  PRE_LIB = -L$(OR_ROOT)lib -l
  POST_LIB =
  LIB_SUFFIX = dylib
  SWIG_LIB_SUFFIX = so# To overcome a bug in Mac OS X loader.
  LINK_CMD = ld -arch x86_64 -dylib -flat_namespace -undefined suppress -macosx_version_min $(MAC_MIN_VERSION) -lSystem -compatibility_version $(OR_TOOLS_SHORT_VERSION) -current_version $(OR_TOOLS_SHORT_VERSION)
  LINK_PREFIX = -o # Space needed.

  ifdef UNIX_GLPK_DIR
    GLPK_LNK = $(UNIX_GLPK_DIR)/lib/libglpk.a
  endif
  ifdef UNIX_CLP_DIR
    CLP_LNK = $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libClp.a $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libCoinUtils.a
  endif
  ifdef UNIX_CBC_DIR
    CBC_LNK = $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libCbcSolver.a $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libCbc.a $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libCgl.a $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libOsi.a $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libOsiCbc.a $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libOsiClp.a
  endif
  ifdef UNIX_SCIP_DIR
    SCIP_ARCH = darwin.x86_64.gnu.opt
    SCIP_LNK = -force_load $(UNIX_SCIP_DIR)/lib/libscip.$(SCIP_ARCH).a $(UNIX_SCIP_DIR)/lib/libnlpi.cppad.$(SCIP_ARCH).a -force_load $(UNIX_SCIP_DIR)/lib/liblpispx.$(SCIP_ARCH).a -force_load $(UNIX_SCIP_DIR)/lib/libsoplex.$(SCIP_ARCH).a
  endif
  ifdef UNIX_GUROBI_DIR
    GUROBI_LNK = -L$(UNIX_GUROBI_DIR)/mac64/bin/ -m64 -lc -ldl -lm -lpthread -lgurobi$(GUROBI_LIB_VERSION)
  endif
  ifdef UNIX_CPLEX_DIR
    CPLEX_LNK = -force_load $(UNIX_CPLEX_DIR)/cplex/lib/x86-64_osx/static_pic/libcplex.a -lm -lpthread -framework CoreFoundation -framework IOKit
  endif
  ifdef UNIX_SLM_DIR
      SLM_LNK = -rpath $(UNIX_SLM_DIR)/mac64/bin/ -L$(UNIX_SLM_DIR)/mac64/bin/ -lc -ldl -lm -lpthread -lsulum$(UNIX_SULUM_VERSION)
  endif
endif  # MAC OS X

BISON = dependencies/install/bin/bison
FLEX = dependencies/install/bin/flex

CFLAGS = $(DEBUG) -I$(INC_DIR) -I$(EX_DIR) -I$(GEN_DIR) $(GFLAGS_INC) $(ARCH) \
  -Wno-deprecated $(PROTOBUF_INC) $(CBC_INC) $(CLP_INC) $(GLPK_INC) \
        $(SCIP_INC) $(SLM_INC) $(GUROBI_INC) $(CPLEX_INC) -DUSE_GLOP -DUSE_BOP $(SPARSEHASH_INC)

JNIFLAGS = $(JNIDEBUG) -I$(INC_DIR) -I$(EX_DIR) -I$(GEN_DIR) $(GFLAGS_INC) $(ARCH) \
        -Wno-deprecated $(PROTOBUF_INC) $(CBC_INC) $(CLP_INC) $(GLPK_INC) $(SCIP_INC) $(SLM_INC) $(GUROBI_INC) $(CPLEX_INC) -DUSE_GLOP -DUSE_BOP
DEPENDENCIES_LNK = $(GLPK_LNK) $(CBC_LNK) $(CLP_LNK) $(SCIP_LNK) $(LM_LNK) $(GUROBI_LNK) $(CPLEX_LNK) $(GFLAGS_LNK) $(PROTOBUF_LNK)
OR_TOOLS_LD_FLAGS = $(ZLIB_LNK) $(SYS_LNK)
