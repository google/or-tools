.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.win.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(

# Checks if the user has overwritten default libraries and binaries.
WINDOWS_ZLIB_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_ZLIB_PATH = $(subst /,$S,$(WINDOWS_ZLIB_DIR))
WINDOWS_ZLIB_NAME ?= zlib.lib
WINDOWS_PROTOBUF_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_PROTOBUF_PATH = $(subst /,$S,$(WINDOWS_PROTOBUF_DIR))
WINDOWS_ABSL_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_ABSL_PATH = $(subst /,$S,$(WINDOWS_ABSL_DIR))
USE_COINOR ?= ON
WINDOWS_CBC_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_CBC_PATH = $(subst /,$S,$(WINDOWS_CBC_DIR))
WINDOWS_CGL_DIR ?= $(WINDOWS_CBC_DIR)
WINDOWS_CGL_PATH = $(subst /,$S,$(WINDOWS_CGL_DIR))
WINDOWS_CLP_DIR ?= $(WINDOWS_CBC_DIR)
WINDOWS_CLP_PATH = $(subst /,$S,$(WINDOWS_CLP_DIR))
WINDOWS_OSI_DIR ?= $(WINDOWS_CBC_DIR)
WINDOWS_OSI_PATH = $(subst /,$S,$(WINDOWS_OSI_DIR))
WINDOWS_COINUTILS_DIR ?= $(WINDOWS_CBC_DIR)
WINDOWS_COINUTILS_PATH = $(subst /,$S,$(WINDOWS_COINUTILS_DIR))
USE_SCIP ?= ON
WINDOWS_SCIP_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_SCIP_PATH = $(subst /,$S,$(WINDOWS_SCIP_DIR))
WINDOWS_SWIG_BINARY ?= "$(OR_ROOT)dependencies\\install\\swigwin-$(SWIG_TAG)\\swig.exe"

# Variable use in others Makefiles
PROTOC = "$(WINDOWS_PROTOBUF_DIR)\\bin\\protoc.exe"
SWIG_BINARY = $(WINDOWS_SWIG_BINARY)

# tags of dependencies to checkout.
ZLIB_TAG = 1.2.11
ZLIB_ARCHIVE_TAG = 1211
PROTOBUF_TAG = v3.17.3
ABSL_TAG = 20210324.2
# We are using a CBC archive containing all coin-or project
# since Clp 2.17.5+ is broken we need to stick with Cbc 2.10.4
# which is shipped with Clp 1.17.4
CBC_TAG = 2.10.4
CGL_TAG = 0.60.3
CLP_TAG = 1.17.4
OSI_TAG = 0.108.6
COINUTILS_TAG = 2.11.4
SWIG_TAG = 4.0.2
SCIP_TAG=7.0.1

# Added in support of clean third party targets
TSVNCACHE_EXE = TSVNCache.exe

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party: build_third_party

.PHONY: third_party_check # Check if third parties are all found
third_party_check: dependencies/check.log

dependencies/check.log: Makefile.local
ifeq ($(wildcard $(WINDOWS_ZLIB_DIR)/include/zlib.h),)
	$(error Third party ZLIB files was not found! did you run 'make third_party' or set WINDOWS_ZLIB_DIR ?)
else
	@echo ZLIB: found
endif
ifeq ($(wildcard $(WINDOWS_PROTOBUF_DIR)/include/google/protobuf/descriptor.h),)
	$(error Third party Protobuf files was not found! did you run 'make third_party' or set WINDOWS_PROTOBUF_DIR ?)
else
	@echo PROTOBUF: found
endif
ifeq ($(wildcard $(WINDOWS_COINUTILS_DIR)/include/coinutils/coin/CoinModel.hpp $(WINDOWS_COINUTILS_DIR)/include/coin/CoinModel.hpp),)
	$(error Third party CoinUtils files was not found! did you run 'make third_party' or set WINDOWS_COINUTILS_DIR ?)
else
	@echo COINUTILS: found
endif
ifeq ($(wildcard $(WINDOWS_OSI_DIR)/include/osi/coin/OsiSolverInterface.hpp $(WINDOWS_OSI_DIR)/include/coin/OsiSolverInterface.hpp),)
	$(error Third party Osi files was not found! did you run 'make third_party' or set WINDOWS_OSI_DIR ?)
else
	@echo OSI: found
endif
ifeq ($(wildcard $(WINDOWS_CLP_DIR)/include/clp/coin/ClpModel.hpp $(WINDOWS_CLP_DIR)/include/coin/ClpModel.hpp),)
	$(error Third party Clp files was not found! did you run 'make third_party' or set WINDOWS_CLP_DIR ?)
else
	@echo CLP: found
endif
ifeq ($(wildcard $(WINDOWS_CGL_DIR)/include/cgl/coin/CglParam.hpp $(WINDOWS_CGL_DIR)/include/coin/CglParam.hpp),)
	$(error Third party Cgl files was not found! did you run 'make third_party' or set WINDOWS_CGL_DIR ?)
else
	@echo CGL: found
endif
ifeq ($(USE_COINOR),OFF)
	@echo Coin OR (CLP, CBC): disabled
else
ifeq ($(wildcard $(WINDOWS_CBC_DIR)/include/cbc/coin/CbcModel.hpp $(WINDOWS_CBC_DIR)/include/coin/CbcModel.hpp),)
	$(error Third party Cbc files was not found! did you run 'make third_party' or set WINDOWS_CBC_DIR ?)
else
	@echo CBC: found
endif
endif  # USE_COINOR
ifeq ($(USE_SCIP),OFF)
	@echo SCIP: disabled
else
ifeq ($(wildcard $(WINDOWS_SCIP_DIR)/include/scip/scip.h),)
	$(error Third party SCIP files was not found! did you run 'make third_party' or set WINDOWS_SCIP_DIR ?)
else
	@echo SCIP: found
endif
endif  # USE_SCIP
ifndef WINDOWS_CPLEX_DIR
	@echo CPLEX: not found
else
	@echo CPLEX: found
endif
ifndef WINDOWS_GLPK_DIR
	@echo GLPK: not found
else
	@echo GLPK: found
endif
ifndef WINDOWS_XPRESS_DIR
	@echo XPRESS: not found
else
	@echo XPRESS: found
endif

	$(TOUCH) dependencies\check.log

.PHONY: build_third_party
build_third_party: \
 Makefile.local \
 install_deps_directories \
 install_zlib \
 install_protobuf \
 install_absl \
 install_swig \
 install_coin_cbc \
 build_scip

download_third_party: \
 dependencies/archives/zlib$(ZLIB_ARCHIVE_TAG).zip \
 dependencies/sources/protobuf/autogen.sh \
 dependencies/archives/swigwin-$(SWIG_TAG).zip \
 dependencies/sources/Cbc-$(CBC_TAG)

# Directories
.PHONY: install_deps_directories
install_deps_directories: \
 dependencies/install/bin \
 dependencies/install/lib/coin \
 dependencies/install/include/coin

dependencies/install:
	$(MKDIR_P) dependencies$Sinstall

dependencies/install/bin: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sbin

dependencies/install/lib: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Slib

dependencies/install/lib/coin: dependencies/install/lib
	$(MKDIR_P) dependencies$Sinstall$Slib$Scoin

dependencies/install/include: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sinclude

dependencies/install/include/coin: dependencies/install/include
	$(MKDIR_P) dependencies$Sinstall$Sinclude$Scoin

######################
##  Makefile.local  ##
######################
# Make sure that local file lands correctly across platforms
Makefile.local: makefiles/Makefile.third_party.$(SYSTEM).mk
	-$(DEL) Makefile.local
	@echo $(SELECTED_PATH_TO_JDK)>> Makefile.local
	@echo $(SELECTED_PATH_TO_PYTHON)>> Makefile.local
	@echo # >> Makefile.local
	@echo ## OPTIONAL DEPENDENCIES ## >> Makefile.local
	@echo # Define WINDOWS_CPLEX_DIR to point to a installation directory of the CPLEX Studio >> Makefile.local
	@echo #   e.g.: WINDOWS_CPLEX_DIR = C:\Progra~1\CPLEX_STUDIO2010\IBM\ILOG\CPLEX_STUDIO2010 >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_CPLEX_VERSION to specify the suffix of the library >> Makefile.local
	@echo #   e.g.: WINDOWS_CPLEX_VERSION = 2010 >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_XPRESS_DIR to point to a installation directory of the XPRESS-MP >> Makefile.local
	@echo #   e.g.: WINDOWS_XPRESS_DIR = C:\xpressmp>> Makefile.local
	@echo # >> Makefile.local
	@echo # SCIP is enabled and built-in by default. >> Makefile.local
	@echo # To disable support, uncomment the following line: >> Makefile.local
	@echo # USE_SCIP = OFF >> Makefile.local
	@echo # >> Makefile.local
	@echo # By default, make third_party will download SCIP and compile it locally. >> Makefile.local
	@echo # To override this behavior, define WINDOWS_SCIP_DIR to point to an installation>> Makefile.local
	@echo # directory of the scip binary packaged to use it >> Makefile.local
	@echo #   e.g.: WINDOWS_SCIP_DIR = C:\Progra~1\SCIPOP~1.2 >> Makefile.local
	@echo #   note: You can use: 'dir "%ProgramFiles%\SCIPOp*" /x' to find the shortname >> Makefile.local
	@echo # >> Makefile.local
	@echo # Coin OR solvers (CLP, CBC) are enabled and built-in by default. >> Makefile.local
	@echo # To disable support, uncomment the following line: >> Makefile.local
	@echo # USE_COINOR = OFF >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_CLP_DIR, WINDOWS_CBC_DIR if you wish to use a custom version >> Makefile.local
	@echo #   e.g.: WINDOWS_CBC_DIR = C:\Progra~1\CoinOR\Cbc >> Makefile.local
	@echo #         WINDOWS_CLP_DIR = C:\Progra~1\CoinOR\Clp >> Makefile.local
	@echo # >> Makefile.local
	@echo ## REQUIRED DEPENDENCIES ## >> Makefile.local
	@echo # By default they will be automatically built -> nothing to define >> Makefile.local
	@echo # Define WINDOWS_PROTOBUF_DIR to depend on external Protobuf library >> Makefile.local
	@echo #   e.g.: WINDOWS_PROTOBUF_DIR = C:\Progra~1\Protobuf >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_ZLIB_DIR, WINDOWS_ZLIB_NAME >> Makefile.local
	@echo #   e.g.: WINDOWS_ZLIB_DIR = C:\Progra~1\zlib >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_SWIG_BINARY for using a custom swig binary >> Makefile.local
	@echo #   e.g.: WINDOWS_SWIG_BINARY = C:\Progra~1\swigwin >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define absolute paths without trailing "\". E.g. "C:\Foo\Bar" >> Makefile.local
	@echo # Paths must be without spaces, try to use 'dir "directory*" /x' to get the shortname without space of each directory >> Makefile.local
	@echo #   e.g. dir "%ProgramFiles%*" /x >> Makefile.local

############
##  ZLIB  ##
############
# Install zlib
.PHONY: install_zlib
install_zlib: dependencies/install/include/zlib.h dependencies/install/include/zconf.h dependencies/install/lib/zlib.lib

dependencies/install/include/zlib.h: dependencies/sources/zlib-$(ZLIB_TAG)/zlib.h | dependencies/install/include
	$(COPY) dependencies$Ssources$Szlib-$(ZLIB_TAG)$Szlib.h dependencies$Sinstall$Sinclude$Szlib.h

dependencies/install/include/zconf.h: dependencies/sources/zlib-$(ZLIB_TAG)/zlib.h | dependencies/install/include
	$(COPY) dependencies$Ssources$Szlib-$(ZLIB_TAG)$Szconf.h dependencies$Sinstall$Sinclude$Szconf.h

dependencies/install/lib/zlib.lib: dependencies/sources/zlib-$(ZLIB_TAG)/zlib.h | dependencies/install/lib
	cd dependencies$Ssources$Szlib-$(ZLIB_TAG) && set MAKEFLAGS= && nmake -f win32$SMakefile.msc zlib.lib
	$(COPY) dependencies$Ssources$Szlib-$(ZLIB_TAG)$Szlib.lib dependencies$Sinstall$Slib

dependencies/sources/zlib-$(ZLIB_TAG)/zlib.h: dependencies/archives/zlib$(ZLIB_ARCHIVE_TAG).zip
	$(UNZIP) -q -d dependencies$Ssources dependencies$Sarchives$Szlib$(ZLIB_ARCHIVE_TAG).zip
	-$(TOUCH) dependencies$Ssources$Szlib-$(ZLIB_TAG)$Szlib.h

dependencies/archives/zlib$(ZLIB_ARCHIVE_TAG).zip:
	$(WGET) --quiet -P dependencies$Sarchives http://zlib.net/zlib$(ZLIB_ARCHIVE_TAG).zip

ZLIB_INC = /I"$(WINDOWS_ZLIB_PATH)\\include"
ZLIB_SWIG = -I"$(WINDOWS_ZLIB_DIR)/include"

ZLIB_LNK = "$(WINDOWS_ZLIB_PATH)\lib\$(WINDOWS_ZLIB_NAME)"

DEPENDENCIES_INC += $(ZLIB_INC)
SWIG_INC += $(ZLIB_SWIG)
DEPENDENCIES_LNK += $(ZLIB_LNK)

################
##  Protobuf  ##
################
# Install protocol buffers.
install_protobuf: dependencies/install/lib/libprotobuf.lib

dependencies/install/lib/libprotobuf.lib: dependencies/sources/protobuf-$(PROTOBUF_TAG) install_zlib
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG) && \
  set MAKEFLAGS= && \
  "$(CMAKE)" -Hcmake -Bbuild_cmake \
    -DCMAKE_PREFIX_PATH=..\..\install \
    -DCMAKE_BUILD_TYPE=Release \
    -Dprotobuf_BUILD_TESTS=OFF \
    -DBUILD_TESTING=OFF \
    -DZLIB_ROOT=..\..\install \
    -DCMAKE_INSTALL_PREFIX=..\..\install \
    -G "NMake Makefiles" && \
  "$(CMAKE)" --build build_cmake --config Release && \
  "$(CMAKE)" --build build_cmake --config Release --target install

dependencies/sources/protobuf-$(PROTOBUF_TAG): | dependencies/sources
	-$(DELREC) dependencies/sources/protobuf-$(PROTOBUF_TAG)
	git clone --quiet -b $(PROTOBUF_TAG) https://github.com/protocolbuffers/protobuf.git dependencies\sources\protobuf-$(PROTOBUF_TAG)
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG) && git apply "$(OR_TOOLS_TOP)\patches\protobuf-$(PROTOBUF_TAG).patch"

PROTOBUF_INC = /I"$(WINDOWS_PROTOBUF_PATH)\\include"
PROTOBUF_SWIG = -I"$(WINDOWS_PROTOBUF_DIR)/include"
PROTOBUF_PROTOC_INC = -I"$(WINDOWS_PROTOBUF_DIR)/include"
DYNAMIC_PROTOBUF_LNK = "$(WINDOWS_PROTOBUF_PATH)\lib\libprotobuf.lib"
STATIC_PROTOBUF_LNK = "$(WINDOWS_PROTOBUF_PATH)\lib\libprotobuf.lib"

PROTOBUF_LNK = $(STATIC_PROTOBUF_LNK)

DEPENDENCIES_INC += $(PROTOBUF_INC)
SWIG_INC += $(PROTOBUF_SWIG)
DEPENDENCIES_LNK += $(PROTOBUF_LNK)

# Install Java protobuf
#  - Compile generic message proto.
#  - Compile duration.proto
dependencies/install/lib/protobuf.jar: | dependencies/install/lib/libprotobuf.lib
	cd dependencies\\sources\\protobuf-$(PROTOBUF_TAG)\\java && \
	  ..\\..\\..\\install\\bin\\protoc --java_out=core/src/main/java -I../src \
	  ../src/google/protobuf/descriptor.proto
	cd dependencies\\sources\\protobuf-$(PROTOBUF_TAG)\\java && \
	  ..\\..\\..\\install\\bin\\protoc --java_out=core/src/main/java -I../src \
	  ../src/google/protobuf/duration.proto
	cd dependencies\\sources\\protobuf-$(PROTOBUF_TAG)\\java\\core\\src\\main\\java && "$(JAVAC_BIN)" com\\google\\protobuf\\*java
	cd dependencies\\sources\\protobuf-$(PROTOBUF_TAG)\\java\\core\\src\\main\\java && "$(JAR_BIN)" cvf ..\\..\\..\\..\\..\\..\\..\\install\\lib\\protobuf.jar com\\google\\protobuf\\*class

##################
##  ABSEIL-CPP  ##
##################
# This uses abseil-cpp cmake-based build.
.PHONY: install_absl
install_absl: dependencies/install/lib/absl.lib

dependencies/install/lib/absl.lib: dependencies/sources/abseil-cpp-$(ABSL_TAG) | dependencies/install
	cd dependencies\sources\abseil-cpp-$(ABSL_TAG) && \
  set MAKEFLAGS= && \
  "$(CMAKE)" -H. -Bbuild_cmake \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_PREFIX_PATH=..\..\install \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=..\..\install \
    -G "NMake Makefiles" && \
  "$(CMAKE)" --build build_cmake --config Release && \
  "$(CMAKE)" --build build_cmake --config Release --target install

dependencies/sources/abseil-cpp-$(ABSL_TAG): | dependencies/sources
	-$(DELREC) dependencies/sources/abseil-cpp-$(ABSL_TAG)
	git clone --quiet https://github.com/abseil/abseil-cpp.git dependencies\sources\abseil-cpp-$(ABSL_TAG)
	cd dependencies\sources\abseil-cpp-$(ABSL_TAG) && git reset --hard $(ABSL_TAG)
# cd dependencies\sources\abseil-cpp-$(ABSL_TAG) && git apply "$(OR_TOOLS_TOP)\patches\abseil-cpp-$(ABSL_TAG).patch"

ABSL_INC = /I"$(WINDOWS_ABSL_PATH)\\include"
ABSL_SWIG = -I"$(WINDOWS_ABSL_PATH)/include"
# Can't explicitly list all abseil-cpp libraries
# otherwise windows prompt seems to overflow...
STATIC_ABSL_LNK = \
 "$(WINDOWS_ABSL_PATH)\lib\absl_*.lib"
DYNAMIC_ABSL_LNK = $(STATIC_ABSL_LNK)

ABSL_LNK = $(STATIC_ABSL_LNK)

DEPENDENCIES_INC += $(ABSL_INC)
SWIG_INC += $(ABSL_SWIG)
DEPENDENCIES_LNK += $(ABSL_LNK)

############
##  COIN  ##
############
# Install Coin CBC
install_coin_cbc: dependencies/install/bin/cbc.exe

CBC_SRCDIR = dependencies/sources/Cbc-$(CBC_TAG)
dependencies/install/bin/cbc.exe: $(CBC_SRCDIR)/Cbc/MSVisualStudio/v10/$(CBC_PLATFORM)/cbc.exe
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\*.lib dependencies\install\lib\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Cbc\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Clp\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Clp\src\OsiClp\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\CoinUtils\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Cgl\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Osi\src\Osi\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Cbc\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Clp\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\CoinUtils\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Cgl\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Osi\src\Osi\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe dependencies\install\bin

$(CBC_SRCDIR)/Cbc/MSVisualStudio/v10/$(CBC_PLATFORM)/cbc.exe: $(CBC_SRCDIR)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Clp\\MSVisualStudio\\v10\\libOsiClp\\libOsiClp.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Clp\\MSVisualStudio\\v10\\libClp\\libClp.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libOsiCbc\\libOsiCbc.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libCbc\\libCbc.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\cbc\\cbc.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libCbcSolver\\libCbcSolver.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Osi\\MSVisualStudio\\v10\\libOsi\\libOsi.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\CoinUtils\\MSVisualStudio\\v10\\libCoinUtils\\libCoinUtils.vcxproj $(VS_RELEASE)
	tools\win\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cgl\\MSVisualStudio\\v10\\libCgl\\libCgl.vcxproj $(VS_RELEASE)
	$(SED) -i 's/CBC_BUILD;/CBC_BUILD;CBC_THREAD_SAFE;CBC_NO_INTERRUPT;/g' dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libCbcSolver\\libCbcSolver.vcxproj
	cd dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10 && msbuild Cbc.sln /t:cbc /p:Configuration=Release;BuildCmd=ReBuild

$(CBC_SRCDIR): | dependencies/sources
	-$(DELREC) $(CBC_SRCDIR)
	$(UNZIP) -q -d dependencies\sources dependencies\archives\Cbc-$(CBC_TAG).zip

# This is needed to find Coin include files and libraries.
COINUTILS_INC = /I"$(WINDOWS_COINUTILS_PATH)\\include" /I"$(WINDOWS_COINUTILS_PATH)\\include\\coin"
COINUTILS_SWIG = -I"$(WINDOWS_COINUTILS_DIR)/include" -I"$(WINDOWS_COINUTILS_DIR)/include/coin"
DYNAMIC_COINUTILS_LNK = "$(WINDOWS_COINUTILS_PATH)\lib\coin\libCoinUtils.lib"
STATIC_COINUTILS_LNK = "$(WINDOWS_COINUTILS_PATH)\lib\coin\libCoinUtils.lib"
COINUTILS_LNK = $(STATIC_COINUTILS_LNK)

OSI_INC = /I"$(WINDOWS_OSI_PATH)\\include" /I"$(WINDOWS_OSI_PATH)\\include\\coin"
OSI_SWIG = -I"$(WINDOWS_OSI_DIR)/include" -I"$(WINDOWS_OSI_DIR)/include/coin"
DYNAMIC_OSI_LNK = "$(WINDOWS_OSI_PATH)\lib\coin\libOsi.lib"
STATIC_OSI_LNK = "$(WINDOWS_OSI_PATH)\lib\coin\libOsi.lib"
OSI_LNK = $(STATIC_OSI_LNK)

CLP_INC = /I"$(WINDOWS_CLP_PATH)\\include" /I"$(WINDOWS_CLP_PATH)\\include\\coin" /DUSE_CLP
CLP_SWIG = -I"$(WINDOWS_CLP_DIR)/include" -I"$(WINDOWS_CLP_DIR)/include/coin" -DUSE_CLP
DYNAMIC_CLP_LNK = \
 "$(WINDOWS_CLP_PATH)\lib\coin\libClp.lib" \
 "$(WINDOWS_CLP_PATH)\lib\coin\libOsiClp.lib"
STATIC_CLP_LNK = \
 "$(WINDOWS_CLP_PATH)\lib\coin\libClp.lib" \
 "$(WINDOWS_CLP_PATH)\lib\coin\libOsiClp.lib"
CLP_LNK = $(STATIC_CLP_LNK)

CGL_INC = /I"$(WINDOWS_CGL_PATH)\\include" /I"$(WINDOWS_CGL_PATH)\\include\\coin"
CGL_SWIG = -I"$(WINDOWS_CGL_DIR)/include" -I"$(WINDOWS_CGL_DIR)/include/coin"
DYNAMIC_CGL_LNK = "$(WINDOWS_CGL_PATH)\lib\coin\libCgl.lib"
STATIC_CGL_LNK = "$(WINDOWS_CGL_PATH)\lib\coin\libCgl.lib"
CGL_LNK = $(STATIC_CGL_LNK)

CBC_INC = /I"$(WINDOWS_CBC_PATH)\\include" /I"$(WINDOWS_CBC_PATH)\\include\\coin" /DUSE_CBC
CBC_SWIG = -I"$(WINDOWS_CBC_DIR)/include" -I"$(WINDOWS_CBC_DIR)/include/coin" -DUSE_CBC
DYNAMIC_CBC_LNK = \
 "$(WINDOWS_CBC_PATH)\lib\coin\libCbcSolver.lib" \
 "$(WINDOWS_CBC_PATH)\lib\coin\libCbc.lib"
STATIC_CBC_LNK = \
 "$(WINDOWS_CBC_PATH)\lib\coin\libCbcSolver.lib" \
 "$(WINDOWS_CBC_PATH)\lib\coin\libCbc.lib"
CBC_LNK = $(STATIC_CBC_LNK)

# Agregate all previous coin packages
COIN_INC = \
  $(COINUTILS_INC) \
  $(OSI_INC) \
  $(CLP_INC) \
  $(CGL_INC) \
  $(CBC_INC)
COIN_SWIG = \
  $(COINUTILS_SWIG) \
  $(OSI_SWIG) \
  $(CLP_SWIG) \
  $(CGL_SWIG) \
  $(CBC_SWIG)
COIN_LNK = \
  $(CBC_LNK) \
  $(CGL_LNK) \
  $(CLP_LNK) \
  $(OSI_LNK) \
  $(COINUTILS_LNK)

DEPENDENCIES_INC += $(COIN_INC)
SWIG_INC += $(COIN_SWIG)
DEPENDENCIES_LNK += $(COIN_LNK)

#########################
##  SCIP               ##
#########################
.PHONY: build_scip
ifeq ($(USE_SCIP),OFF)
build_scip: $(GEN_DIR)/ortools/linear_solver/lpi_glop.cc

$(GEN_DIR)/ortools/linear_solver/lpi_glop.cc: | $(GEN_DIR)/ortools/linear_solver
	$(TOUCH) $(GEN_DIR)/ortools/linear_solver/lpi_glop.cc
else
build_scip: dependencies/install/lib/libscip.lib $(GEN_DIR)/ortools/linear_solver/lpi_glop.cc

SCIP_SRCDIR = dependencies/sources/scip-$(SCIP_TAG)
dependencies/install/lib/libscip.lib: $(SCIP_SRCDIR)
	-tools\win\rm -rf $(SCIP_SRCDIR)\build_cmake
	cd dependencies\sources\scip-$(SCIP_TAG) && \
  set MAKEFLAGS= && \
  "$(CMAKE)" -H. -Bbuild_cmake \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_PREFIX_PATH=..\..\install \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DSHARED=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=..\..\install \
    -DREADLINE=OFF \
    -DGMP=OFF \
    -DPAPILO=OFF \
    -DZIMPL=OFF \
    -DIPOPT=OFF \
    -DTPI="tny" \
    -DEXPRINT="none" \
    -DLPS="none" \
    -DSYM="none" \
    -G "NMake Makefiles" && \
  "$(CMAKE)" --build build_cmake --config Release && \
  "$(CMAKE)" --build build_cmake --config Release --target install
	lib /REMOVE:CMakeFiles\libscip.dir\lpi\lpi_none.c.obj $(OR_TOOLS_TOP)\dependencies\install\lib\libscip.lib

$(SCIP_SRCDIR): | dependencies/sources
	-$(DELREC) $(SCIP_SRCDIR)
	-tools\win\gzip.exe -dc dependencies\archives\scip-$(SCIP_TAG).tgz | tools\win\tar.exe -x -v -m -C dependencies\\sources -f -
	cd dependencies\sources\scip-$(SCIP_TAG) && git apply --ignore-whitespace "$(OR_TOOLS_TOP)\patches\scip-$(SCIP_TAG).patch"

$(GEN_DIR)/ortools/linear_solver/lpi_glop.cc: $(SCIP_SRCDIR) | $(GEN_DIR)/ortools/linear_solver
	copy dependencies\sources\scip-$(SCIP_TAG)\src\lpi\lpi_glop.cpp $(GEN_PATH)\ortools\linear_solver\lpi_glop.cc

SCIP_INC = /I"$(WINDOWS_SCIP_PATH)\\include" /DUSE_SCIP /DNO_CONFIG_HEADER
SCIP_SWIG = -I"$(WINDOWS_SCIP_DIR)/include" -DUSE_SCIP -DNO_CONFIG_HEADER
SCIP_LNK = "$(WINDOWS_SCIP_PATH)\lib\libscip.lib"

DEPENDENCIES_INC += $(SCIP_INC)
SWIG_INC += $(SCIP_SWIG)
DEPENDENCIES_LNK += $(SCIP_LNK)
endif

############
##  SWIG  ##
############
# Install SWIG.
install_swig: dependencies/install/swigwin-$(SWIG_TAG)/swig.exe

dependencies/install/swigwin-$(SWIG_TAG)/swig.exe: dependencies/archives/swigwin-$(SWIG_TAG).zip
	$(UNZIP) -q -d dependencies$Sinstall dependencies$Sarchives$Sswigwin-$(SWIG_TAG).zip
	$(TOUCH) dependencies$Sinstall$Sswigwin-$(SWIG_TAG)$Sswig.exe

SWIG_ARCHIVE:=http://prdownloads.sourceforge.net/swig/swigwin-$(SWIG_TAG).zip

dependencies/archives/swigwin-$(SWIG_TAG).zip:
	$(WGET) --quiet -P dependencies$Sarchives --no-check-certificate $(SWIG_ARCHIVE)

# TODO: TBD: Don't know if this is a ubiquitous issue across platforms...
# Handle a couple of extraneous circumstances involving TortoiseSVN caching and .svn readonly attributes.
kill_tortoisesvn_cache:
	$(TASKKILL) /IM "$(TSVNCACHE_EXE)" /F /FI "STATUS eq RUNNING"

remove_readonly_svn_attribs: kill_tortoisesvn_cache
	if exist dependencies\sources\* $(ATTRIB) -r /s dependencies\sources\*

.PHONY: clean_third_party # Clean everything. Remember to also delete archived dependencies, i.e. in the event of download failure, etc.
clean_third_party: remove_readonly_svn_attribs
	-$(DEL) Makefile.local
	-$(DEL) dependencies\check.log
	-$(DEL) dependencies\archives\swigwin*.zip
	-$(DEL) dependencies\archives\sparsehash*.zip
	-$(DEL) dependencies\archives\zlib*.zip
	-$(DEL) dependencies\archives\v*.zip
	-$(DEL) dependencies\archives\win_flex_bison*.zip
	-$(DELREC) dependencies\sources\zlib*
	-$(DELREC) dependencies\sources\protobuf*
	-$(DELREC) dependencies\sources\abseil-cpp*
	-$(DELREC) dependencies\sources\Cbc-*
	-$(DELREC) dependencies\sources\scip*
	-$(DELREC) dependencies\sources\google*
	-$(DELREC) dependencies\sources\glpk*
	-$(DELREC) dependencies\sources\sparsehash*
	-$(DELREC) dependencies\install

.PHONY: detect_third_party # Show variables used to find third party
detect_third_party:
	@echo Relevant info on third party:
	@echo WINDOWS_ZLIB_DIR = $(WINDOWS_ZLIB_DIR)
	@echo ZLIB_INC = $(ZLIB_INC)
	@echo ZLIB_LNK = $(ZLIB_LNK)
	@echo WINDOWS_PROTOBUF_DIR = $(WINDOWS_PROTOBUF_DIR)
	@echo PROTOBUF_INC = $(PROTOBUF_INC)
	@echo PROTOBUF_LNK = $(PROTOBUF_LNK)
	@echo ABSL_INC = $(ABSL_INC)
	@echo ABSL_LNK = $(ABSL_LNK)
	@echo WINDOWS_CBC_DIR = $(WINDOWS_CBC_DIR)
	@echo CBC_INC = $(CBC_INC)
	@echo CBC_LNK = $(CBC_LNK)
	@echo WINDOWS_CLP_DIR = $(WINDOWS_CLP_DIR)
	@echo CLP_INC = $(CLP_INC)
	@echo CLP_LNK = $(CLP_LNK)
	@echo WINDOWS_SCIP_DIR = $(WINDOWS_SCIP_DIR)
	@echo SCIP_INC = $(SCIP_INC)
	@echo SCIP_LNK = $(SCIP_LNK)
ifdef WINDOWS_GLPK_DIR
	@echo WINDOWS_GLPK_DIR = $(WINDOWS_GLPK_DIR)
	@echo GLPK_INC = $(GLPK_INC)
	@echo DYNAMIC_GLPK_LNK = $(DYNAMIC_GLPK_LNK)
	@echo STATIC_GLPK_LNK = $(STATIC_GLPK_LNK)
endif
ifdef WINDOWS_CPLEX_DIR
	@echo WINDOWS_CPLEX_DIR = $(WINDOWS_CPLEX_DIR)
	@echo CPLEX_INC = $(CPLEX_INC)
	@echo DYNAMIC_CPLEX_LNK = $(DYNAMIC_CPLEX_LNK)
	@echo STATIC_CPLEX_LNK = $(STATIC_CPLEX_LNK)
endif
ifdef WINDOWS_XPRESS_DIR
	@echo WINDOWS_XPRESS_DIR = $(WINDOWS_XPRESS_DIR)
	@echo XPRESS_INC = $(XPRESS_INC)
	@echo XPRESS_LNK = $(XPRESS_LNK)
endif
	@echo off & echo(
