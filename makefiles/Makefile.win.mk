# Windows specific definitions
PROTOBUF_DIR = $(WINDOWS_PROTOBUF_DIR)
SWIG_BINARY = $(WINDOWS_SWIG_BINARY)
LIB_DIR = $(OR_ROOT)lib
LIB_PREFIX =
STATIC_LIB_SUFFIX = lib
LIB_SUFFIX = lib
BIN_DIR = $(OR_ROOT)bin
GEN_DIR = $(OR_ROOT)ortools\\gen
OBJ_DIR = $(OR_ROOT)objs
SRC_DIR = $(OR_ROOT).
EX_DIR  = $(OR_ROOT)examples
INC_DIR = $(OR_ROOT).
LINK_CMD = lib
LINK_PREFIX = /OUT:
STATIC_LINK_CMD = lib
STATIC_LINK_PREFIX = /OUT:
PRE_LIB = $(OR_ROOT)lib\\
POST_LIB = .lib
STATIC_PRE_LIB = $(OR_ROOT)lib\\
STATIC_POST_LIB = .lib
O=obj
E=.exe
L=.lib
DLL=.dll
PDB=.pdb
EXP=.exp
OBJ_OUT = /Fo
EXE_OUT = /Fe
SWIG_LIB_SUFFIX = dll
JNI_LIB_EXT = dll
LDOUT = /OUT:
DYNAMIC_LD = link /DLL /LTCG /debug
S = \\
CPSEP =;
DEL = del
DELREC = tools\rm.exe -rf
RENAME = rename
MKDIR = md
MKDIR_P = tools\mkdir.exe -p
COPY = copy
TOUCH = tools\touch.exe
SED = tools\sed.exe
CMAKE = cmake
ARCHIVE_EXT = .zip
FZ_EXE = fzn-or-tools$E

# Add some additional macros
CD = cd
ATTRIB = attrib
TASKKILL = taskkill
NUGET = nuget.exe
NUGET_PACK = nuget.exe pack
NUGET_PUSH = nuget.exe push
NUGET_SRC = https://www.nuget.org/api/v2/package

# Default paths for libraries and binaries.
WINDOWS_ZLIB_DIR ?= $(OR_ROOT_FULL)\\dependencies\\install
WINDOWS_ZLIB_NAME ?= zlib.lib
WINDOWS_GFLAGS_DIR ?= $(OR_ROOT_FULL)\\dependencies\\install
WINDOWS_PROTOBUF_DIR ?= $(OR_ROOT_FULL)\\dependencies\\install
WINDOWS_SWIG_BINARY ?= $(OR_ROOT_FULL)\\dependencies\\install\\swigwin-$(SWIG_TAG)\\swig.exe
WINDOWS_CLP_DIR ?= $(OR_ROOT_FULL)\\dependencies\\install
WINDOWS_CBC_DIR ?= $(OR_ROOT_FULL)\\dependencies\\install

# Compilation macros.
DEBUG=/O2 -DNDEBUG
ifeq ("$(VISUAL_STUDIO_YEAR)","2015")
CCC=cl /EHsc /MD /nologo /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
else
CCC=cl /EHsc /MD /nologo
endif

GFLAGS_INC = /I$(WINDOWS_GFLAGS_DIR)\\include /DGFLAGS_DLL_DECL= /DGFLAGS_DLL_DECLARE_FLAG= /DGFLAGS_DLL_DEFINE_FLAG=
ZLIB_INC = /I$(WINDOWS_ZLIB_DIR)\\include
PROTOBUF_INC = /I$(WINDOWS_PROTOBUF_DIR)\\include
GLOG_INC = /I$(WINDOWS_GLOG_DIR)\\include /DGOOGLE_GLOG_DLL_DECL=

PYTHON_VERSION = $(WINDOWS_PYTHON_VERSION)
PYTHON_INC=/I$(WINDOWS_PATH_TO_PYTHON)\\include
PYTHON_LNK=$(WINDOWS_PATH_TO_PYTHON)\\libs\\python$(PYTHON_VERSION).lib

# Define CLP_DIR if unset and if CBC_DIR is set.
ifdef WINDOWS_CBC_DIR
ifndef WINDOWS_CLP_DIR
WINDOWS_CLP_DIR=$(WINDOWS_CBC_DIR)
endif
endif
# This is needed to find Coin LP include files and libraries.
ifdef WINDOWS_CLP_DIR
CLP_INC = /I$(WINDOWS_CLP_DIR)\\include /I$(WINDOWS_CLP_DIR)\\include\\coin /DUSE_CLP
CLP_SWIG = -DUSE_CLP
DYNAMIC_CLP_LNK = $(WINDOWS_CLP_DIR)\\lib\\coin\\libClp.lib  $(WINDOWS_CLP_DIR)\\lib\\coin\\libCoinUtils.lib
STATIC_CLP_LNK = $(WINDOWS_CLP_DIR)\\lib\\coin\\libClp.lib  $(WINDOWS_CLP_DIR)\\lib\\coin\\libCoinUtils.lib
endif
# This is needed to find Coin Branch and Cut include files and libraries.
ifdef WINDOWS_CBC_DIR
CBC_INC = /I$(WINDOWS_CBC_DIR)\\include /I$(WINDOWS_CBC_DIR)\\include\\coin /DUSE_CBC
CBC_SWIG = -DUSE_CBC
DYNAMIC_CBC_LNK = $(WINDOWS_CBC_DIR)\\lib\\coin\\libCbcSolver.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libCbc.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libCgl.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libOsi.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libOsiClp.lib
STATIC_CBC_LNK = $(WINDOWS_CBC_DIR)\\lib\\coin\\libCbcSolver.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libCbc.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libCgl.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libOsi.lib $(WINDOWS_CBC_DIR)\\lib\\coin\\libOsiClp.lib
endif
# This is needed to find GLPK include files and libraries.
ifdef WINDOWS_GLPK_DIR
GLPK_INC = /I$(WINDOWS_GLPK_DIR)\\include /DUSE_GLPK
GLPK_SWIG = -DUSE_GLPK
DYNAMIC_GLPK_LNK = $(WINDOWS_GLPK_DIR)\\lib\\glpk.lib
STATIC_GLPK_LNK = $(WINDOWS_GLPK_DIR)\\lib\\glpk.lib
endif
# This is needed to find SCIP include files and libraries.
ifdef WINDOWS_SCIP_DIR
  SCIP_INC = /I$(WINDOWS_SCIP_DIR)\\include /DUSE_SCIP
  SCIP_SWIG = -DUSE_SCIP
  STATIC_SCIP_LNK = $(WINDOWS_SCIP_DIR)\\libscipopt.lib
  DYNAMIC_SCIP_LNK = $(WINDOWS_SCIP_DIR)\\libscipopt.lib
endif
ifdef WINDOWS_GUROBI_DIR
  ifeq ($(PTRLENGTH),64)
    GUROBI_INC = /I$(WINDOWS_GUROBI_DIR)\win64\include /DUSE_GUROBI
    GUROBI_SWIG = -I$(WINDOWS_GUROBI_DIR)/win64/include -DUSE_GUROBI
    DYNAMIC_GUROBI_LNK = $(WINDOWS_GUROBI_DIR)\win64\lib\gurobi$(GUROBI_LIB_VERSION).lib
    STATIC_GUROBI_LNK = $(DYNAMIC_GUROBI_LNK)
  else
    GUROBI_INC = /I$(WINDOWS_GUROBI_DIR)\win32\include /DUSE_GUROBI
    GUROBI_SWIG = -I$(WINDOWS_GUROBI_DIR)/win32/include -DUSE_GUROBI
    DYNAMIC_GUROBI_LNK = $(WINDOWS_GUROBI_DIR)\\win32\lib\gurobi$(GUROBI_LIB_VERSION).lib
    STATIC_GUROBI_LNK = $(DYNAMIC_GUROBI_LNK)
  endif
endif

SWIG_INC = $(GLPK_SWIG) $(CLP_SWIG) $(CBC_SWIG) $(SCIP_SWIG) $(GUROBI_SWIG) -DUSE_GLOP -DUSE_BOP

JAVA_INC=/I"$(JDK_DIRECTORY)\\include" /I"$(JDK_DIRECTORY)\\include\\win32"
JAVAC_BIN="$(JDK_DIRECTORY)/bin/javac"
JAVA_BIN="$(JDK_DIRECTORY)/bin/java"
JAR_BIN="$(JDK_DIRECTORY)/bin/jar"

CFLAGS= -nologo $(SYSCFLAGS) $(DEBUG) /I$(INC_DIR) /I$(GEN_DIR) \
        $(GFLAGS_INC) $(ZLIB_INC) $(MINISAT_INC) $(PROTOBUF_INC) $(GLOG_INC) $(CBC_INC) \
        $(CLP_INC) $(GLPK_INC) $(SCIP_INC) $(GUROBI_INC) /DUSE_GLOP /DUSE_BOP \
        /D__WIN32__  $(SPARSEHASH_INC) /DPSAPI_VERSION=1 $(ARCH)
JNIFLAGS=$(CFLAGS) $(JAVA_INC)
DYNAMIC_GFLAGS_LNK = $(WINDOWS_GFLAGS_DIR)\\lib\\gflags_static.lib
STATIC_GFLAGS_LNK = $(WINDOWS_GFLAGS_DIR)\\lib\\gflags_static.lib
ZLIB_LNK = $(WINDOWS_ZLIB_DIR)\\lib\\$(WINDOWS_ZLIB_NAME)
DYNAMIC_PROTOBUF_LNK = $(PROTOBUF_DIR)\\lib\\libprotobuf.lib
STATIC_PROTOBUF_LNK = $(PROTOBUF_DIR)\\lib\\libprotobuf.lib
DYNAMIC_GLOG_LNK = $(PROTOBUF_DIR)\\lib\\glog.lib
STATIC_GLOG_LNK = $(PROTOBUF_DIR)\\lib\\glog.lib
SYS_LNK=psapi.lib ws2_32.lib shlwapi.lib
DEPENDENCIES_LNK = $(STATIC_CBC_LNK) $(STATIC_CLP_LNK) $(STATIC_GLPK_LNK) $(STATIC_SCIP_LNK) $(STATIC_GUROBI_LNK) $(STATIC_GLOG_LNK) $(STATIC_GFLAGS_LNK) $(STATIC_PROTOBUF_LNK)
OR_TOOLS_LD_FLAGS = $(ZLIB_LNK) $(SYS_LNK)

COMMA := ,
BACK_SLASH := \\
