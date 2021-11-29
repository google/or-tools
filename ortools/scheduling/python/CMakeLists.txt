set_property(SOURCE rcpsp.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE rcpsp.i PROPERTY SWIG_MODULE_NAME pywraprcpsp)
set_property(SOURCE rcpsp.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
swig_add_library(pywraprcpsp
  TYPE SHARED
  LANGUAGE python
  OUTPUT_DIR  ${PROJECT_BINARY_DIR}/python/${PROJECT_NAME}/scheduling
  SOURCES rcpsp.i)

target_include_directories(pywraprcpsp PRIVATE ${Python3_INCLUDE_DIRS})
set_property(TARGET pywraprcpsp PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_compile_definitions(pywraprcpsp PUBLIC "PY3")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywraprcpsp PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../${PROJECT_NAME}/.libs")
  set_property(TARGET pywraprcpsp APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress")
elseif(UNIX)
  set_target_properties(pywraprcpsp PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../${PROJECT_NAME}/.libs")
endif()
target_link_libraries(pywraprcpsp PRIVATE ortools::ortools)

# Variable PYTHON_LIBRARIES can contains keyword `optimized`
# which won't be interpreted inside a generator expression.
# i.e. we can't use: $<$<PLATFORM_ID:Windows>:${PYTHON_LIBRARIES}>
# see: https://cmake.org/cmake/help/git-stage/command/target_link_libraries.html#command:target_link_libraries
if(MSVC)
  target_link_libraries(pywraprcpsp PRIVATE ${Python3_LIBRARIES})
endif()