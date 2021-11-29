# Util variables
SPACE := $(empty) $(empty)
BACKSLASH_SPACE := $(empty)\ $(empty)

# Let's discover something about where we run
ifeq ($(OS),Windows_NT)
OS = Windows
endif
ifeq ($(OS),Windows)
  SYSTEM = win
else
  SYSTEM = unix
endif

# Unix specific part.
ifeq ($(SYSTEM),unix)
  OR_TOOLS_TOP ?= $(shell pwd)
  OS = $(shell uname -s)
  ifeq ($(UNIX_PYTHON_VER),)
    ifeq ($(shell which python3),)
      DETECTED_PYTHON_VERSION := $(shell python -c "from sys import version_info as v; print (str(v[0]) + '.' + str(v[1]))")
    else
      DETECTED_PYTHON_VERSION := $(shell python3 -c "from sys import version_info as v; print (str(v[0]) + '.' + str(v[1]))")
    endif
  else
    DETECTED_PYTHON_VERSION := $(UNIX_PYTHON_VER)
  endif

  ifeq ($(OS),Linux)
    PLATFORM = LINUX
    CODEPORT = OpSys-Linux
    LBITS = $(shell getconf LONG_BIT)
    DISTRIBUTION_ID = $(shell lsb_release -i -s)
    DISTRIBUTION_NUMBER = $(shell lsb_release -r -s)
    DISTRIBUTION = $(DISTRIBUTION_ID)-$(DISTRIBUTION_NUMBER)
    ifeq ($(LBITS),64)
      NETPLATFORM = anycpu
      PORT = $(DISTRIBUTION)-64bit
      PTRLENGTH = 64
      GUROBI_PLATFORM=linux64
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
        /usr/lib/jvm/java-7-openjdk-amd64 \
        /usr/lib64/jvm/java-1.7.0-openjdk \
        /usr/lib/jvm/java-1.7.0-openjdk \
        /usr/lib/jvm/java-1.7.0-openjdk.x86_64 \
        /usr/lib/jvm/java-6-openjdk-amd64 \
        /usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86_64 \
        /usr/lib64/jvm/java-6-sun-1.6.0.26 \
        /usr/lib64/jvm/java-1.6.0-openjdk-1.6.0 \
        /usr/local/buildtools/java/jdk
    else
      NETPLATFORM = x86
      PORT = $(DISTRIBUTION)-32bit
      PTRLENGTH = 32
      GUROBI_PLATFORM=linux32
      CANDIDATE_JDK_ROOTS = \
        /usr/local/buildtools/java/jdk-32 \
        /usr/lib/jvm/java-1.13.0-openjdk-i386 \
        /usr/lib/jvm/java-1.12.0-openjdk-i386 \
        /usr/lib/jvm/java-1.11.0-openjdk-i386 \
        /usr/lib/jvm/java-1.10.0-openjdk-i386 \
        /usr/lib/jvm/java-1.9.0-openjdk-i386 \
        /usr/lib/jvm/java-1.8.0-openjdk-i386 \
        /usr/lib/jvm/java-1.7.0-openjdk-i386 \
        /usr/lib/jvm/java-1.6.0-openjdk-1.6.0 \
        /usr/lib/jvm/java-6-sun-1.6.0.26 \
        /usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86 \
        /usr/lib/jvm/java-6-openjdk-i386 \
        /usr/lib/jvm/java-7-openjdk-i386

    endif
    JAVA_HOME ?= $(firstword $(wildcard $(CANDIDATE_JDK_ROOTS)))
  endif # ($(OS),Linux)
  ifeq ($(OS),Darwin) # Assume Mac OS X
    PLATFORM = MACOSX
    OS_VERSION = $(shell sw_vers -productVersion)
    PORT = MacOsX-$(OS_VERSION)
    CODEPORT = OpSys-OSX
    NETPLATFORM = x64
    PTRLENGTH = 64
    GUROBI_PLATFORM=mac64
    ifeq ($(wildcard /usr/libexec/java_home),)
      JAVA_HOME = \\\# /usr/libexec/java_home could not be found on your system. Set this variable to the path to jdk to build the java files.
    else
      JAVA_HOME ?= $(shell /usr/libexec/java_home)
    endif
    MAC_MIN_VERSION = 10.14
  endif # ($(OS),Darwin)
endif # ($(SYSTEM),unix)

# Windows specific part.
ifeq ("$(SYSTEM)","win")
  PLATFORM = WIN64
  PTRLENGTH = 64
  CBC_PLATFORM_PREFIX = x64
  GLPK_PLATFORM = w64
  NETPLATFORM = x64

  # Check 64 bit.
  ifneq ("$(Platform)","x64")  # Visual Studio 2017/2019 64 bit
    $(warning "Only 64 bit compilation is supported")
  endif

  # Detect visual studio version
  ifeq ("$(VisualStudioVersion)","15.0")
    VISUAL_STUDIO_YEAR = 2017
    VISUAL_STUDIO_MAJOR = 15
    VS_RELEASE = v141
    CMAKE_PLATFORM = "Visual Studio 15 2017 Win64"
  endif
  ifeq ("$(VisualStudioVersion)","16.0")
    VISUAL_STUDIO_YEAR = 2019
    VISUAL_STUDIO_MAJOR = 16
    VS_RELEASE = v142
    CMAKE_PLATFORM = "Visual Studio 16 2019" -A x64
  endif

  ifeq ("$(VISUAL_STUDIO_YEAR)","")
    $(warning "Unrecognized visual studio version")
  endif

# OS Specific
  OS = Windows
  OR_TOOLS_TOP_AUX = $(shell cd)
  OR_TOOLS_TOP = $(shell echo $(OR_TOOLS_TOP_AUX) | tools\\win\\sed.exe -e "s/\\/\\\\/g" | tools\\win\\sed.exe -e "s/ //g")
  CODEPORT = OpSys-Windows

  # Compiler specific
  PORT = VisualStudio$(VISUAL_STUDIO_YEAR)-$(PTRLENGTH)bit
  VS_COMTOOLS = $(VISUAL_STUDIO_MAJOR)0

  # Third party specific
  CBC_PLATFORM = $(CBC_PLATFORM_PREFIX)-$(VS_RELEASE)-Release

  # Java specific
  ifeq ($(JAVA_HOME),)
    SELECTED_PATH_TO_JDK = JAVA_HOME = \# JAVA_HOME is not set on your system. Set it to the path to jdk to build the java files.
  else
    SELECTED_PATH_TO_JDK = JAVA_HOME = $(JAVA_HOME)
  endif

  # Detect Python
  ifeq ($(WINDOWS_PATH_TO_PYTHON),)
    DETECTED_PATH_TO_PYTHON = $(shell python -c "from sys import executable; from os.path import sep; print(sep.join(executable.split(sep)[:-1]).rstrip())")
    CANONIC_DETECTED_PATH_TO_PYTHON = $(subst $(SPACE),$(BACKSLASH_SPACE),$(subst \,/,$(subst \\,/,$(DETECTED_PATH_TO_PYTHON))))
    ifeq ($(wildcard $(CANONIC_DETECTED_PATH_TO_PYTHON)),)
      SELECTED_PATH_TO_PYTHON = WINDOWS_PATH_TO_PYTHON =\# python was not found. Set this variable to the path to python to build the python files. Don\'t include the name of the executable in the path! (ex: WINDOWS_PATH_TO_PYTHON = c:\\python37-64)
    else
      SELECTED_PATH_TO_PYTHON = WINDOWS_PATH_TO_PYTHON = $(DETECTED_PATH_TO_PYTHON)
      WINDOWS_PATH_TO_PYTHON = $(DETECTED_PATH_TO_PYTHON)
    endif
  else
    SELECTED_PATH_TO_PYTHON = WINDOWS_PATH_TO_PYTHON = $(WINDOWS_PATH_TO_PYTHON)
  endif
  ifneq ($(WINDOWS_PATH_TO_PYTHON),)
    WINDOWS_PYTHON_VERSION = $(shell "$(WINDOWS_PATH_TO_PYTHON)\python" -c "from sys import version_info as v; print (str(v[0]) + str(v[1]))")
  endif
endif # ($(SYSTEM),win)

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
INSTALL_DIR = or-tools_$(PORT)_v$(OR_TOOLS_VERSION)
FZ_INSTALL_DIR = or-tools_flatzinc_$(PORT)_v$(OR_TOOLS_VERSION)
DATA_INSTALL_DIR = or-tools_data_v$(OR_TOOLS_VERSION)

.PHONY: detect_port # Show variables used to build OR-Tools.
detect_port:
	@echo Relevant info on the system:
	@echo SYSTEM = $(SYSTEM)
	@echo OS = $(OS)
	@echo PLATFORM = $(PLATFORM)
	@echo PTRLENGTH = $(PTRLENGTH)
	@echo PORT = $(PORT)
	@echo SHELL = $(SHELL)
	@echo OR_TOOLS_TOP = $(OR_TOOLS_TOP)
	@echo OR_TOOLS_VERSION = $(OR_TOOLS_VERSION)
	@echo OR_TOOLS_SHORT_VERSION = $(OR_TOOLS_SHORT_VERSION)
	@echo GIT_REVISION = $(GIT_REVISION)
	@echo GIT_HASH = $(GIT_HASH)
	@echo CMAKE = $(CMAKE)
ifeq ($(SYSTEM),win)
	@echo CMAKE_PLATFORM = $(CMAKE_PLATFORM)
endif
	@echo SWIG_BINARY = $(SWIG_BINARY)
	@echo SWIG_INC = $(SWIG_INC)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
