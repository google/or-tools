.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.unix.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo

# Checks if the user has overwritten default libraries and binaries.
UNIX_GFLAGS_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_GLOG_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_PROTOBUF_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_CBC_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_CLP_DIR ?= $(UNIX_CBC_DIR)
UNIX_CGL_DIR ?= $(UNIX_CBC_DIR)
UNIX_OSI_DIR ?= $(UNIX_CBC_DIR)
UNIX_COINUTILS_DIR ?= $(UNIX_CBC_DIR)

# Unix specific definitions
PROTOBUF_DIR = $(UNIX_PROTOBUF_DIR)
UNIX_SWIG_BINARY ?= swig
SWIG_BINARY = $(UNIX_SWIG_BINARY)

# Tags of dependencies to checkout.
GFLAGS_TAG = 2.2.1
PROTOBUF_TAG = 3.5.1
GLOG_TAG = 0.3.5
CBC_TAG = 2.9.9
PATCHELF_TAG = 0.9

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party: makefile_third_party install_third_party

.PHONY: third_party_check # Check if "make third_party" have been run or not
third_party_check:
ifeq ($(wildcard $(UNIX_GFLAGS_DIR)/include/gflags/gflags.h),)
	$(error Third party GFlags files was not found! did you run 'make third_party' or set UNIX_GFLAGS_DIR ?)
endif
ifeq ($(wildcard $(UNIX_GLOG_DIR)/include/glog/logging.h),)
	$(error Third party GLog files was not found! did you run 'make third_party' or set UNIX_GLOG_DIR ?)
endif
ifeq ($(wildcard $(UNIX_PROTOBUF_DIR)/include/google/protobuf/descriptor.h),)
	$(error Third party Protobuf files was not found! did you run 'make third_party' or set UNIX_PROTOBUF_DIR ?)
endif
ifeq ($(wildcard $(UNIX_CBC_DIR)/include/cbc/coin/CbcModel.hpp $(UNIX_CBC_DIR)/include/coin/CbcModel.hpp),)
	$(error Third party Cbc files was not found! did you run 'make third_party' or set UNIX_CBC_DIR ?)
endif
ifeq ($(wildcard $(UNIX_CLP_DIR)/include/clp/coin/ClpSimplex.hpp $(UNIX_CLP_DIR)/include/coin/ClpSimplex.hpp),)
	$(error Third party Clp files was not found! did you run 'make third_party' or set UNIX_CLP_DIR ?)
endif

.PHONY: install_third_party
install_third_party: \
 archives_directory \
 install_directories \
 install_cbc \
 install_gflags \
 install_glog \
 install_protobuf

.PHONY: archives_directory
archives_directory:
	$(MKDIR_P) dependencies$Sarchives

.PHONY: install_directories
install_directories: dependencies/install/bin dependencies/install/lib dependencies/install/include/coin

dependencies/install:
	$(MKDIR_P) dependencies$Sinstall

dependencies/install/bin: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sbin

dependencies/install/lib: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Slib

dependencies/install/include: dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sinclude

dependencies/install/include/coin: dependencies/install/include
	$(MKDIR_P) dependencies$Sinstall$Sinclude$Scoin

##############
##  GFLAGS  ##
##############
# This uses gflags cmake-based build.
install_gflags: dependencies/install/include/gflags/gflags.h

dependencies/install/include/gflags/gflags.h: dependencies/sources/gflags-$(GFLAGS_TAG)/build_cmake/Makefile
	cd dependencies/sources/gflags-$(GFLAGS_TAG)/build_cmake && \
	$(SET_COMPILER) make -j 4 && make install
	touch $@

dependencies/sources/gflags-$(GFLAGS_TAG)/build_cmake/Makefile: dependencies/sources/gflags-$(GFLAGS_TAG)/CMakeLists.txt
	-mkdir dependencies/sources/gflags-$(GFLAGS_TAG)/build_cmake
	cd dependencies/sources/gflags-$(GFLAGS_TAG)/build_cmake && $(SET_COMPILER) \
	$(CMAKE) -D BUILD_SHARED_LIBS=OFF \
           -D BUILD_STATIC_LIBS=ON \
           -D CMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
           -D CMAKE_INSTALL_PREFIX=../../../install \
           ..

dependencies/sources/gflags-$(GFLAGS_TAG)/CMakeLists.txt:
	git clone --quiet -b v$(GFLAGS_TAG) https://github.com/gflags/gflags.git dependencies/sources/gflags-$(GFLAGS_TAG)

GFLAGS_INC = -I$(UNIX_GFLAGS_DIR)/include
STATIC_GFLAGS_LNK = $(UNIX_GFLAGS_DIR)/lib/libgflags.a
DYNAMIC_GFLAGS_LNK = -L$(UNIX_GFLAGS_DIR)/lib -lgflags

############
##  GLOG  ##
############
# This uses glog cmake-based build.
install_glog: dependencies/install/include/glog/logging.h

dependencies/install/include/glog/logging.h: dependencies/sources/glog-$(GLOG_TAG)/build_cmake/Makefile
	cd dependencies/sources/glog-$(GLOG_TAG)/build_cmake && $(SET_COMPILER) make -j 4 && make install
	touch $@

dependencies/sources/glog-$(GLOG_TAG)/build_cmake/Makefile: dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt | install_gflags
	-$(MKDIR) dependencies/sources/glog-$(GLOG_TAG)/build_cmake
	cd dependencies/sources/glog-$(GLOG_TAG)/build_cmake && \
	$(CMAKE) -D CMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)/dependencies/install" \
           -D BUILD_SHARED_LIBS=OFF \
           -D CMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
           -D CMAKE_INSTALL_PREFIX=../../../install \
           ..

dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt:
	git clone --quiet -b v$(GLOG_TAG) https://github.com/google/glog.git dependencies/sources/glog-$(GLOG_TAG)

GLOG_INC = -I$(UNIX_GLOG_DIR)/include
STATIC_GLOG_LNK = $(UNIX_GLOG_DIR)/lib/libglog.a
DYNAMIC_GLOG_LNK = -L$(UNIX_GLOG_DIR)/lib -lglog

################
##  Protobuf  ##
################
# This uses Protobuf cmake-based build.
install_protobuf: dependencies/install/bin/protoc

dependencies/install/bin/protoc: dependencies/sources/protobuf-$(PROTOBUF_TAG)/cmake/build/Makefile
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/cmake/build && $(SET_COMPILER) make -j 4 && make install

dependencies/sources/protobuf-$(PROTOBUF_TAG)/cmake/build/Makefile: dependencies/sources/protobuf-$(PROTOBUF_TAG)/cmake/CMakeLists.txt
	-$(MKDIR) dependencies/sources/protobuf-$(PROTOBUF_TAG)/cmake/build
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/cmake/build && \
	$(CMAKE) -D CMAKE_INSTALL_PREFIX=../../../../install \
           -D protobuf_BUILD_TESTS=OFF \
           -D BUILD_SHARED_LIBS=OFF \
           -D CMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
           ..

dependencies/sources/protobuf-$(PROTOBUF_TAG)/cmake/CMakeLists.txt:
	git clone --quiet https://github.com/google/protobuf.git dependencies/sources/protobuf-$(PROTOBUF_TAG) && \
		cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && \
		git checkout tags/v$(PROTOBUF_TAG) -b $(PROTOBUF_TAG)

# Install Java protobuf
dependencies/install/lib/protobuf.jar: dependencies/install/bin/protoc
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java && \
	  ../../../install/bin/protoc --java_out=core/src/main/java -I../src \
	  ../src/google/protobuf/descriptor.proto
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && \
		$(JAVAC_BIN) com/google/protobuf/*java
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && \
		$(JAR_BIN) cvf ../../../../../../../install/lib/protobuf.jar com/google/protobuf/*class

# This is needed to find protocol buffers.
PROTOBUF_INC = -I$(UNIX_PROTOBUF_DIR)/include
PROTOBUF_PROTOC_INC = $(PROTOBUF_INC)
# libprotobuf.a goes in a different subdirectory depending on the distribution
# and architecture, eg. "lib/" or "lib64/" for Fedora and Centos,
# "lib/x86_64-linux-gnu/" for Ubuntu (all on 64 bits), etc. So we wildcard it.
STATIC_PROTOBUF_LNK = $(wildcard $(UNIX_PROTOBUF_DIR)/lib*/libprotobuf.a \
                          $(UNIX_PROTOBUF_DIR)/lib/*/libprotobuf.a)
DYNAMIC_PROTOBUF_LNK = -L$(dir $(STATIC_PROTOBUF_LNK)) -lprotobuf

###############
##  COIN-OR  ##
###############
# Install Coin CBC/CLP/CGL/OSI/COINUTILS.
install_cbc: dependencies/install/bin/cbc

dependencies/install/bin/cbc: dependencies/sources/Cbc-$(CBC_TAG)/Makefile
	cd dependencies/sources/Cbc-$(CBC_TAG) && $(SET_COMPILER) make -j 4 && $(SET_COMPILER) make install

dependencies/sources/Cbc-$(CBC_TAG)/Makefile: dependencies/sources/Cbc-$(CBC_TAG)/Makefile.in
	cd dependencies/sources/Cbc-$(CBC_TAG) && \
		$(SET_COMPILER) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install \
		--disable-bzlib --without-lapack --enable-static --with-pic \
		--enable-cbc-parallel ADD_CXXFLAGS="-w -DCBC_THREAD_SAFE -DCBC_NO_INTERRUPT $(MAC_VERSION)"

CBC_ARCHIVE:=https://www.coin-or.org/download/source/Cbc/Cbc-${CBC_TAG}.tgz

dependencies/sources/Cbc-$(CBC_TAG)/Makefile.in:
	wget --quiet --no-check-certificate --continue -P dependencies/archives ${CBC_ARCHIVE} || \
		(@echo wget failed to dowload $(CBC_ARCHIVE), try running 'wget -P dependencies/archives --no-check-certificate $(CBC_ARCHIVE)' then rerun 'make third_party' && exit 1)
	tar xzf dependencies/archives/Cbc-${CBC_TAG}.tgz -C dependencies/sources/

# This is needed to find CBC include files.
CBC_COIN_DIR = $(firstword $(wildcard $(UNIX_CBC_DIR)/include/cbc/coin \
                                      $(UNIX_CBC_DIR)/include/coin))
CBC_INC = -I$(UNIX_CBC_DIR)/include -I$(CBC_COIN_DIR) -DUSE_CBC
CBC_SWIG = $(CBC_INC)
ifneq ($(wildcard $(UNIX_CBC_DIR)/lib/coin),)
 UNIX_CBC_COIN = /coin
endif
STATIC_CBC_LNK = $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libCbcSolver.a \
          $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libOsiCbc.a \
          $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libCbc.a
DYNAMIC_CBC_LNK = -L$(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN) -lCbcSolver -lCbc -lOsiCbc

# This is needed to find CLP include files.
CLP_COIN_DIR = $(firstword $(wildcard $(UNIX_CLP_DIR)/include/clp/coin \
                                      $(UNIX_CLP_DIR)/include/coin))
CLP_INC = -I$(UNIX_CLP_DIR)/include -I$(CLP_COIN_DIR) -DUSE_CLP
CLP_SWIG = $(CLP_INC)
ifneq ($(wildcard $(UNIX_CLP_DIR)/lib/coin),)
 UNIX_CLP_COIN = /coin
endif
STATIC_CLP_LNK = $(UNIX_CBC_DIR)/lib$(UNIX_CLP_COIN)/libClpSolver.a \
          $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libOsiClp.a \
          $(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN)/libClp.a
DYNAMIC_CLP_LNK = -L$(UNIX_CLP_DIR)/lib$(UNIX_CLP_COIN) -lClpSolver -lClp -lOsiClp

# This is needed to find CGL include files.
CGL_COIN_DIR = $(firstword $(wildcard $(UNIX_CGL_DIR)/include/cgl/coin \
                                      $(UNIX_CGL_DIR)/include/coin))
CGL_INC = -I$(UNIX_CGL_DIR)/include -I$(CGL_COIN_DIR)
CGL_SWIG = $(CGL_INC)
ifneq ($(wildcard $(UNIX_CGL_DIR)/lib/coin),)
 UNIX_CGL_COIN = /coin
endif
STATIC_CGL_LNK = $(UNIX_CGL_DIR)/lib$(UNIX_CGL_COIN)/libCgl.a
DYNAMIC_CGL_LNK = -L$(UNIX_CGL_DIR)/lib$(UNIX_CGL_COIN) -lCgl

# This is needed to find OSI include files.
OSI_COIN_DIR = $(firstword $(wildcard $(UNIX_OSI_DIR)/include/osi/coin \
                                      $(UNIX_OSI_DIR)/include/coin))
OSI_INC = -I$(UNIX_OSI_DIR)/include -I$(OSI_COIN_DIR)
OSI_SWIG = $(OSI_INC)
ifneq ($(wildcard $(UNIX_OSI_DIR)/lib/coin),)
 UNIX_OSI_COIN = /coin
endif
STATIC_OSI_LNK = $(UNIX_OSI_DIR)/lib$(UNIX_OSI_COIN)/libOsi.a
DYNAMIC_OSI_LNK = -L$(UNIX_OSI_DIR)/lib$(UNIX_OSI_COIN) -lOsi

# This is needed to find COINUTILS include files.
COINUTILS_COIN_DIR = $(firstword $(wildcard $(UNIX_COINUTILS_DIR)/include/coinutils/coin \
                                      $(UNIX_COINUTILS_DIR)/include/coin))
COINUTILS_INC = -I$(UNIX_COINUTILS_DIR)/include -I$(COINUTILS_COIN_DIR)
COINUTILS_SWIG = $(COINUTILS_INC)
ifneq ($(wildcard $(UNIX_COINUTILS_DIR)/lib/coin),)
 UNIX_COINUTILS_COIN = /coin
endif
STATIC_COINUTILS_LNK = $(UNIX_COINUTILS_DIR)/lib$(UNIX_COINUTILS_COIN)/libCoinUtils.a
DYNAMIC_COINUTILS_LNK = -L$(UNIX_COINUTILS_DIR)/lib$(UNIX_COINUTILS_COIN) -lCoinUtils

# Agregate all previous coin packages
COIN_INC = \
  $(COINUTILS_INC) \
  $(OSI_INC) \
  $(CGL_INC) \
  $(CLP_INC) \
  $(CBC_INC)
COIN_SWIG = $(COIN_INC)
STATIC_COIN_LNK = \
	$(STATIC_CBC_LNK) \
  $(STATIC_CLP_LNK) \
  $(STATIC_CGL_LNK) \
  $(STATIC_OSI_LNK) \
  $(STATIC_COINUTILS_LNK)
DYNAMIC_COIN_LNK = \
  $(DYNAMIC_CBC_LNK) \
  $(DYNAMIC_CLP_LNK) \
  $(DYNAMIC_CGL_LNK) \
  $(DYNAMIC_OSI_LNK) \
	$(DYNAMIC_COINUTILS_LNK)

##################################
##  USE DYNAMIC DEPENDENCIES ?  ##
##################################
ifeq ($(UNIX_GFLAGS_DIR), $(OR_TOOLS_TOP)/dependencies/install)
  DEPENDENCIES_LNK += $(STATIC_GFLAGS_LNK)
else
  DEPENDENCIES_LNK += $(DYNAMIC_GFLAGS_LNK)
	OR_TOOLS_LNK += $(DYNAMIC_GFLAGS_LNK)
endif
ifeq ($(UNIX_GLOG_DIR), $(OR_TOOLS_TOP)/dependencies/install)
  DEPENDENCIES_LNK += $(STATIC_GLOG_LNK)
else
  DEPENDENCIES_LNK += $(DYNAMIC_GLOG_LNK)
	OR_TOOLS_LNK += $(DYNAMIC_GLOG_LNK)
endif
ifeq ($(UNIX_PROTOBUF_DIR), $(OR_TOOLS_TOP)/dependencies/install)
  DEPENDENCIES_LNK += $(STATIC_PROTOBUF_LNK)
else
  DEPENDENCIES_LNK += $(DYNAMIC_PROTOBUF_LNK)
	OR_TOOLS_LNK += $(DYNAMIC_PROTOBUF_LNK)
endif
ifeq ($(UNIX_CBC_DIR), $(OR_TOOLS_TOP)/dependencies/install)
  DEPENDENCIES_LNK += $(STATIC_COIN_LNK)
else
  DEPENDENCIES_LNK += $(DYNAMIC_COIN_LNK)
	OR_TOOLS_LNK += $(DYNAMIC_COIN_LNK)
endif

############################################
##  Install Patchelf on linux platforms.  ##
############################################
# Detect if patchelf is needed
ifeq ($(PLATFORM), LINUX)
  PATCHELF=dependencies/install/bin/patchelf
endif

dependencies/install/bin/patchelf: dependencies/sources/patchelf-$(PATCHELF_TAG)/Makefile
	cd dependencies/sources/patchelf-$(PATCHELF_TAG) && make && make install

dependencies/sources/patchelf-$(PATCHELF_TAG)/Makefile: dependencies/sources/patchelf-$(PATCHELF_TAG)/configure
	cd dependencies/sources/patchelf-$(PATCHELF_TAG) && ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/patchelf-$(PATCHELF_TAG)/configure:
	git clone --quiet -b $(PATCHELF_TAG) https://github.com/NixOS/patchelf.git dependencies/sources/patchelf-$(PATCHELF_TAG)
	cd dependencies/sources/patchelf-$(PATCHELF_TAG) && ./bootstrap.sh

.PHONY: clean_third_party # Clean everything. Remember to also delete archived dependencies, i.e. in the event of download failure, etc.
clean_third_party:
	-$(DEL) Makefile.local
	-$(DELREC) dependencies/archives/Cbc*
	-$(DELREC) dependencies/archives
	-$(DELREC) dependencies/sources/Cbc*
	-$(DELREC) dependencies/sources/coin-cbc*
	-$(DELREC) dependencies/sources/gflags*
	-$(DELREC) dependencies/sources/glog*
	-$(DELREC) dependencies/sources/glpk*
	-$(DELREC) dependencies/sources/google*
	-$(DELREC) dependencies/sources/mono*
	-$(DELREC) dependencies/sources/pcre*
	-$(DELREC) dependencies/sources/swig*
	-$(DELREC) dependencies/sources/protobuf*
	-$(DELREC) dependencies/sources/sparsehash*
	-$(DELREC) dependencies/sources/libtool*
	-$(DELREC) dependencies/sources/autoconf*
	-$(DELREC) dependencies/sources/automake*
	-$(DELREC) dependencies/sources/bison*
	-$(DELREC) dependencies/sources/flex*
	-$(DELREC) dependencies/sources/help2man*
	-$(DELREC) dependencies/sources/patchelf*
	-$(DELREC) dependencies/install

# Create Makefile.local
.PHONY: makefile_third_party
makefile_third_party: Makefile.local

Makefile.local: makefiles/Makefile.third_party.unix.mk
	-$(DEL) Makefile.local
	@echo Generating Makefile.local
	@echo "# Define UNIX_SWIG_BINARY to use a custom version." >> Makefile.local
	@echo "#   e.g. UNIX_SWIG_BINARY = /opt/swig-x.y.z/bin/swig" >> Makefile.local
	@echo JAVA_HOME = $(JAVA_HOME)>> Makefile.local
	@echo UNIX_PYTHON_VER = $(DETECTED_PYTHON_VERSION)>> Makefile.local
	@echo PATH_TO_CSHARP_COMPILER = $(DETECTED_MCS_BINARY)>> Makefile.local
	@echo DOTNET_INSTALL_PATH = $(DOTNET_INSTALL_PATH)>> Makefile.local
	@echo CLR_KEYFILE = bin/or-tools.snk>> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GLPK_DIR to point to a compiled version of GLPK to use it" >> Makefile.local
	@echo "#   e.g. UNIX_GLPK_DIR = /opt/glpk-x.y.z" >> Makefile.local
	@echo "# Define UNIX_SCIP_DIR to point to a compiled version of SCIP to use it ">> Makefile.local
	@echo "#   e.g. UNIX_SCIP_DIR = <path>/scipoptsuite-4.0.1/scip" >> Makefile.local
	@echo "#   On Mac OS X, compile scip with: " >> Makefile.local
	@echo "#     make GMP=false READLINE=false TPI=tny" >> Makefile.local
	@echo "#   On Linux, compile scip with: " >> Makefile.local
	@echo "#     make GMP=false READLINE=false TPI=tny USRCFLAGS=-fPIC USRCXXFLAGS=-fPIC USRCPPFLAGS=-fPIC" >> Makefile.local
	@echo "# Define UNIX_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi" >> Makefile.local
	@echo "# Define UNIX_CPLEX_DIR to use CPLEX" >> Makefile.local
	@echo >> Makefile.local
	@echo "# By default OR-Tools will compile and use Gflags, Glog, Protobuf and CBC as static dependencies," >> Makefile.local
	@echo "# Define UNIX_GFLAGS_DIR to depend on external Gflags dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_GFLAGS_DIR = /opt/gflags-x.y.z" >> Makefile.local
	@echo "# Define UNIX_GLOG_DIR to depend on external Glog dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_GLOG_DIR = /opt/glog-x.y.z" >> Makefile.local
	@echo "# Define UNIX_PROTOBUF_DIR to depend on external Protobuf dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_PROTOBUF_DIR = /opt/protobuf-x.y.z" >> Makefile.local
	@echo "# Define UNIX_CBC_DIR to depend on external CBC dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_CBC_DIR = /opt/cbc-x.y.z" >> Makefile.local
	@echo "#   If you use a splitted version of CBC you can also define:" >> Makefile.local
	@echo "#     UNIX_CLP_DIR, UNIX_CGL_DIR, UNIX_OSI_DIR, UNIX_COINUTILS_DIR" >> Makefile.local
	@echo "#   note: by default they all point to UNIX_CBC_DIR" >> Makefile.local
	@echo "# note: You don't need to run \"make third_party\" if you only use external dependencies" >> Makefile.local

.PHONY: detect_third_party # Show variables used to find third party
detect_third_party:
	@echo Relevant info on third party:
	@echo UNIX_GFLAGS_DIR = $(UNIX_GFLAGS_DIR)
	@echo GFLAGS_INC = $(GFLAGS_INC)
	@echo GFLAGS_LNK = $(GFLAGS_LNK)
	@echo UNIX_GLOG_DIR = $(UNIX_GLOG_DIR)
	@echo GLOG_INC = $(GLOG_INC)
	@echo GLOG_LNK = $(GLOG_LNK)
	@echo UNIX_PROTOBUF_DIR = $(UNIX_PROTOBUF_DIR)
	@echo PROTOBUF_DIR = $(PROTOBUF_DIR)
	@echo PROTOBUF_INC = $(PROTOBUF_INC)
	@echo PROTOBUF_LNK = $(PROTOBUF_LNK)
	@echo UNIX_CBC_DIR = $(UNIX_CBC_DIR)
	@echo CBC_INC = $(CBC_INC)
	@echo CBC_LNK = $(CBC_LNK)
	@echo UNIX_CLP_DIR = $(UNIX_CLP_DIR)
	@echo CLP_INC = $(CLP_INC)
	@echo CLP_LNK = $(CLP_LNK)
	@echo UNIX_CGL_DIR = $(UNIX_CGL_DIR)
	@echo CGL_INC = $(CGL_INC)
	@echo CGL_LNK = $(CGL_LNK)
	@echo UNIX_OSI_DIR = $(UNIX_OSI_DIR)
	@echo OSI_INC = $(OSI_INC)
	@echo OSI_LNK = $(OSI_LNK)
	@echo UNIX_COINUTILS_DIR = $(UNIX_COINUTILS_DIR)
	@echo COINUTILS_INC = $(COINUTILS_INC)
	@echo COINUTILS_LNK = $(COINUTILS_LNK)
ifdef UNIX_GLPK_DIR
	@echo UNIX_GLPK_DIR = $(UNIX_GLPK_DIR)
	@echo GLPK_INC = $(GLPK_INC)
	@echo GLPK_LNK = $(GLPK_LNK)
endif
ifdef UNIX_SCIP_DIR
	@echo UNIX_SCIP_DIR = $(UNIX_SCIP_DIR)
	@echo SCIP_INC = $(SCIP_INC)
	@echo SCIP_LNK = $(SCIP_LNK)
endif
ifdef UNIX_CPLEX_DIR
	@echo UNIX_CPLEX_DIR = $(UNIX_CPLEX_DIR)
	@echo CPLEX_INC = $(CPLEX_INC)
	@echo CPLEX_LNK = $(CPLEX_LNK)
endif
ifdef UNIX_GUROBI_DIR
	@echo UNIX_GUROBI_DIR = $(UNIX_GUROBI_DIR)
	@echo GUROBI_INC = $(GUROBI_INC)
	@echo GUROBI_LNK = $(GUROBI_LNK)
endif
	@echo
