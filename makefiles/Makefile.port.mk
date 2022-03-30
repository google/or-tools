# Util variables
SPACE := $(empty) $(empty)
BACKSLASH_SPACE := $(empty)\ $(empty)

# Let's discover something about where we run
ifeq ($(OS),Windows_NT)
OS = Windows
endif
ifeq ($(OS),Windows)
  PLATFORM=WIN64
  SHELL = cmd.exe
endif

# Unix specific part.
ifneq ($(PLATFORM),WIN64)
  OR_TOOLS_TOP ?= $(shell pwd)
  OS = $(shell uname -s)
  PYTHON_VERSION ?= $(shell python3 -c "from sys import version_info as v; print (str(v[0]) + '.' + str(v[1]))")
  CMAKE_PLATFORM = "Unix Makefiles"

  # Unix specific definitions
  LIB_PREFIX = lib

  O = o
  E =
  J = .jar
  D = .dll
  PDB = .pdb
  EXP = .exp
  ARCHIVE_EXT = .tar.gz
  FZ_EXE = fzn-or-tools$E
  OBJ_OUT = -o # need the space
  EXE_OUT = -o # need the space
  S = /

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

  ifeq ($(OS),Linux)
    PLATFORM = LINUX

    CXX ?= g++
    L = so

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

    PRE_LIB = -L$(OR_ROOT_FULL)/install/lib64 -L$(OR_ROOT_FULL)/install/lib -l
    POST_LIB =
    LINK_FLAGS = \
  -Wl,-rpath,'$$ORIGIN' \
  -Wl,-rpath,'$$ORIGIN/../lib64' \
  -Wl,-rpath,'$$ORIGIN/../lib'
    DISTRIBUTION_ID = $(shell lsb_release -i -s)
    DISTRIBUTION_NUMBER = $(shell lsb_release -r -s)
    DISTRIBUTION = $(DISTRIBUTION_ID)-$(DISTRIBUTION_NUMBER)
    PORT = $(DISTRIBUTION)-64bit
    CANDIDATE_JDK_ROOTS = \
      /usr/lib64/jvm/default \
      /usr/lib/jvm/default \
      /usr/lib64/jvm/default-runtime \
      /usr/lib/jvm/default-runtime \
      /usr/lib64/jvm/default-java \
      /usr/lib/jvm/default-java \
      /usr/lib64/jvm/java-openjdk \
      /usr/lib/jvm/java-openjdk \
      /usr/lib/jvm/java-17-openjdk-amd64 \
      /usr/lib/jvm/java-17-openjdk \
      /usr/lib/jvm/java-16-openjdk-amd64 \
      /usr/lib/jvm/java-16-openjdk \
      /usr/lib/jvm/java-15-openjdk-amd64 \
      /usr/lib/jvm/java-15-openjdk \
      /usr/lib/jvm/java-14-openjdk-amd64 \
      /usr/lib/jvm/java-14-openjdk \
      /usr/lib/jvm/java-13-openjdk-amd64 \
      /usr/lib/jvm/java-13-openjdk \
      /usr/lib/jvm/java-12-openjdk-amd64 \
      /usr/lib/jvm/java-12-openjdk \
      /usr/lib/jvm/java-11-openjdk-amd64 \
      /usr/lib/jvm/java-11-openjdk \
      /usr/lib64/jvm/java-1.11.0-openjdk \
      /usr/lib/jvm/java-1.11.0-openjdk \
      /usr/lib/jvm/java-1.11.0-openjdk-amd64 \
      /usr/lib/jvm/java-10-openjdk-amd64 \
      /usr/lib/jvm/java-10-openjdk \
      /usr/lib/jvm/java-9-openjdk-amd64 \
      /usr/lib/jvm/java-9-openjdk \
      /usr/lib/jvm/java-8-openjdk-amd64 \
      /usr/lib/jvm/java-8-openjdk \
      /usr/lib64/jvm/java-1.8.0-openjdk \
      /usr/lib/jvm/java-1.8.0-openjdk \
      /usr/lib/jvm/java-1.8.0-openjdk-amd64 \
      /usr/local/buildtools/java/jdk
    JAVA_HOME ?= $(firstword $(wildcard $(CANDIDATE_JDK_ROOTS)))
  endif # ($(OS),Linux)
  ifeq ($(OS),Darwin) # Assume Mac OS X
    PLATFORM = MACOSX
    OS_VERSION = $(shell sw_vers -productVersion)
    PORT = MacOsX-$(OS_VERSION)
    ifeq ($(wildcard /usr/libexec/java_home),)
      JAVA_HOME = \\\# /usr/libexec/java_home could not be found on your system. Set this variable to the path to jdk to build the java files.
    else
      JAVA_HOME ?= $(shell /usr/libexec/java_home)
    endif
    MAC_MIN_VERSION = 10.15
    MAC_VERSION = -mmacosx-version-min=$(MAC_MIN_VERSION)
    CXX ?= clang++
    L = dylib

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

    PRE_LIB = -L$(OR_ROOT)lib -l
    POST_LIB =
    LINK_FLAGS = \
  -framework CoreFoundation \
  -Wl,-rpath,@loader_path \
  -Wl,-rpath,@loader_path/../lib \
  -Wl,-rpath,@loader_path/../dependencies/lib
  endif # ($(OS),Darwin)

  CFLAGS = \
    $(DEBUG) \
    -I$(INC_DIR) \
    -I$(SRC_DIR) \
    -I$(GEN_DIR) \
    -Wno-deprecated \
    -DUSE_GLOP \
    -DUSE_BOP \
    -DUSE_PDLP \
    $(GLPK_INC) \
    $(CPLEX_INC) \
    $(XPRESS_INC) \
    -DOR_TOOLS_MAJOR=$(OR_TOOLS_MAJOR) \
    -DOR_TOOLS_MINOR=$(OR_TOOLS_MINOR) \
    -DOR_TOOLS_PATCH=$(GIT_REVISION)
  LDFLAGS = \
    $(PRE_LIB)ortools$(POST_LIB) \
    $(ZLIB_LNK) \
    $(SYS_LNK) \
    $(LINK_FLAGS) \
    $(GLPK_LNK) \
    $(CPLEX_LNK) \
    $(XPRESS_LNK)

  # language targets
  HAS_PYTHON ?= ON
  ifeq ($(PYTHON_VERSION),)
    HAS_PYTHON = OFF
  endif

  HAS_JAVA ?= ON
  JAVAC_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/javac)
  JAVA_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/java)
  JAR_BIN = $(shell $(WHICH) $(JAVA_HOME)/bin/jar)
  MVN_BIN := $(shell $(WHICH) mvn)
  ifndef JAVAC_BIN
    HAS_JAVA = OFF
  endif
  ifndef JAR_BIN
    HAS_JAVA = OFF
  endif
  ifndef JAVA_BIN
    HAS_JAVA = OFF
  endif
  ifndef MVN_BIN
    HAS_JAVA = OFF
  endif

  HAS_DOTNET ?= ON
  DOTNET_BIN := $(shell $(WHICH) dotnet 2> /dev/null)
  ifndef DOTNET_BIN
    HAS_DOTNET=OFF
  endif
else # Windows specific part.
  # Check 64 bit.
  ifneq ("$(Platform)","x64")  # Visual Studio 2019/2022 64 bit
    $(warning "Only 64 bit compilation is supported")
  endif

  # Detect visual studio version
  ifeq ("$(VisualStudioVersion)","16.0")
    VISUAL_STUDIO_YEAR = 2019
    VISUAL_STUDIO_MAJOR = 16
    VS_RELEASE = v142
    CMAKE_PLATFORM = "Visual Studio 16 2019" -A x64
  endif
  ifeq ("$(VisualStudioVersion)","17.0")
    VISUAL_STUDIO_YEAR = 2022
    VISUAL_STUDIO_MAJOR = 17
    VS_RELEASE = v143
    CMAKE_PLATFORM = "Visual Studio 17 2022" -A x64
  endif

  ifeq ("$(VISUAL_STUDIO_YEAR)","")
    $(warning "Unrecognized visual studio version")
  endif
  # OS Specific
  OS = Windows
  OR_TOOLS_TOP_AUX = $(shell cd)
  OR_TOOLS_TOP = $(shell echo $(OR_TOOLS_TOP_AUX) | tools\\win\\sed.exe -e "s/\\/\\\\/g" | tools\\win\\sed.exe -e "s/ //g")

  # Compiler specific
  PORT = VisualStudio$(VISUAL_STUDIO_YEAR)-64bit
  VS_COMTOOLS = $(VISUAL_STUDIO_MAJOR)0

  # Detect Python
  PYTHON_VERSION ?= $(shell python -c "from sys import version_info as v; print (str(v[0]) + str(v[1]))")

  # Windows specific definitions
  LIB_PREFIX =
  PRE_LIB = $(OR_ROOT)lib\\
  POST_LIB = .lib
  LIB_SUFFIX = lib
  STATIC_PRE_LIB = $(OR_ROOT)lib\\
  STATIC_POST_LIB = .lib
  STATIC_LIB_SUFFIX = lib

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
  S = \\

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

  # Compilation macros.
  DEBUG=/O2 -DNDEBUG
  CCC=cl /std:c++20 /EHsc /MD /nologo -nologo $(SYSCFLAGS) /D__WIN32__ /DPSAPI_VERSION=1 \
  /DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS

  # This is needed to find GLPK include files and libraries.
  ifdef WINDOWS_GLPK_DIR
    GLPK_INC = /I"$(WINDOWS_GLPK_DIR)\\include" /DUSE_GLPK
    GLPK_LNK = "$(WINDOWS_GLPK_DIR)\\lib\\glpk.lib"
  endif
  # This is needed to find CPLEX include files and libraries.
  ifdef WINDOWS_CPLEX_DIR
    CPLEX_INC = /I"$(WINDOWS_CPLEX_DIR)\\cplex\\include" /DUSE_CPLEX
    CPLEX_LNK = "$(WINDOWS_CPLEX_DIR)\\cplex\\lib\\x64_windows_msvc14\\stat_mda\\cplex$(WINDOWS_CPLEX_VERSION).lib"
  endif
  ifdef WINDOWS_XPRESS_DIR
    XPRESS_INC = /I"$(WINDOWS_XPRESS_DIR)\\include" /DUSE_XPRESS /DXPRESS_PATH=\"$(WINDOWS_XPRESS_DIR)\"
    XPRESS_LNK = "$(WINDOWS_XPRESS_DIR)\\lib\\xprs.lib"
  endif

  SYS_LNK = psapi.lib ws2_32.lib shlwapi.lib

  CFLAGS = \
    $(DEBUG) \
    /I$(INC_DIR) \
    /I$(SRC_DIR) \
    /I$(GEN_DIR) \
    /DUSE_GLOP \
    /DUSE_BOP \
    /DUSE_PDLP \
    $(GLPK_INC) \
    $(CPLEX_INC) \
    $(XPRESS_INC) \
    /DOR_TOOLS_MAJOR=$(OR_TOOLS_MAJOR) \
    /DOR_TOOLS_MINOR=$(OR_TOOLS_MINOR) \
    /DOR_TOOLS_PATCH=$(GIT_REVISION)
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

  LDFLAGS = \
    $(PRE_LIB)ortools$(POST_LIB) \
    $(PRE_LIB)libprotobuf.lib \
    $(PRE_LIB)re2.lib \
    $(PRE_LIB)zlib.lib \
    $(ABSL_LNK) \
    $(COINOR_LNK) \
    $(SCIP_LNK) \
    $(SYS_LNK) \
    $(GLPK_LNK) \
    $(CPLEX_LNK) \
    $(XPRESS_LNK)

  # language targets
  HAS_PYTHON ?= ON
  ifeq ($(PYTHON_VERSION),)
    HAS_PYTHON = OFF
  endif

  HAS_JAVA ?= ON
  JAVAC_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\javac")
  JAVA_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\java")
  JAR_BIN=$(shell $(WHICH) "$(JAVA_HOME)\bin\jar")
  MVN_BIN := $(shell $(WHICH) mvn.cmd)
  ifndef JAVAC_BIN
    HAS_JAVA = OFF
  endif
  ifndef JAR_BIN
    HAS_JAVA = OFF
  endif
  ifndef JAVA_BIN
    HAS_JAVA = OFF
  endif
  ifndef MVN_BIN
    HAS_JAVA = OFF
  endif

  HAS_DOTNET ?= ON
  DOTNET_BIN := $(shell $(WHICH) dotnet 2> NUL)
  ifndef DOTNET_BIN
    HAS_DOTNET=OFF
  endif
endif  # ($(PLATFORM),WIN64)

# C++ relevant directory
SRC_DIR = $(OR_ROOT).
BUILD_DIR = $(OR_ROOT)build_make
INSTALL_DIR ?= $(OR_ROOT)install_make

GEN_DIR = $(OR_ROOT)ortools/gen
GEN_PATH = $(subst /,$S,$(GEN_DIR))
INC_DIR = $(OR_ROOT)include
LIB_DIR = $(OR_ROOT)lib
FZ_EX_DIR  = $(OR_ROOT)examples/flatzinc
FZ_EX_PATH = $(subst /,$S,$(FZ_EX_DIR))

# Python relevant directory
PYTHON_EX_DIR  = $(OR_ROOT)examples/python
PYTHON_EX_PATH = $(subst /,$S,$(PYTHON_EX_DIR))

# Java relevant directory
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

# Get github revision level
ifneq ($(wildcard .git),)
 ifneq ($(wildcard .git/shallow),)
  $(warning you are using a shallow copy)
  GIT_REVISION:= 9999
 else
   GIT_REVISION:= $(shell git rev-list --count HEAD)
 endif
 GIT_HASH:= $(shell git rev-parse --short HEAD)
else
  GIT_REVISION:= 9999
  GIT_HASH:= "not_on_git"
endif

OR_TOOLS_VERSION := $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR).$(GIT_REVISION)
OR_TOOLS_SHORT_VERSION := $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR)
ifdef PRE_RELEASE
  OR_TOOLS_VERSION := $(OR_TOOLS_VERSION)-beta
  OR_TOOLS_SHORT_VERSION := $(OR_TOOLS_SHORT_VERSION)-beta
endif
FZ_INSTALL_DIR = or-tools_flatzinc_$(PORT)_v$(OR_TOOLS_VERSION)

INSTALL_CPP_NAME = or-tools_cpp_$(PORT)_v$(OR_TOOLS_VERSION)
INSTALL_DOTNET_NAME = or-tools_dotnet_$(PORT)_v$(OR_TOOLS_VERSION)
INSTALL_JAVA_NAME = or-tools_java_$(PORT)_v$(OR_TOOLS_VERSION)
INSTALL_PYTHON_NAME = or-tools_python_$(PORT)_v$(OR_TOOLS_VERSION)
INSTALL_DATA_NAME = or-tools_data_v$(OR_TOOLS_VERSION)

BUILD_DOTNET ?= OFF
BUILD_JAVA ?= OFF
BUILD_PYTHON ?= OFF

.PHONY: detect_port # Show variables used to build OR-Tools.
detect_port:
	@echo Relevant info on the system:
	@echo OR_TOOLS_TOP = $(OR_TOOLS_TOP)
	@echo SHELL = $(SHELL)
	@echo OS = $(OS)
	@echo PLATFORM = $(PLATFORM)
	@echo PORT = $(PORT)
	@echo OR_TOOLS_VERSION = $(OR_TOOLS_VERSION)
	@echo OR_TOOLS_SHORT_VERSION = $(OR_TOOLS_SHORT_VERSION)
	@echo GIT_REVISION = $(GIT_REVISION)
	@echo GIT_HASH = $(GIT_HASH)
	@echo CMAKE = $(CMAKE)
	@echo CMAKE_PLATFORM = $(CMAKE_PLATFORM)
	@echo HAS_DOTNET = $(HAS_DOTNET)
	@echo HAS_JAVA = $(HAS_JAVA)
	@echo HAS_PYTHON = $(HAS_PYTHON)
ifeq ($(PLATFORM),WIN64)
	@echo off & echo(
else
	@echo
endif
