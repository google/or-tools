# SVN tags of dependencies to checkout.

GFLAGS_TAG = 2.2.0
PROTOBUF_TAG = 3.3.0
GLOG_TAG = 0.3.5
CBC_TAG = 2.9.8

# Detect if patchelf is needed
ifeq ($(PLATFORM), LINUX)
    PATCHELF=dependencies/install/bin/patchelf
endif

ifeq ($(PLATFORM), MACOSX)
  SET_COMPILER = CXX="$(CCC)"
endif

# Main target.
.PHONY: makefile_third_party missing_directories install_java_protobuf
third_party: makefile_third_party install_third_party

# Create missing directories

MISSING_DIRECTORIES = \
	bin \
	lib \
	objs/algorithms \
	objs/base \
	objs/bop \
	objs/com/google/ortools \
	objs/constraint_solver \
	objs/flatzinc \
	objs/glop \
	objs/graph \
	objs/linear_solver \
	objs/lp_data \
	objs/sat \
	objs/swig \
	objs/util \
	ortools/gen/com/google/ortools/algorithms \
	ortools/gen/com/google/ortools/constraintsolver \
	ortools/gen/com/google/ortools/flatzinc \
	ortools/gen/com/google/ortools/graph \
	ortools/gen/com/google/ortools/linearsolver \
	ortools/gen/com/google/ortools/properties \
	ortools/gen/ortools/algorithms \
	ortools/gen/ortools/bop \
	ortools/gen/ortools/constraint_solver \
	ortools/gen/ortools/flatzinc \
	ortools/gen/ortools/glop \
	ortools/gen/ortools/graph \
	ortools/gen/ortools/linear_solver \
	ortools/gen/ortools/sat

missing_directories: $(MISSING_DIRECTORIES)

install_third_party: \
	missing_directories \
	install_gflags \
	install_protobuf \
	install_glog \
	install_cbc \
	$(CSHARP_THIRD_PARTY)

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

ortools/gen/com/google/ortools/properties:
	$(MKDIR_P) ortools$Sgen$Scom$Sgoogle$Sortools$Sproperties

ortools/gen/ortools/algorithms:
	$(MKDIR_P) ortools$Sgen$Sortools$Salgorithms

ortools/gen/ortools/bop:
	$(MKDIR_P) ortools$Sgen$Sortools$Sbop

ortools/gen/ortools/constraint_solver:
	$(MKDIR_P) ortools$Sgen$Sortools$Sconstraint_solver

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

CMAKE_MISSING = "cmake not found in /Applications, nor in the PATH. Install the official version, or from brew"

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
	git clone -b v$(GFLAGS_TAG) https://github.com/gflags/gflags.git dependencies/sources/gflags-$(GFLAGS_TAG)

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
	git clone https://github.com/google/protobuf.git dependencies/sources/protobuf-$(PROTOBUF_TAG) && cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && git checkout 3d9d1a1

# Install GLOG.
install_glog: dependencies/install/include/glog/logging.h

dependencies/install/include/glog/logging.h: dependencies/sources/glog-$(GLOG_TAG)/build_cmake/Makefile
	cd dependencies/sources/glog-$(GLOG_TAG)/build_cmake && $(SET_COMPILER) make -j 4 && make install
	touch $@

dependencies/sources/glog-$(GLOG_TAG)/build_cmake/Makefile: dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt
	-$(MKDIR) dependencies/sources/glog-$(GLOG_TAG)/build_cmake
	cd dependencies/sources/glog-$(GLOG_TAG)/build_cmake && \
	  $(CMAKE) -D CMAKE_INSTALL_PREFIX=../../../install \
                   -D BUILD_SHARED_LIBS=OFF \
                   -D CMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
                   -D GFLAGS_DIR=../../gflags-$(GFLAGS_TAG) \
	           ..

dependencies/sources/glog-$(GLOG_TAG)/CMakeLists.txt:
	git clone -b v$(GLOG_TAG) https://github.com/google/glog.git dependencies/sources/glog-$(GLOG_TAG)

# Install Coin CBC.
install_cbc: dependencies/install/bin/cbc

dependencies/install/bin/cbc: dependencies/sources/cbc-$(CBC_TAG)/Makefile
	cd dependencies/sources/cbc-$(CBC_TAG) && $(SET_COMPILER) make -j 4 && $(SET_COMPILER) make install

dependencies/sources/cbc-$(CBC_TAG)/Makefile: dependencies/sources/cbc-$(CBC_TAG)/Makefile.in
	cd dependencies/sources/cbc-$(CBC_TAG) && $(SET_COMPILER) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install --disable-bzlib --without-lapack --enable-static --with-pic ADD_CXXFLAGS="-DCBC_THREAD_SAFE -DCBC_NO_INTERRUPT $(MAC_VERSION)"

dependencies/sources/cbc-$(CBC_TAG)/Makefile.in:
	svn co https://projects.coin-or.org/svn/Cbc/releases/$(CBC_TAG) dependencies/sources/cbc-$(CBC_TAG)

# Install patchelf on linux platforms.
dependencies/install/bin/patchelf: dependencies/sources/patchelf-0.8/Makefile
	cd dependencies/sources/patchelf-0.8 && make && make install

dependencies/sources/patchelf-0.8/Makefile: dependencies/sources/patchelf-0.8/configure
	cd dependencies/sources/patchelf-0.8 && ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/patchelf-0.8/configure: dependencies/archives/patchelf-0.8.tar.gz
	cd dependencies/sources && tar xzmf ../archives/patchelf-0.8.tar.gz


# Install Java protobuf

install_java_protobuf: dependencies/install/lib/protobuf.jar

dependencies/install/lib/protobuf.jar: dependencies/install/bin/protoc
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java && \
	  ../../../install/bin/protoc --java_out=core/src/main/java -I../src \
	  ../src/google/protobuf/descriptor.proto
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG)/java/core/src/main/java && jar cvf ../../../../../../../install/lib/protobuf.jar com/google/protobuf/*java

# Install C# protobuf

#create .snk file if strong named dll is required (this is the default behaviour)
# Clean everything.
clean_third_party:
	-$(DEL) Makefile.local
	-$(DELREC) dependencies/install
	-$(DELREC) dependencies/sources/cbc*
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

# Create Makefile.local
makefile_third_party: Makefile.local

Makefile.local: makefiles/Makefile.third_party.unix.mk
	-$(DEL) Makefile.local
	@echo Generating Makefile.local
	@echo JDK_DIRECTORY = $(JDK_DIRECTORY)>> Makefile.local
	@echo UNIX_PYTHON_VER = $(DETECTED_PYTHON_VERSION)>> Makefile.local
	@echo PATH_TO_CSHARP_COMPILER = $(DETECTED_MCS_BINARY)>> Makefile.local
	@echo CLR_KEYFILE = bin/or-tools.snk>> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GLPK_DIR to point to a compiled version of GLPK to use it" >> Makefile.local
	@echo "# Define UNIX_SCIP_DIR to point to a compiled version of SCIP to use it ">> Makefile.local
	@echo "#   i.e.: <path>/scipoptsuite-4.0.0/scip-4.0.0" >> Makefile.local
	@echo "#   compile scip with GMP=false READLINE=false" >> Makefile.local
	@echo "# Define UNIX_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi" >> Makefile.local
	@echo "# Define UNIX_CPLEX_DIR to use CPLEX" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GFLAGS_DIR, UNIX_PROTOBUF_DIR, UNIX_GLOG_DIR," >> Makefile.local
	@echo "# UNIX_CLP_DIR, UNIX_CBC_DIR, UNIX_SWIG_BINARY if you wish to " >> Makefile.local
	@echo "# use a custom version. " >> Makefile.local
