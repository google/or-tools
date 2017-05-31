SET(CBC_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/src)
SET(CBC_URL https://github.com/google/protobuf)

IF(WIN32)
    SET(CBC_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/${CMAKE_BUILD_TYPE}/libcbc.lib)
ELSE()
    SET(CBC_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf/libprotobuf.a)
ENDIF()

ExternalProject_Add(Cbc
        PREFIX Cbc
        GIT_REPOSITORY ${CBC_URL}
        GIT_TAG "v${CBC_VERSION}"
        DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
        BUILD_IN_SOURCE 1
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/protobuf/src/protobuf
        CONFIGURE_COMMAND ${CMAKE_COMMAND} cmake/
        -Dprotobuf_BUILD_TESTS=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${Protobuf_ADDITIONAL_CMAKE_OPTIONS}
        INSTALL_COMMAND ""
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON)

LIST(APPEND ${PROJECT_NAME}externalTargets Protobuf)