# Windows specific definitions
S = \\
PROTOBUF_DIR = $(WINDOWS_PROTOBUF_DIR)
SWIG_BINARY = $(WINDOWS_SWIG_BINARY)
LIB_DIR = $(OR_ROOT)lib
LIB_PREFIX =
STATIC_LIB_SUFFIX = lib
LIB_SUFFIX = lib
BIN_DIR = $(OR_ROOT)bin
GEN_DIR = $(OR_ROOT)src$Sgen
OBJ_DIR = $(OR_ROOT)objs
SRC_DIR = $(OR_ROOT)src
EX_DIR  = $(OR_ROOT)examples
INC_DIR = $(OR_ROOT)src
LINK_CMD = lib
LINK_PREFIX = /OUT:
STATIC_LINK_CMD = lib
STATIC_LINK_PREFIX = /OUT:
PRE_LIB = $(OR_ROOT)lib$S
POST_LIB = .lib
STATIC_PRE_LIB = $(OR_ROOT)lib$S
STATIC_POST_LIB = .lib
O=.obj
E=.exe
L=.lib
DLL=.dll
OBJ_OUT = /Fo
EXE_OUT = /Fe
SWIG_LIB_SUFFIX = dll
JNI_LIB_EXT = dll
LDOUT = /OUT:
DYNAMIC_LD = link /DLL /LTCG /debug
CPSEP =;
DEL = del
# Will maintain DELREC for now, but rename to something more descriptive like RM_RECURSE_FORCED
DELREC = tools\rm.exe -rf
RENAME = rename
MKDIR = md
MKDIR_P = tools\mkdir.exe -p
# TODO: TBD: consider using XCOPY instead; historically more efficient and with more elaborate options
COPY = copy
TOUCH = tools\touch.exe
SED = tools\sed.exe
BISON = dependencies\install\bin\win_bison.exe
FLEX = dependencies\install\bin\win_flex.exe
CMAKE = cmake
ARCHIVE_EXT = .zip
FZ_EXE = fzn-or-tools$E

# Add some additional macros
CD = cd
RMDIR = rmdir /q
RMDIR_S = rmdir /s /q
NMAKE = nmake
ATTRIB = attrib
TASKKILL = taskkill
GIT = git
SVN = svn
NUGET = nuget.exe
NUGET_PACK = nuget.exe pack
NUGET_PUSH = nuget.exe push
RM = tools\rm.exe
RM_RECURSE_FORCED = tools\rm.exe -fr
WGET = tools\wget
WGET_P = tools\get -P
UNZIP = tools\unzip
UNZIP_EXTRACT = tools\unzip -d
TOUCH = tools\touch.exe

# Add some option macros
CSCOUT = /out:
CSCLIB = /lib:

# Compilation macros.
DEBUG=/O2 -DNDEBUG
ifeq ("$(VISUAL_STUDIO_YEAR)","2015")
CCC=cl /EHsc /MD /nologo /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
else
CCC=cl /EHsc /MD /nologo
endif

GFLAGS_INC = /I$(WINDOWS_GFLAGS_DIR)$Sinclude /DGFLAGS_DLL_DECL= /DGFLAGS_DLL_DECLARE_FLAG= /DGFLAGS_DLL_DEFINE_FLAG=
ZLIB_INC = /I$(WINDOWS_ZLIB_DIR)$Sinclude
PROTOBUF_INC = /I$(WINDOWS_PROTOBUF_DIR)$Sinclude
SPARSEHASH_INC = /I$(WINDOWS_SPARSEHASH_DIR)$Sinclude

PYTHON_VERSION = $(WINDOWS_PYTHON_VERSION)
PYTHON_INC=/I$(WINDOWS_PATH_TO_PYTHON)$Sinclude
PYTHON_LNK=$(WINDOWS_PATH_TO_PYTHON)$Slibs$Spython$(PYTHON_VERSION).lib

# Define CLP_DIR if unset and if CBC_DIR is set.
ifdef WINDOWS_CBC_DIR
ifndef WINDOWS_CLP_DIR
WINDOWS_CLP_DIR=$(WINDOWS_CBC_DIR)
endif
endif
# This is needed to find Coin LP include files and libraries.
ifdef WINDOWS_CLP_DIR
CLP_INC = /I$(WINDOWS_CLP_DIR)$Sinclude /DUSE_CLP
CLP_SWIG = -DUSE_CLP
DYNAMIC_CLP_LNK = $(WINDOWS_CLP_DIR)$Slib$Scoin$SlibClp.lib  $(WINDOWS_CLP_DIR)$Slib$Scoin$SlibCoinUtils.lib
STATIC_CLP_LNK = $(WINDOWS_CLP_DIR)$Slib$Scoin$SlibClp.lib  $(WINDOWS_CLP_DIR)$Slib$Scoin$SlibCoinUtils.lib
endif
# This is needed to find Coin Branch and Cut include files and libraries.
ifdef WINDOWS_CBC_DIR
CBC_INC = /I$(WINDOWS_CBC_DIR)$Sinclude /DUSE_CBC
CBC_SWIG = -DUSE_CBC
DYNAMIC_CBC_LNK = $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibCbcSolver.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibCbc.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibCgl.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibOsi.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibOsiClp.lib
STATIC_CBC_LNK = $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibCbcSolver.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibCbc.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibCgl.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibOsi.lib $(WINDOWS_CBC_DIR)$Slib$Scoin$SlibOsiClp.lib
endif
# This is needed to find GLPK include files and libraries.
ifdef WINDOWS_GLPK_DIR
GLPK_INC = /I$(WINDOWS_GLPK_DIR)$Sinclude /DUSE_GLPK
GLPK_SWIG = -DUSE_GLPK
DYNAMIC_GLPK_LNK = $(WINDOWS_GLPK_DIR)$Slib$Sglpk.lib
STATIC_GLPK_LNK = $(WINDOWS_GLPK_DIR)$Slib$Sglpk.lib
endif
# This is needed to find SCIP include files and libraries.
ifdef WINDOWS_SCIP_DIR
  SCIP_LNK_DIR = $(OR_ROOT)dependencies$Sinstall
  SCIP_INC = /I$(WINDOWS_SCIP_DIR)$Sinclude$Sscip /DUSE_SCIP
  SCIP_SWIG = -DUSE_SCIP
  STATIC_SCIP_LNK = $(SCIP_LNK_DIR)$Slib$Sscip.lib $(SCIP_LNK_DIR)$Slib$Ssoplex.lib
  DYNAMIC_SCIP_LNK = $(SCIP_LNK_DIR)$Slib$Sscip.lib $(SCIP_LNK_DIR)$Slib$Ssoplex.lib
endif
# This is needed to find SULUM include files and libraries.
ifdef WINDOWS_SLM_DIR
SLM_INC = /I$(WINDOWS_SLM_DIR)$Sheader /DUSE_SLM
SLM_SWIG = -DUSE_SLM
DYNAMIC_SLM_LNK = $(WINDOWS_SLM_DIR)$Swin$(PTRLENGTH)$Sbin$Ssulum$(WINDOWS_SULUM_VERSION).lib
STATIC_SLM_LNK = $(WINDOWS_SLM_DIR)$Swin$(PTRLENGTH)$Sbin$Ssulum$(WINDOWS_SULUM_VERSION).lib
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
    DYNAMIC_GUROBI_LNK = $(WINDOWS_GUROBI_DIR)$Swin32\lib\gurobi$(GUROBI_LIB_VERSION).lib
    STATIC_GUROBI_LNK = $(DYNAMIC_GUROBI_LNK)
  endif
endif

SWIG_INC = $(GLPK_SWIG) $(CLP_SWIG) $(CBC_SWIG) $(SCIP_SWIG) $(SLM_SWIG) $(GUROBI_SWIG) -DUSE_GLOP -DUSE_BOP

JAVA_INC=/I"$(JDK_DIRECTORY)$Sinclude" /I"$(JDK_DIRECTORY)$Sinclude$Swin32"
JAVAC_BIN="$(JDK_DIRECTORY)/bin/javac"
JAVA_BIN="$(JDK_DIRECTORY)/bin/java"
JAR_BIN="$(JDK_DIRECTORY)/bin/jar"

CFLAGS= -nologo $(SYSCFLAGS) $(DEBUG) /I$(INC_DIR) /I$(EX_DIR) /I$(GEN_DIR) \
        $(GFLAGS_INC) $(ZLIB_INC) $(MINISAT_INC) $(PROTOBUF_INC) $(CBC_INC) \
        $(CLP_INC) $(GLPK_INC) $(SCIP_INC) $(SLM_INC) $(GUROBI_INC) /DUSE_GLOP /DUSE_BOP \
        /D__WIN32__  $(SPARSEHASH_INC) /DPSAPI_VERSION=1 $(ARCH)
JNIFLAGS=$(CFLAGS) $(JAVA_INC)
DYNAMIC_GFLAGS_LNK = $(WINDOWS_GFLAGS_DIR)$Slib$Sgflags_static.lib
STATIC_GFLAGS_LNK = $(WINDOWS_GFLAGS_DIR)$Slib$Sgflags_static.lib
ZLIB_LNK = $(WINDOWS_ZLIB_DIR)$Slib$S$(WINDOWS_ZLIB_NAME)
DYNAMIC_PROTOBUF_LNK = $(PROTOBUF_DIR)$Slib$Slibprotobuf.lib
STATIC_PROTOBUF_LNK = $(PROTOBUF_DIR)$Slib$Slibprotobuf.lib
SYS_LNK=psapi.lib ws2_32.lib shlwapi.lib
DEPENDENCIES_LNK = $(STATIC_CBC_LNK) $(STATIC_CLP_LNK) $(STATIC_GLPK_LNK) $(STATIC_SCIP_LNK) $(STATIC_SLM_LNK) $(STATIC_GUROBI_LNK) $(STATIC_GFLAGS_LNK) $(STATIC_PROTOBUF_LNK)
OR_TOOLS_LD_FLAGS = $(ZLIB_LNK) $(SYS_LNK)

COMMA := ,
BACK_SLASH := \\
