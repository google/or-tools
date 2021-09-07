set_property(SOURCE knapsack_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE knapsack_solver.i PROPERTY SWIG_MODULE_NAME pywrapknapsack_solver)
set_property(SOURCE knapsack_solver.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
swig_add_library(pywrapknapsack_solver
  TYPE SHARED
  LANGUAGE python
  OUTPUT_DIR  ${PROJECT_BINARY_DIR}/python/${PROJECT_NAME}/algorithms
  SOURCES knapsack_solver.i)

target_include_directories(pywrapknapsack_solver PRIVATE ${Python3_INCLUDE_DIRS})
set_property(TARGET pywrapknapsack_solver PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_compile_definitions(pywrapknapsack_solver PUBLIC "PY3")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywrapknapsack_solver PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../${PROJECT_NAME}/.libs")
  set_property(TARGET pywrapknapsack_solver APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress")
elseif(UNIX)
  set_target_properties(pywrapknapsack_solver PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../${PROJECT_NAME}/.libs")
endif()
target_link_libraries(pywrapknapsack_solver PRIVATE ortools::ortools)

# Variable PYTHON_LIBRARIES can contains keyword `optimized`
# which won't be interpreted inside a generator expression.
# i.e. we can't use: $<$<PLATFORM_ID:Windows>:${PYTHON_LIBRARIES}>
# see: https://cmake.org/cmake/help/git-stage/command/target_link_libraries.html#command:target_link_libraries
if(MSVC)
  target_link_libraries(pywrapknapsack_solver PRIVATE ${Python3_LIBRARIES})
endif()
