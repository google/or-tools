FIND_PACKAGE(ZLIB REQUIRED)

SET(Protobuf_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/protobuf_project/src/protobuf/src)
SET(Protobuf_URL https://github.com/google/protobuf)

ExternalProject_Add(Protobuf_project
        PREFIX Protobuf
        GIT_REPOSITORY ${Protobuf_URL}
        GIT_TAG "v${Protobuf_VERSION}"
        DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
        UPDATE_COMMAND ""
        BUILD_IN_SOURCE 1
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/protobuf_project/src/protobuf
        CONFIGURE_COMMAND ${CMAKE_COMMAND} cmake/
        -Dprotobuf_BUILD_TESTS=OFF
        -DBUILD_STATIC_LIBS=ON
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${Protobuf_ADDITIONAL_CMAKE_OPTIONS}
        INSTALL_COMMAND ""
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON)

ADD_LIBRARY(Protobuf STATIC IMPORTED)
SET_PROPERTY(TARGET Protobuf PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/protobuf_project/src/protobuf/libprotobuf.a)
SET(Protobuf_LIBRARIES "")
LIST(APPEND Protobuf_LIBRARIES Protobuf ${ZLIB_LIBRARIES})

ADD_EXECUTABLE(Protobuf_PROTOC_EXECUTABLE IMPORTED)
SET_PROPERTY(TARGET Protobuf_PROTOC_EXECUTABLE IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/protobuf_project/src/protobuf/protoc)
SET(Protobuf_PROTOC_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/protobuf_project/src/protobuf/protoc)
