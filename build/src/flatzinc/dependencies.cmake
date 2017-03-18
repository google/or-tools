set(FLEX_VERSION "2.6.0" CACHE INTERNAL "" FORCE)
set(BISON_VERSION "3.0.4" CACHE INTERNAL "" FORCE)

# On Linux : sudo apt-get install bison flex

# On Mac : flex comes with Xcode
# Install bison with cmake :
if (APPLE)
    set(BISON_TAG "bison-${BISON_VERSION}")
    set(BISON_DIR "${BISON_TAG}")
    ExternalProject_Add( ${BISON_DIR}
      DEPENDS ${HELP2MAN_DIR}
      URL http://ftpmirror.gnu.org/bison/${BISON_TAG}.tar.gz
      BUILD_COMMAND ""
      CONFIGURE_COMMAND cd ${DEPENDENCIES_SOURCES}/${BISON_DIR} && autoreconf && ./configure --prefix=${DEPENDENCIES_INSTALL}
      INSTALL_COMMAND cd ${DEPENDENCIES_SOURCES}/${BISON_DIR} && make install
      PATCH_COMMAND ""
      UPDATE_COMMAND ""  
      TEST_COMMAND ""
    )
endif()

# On Windows download https://sourceforge.net/projects/winflexbison/files/win_flex_bison-2.5.5.zip and add it to the path
