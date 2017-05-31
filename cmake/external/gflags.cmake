SET(Gflags_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/gflags/src/gflags/include/)
SET(Gflags_URL https://github.com/gflags/gflags)

IF(WIN32)
    SET(Gflags_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/gflags/src/gflags/${CMAKE_BUILD_TYPE}/libgflags.lib)
ELSE()
    SET(Gflags_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/gflags/src/gflags/lib/libgflags.a)
ENDIF()

ExternalProject_Add(Gflags
        PREFIX Gflags
        GIT_REPOSITORY ${Gflags_URL}
        GIT_TAG "v${Gflags_VERSION}"
        DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
        BUILD_IN_SOURCE 1
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/gflags/src/gflags
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
        -DBUILD_TESTING=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${Gflags_ADDITIONAL_CMAKE_OPTIONS}
        INSTALL_COMMAND ""
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON)

LIST(APPEND ${PROJECT_NAME}externalTargets Gflags)