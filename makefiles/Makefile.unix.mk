#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard. In that case, please tell us -----
#  ----- about it. -----

LIB_PREFIX = lib
SRC_DIR = $(OR_ROOT).
EX_DIR  = $(OR_ROOT)examples
GEN_DIR = $(OR_ROOT)ortools/gen
GEN_PATH = $(subst /,$S,$(GEN_DIR))
OBJ_DIR = $(OR_ROOT)objs
LIB_DIR = $(OR_ROOT)lib
BIN_DIR = $(OR_ROOT)bin
INC_DIR = $(OR_ROOT).
DEP_BIN_DIR = $(OR_ROOT)dependencies/install/bin

O = o
E =
ifeq ($(PLATFORM),LINUX)
L = so
else # MACOS
L = dylib
endif
DLL=.dll
PDB=.pdb
EXP=.exp
ARCHIVE_EXT = .tar.gz
FZ_EXE = fzn-or-tools$E
LD_OUT = -o # need the space.
OBJ_OUT = -o # need the space
EXE_OUT = -o # need the space
S = /
CMDSEP=;
CPSEP = :

COPY = cp
COPYREC = cp -r
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

# This is needed to find python.h
PYTHON_VERSION = $(UNIX_PYTHON_VER)
MAJOR_PYTHON_VERSION = $(shell python$(UNIX_PYTHON_VER) -c "from sys import version_info as v; print (str(v[0]))")

PATH_TO_PYTHON_LIB = $(shell python$(UNIX_PYTHON_VER) -c 'import sysconfig; print (sysconfig.get_paths()["stdlib"])')
PATH_TO_PYTHON_INCLUDE = $(shell python$(UNIX_PYTHON_VER) -c 'import sysconfig; print (sysconfig.get_paths()["platinclude"])')
PYTHON_INC = -I$(PATH_TO_PYTHON_INCLUDE) -I$(PATH_TO_PYTHON_LIB) $(ADD_PYTHON_INC)

PYTHON_INC += $(shell pkg-config --cflags python$(MAJOR_PYTHON_VERSION) 2> /dev/null)
PYTHON_LNK += $(shell pkg-config --libs python$(MAJOR_PYTHON_VERSION) 2> /dev/null)

ifeq ("${PYTHON_LNK}","")
PYTHON_LNK = "-lpython${UNIX_PYTHON_VERSION}"
endif

MONO_COMPILER ?= mono
MONO_EXECUTABLE := $(shell $(WHICH) $(MONO_COMPILER))

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
ifdef UNIX_GUROBI_DIR
  GUROBI_INC = -I$(UNIX_GUROBI_DIR)/$(GUROBI_PLATFORM)/include -DUSE_GUROBI
  GUROBI_SWIG = $(GUROBI_INC)
endif
ifdef UNIX_CPLEX_DIR
  CPLEX_INC = -I$(UNIX_CPLEX_DIR)/cplex/include -DUSE_CPLEX
  CPLEX_SWIG = $(CPLEX_INC)
endif

SWIG_INC = \
 $(GFLAGS_SWIG) $(GLOG_SWIG) $(PROTOBUF_SWIG) $(COIN_SWIG) \
 -DUSE_GLOP -DUSE_BOP \
 $(GLPK_SWIG) $(SCIP_SWIG) $(GUROBI_SWIG) $(CPLEX_SWIG)

# Compilation flags
DEBUG = -O4 -DNDEBUG
JNIDEBUG = -O1 -DNDEBUG

ifeq ($(PLATFORM),LINUX)
  CCC = g++ -fPIC -std=c++11 -fwrapv
  DYNAMIC_LD = g++ -shared
  DYNAMIC_LDFLAGS = -Wl,-rpath,\"\\\$$\$$ORIGIN\"
  MONO = LD_LIBRARY_PATH=$(LIB_DIR):$(LD_LIBRARY_PATH) $(MONO_EXECUTABLE)

  # This is needed to find libz.a
  ZLIB_LNK = -lz
  ifdef UNIX_GLPK_DIR
  GLPK_LNK = $(UNIX_GLPK_DIR)/lib/libglpk.a
  endif
  ifdef UNIX_SCIP_DIR
    ifeq ($(PTRLENGTH),64)
      SCIP_ARCH = linux.x86_64.gnu.opt
    else
      SCIP_ARCH = linux.x86.gnu.opt
    endif
    SCIP_LNK = \
 $(UNIX_SCIP_DIR)/lib/static/libscip.$(SCIP_ARCH).a \
 $(UNIX_SCIP_DIR)/lib/static/libnlpi.cppad.$(SCIP_ARCH).a \
 $(UNIX_SCIP_DIR)/lib/static/liblpispx2.$(SCIP_ARCH).a \
 $(UNIX_SCIP_DIR)/lib/static/libsoplex.$(SCIP_ARCH).a \
 $(UNIX_SCIP_DIR)/lib/static/libtpitny.$(SCIP_ARCH).a
  endif
  ifdef UNIX_GUROBI_DIR
    ifeq ($(PTRLENGTH),64)
      GUROBI_LNK = \
 -Wl,-rpath $(UNIX_GUROBI_DIR)/linux64/lib/ \
 -L$(UNIX_GUROBI_DIR)/linux64/lib/ -m64 -lc -ldl -lm -lpthread \
 -lgurobi$(GUROBI_LIB_VERSION)
    else
      GUROBI_LNK = \
 -Wl,-rpath $(UNIX_GUROBI_DIR)/linux32/lib/ \
 -L$(UNIX_GUROBI_DIR)/linux32/lib/ -m32 -lc -ldl -lm -lpthread \
 -lgurobi$(GUROBI_LIB_VERSION)
    endif
  endif
  ifdef UNIX_CPLEX_DIR
    ifeq ($(PTRLENGTH),64)
      CPLEX_LNK = \
 $(UNIX_CPLEX_DIR)/cplex/lib/x86-64_linux/static_pic/libcplex.a \
 -lm -lpthread
    else
      CPLEX_LNK = \
 $(UNIX_CPLEX_DIR)/cplex/lib/x86_linux/static_pic/libcplex.a \
 -lm -lpthread
    endif
  endif
  SYS_LNK = -lrt -lpthread
  JAVA_INC = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
  JAVAC_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/javac)
  JAVA_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/java)
  JAR_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/jar)
  JNI_LIB_EXT = so

  SWIG_PYTHON_LIB_SUFFIX = so
  SWIG_DOTNET_LIB_SUFFIX = so
  LINK_CMD = $(DYNAMIC_LD)
  PRE_LIB = -L$(OR_ROOT_FULL)/lib -l
  #PRE_LIB = -Wl,-rpath $(OR_ROOT_FULL)/lib -L$(OR_ROOT_FULL)/lib -l
  POST_LIB =
  LINK_FLAGS = \
 -Wl,-rpath,"\$$ORIGIN" \
 -Wl,-rpath,"\$$ORIGIN/../lib" \
 -Wl,-rpath,"\$$ORIGIN/../dependencies/install/lib"
  PYTHON_LDFLAGS = \
 -Wl,-rpath,"\$$ORIGIN" \
 -Wl,-rpath,"\$$ORIGIN/../../ortools" \
 -Wl,-rpath,"\$$ORIGIN/../../../../lib" \
 -Wl,-rpath,"\$$ORIGIN/../../../../dependencies/install/lib"
endif  # ifeq ($(PLATFORM),LINUX)
ifeq ($(PLATFORM),MACOSX)
  MAC_VERSION = -mmacosx-version-min=$(MAC_MIN_VERSION)
  CCC = clang++ -fPIC -std=c++11  $(MAC_VERSION) -stdlib=libc++
  DYNAMIC_LD = clang++ -dynamiclib \
 -Wl,-search_paths_first \
 -Wl,-headerpad_max_install_names \
 -current_version $(OR_TOOLS_SHORT_VERSION) \
 -compatibility_version $(OR_TOOLS_SHORT_VERSION)
  DYNAMIC_LDFLAGS = -Wl,-rpath,\"@loader_path\"
  MONO =  DYLD_FALLBACK_LIBRARY_PATH=$(LIB_DIR):$(DYLD_LIBRARY_PATH) $(MONO_EXECUTABLE)

  ZLIB_LNK = -lz
  ifdef UNIX_GLPK_DIR
    GLPK_LNK = $(UNIX_GLPK_DIR)/lib/libglpk.a
  endif
  ifdef UNIX_SCIP_DIR
    SCIP_ARCH = darwin.x86_64.gnu.opt
    SCIP_LNK = \
 -force_load $(UNIX_SCIP_DIR)/lib/static/libscip.$(SCIP_ARCH).a \
 $(UNIX_SCIP_DIR)/lib/static/libnlpi.cppad.$(SCIP_ARCH).a \
 -force_load $(UNIX_SCIP_DIR)/lib/static/liblpispx2.$(SCIP_ARCH).a \
 -force_load $(UNIX_SCIP_DIR)/lib/static/libsoplex.$(SCIP_ARCH).a \
 -force_load $(UNIX_SCIP_DIR)/lib/static/libtpitny.$(SCIP_ARCH).a
  endif
  ifdef UNIX_GUROBI_DIR
    GUROBI_LNK = \
 -L$(UNIX_GUROBI_DIR)/mac64/bin/ -lc -ldl -lm -lpthread \
 -lgurobi$(GUROBI_LIB_VERSION)
  endif
  ifdef UNIX_CPLEX_DIR
    CPLEX_LNK = \
 -force_load $(UNIX_CPLEX_DIR)/cplex/lib/x86-64_osx/static_pic/libcplex.a \
 -lm -lpthread -framework CoreFoundation -framework IOKit
  endif
  SYS_LNK =
  SET_COMPILER = CXX="$(CCC)"
  JAVA_INC = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/darwin
  JAVAC_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/javac)
  JAVA_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/java)
  JAR_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/jar)
  JNI_LIB_EXT = jnilib

  SWIG_PYTHON_LIB_SUFFIX = so# To overcome a bug in Mac OS X loader.
  SWIG_DOTNET_LIB_SUFFIX = dll# To overcome a bug in Mac OS X loader.
  LINK_CMD = clang++ -dynamiclib \
 -Wl,-search_paths_first \
 -Wl,-headerpad_max_install_names \
 -current_version $(OR_TOOLS_SHORT_VERSION) \
 -compatibility_version $(OR_TOOLS_SHORT_VERSION)
  PRE_LIB = -L$(OR_ROOT)lib -l
  POST_LIB =
  LINK_FLAGS = \
 -Wl,-rpath,@loader_path \
 -Wl,-rpath,@loader_path/../lib \
 -Wl,-rpath,@loader_path/../dependencies/install/lib
  LDFLAGS = -install_name @rpath/$(LIB_PREFIX)ortools.$L #
  PYTHON_LDFLAGS = \
 -Wl,-rpath,@loader_path \
 -Wl,-rpath,@loader_path/../../ortools \
 -Wl,-rpath,@loader_path/../../../../lib \
 -Wl,-rpath,@loader_path/../../../../dependencies/install/lib
endif # ifeq ($(PLATFORM),MACOSX)

DEPENDENCIES_INC = -I$(INC_DIR) -I$(EX_DIR) -I$(GEN_DIR) \
 $(GFLAGS_INC) $(GLOG_INC) $(PROTOBUF_INC) \
 $(COIN_INC) \
 -Wno-deprecated -DUSE_GLOP -DUSE_BOP \
 $(GLPK_INC) $(SCIP_INC) $(GUROBI_INC) $(CPLEX_INC)

CFLAGS = $(DEBUG) $(DEPENDENCIES_INC)
JNIFLAGS = $(JNIDEBUG) $(DEPENDENCIES_INC)
LDFLAGS += $(ZLIB_LNK) $(SYS_LNK) $(LINK_FLAGS)
DEPENDENCIES_LNK = $(GLPK_LNK) $(SCIP_LNK) $(GUROBI_LNK) $(CPLEX_LNK)

OR_TOOLS_LNK =
OR_TOOLS_LDFLAGS = $(ZLIB_LNK) $(SYS_LNK) $(LINK_FLAGS)
