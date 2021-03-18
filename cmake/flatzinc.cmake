if(NOT BUILD_FLATZINC)
  return()
endif()

# Flags
if(WIN32)
  list(APPEND FLATZINC_COMPILE_DEFINITIONS "__WIN32__")
endif()
if(MSVC)
  list(APPEND FLATZINC_COMPILE_OPTIONS
    "/bigobj" # Allow big object
    "/DNOMINMAX"
    "/DWIN32_LEAN_AND_MEAN=1"
    "/D_CRT_SECURE_NO_WARNINGS"
    "/D_CRT_SECURE_NO_DEPRECATE"
    "/MP" # Build with multiple processes
    "/DNDEBUG"
    )
  # MSVC warning suppressions
  list(APPEND FLATZINC_COMPILE_OPTIONS
    "/wd4005" # 'macro-redefinition'
    "/wd4018" # 'expression' : signed/unsigned mismatch
    "/wd4065" # switch statement contains 'default' but no 'case' labels
    "/wd4068" # 'unknown pragma'
    "/wd4101" # 'identifier' : unreferenced local variable
    "/wd4146" # unary minus operator applied to unsigned type, result still unsigned
    "/wd4200" # nonstandard extension used : zero-sized array in struct/union
    "/wd4244" # 'conversion' conversion from 'type1' to 'type2', possible loss of data
    "/wd4251" # 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
    "/wd4267" # 'var' : conversion from 'size_t' to 'type', possible loss of data
    "/wd4305" # 'identifier' : truncation from 'type1' to 'type2'
    "/wd4307" # 'operator' : integral constant overflow
    "/wd4309" # 'conversion' : truncation of constant value
    "/wd4334" # 'operator' : result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
    "/wd4355" # 'this' : used in base member initializer list
    "/wd4477" # 'fwprintf' : format string '%s' requires an argument of type 'wchar_t *'
    "/wd4506" # no definition for inline function 'function'
    "/wd4715" # function' : not all control paths return a value
    "/wd4800" # 'type' : forcing value to bool 'true' or 'false' (performance warning)
    "/wd4996" # The compiler encountered a deprecated declaration.
    )
else()
  list(APPEND FLATZINC_COMPILE_OPTIONS "-fwrapv")
endif()

# Main Target
add_library(flatzinc
  ortools/flatzinc/checker.cc
  ortools/flatzinc/checker.h
  ortools/flatzinc/cp_model_fz_solver.cc
  ortools/flatzinc/cp_model_fz_solver.h
  ortools/flatzinc/model.cc
  ortools/flatzinc/model.h
  ortools/flatzinc/parser.cc
  ortools/flatzinc/parser.h
  ortools/flatzinc/parser.tab.cc
  ortools/flatzinc/parser.tab.hh
  ortools/flatzinc/parser.yy.cc
  #ortools/flatzinc/parser_util.cc # Already #include in parser.tab.cc
  ortools/flatzinc/parser_util.h
  ortools/flatzinc/presolve.cc
  ortools/flatzinc/presolve.h
  )
## Includes
target_include_directories(flatzinc PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
  )
## Compile options
set_target_properties(flatzinc PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  )
target_compile_features(flatzinc PUBLIC cxx_std_17)
target_compile_definitions(flatzinc PUBLIC ${FLATZINC_COMPILE_DEFINITIONS})
target_compile_options(flatzinc PUBLIC ${FLATZINC_COMPILE_OPTIONS})
## Properties
if(NOT APPLE)
  set_target_properties(flatzinc PROPERTIES VERSION ${PROJECT_VERSION})
else()
  # Clang don't support version x.y.z with z > 255
  set_target_properties(flatzinc PROPERTIES
    INSTALL_RPATH "@loader_path"
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
endif()
set_target_properties(flatzinc PROPERTIES
  SOVERSION ${PROJECT_VERSION_MAJOR}
  POSITION_INDEPENDENT_CODE ON
  INTERFACE_POSITION_INDEPENDENT_CODE ON
)
set_target_properties(flatzinc PROPERTIES INTERFACE_flatzinc_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(flatzinc PROPERTIES COMPATIBLE_INTERFACE_STRING flatzinc_MAJOR_VERSION)
## Dependencies
target_link_libraries(flatzinc PUBLIC ortools::ortools)
if(WIN32)
  #target_link_libraries(flatzinc PUBLIC psapi.lib ws2_32.lib)
endif()
## Alias
add_library(${PROJECT_NAME}::flatzinc ALIAS flatzinc)


if(APPLE)
  set(CMAKE_INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
endif()
# Binary
add_executable(fz
  ortools/flatzinc/fz.cc
  )
## Includes
target_include_directories(fz PRIVATE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  )
## Compile options
set_target_properties(fz PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  )
target_compile_features(fz PUBLIC cxx_std_17)
target_compile_definitions(fz PUBLIC ${FLATZINC_COMPILE_DEFINITIONS})
target_compile_options(fz PUBLIC ${FLATZINC_COMPILE_OPTIONS})
## Dependencies
target_link_libraries(fz PRIVATE ortools::flatzinc)
## Alias
add_executable(${PROJECT_NAME}::fz ALIAS fz)

# MiniZinc solver configuration
file(RELATIVE_PATH FZ_REL_INSTALL_BINARY
  ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/minizinc/solvers
  ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/fz)
configure_file(
  ortools/flatzinc/ortools.msc.in
  ${PROJECT_BINARY_DIR}/ortools.msc
  @ONLY)

# Install rules
include(GNUInstallDirs)
install(TARGETS flatzinc fz
  EXPORT ${PROJECT_NAME}Targets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

install(DIRECTORY ortools/flatzinc/mznlib/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/minizinc/ortools
  FILES_MATCHING PATTERN "*.mzn")
install(FILES ${PROJECT_BINARY_DIR}/ortools.msc
  DESTINATION ${CMAKE_INSTALL_DATADIR}/minizinc/solvers)
