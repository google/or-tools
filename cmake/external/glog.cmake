SET(glog_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/glog_project/src/glog/)
LIST(APPEND glog_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/glog_project/src/glog/src/)
SET(glog_URL https://github.com/google/glog)

IF(NOT gflags_FOUND)
    ExternalProject_Get_Property(gflags_project SOURCE_DIR)
    SET(glog_EXTRA_ARGS "-DCMAKE_PREFIX_PATH=${SOURCE_DIR}")
ENDIF()

ExternalProject_Add(glog_project
        PREFIX glog
        GIT_REPOSITORY ${glog_URL}
        GIT_TAG "v${glog_VERSION}"
        DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
        UPDATE_COMMAND ""
        PATCH_COMMAND git am -3 ${CMAKE_SOURCE_DIR}/patches/glog_includedir_fix.patch
        BUILD_IN_SOURCE 1
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/glog_project/src/glog
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
        -DWITH_GFLAGS=ON
        -DBUILD_TESTING=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${glog_EXTRA_ARGS}
        INSTALL_COMMAND ""
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON)

IF(NOT gflags_FOUND)
    ADD_DEPENDENCIES(glog_project gflags_project)
ENDIF()

ADD_LIBRARY(glog STATIC IMPORTED) 
SET_PROPERTY(TARGET glog PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glog_project/src/glog/libglog.a)
ADD_DEPENDENCIES(glog glog_project)
SET(glog_LIBRARIES "")
LIST(APPEND glog_LIBRARIES glog)
