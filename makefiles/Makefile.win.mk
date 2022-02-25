#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard. In that case, please tell us -----
#  ----- about it. -----

# Windows specific definitions
LIB_PREFIX =
PRE_LIB = $(OR_ROOT)lib\\
POST_LIB = .lib
LIB_SUFFIX = lib
JNI_LIB_EXT = dll
STATIC_PRE_LIB = $(OR_ROOT)lib\\
STATIC_POST_LIB = .lib
STATIC_LIB_SUFFIX = lib
LINK_CMD = lib
STATIC_LINK_CMD = lib
# C++ relevant directory
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

CMAKE := $(shell $(WHICH) cmake)
ifeq ($(CMAKE),)
$(error Please add "cmake" to your PATH)
endif


# Add some additional macros
ATTRIB = attrib
TASKKILL = taskkill

# Compilation macros.
DEBUG=/O2 -DNDEBUG
CCC=cl /std:c++20 /EHsc /MD /nologo -nologo $(SYSCFLAGS) /D__WIN32__ /DPSAPI_VERSION=1 \
 /DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS

# This is needed to find GLPK include files and libraries.
ifdef WINDOWS_GLPK_DIR
GLPK_INC = /I"$(WINDOWS_GLPK_DIR)\\include" /DUSE_GLPK
DYNAMIC_GLPK_LNK = "$(WINDOWS_GLPK_DIR)\\lib\\glpk.lib"
STATIC_GLPK_LNK = "$(WINDOWS_GLPK_DIR)\\lib\\glpk.lib"
endif
# This is needed to find CPLEX include files and libraries.
ifdef WINDOWS_CPLEX_DIR
  CPLEX_INC = /I"$(WINDOWS_CPLEX_DIR)\\cplex\\include" /DUSE_CPLEX
  STATIC_CPLEX_LNK = "$(WINDOWS_CPLEX_DIR)\\cplex\\lib\\x64_windows_msvc14\\stat_mda\\cplex$(WINDOWS_CPLEX_VERSION).lib"
  DYNAMIC_CPLEX_LNK = $(STATIC_CPLEX_LNK)
endif
ifdef WINDOWS_XPRESS_DIR
  XPRESS_INC = /I"$(WINDOWS_XPRESS_DIR)\\include" /DUSE_XPRESS /DXPRESS_PATH=\"$(WINDOWS_XPRESS_DIR)\"
  STATIC_XPRESS_LNK = "$(WINDOWS_XPRESS_DIR)\\lib\\xprs.lib"
  DYNAMIC_XPRESS_LNK = $(STATIC_XPRESS_LNK)
endif

SYS_LNK = psapi.lib ws2_32.lib shlwapi.lib

DEPENDENCIES_INC = /I$(INC_DIR) /I$(SRC_DIR) /I$(GEN_DIR) \
 /DUSE_GLOP /DUSE_BOP \
 $(GLPK_INC) $(GUROBI_INC) $(CPLEX_INC) $(XPRESS_INC)

CFLAGS = $(DEBUG) $(DEPENDENCIES_INC)  /DOR_TOOLS_MAJOR=$(OR_TOOLS_MAJOR) /DOR_TOOLS_MINOR=$(OR_TOOLS_MINOR) /DOR_TOOLS_PATCH=$(GIT_REVISION)
JNIFLAGS=$(CFLAGS) $(DEPENDENCIES_INC)
LDFLAGS =
DEPENDENCIES_LNK += $(PRE_LIB)libprotobuf.lib $(PRE_LIB)re2.lib $(PRE_LIB)zlib.lib
ifneq ($(USE_COINOR),OFF)
    COINOR_LNK = \
    $(PRE_LIB)CoinUtils.lib \
    $(PRE_LIB)Osi.lib \
    $(PRE_LIB)Clp.lib \
    $(PRE_LIB)OsiClp.lib \
    $(PRE_LIB)Cgl.lib \
    $(PRE_LIB)Cbc.lib \
    $(PRE_LIB)OsiCbc.lib \
    $(PRE_LIB)ClpSolver.lib \
    $(PRE_LIB)CbcSolver.lib
endif
ifneq ($(USE_SCIP),OFF)
    SCIP_LNK = $(PRE_LIB)libscip.lib
endif
ABSL_LNK = \
 $(PRE_LIB)absl_bad_any_cast_impl.lib \
 $(PRE_LIB)absl_bad_optional_access.lib \
 $(PRE_LIB)absl_bad_variant_access.lib \
 $(PRE_LIB)absl_base.lib \
 $(PRE_LIB)absl_city.lib \
 $(PRE_LIB)absl_civil_time.lib \
 $(PRE_LIB)absl_cord.lib \
 $(PRE_LIB)absl_cordz_functions.lib \
 $(PRE_LIB)absl_cordz_handle.lib \
 $(PRE_LIB)absl_cordz_info.lib \
 $(PRE_LIB)absl_cordz_sample_token.lib \
 $(PRE_LIB)absl_cord_internal.lib \
 $(PRE_LIB)absl_debugging_internal.lib \
 $(PRE_LIB)absl_demangle_internal.lib \
 $(PRE_LIB)absl_examine_stack.lib \
 $(PRE_LIB)absl_exponential_biased.lib \
 $(PRE_LIB)absl_failure_signal_handler.lib \
 $(PRE_LIB)absl_flags.lib \
 $(PRE_LIB)absl_flags_commandlineflag.lib \
 $(PRE_LIB)absl_flags_commandlineflag_internal.lib \
 $(PRE_LIB)absl_flags_config.lib \
 $(PRE_LIB)absl_flags_internal.lib \
 $(PRE_LIB)absl_flags_marshalling.lib \
 $(PRE_LIB)absl_flags_parse.lib \
 $(PRE_LIB)absl_flags_private_handle_accessor.lib \
 $(PRE_LIB)absl_flags_program_name.lib \
 $(PRE_LIB)absl_flags_reflection.lib \
 $(PRE_LIB)absl_flags_usage.lib \
 $(PRE_LIB)absl_flags_usage_internal.lib \
 $(PRE_LIB)absl_graphcycles_internal.lib \
 $(PRE_LIB)absl_hash.lib \
 $(PRE_LIB)absl_hashtablez_sampler.lib \
 $(PRE_LIB)absl_int128.lib \
 $(PRE_LIB)absl_leak_check.lib \
 $(PRE_LIB)absl_leak_check_disable.lib \
 $(PRE_LIB)absl_log_severity.lib \
 $(PRE_LIB)absl_low_level_hash.lib \
 $(PRE_LIB)absl_malloc_internal.lib \
 $(PRE_LIB)absl_periodic_sampler.lib \
 $(PRE_LIB)absl_random_distributions.lib \
 $(PRE_LIB)absl_random_internal_distribution_test_util.lib \
 $(PRE_LIB)absl_random_internal_platform.lib \
 $(PRE_LIB)absl_random_internal_pool_urbg.lib \
 $(PRE_LIB)absl_random_internal_randen.lib \
 $(PRE_LIB)absl_random_internal_randen_hwaes.lib \
 $(PRE_LIB)absl_random_internal_randen_hwaes_impl.lib \
 $(PRE_LIB)absl_random_internal_randen_slow.lib \
 $(PRE_LIB)absl_random_internal_seed_material.lib \
 $(PRE_LIB)absl_random_seed_gen_exception.lib \
 $(PRE_LIB)absl_random_seed_sequences.lib \
 $(PRE_LIB)absl_raw_hash_set.lib \
 $(PRE_LIB)absl_raw_logging_internal.lib \
 $(PRE_LIB)absl_scoped_set_env.lib \
 $(PRE_LIB)absl_spinlock_wait.lib \
 $(PRE_LIB)absl_stacktrace.lib \
 $(PRE_LIB)absl_status.lib \
 $(PRE_LIB)absl_statusor.lib \
 $(PRE_LIB)absl_strerror.lib \
 $(PRE_LIB)absl_strings.lib \
 $(PRE_LIB)absl_strings_internal.lib \
 $(PRE_LIB)absl_str_format_internal.lib \
 $(PRE_LIB)absl_symbolize.lib \
 $(PRE_LIB)absl_synchronization.lib \
 $(PRE_LIB)absl_throw_delegate.lib \
 $(PRE_LIB)absl_time.lib \
 $(PRE_LIB)absl_time_zone.lib

 DEPENDENCIES_LNK += $(ABSL_LNK) $(COINOR_LNK) $(SCIP_LNK) $(SYS_LNK) $(STATIC_GLPK_LNK) $(STATIC_CPLEX_LNK) $(STATIC_XPRESS_LNK)

OR_TOOLS_LNK = $(PRE_LIB)ortools$(POST_LIB) $(DEPENDENCIES_LNK)
OR_TOOLS_LDFLAGS =

# language targets

BUILD_PYTHON ?= ON
ifeq ($(PYTHON_VERSION),)
BUILD_PYTHON = OFF
endif

BUILD_JAVA ?= ON
JAVAC_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\javac")
JAVA_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\java")
JAR_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\jar")
MVN_BIN := $(shell $(WHICH) mvn.cmd)
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
DOTNET_BIN := $(shell $(WHICH) dotnet 2> NUL)
ifndef DOTNET_BIN
BUILD_DOTNET=OFF
endif

.PHONY detect_languages:
detect_languages:
	@echo BUILD_PYTHON = $(BUILD_PYTHON)
	@echo BUILD_JAVA = $(BUILD_JAVA)
	@echo BUILD_DOTNET = $(BUILD_DOTNET)

