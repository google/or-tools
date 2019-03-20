.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.win.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(

# Checks if the user has overwritten default libraries and binaries.
WINDOWS_ZLIB_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_ZLIB_PATH = $(subst /,$S,$(WINDOWS_ZLIB_DIR))
WINDOWS_ZLIB_NAME ?= zlib.lib
WINDOWS_GFLAGS_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_GFLAGS_PATH = $(subst /,$S,$(WINDOWS_GFLAGS_DIR))
WINDOWS_GLOG_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_GLOG_PATH = $(subst /,$S,$(WINDOWS_GLOG_DIR))
WINDOWS_PROTOBUF_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_PROTOBUF_PATH = $(subst /,$S,$(WINDOWS_PROTOBUF_DIR))
WINDOWS_ABSL_DIR ?= $(OR_ROOT)dependencies/install
WINDOWS_ABSL_PATH = $(subst /,$S,$(WINDOWS_ABSL_DIR))
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
WINDOWS_SWIG_BINARY ?= "$(OR_ROOT)dependencies\\install\\swigwin-$(SWIG_TAG)\\swig.exe"

# Variable use in others Makefiles
PROTOC = "$(WINDOWS_PROTOBUF_DIR)\\bin\\protoc.exe"
SWIG_BINARY = $(WINDOWS_SWIG_BINARY)

# tags of dependencies to checkout.
ZLIB_TAG = 1.2.11
ZLIB_ARCHIVE_TAG = 1211
GFLAGS_TAG = 2.2.2
GLOG_TAG = 0.3.5
PROTOBUF_TAG = 3.6.1
ABSL_TAG = bf29470
CBC_TAG = 2.9.9
CGL_TAG = 0.59.10
CLP_TAG = 1.16.11
OSI_TAG = 0.107.9
COINUTILS_TAG = 2.10.14
SWIG_TAG = 3.0.12

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
ifeq ($(wildcard $(WINDOWS_GFLAGS_DIR)/include/gflags/gflags.h),)
	$(error Third party GFlags files was not found! did you run 'make third_party' or set WINDOWS_GFLAGS_DIR ?)
else
	@echo GFLAGS: found
endif
ifeq ($(wildcard $(WINDOWS_GLOG_DIR)/include/glog/logging.h),)
	$(error Third party GLog files was not found! did you run 'make third_party' or set WINDOWS_GLOG_DIR ?)
else
	@echo GLOG: found
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
ifeq ($(wildcard $(WINDOWS_CBC_DIR)/include/cbc/coin/CbcModel.hpp $(WINDOWS_CBC_DIR)/include/coin/CbcModel.hpp),)
	$(error Third party Cbc files was not found! did you run 'make third_party' or set WINDOWS_CBC_DIR ?)
else
	@echo CBC: found
endif
ifndef WINDOWS_CPLEX_DIR
	@echo CPLEX: not found
endif
ifndef WINDOWS_GLPK_DIR
	@echo GLPK: not found
endif
ifndef WINDOWS_GUROBI_DIR
	@echo GUROBI: not found
endif
ifndef WINDOWS_SCIP_DIR
	@echo SCIP: not found
else
  ifeq ($(wildcard $(WINDOWS_SCIP_DIR)/include/scip/scip.h),)
	$(error Third party SCIP files was not found! please check the path given to WINDOWS_SCIP_DIR)
  else
	@echo SCIP: found
  endif
endif
	$(TOUCH) dependencies\check.log

.PHONY: build_third_party
build_third_party: \
 Makefile.local \
 archives_directory \
 install_deps_directories \
 install_zlib \
 install_gflags \
 install_glog \
 install_protobuf \
 install_absl \
 install_swig \
 install_coin_cbc

download_third_party: \
 dependencies/archives/zlib$(ZLIB_ARCHIVE_TAG).zip \
 dependencies/sources/gflags/autogen.sh \
 dependencies/sources/protobuf/autogen.sh \
 dependencies/archives/swigwin-$(SWIG_TAG).zip \
 dependencies/sources/Cbc-$(CBC_TAG)/configure

# Directories
.PHONY: archives_directory
archives_directory: dependencies/archives

dependencies/archives:
	$(MKDIR_P) dependencies$Sarchives

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
	@echo # Define WINDOWS_SCIP_DIR to point to a compiled version of SCIP to use it >> Makefile.local
	@echo #   e.g.: WINDOWS_SCIP_DIR = "relative_path/to/scip-4.0.0" >> Makefile.local
	@echo # See instructions here: >> Makefile.local
	@echo #   http://or-tools.blogspot.com/2017/03/changing-way-we-link-with-scip.html >> Makefile.local
	@echo # Define WINDOWS_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_ZLIB_DIR, WINDOWS_ZLIB_NAME, WINDOWS_GFLAGS_DIR, >> Makefile.local
	@echo # WINDOWS_GLOG_DIR, WINDOWS_PROTOBUF_DIR, WINDOWS_SWIG_BINARY, >> Makefile.local
	@echo # WINDOWS_CLP_DIR, WINDOWS_CBC_DIR if you wish to use a custom version >> Makefile.local
	@echo #   e.g.: WINDOWS_GFLAGS_DIR = "relative_path/to/gflags/dir" >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define absolute paths without trailing "\". E.g. "c:\Installs\SCIP-6.0.0" >> Makefile.local

############
##  ZLIB  ##
############
# Install zlib
.PHONY: install_zlib
install_zlib: dependencies/install/include/zlib.h dependencies/install/include/zconf.h dependencies/install/lib/zlib.lib

dependencies/install/include/zlib.h: dependencies/install/include dependencies/sources/zlib-$(ZLIB_TAG)/zlib.h
	$(COPY) dependencies$Ssources$Szlib-$(ZLIB_TAG)$Szlib.h dependencies$Sinstall$Sinclude$Szlib.h

dependencies/install/include/zconf.h: dependencies/install/include dependencies/sources/zlib-$(ZLIB_TAG)/zlib.h
	$(COPY) dependencies$Ssources$Szlib-$(ZLIB_TAG)$Szconf.h dependencies$Sinstall$Sinclude$Szconf.h

dependencies/install/lib/zlib.lib: dependencies/sources/zlib-$(ZLIB_TAG)/zlib.h
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
DEPENDENCIES_LNK += $(ZLIB_LNK)

##############
##  GFLAGS  ##
##############
# This uses gflags cmake-based build.
install_gflags: dependencies/install/lib/gflags.lib

dependencies/install/lib/gflags.lib: dependencies/sources/gflags-$(GFLAGS_TAG)
	cd dependencies\sources\gflags-$(GFLAGS_TAG) && \
  set MAKEFLAGS= && \
  "$(CMAKE)" -H. -Bbuild_cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=..\..\install \
    -G "NMake Makefiles" && \
  "$(CMAKE)" --build build_cmake && \
  "$(CMAKE)" --build build_cmake --target install

dependencies/sources/gflags-$(GFLAGS_TAG): dependencies/archives/gflags-$(GFLAGS_TAG).zip | dependencies/sources
	$(UNZIP) -q -d dependencies/sources dependencies\archives\gflags-$(GFLAGS_TAG).zip
	-$(TOUCH) dependencies\sources\gflags-$(GFLAGS_TAG)\INSTALL.md

dependencies/archives/gflags-$(GFLAGS_TAG).zip:
	$(WGET) --quiet -P dependencies\archives --no-check-certificate https://github.com/gflags/gflags/archive/v$(GFLAGS_TAG).zip
	cd dependencies/archives && rename v$(GFLAGS_TAG).zip gflags-$(GFLAGS_TAG).zip

GFLAGS_INC = /I"$(WINDOWS_GFLAGS_PATH)\\include" /DGFLAGS_DLL_DECL= /DGFLAGS_DLL_DECLARE_FLAG= /DGFLAGS_DLL_DEFINE_FLAG=
GFLAGS_SWIG = -I"$(WINDOWS_GFLAGS_DIR)/include" -DGFLAGS_DLL_DECL= -DGFLAGS_DLL_DECLARE_FLAG= -DGFLAGS_DLL_DEFINE_FLAG=
DYNAMIC_GFLAGS_LNK = "$(WINDOWS_GFLAGS_PATH)\lib\gflags_static.lib"
STATIC_GFLAGS_LNK = "$(WINDOWS_GFLAGS_PATH)\lib\gflags_static.lib"

GFLAGS_LNK = $(STATIC_GFLAGS_LNK)
DEPENDENCIES_LNK += $(GFLAGS_LNK)

############
##  GLOG  ##
############
# This uses glog cmake-based build.
install_glog: dependencies/install/lib/glog.lib

dependencies/install/lib/glog.lib: dependencies/sources/glog-$(GLOG_TAG) install_gflags
	cd dependencies\sources\glog-$(GLOG_TAG) && \
  set MAKEFLAGS= && \
  "$(CMAKE)" -H. -Bbuild_cmake \
    -DCMAKE_PREFIX_PATH=..\..\install \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=..\..\install \
    -G "NMake Makefiles" && \
  "$(CMAKE)" --build build_cmake && \
  "$(CMAKE)" --build build_cmake --target install

dependencies/sources/glog-$(GLOG_TAG): dependencies/archives/glog-$(GLOG_TAG).zip | dependencies/sources
	$(UNZIP) -q -d dependencies/sources dependencies\archives\glog-$(GLOG_TAG).zip
	-$(TOUCH) dependencies\sources\glog-$(GLOG_TAG)\CMakeLists.txt

dependencies/archives/glog-$(GLOG_TAG).zip:
	$(WGET) --quiet -P dependencies\archives --no-check-certificate https://github.com/google/glog/archive/v$(GLOG_TAG).zip
	cd dependencies/archives && rename v$(GLOG_TAG).zip glog-$(GLOG_TAG).zip

GLOG_INC = /I"$(WINDOWS_GLOG_PATH)\\include" /DGOOGLE_GLOG_DLL_DECL=
GLOG_SWIG = -I"$(WINDOWS_GLOG_DIR)/include" -DGOOGLE_GLOG_DLL_DECL=
DYNAMIC_GLOG_LNK = "$(WINDOWS_GLOG_PATH)\lib\glog.lib"
STATIC_GLOG_LNK = "$(WINDOWS_GLOG_PATH)\lib\glog.lib"

GLOG_LNK = $(STATIC_GLOG_LNK)
DEPENDENCIES_LNK += $(GLOG_LNK)

################
##  Protobuf  ##
################
# Install protocol buffers.
install_protobuf: dependencies\install\bin\protoc.exe  dependencies\install\include\google\protobuf\message.h

dependencies\install\bin\protoc.exe: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe
	copy dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe dependencies\install\bin
	copy dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\*.lib dependencies\install\lib

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && msbuild protobuf.sln /t:Build /p:Configuration=Release;LinkIncremental=false

dependencies\install\include\google\protobuf\message.h: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar
	cd dependencies\install && ..\..\$(TAR) xvmf ..\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar
#	copy dependencies\sources\protobuf-$(PROTOBUF_TAG)\src\google\protobuf-$(PROTOBUF_TAG)\stubs\stl_util.h dependencies\install\include\google\protobuf-$(PROTOBUF_TAG)\stubs

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && extract_includes.bat
# workaround waiting for https://github.com/google/protobuf/pull/4538
	$(COPY) dependencies\sources\protobuf-$(PROTOBUF_TAG)\src\google\protobuf\*.proto dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include\google\protobuf
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && ..\..\..\..\..\$(TAR) cf include.tar include

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt
	-${MKDIR} dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build
	${SED} -i -e '/\"\/MD\"/d' dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && "$(CMAKE)" -G $(CMAKE_PLATFORM) -Dprotobuf_BUILD_TESTS=OFF ..

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt:
	$(WGET) --quiet -P dependencies\archives --no-check-certificate https://github.com/google/protobuf/archive/v$(PROTOBUF_TAG).zip
	$(UNZIP) -q -d dependencies\sources dependencies\archives\v$(PROTOBUF_TAG).zip

PROTOBUF_INC = /I"$(WINDOWS_PROTOBUF_PATH)\\include"
PROTOBUF_SWIG = -I"$(WINDOWS_PROTOBUF_DIR)/include"
PROTOBUF_PROTOC_INC = -I"$(WINDOWS_PROTOBUF_DIR)/include"
DYNAMIC_PROTOBUF_LNK = "$(WINDOWS_PROTOBUF_PATH)\lib\libprotobuf.lib"
STATIC_PROTOBUF_LNK = "$(WINDOWS_PROTOBUF_PATH)\lib\libprotobuf.lib"

PROTOBUF_LNK = $(STATIC_PROTOBUF_LNK)
DEPENDENCIES_LNK += $(PROTOBUF_LNK)

# Install Java protobuf
#  - Compile generic message proto.
#  - Compile duration.proto
dependencies/install/lib/protobuf.jar: | dependencies/install/bin/protoc.exe
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
    -DCMAKE_PREFIX_PATH=..\..\install \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=..\..\install \
    -G "NMake Makefiles" && \
  "$(CMAKE)" --build build_cmake && \
  "$(CMAKE)" --build build_cmake --target install

dependencies/sources/abseil-cpp-$(ABSL_TAG): | dependencies/sources
	-$(DELREC) dependencies/sources/abseil-cpp-$(ABSL_TAG)
	git clone --quiet https://github.com/abseil/abseil-cpp.git dependencies\sources\abseil-cpp-$(ABSL_TAG)
	cd dependencies\sources\abseil-cpp-$(ABSL_TAG) && git reset --hard $(ABSL_TAG)
	cd dependencies\sources\abseil-cpp-$(ABSL_TAG) && git apply "$(OR_TOOLS_TOP)\patches\abseil-cpp-$(ABSL_TAG).patch"

ABSL_INC = /I"$(WINDOWS_ABSL_PATH)\\include"
ABSL_SWIG = -I"$(WINDOWS_ABSL_PATH)/include"
# Can't explicitly list all abseil-cpp libraries
# otherwise windows prompt seems to overflow...
STATIC_ABSL_LNK = \
 "$(WINDOWS_ABSL_PATH)\lib\absl_*.lib"
DYNAMIC_ABSL_LNK = $(STATIC_ABSL_LNK)

ABSL_LNK = $(STATIC_ABSL_LNK)
DEPENDENCIES_LNK += $(ABSL_LNK)

############
##  COIN  ##
############
# Install Coin CBC
install_coin_cbc: dependencies\install\bin\cbc.exe

dependencies\install\bin\cbc.exe: dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe
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

dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe: dependencies\sources\Cbc-$(CBC_TAG)\configure
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

CBC_ARCHIVE:=https://www.coin-or.org/download/source/Cbc/Cbc-${CBC_TAG}.zip

dependencies\sources\Cbc-$(CBC_TAG)\configure:
	$(WGET) --quiet --continue -P dependencies\archives --no-check-certificate ${CBC_ARCHIVE} || (@echo wget failed to dowload $(CBC_ARCHIVE), try running '$(WGET) -P dependencies\archives --no-check-certificate $(CBC_ARCHIVE)' then rerun 'make third_party' && exit 1)
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

DEPENDENCIES_LNK += $(COIN_LNK)

############
##  SWIG  ##
############
# Install SWIG.
install_swig: dependencies/install/swigwin-$(SWIG_TAG)/swig.exe

dependencies/install/swigwin-$(SWIG_TAG)/swig.exe: dependencies/archives/swigwin-$(SWIG_TAG).zip
	$(UNZIP) -q -d dependencies$Sinstall dependencies$Sarchives$Sswigwin-$(SWIG_TAG).zip
	$(TOUCH) dependencies$Sinstall$Sswigwin-$(SWIG_TAG)$Sswig.exe

SWIG_ARCHIVE:=https://superb-dca2.dl.sourceforge.net/project/swig/swigwin/swigwin-$(SWIG_TAG)/swigwin-$(SWIG_TAG).zip

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
	-$(DEL) dependencies\archives\Cbc*
	-$(DEL) dependencies\archives\gflags*.zip
	-$(DEL) dependencies\archives\sparsehash*.zip
	-$(DEL) dependencies\archives\zlib*.zip
	-$(DEL) dependencies\archives\v*.zip
	-$(DEL) dependencies\archives\win_flex_bison*.zip
	-$(DELREC) dependencies\archives
	-$(DELREC) dependencies\sources\zlib*
	-$(DELREC) dependencies\sources\gflags*
	-$(DELREC) dependencies\sources\glog*
	-$(DELREC) dependencies\sources\protobuf*
	-$(DELREC) dependencies\sources\abseil-cpp*
	-$(DELREC) dependencies\sources\Cbc-*
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
	@echo WINDOWS_GFLAGS_DIR = $(WINDOWS_GFLAGS_DIR)
	@echo GFLAGS_INC = $(GFLAGS_INC)
	@echo GFLAGS_LNK = $(GFLAGS_LNK)
	@echo WINDOWS_GLOG_DIR = $(WINDOWS_GLOG_DIR)
	@echo GLOG_INC = $(GLOG_INC)
	@echo GLOG_LNK = $(GLOG_LNK)
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
ifdef WINDOWS_GLPK_DIR
	@echo WINDOWS_GLPK_DIR = $(WINDOWS_GLPK_DIR)
	@echo GLPK_INC = $(GLPK_INC)
	@echo DYNAMIC_GLPK_LNK = $(DYNAMIC_GLPK_LNK)
	@echo STATIC_GLPK_LNK = $(STATIC_GLPK_LNK)
endif
ifdef WINDOWS_SCIP_DIR
	@echo WINDOWS_SCIP_DIR = $(WINDOWS_SCIP_DIR)
	@echo SCIP_INC = $(SCIP_INC)
	@echo DYNAMIC_SCIP_LNK = $(DYNAMIC_SCIP_LNK)
	@echo STATIC_SCIP_LNK = $(STATIC_SCIP_LNK)
endif
ifdef WINDOWS_CPLEX_DIR
	@echo WINDOWS_CPLEX_DIR = $(WINDOWS_CPLEX_DIR)
	@echo CPLEX_INC = $(CPLEX_INC)
	@echo DYNAMIC_CPLEX_LNK = $(DYNAMIC_CPLEX_LNK)
	@echo STATIC_CPLEX_LNK = $(STATIC_CPLEX_LNK)
endif
ifdef WINDOWS_GUROBI_DIR
	@echo WINDOWS_GUROBI_DIR = $(WINDOWS_GUROBI_DIR)
	@echo GUROBI_INC = $(GUROBI_INC)
	@echo DYNAMIC_GUROBI_LNK = $(DYNAMIC_GUROBI_LNK)
	@echo STATIC_GUROBI_LNK = $(STATIC_GUROBI_LNK)
endif
	@echo off & echo(
