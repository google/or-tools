# tags of dependencies to checkout.
GFLAGS_TAG = master
PROTOBUF_TAG = 3.0.0
SPARSEHASH_TAG = 2.0.3
CBC_TAG = 2.9.8
ZLIB_TAG = 1.2.11
ZLIB_ARCHIVE_TAG = 1211
SWIG_TAG = 3.0.10
BISON_FLEX_TAG = 2.5.5

# Build extra dependencies (GLPK, SCIP) from archive only if the archive is present.
# The archive should be glpk-4.57.tar.gz
GLPK_TAG = 4.57
# The archive should be scipoptsuite-3.2.0.tgz
SCIP_TAG = 3.2.0
SOPLEX_TAG = 2.2.0
# Version of Sulum
SULUM_TAG = 43

# Arrange for a base CLR SNK key file for signature purposes
BASE_CLR_KEYFILE = or-tools.snk

# Used to support dealing with Subversion concerns
# Remember to kill any TortoiseSVN Cache process(es) prior to clean
TSVNCACHE_EXE ?= TSVNCache.exe

# Detect if scip archive is there.
ifeq ($(wildcard dependencies/archives/scipoptsuite-$(SCIP_TAG).tgz),)
    SCIP_TARGET =
    SCIP_MAKEFILE = \# WINDOWS_SCIP_DIR support not included.
else
    SCIP_TARGET = dependencies/install/lib/scip.lib
    SCIP_MAKEFILE = WINDOWS_SCIP_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall
endif

# Detect if GLPK archive is there.
ifeq ($(wildcard dependencies/archives/glpk-$(GLPK_TAG).tar.gz),)
    GLPK_TARGET =
    GLPK_MAKEFILE = \# GLPK support not included.
else
    GLPK_TARGET = dependencies\install\bin\glpsol.exe
    GLPK_MAKEFILE = WINDOWS_GLPK_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall
endif
# Main target.
.PHONY: third_party build_third_party makefile_third_party
third_party: build_third_party makefile_third_party

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
	src/gen/com/google/ortools/graph \
	src/gen/com/google/ortools/linearsolver \
	src/gen/com/google/ortools/flatzinc \
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

build_third_party: \
	install_directories \
	missing_directories \
	install_zlib \
	install_gflags \
	install_protobuf \
	install_sparsehash \
	install_swig \
	install_coin_cbc \
	install_glpk \
	install_scip \
	install_bison

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

download_third_party: \
    dependencies/archives/zlib$(ZLIB_ARCHIVE_TAG).zip \
	dependencies/sources/gflags/autogen.sh \
	dependencies/sources/protobuf/autogen.sh \
	dependencies/sources/sparsehash/autogen.sh \
	dependencies/archives/swigwin-$(SWIG_TAG).zip \
	dependencies/sources/cbc-$(CBC_TAG)/configure

# Directories:
.PHONY: install_directories
install_directories: dependencies\install\bin dependencies\install\lib\coin dependencies\install\include\coin

dependencies\install\bin: dependencies\install
	-$(MKDIR_P) dependencies\install\bin

dependencies\install\lib: dependencies\install
	-$(MKDIR_P) dependencies\install\lib

dependencies\install\lib\coin: dependencies\install\lib
	-$(MKDIR_P) dependencies\install\lib\coin
dependencies\install\include: dependencies\install
	-$(MKDIR_P) dependencies\install\include

dependencies\install\include\coin: dependencies\install\include
	-$(MKDIR_P) dependencies\install\include\coin

dependencies\install:
	-$(MKDIR_P) dependencies\install

# Install zlib
install_zlib: dependencies\install\include\zlib.h dependencies\install\include\zconf.h dependencies\install\lib\zlib.lib

dependencies\install\include\zlib.h: dependencies\install\include dependencies\sources\zlib-$(ZLIB_TAG)\zlib.h
	$(COPY) dependencies\sources\zlib-$(ZLIB_TAG)\zlib.h dependencies\install\include

dependencies\install\include\zconf.h: dependencies\sources\zlib-$(ZLIB_TAG)\zlib.h
	$(COPY) dependencies\sources\zlib-$(ZLIB_TAG)\zconf.h dependencies\install\include
	$(TOUCH) dependencies\install\include\zconf.h

dependencies\install\lib\zlib.lib: dependencies\sources\zlib-$(ZLIB_TAG)\zlib.h
	cd dependencies\sources\zlib-$(ZLIB_TAG) && nmake -f win32\Makefile.msc zlib.lib
	$(COPY) dependencies\sources\zlib-$(ZLIB_TAG)\zlib.lib dependencies\install\lib

dependencies\sources\zlib-$(ZLIB_TAG)\zlib.h: dependencies\archives\zlib$(ZLIB_ARCHIVE_TAG).zip
	$(UNZIP_EXTRACT) dependencies\sources dependencies\archives\zlib$(ZLIB_ARCHIVE_TAG).zip
	$(TOUCH) dependencies\sources\zlib-$(ZLIB_TAG)\zlib.h

dependencies\archives\zlib$(ZLIB_ARCHIVE_TAG).zip:
	$(WGET_P) dependencies\archives http://zlib.net/zlib$(ZLIB_ARCHIVE_TAG).zip

install_gflags: dependencies/install/lib/gflags.lib

dependencies/install/lib/gflags.lib: dependencies/sources/gflags-$(GFLAGS_TAG)/INSTALL.md
	cd dependencies/sources/gflags-$(GFLAGS_TAG) && \
	  $(CMAKE) -D CMAKE_INSTALL_PREFIX=..\..\install \
	           -D CMAKE_BUILD_TYPE=Release \
	           -G "NMake Makefiles" \
	           .
	cd dependencies/sources/gflags-$(GFLAGS_TAG) && nmake install
	$(TOUCH) dependencies/install/lib/gflags_static.lib

dependencies/sources/gflags-$(GFLAGS_TAG)/INSTALL.md: dependencies/archives/gflags-$(GFLAGS_TAG).zip
	$(UNZIP_EXTRACT) dependencies/sources dependencies\archives\gflags-$(GFLAGS_TAG).zip
	-$(TOUCH) dependencies\sources\gflags-$(GFLAGS_TAG)\INSTALL.md

dependencies/archives/gflags-$(GFLAGS_TAG).zip:
#	$(WGET_P) dependencies\archives --no-check-certificate https://github.com/gflags/gflags/archive/v$(GFLAGS_TAG).zip
	$(WGET_P) dependencies\archives --no-check-certificate https://github.com/gflags/gflags/archive/master.zip
	cd dependencies/archives && rename master.zip gflags-$(GFLAGS_TAG).zip

# Install protocol buffers.
install_protobuf: dependencies\install\bin\protoc.exe  dependencies\install\include\google\protobuf\message.h

dependencies\install\bin\protoc.exe: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe
	$(COPY) dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe dependencies\install\bin
	$(COPY) dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\*.lib dependencies\install\lib

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\Release\protoc.exe: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && msbuild protobuf.sln /t:Build /p:Configuration=Release;LinkIncremental=false

dependencies\install\include\google\protobuf\message.h: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar
	cd dependencies\install && ..\..\tools\tar.exe xvmf ..\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar
#	$(COPY) dependencies\sources\protobuf-$(PROTOBUF_TAG)\src\google\protobuf-$(PROTOBUF_TAG)\stubs\stl_util.h dependencies\install\include\google\protobuf-$(PROTOBUF_TAG)\stubs

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\include.tar: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && extract_includes.bat
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && ..\..\..\..\..\tools\tar.exe cf include.tar include

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build\protobuf.sln: dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt
	-md dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build
	$(SED) -i -e '/\"\/MD\"/d' dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt
	cd dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\build && cmake -G $(CMAKE_PLATFORM) -Dprotobuf_BUILD_TESTS=OFF ..

dependencies\sources\protobuf-$(PROTOBUF_TAG)\cmake\CMakeLists.txt:
#	$(WGET_P) dependencies\archives --no-check-certificate https://github.com/google/protobuf/release/download/v$(PROTOBUF_TAG)/protobuf-$(PROTOBUF_TAG).zip
	$(WGET_P) dependencies\archives --no-check-certificate https://github.com/google/protobuf/archive/v$(PROTOBUF_TAG).zip
	$(UNZIP_EXTRACT) dependencies\sources dependencies\archives\v$(PROTOBUF_TAG).zip

# Install sparsehash.
install_sparsehash: dependencies\install\include\google\dense_hash_map

dependencies\install\include\google\dense_hash_map: dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\autogen.sh
	-md dependencies\install\include\sparsehash
	-md dependencies\install\include\sparsehash\internal
	-md dependencies\install\include\google
	-md dependencies\install\include\google\sparsehash
	-$(COPY) dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\src\sparsehash\* dependencies\install\include\sparsehash
	-$(COPY) dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\src\sparsehash\internal\* dependencies\install\include\sparsehash\internal
	-$(COPY) dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\src\google\* dependencies\install\include\google
	-$(COPY) dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\src\windows\google\* dependencies\install\include\google
	-$(COPY) dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\src\windows\google\sparsehash\* dependencies\install\include\google\sparsehash
	-$(COPY) dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\src\windows\sparsehash\internal\* dependencies\install\include\sparsehash\internal

dependencies\sources\sparsehash-sparsehash-$(SPARSEHASH_TAG)\autogen.sh:
	$(WGET_P) dependencies\archives --no-check-certificate https://github.com/sparsehash/sparsehash/archive/sparsehash-$(SPARSEHASH_TAG).zip
	$(UNZIP_EXTRACT) dependencies\sources dependencies\archives\sparsehash-$(SPARSEHASH_TAG).zip


# Install Coin CBC.
install_coin_cbc: dependencies\install\bin\cbc.exe

dependencies\install\bin\cbc.exe: dependencies\sources\cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\*.lib dependencies\install\lib\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Cbc\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Clp\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Clp\src\OsiClp\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\CoinUtils\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Cgl\src\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Osi\src\Osi\*.hpp dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Cbc\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Clp\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\CoinUtils\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Cgl\src\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Osi\src\Osi\*.h dependencies\install\include\coin
	$(COPY) dependencies\sources\cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe dependencies\install\bin

dependencies\sources\cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10\$(CBC_PLATFORM)\cbc.exe: dependencies\sources\cbc-$(CBC_TAG)\configure
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SClp$SMSVisualStudio$Sv10$SlibOsiClp$SlibOsiClp.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SClp$SMSVisualStudio$Sv10$SlibClp$SlibClp.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SCbc$SMSVisualStudio$Sv10$SlibOsiCbc$SlibOsiCbc.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SCbc$SMSVisualStudio$Sv10$SlibCbc$SlibCbc.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SCbc$SMSVisualStudio$Sv10$Scbc$Scbc.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SCbc$SMSVisualStudio$Sv10$SlibCbcSolver$SlibCbcSolver.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SOsi$SMSVisualStudio$Sv10$SlibOsi$SlibOsi.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SCoinUtils$SMSVisualStudio$Sv10$SlibCoinUtils$SlibCoinUtils.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssources$Scbc-$(CBC_TAG)$SCgl$SMSVisualStudio$Sv10$SlibCgl$SlibCgl.vcxproj $(VS_RELEASE)
	$(SED) -i 's/CBC_BUILD;/CBC_BUILD;CBC_THREAD_SAFE;CBC_NO_INTERRUPT;/g' dependencies$Ssources$Scbc-$(CBC_TAG)$SCbc$SMSVisualStudio$Sv10$SlibCbcSolver$SlibCbcSolver.vcxproj
	cd dependencies\sources\cbc-$(CBC_TAG)\Cbc\MSVisualStudio\v10 && msbuild Cbc.sln /t:cbc /p:Configuration=Release;BuildCmd=ReBuild

dependencies\sources\cbc-$(CBC_TAG)\configure:
	$(SVN) co https://projects.coin-or.org/svn/Cbc/releases/$(CBC_TAG) dependencies/sources/cbc-$(CBC_TAG)
	$(SED) -i -e "s/#  include <direct.h>/#  include <direct.h>\n#  include <cctype>/g" dependencies\sources\cbc-$(CBC_TAG)\CoinUtils\src\CoinHelperFunctions.hpp


# Install SWIG.
install_swig: dependencies\install\swigwin-$(SWIG_TAG)\swig.exe

dependencies\install\swigwin-$(SWIG_TAG)\swig.exe: dependencies\archives\swigwin-$(SWIG_TAG).zip
	$(UNZIP_EXTRACT) dependencies/install dependencies\archives\swigwin-$(SWIG_TAG).zip
	$(TOUCH) dependencies\install\swigwin-$(SWIG_TAG)\swig.exe

dependencies\archives\swigwin-$(SWIG_TAG).zip:
	$(WGET_P) dependencies\archives --no-check-certificate http://prdownloads.sourceforge.net/swig/swigwin-$(SWIG_TAG).zip || (@echo wget failed to dowload http://prdownloads.sourceforge.net/swig/swigwin-$(SWIG_TAG).zip, try running '$(WGET_P) dependencies\archives --no-check-certificate http://prdownloads.sourceforge.net/swig/swigwin-$(SWIG_TAG).zip' then rerun 'make third_party' && exit 1)

# Install glpk if needed.
install_glpk: $(GLPK_TARGET)

dependencies\install\bin\glpsol.exe: dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM)\glpsol.exe
	$(COPY) dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM)\glpk.lib dependencies\install\lib
	$(COPY) dependencies\sources\glpk-$(GLPK_TAG)\src\glpk.h dependencies\install\include
	$(COPY) dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM)\glpsol.exe dependencies\install\bin

 dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM)\glpsol.exe: dependencies\sources\glpk-$(GLPK_TAG)\configure
	$(COPY) dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM)\config_VC  dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM)\config.h
	cd dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM) && nmake -f makefile_VC

dependencies\sources\glpk-$(GLPK_TAG)\configure: dependencies\archives\glpk-$(GLPK_TAG).tar.gz
	cd dependencies\sources && ..\..\tools\gzip -dc ..\archives\glpk-$(GLPK_TAG).tar.gz | ..\..\tools\tar.exe xvmf -
	$(SED) -i -e 's/nologo/nologo \/MD/g' dependencies\sources\glpk-$(GLPK_TAG)\$(GLPK_PLATFORM)/Makefile_VC

# Install scip if needed.
install_scip: $(SCIP_TARGET)

dependencies/install/lib/scip.lib: dependencies/archives/scipoptsuite-$(SCIP_TAG).tgz
#	cd dependencies\install && ..\..\tools\gzip -dc ..\archives\scipoptsuite-$(SCIP_TAG).tgz | ..\..\tools\tar.exe xvmf -
	cd dependencies\install && ..\..\tools\tar.exe xvmf ..\archives\scipoptsuite-$(SCIP_TAG).tgz
	cd dependencies\install\scipoptsuite-$(SCIP_TAG) && ..\..\..\tools\gzip -dc soplex-$(SOPLEX_TAG).tgz | ..\..\..\tools\tar.exe xvmf -
#	cd dependencies\install\scipoptsuite-$(SCIP_TAG) && ..\..\..\tools\gzip -dc scip-$(SCIP_TAG).tgz | ..\..\..\tools\tar.exe xvmf -
	cd dependencies\install\scipoptsuite-$(SCIP_TAG) && ..\..\..\tools\gzip -d scip-$(SCIP_TAG).tgz
	- cd dependencies\install\scipoptsuite-$(SCIP_TAG) && ..\..\..\tools\tar.exe xvmf scip-$(SCIP_TAG).tar
	tools\upgrade_vs_project.cmd dependencies$Ssolutions$SScip$Ssoplex$Ssoplex.vcxproj $(VS_RELEASE)
	tools\upgrade_vs_project.cmd dependencies$Ssolutions$SScip$Sscip$Sscip.vcxproj $(VS_RELEASE)
	cd dependencies\solutions\Scip && msbuild /t:soplex
	cd dependencies\solutions\Scip && msbuild /t:scip
	-$(MKDIR_P)dependencies\install\include\scip\scip
	-$(MKDIR_P)dependencies\install\include\scip\blockmemshell
	-$(MKDIR_P)dependencies\install\include\scip\lpi
	-$(MKDIR_P) dependencies\install\include\scip\nlpi
	$(COPY) dependencies\install\scipoptsuite-$(SCIP_TAG)\scip-$(SCIP_TAG)\src\scip\*.h dependencies\install\include\scip\scip
	$(COPY) dependencies\install\scipoptsuite-$(SCIP_TAG)\scip-$(SCIP_TAG)\src\lpi\*.h dependencies\install\include\scip\lpi
	$(COPY) dependencies\install\scipoptsuite-$(SCIP_TAG)\scip-$(SCIP_TAG)\src\nlpi\*.h dependencies\install\include\scip\nlpi
	$(COPY) dependencies\install\scipoptsuite-$(SCIP_TAG)\scip-$(SCIP_TAG)\src\blockmemshell\*.h dependencies\install\include\scip\blockmemshell
	$(GIT) checkout dependencies/solutions/Scip/soplex/soplex.vcxproj
	$(GIT) checkout dependencies/solutions/Scip/scip/scip.vcxproj

# Install bison and flex in one package.
install_bison: dependencies\install\bin\win_bison.exe

dependencies\install\bin\win_flex.exe: dependencies\install\bin\win_bison.exe

dependencies\install\bin\win_bison.exe: dependencies\archives\win_flex_bison-$(BISON_FLEX_TAG).zip
	$(UNZIP_EXTRACT) dependencies\install\bin dependencies\archives\win_flex_bison-$(BISON_FLEX_TAG).zip
	$(TOUCH) dependencies\install\bin/win_bison.exe

dependencies\archives\win_flex_bison-$(BISON_FLEX_TAG).zip:
	$(WGET_P) dependencies\archives --no-check-certificate https://sourceforge.net/projects/winflexbison/files/win_flex_bison-$(BISON_FLEX_TAG).zip

# Install Java protobuf

install_java_protobuf: dependencies/install/lib/protobuf.jar

dependencies/install/lib/protobuf.jar: dependencies/install/bin/protoc.exe
	cd dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Sjava && \
	  ..$S..$S..$Sinstall$Sbin$Sprotoc --java_out=core/src/main/java -I../src \
	  ../src/google/protobuf/descriptor.proto
	cd dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Sjava$Score$Ssrc$Smain$Sjava && jar cvf ..$S..$S..$S..$S..$S..$S..$Sinstall$Slib$Sprotobuf.jar com$Sgoogle$Sprotobuf$S*java

# Handle a couple of extraneous circumstances involving TortoiseSVN caching and .svn readonly attributes.
kill_tortoisesvn_cache:
ifeq ($(SYSTEM),win)
	$(TASKKILL) /IM "$(TSVNCACHE_EXE)" /F /FI "STATUS eq RUNNING"
else
# TODO: TBD: Don't know if this is a ubiquitous issue across platforms...
endif

remove_readonly_svn_attribs: kill_tortoisesvn_cache
ifeq ($(SYSTEM),win)
	if exist dependencies\sources\* $(ATTRIB) -r /s dependencies\sources\*
else
# TODO: TBD: Don't know if this is a ubiquitous issue across platforms...
endif

# Clean everything but the sources\README
clean_third_party: remove_readonly_svn_attribs
	-$(RM) Makefile.local
# Otherwise, the rest of the target is just fine just as long as the process(es) and file attributes are properly handled.
	-$(RM_RECURSE_FORCED) dependencies\install
	-$(RM_RECURSE_FORCED) dependencies\sources\cbc-*
	-$(RM_RECURSE_FORCED) dependencies\sources\gflags*
	-$(RM_RECURSE_FORCED) dependencies\sources\glpk*
	-$(RM_RECURSE_FORCED) dependencies\sources\google*
	-$(RM_RECURSE_FORCED) dependencies\sources\protobuf*
	-$(RM_RECURSE_FORCED) dependencies\sources\sparsehash*
	-$(RM_RECURSE_FORCED) dependencies\sources\zlib*

# Create Makefile.local
makefile_third_party: Makefile.local

Makefile.local: makefiles/Makefile.third_party.win.mk
	-$(DEL) Makefile.local
	@echo $(SELECTED_JDK_DEF)>> Makefile.local
	@echo $(SELECTED_PATH_TO_PYTHON)>> Makefile.local
	@echo $(SELECTED_CSC_BINARY)>> Makefile.local
	@echo #>> Makefile.local
	@echo $(GLPK_MAKEFILE)>> Makefile.local
	@echo $(SCIP_MAKEFILE)>> Makefile.local
	@echo CLR_KEYFILE = bin$S$(BASE_CLR_KEYFILE)>> Makefile.local
	@echo WINDOWS_SULUM_VERSION = $(SULUM_TAG)>> Makefile.local
	@echo # Define WINDOWS_SLM_DIR to use Sulum Optimization.>> Makefile.local
	@echo # Define WINDOWS_GUROBI_DIR and GUROBI_LIB_VERSION to use Gurobi.>> Makefile.local
	@echo #>> Makefile.local
	@echo WINDOWS_ZLIB_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall>> Makefile.local
	@echo WINDOWS_ZLIB_NAME=zlib.lib>> Makefile.local
	@echo WINDOWS_GFLAGS_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall>> Makefile.local
	@echo WINDOWS_PROTOBUF_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall>> Makefile.local
	@echo WINDOWS_SPARSEHASH_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall>> Makefile.local
	@echo WINDOWS_SWIG_BINARY = $(OR_ROOT_FULL)$Sdependencies$Sinstall$Sswigwin-$(SWIG_TAG)$Sswig.exe>> Makefile.local
	@echo WINDOWS_CLP_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall>> Makefile.local
	@echo WINDOWS_CBC_DIR = $(OR_ROOT_FULL)$Sdependencies$Sinstall>> Makefile.local
