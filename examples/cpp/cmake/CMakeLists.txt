include_directories(${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE})

add_executable(golomb ${EXAMPLES_DIR}/cpp/golomb.cc)
target_link_libraries(golomb ortools)