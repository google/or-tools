include(ExternalProject)

# git check 
find_package(Git)
if(NOT GIT_FOUND)
  message(FATAL_ERROR "git not found")
endif()

# svn check 
find_package(Subversion)
if(NOT SUBVERSION_FOUND)
  message(FATAL_ERROR "svn not found")
endif()

#TODO(implement make clean_third_party that cleans all the downloaded sources/it is probably only for protobuf!)
# download tags (git, svn, and URLs)
SET(GFLAGS_TAG "v2.1.2")
SET(PROTOBUF_TAG "v3.2.0")
SET(CBC_TAG "2.9.8")
SET(SPARSEHASH_TAG "sparsehash-2.0.3")
SET(SWIG_TAG "rel-3.0.10")
SET(PCRE_TAG "pcre-8.37")
SET(AUTOCONF_TAG "autoconf-2.69")
SET(AUTOMAKE_TAG "automake-1.15")
SET(LIBTOOL_TAG "libtool-2.4.6")
SET(HELP2MAN_TAG "help2man-1.43.3")
SET(SCIP_TAG "scipoptsuite-3.2.1")
SET(GLPK_TAG "glpk-4.57")

# Directories names
SET(GFLAGS_DIR "gflags-${GFLAGS_TAG}")
SET(PROTOBUF_DIR "protobuf-${PROTOBUF_TAG}")
SET(CBC_DIR "cbc-${CBC_TAG}")
SET(SPARSEHASH_DIR "${SPARSEHASH_TAG}")
SET(SWIG_DIR "${SWIG_TAG}")
SET(PCRE_DIR "${PCRE_TAG}")
SET(AUTOCONF_DIR "${AUTOCONF_TAG}")
SET(AUTOMAKE_DIR "${AUTOMAKE_TAG}")
SET(LIBTOOL_DIR "${LIBTOOL_TAG}")
SET(HELP2MAN_DIR "${HELP2MAN_TAG}")
SET(SCIP_DIR "${SCIP_TAG}")
SET(GLPK_DIR "${GLPK_TAG}")

#TODO(add test commands for external projects)

ExternalProject_Add(
  ${AUTOCONF_DIR} 
  URL http://ftpmirror.gnu.org/autoconf/${AUTOCONF_TAG}.tar.gz
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${AUTOCONF_DIR} && ./configure --prefix=${CMAKE_INSTALL_PREFIX}
  BUILD_COMMAND cd ${DEPENDENCIES_SOURCES}/${AUTOCONF_DIR} && make
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${AUTOCONF_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

ExternalProject_Add(
  ${AUTOMAKE_DIR}
  DEPENDS ${AUTOCONF_DIR}
  URL http://ftpmirror.gnu.org/automake/${AUTOMAKE_TAG}.tar.gz
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${AUTOMAKE_DIR} && ./configure --prefix=${CMAKE_INSTALL_PREFIX}
  BUILD_COMMAND cd ${DEPENDENCIES_SOURCES}/${AUTOMAKE_DIR} && ./bootstrap.sh && make
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${AUTOMAKE_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

ExternalProject_Add(
  ${LIBTOOL_DIR} 
  URL http://ftpmirror.gnu.org/libtool/${LIBTOOL_TAG}.tar.gz
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${LIBTOOL_DIR} && ./configure --prefix=${CMAKE_INSTALL_PREFIX}
  BUILD_COMMAND ""
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${LIBTOOL_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

ExternalProject_Add(
  ${GFLAGS_DIR} 
  GIT_REPOSITORY https://github.com/gflags/gflags
  GIT_TAG ${GFLAGS_TAG}
  CMAKE_ARGS -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=ON -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
  PATCH_COMMAND ""
  UPDATE_COMMAND ""  
)

ExternalProject_Add(
  ${PROTOBUF_DIR} 
  GIT_REPOSITORY https://github.com/google/protobuf
  GIT_TAG ${PROTOBUF_TAG}
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${PROTOBUF_DIR} && ./autogen.sh && ./configure --prefix=${CMAKE_INSTALL_PREFIX} --with-pic
  BUILD_COMMAND cd ${DEPENDENCIES_SOURCES}/${PROTOBUF_DIR} && make && make check
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${PROTOBUF_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

ExternalProject_Add(
  ${SPARSEHASH_DIR} 
  GIT_REPOSITORY https://github.com/sparsehash/sparsehash
  GIT_TAG ${SPARSEHASH_TAG}
  BUILD_COMMAND ""
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${SPARSEHASH_DIR}  && ./autogen.sh && ./configure --prefix=${CMAKE_INSTALL_PREFIX} --with-pic
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${SPARSEHASH_DIR}  && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

#TODO(retest this command)
# execute_process(COMMAND ${Subversion_SVN_EXECUTABLE} list https://projects.coin-or.org)

ExternalProject_Add(
  ${CBC_DIR}
  DEPENDS ${LIBTOOL_DIR}
  SVN_REPOSITORY https://projects.coin-or.org/svn/Cbc/releases/${CBC_TAG}
  BUILD_COMMAND ""
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${CBC_DIR} && ./configure --prefix=${CMAKE_INSTALL_PREFIX} --disable-bzlib --without-lapack --enable-static --enable-shared --with-pic "ADD_CXXFLAGS=-DCBC_THREAD_SAFE -DCBC_NO_INTERRUPT"
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${CBC_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

ExternalProject_Add(
  ${PCRE_DIR} 
  GIT_REPOSITORY https://github.com/Distrotech/pcre
  GIT_TAG ${PCRE_TAG}
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${PCRE_DIR} && ./autogen.sh && ./configure --disable-shared --prefix=${CMAKE_INSTALL_PREFIX}
  BUILD_COMMAND cd ${DEPENDENCIES_SOURCES}/${PCRE_DIR} && make  
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${PCRE_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

ExternalProject_Add(
  ${SWIG_DIR}
  DEPENDS ${PCRE_DIR}
  GIT_REPOSITORY https://github.com/swig/swig
  GIT_TAG ${SWIG_TAG}
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${SWIG_DIR} && ./autogen.sh && ./configure --prefix=${CMAKE_INSTALL_PREFIX} --with-pcre-prefix=${CMAKE_INSTALL_PREFIX} --disable-ccache --without-octave
  BUILD_COMMAND cd ${DEPENDENCIES_SOURCES}/${SWIG_DIR} && make
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${SWIG_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

ExternalProject_Add( ${HELP2MAN_DIR} 
  URL http://ftpmirror.gnu.org/help2man/${HELP2MAN_TAG}.tar.gz
  BUILD_COMMAND ""
  CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${HELP2MAN_DIR} && ./configure --prefix=${CMAKE_INSTALL_PREFIX}
  INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${HELP2MAN_DIR} && make install
  PATCH_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

if(EXISTS "${DEPENDENCIES_ARCHIVES}/scipoptsuite-$(SCIP_TAG).tgz")
  ExternalProject_Add(
    $(SCIP_DIR)
    URL ${DEPENDENCIES_ARCHIVES}/scipoptsuite-$(SCIP_TAG).tgz
    TLS_VERIFY OFF
    BUILD_COMMAND ""
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/$(SCIP_DIR) && make ZIMPL=false READLINE=false GMP=false
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    UPDATE_COMMAND ""
    TEST_COMMAND ""
  )
  SET(SCIP_MAKEFILE "UNIX_SCIP_DIR = $(OR_ROOT_FULL)/dependencies/install/$(SCIP_DIR)/scip-$(SCIP_TAG)")
else()
  SET(SCIP_MAKEFILE "\# Please add scipoptsuite-$(SCIP_TAG).tgz to dependencies/archives to add support for SCIP.")
endif()

if(EXISTS "${DEPENDENCIES_ARCHIVES}/scipoptsuite-$(GLPK_TAG).tgz")
  ExternalProject_Add(
    ${GLPK_DIR} 
    URL ${DEPENDENCIES_ARCHIVES}/scipoptsuite-$(GLPK_TAG).tgz
    BUILD_COMMAND ""
    CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${GLPK_DIR} && ./configure --prefix=${CMAKE_INSTALL_PREFIX} --with-pic
    INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${GLPK_DIR} && make install
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    TEST_COMMAND ""
  )
  SET(GLPK_MAKEFILE "UNIX_SCIP_DIR = $(OR_ROOT_FULL)/dependencies/install/$(GLPK_DIR)/scip-$(SCIP_TAG)")
else()
  SET(GLPK_MAKEFILE "\# Please add scipoptsuite-$(SCIP_TAG).tgz to dependencies/archives to add support for SCIP.")
endif()

#TODO(fix how scip and glpk are included)
if(EXISTS ${MAKEFILE_LOCAL})
else()
#TODO(deal with scip and glpk)
#  file(APPEND ${MAKEFILE_LOCAL} "${SCIP_MAKEFILE}\n")
endif()

