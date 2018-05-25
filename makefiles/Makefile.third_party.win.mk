.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@tools\grep.exe "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.win.mk | tools\sed.exe "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(

# tags of dependencies to checkout.
GFLAGS_TAG = 2.2.1
PROTOBUF_TAG = 3.5.1
GLOG_TAG = 0.3.5
CBC_TAG = 2.9.9
ZLIB_TAG = 1.2.11
ZLIB_ARCHIVE_TAG = 1211
SWIG_TAG = 3.0.12

# Added in support of clean third party targets
TSVNCACHE_EXE = TSVNCache.exe

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party: build_third_party makefile_third_party

.PHONY: third_party_check # Check if "make third_party" have been run or not
third_party_check:
ifeq ($(wildcard $(WINDOWS_ZLIB_DIR)/include/zlib.h),)
	$(error Third party ZLIB files was not found! did you run 'make third_party' or set WINDOWS_ZLIB_DIR ?)
endif
ifeq ($(wildcard $(WINDOWS_GFLAGS_DIR)/include/gflags/gflags.h),)
	$(error Third party GFlags files was not found! did you run 'make third_party' or set WINDOWS_GFLAGS_DIR ?)
endif
ifeq ($(wildcard $(WINDOWS_GLOG_DIR)/include/glog/logging.h),)
	$(error Third party GLog files was not found! did you run 'make third_party' or set WINDOWS_GLOG_DIR ?)
endif
ifeq ($(wildcard $(WINDOWS_PROTOBUF_DIR)/include/google/protobuf/descriptor.h),)
	$(error Third party Protobuf files was not found! did you run 'make third_party' or set WINDOWS_PROTOBUF_DIR ?)
endif
ifeq ($(wildcard $(WINDOWS_CBC_DIR)/include/coin/CbcModel.hpp),)
	$(error Third party Cbc files was not found! did you run 'make third_party' or set WINDOWS_CBC_DIR ?)
endif
ifeq ($(wildcard $(WINDOWS_CLP_DIR)/include/coin/ClpSimplex.hpp),)
	$(error Third party Clp files was not found! did you run 'make third_party' or set WINDOWS_CLP_DIR ?)
endif

.PHONY: build_third_party
build_third_party: \
 install_directories \
 archives_directory \
 install_zlib \
 install_gflags \
 install_glog \
 install_protobuf \
 install_swig \
 install_coin_cbc

download_third_party: \
 dependencies/archives/zlib$(ZLIB_ARCHIVE_TAG).zip \
 dependencies/sources/gflags/autogen.sh \
 dependencies/sources/protobuf/autogen.sh \
 dependencies/archives/swigwin-$(SWIG_TAG).zip \
 dependencies/sources/Cbc-$(CBC_TAG)/configure

# Directories
.PHONY: install_directories
install_directories: dependencies/install/bin dependencies/install/lib/coin dependencies/install/include/coin

dependencies/install/bin: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sbin

dependencies/install/lib: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Slib

dependencies/install/lib/coin: dependencies/install/lib
	$(MKDIR_P) dependencies$Sinstall$Slib$Scoin

dependencies/install:
	$(MKDIR_P) dependencies$Sinstall

dependencies/install/include: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sinclude

dependencies/install/include/coin: dependencies/install/include
	$(MKDIR_P) dependencies$Sinstall$Sinclude$Scoin

.PHONY: archives_directory
archives_directory:
	-$(MKDIR_P) dependencies$Sarchives

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
	tools\unzip -q -d dependencies$Ssources dependencies$Sarchives$Szlib$(ZLIB_ARCHIVE_TAG).zip
	-$(TOUCH) dependencies$Ssources$Szlib-$(ZLIB_TAG)$Szlib.h

dependencies/archives/zlib$(ZLIB_ARCHIVE_TAG).zip:
	tools\wget --quiet -P dependencies$Sarchives http://zlib.net/zlib$(ZLIB_ARCHIVE_TAG).zip

install_gflags: dependencies/install/lib/gflags.lib

dependencies/install/lib/gflags.lib: dependencies/sources/gflags-$(GFLAGS_TAG)/INSTALL.md
	-mkdir dependencies\sources\gflags-$(GFLAGS_TAG)\build_cmake
	cd dependencies\sources\gflags-$(GFLAGS_TAG)\build_cmake && set MAKEFLAGS= && \
	  "$(CMAKE)" -D CMAKE_INSTALL_PREFIX=..\..\..\install \
	           -D CMAKE_BUILD_TYPE=Release \
	           -G "NMake Makefiles" \
	           ..
	cd dependencies\sources\gflags-$(GFLAGS_TAG)\build_cmake && set MAKEFLAGS= && \
	nmake install
	$(TOUCH) dependencies/install/lib/gflags_static.lib

dependencies/sources/gflags-$(GFLAGS_TAG)/INSTALL.md: dependencies/archives/gflags-$(GFLAGS_TAG).zip
	tools\unzip -q -d dependencies/sources dependencies\archives\gflags-$(GFLAGS_TAG).zip
	-$(TOUCH) dependencies\sources\gflags-$(GFLAGS_TAG)\INSTALL.md

dependencies/archives/gflags-$(GFLAGS_TAG).zip:
	tools\wget --quiet -P dependencies\archives --no-check-certificate https://github.com/gflags/gflags/archive/v$(GFLAGS_TAG).zip
	cd dependencies/archives && rename v$(GFLAGS_TAG).zip gflags-$(GFLAGS_TAG).zip


# Install protocol buffers.
install_protobuf: dependencies\install\bin\protoc.exe  dependencies\install\include\google\protobuf\message.h

dependencies\install\bin\protoc.exe: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe
	copy dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe dependencies\install\bin
	copy dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\*.lib dependencies\install\lib

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && msbuild protobuf.sln /t:Build /p:Configuration=Release;LinkIncremental=false

dependencies\install\include\google\protobuf\message.h: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar
	cd dependencies\install && ..\..\tools\tar.exe xvmf ..\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar
#	copy dependencies\sources\protobuf-$(PROTOBUF_TAG)\src\google\protobuf-$(PROTOBUF_TAG)\stubs\stl_util.h dependencies\install\include\google\protobuf-$(PROTOBUF_TAG)\stubs

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && extract_includes.bat
# workaround waiting for https://github.com/google/protobuf/pull/4538
	$(COPY) dependencies\sources\protobuf-$(PROTOBUF_TAG)\src\google\protobuf\*.proto dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include\google\protobuf
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && ..\..\..\..\..\tools\tar.exe cf include.tar include

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt
	-${MKDIR} dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build
	${SED} -i -e '/\"\/MD\"/d' dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && "$(CMAKE)" -G $(CMAKE_PLATFORM) -Dprotobuf_BUILD_TESTS=OFF ..

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt:
	tools\wget --quiet -P dependencies\archives --no-check-certificate https://github.com/google/protobuf/archive/v$(PROTOBUF_TAG).zip
	tools\unzip -q -d dependencies\sources dependencies\archives\v$(PROTOBUF_TAG).zip

install_glog: dependencies/install/include/glog/logging.h

dependencies/install/include/glog/logging.h: dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt install_gflags
	-md dependencies\sources\glog-$(GLOG_TAG)\build_cmake
	cd dependencies\sources\glog-$(GLOG_TAG)\build_cmake && \
	  "$(CMAKE)" -D CMAKE_INSTALL_PREFIX=..\..\..\install \
	           -D CMAKE_BUILD_TYPE=Release \
	           -D CMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)\dependencies\install" \
	           -G "NMake Makefiles" \
	           ..
	cd dependencies\sources\glog-$(GLOG_TAG)\build_cmake && set MAKEFLAGS= && nmake install
	$(TOUCH) dependencies/install/lib/glog_static.lib

dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt: dependencies/archives/glog-$(GLOG_TAG).zip
	tools\unzip -q -d dependencies/sources dependencies\archives\glog-$(GLOG_TAG).zip
	-$(TOUCH) dependencies\sources\glog-$(GLOG_TAG)\CMakeLists.txt

dependencies/archives/glog-$(GLOG_TAG).zip:
	tools\wget --quiet -P dependencies\archives --no-check-certificate https://github.com/google/glog/archive/v$(GLOG_TAG).zip
	cd dependencies/archives && rename v$(GLOG_TAG).zip glog-$(GLOG_TAG).zip

# Install Coin CBC.
install_coin_cbc: dependencies\install\bin\cbc.exe

dependencies\install\bin\cbc.exe: dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe
	copy dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\*.lib dependencies\install\lib\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Cbc\src\*.hpp dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Clp\src\*.hpp dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Clp\src\OsiClp\*.hpp dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\CoinUtils\src\*.hpp dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Cgl\src\*.hpp dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Osi\src\Osi\*.hpp dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Cbc\src\*.h dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Clp\src\*.h dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\CoinUtils\src\*.h dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Cgl\src\*.h dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Osi\src\Osi\*.h dependencies\install\include\coin
	copy dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe dependencies\install\bin

dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe: dependencies\sources\Cbc-$(CBC_TAG)\configure
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Clp\\MSVisualStudio\\v10\\libOsiClp\\libOsiClp.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Clp\\MSVisualStudio\\v10\\libClp\\libClp.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libOsiCbc\\libOsiCbc.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libCbc\\libCbc.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\cbc\\cbc.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libCbcSolver\\libCbcSolver.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Osi\\MSVisualStudio\\v10\\libOsi\\libOsi.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\CoinUtils\\MSVisualStudio\\v10\\libCoinUtils\\libCoinUtils.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies\\sources\\Cbc-$(CBC_TAG)\\Cgl\\MSVisualStudio\\v10\\libCgl\\libCgl.vcxproj $(VS_RELEASE)
	tools\sed -i 's/CBC_BUILD;/CBC_BUILD;CBC_THREAD_SAFE;CBC_NO_INTERRUPT;/g' dependencies\\sources\\Cbc-$(CBC_TAG)\\Cbc\\MSVisualStudio\\v10\\libCbcSolver\\libCbcSolver.vcxproj
	cd dependencies\sources\Cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10 && msbuild Cbc.sln /t:cbc /p:Configuration=Release;BuildCmd=ReBuild

CBC_ARCHIVE:=https://www.coin-or.org/download/source/Cbc/Cbc-${CBC_TAG}.zip

dependencies\sources\Cbc-$(CBC_TAG)\configure:
	tools\wget --quiet --continue -P dependencies\archives --no-check-certificate ${CBC_ARCHIVE} || (@echo wget failed to dowload $(CBC_ARCHIVE), try running 'tools\wget -P dependencies\archives --no-check-certificate $(CBC_ARCHIVE)' then rerun 'make third_party' && exit 1)
	tools\unzip -q -d dependencies\sources dependencies\archives\Cbc-$(CBC_TAG).zip

# Install SWIG.
install_swig: dependencies/install/swigwin-$(SWIG_TAG)/swig.exe

dependencies/install/swigwin-$(SWIG_TAG)/swig.exe: dependencies/archives/swigwin-$(SWIG_TAG).zip
	tools$Sunzip -q -d dependencies$Sinstall dependencies$Sarchives$Sswigwin-$(SWIG_TAG).zip
	tools$Stouch dependencies$Sinstall$Sswigwin-$(SWIG_TAG)$Sswig.exe

SWIG_ARCHIVE:=https://superb-dca2.dl.sourceforge.net/project/swig/swigwin/swigwin-$(SWIG_TAG)/swigwin-$(SWIG_TAG).zip

dependencies/archives/swigwin-$(SWIG_TAG).zip:
	tools$Swget --quiet -P dependencies$Sarchives --no-check-certificate $(SWIG_ARCHIVE)

# Install Java protobuf
dependencies/install/lib/protobuf.jar: dependencies/install/bin/protoc.exe
	cd dependencies\\sources\\protobuf-$(PROTOBUF_TAG)\\java && \
	  ..\\..\\..\\install\\bin\\protoc --java_out=core/src/main/java -I../src \
	  ../src/google/protobuf/descriptor.proto
	cd dependencies\\sources\\protobuf-$(PROTOBUF_TAG)\\java\\core\\src\\main\\java && $(JAVAC_BIN) com\\google\\protobuf\\*java
	cd dependencies\\sources\\protobuf-$(PROTOBUF_TAG)\\java\\core\\src\\main\\java && $(JAR_BIN) cvf ..\\..\\..\\..\\..\\..\\..\\install\\lib\\protobuf.jar com\\google\\protobuf\\*class

# TODO: TBD: Don't know if this is a ubiquitous issue across platforms...
# Handle a couple of extraneous circumstances involving TortoiseSVN caching and .svn readonly attributes.
kill_tortoisesvn_cache:
	$(TASKKILL) /IM "$(TSVNCACHE_EXE)" /F /FI "STATUS eq RUNNING"

remove_readonly_svn_attribs: kill_tortoisesvn_cache
	if exist dependencies\sources\* $(ATTRIB) -r /s dependencies\sources\*

.PHONY: clean_third_party # Clean everything. Remember to also delete archived dependencies, i.e. in the event of download failure, etc.
clean_third_party: remove_readonly_svn_attribs
	-$(DEL) Makefile.local
	-$(DEL) dependencies\archives\swigwin*.zip
	-$(DEL) dependencies\archives\Cbc*
	-$(DEL) dependencies\archives\gflags*.zip
	-$(DEL) dependencies\archives\sparsehash*.zip
	-$(DEL) dependencies\archives\zlib*.zip
	-$(DEL) dependencies\archives\v*.zip
	-$(DEL) dependencies\archives\win_flex_bison*.zip
	-$(DELREC) dependencies\archives
	-$(DELREC) dependencies\sources\Cbc-*
	-$(DELREC) dependencies\sources\gflags*
	-$(DELREC) dependencies\sources\glpk*
	-$(DELREC) dependencies\sources\google*
	-$(DELREC) dependencies\sources\protobuf*
	-$(DELREC) dependencies\sources\sparsehash*
	-$(DELREC) dependencies\sources\zlib*
	-$(DELREC) dependencies\install

# Create Makefile.local
.PHONY: makefile_third_party
makefile_third_party: Makefile.local

# Make sure that local file lands correctly across platforms
Makefile.local: makefiles/Makefile.third_party.$(SYSTEM).mk
	-$(DEL) Makefile.local
	@echo $(SELECTED_PATH_TO_JDK)>> Makefile.local
	@echo $(SELECTED_PATH_TO_PYTHON)>> Makefile.local
	@echo $(SELECTED_CSC_BINARY)>> Makefile.local
	@echo $(SELECTED_DOTNET_BINARY)>> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_SCIP_DIR to point to a compiled version of SCIP to use it >> Makefile.local
	@echo #   i.e.: path\\scip-4.0.0 >> Makefile.local
	@echo # See instructions here: >> Makefile.local
	@echo #   http://or-tools.blogspot.com/2017/03/changing-way-we-link-with-scip.html >> Makefile.local
	@echo CLR_KEYFILE = bin\\or-tools.snk >> Makefile.local
	@echo # Define WINDOWS_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_ZLIB_DIR, WINDOWS_ZLIB_NAME, WINDOWS_GFLAGS_DIR, >> Makefile.local
	@echo # WINDOWS_PROTOBUF_DIR, WINDOWS_GLOG_DIR, WINDOWS_SWIG_BINARY, >> Makefile.local
	@echo # WINDOWS_CLP_DIR, WINDOWS_CBC_DIR if you wish to use a custom version >> Makefile.local

.PHONY: detect_third_party # Show variables used to find third party
detect_third_party:
	@echo Relevant info on third party:
	@echo ZLIB_INC = $(ZLIB_INC)
	@echo WINDOWS_GFLAGS_DIR = $(WINDOWS_GFLAGS_DIR)
	@echo GFLAGS_INC = $(GFLAGS_INC)
	@echo GFLAGS_LNK = $(GFLAGS_LNK)
	@echo WINDOWS_GLOG_DIR = $(WINDOWS_GLOG_DIR)
	@echo GLOG_INC = $(GLOG_INC)
	@echo GLOG_LNK = $(GLOG_LNK)
	@echo WINDOWS_PROTOBUF_DIR = $(WINDOWS_PROTOBUF_DIR)
	@echo PROTOBUF_DIR = $(PROTOBUF_DIR)
	@echo PROTOBUF_INC = $(PROTOBUF_INC)
	@echo PROTOBUF_LNK = $(PROTOBUF_LNK)
	@echo WINDOWS_CBC_DIR = $(WINDOWS_CBC_DIR)
	@echo CBC_INC = $(CBC_INC)
	@echo DYNAMIC_CBC_LNK = $(DYNAMIC_CBC_LNK)
	@echo STATIC_CBC_LNK = $(STATIC_CBC_LNK)
	@echo WINDOWS_CLP_DIR = $(WINDOWS_CLP_DIR)
	@echo CLP_INC = $(CLP_INC)
	@echo DYNAMIC_CLP_LNK = $(DYNAMIC_CLP_LNK)
	@echo STATIC_CLP_LNK = $(STATIC_CLP_LNK)
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
