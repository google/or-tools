.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.unix.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo

# Checks if the user has overwritten default libraries and binaries.
UNIX_PROTOBUF_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_PROTOC_BINARY ?= $(UNIX_PROTOBUF_DIR)/bin/protoc
UNIX_ABSL_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
USE_COINOR ?= ON
UNIX_CBC_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_CGL_DIR ?= $(UNIX_CBC_DIR)
UNIX_CLP_DIR ?= $(UNIX_CBC_DIR)
UNIX_OSI_DIR ?= $(UNIX_CBC_DIR)
UNIX_COINUTILS_DIR ?= $(UNIX_CBC_DIR)
USE_SCIP ?= ON
UNIX_SCIP_DIR ?= $(OR_TOOLS_TOP)/dependencies/install
UNIX_SWIG_BINARY ?= swig
PROTOC_BINARY := $(shell $(WHICH) ${UNIX_PROTOC_BINARY})

# Tags of dependencies to checkout.
PROTOBUF_TAG = v3.19.1
ABSL_TAG = 20211102.0
CBC_TAG = 2.10.5
CGL_TAG = 0.60.3
# Clp >= 1.17.5 is broken, so we must keep 1.17.4
# see: https://github.com/coin-or/Clp/issues/130
CLP_TAG = 1.17.4
OSI_TAG = 0.108.6
COINUTILS_TAG = 2.11.4
PATCHELF_TAG = 0.10
SCIP_TAG = master

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party: build_third_party

.PHONY: third_party_check # Check if third parties are all found
third_party_check: dependencies/check.log

dependencies/check.log: Makefile.local
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
ifeq ($(wildcard $(UNIX_ABSL_DIR)/include/absl/base/config.h),)
	$(error Third party Abseil-cpp files was not found! did you run 'make third_party' or set UNIX_ABSL_DIR ?)
else
	$(info ABSEIL-CPP: found)
endif
ifeq ($(USE_SCIP),OFF)
	$(info SCIP: disabled)
else
ifeq ($(wildcard $(UNIX_SCIP_DIR)/include/scip/scip.h),)
	$(error Third party SCIP files was not found! did you run 'make third_party' or set UNIX_SCIP_DIR ?)
else
	$(info SCIP: found)
endif
endif  # USE_SCIP
ifeq ($(USE_COINOR),OFF)
	$(info Coin OR (CLP, CBC): disabled)
else
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
endif  # USE_COINOR
# Optional dependencies
ifndef UNIX_CPLEX_DIR
	$(info CPLEX: not found)
else
	$(info CPLEX: found)
endif
ifndef UNIX_GLPK_DIR
	$(info GLPK: not found)
else
	$(info GLP: found)
endif
ifndef UNIX_XPRESS_DIR
	$(info XPRESS: not found)
else
	$(info XPRESS: found)
endif
	$(TOUCH) $@

.PHONY: build_third_party
build_third_party: \
 Makefile.local \
 install_deps_directories \
 install_protobuf \
 install_absl \
 install_cbc \
 install_scip

.PHONY: install_deps_directories
install_deps_directories: \
 dependencies/install/bin \
 dependencies/install/lib/pkgconfig \
 dependencies/install/include/coin

dependencies/sources:
	$(MKDIR_P) dependencies$Ssources

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
	@echo "#   e.g. UNIX_CPLEX_DIR = /opt/CPLEX_Studio-X.Y" >> Makefile.local
	@echo >> Makefile.local
	@echo "# SCIP is enabled and built-in by default. To disable SCIP support" >> Makefile.local
	@echo "# completely, uncomment the following line:">> Makefile.local
	@echo "# USE_SCIP = OFF" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GLPK_DIR to point to a compiled version of GLPK to use it" >> Makefile.local
	@echo "#   e.g. UNIX_GLPK_DIR = /opt/glpk-x.y.z" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_XPRESS_DIR to use XPRESS MP" >> Makefile.local
	@echo "#   e.g. on Mac OS X: UNIX_XPRESS_DIR = /Applications/FICO\ Xpress/xpressmp" >> Makefile.local
	@echo "#   e.g. on linux: UNIX_XPRESS_DIR = /opt/xpressmp" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Coin OR solvers (CLP, CBC) are enabled and built-in by default." >> Makefile.local
	@echo "# To disable Coin OR support completely, uncomment the following line:">> Makefile.local
	@echo "# USE_COINOR = OFF" >> Makefile.local
	@echo >> Makefile.local
	@echo "# If Coin OR solvers are enabled, by default 'make third_party' will download" >> Makefile.local
	@echo "# the source code and compile it locally." >> Makefile.local
	@echo "# To override this behavior, please define the below directories." >> Makefile.local
	@echo "# Define UNIX_CBC_DIR to depend on external CBC dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_CBC_DIR = /opt/cbc-x.y.z" >> Makefile.local
	@echo "#   If you use a splitted version of CBC you can also define:" >> Makefile.local
	@echo "#     UNIX_CLP_DIR, UNIX_CGL_DIR, UNIX_OSI_DIR, UNIX_COINUTILS_DIR" >> Makefile.local
	@echo "#   note: by default they all point to UNIX_CBC_DIR" >> Makefile.local
	@echo >> Makefile.local
	@echo "## REQUIRED DEPENDENCIES ##" >> Makefile.local
	@echo "# By default they will be automatically built -> nothing to define" >> Makefile.local
	@echo "# Define UNIX_PROTOBUF_DIR to depend on external Protobuf dynamic library" >> Makefile.local
	@echo "#   e.g. UNIX_PROTOBUF_DIR = /opt/protobuf-x.y.z" >> Makefile.local
	@echo "# Define UNIX_PROTOC_BINARY to use a custom version." >> Makefile.local
	@echo "#   e.g. UNIX_PROTOC_BINARY = /opt/protoc-x.y.z/bin/protoc" >> Makefile.local
	@echo "#   (default: UNIX_PROTOBUF_DIR/bin/protoc)" >> Makefile.local
	@echo >> Makefile.local
	@echo "# note: You don't need to run \"make third_party\" if you only use external dependencies" >> Makefile.local
	@echo "# i.e. You have defined all UNIX_PROTOBUF_DIR and UNIX_CBC_DIR" >> Makefile.local


################
##  Protobuf  ##
################
# This uses Protobuf cmake-based build.
.PHONY: install_protobuf
install_protobuf: dependencies/install/lib/libprotobuf.a

PROTOBUF_SRCDIR = dependencies/sources/protobuf-$(PROTOBUF_TAG)
dependencies/install/lib/libprotobuf.a: $(PROTOBUF_SRCDIR) | dependencies/install
	cd $(PROTOBUF_SRCDIR) && \
  $(SET_COMPILER) $(CMAKE) -Hcmake -Bbuild_cmake \
    -DCMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)/dependencies/install" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_CXX_EXTENSIONS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_BUILD_EXAMPLES=OFF \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$(MAC_MIN_VERSION)" \
    -DCMAKE_INSTALL_PREFIX=../../install && \
  $(CMAKE) --build build_cmake --config Release -v -- -j4 && \
  $(CMAKE) --build build_cmake --config Release --target install

$(PROTOBUF_SRCDIR): patches/protobuf-$(PROTOBUF_TAG).patch | dependencies/sources
	-$(DELREC) $(PROTOBUF_SRCDIR)
	git clone --quiet -b $(PROTOBUF_TAG) https://github.com/google/protobuf.git $(PROTOBUF_SRCDIR)
	cd $(PROTOBUF_SRCDIR) && git apply "$(OR_TOOLS_TOP)/patches/protobuf-$(PROTOBUF_TAG).patch"

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

PROTOBUF_LNK = $(STATIC_PROTOBUF_LNK)

DEPENDENCIES_INC += $(PROTOBUF_INC)
SWIG_INC += $(PROTOBUF_SWIG)
DEPENDENCIES_LNK += $(PROTOBUF_LNK)

# Define Protoc
ifeq ($(PLATFORM),LINUX)
 PROTOC = \
LD_LIBRARY_PATH="$(UNIX_PROTOBUF_DIR)/lib64":"$(UNIX_PROTOBUF_DIR)/lib":$(LD_LIBRARY_PATH) $(PROTOC_BINARY)
else
 PROTOC = \
DYLD_LIBRARY_PATH="$(UNIX_PROTOBUF_DIR)/lib":$(DYLD_LIBRARY_PATH) $(PROTOC_BINARY)
endif

# Install Java protobuf
#  - Compile generic message proto.
#  - Compile duration.proto
dependencies/install/lib/protobuf.jar: | dependencies/install/lib/libprotobuf.a
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java && \
 $(PROTOC) --java_out=core/src/main/java -I../src \
 ../src/google/protobuf/descriptor.proto
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java && \
 $(PROTOC) --java_out=core/src/main/java -I../src \
 ../src/google/protobuf/duration.proto
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && \
 "$(JAVAC_BIN)" com/google/protobuf/*java
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && \
 "$(JAR_BIN)" cvf ../../../../../../../install/lib/protobuf.jar com/google/protobuf/*class

##################
##  ABSEIL-CPP  ##
##################
# This uses abseil-cpp cmake-based build.
install_absl: dependencies/install/lib/libabsl.a

dependencies/install/lib/libabsl.a: dependencies/sources/abseil-cpp-$(ABSL_TAG) | dependencies/install
	cd dependencies/sources/abseil-cpp-$(ABSL_TAG) && \
  $(SET_COMPILER) $(CMAKE) -H. -Bbuild_cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_CXX_EXTENSIONS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)/dependencies/install" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$(MAC_MIN_VERSION)" \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=../../install && \
  $(CMAKE) --build build_cmake --config Release -v -- -j4 && \
  $(CMAKE) --build build_cmake --config Release --target install

dependencies/sources/abseil-cpp-$(ABSL_TAG): | dependencies/sources
	-$(DELREC) dependencies/sources/abseil-cpp-$(ABSL_TAG)
	git clone --quiet https://github.com/abseil/abseil-cpp.git dependencies/sources/abseil-cpp-$(ABSL_TAG)
	cd dependencies/sources/abseil-cpp-$(ABSL_TAG) && git reset --hard $(ABSL_TAG)
#	cd dependencies/sources/abseil-cpp-$(ABSL_TAG) && git apply "$(OR_TOOLS_TOP)/patches/abseil-cpp-$(ABSL_TAG).patch"

ABSL_INC = -I$(UNIX_ABSL_DIR)/include
ABSL_SWIG = $(ABSL_INC)
_ABSL_STATIC_LIB_DIR = $(dir $(wildcard \
 $(UNIX_ABSL_DIR)/lib*/libabsl_base.a \
 $(UNIX_ABSL_DIR)/lib/*/libabsl_base.a))
STATIC_ABSL_LNK = \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_parse.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_usage.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_usage_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_marshalling.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_reflection.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_config.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_private_handle_accessor.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_commandlineflag.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_commandlineflag_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_flags_program_name.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_distributions.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_seed_sequences.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_internal_pool_urbg.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_internal_randen.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_internal_randen_hwaes.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_internal_randen_hwaes_impl.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_internal_randen_slow.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_internal_platform.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_internal_seed_material.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_random_seed_gen_exception.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_raw_hash_set.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_hashtablez_sampler.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_hash.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_city.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_low_level_hash.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_leak_check.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_statusor.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_status.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_cord.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_cordz_info.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_cord_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_cordz_functions.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_exponential_biased.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_cordz_handle.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_bad_optional_access.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_bad_variant_access.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_str_format_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_synchronization.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_stacktrace.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_symbolize.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_debugging_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_demangle_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_graphcycles_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_malloc_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_time.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_strings.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_strings_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_base.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_spinlock_wait.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_throw_delegate.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_int128.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_civil_time.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_time_zone.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_bad_any_cast_impl.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_raw_logging_internal.a \
$(_ABSL_STATIC_LIB_DIR)libabsl_log_severity.a

ABSL_LNK = $(STATIC_ABSL_LNK)

DEPENDENCIES_INC += $(ABSL_INC)
SWIG_INC += $(ABSL_SWIG)
DEPENDENCIES_LNK += $(ABSL_LNK)

###################
##  COIN-OR-CBC  ##
###################
.PHONY: install_cbc
ifeq ($(USE_COINOR),OFF)
install_cbc:
else
install_cbc: dependencies/install/lib/libCbc.a

CBC_SRCDIR = dependencies/sources/Cbc-$(CBC_TAG)
dependencies/install/lib/libCbc.a: install_cgl $(CBC_SRCDIR)
	cd $(CBC_SRCDIR) && $(SET_COMPILER) $(SET_COIN_OPT) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --enable-static --disable-shared \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    --enable-cbc-parallel \
    ADD_CXXFLAGS="-w -DCBC_THREAD_SAFE -DCBC_NO_INTERRUPT $(MAC_VERSION) -std=c++11"
	$(SET_COMPILER) $(SET_COIN_OPT) make -C $(CBC_SRCDIR)
	$(SET_COMPILER) $(SET_COIN_OPT) make install -C $(CBC_SRCDIR)

$(CBC_SRCDIR): | dependencies/sources
	-$(DELREC) $(CBC_SRCDIR)
	git clone --quiet -b releases/$(CBC_TAG) https://github.com/coin-or/Cbc.git dependencies/sources/Cbc-$(CBC_TAG)

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
CBC_LNK = $(STATIC_CBC_LNK)

###################
##  COIN-OR-CGL  ##
###################
.PHONY: install_cgl
install_cgl: dependencies/install/lib/libCgl.a

CGL_SRCDIR = dependencies/sources/Cgl-$(CGL_TAG)
dependencies/install/lib/libCgl.a: install_clp $(CGL_SRCDIR)
	cd $(CGL_SRCDIR) && $(SET_COMPILER) $(SET_COIN_OPT) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --enable-static --disable-shared \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION) -std=c++11"
	$(SET_COMPILER) $(SET_COIN_OPT) make -C $(CGL_SRCDIR)
	$(SET_COMPILER) $(SET_COIN_OPT) make install -C $(CGL_SRCDIR)

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
CGL_LNK = $(STATIC_CGL_LNK)

###################
##  COIN-OR-CLP  ##
###################
.PHONY: install_clp
install_clp: dependencies/install/lib/libClp.a

CLP_SRCDIR = dependencies/sources/Clp-$(CLP_TAG)
dependencies/install/lib/libClp.a: install_osi $(CLP_SRCDIR)
	cd $(CLP_SRCDIR) && $(SET_COMPILER) $(SET_COIN_OPT) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --enable-static --disable-shared \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION) -std=c++11"
	$(SET_COMPILER) $(SET_COIN_OPT) make -C $(CLP_SRCDIR)
	$(SET_COMPILER) $(SET_COIN_OPT) make install -C $(CLP_SRCDIR)

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
CLP_LNK = $(STATIC_CLP_LNK)

###################
##  COIN-OR-OSI  ##
###################
.PHONY: install_osi
install_osi: dependencies/install/lib/libOsi.a

OSI_SRCDIR = dependencies/sources/Osi-$(OSI_TAG)
dependencies/install/lib/libOsi.a: install_coinutils $(OSI_SRCDIR)
	cd $(OSI_SRCDIR) && $(SET_COMPILER) $(SET_COIN_OPT) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --enable-static --disable-shared \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --with-coinutils \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION) -std=c++11"
	$(SET_COMPILER) $(SET_COIN_OPT) make -C $(OSI_SRCDIR)
	$(SET_COMPILER) $(SET_COIN_OPT) make install -C $(OSI_SRCDIR)

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
OSI_LNK = $(STATIC_OSI_LNK)

#########################
##  COIN-OR-COINUTILS  ##
#########################
.PHONY: install_coinutils
install_coinutils: dependencies/install/lib/libCoinUtils.a

COINUTILS_SRCDIR = dependencies/sources/CoinUtils-$(COINUTILS_TAG)
dependencies/install/lib/libCoinUtils.a: $(COINUTILS_SRCDIR) | \
 dependencies/install/lib/pkgconfig dependencies/install/include/coin
	cd $(COINUTILS_SRCDIR) && $(SET_COMPILER) $(SET_COIN_OPT) ./configure \
    --prefix=$(OR_ROOT_FULL)/dependencies/install \
    --enable-static --disable-shared \
    --disable-bzlib \
    --disable-debug \
    --without-blas \
    --without-lapack \
    --without-glpk \
    --with-pic \
    --disable-dependency-tracking \
    --enable-dependency-linking \
    ADD_CXXFLAGS="-w $(MAC_VERSION) -std=c++11"
	$(SET_COMPILER) $(SET_COIN_OPT) make -C $(COINUTILS_SRCDIR)
	$(SET_COMPILER) $(SET_COIN_OPT) make install -C $(COINUTILS_SRCDIR)

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
COINUTILS_LNK = $(STATIC_COINUTILS_LNK)

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

DEPENDENCIES_INC += $(COIN_INC)
SWIG_INC += $(COIN_SWIG)
DEPENDENCIES_LNK += $(COIN_LNK)
endif  # USE_COINOR

#########################
##  SCIP               ##
#########################
.PHONY: install_scip
ifeq ($(USE_SCIP),OFF)
install_scip: $(GEN_DIR)/ortools/linear_solver/lpi_glop.cc

$(GEN_DIR)/ortools/linear_solver/lpi_glop.cc: | $(GEN_DIR)/ortools/linear_solver
	touch $(GEN_DIR)/ortools/linear_solver/lpi_glop.cc
else
install_scip: dependencies/install/lib/libscip.a $(GEN_DIR)/ortools/linear_solver/lpi_glop.cc

SCIP_SRCDIR = dependencies/sources/scip-$(SCIP_TAG)
dependencies/install/lib/libscip.a: $(SCIP_SRCDIR) | dependencies/install
	cd $(SCIP_SRCDIR) && \
	$(SET_COMPILER) $(CMAKE) -H. -Bbuild_cmake \
    -DCMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)/dependencies/install" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_CXX_EXTENSIONS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DSHARED=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_TESTING=OFF \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$(MAC_MIN_VERSION)" \
    -DREADLINE=OFF \
    -DGMP=OFF \
    -DPAPILO=OFF \
    -DZIMPL=OFF \
    -DIPOPT=OFF \
    -DAMPL=OFF \
    -DTPI="tny" \
    -DEXPRINT="none" \
    -DLPS="none" \
    -DSYM="none" \
    -DCMAKE_INSTALL_PREFIX=../../install && \
  $(CMAKE) --build build_cmake --config Release -v -- -j4 && \
  $(CMAKE) --build build_cmake --config Release --target install

$(SCIP_SRCDIR): | dependencies/sources
	-$(DELREC) $(SCIP_SRCDIR)
	git clone --quiet -b $(SCIP_TAG) https://github.com/scipopt/scip.git $(SCIP_SRCDIR)
#	cd $(SCIP_SRCDIR) && git apply --ignore-whitespace "$(OR_TOOLS_TOP)/patches/scip-$(SCIP_TAG).patch"


$(GEN_DIR)/ortools/linear_solver/lpi_glop.cc: $(SCIP_SRCDIR) | $(GEN_DIR)/ortools/linear_solver
	$(COPY) $(SCIP_SRCDIR)/src/lpi/lpi_glop.cpp $(GEN_DIR)/ortools/linear_solver/lpi_glop.cc

SCIP_INC = -I$(UNIX_SCIP_DIR)/include -DUSE_SCIP -DNO_CONFIG_HEADER
SCIP_SWIG = $(SCIP_INC)
SCIP_LNK = $(UNIX_SCIP_DIR)/lib/libscip.a

DEPENDENCIES_INC += $(SCIP_INC)
SWIG_INC += $(SCIP_SWIG)
DEPENDENCIES_LNK += $(SCIP_LNK)
endif

############
##  SWIG  ##
############
# Swig is only needed when building .Net, Java or Python wrapper
SWIG_BINARY = $(shell $(WHICH) $(UNIX_SWIG_BINARY))
#$(error "Can't find $(UNIX_SWIG_BINARY). Please verify UNIX_SWIG_BINARY")
SWIG_VERSION = $(shell $(SWIG_BINARY) -version | grep Version | cut -d " " -f 3)
ifeq ("$(SWIG_VERSION)","4.0.0")
SWIG_DOXYGEN = -doxygen
endif
ifeq ("$(SWIG_VERSION)","4.0.1")
SWIG_DOXYGEN = -doxygen
endif
ifeq ("$(SWIG_VERSION)","4.0.2")
SWIG_DOXYGEN = -doxygen
endif

test_doxy:
ifneq ("$(PYTHON_EXECUTABLE)","")
	echo "Pass 1"
ifeq ($(shell "$(PYTHON_EXECUTABLE)" -c "from sys import version_info as v; print (str(v[0]))"),3)
	echo "SWIG_PY_DOXYGEN = -doxygen"
endif
endif
	echo \'$(shell "$(PYTHON_EXECUTABLE)" -c "from sys import version_info as v; print (str(v[0]))")\'

.PHONY: clean_third_party # Clean everything. Remember to also delete archived dependencies, i.e. in the event of download failure, etc.
clean_third_party:
	-$(DEL) Makefile.local
	-$(DEL) dependencies/check.log
	-$(DELREC) dependencies/sources/protobuf*
	-$(DELREC) dependencies/sources/abseil-cpp*
	-$(DELREC) dependencies/sources/google*
	-$(DELREC) dependencies/sources/Cbc*
	-$(DELREC) dependencies/sources/Cgl*
	-$(DELREC) dependencies/sources/Clp*
	-$(DELREC) dependencies/sources/Osi*
	-$(DELREC) dependencies/sources/CoinUtils*
	-$(DELREC) dependencies/sources/scip*
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
	@echo UNIX_SCIP_DIR = $(UNIX_SCIP_DIR)
	@echo SCIP_INC = $(SCIP_INC)
	@echo SCIP_LNK = $(SCIP_LNK)
ifdef UNIX_GLPK_DIR
	@echo UNIX_GLPK_DIR = $(UNIX_GLPK_DIR)
	@echo GLPK_INC = $(GLPK_INC)
	@echo GLPK_LNK = $(GLPK_LNK)
endif
ifdef UNIX_CPLEX_DIR
	@echo UNIX_CPLEX_DIR = $(UNIX_CPLEX_DIR)
	@echo CPLEX_INC = $(CPLEX_INC)
	@echo CPLEX_LNK = $(CPLEX_LNK)
endif
ifdef UNIX_XPRESS_DIR
	@echo UNIX_XPRESS_DIR = $(UNIX_XPRESS_DIR)
	@echo XPRESS_INC = $(XPRESS_INC)
	@echo XPRESS_LNK = $(XPRESS_LNK)
endif
	@echo SWIG_VERSION = $(SWIG_VERSION)
	@echo
