.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.unix.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo

# Checks if the user has overwritten default libraries and binaries.
UNIX_FFTW_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_GTEST_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_GFLAGS_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_GLOG_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_PROTOBUF_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_PROTOC_BINARY ?= $(UNIX_PROTOBUF_DIR)/bin/protoc
UNIX_CBC_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_CGL_DIR ?= $(UNIX_CBC_DIR)
UNIX_CLP_DIR ?= $(UNIX_CBC_DIR)
UNIX_OSI_DIR ?= $(UNIX_CBC_DIR)
UNIX_COINUTILS_DIR ?= $(UNIX_CBC_DIR)
UNIX_SWIG_BINARY ?= swig
PROTOC_BINARY := $(shell $(WHICH) ${UNIX_PROTOC_BINARY})
UNIX_OCAML_PACKAGE ?= ocaml
UNIX_OCAMLBUILD_PACKAGE ?= ocamlbuild
UNIX_INDENT_PACKAGE ?= indent
UNIX_FIG2DEV_PACKAGE ?= fig2dev
UNIX_TEXINFO_PACKAGE ?= texinfo

# Tags of dependencies to checkout.
FFTW_TAG = 3.3.8
GTEST_TAG = 1.8.1
GFLAGS_TAG = 2.2.1
GLOG_TAG = 0.3.5
PROTOBUF_TAG = 3.6.1
CBC_TAG = 2.9.9
CGL_TAG = 0.59.10
CLP_TAG = 1.16.11
OSI_TAG = 0.107.9
COINUTILS_TAG = 2.10.14
PATCHELF_TAG = 0.9

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party: build_third_party

.PHONY: third_party_check # Check if third parties are all found
third_party_check: dependencies/check.log

dependencies/check.log: Makefile.local
ifeq ($(wildcard $(UNIX_GTEST_DIR)/include/gtest/gtest.h),)
	$(error Third party GTest files were not found! did you run 'make third_party' or set UNIX_GTEST_DIR ?)
else
	$(info GTEST: found)
endif
ifeq ($(wildcard $(UNIX_GFLAGS_DIR)/include/gflags/gflags.h),)
	$(error Third party GFlags files was not found! did you run 'make third_party' or set UNIX_GFLAGS_DIR ?)
else
	$(info GFLAGS: found)
endif
ifeq ($(wildcard $(UNIX_GLOG_DIR)/include/glog/logging.h),)
	$(error Third party GLog files was not found! did you run 'make third_party' or set UNIX_GLOG_DIR ?)
else
	$(info GLOG: found)
endif
ifeq ($(wildcard $(UNIX_PROTOBUF_DIR)/include/google/protobuf/descriptor.h),)
	$(error Third party Protobuf files was not found! did you run 'make third_party' or set UNIX_PROTOBUF_DIR ?)
else
	$(info PROTOBUF: found)
endif
ifeq ($(wildcard $(PROTOC_BINARY)),)
	$(error Cannot find $(UNIX_PROTOC_BINARY). Please verify UNIX_PROTOC_BINARY)
else
	$(info PROTOC: found)
endif
ifeq ($(wildcard $(UNIX_COINUTILS_DIR)/include/coinutils/coin/CoinModel.hpp $(UNIX_COINUTILS_DIR)/include/coin/CoinModel.hpp),)
	$(error Third party CoinUtils files was not found! did you run 'make third_party' or set UNIX_COINUTILS_DIR ?)
else
	$(info COINUTILS: found)
endif
ifeq ($(wildcard $(UNIX_OSI_DIR)/include/osi/coin/OsiSolverInterface.hpp $(UNIX_OSI_DIR)/include/coin/OsiSolverInterface.hpp),)
	$(error Third party Osi files was not found! did you run 'make third_party' or set UNIX_OSI_DIR ?)
else
	$(info OSI: found)
endif
ifeq ($(wildcard $(UNIX_CLP_DIR)/include/clp/coin/ClpModel.hpp $(UNIX_CLP_DIR)/include/coin/ClpSimplex.hpp),)
	$(error Third party Clp files was not found! did you run 'make third_party' or set UNIX_CLP_DIR ?)
else
	$(info CLP: found)
endif
ifeq ($(wildcard $(UNIX_CGL_DIR)/include/cgl/coin/CglParam.hpp $(UNIX_CGL_DIR)/include/coin/CglParam.hpp),)
	$(error Third party Cgl files was not found! did you run 'make third_party' or set UNIX_CGL_DIR ?)
else
	$(info CGL: found)
endif
ifeq ($(wildcard $(UNIX_CBC_DIR)/include/cbc/coin/CbcModel.hpp $(UNIX_CBC_DIR)/include/coin/CbcModel.hpp),)
	$(error Third party Cbc files was not found! did you run 'make third_party' or set UNIX_CBC_DIR ?)
else
	$(info CBC: found)
endif
# Optional dependencies
ifndef UNIX_CPLEX_DIR
	$(info CPLEX: not found)
endif
ifndef UNIX_GLPK_DIR
	$(info GLPK: not found)
endif
ifndef UNIX_GUROBI_DIR
	$(info GUROBI: not found)
endif
ifndef UNIX_SCIP_DIR
	$(info SCIP: not found)
#else
  # ifeq ($(wildcard $(UNIX_SCIP_DIR)/src/scip/scip.h),)
  #	$(error Third party SCIP files was not found! please check the path given to UNIX_SCIP_DIR)
  #else
#	$(info SCIP: found)
  #endif
endif
	$(TOUCH) $@

.PHONY: build_third_party
build_third_party: \
 Makefile.local \
 archives_directory \
 install_deps_directories \
 build_fftw \
 build_gtest \
 build_gflags \
 build_glog \
 build_protobuf \
 build_cbc

.PHONY: archives_directory
archives_directory: dependencies/archives

dependencies/archives:
	$(MKDIR_P) dependencies$Sarchives

.PHONY: install_deps_directories
install_deps_directories: \
 dependencies/install/bin \
 dependencies/install/lib/pkgconfig \
 dependencies/install/include/coin

dependencies/install:
	$(MKDIR_P) dependencies$Sinstall

dependencies/install/bin: | dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sbin

dependencies/install/lib: | dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Slib

dependencies/install/lib/pkgconfig: | dependencies/install/lib
	$(MKDIR_P) dependencies$Sinstall$Slib$Spkgconfig

dependencies/install/include: | dependencies/install
	$(MKDIR_P) dependencies$Sinstall$Sinclude

dependencies/install/include/coin: | dependencies/install/include
	$(MKDIR_P) dependencies$Sinstall$Sinclude$Scoin

######################
##  Makefile.local  ##
######################
# Make sure that local file lands correctly across platforms
Makefile.local: makefiles/Makefile.third_party.$(SYSTEM).mk
	-$(DEL) Makefile.local
	@echo Generating Makefile.local
	@echo "# Define UNIX_SWIG_BINARY to use a custom version." >> Makefile.local
	@echo "#   e.g. UNIX_SWIG_BINARY = /opt/swig-x.y.z/bin/swig" >> Makefile.local
	@echo JAVA_HOME = $(JAVA_HOME)>> Makefile.local
	@echo UNIX_PYTHON_VER = $(DETECTED_PYTHON_VERSION)>> Makefile.local
	@echo >> Makefile.local
	@echo "## OPTIONAL DEPENDENCIES ##" >> Makefile.local
	@echo "# Define UNIX_CPLEX_DIR to use CPLEX" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GLPK_DIR to point to a compiled version of GLPK to use it" >> Makefile.local
	@echo "#   e.g. UNIX_GLPK_DIR = /opt/glpk-x.y.z" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_SCIP_DIR to point to a compiled version of SCIP to use it ">> Makefile.local
	@echo "#   e.g. UNIX_SCIP_DIR = <path>/scipoptsuite-6.0.0/scip" >> Makefile.local
	@echo "#   On Mac OS X, compile scip with: " >> Makefile.local
	@echo "#     make GMP=false READLINE=false TPI=tny ZIMPL=false" >> Makefile.local
	@echo "#   On Linux, compile scip with: " >> Makefile.local
	@echo "#     make GMP=false READLINE=false ZIMPL=false TPI=tny USRCFLAGS=-fPIC USRCXXFLAGS=-fPIC USRCPPFLAGS=-fPIC" >> Makefile.local
	@echo >> Makefile.local
	@echo "## REQUIRED DEPENDENCIES ##" >> Makefile.local
	@echo "# By default they will be automatically built -> nothing to define" >> Makefile.local
	@echo "# Define UNIX_GFLAGS_DIR to depend on external Gflags dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_GFLAGS_DIR = /opt/gflags-x.y.z" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GLOG_DIR to depend on external Glog dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_GLOG_DIR = /opt/glog-x.y.z" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_PROTOBUF_DIR to depend on external Protobuf dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_PROTOBUF_DIR = /opt/protobuf-x.y.z" >> Makefile.local
	@echo "# Define UNIX_PROTOC_BINARY to use a custom version." >> Makefile.local
	@echo "#   e.g. UNIX_PROTOC_BINARY = /opt/protoc-x.y.z/bin/protoc" >> Makefile.local
	@echo "#   (default: UNIX_PROTOBUF_DIR/bin/protoc)" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_CBC_DIR to depend on external CBC dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_CBC_DIR = /opt/cbc-x.y.z" >> Makefile.local
	@echo "#   If you use a splitted version of CBC you can also define:" >> Makefile.local
	@echo "#     UNIX_CLP_DIR, UNIX_CGL_DIR, UNIX_OSI_DIR, UNIX_COINUTILS_DIR" >> Makefile.local
	@echo "#   note: by default they all point to UNIX_CBC_DIR" >> Makefile.local
	@echo >> Makefile.local
	@echo "# note: You don't need to run \"make third_party\" if you only use external dependencies" >> Makefile.local
	@echo "# i.e. you define all UNIX_GFLAGS_DIR, UNIX_GLOG_DIR, UNIX_PROTOBUF_DIR and UNIX_CBC_DIR" >> Makefile.local
	@echo "UNIX_GUROBI_DIR=/opt/gurobi800" >> Makefile.local
	@echo "GUROBI_LIB_VERSION=80" >> Makefile.local
	@echo "UNIX_CPLEX_DIR=/opt/CPLEX_Studio_Community128" >> Makefile.local
	@echo "UNIX_SCIP_DIR=/opt/scipoptsuite-5.0.1/scip" >> Makefile.local
	@echo "# i.e. you define all UNIX_GTEST_DIR, UNIX_GFLAGS_DIR, UNIX_GLOG_DIR, UNIX_PROTOBUF_DIR and UNIX_CBC_DIR" >> Makefile.local

##############
##   FFTW   ## 
##############
.PHONY: build_fftw
build_fftw: dependencies/install/lib/libfftw3.$L

dependencies/install/lib/libfftw3.$L: dependencies/sources/fftw-$(FFTW_TAG) fftw_packages | dependencies/install
	cd dependencies/sources/fftw-$(FFTW_TAG) && \
  ./bootstrap.sh --prefix=$(shell pwd)/dependencies/install && \
  $(MAKE) install

dependencies/sources/fftw-$(FFTW_TAG) : | dependencies/sources
	-$(DELREC) dependencies/sources/fftw-$(FFTW_TAG)
	git clone --quiet -b fftw-$(FFTW_TAG) --single-branch https://github.com/FFTW/fftw3.git dependencies/sources/fftw-$(FFTW_TAG)

OCAML_INSTALLED_VERSION = $(shell $(APTLISTINSTALLED) $(UNIX_OCAML_PACKAGE) $(STDERR_OFF) | $(GREP) $(UNIX_OCAML_PACKAGE) | $(AWK) '{ print $2 }')

OCAMLBUILD_INSTALLED_VERSION = $(shell $(APTLISTINSTALLED) $(UNIX_OCAMLBUILD_PACKAGE) $(STDERR_OFF) | $(GREP) $(UNIX_OCAMLBUILD_PACKAGE) | $(AWK) '{ print $2 }')

INDENT_INSTALLED_VERSION = $(shell $(APTLISTINSTALLED) $(UNIX_INDENT_PACKAGE) $(STDERR_OFF) | $(GREP) $(UNIX_INDENT_PACKAGE) | $(AWK) '{ print $2 }')

FIG2DEV_INSTALLED_VERSION = $(shell $(APTLISTINSTALLED) $(UNIX_FIG2DEV_PACKAGE) $(STDERR_OFF) | $(GREP) $(UNIX_FIG2DEV_PACKAGE) | $(AWK) '{ print $2 }')

TEXINFO_INSTALLED_VERSION = $(shell $(APTLISTINSTALLED) $(UNIX_TEXINFO_PACKAGE) $(STDERR_OFF) | $(GREP) $(UNIX_TEXINFO_PACKAGE) | $(AWK) '{ print $2 }')

fftw_packages :
	if [ -z "$(OCAML_INSTALLED_VERSION)" ]; then $(INSTALL) $(UNIX_OCAML_PACKAGE); fi
	if [ -z "$(OCAMLBUILD_INSTALLED_VERSION)" ]; then $(INSTALL) $(UNIX_OCAMLBUILD_PACKAGE); fi
	if [ -z "$(INDENT_INSTALLED_VERSION)" ]; then $(INSTALL) $(UNIX_INDENT_PACKAGE); fi
	if [ -z "$(FIG2DEV_INSTALLED_VERSION)" ]; then $(INSTALL) $(UNIX_FIG2DEV_PACKAGE); fi
	if [ -z "$(TEXINFO_INSTALLED_VERSION)" ]; then $(INSTALL) $(UNIX_TEXINFO_PACKAGE); fi

##############
##  GTEST  ##
##############
# This uses gflags cmake-based build.
.PHONY: build_gtest
build_gtest: dependencies/install/lib/libgtest.$L

dependencies/install/lib/libgtest.$L: dependencies/sources/gtest-$(GTEST_TAG) | dependencies/install
	cd dependencies/sources/gtest-$(GTEST_TAG) && \
  $(SET_COMPILER) $(CMAKE) -H. -Bbuild_cmake \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_STATIC_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DGTEST_NAMESPACE=gtest \
    -DCMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
    -DCMAKE_INSTALL_PREFIX=../../install && \
  $(CMAKE) --build build_cmake -- -j 4 && \
  $(CMAKE) --build build_cmake --target install

dependencies/sources/gtest-$(GTEST_TAG): | dependencies/sources
	-$(DELREC) dependencies/sources/gtest-$(GTEST_TAG)
	git clone --quiet -b release-$(GTEST_TAG) https://github.com/google/googletest.git dependencies/sources/gtest-$(GTEST_TAG)

GTEST_INC = -I$(UNIX_GTEST_DIR)/include
GTEST_SWIG = $(GTEST_INC)
STATIC_GTEST_LNK = $(UNIX_GTEST_DIR)/lib/libgtest.a
DYNAMIC_GTEST_LNK = -L$(UNIX_GTEST_DIR)/lib -lgtest

GTEST_LNK = $(DYNAMIC_GTEST_LNK)
#DEPENDENCIES_LNK += $(GTEST_LNK)
#OR_TOOLS_LNK += $(GTEST_LNK)

##############
##  GFLAGS  ##
##############
# This uses gflags cmake-based build.
.PHONY: build_gflags
build_gflags: dependencies/install/lib/libgflags.$L

dependencies/install/lib/libgflags.$L: dependencies/sources/gflags-$(GFLAGS_TAG) | dependencies/install
	cd dependencies/sources/gflags-$(GFLAGS_TAG) && \
  $(SET_COMPILER) $(CMAKE) -H. -Bbuild_cmake \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_STATIC_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DGFLAGS_NAMESPACE=gflags \
    -DCMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
    -DCMAKE_INSTALL_PREFIX=../../install && \
  $(CMAKE) --build build_cmake -- -j 4 && \
  $(CMAKE) --build build_cmake --target install

dependencies/sources/gflags-$(GFLAGS_TAG): | dependencies/sources
	-$(DELREC) dependencies/sources/gflags-$(GFLAGS_TAG)
	git clone --quiet -b v$(GFLAGS_TAG) https://github.com/gflags/gflags.git dependencies/sources/gflags-$(GFLAGS_TAG)

GFLAGS_INC = -I$(UNIX_GFLAGS_DIR)/include
GFLAGS_SWIG = $(GFLAGS_INC)
STATIC_GFLAGS_LNK = $(UNIX_GFLAGS_DIR)/lib/libgflags.a
DYNAMIC_GFLAGS_LNK = -L$(UNIX_GFLAGS_DIR)/lib -lgflags

GFLAGS_LNK = $(DYNAMIC_GFLAGS_LNK)
DEPENDENCIES_LNK += $(GFLAGS_LNK)
OR_TOOLS_LNK += $(GFLAGS_LNK)

############
##  GLOG  ##
############
# This uses glog cmake-based build.
.PHONY: build_glog
build_glog: dependencies/install/lib/libglog.$L

dependencies/install/lib/libglog.$L: dependencies/install/lib/libgflags.$L dependencies/sources/glog-$(GLOG_TAG) | dependencies/install
	cd dependencies/sources/glog-$(GLOG_TAG) && \
  $(SET_COMPILER) $(CMAKE) -H. -Bbuild_cmake \
    -DCMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)/dependencies/install" \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTING=OFF \
    -DCMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-rpath,\$$ORIGIN" \
    -DCMAKE_INSTALL_PREFIX=../../install && \
  $(CMAKE) --build build_cmake -- -j 4 && \
  $(CMAKE) --build build_cmake --target install

dependencies/sources/glog-$(GLOG_TAG): | dependencies/sources
	-$(DELREC) dependencies/sources/glog-$(GLOG_TAG)
	git clone --quiet -b v$(GLOG_TAG) https://github.com/google/glog.git dependencies/sources/glog-$(GLOG_TAG)

GLOG_INC = -I$(UNIX_GLOG_DIR)/include
GLOG_SWIG = $(GLOG_INC)
STATIC_GLOG_LNK = $(UNIX_GLOG_DIR)/lib/libglog.a
DYNAMIC_GLOG_LNK = -L$(UNIX_GLOG_DIR)/lib -lglog

GLOG_LNK = $(DYNAMIC_GLOG_LNK)
DEPENDENCIES_LNK += $(GLOG_LNK)
OR_TOOLS_LNK += $(GLOG_LNK)

################
##  Protobuf  ##
################
# This uses Protobuf cmake-based build.
.PHONY: build_protobuf
build_protobuf: dependencies/install/lib/libprotobuf.$L

dependencies/install/lib/libprotobuf.$L: dependencies/install/lib/libglog.$L dependencies/sources/protobuf-$(PROTOBUF_TAG) | dependencies/install
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && \
  $(SET_COMPILER) $(CMAKE) -Hcmake -Bbuild_cmake \
    -DCMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)/dependencies/install" \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTING=OFF \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_BUILD_EXAMPLES=OFF \
    -DCMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
    -DCMAKE_INSTALL_PREFIX=../../install && \
  $(CMAKE) --build build_cmake -- -j 4 && \
  $(CMAKE) --build build_cmake --target install

dependencies/sources/protobuf-$(PROTOBUF_TAG): patches/protobuf.patch | dependencies/sources
	-$(DELREC) dependencies/sources/protobuf-$(PROTOBUF_TAG)
	git clone --quiet -b v$(PROTOBUF_TAG) https://github.com/google/protobuf.git dependencies/sources/protobuf-$(PROTOBUF_TAG)
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && \
    git apply "$(OR_TOOLS_TOP)/patches/protobuf-$(PROTOBUF_TAG).patch"

# This is needed to find protocol buffers.
PROTOBUF_INC = -I$(UNIX_PROTOBUF_DIR)/include
PROTOBUF_SWIG = $(PROTOBUF_INC)
PROTOBUF_PROTOC_INC = $(PROTOBUF_INC)
# libprotobuf.a goes in a different subdirectory depending on the distribution
# and architecture, eg. "lib/" or "lib64/" for Fedora and Centos,
# "lib/x86_64-linux-gnu/" for Ubuntu (all on 64 bits), etc. So we wildcard it.
STATIC_PROTOBUF_LNK = $(wildcard \
 $(UNIX_PROTOBUF_DIR)/lib*/libprotobuf.a \
 $(UNIX_PROTOBUF_DIR)/lib*/libprotobuf.a@ \
 $(UNIX_PROTOBUF_DIR)/lib/*/libprotobuf.a)
_PROTOBUF_LIB_DIR = $(dir $(wildcard \
 $(UNIX_PROTOBUF_DIR)/lib*/libprotobuf.$L \
 $(UNIX_PROTOBUF_DIR)/lib*/libprotobuf.$L@ \
 $(UNIX_PROTOBUF_DIR)/lib/*/libprotobuf.$L))
DYNAMIC_PROTOBUF_LNK = -L$(_PROTOBUF_LIB_DIR) -lprotobuf

PROTOBUF_LNK = $(DYNAMIC_PROTOBUF_LNK)
DEPENDENCIES_LNK += $(PROTOBUF_LNK)
OR_TOOLS_LNK += $(PROTOBUF_LNK)

# Define Protoc
ifeq ($(PLATFORM),LINUX)
 PROTOC = \
LD_LIBRARY_PATH="$(UNIX_PROTOBUF_DIR)/lib64":"$(UNIX_PROTOBUF_DIR)/lib":$(LD_LIBRARY_PATH) $(PROTOC_BINARY)
else
 PROTOC = \
DYLD_LIBRARY_PATH="$(UNIX_PROTOBUF_DIR)/lib":$(DYLD_LIBRARY_PATH) $(PROTOC_BINARY)
endif

# Install Java protobuf
dependencies/install/lib/protobuf.jar: | dependencies/install/lib/libprotobuf.$L
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java && \
 $(PROTOC) --java_out=core/src/main/java -I../src \
 ../src/google/protobuf/descriptor.proto
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && \
 "$(JAVAC_BIN)" com/google/protobuf/*java
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && \
 "$(JAR_BIN)" cvf ../../../../../../../install/lib/protobuf.jar com/google/protobuf/*class

############################################
##  Install Patchelf on linux platforms.  ##
############################################
# Detect if patchelf is needed
ifeq ($(PLATFORM),LINUX)
 PATCHELF = dependencies/install/bin/patchelf
endif

PATCHELF_SRCDIR = dependencies/sources/patchelf-$(PATCHELF_TAG)
dependencies/install/bin/patchelf: $(PATCHELF_SRCDIR) | dependencies/install/bin
	cd $(PATCHELF_SRCDIR) && ./configure \
    --prefix="$(OR_ROOT_FULL)/dependencies/install"
	make -C $(PATCHELF_SRCDIR)
	make install -C $(PATCHELF_SRCDIR)

$(PATCHELF_SRCDIR): | dependencies/sources
	git clone --quiet -b $(PATCHELF_TAG) https://github.com/NixOS/patchelf.git $(PATCHELF_SRCDIR)
	cd $(PATCHELF_SRCDIR) && ./bootstrap.sh

###################
##  COIN-OR-CBC  ##
###################
.PHONY: build_cbc
build_cbc: dependencies/install/lib/libCbc.$L

CBC_SRCDIR = dependencies/sources/Cbc-$(CBC_TAG)
dependencies/install/lib/libCbc.$L: build_cgl $(CBC_SRCDIR) $(PATCHELF)
	cd $(CBC_SRCDIR) && $(SET_COMPILER) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    --enable-cbc-parallel \
    ADD_CXXFLAGS="-w -DCBC_THREAD_SAFE -DCBC_NO_INTERRUPT $(MAC_VERSION)"
	$(SET_COMPILER) make -C $(CBC_SRCDIR)
	$(SET_COMPILER) make install -C $(CBC_SRCDIR)
ifeq ($(PLATFORM),LINUX)
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libCbc.$L
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libCbcSolver.$L
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libOsiCbc.$L
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN/../lib' dependencies/install/bin/cbc
endif
ifeq ($(PLATFORM),MACOSX)
# libCbc.dylib
	install_name_tool -id @rpath/libCbc.3.$L dependencies/install/lib/libCbc.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libCbc.$L
# libCbcSolver.dylib
	install_name_tool -id @rpath/libCbcSolver.3.$L dependencies/install/lib/libCbcSolver.$L
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libCbc.3.$L @rpath/libCbc.3.$L \
 dependencies/install/lib/libCbcSolver.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libCbcSolver.$L
# libOsiCbc.dylib
	install_name_tool -id @rpath/libOsiCbc.3.$L dependencies/install/lib/libOsiCbc.$L
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libCbc.3.$L @rpath/libCbc.3.$L \
 dependencies/install/lib/libOsiCbc.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libOsiCbc.$L
# bin/cbc
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libCbc.3.$L @rpath/libCbc.3.$L \
 dependencies/install/bin/cbc
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libCbcSolver.3.$L @rpath/libCbcSolver.3.$L \
 dependencies/install/bin/cbc
	install_name_tool -add_rpath @loader_path/../lib dependencies/install/bin/cbc
endif

$(CBC_SRCDIR): | dependencies/sources
	-$(DELREC) $(CBC_SRCDIR)
	git clone --quiet -b releases/$(CBC_TAG) https://github.com/coin-or/Cbc.git dependencies/sources/Cbc-$(CBC_TAG)

# This is needed to find CBC include files.
CBC_COIN_DIR = $(firstword $(wildcard $(UNIX_CBC_DIR)/include/cbc/coin \
                                      $(UNIX_CBC_DIR)/include/coin))
CBC_INC = -I$(UNIX_CBC_DIR)/include -I$(CBC_COIN_DIR) -DUSE_CBC -DUSE_GUROBI
CBC_SWIG = $(CBC_INC)
ifneq ($(wildcard $(UNIX_CBC_DIR)/lib/coin),)
 UNIX_CBC_COIN = /coin
endif
STATIC_CBC_LNK = $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libCbcSolver.a \
          $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libOsiCbc.a \
          $(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN)/libCbc.a
DYNAMIC_CBC_LNK = -L$(UNIX_CBC_DIR)/lib$(UNIX_CBC_COIN) -lCbcSolver -lCbc -lOsiCbc
CBC_LNK = $(DYNAMIC_CBC_LNK)

###################
##  COIN-OR-CGL  ##
###################
.PHONY: build_cgl
build_cgl: dependencies/install/lib/libCgl.$L

CGL_SRCDIR = dependencies/sources/Cgl-$(CGL_TAG)
dependencies/install/lib/libCgl.$L: build_clp $(CGL_SRCDIR) $(PATCHELF)
	cd $(CGL_SRCDIR) && $(SET_COMPILER) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION)"
	$(SET_COMPILER) make -C $(CGL_SRCDIR)
	$(SET_COMPILER) make install -C $(CGL_SRCDIR)
ifeq ($(PLATFORM),LINUX)
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libCgl.$L
endif
ifeq ($(PLATFORM),MACOSX)
	install_name_tool -id @rpath/libCgl.1.$L dependencies/install/lib/libCgl.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libCgl.$L
endif

$(CGL_SRCDIR): | dependencies/sources
	-$(DELREC) $(CGL_SRCDIR)
	git clone --quiet -b releases/$(CGL_TAG) https://github.com/coin-or/Cgl.git $(CGL_SRCDIR)

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
CGL_LNK = $(DYNAMIC_CGL_LNK)

###################
##  COIN-OR-CLP  ##
###################
.PHONY: build_clp
build_clp: dependencies/install/lib/libClp.$L

CLP_SRCDIR = dependencies/sources/Clp-$(CLP_TAG)
dependencies/install/lib/libClp.$L: build_osi $(CLP_SRCDIR) $(PATCHELF)
	cd $(CLP_SRCDIR) && $(SET_COMPILER) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION)"
	$(SET_COMPILER) make -C $(CLP_SRCDIR)
	$(SET_COMPILER) make install -C $(CLP_SRCDIR)
ifeq ($(PLATFORM),LINUX)
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libClp.$L
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libClpSolver.$L
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libOsiClp.$L
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN/../lib' dependencies/install/bin/clp
endif
ifeq ($(PLATFORM),MACOSX)
# libClp.dylib
	install_name_tool -id @rpath/libClp.1.$L dependencies/install/lib/libClp.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libClp.$L
# libClpSolver.dylib
	install_name_tool -id @rpath/libClpSolver.1.$L dependencies/install/lib/libClpSolver.$L
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libClp.1.$L @rpath/libClp.1.$L \
 dependencies/install/lib/libClpSolver.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libClpSolver.$L
# libOsiClp.dylib
	install_name_tool -id @rpath/libOsiClp.1.$L dependencies/install/lib/libOsiClp.$L
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libClp.1.$L @rpath/libClp.1.$L \
 dependencies/install/lib/libOsiClp.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libOsiClp.$L
# bin/clp
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libClp.1.$L @rpath/libClp.1.$L \
 dependencies/install/bin/clp
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libClpSolver.1.$L @rpath/libClpSolver.1.$L \
 dependencies/install/bin/clp
	install_name_tool -add_rpath @loader_path/../lib dependencies/install/bin/clp
endif

$(CLP_SRCDIR): | dependencies/sources
	-$(DELREC) $(CLP_SRCDIR)
	git clone --quiet -b releases/$(CLP_TAG) https://github.com/coin-or/Clp.git $(CLP_SRCDIR)

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
CLP_LNK = $(DYNAMIC_CLP_LNK)

###################
##  COIN-OR-OSI  ##
###################
.PHONY: build_osi
build_osi: dependencies/install/lib/libOsi.$L

OSI_SRCDIR = dependencies/sources/Osi-$(OSI_TAG)
dependencies/install/lib/libOsi.$L: build_coinutils $(OSI_SRCDIR) $(PATCHELF)
	cd $(OSI_SRCDIR) && $(SET_COMPILER) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --with-coinutils \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION)"
	$(SET_COMPILER) make -C $(OSI_SRCDIR)
	$(SET_COMPILER) make install -C $(OSI_SRCDIR)
ifeq ($(PLATFORM),LINUX)
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libOsi.$L
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libOsiCommonTests.$L
endif
ifeq ($(PLATFORM),MACOSX)
# libOsi.dylib
	install_name_tool -id @rpath/libOsi.1.$L dependencies/install/lib/libOsi.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libOsi.$L
# libOsiCommonTests.dylib
	install_name_tool -id @rpath/libOsiCommonTests.1.$L dependencies/install/lib/libOsiCommonTests.$L
	install_name_tool -change \
 $(OR_ROOT_FULL)/dependencies/install/lib/libOsi.1.$L @rpath/libOsi.1.$L \
 dependencies/install/lib/libOsiCommonTests.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libOsiCommonTests.$L
endif

$(OSI_SRCDIR): | dependencies/sources
	-$(DELREC) $(OSI_SRCDIR)
	git clone --quiet -b releases/$(OSI_TAG) https://github.com/coin-or/Osi.git $(OSI_SRCDIR)

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
OSI_LNK = $(DYNAMIC_OSI_LNK)

#########################
##  COIN-OR-COINUTILS  ##
#########################
.PHONY: build_coinutils
build_coinutils: dependencies/install/lib/libCoinUtils.$L

COINUTILS_SRCDIR = dependencies/sources/CoinUtils-$(COINUTILS_TAG)
dependencies/install/lib/libCoinUtils.$L: $(COINUTILS_SRCDIR) $(PATCHELF) | \
 dependencies/install/lib/pkgconfig dependencies/install/include/coin
	cd $(COINUTILS_SRCDIR) && $(SET_COMPILER) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION)"
	$(SET_COMPILER) make -C $(COINUTILS_SRCDIR)
	$(SET_COMPILER) make install -C $(COINUTILS_SRCDIR)
ifeq ($(PLATFORM),LINUX)
	$(DEP_BIN_DIR)/patchelf --set-rpath '$$ORIGIN' dependencies/install/lib/libCoinUtils.$L
endif
ifeq ($(PLATFORM),MACOSX)
	install_name_tool -id @rpath/libCoinUtils.3.$L dependencies/install/lib/libCoinUtils.$L
	install_name_tool -add_rpath @loader_path dependencies/install/lib/libCoinUtils.$L
endif

$(COINUTILS_SRCDIR): | dependencies/sources
	-$(DELREC) $(COINUTILS_SRCDIR)
	git clone --quiet -b releases/$(COINUTILS_TAG) https://github.com/coin-or/CoinUtils.git $(COINUTILS_SRCDIR)

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
COINUTILS_LNK = $(DYNAMIC_COINUTILS_LNK)

############
##  COIN  ##
############
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
OR_TOOLS_LNK += $(COIN_LNK)

############
##  SWIG  ##
############
# Swig is only needed when building .Net, Java or Python wrapper
SWIG_BINARY = $(shell $(WHICH) $(UNIX_SWIG_BINARY))
#$(error "Can't find $(UNIX_SWIG_BINARY). Please verify UNIX_SWIG_BINARY")

.PHONY: clean_third_party # Clean everything. Remember to also delete archived dependencies, i.e. in the event of download failure, etc.
clean_third_party:
	-$(DEL) Makefile.local
	-$(DEL) dependencies/check.log
	-$(DELREC) dependencies/archives/Cbc*
	-$(DELREC) dependencies/archives/Cgl*
	-$(DELREC) dependencies/archives/Clp*
	-$(DELREC) dependencies/archives/Osi*
	-$(DELREC) dependencies/archives/CoinUtils*
	-$(DELREC) dependencies/archives
	-$(DELREC) dependencies/sources/gflags*
	-$(DELREC) dependencies/sources/glog*
	-$(DELREC) dependencies/sources/protobuf*
	-$(DELREC) dependencies/sources/abseil-cpp*
	-$(DELREC) dependencies/sources/google*
	-$(DELREC) dependencies/sources/Cbc*
	-$(DELREC) dependencies/sources/Cgl*
	-$(DELREC) dependencies/sources/Clp*
	-$(DELREC) dependencies/sources/Osi*
	-$(DELREC) dependencies/sources/CoinUtils*
	-$(DELREC) dependencies/sources/swig*
	-$(DELREC) dependencies/sources/mono*
	-$(DELREC) dependencies/sources/glpk*
	-$(DELREC) dependencies/sources/pcre*
	-$(DELREC) dependencies/sources/sparsehash*
	-$(DELREC) dependencies/sources/libtool*
	-$(DELREC) dependencies/sources/autoconf*
	-$(DELREC) dependencies/sources/automake*
	-$(DELREC) dependencies/sources/bison*
	-$(DELREC) dependencies/sources/flex*
	-$(DELREC) dependencies/sources/help2man*
	-$(DELREC) dependencies/sources/patchelf*
	-$(DELREC) dependencies/install

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
