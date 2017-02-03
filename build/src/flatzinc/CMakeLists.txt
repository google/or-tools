include(dependencies.cmake)

find_package(FLEX ${FLEX_VERSION})
if(NOT FLEX_FOUND)
  message(FATAL_ERROR "flex could not be found. The minimum required version is ${FLEX_VERSION}.")
endif()

find_package(BISON ${BISON_VERSION})
if(NOT BISON_FOUND)
  message(FATAL_ERROR "bison could not be found. The minimum required version is ${BISON_VERSION}.")
endif()

#TODO(include this once)
include_directories(${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE} ${EX_DIR})

set(FZ_SRC_DIR ${SRC_DIR}/flatzinc)
set(FZ_GEN_DIR ${GEN_DIR}/flatzinc)
file(MAKE_DIRECTORY ${FZ_GEN_DIR})

add_custom_command(
    OUTPUT  ${FZ_GEN_DIR}/parser.yy.cc
    COMMAND ${FLEX_EXECUTABLE} -o${FZ_GEN_DIR}/parser.yy.cc ${FZ_SRC_DIR}/parser.lex
    DEPENDS ${FZ_SRC_DIR}/parser.lex
    COMMENT "Flex : Generating ${REL_GEN_DIR}/flatzinc/parser.yy.cc"
)

add_custom_command(
    OUTPUT  ${FZ_GEN_DIR}/parser.tab.cc
    COMMAND ${BISON_EXECUTABLE} -t -o  ${FZ_GEN_DIR}/parser.tab.cc -d ${FZ_SRC_DIR}/parser.yy
    DEPENDS ${FZ_GEN_DIR}/parser.yy.cc
    COMMENT "Bison : Generating ${FZ_GEN_DIR}/parser.tab.cc"
)

file(GLOB fz_src "${FZ_SRC_DIR}/*.cc")
list(REMOVE_ITEM fz_src ${FZ_SRC_DIR}/fz.cc ${FZ_SRC_DIR}/parser_main.cc ${FZ_SRC_DIR}/parser_util.cc)
list(APPEND fz_src ${FZ_GEN_DIR}/parser.tab.cc ${FZ_GEN_DIR}/parser.yy.cc)

add_library(fz_lib SHARED ${fz_src})
target_link_libraries(fz_lib ortools)

add_executable(fz ${FZ_SRC_DIR}/fz.cc)
target_link_libraries(fz fz_lib)
add_executable(parser_main ${FZ_SRC_DIR}/parser_main.cc)
target_link_libraries(parser_main fz_lib)
