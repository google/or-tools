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

O = obj
L = lib
E = .exe
J = .jar
D = .dll
PDB = .pdb
EXP = .exp
ARCHIVE_EXT = .zip
FZ_EXE = fzn-or-tools$E
OBJ_OUT = /Fo
EXE_OUT = /Fe
LD_OUT = /OUT:
DYNAMIC_LD = link /DLL /LTCG /debug
S = \\
CMDSEP = &
CPSEP = ;

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
MVN_BIN := $(shell $(WHICH) mvn.cmd)

# Add some additional macros
ATTRIB = attrib
TASKKILL = taskkill

# Compilation macros.
DEBUG=/O2 -DNDEBUG
CCC=cl /std:c++17 /EHsc /MD /nologo -nologo $(SYSCFLAGS) /D__WIN32__ /DPSAPI_VERSION=1 \
 /DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS

PYTHON_VERSION = $(WINDOWS_PYTHON_VERSION)
PYTHON_INC=/I"$(WINDOWS_PATH_TO_PYTHON)\\include"
PYTHON_LNK="$(WINDOWS_PATH_TO_PYTHON)\\libs\\python$(PYTHON_VERSION).lib"

# This is needed to find GLPK include files and libraries.
ifdef WINDOWS_GLPK_DIR
GLPK_INC = /I"$(WINDOWS_GLPK_DIR)\\include" /DUSE_GLPK
GLPK_SWIG = -I"$(WINDOWS_GLPK_DIR)/include" -DUSE_GLPK
DYNAMIC_GLPK_LNK = "$(WINDOWS_GLPK_DIR)\\lib\\glpk.lib"
STATIC_GLPK_LNK = "$(WINDOWS_GLPK_DIR)\\lib\\glpk.lib"
endif
# This is needed to find CPLEX include files and libraries.
ifdef WINDOWS_CPLEX_DIR
  CPLEX_INC = /I"$(WINDOWS_CPLEX_DIR)\\cplex\\include" /DUSE_CPLEX
  CPLEX_SWIG = -I"$(WINDOWS_CPLEX_DIR)/cplex/include" -DUSE_CPLEX
  STATIC_CPLEX_LNK = "$(WINDOWS_CPLEX_DIR)\\cplex\\lib\\x64_windows_msvc14\\stat_mda\\cplex12100.lib"
  DYNAMIC_CPLEX_LNK = $(STATIC_CPLEX_LNK)
endif
ifdef WINDOWS_XPRESS_DIR
  XPRESS_INC = /I"$(WINDOWS_XPRESS_DIR)\\include" /DUSE_XPRESS /DXPRESS_PATH=\"$(WINDOWS_XPRESS_DIR)\"
  XPRESS_SWIG = -I"$(WINDOWS_XPRESS_DIR)/include" -DUSE_XPRESS
  STATIC_XPRESS_LNK = "$(WINDOWS_XPRESS_DIR)\\lib\\xprs.lib"
  DYNAMIC_XPRESS_LNK = $(STATIC_XPRESS_LNK)
endif

SWIG_INC = \
 -DUSE_GLOP -DUSE_BOP -DABSL_MUST_USE_RESULT \
 $(GLPK_SWIG) $(GUROBI_SWIG) $(CPLEX_SWIG) $(XPRESS_SWIG)

SYS_LNK = psapi.lib ws2_32.lib shlwapi.lib

JAVA_INC=/I"$(JAVA_HOME)\\include" /I"$(JAVA_HOME)\\include\\win32"
JAVAC_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\javac")
JAVA_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\java")
JAR_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\jar")

DEPENDENCIES_INC = /I$(INC_DIR) /I$(GEN_DIR) \
 /DUSE_GLOP /DUSE_BOP \
 $(GLPK_INC) $(GUROBI_INC) $(CPLEX_INC) $(XPRESS_INC)

CFLAGS = $(DEBUG) $(DEPENDENCIES_INC)  /DOR_TOOLS_MAJOR=$(OR_TOOLS_MAJOR) /DOR_TOOLS_MINOR=$(OR_TOOLS_MINOR)
JNIFLAGS=$(CFLAGS) $(DEPENDENCIES_INC)
LDFLAGS =
DEPENDENCIES_LNK = $(SYS_LNK) $(STATIC_GLPK_LNK) $(STATIC_CPLEX_LNK) $(STATIC_XPRESS_LNK)

OR_TOOLS_LNK = $(PRE_LIB)ortools$(POST_LIB)
OR_TOOLS_LDFLAGS =
