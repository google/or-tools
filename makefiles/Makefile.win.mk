#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard. In that case, please tell us -----
#  ----- about it. -----

# Windows specific definitions
LIB_PREFIX =
PRE_LIB = $(OR_ROOT)lib\\
POST_LIB = .lib
LIB_SUFFIX = lib
SWIG_PYTHON_LIB_SUFFIX = dll
SWIG_DOTNET_LIB_SUFFIX = dll
JNI_LIB_EXT = dll
STATIC_PRE_LIB = $(OR_ROOT)lib\\
STATIC_POST_LIB = .lib
STATIC_LIB_SUFFIX = lib
LINK_CMD = lib
STATIC_LINK_CMD = lib

LIB_DIR = $(OR_ROOT)lib
BIN_DIR = $(OR_ROOT)bin
GEN_DIR = $(OR_ROOT)ortools/gen
GEN_PATH = $(subst /,$S,$(GEN_DIR))
OBJ_DIR = $(OR_ROOT)objs
SRC_DIR = $(OR_ROOT).
EX_DIR  = $(OR_ROOT)examples
INC_DIR = $(OR_ROOT).

O=obj
E=.exe
L=lib
DLL=.dll
PDB=.pdb
EXP=.exp
ARCHIVE_EXT = .zip
FZ_EXE = fzn-or-tools$E
OBJ_OUT = /Fo
EXE_OUT = /Fe
LD_OUT = /OUT:
DYNAMIC_LD = link /DLL /LTCG /debug
S = \\
CMDSEP=&
CPSEP=;

COPY = copy
COPYREC = xcopy
DEL = del
DELREC = tools\win\rm.exe -rf
GREP = tools\win\grep.exe
MKDIR = md
MKDIR_P = tools\win\mkdir.exe -p
RENAME = rename
SED = tools\win\sed.exe
TAR = tools\win\tar.exe
TOUCH = tools\win\touch.exe
UNZIP = tools\win\unzip.exe
ZIP = tools\win\zip.exe
WGET = tools\win\wget.exe
WHICH = tools\win\which.exe

# We Can't force SHELL to cmd.exe if sh.exe is in the PATH
# cf https://www.gnu.org/software/make/manual/html_node/Choosing-the-Shell.html
SHCHECK := $(shell where sh.exe 2>NUL)
ifneq ($(SHCHECK),)
$(error Please remove sh.exe ($(SHCHECK)) from your PATH (e.g. set PATH=%PATH:C:\Program Files\Git\bin\;=%))
else
SHELL = cmd
endif

CMAKE := $(shell $(WHICH) cmake)
ifeq ($(CMAKE),)
$(error Please add "cmake" to your PATH)
endif

# Add some additional macros
ATTRIB = attrib
TASKKILL = taskkill

# Compilation macros.
DEBUG=/O2 -DNDEBUG
ifeq ("$(VISUAL_STUDIO_YEAR)","2015")
CCC=cl /EHsc /MD /nologo /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
else
CCC=cl /EHsc /MD /nologo
endif

ZLIB_INC = /I$(WINDOWS_ZLIB_DIR)\\include
GFLAGS_INC = /I$(WINDOWS_GFLAGS_DIR)\\include /DGFLAGS_DLL_DECL= /DGFLAGS_DLL_DECLARE_FLAG= /DGFLAGS_DLL_DEFINE_FLAG=
GLOG_INC = /I$(WINDOWS_GLOG_DIR)\\include /DGOOGLE_GLOG_DLL_DECL=
PROTOBUF_INC = /I$(WINDOWS_PROTOBUF_DIR)\\include
PROTOBUF_PROTOC_INC = -I$(WINDOWS_PROTOBUF_DIR)\\include

PYTHON_VERSION = $(WINDOWS_PYTHON_VERSION)
PYTHON_INC=/I$(WINDOWS_PATH_TO_PYTHON)\\include
PYTHON_LNK="$(WINDOWS_PATH_TO_PYTHON)\\libs\\python$(PYTHON_VERSION).lib"

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
# This is needed to find CPLEX include files and libraries.
ifdef WINDOWS_CPLEX_DIR
  CPLEX_INC = /I$(WINDOWS_CPLEX_DIR)\\include /DUSE_CPLEX
  CPLEX_SWIG = -DUSE_CPLEX
  STATIC_CPLEX_LNK = $(WINDOWS_CPLEX_DIR)\\cplex.lib
  DYNAMIC_CPLEX_LNK = $(STATIC_CPLEX_LNK)
endif
# This is needed to find Gurobi include files and libraries.
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

SWIG_INC = $(GLPK_SWIG) $(CLP_SWIG) $(CBC_SWIG) $(SCIP_SWIG) $(CPLEX_SWIG) $(GUROBI_SWIG) -DUSE_GLOP -DUSE_BOP

JAVA_INC=/I"$(JAVA_HOME)\\include" /I"$(JAVA_HOME)\\include\\win32"
JAVAC_BIN="$(shell $(WHICH) "$(JAVA_HOME)\bin\javac")"
JAVA_BIN="$(shell $(WHICH) "$(JAVA_HOME)\bin\java")"
JAR_BIN="$(shell $(WHICH) "$(JAVA_HOME)\bin\jar")"

CFLAGS = -nologo $(SYSCFLAGS) $(DEBUG) /I$(INC_DIR) /I$(GEN_DIR) \
 $(GFLAGS_INC) $(GLOG_INC) $(ZLIB_INC) $(MINISAT_INC) $(PROTOBUF_INC) \
 $(CBC_INC) $(CLP_INC) \
 $(GLPK_INC) $(SCIP_INC) $(CPLEX_INC) $(GUROBI_INC) \
 /DUSE_GLOP /DUSE_BOP /D__WIN32__ /DPSAPI_VERSION=1 /DNOMINMAX
JNIFLAGS=$(CFLAGS) $(JAVA_INC)
DYNAMIC_GFLAGS_LNK = $(WINDOWS_GFLAGS_DIR)\\lib\\gflags_static.lib
STATIC_GFLAGS_LNK = $(WINDOWS_GFLAGS_DIR)\\lib\\gflags_static.lib
ZLIB_LNK = $(WINDOWS_ZLIB_DIR)\\lib\\$(WINDOWS_ZLIB_NAME)
DYNAMIC_PROTOBUF_LNK = $(WINDOWS_PROTOBUF_DIR)\\lib\\libprotobuf.lib
STATIC_PROTOBUF_LNK = $(WINDOWS_PROTOBUF_DIR)\\lib\\libprotobuf.lib
DYNAMIC_GLOG_LNK = $(WINDOWS_PROTOBUF_DIR)\\lib\\glog.lib
STATIC_GLOG_LNK = $(WINDOWS_PROTOBUF_DIR)\\lib\\glog.lib
SYS_LNK=psapi.lib ws2_32.lib shlwapi.lib
DEPENDENCIES_LNK = $(STATIC_CBC_LNK) $(STATIC_CLP_LNK) $(STATIC_GLPK_LNK) $(STATIC_SCIP_LNK) $(STATIC_CPLEX_LNK) $(STATIC_GUROBI_LNK) $(STATIC_GLOG_LNK) $(STATIC_GFLAGS_LNK) $(STATIC_PROTOBUF_LNK)
LDFLAGS = $(ZLIB_LNK) $(SYS_LNK)

OR_TOOLS_LDFLAGS = $(ZLIB_LNK) $(SYS_LNK)
COMMA := ,
BACK_SLASH := \\
