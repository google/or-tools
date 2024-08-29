# Copyright 2010-2024 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Util variables
SPACE := $(empty) $(empty)
BACKSLASH_SPACE := $(empty)\ $(empty)

# Let's discover something about where we run
ifeq ($(OS),Windows_NT)
OS := Windows
endif
ifeq ($(OS),Windows)
  PLATFORM := WIN64
  SHELL := cmd.exe
endif

# Unix specific part.
ifneq ($(PLATFORM),WIN64)
  OR_TOOLS_TOP ?= $(CURDIR)
  OS = $(shell uname -s)
  CPU = $(shell uname -m)
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
  FZ_EXE = fzn-ortools$E
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

  # Compilation flags
  DEBUG = -O4 -DNDEBUG

  ifeq ($(OS),Linux)
    PLATFORM = LINUX

    CXX ?= g++
    L = so

    DISTRIBUTION_ID = $(shell lsb_release -i -s)
    DISTRIBUTION_NUMBER = $(shell lsb_release -r -s)
    DISTRIBUTION ?= $(DISTRIBUTION_ID)-$(DISTRIBUTION_NUMBER)
    PORT = $(DISTRIBUTION)
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
    PORT = macOS-$(OS_VERSION)
    ifeq ($(wildcard /usr/libexec/java_home),)
      JAVA_HOME = \\\# /usr/libexec/java_home could not be found on your system. Set this variable to the path to jdk to build the java files.
    else
      JAVA_HOME ?= $(shell /usr/libexec/java_home)
    endif
    MAC_MIN_VERSION = 10.15
    MAC_VERSION = -mmacosx-version-min=$(MAC_MIN_VERSION)
    CXX ?= clang++
    L = dylib

  endif # ($(OS),Darwin)

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
    $(error "Only 64 bit compilation is supported")
  else
    CPU = x64
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
  ifeq ("$(VisualStudioVersion)","17.1")
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
  OR_TOOLS_TOP := $(shell echo $(OR_TOOLS_TOP_AUX) | tools\\win\\sed.exe -e "s/\\/\\\\/g" | tools\\win\\sed.exe -e "s/ //g")

  # Compiler specific
  PORT = VisualStudio$(VISUAL_STUDIO_YEAR)
  VS_COMTOOLS = $(VISUAL_STUDIO_MAJOR)0

  # Detect Python
  PYTHON_VERSION ?= $(shell python -c "from sys import version_info as v; print (str(v[0]) + str(v[1]))")

  # Windows specific definitions
  LIB_PREFIX =
  LIB_SUFFIX = lib

  O = obj
  L = lib
  E = .exe
  J = .jar
  D = .dll
  PDB = .pdb
  EXP = .exp
  ARCHIVE_EXT = .zip
  FZ_EXE = fzn-ortools$E
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
SRC_DIR := $(OR_ROOT).
BUILD_DIR := $(OR_ROOT)build_make
INSTALL_DIR ?= $(OR_ROOT)install_make

# Get github revision level
ifeq ($(OR_TOOLS_PATCH),)
  ifneq ($(wildcard .git),)
   ifneq ($(wildcard .git/shallow),)
    $(warning you are using a shallow copy)
    OR_TOOLS_PATCH:= 9999
   else
     OR_TOOLS_PATCH:= $(shell git rev-list --count v$(OR_TOOLS_MAJOR).0..HEAD || echo 0)
   endif
  else
    $(warning you are not using a .git archive)
    OR_TOOLS_PATCH:= 9999
  endif
endif

OR_TOOLS_VERSION := $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR).$(OR_TOOLS_PATCH)
OR_TOOLS_SHORT_VERSION := $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR)
ifdef PRE_RELEASE
  OR_TOOLS_VERSION := $(OR_TOOLS_VERSION)-beta
  OR_TOOLS_SHORT_VERSION := $(OR_TOOLS_SHORT_VERSION)-beta
endif

ifneq ($(wildcard .git),)
  GIT_HASH:= $(shell git rev-parse --short HEAD)
else
  GIT_HASH:= "not_on_git"
endif

#FZ_INSTALL_NAME := or-tools_$(CPU)_$(PORT)_flatzinc_v$(OR_TOOLS_VERSION)
INSTALL_CPP_NAME := or-tools_$(CPU)_$(PORT)_cpp_v$(OR_TOOLS_VERSION)
INSTALL_DOTNET_NAME := or-tools_$(CPU)_$(PORT)_dotnet_v$(OR_TOOLS_VERSION)
INSTALL_JAVA_NAME := or-tools_$(CPU)_$(PORT)_java_v$(OR_TOOLS_VERSION)
INSTALL_PYTHON_NAME := or-tools_$(CPU)_$(PORT)_python$(PYTHON_VERSION)_v$(OR_TOOLS_VERSION)

BUILD_DOTNET ?= OFF
BUILD_JAVA ?= OFF
BUILD_PYTHON ?= OFF

.PHONY: detect_port # Show variables used to build OR-Tools.
detect_port:
	@echo Relevant info on the system:
	@echo OR_TOOLS_TOP = $(OR_TOOLS_TOP)
	@echo SHELL = $(SHELL)
	@echo OS = $(OS)
	@echo CPU = $(CPU)
	@echo PLATFORM = $(PLATFORM)
	@echo PORT = $(PORT)
	@echo OR_TOOLS_MAJOR = $(OR_TOOLS_MAJOR)
	@echo OR_TOOLS_MINOR = $(OR_TOOLS_MINOR)
	@echo OR_TOOLS_PATCH = $(OR_TOOLS_PATCH)
	@echo OR_TOOLS_VERSION = $(OR_TOOLS_VERSION)
	@echo OR_TOOLS_SHORT_VERSION = $(OR_TOOLS_SHORT_VERSION)
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
