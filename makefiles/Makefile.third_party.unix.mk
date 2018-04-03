.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.unix.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo

# Tags of dependencies to checkout.
GFLAGS_TAG = 2.2.1
PROTOBUF_TAG = 3.5.1
GLOG_TAG = 0.3.5
CBC_TAG = 2.9.9
PATCHELF_TAG = 0.9

# Detect if patchelf is needed
ifeq ($(PLATFORM), LINUX)
    PATCHELF=dependencies/install/bin/patchelf
endif

ifeq ($(PLATFORM), MACOSX)
  SET_COMPILER = CXX="$(CCC)"
endif

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party: makefile_third_party install_third_party

.PHONY: third_party_check # Check if "make third_party" have been run or not
third_party_check:
ifeq ($(wildcard dependencies/install/include/gflags/gflags.h),)
	@echo "One of the third party files was not found! did you run 'make third_party'?" && exit 1
endif

# Create missing directories
MISSING_DIRECTORIES = \
	dependencies/install \
	dependencies/archives \
	bin \
	lib \
	objs/algorithms \
	objs/base \
	objs/bop \
	objs/com/google/ortools \
	objs/constraint_solver \
	objs/data \
	objs/flatzinc \
	objs/glop \
	objs/graph \
	objs/linear_solver \
	objs/lp_data \
	objs/sat \
	objs/port \
	objs/swig \
	objs/util \
	ortools/gen/com/google/ortools/algorithms \
	ortools/gen/com/google/ortools/constraintsolver \
	ortools/gen/com/google/ortools/flatzinc \
	ortools/gen/com/google/ortools/graph \
	ortools/gen/com/google/ortools/sat \
	ortools/gen/com/google/ortools/linearsolver \
	ortools/gen/com/google/ortools/properties \
	ortools/gen/ortools/algorithms \
	ortools/gen/ortools/bop \
	ortools/gen/ortools/constraint_solver \
	ortools/gen/ortools/data \
	ortools/gen/ortools/flatzinc \
	ortools/gen/ortools/glop \
	ortools/gen/ortools/graph \
	ortools/gen/ortools/linear_solver \
	ortools/gen/ortools/sat

.PHONY: makefile_third_party missing_directories
missing_directories: $(MISSING_DIRECTORIES)

install_third_party: \
	missing_directories \
	install_cbc \
	install_gflags \
	install_glog \
	install_protobuf \
	$(CSHARP_THIRD_PARTY)

dependencies/archives:
	$(MKDIR_P) dependencies/archives

dependencies/install:
	$(MKDIR_P) dependencies/install

bin:
	$(MKDIR_P) bin

lib:
	$(MKDIR_P) lib

objs/algorithms:
	$(MKDIR_P) objs$Salgorithms

objs/base:
	$(MKDIR_P) objs$Sbase

objs/bop:
	$(MKDIR_P) objs$Sbop

objs/com/google/ortools:
	$(MKDIR_P) objs$Scom$Sgoogle$Sortools

objs/constraint_solver:
	$(MKDIR_P) objs$Sconstraint_solver

objs/data:
	$(MKDIR_P) objs$Sdata

objs/flatzinc:
	$(MKDIR_P) objs$Sflatzinc

objs/glop:
	$(MKDIR_P) objs$Sglop

objs/graph:
	$(MKDIR_P) objs$Sgraph

objs/linear_solver:
	$(MKDIR_P) objs$Slinear_solver

objs/lp_data:
	$(MKDIR_P) objs$Slp_data

objs/port:
	$(MKDIR_P) objs$Sport

objs/sat:
	$(MKDIR_P) objs$Ssat

objs/swig:
	$(MKDIR_P) objs$Sswig

objs/util:
	$(MKDIR_P) objs$Sutil

ortools/gen/com/google/ortools/algorithms:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Salgorithms

ortools/gen/com/google/ortools/constraintsolver:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Sconstraintsolver

ortools/gen/com/google/ortools/graph:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Sgraph

ortools/gen/com/google/ortools/linearsolver:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Slinearsolver

ortools/gen/com/google/ortools/flatzinc:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Sflatzinc

ortools/gen/com/google/ortools/sat:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Ssat

ortools/gen/com/google/ortools/properties:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Sproperties

ortools/gen/ortools/algorithms:
	$(MKDIR_P) ortools$Sgen$Sortools$Salgorithms

ortools/gen/ortools/bop:
	$(MKDIR_P) ortools$Sgen$Sortools$Sbop

ortools/gen/ortools/constraint_solver:
	$(MKDIR_P) ortools$Sgen$Sortools$Sconstraint_solver

ortools/gen/ortools/data:
	$(MKDIR_P) ortools$Sgen$Sortools$Sdata

ortools/gen/ortools/flatzinc:
	$(MKDIR_P) ortools$Sgen$Sortools$Sflatzinc

ortools/gen/ortools/glop:
	$(MKDIR_P) ortools$Sgen$Sortools$Sglop

ortools/gen/ortools/graph:
	$(MKDIR_P) ortools$Sgen$Sortools$Sgraph

ortools/gen/ortools/linear_solver:
	$(MKDIR_P) ortools$Sgen$Sortools$Slinear_solver

ortools/gen/ortools/sat:
	$(MKDIR_P) ortools$Sgen$Sortools$Ssat

# Install gflags. This uses cmake.
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
	         -D CMAKE_INSTALL_PREFIX=../../../install \
		 -D CMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
	         ..

dependencies/sources/gflags-$(GFLAGS_TAG)/CMakeLists.txt:
	git clone --quiet -b v$(GFLAGS_TAG) https://github.com/gflags/gflags.git dependencies/sources/gflags-$(GFLAGS_TAG)

# Install protocol buffers.
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

# Install GLOG.
install_glog: dependencies/install/include/glog/logging.h

dependencies/install/include/glog/logging.h: dependencies/sources/glog-$(GLOG_TAG)/build_cmake/Makefile
	cd dependencies/sources/glog-$(GLOG_TAG)/build_cmake && $(SET_COMPILER) make -j 4 && make install
	touch $@

dependencies/sources/glog-$(GLOG_TAG)/build_cmake/Makefile: dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt | install_gflags
	-$(MKDIR) dependencies/sources/glog-$(GLOG_TAG)/build_cmake
	cd dependencies/sources/glog-$(GLOG_TAG)/build_cmake && \
	  $(CMAKE) -D CMAKE_INSTALL_PREFIX=../../../install \
                   -D BUILD_SHARED_LIBS=OFF \
                   -D CMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
                   -D CMAKE_PREFIX_PATH="$(OR_TOOLS_TOP)/dependencies/install" \
	           ..

dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt:
	git clone --quiet -b v$(GLOG_TAG) https://github.com/google/glog.git dependencies/sources/glog-$(GLOG_TAG)

# Install Coin CBC.
install_cbc: dependencies/install/bin/cbc

dependencies/install/bin/cbc: dependencies/sources/Cbc-$(CBC_TAG)/Makefile
	cd dependencies/sources/Cbc-$(CBC_TAG) && $(SET_COMPILER) make -j 4 && $(SET_COMPILER) make install

dependencies/sources/Cbc-$(CBC_TAG)/Makefile: dependencies/sources/Cbc-$(CBC_TAG)/Makefile.in
	cd dependencies/sources/Cbc-$(CBC_TAG) && $(SET_COMPILER) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install --disable-bzlib --without-lapack --enable-static --with-pic ADD_CXXFLAGS="-w -DCBC_THREAD_SAFE -DCBC_NO_INTERRUPT $(MAC_VERSION)"

CBC_ARCHIVE:=https://www.coin-or.org/download/source/Cbc/Cbc-${CBC_TAG}.tgz

dependencies/sources/Cbc-$(CBC_TAG)/Makefile.in:
	wget --quiet --no-check-certificate --continue -P dependencies/archives ${CBC_ARCHIVE} || (@echo wget failed to dowload $(CBC_ARCHIVE), try running 'wget -P dependencies/archives --no-check-certificate $(CBC_ARCHIVE)' then rerun 'make third_party' && exit 1)
	tar xzf dependencies/archives/Cbc-${CBC_TAG}.tgz -C dependencies/sources/

# Install patchelf on linux platforms.
dependencies/install/bin/patchelf: dependencies/sources/patchelf-$(PATCHELF_TAG)/Makefile
	cd dependencies/sources/patchelf-$(PATCHELF_TAG) && make && make install

dependencies/sources/patchelf-$(PATCHELF_TAG)/Makefile: dependencies/sources/patchelf-$(PATCHELF_TAG)/configure
	cd dependencies/sources/patchelf-$(PATCHELF_TAG) && ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/patchelf-$(PATCHELF_TAG)/configure:
	git clone --quiet -b $(PATCHELF_TAG) https://github.com/NixOS/patchelf.git dependencies/sources/patchelf-$(PATCHELF_TAG)
	cd dependencies/sources/patchelf-$(PATCHELF_TAG) && ./bootstrap.sh

# Install Java protobuf
dependencies/install/lib/protobuf.jar: dependencies/install/bin/protoc
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java && \
	  ../../../install/bin/protoc --java_out=core/src/main/java -I../src \
	  ../src/google/protobuf/descriptor.proto
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && $(JAVAC_BIN) com/google/protobuf/*java
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && $(JAR_BIN) cvf ../../../../../../../install/lib/protobuf.jar com/google/protobuf/*class

# Install C# protobuf

#create .snk file if strong named dll is required (this is the default behaviour)

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
makefile_third_party: Makefile.local

Makefile.local: makefiles/Makefile.third_party.unix.mk
	-$(DEL) Makefile.local
	@echo Generating Makefile.local
	@echo JAVA_HOME = $(JAVA_HOME)>> Makefile.local
	@echo UNIX_PYTHON_VER = $(DETECTED_PYTHON_VERSION)>> Makefile.local
	@echo PATH_TO_CSHARP_COMPILER = $(DETECTED_MCS_BINARY)>> Makefile.local
	@echo DOTNET_INSTALL_PATH = $(DOTNET_INSTALL_PATH)>> Makefile.local
	@echo CLR_KEYFILE = bin/or-tools.snk>> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GLPK_DIR to point to a compiled version of GLPK to use it" >> Makefile.local
	@echo "# Define UNIX_SCIP_DIR to point to a compiled version of SCIP to use it ">> Makefile.local
	@echo "#   i.e.: <path>/scipoptsuite-4.0.1/scip" >> Makefile.local
	@echo "#   On Mac OS X, compile scip with: " >> Makefile.local
	@echo "#     make GMP=false READLINE=false TPI=tny" >> Makefile.local
	@echo "#   On Linux, compile scip with: " >> Makefile.local
	@echo "#     make GMP=false READLINE=false TPI=tny USRCFLAGS=-fPIC USRCXXFLAGS=-fPIC USRCPPFLAGS=-fPIC" >> Makefile.local
	@echo "# Define UNIX_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi" >> Makefile.local
	@echo "# Define UNIX_CPLEX_DIR to use CPLEX" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GFLAGS_DIR, UNIX_PROTOBUF_DIR, UNIX_GLOG_DIR," >> Makefile.local
	@echo "# UNIX_CLP_DIR, UNIX_CBC_DIR, UNIX_SWIG_BINARY if you wish to " >> Makefile.local
	@echo "# use a custom version. " >> Makefile.local
