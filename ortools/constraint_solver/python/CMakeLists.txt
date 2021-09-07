set_property(SOURCE routing.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE routing.i PROPERTY SWIG_MODULE_NAME pywrapcp)
set_property(SOURCE routing.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE routing.i PROPERTY COMPILE_OPTIONS -nofastunpack)
swig_add_library(pywrapcp
  TYPE SHARED
  LANGUAGE python
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/python/${PROJECT_NAME}/constraint_solver
  SOURCES routing.i)

target_include_directories(pywrapcp PRIVATE ${Python3_INCLUDE_DIRS})
set_property(TARGET pywrapcp PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_compile_definitions(pywrapcp PUBLIC "PY3")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywrapcp PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../${PROJECT_NAME}/.libs")
  set_property(TARGET pywrapcp APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress")
elseif(UNIX)
  set_target_properties(pywrapcp PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../${PROJECT_NAME}/.libs")
endif()
target_link_libraries(pywrapcp PRIVATE ortools::ortools)

# Variable PYTHON_LIBRARIES can contains keyword `optimized`
# which won't be interpreted inside a generator expression.
# i.e. we can't use: $<$<PLATFORM_ID:Windows>:${PYTHON_LIBRARIES}>
# see: https://cmake.org/cmake/help/git-stage/command/target_link_libraries.html#command:target_link_libraries
if(MSVC)
  target_link_libraries(pywrapcp PRIVATE ${Python3_LIBRARIES})
endif()
