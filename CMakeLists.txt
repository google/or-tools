CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

PROJECT(ortools LANGUAGES CXX)
SET(CMAKE_CXX_STANDARD 11)

INCLUDE(ExternalProject)


IF(UNIX)
    IF(NOT APPLE)
        INCLUDE(GNUInstallDirs)
    ELSE()
        SET(CMAKE_INSTALL_LIBDIR lib)
        SET(CMAKE_INSTALL_BINDIR bin)
        SET(CMAKE_INSTALL_INCLUDEDIR include)
    ENDIF()
ELSE()
    SET(CMAKE_INSTALL_LIBDIR lib)
    SET(CMAKE_INSTALL_BINDIR bin)
    SET(CMAKE_INSTALL_INCLUDEDIR include)
ENDIF()

SET(BUILD_CXX ON)
SET(DOWNLOAD_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/downloads" CACHE PATH "Location where external projects will be downloaded.")

MARK_AS_ADVANCED(BUILD_CXX)
MARK_AS_ADVANCED(DOWNLOAD_LOCATION)

MESSAGE("-- Reading dependency tags from  ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies.txt")
FILE(READ ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies.txt _Dependency_file)
FOREACH(DEPENDENCY Protobuf gflags glog Cbc CoinUtils Osi Clp Cgl)
    STRING(REGEX REPLACE ".*${DEPENDENCY} = ([0-9.]+).*" "\\1" ${DEPENDENCY}_VERSION ${_Dependency_file})
    MESSAGE("  -- ${DEPENDENCY}_VERSION = ${${DEPENDENCY}_VERSION}")
ENDFOREACH()

IF(BUILD_CXX)

    IF(MSVC)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj /DNDEBUG /DUSE_GLOP /DUSE_BOP")
    ELSE()
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fwrapv -DNDEBUG -DUSE_GLOP -DUSE_BOP")
    ENDIF()

    FIND_PACKAGE(Threads REQUIRED)

    FIND_PACKAGE(Protobuf ${Protobuf_VERSION})
    IF(NOT Protobuf_FOUND)
        MESSAGE(STATUS "Did not find system protobuf. Building as an external project")
        INCLUDE(cmake/external/protobuf.cmake)
    ENDIF()
    INCLUDE_DIRECTORIES(${Protobuf_INCLUDE_DIRS})

    FIND_PACKAGE(gflags ${gflags_VERSION})
    IF(NOT gflags_FOUND)
        MESSAGE(STATUS "Did not find system gflags. Building as an external project")
        INCLUDE(cmake/external/gflags.cmake)
    ENDIF()
    INCLUDE_DIRECTORIES(${gflags_INCLUDE_DIRS})

    FIND_PACKAGE(glog ${glog_VERSION})
    IF(NOT glog_FOUND)
        MESSAGE(STATUS "Did not find system glog. Building as an external project.")
        INCLUDE(cmake/external/glog.cmake)
    ENDIF()
    INCLUDE_DIRECTORIES(${glog_INCLUDE_DIRS})

    FIND_PACKAGE(Cbc ${Cbc_VERSION})
    IF(NOT Cbc_FOUND)
        IF(NOT MSVC)
            MESSAGE(STATUS "Did not find system coin-cbc. Building as an external project.")
            INCLUDE(cmake/external/cbc.cmake)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CLP -DUSE_CBC")
            INCLUDE_DIRECTORIES(${Cbc_INCLUDE_DIRS})
        ENDIF()
    ELSE()
        INCLUDE_DIRECTORIES(${Cbc_INCLUDE_DIRS})
        IF(MSVC)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DUSE_CLP /DUSE_CBC")
        ELSE()
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CLP -DUSE_CBC")
        ENDIF()
    ENDIF()

    FILE(GLOB_RECURSE proto_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "ortools/*.proto")
    SET(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)

    INCLUDE_DIRECTORIES(.)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
    INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIRS})

    PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${proto_files})
    ADD_LIBRARY(${PROJECT_NAME}Proto OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
    IF(NOT Protobuf_FOUND)
        ADD_DEPENDENCIES(${PROJECT_NAME}Proto Protobuf_PROTOC_EXECUTABLE)
    ENDIF()

    IF(NOT Cbc_FOUND)
        IF(NOT MSVC)
            ADD_DEPENDENCIES(${PROJECT_NAME}Proto Cbc)
        ENDIF()
    ENDIF()

    IF(NOT glog_FOUND)
        ADD_DEPENDENCIES(${PROJECT_NAME}Proto glog)
    ENDIF()

    SET_TARGET_PROPERTIES(${PROJECT_NAME}Proto PROPERTIES POSITION_INDEPENDENT_CODE ON)

    SET(SUBTARGETS "")

    FOREACH(SUBPROJECT base util lp_data glop graph algorithms sat bop linear_solver constraint_solver)
        ADD_SUBDIRECTORY(ortools/${SUBPROJECT})
        LIST(APPEND SUBTARGETS "$<TARGET_OBJECTS:${PROJECT_NAME}_${SUBPROJECT}>")
    ENDFOREACH()

    LIST(APPEND SUBTARGETS "$<TARGET_OBJECTS:${PROJECT_NAME}Proto>")

    ADD_LIBRARY(${PROJECT_NAME} SHARED ${SUBTARGETS})
    TARGET_LINK_LIBRARIES(${PROJECT_NAME}
            ${Cbc_LIBRARIES}
            ${gflags_LIBRARIES}
            ${Protobuf_LIBRARIES}
            ${glog_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT})

    ADD_DEPENDENCIES(${PROJECT_NAME} ${PROJECT_NAME}Proto)
    INSTALL(TARGETS ${PROJECT_NAME}
            EXPORT ${PROJECT_NAME}_Exports
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    ADD_SUBDIRECTORY(examples/cpp)
ENDIF(BUILD_CXX)

INSTALL(DIRECTORY ortools
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ortools
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.pb.h"
        PATTERN CMakeFiles EXCLUDE)
