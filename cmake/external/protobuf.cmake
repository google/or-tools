SET(Protobuf_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/src)
SET(Protobuf_URL https://github.com/google/protobuf)

IF(WIN32)
    SET(Protobuf_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/${CMAKE_BUILD_TYPE}/libprotobuf.lib)
    SET(Protobuf_PROTOC_EXECUTABLE
            ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/${CMAKE_BUILD_TYPE}/protoc.exe)
    SET(Protobuf_ADDITIONAL_CMAKE_OPTIONS -Dprotobuf_MSVC_STATIC_RUNTIME:BOOL=OFF -A x64)
ELSE()
    SET(Protobuf_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/libprotobuf.a)
    SET(Protobuf_PROTOC_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/protoc)
ENDIF()

ExternalProject_Add(Protobuf
        PREFIX Protobuf
        DEPENDS Zlib
        GIT_REPOSITORY ${Protobuf_URL}
        GIT_TAG "v${Protobuf_VERSION}"
        DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
        BUILD_IN_SOURCE 1
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf
        CONFIGURE_COMMAND ${CMAKE_COMMAND} cmake/
        -Dprotobuf_BUILD_TESTS=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DZLIB_ROOT=${ZLIB_INSTALL}
        ${Protobuf_ADDITIONAL_CMAKE_OPTIONS}
        INSTALL_COMMAND ""
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
        -DZLIB_ROOT:STRING=${ZLIB_INSTALL})

LIST(APPEND ${PROJECT_NAME}externalTargets Protobuf)