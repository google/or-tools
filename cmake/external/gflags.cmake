SET(Gflags_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/gflags_project/src/gflags/include/)
SET(Gflags_URL https://github.com/gflags/gflags)

ExternalProject_Add(Gflags_project
        PREFIX Gflags
        GIT_REPOSITORY ${Gflags_URL}
        GIT_TAG "v${Gflags_VERSION}"
        DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
        UPDATE_COMMAND ""
        BUILD_IN_SOURCE 1
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/gflags_project/src/gflags
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
        -DBUILD_STATIC_LIBS=ON
        -DBUILD_TESTING=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${Gflags_ADDITIONAL_CMAKE_OPTIONS}
        INSTALL_COMMAND ""
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON)

ADD_LIBRARY(Gflags STATIC IMPORTED) 
SET_PROPERTY(TARGET Gflags PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/gflags_project/src/gflags/lib/libgflags.a)
SET(Gflags_LIBRARIES "")
LIST(APPEND Gflags_LIBRARIES Gflags)
