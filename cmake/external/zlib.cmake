INCLUDE(ExternalProject)

SET(Zlib_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/zlib_archive)
SET(Zlib_URL https://github.com/madler/zlib)
SET(Zlib_BUILD ${CMAKE_CURRENT_BINARY_DIR}/zlib/src/zlib)
SET(Zlib_INSTALL ${CMAKE_CURRENT_BINARY_DIR}/zlib/install)
SET(Zlib_TAG 50893291621658f355bc5b4d450a8d06a563053d)

IF(WIN32)
    SET(Zlib_STATIC_LIBRARIES
            debug ${CMAKE_CURRENT_BINARY_DIR}/zlib/install/lib/zlibstaticd.lib
            optimized ${CMAKE_CURRENT_BINARY_DIR}/zlib/install/lib/zlibstatic.lib)
ELSE()
    SET(Zlib_STATIC_LIBRARIES
            ${CMAKE_CURRENT_BINARY_DIR}/zlib/install/lib/libz.a)
ENDIF()

SET(Zlib_HEADERS
        "${Zlib_INSTALL}/include/zconf.h"
        "${Zlib_INSTALL}/include/zlib.h")

ExternalProject_Add(Zlib
        PREFIX zlib
        GIT_REPOSITORY ${Zlib_URL}
        GIT_TAG ${Zlib_TAG}
        INSTALL_DIR ${Zlib_INSTALL}
        BUILD_IN_SOURCE 1
        DOWNLOAD_DIR ${DOWNLOAD_LOCATION}
        CMAKE_CACHE_ARGS
            -DCMAKE_BUILD_TYPE:STRING=RELEASE
            -DCMAKE_INSTALL_PREFIX:STRING=${Zlib_INSTALL}
            -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON)

ADD_CUSTOM_TARGET(Zlib_CREATE_DESTINATION_DIR
        COMMAND ${CMAKE_COMMAND} -E make_directory ${Zlib_INCLUDE_DIR}
        DEPENDS Zlib)

ADD_CUSTOM_TARGET(Zlib_COPY_HEADERS_TO_DESTINATION DEPENDS Zlib_CREATE_DESTINATION_DIR)

FOREACH(header_file ${ZLIB_HEADERS})
    ADD_CUSTOM_COMMAND(TARGET Zlib_COPY_HEADERS_TO_DESTINATION PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${header_file} ${Zlib_INCLUDE_DIR})
ENDFOREACH()

LIST(APPEND ${PROJECT_NAME}externalTargets Zlib)