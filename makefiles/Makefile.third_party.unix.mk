# SVN tags of dependencies to checkout.

GFLAGS_TAG = 2.2.0
PROTOBUF_TAG = 3.2.0
CBC_TAG = 2.9.8
SWIG_TAG = 3.0.12
PCRE_TAG = 8.37

HELP2MAN_TAG = 1.43.3
# Autoconf support
AUTOCONF_TAG = 2.69
AUTOMAKE_TAG = 1.15
LIBTOOL_TAG = 2.4.6

# Detect if patchelf is needed
ifeq ($(PLATFORM), LINUX)
    PATCHELF=dependencies/install/bin/patchelf
endif

ACLOCAL_TARGET = \
	dependencies/install/bin/autoconf \
	dependencies/install/bin/automake \
	dependencies/install/bin/libtool

SET_PATH = PATH=$(OR_ROOT_FULL)/dependencies/install/bin:$(PATH)
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
	install_swig \
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
install_gflags: dependencies/install/bin/gflags_completions.sh

CMAKE_MISSING = "cmake not found in /Applications, nor in the PATH. Install the official version, or from brew"

check_cmake:
ifeq ($(PLATFORM),MACOSX)
	@$(CMAKE) --version >& /dev/null || echo $(CMAKE_MISSING)
endif

dependencies/install/bin/gflags_completions.sh: dependencies/sources/gflags-$(GFLAGS_TAG)/INSTALL.md check_cmake
	cd dependencies/sources/gflags-$(GFLAGS_TAG) && $(SET_COMPILER) \
	$(CMAKE) -D BUILD_SHARED_LIBS=ON \
		 -D BUILD_STATIC_LIBS=ON \
	         -D CMAKE_INSTALL_PREFIX=../../install \
		 -D CMAKE_CXX_FLAGS="-fPIC $(MAC_VERSION)" \
	         .
	cd dependencies/sources/gflags-$(GFLAGS_TAG) && \
	$(SET_COMPILER) make -j 4 && \
	make install
	$(TOUCH) dependencies/install/bin/gflags_completions.sh
ifeq ($(PLATFORM),MACOSX)
	install_name_tool -id $(OR_TOOLS_TOP)/dependencies/install/lib/libgflags.dylib \
                          dependencies/install/lib/libgflags.dylib
endif

dependencies/sources/gflags-$(GFLAGS_TAG)/INSTALL.md:
	git clone -b v$(GFLAGS_TAG) https://github.com/gflags/gflags.git dependencies/sources/gflags-$(GFLAGS_TAG)

# Install protocol buffers.
install_protobuf: dependencies/install/bin/protoc

dependencies/install/bin/protoc: dependencies/sources/protobuf-$(PROTOBUF_TAG)/Makefile $(ACLOCAL_TARGET)
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && $(SET_PATH) $(SET_COMPILER) make install

dependencies/sources/protobuf-$(PROTOBUF_TAG)/Makefile: dependencies/sources/protobuf-$(PROTOBUF_TAG)/configure $(ACLOCAL_TARGET)
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && $(SET_PATH) $(SET_COMPILER) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install --with-pic

dependencies/sources/protobuf-$(PROTOBUF_TAG)/configure: dependencies/sources/protobuf-$(PROTOBUF_TAG)/autogen.sh $(ACLOCAL_TARGET)
	cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && $(SET_PATH) $(SET_COMPILER) ./autogen.sh

dependencies/sources/protobuf-$(PROTOBUF_TAG)/autogen.sh:
	git clone https://github.com/google/protobuf.git dependencies/sources/protobuf-$(PROTOBUF_TAG) && cd dependencies/sources/protobuf-$(PROTOBUF_TAG) && git checkout 3d9d1a1

# Install Coin CBC.
install_cbc: dependencies/install/bin/cbc

dependencies/install/bin/cbc: dependencies/sources/cbc-$(CBC_TAG)/Makefile $(ACLOCAL_TARGET)
	cd dependencies/sources/cbc-$(CBC_TAG) && $(SET_PATH) $(SET_COMPILER) make install

dependencies/sources/cbc-$(CBC_TAG)/Makefile: dependencies/sources/cbc-$(CBC_TAG)/Makefile.in $(ACLOCAL_TARGET)
	cd dependencies/sources/cbc-$(CBC_TAG) && $(SET_PATH) $(SET_COMPILER) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install --disable-bzlib --without-lapack --enable-static --enable-shared --with-pic ADD_CXXFLAGS="-DCBC_THREAD_SAFE -DCBC_NO_INTERRUPT $(MAC_VERSION)"

dependencies/sources/cbc-$(CBC_TAG)/Makefile.in:
	svn co https://projects.coin-or.org/svn/Cbc/releases/$(CBC_TAG) dependencies/sources/cbc-$(CBC_TAG)

# Install pcre (dependency of SWIG).
dependencies/install/bin/pcretest: dependencies/sources/pcre-$(PCRE_TAG)/Makefile $(ACLOCAL_TARGET)
	cd dependencies/sources/pcre-$(PCRE_TAG) && $(SET_PATH) make && make install

dependencies/sources/pcre-$(PCRE_TAG)/Makefile: dependencies/sources/pcre-$(PCRE_TAG)/configure $(ACLOCAL_TARGET)
	cd dependencies/sources/pcre-$(PCRE_TAG) && $(SET_PATH) ./configure --disable-shared --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/pcre-$(PCRE_TAG)/configure: dependencies/sources/pcre-$(PCRE_TAG)/autogen.sh $(ACLOCAL_TARGET)
	cd dependencies/sources/pcre-$(PCRE_TAG) && $(SET_PATH) ./autogen.sh

dependencies/sources/pcre-$(PCRE_TAG)/autogen.sh:
	git clone -b pcre-$(PCRE_TAG) https://github.com/Distrotech/pcre dependencies/sources/pcre-$(PCRE_TAG)

# Install SWIG.
install_swig: dependencies/install/bin/swig

dependencies/install/bin/swig: dependencies/sources/swig-$(SWIG_TAG)/Makefile $(ACLOCAL_TARGET)
	cd dependencies/sources/swig-$(SWIG_TAG) && $(SET_PATH) make && make install

dependencies/sources/swig-$(SWIG_TAG)/Makefile: dependencies/sources/swig-$(SWIG_TAG)/configure dependencies/install/bin/pcretest $(ACLOCAL_TARGET)
	cd dependencies/sources/swig-$(SWIG_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install --with-pcre-prefix=$(OR_ROOT_FULL)/dependencies/install --disable-ccache --without-octave

dependencies/sources/swig-$(SWIG_TAG)/configure: dependencies/sources/swig-$(SWIG_TAG)/autogen.sh $(ACLOCAL_TARGET)
	cd dependencies/sources/swig-$(SWIG_TAG) && $(SET_PATH) ./autogen.sh

dependencies/sources/swig-$(SWIG_TAG)/autogen.sh:
	git clone -b rel-$(SWIG_TAG) https://github.com/swig/swig dependencies/sources/swig-$(SWIG_TAG)

# Install patchelf on linux platforms.
dependencies/install/bin/patchelf: dependencies/sources/patchelf-0.8/Makefile
	cd dependencies/sources/patchelf-0.8 && make && make install

dependencies/sources/patchelf-0.8/Makefile: dependencies/sources/patchelf-0.8/configure
	cd dependencies/sources/patchelf-0.8 && ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/patchelf-0.8/configure: dependencies/archives/patchelf-0.8.tar.gz
	cd dependencies/sources && tar xzmf ../archives/patchelf-0.8.tar.gz


# Install help2man
dependencies/install/bin/help2man: dependencies/sources/help2man-$(HELP2MAN_TAG)/Makefile
	cd dependencies/sources/help2man-$(HELP2MAN_TAG) && make install

dependencies/sources/help2man-$(HELP2MAN_TAG)/Makefile: dependencies/sources/help2man-$(HELP2MAN_TAG)/configure
	cd dependencies/sources/help2man-$(HELP2MAN_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/help2man-$(HELP2MAN_TAG)/configure: dependencies/archives/help2man-$(HELP2MAN_TAG).tar.gz
	cd dependencies/sources && tar xvzmf ../archives/help2man-$(HELP2MAN_TAG).tar.gz

dependencies/archives/help2man-$(HELP2MAN_TAG).tar.gz:
	cd dependencies/archives && curl -OL http://ftpmirror.gnu.org/help2man/help2man-$(HELP2MAN_TAG).tar.gz

# Install libtool
dependencies/install/bin/libtool: dependencies/sources/libtool-$(LIBTOOL_TAG)/Makefile dependencies/install/bin/help2man
	cd dependencies/sources/libtool-$(LIBTOOL_TAG) && $(SET_PATH) make install

dependencies/sources/libtool-$(LIBTOOL_TAG)/Makefile: dependencies/sources/libtool-$(LIBTOOL_TAG)/configure
	cd dependencies/sources/libtool-$(LIBTOOL_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/libtool-$(LIBTOOL_TAG)/configure: dependencies/archives/libtool-$(LIBTOOL_TAG).tar.gz
	cd dependencies/sources && tar xvzmf ../archives/libtool-$(LIBTOOL_TAG).tar.gz

dependencies/archives/libtool-$(LIBTOOL_TAG).tar.gz:
	cd dependencies/archives && curl -OL http://ftpmirror.gnu.org/libtool/libtool-$(LIBTOOL_TAG).tar.gz

# Install automake
dependencies/install/bin/automake: dependencies/sources/automake-$(AUTOMAKE_TAG)/Makefile
	cd dependencies/sources/automake-$(AUTOMAKE_TAG) && $(SET_PATH) ./bootstrap.sh
	cd dependencies/sources/automake-$(AUTOMAKE_TAG) && $(SET_PATH) make
	cd dependencies/sources/automake-$(AUTOMAKE_TAG) && $(SET_PATH) make install


dependencies/sources/automake-$(AUTOMAKE_TAG)/Makefile: dependencies/sources/automake-$(AUTOMAKE_TAG)/configure dependencies/install/bin/autoconf
	cd dependencies/sources/automake-$(AUTOMAKE_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/automake-$(AUTOMAKE_TAG)/configure: dependencies/archives/automake-$(AUTOMAKE_TAG).tar.gz
	cd dependencies/sources && tar xvzmf ../archives/automake-$(AUTOMAKE_TAG).tar.gz

dependencies/archives/automake-$(AUTOMAKE_TAG).tar.gz:
	cd dependencies/archives && curl -OL http://ftpmirror.gnu.org/automake/automake-$(AUTOMAKE_TAG).tar.gz

# Install autoconf
dependencies/install/bin/autoconf: dependencies/sources/autoconf-$(AUTOCONF_TAG)/Makefile
	cd dependencies/sources/autoconf-$(AUTOCONF_TAG) && $(SET_PATH) make && make install

dependencies/sources/autoconf-$(AUTOCONF_TAG)/Makefile: dependencies/sources/autoconf-$(AUTOCONF_TAG)/configure
	cd dependencies/sources/autoconf-$(AUTOCONF_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/autoconf-$(AUTOCONF_TAG)/configure: dependencies/archives/autoconf-$(AUTOCONF_TAG).tar.gz
	cd dependencies/sources && tar xvzmf ../archives/autoconf-$(AUTOCONF_TAG).tar.gz
	cd dependencies/sources/autoconf-$(AUTOCONF_TAG) && patch -p 1 -i ../../archives/autoconf.patch

dependencies/archives/autoconf-$(AUTOCONF_TAG).tar.gz:
	cd dependencies/archives && curl -OL http://ftpmirror.gnu.org/autoconf/autoconf-$(AUTOCONF_TAG).tar.gz

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
	@echo UNIX_GFLAGS_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
	@echo UNIX_PROTOBUF_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
	@echo UNIX_SWIG_BINARY = $(OR_ROOT_FULL)/dependencies/install/bin/swig>> Makefile.local
	@echo UNIX_CLP_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
	@echo UNIX_CBC_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
