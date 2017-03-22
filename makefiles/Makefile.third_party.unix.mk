# SVN tags of dependencies to checkout.

GFLAGS_TAG = 2.1.2
PROTOBUF_TAG = 3.0.0
CBC_TAG = 2.9.8
SWIG_TAG = 3.0.12
PCRE_TAG = 8.37
# BISON, FLEX
BISON_TAG = 3.0.4
FLEX_TAG = 2.6.0
# help2man is needed by bison
HELP2MAN_TAG = 1.43.3
# Autoconf support
AUTOCONF_TAG = 2.69
AUTOMAKE_TAG = 1.15
LIBTOOL_TAG = 2.4.6

# Build extra dependencies (GLPK, SCIP) from archive only if the
# archive is present.
#
# The GLPK archive should be glpk-4.57.tar.gz
GLPK_TAG = 4.57
# The SCIP archive should be scipoptsuite-3.2.1.tgz
SCIP_TAG = 3.2.1
# Version of Sulum
SULUM_TAG = 43

# Detect if SCIP archive is there.
ifeq ($(wildcard dependencies/archives/scipoptsuite-$(SCIP_TAG).tgz),)
    SCIP_TARGET =
    SCIP_MAKEFILE = "\# Download and put scipoptsuite-$(SCIP_TAG).tgz in the dependencies/archives directory to add support for SCIP."
else
    SCIP_TARGET = dependencies/install/scipoptsuite-$(SCIP_TAG)/scip-$(SCIP_TAG)/bin/scip
    SCIP_MAKEFILE = UNIX_SCIP_DIR = $(OR_ROOT_FULL)/dependencies/install/scipoptsuite-$(SCIP_TAG)/scip-$(SCIP_TAG)
    ifeq ($(PLATFORM), LINUX)
	BUILD_SCIP = make ZIMPL=false READLINE=false USRCXXFLAGS=-fPIC CFLAGS=-fPIC GMP=false
    endif
    ifeq ($(PLATFORM), MACOSX)
	BUILD_SCIP = make ZIMPL=false READLINE=false GMP=false
    endif
endif

# Detect if GLPK archive is there.
ifeq ($(wildcard dependencies/archives/glpk-$(GLPK_TAG).tar.gz),)
    GLPK_TARGET =
    GLPK_MAKEFILE = "\# Download and put glpk-$(GLPK_TAG).tar.gz under dependencies/archives to add support for GLPK."
else
    GLPK_TARGET = dependencies/install/bin/glpsol
    GLPK_MAKEFILE = UNIX_GLPK_DIR = $(OR_ROOT_FULL)/dependencies/install
endif

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
	src/gen/algorithms \
	src/gen/bop \
	src/gen/com/google/ortools/algorithms \
	src/gen/com/google/ortools/constraintsolver \
	src/gen/com/google/ortools/flatzinc \
	src/gen/com/google/ortools/graph \
	src/gen/com/google/ortools/linearsolver \
	src/gen/com/google/ortools/properties \
	src/gen/constraint_solver \
	src/gen/flatzinc \
	src/gen/glop \
	src/gen/graph \
	src/gen/linear_solver \
	src/gen/ortools/algorithms \
	src/gen/ortools/constraint_solver \
	src/gen/ortools/graph \
	src/gen/ortools/linear_solver \
	src/gen/sat

missing_directories: $(MISSING_DIRECTORIES)

install_third_party: \
	missing_directories \
	install_gflags \
	install_protobuf \
	install_swig \
	install_cbc \
	install_glpk \
	install_scip \
	install_bison \
	install_flex \
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

src/gen/algorithms:
	$(MKDIR_P) src$Sgen$Salgorithms

src/gen/bop:
	$(MKDIR_P) src$Sgen$Sbop

src/gen/com/google/ortools/algorithms:
	$(MKDIR_P) src$Sgen$Scom$Sgoogle$Sortools$Salgorithms

src/gen/com/google/ortools/constraintsolver:
	$(MKDIR_P) src$Sgen$Scom$Sgoogle$Sortools$Sconstraintsolver

src/gen/com/google/ortools/graph:
	$(MKDIR_P) src$Sgen$Scom$Sgoogle$Sortools$Sgraph

src/gen/com/google/ortools/linearsolver:
	$(MKDIR_P) src$Sgen$Scom$Sgoogle$Sortools$Slinearsolver

src/gen/com/google/ortools/flatzinc:
	$(MKDIR_P) src$Sgen$Scom$Sgoogle$Sortools$Sflatzinc

src/gen/com/google/ortools/properties:
	$(MKDIR_P) src$Sgen$Scom$Sgoogle$Sortools$Sproperties

src/gen/constraint_solver:
	$(MKDIR_P) src$Sgen$Sconstraint_solver

src/gen/flatzinc:
	$(MKDIR_P) src$Sgen$Sflatzinc

src/gen/glop:
	$(MKDIR_P) src$Sgen$Sglop

src/gen/graph:
	$(MKDIR_P) src$Sgen$Sgraph

src/gen/linear_solver:
	$(MKDIR_P) src$Sgen$Slinear_solver

src/gen/ortools/algorithms:
	$(MKDIR_P) src$Sgen$Sortools$Salgorithms

src/gen/ortools/constraint_solver:
	$(MKDIR_P) src$Sgen$Sortools$Sconstraint_solver

src/gen/ortools/graph:
	$(MKDIR_P) src$Sgen$Sortools$Sgraph

src/gen/ortools/linear_solver:
	$(MKDIR_P) src$Sgen$Sortools$Slinear_solver

src/gen/sat:
	$(MKDIR_P) src$Sgen$Ssat

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

# Install glpk if needed.
install_glpk: $(GLPK_TARGET)

dependencies/install/bin/glpsol: dependencies/sources/glpk-$(GLPK_TAG)/Makefile
	cd dependencies/sources/glpk-$(GLPK_TAG) && make install

dependencies/sources/glpk-$(GLPK_TAG)/Makefile: dependencies/sources/glpk-$(GLPK_TAG)/configure $(ACLOCAL_TARGET)
	cd dependencies/sources/glpk-$(GLPK_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install --with-pic

dependencies/sources/glpk-$(GLPK_TAG)/configure: dependencies/archives/glpk-$(GLPK_TAG).tar.gz
	cd dependencies/sources && tar xvzmf ../archives/glpk-$(GLPK_TAG).tar.gz

# Install scip if needed.
install_scip: $(SCIP_TARGET)

dependencies/install/scipoptsuite-$(SCIP_TAG)/scip-$(SCIP_TAG)/bin/scip: dependencies/archives/scipoptsuite-$(SCIP_TAG).tgz
	cd dependencies/install && tar xvzmf ../archives/scipoptsuite-$(SCIP_TAG).tgz && cd scipoptsuite-$(SCIP_TAG) && $(BUILD_SCIP)

# Install patchelf on linux platforms.
dependencies/install/bin/patchelf: dependencies/sources/patchelf-0.8/Makefile
	cd dependencies/sources/patchelf-0.8 && make && make install

dependencies/sources/patchelf-0.8/Makefile: dependencies/sources/patchelf-0.8/configure
	cd dependencies/sources/patchelf-0.8 && ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/patchelf-0.8/configure: dependencies/archives/patchelf-0.8.tar.gz
	cd dependencies/sources && tar xzmf ../archives/patchelf-0.8.tar.gz

# Install bison
install_bison: dependencies/install/bin/bison

dependencies/install/bin/bison: dependencies/sources/bison-$(BISON_TAG)/Makefile $(ACLOCAL_TARGET) dependencies/install/bin/help2man
	cd dependencies/sources/bison-$(BISON_TAG) && $(SET_PATH) make install

dependencies/sources/bison-$(BISON_TAG)/Makefile: dependencies/sources/bison-$(BISON_TAG)/configure $(ACLOCAL_TARGET)
	cd dependencies/sources/bison-$(BISON_TAG) && $(SET_PATH) autoreconf
	cd dependencies/sources/bison-$(BISON_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/bison-$(BISON_TAG)/configure: dependencies/archives/bison-$(BISON_TAG).tar.gz
	cd dependencies/sources && tar xvzf ../archives/bison-$(BISON_TAG).tar.gz

dependencies/archives/bison-$(BISON_TAG).tar.gz:
	cd dependencies/archives && curl -OL http://ftpmirror.gnu.org/bison/bison-$(BISON_TAG).tar.gz

# Install flex
install_flex: dependencies/install/bin/flex

dependencies/install/bin/flex: dependencies/sources/flex-$(FLEX_TAG)/Makefile $(ACLOCAL_TARGET)
	cd dependencies/sources/flex-$(FLEX_TAG) && $(SET_PATH) make install

dependencies/sources/flex-$(FLEX_TAG)/Makefile: dependencies/sources/flex-$(FLEX_TAG)/configure $(ACLOCAL_TARGET)
	cd dependencies/sources/flex-$(FLEX_TAG) && $(SET_PATH) ./configure --prefix=$(OR_ROOT_FULL)/dependencies/install

dependencies/sources/flex-$(FLEX_TAG)/configure: dependencies/archives/flex-$(FLEX_TAG).tar.bz2
	cd dependencies/sources && tar xvjmf ../archives/flex-$(FLEX_TAG).tar.bz2

dependencies/archives/flex-$(FLEX_TAG).tar.bz2:
	cd dependencies/archives && curl -OL http://sourceforge.net/projects/flex/files/flex-$(FLEX_TAG).tar.bz2

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
	@echo $(SELECTED_JDK_DEF)>> Makefile.local
	@echo UNIX_PYTHON_VER = $(DETECTED_PYTHON_VERSION)>> Makefile.local
	@echo PATH_TO_CSHARP_COMPILER = $(DETECTED_MCS_BINARY)>> Makefile.local
	@echo CLR_KEYFILE = bin/or-tools.snk>> Makefile.local
	@echo >> Makefile.local
	@echo $(GLPK_MAKEFILE)>> Makefile.local
	@echo $(SCIP_MAKEFILE)>> Makefile.local
	@echo \# Define UNIX_SLM_DIR to use Sulum Optimization.>> Makefile.local
	@echo \# Define UNIX_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi.>> Makefile.local
	@echo \# Define UNIX_CPLEX_DIR to use CPLEX.>> Makefile.local
	@echo >> Makefile.local
	@echo UNIX_GFLAGS_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
	@echo UNIX_PROTOBUF_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
	@echo UNIX_SWIG_BINARY = $(OR_ROOT_FULL)/dependencies/install/bin/swig>> Makefile.local
	@echo UNIX_CLP_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
	@echo UNIX_CBC_DIR = $(OR_ROOT_FULL)/dependencies/install>> Makefile.local
	@echo UNIX_SCIP_TAG = $(SCIP_TAG)>> Makefile.local
	@echo UNIX_SULUM_VERSION = $(SULUM_TAG) >> Makefile.local
