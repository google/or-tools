# Glog doesn't seem to provide any versioning information in its source tree.
# Thus we do not extract version

INCLUDE(FindPackageHandleStandardArgs)

SET(glog_ROOT_DIR "" CACHE PATH "Path to Google glog")

IF(WIN32)
    FIND_PATH(glog_INCLUDE_DIR glog/logging.h PATHS ${glog_ROOT_DIR}/src/windows)
ELSE()
    FIND_PATH(glog_INCLUDE_DIR glog/logging.h PATHS ${glog_ROOT_DIR})
ENDIF()

IF(MSVC)
    FIND_LIBRARY(glog_LIBRARY_RELEASE libglog_static PATHS ${glog_ROOT_DIR} PATH_SUFFIXES Release)
    FIND_LIBRARY(glog_LIBRARY_DEBUG libglog_static PATHS ${glog_ROOT_DIR} PATH_SUFFIXES Debug)

    SET(glog_LIBRARY OPTIMIZED ${glog_LIBRARY_RELEASE} DEBUG ${glog_LIBRARY_DEBUG})
ELSE()
    FIND_LIBRARY(glog_LIBRARY glog PATHS ${glog_ROOT_DIR} PATH_SUFFIXES lib lib64)
ENDIF()

FIND_PACKAGE_HANDLE_STANDARD_ARGS(glog DEFAULT_MSG glog_INCLUDE_DIR glog_LIBRARY)

IF(glog_FOUND)
    SET(glog_INCLUDE_DIRS ${glog_INCLUDE_DIR})
    SET(glog_LIBRARIES ${glog_LIBRARY})
    MARK_AS_ADVANCED(glog_ROOT_DIR glog_LIBRARY_RELEASE glog_LIBRARY_DEBUG glog_LIBRARY glog_INCLUDE_DIR)
ENDIF()
