if(NOT BUILD_GLOP)
  return()
endif()


# Generate Protobuf cpp sources
set(PROTO_HDRS)
set(PROTO_SRCS)
set(proto_files
  "ortools/linear_solver/linear_solver.proto"
  "ortools/glop/parameters.proto"
  "ortools/util/optional_boolean.proto"
  )
## Get Protobuf include dir
get_target_property(protobuf_dirs protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir IN LISTS protobuf_dirs)
  if (NOT "${dir}" MATCHES "INSTALL_INTERFACE|-NOTFOUND")
    message(STATUS "Adding proto path: ${dir}")
    list(APPEND PROTO_DIRS "--proto_path=${dir}")
  endif()
endforeach()

foreach(PROTO_FILE IN LISTS proto_files)
  #message(STATUS "protoc proto(cc): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_HDR ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.h)
  set(PROTO_SRC ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.cc)
  #message(STATUS "protoc hdr: ${PROTO_HDR}")
  #message(STATUS "protoc src: ${PROTO_SRC}")
  add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND ${PROTOC_PRG}
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--cpp_out=${PROJECT_BINARY_DIR}"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} ${PROTOC_PRG}
    COMMENT "Generate C++ protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_HDRS ${PROTO_HDR})
  list(APPEND PROTO_SRCS ${PROTO_SRC})
endforeach()

add_library(glop_proto OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
set_target_properties(glop_proto PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)
target_include_directories(glop_proto PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
  $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
  )
target_compile_definitions(glop_proto PUBLIC ${GLOP_COMPILE_DEFINITIONS})
target_compile_options(glop_proto PUBLIC ${GLOP_COMPILE_OPTIONS})
target_link_libraries(glop_proto PRIVATE protobuf::libprotobuf)

# Main Target
add_library(glop)
target_sources(glop PRIVATE
  ortools/base/commandlineflags.h
  ortools/base/file.cc
  ortools/base/file.h
  ortools/base/integral_types.h
  ortools/base/log_severity.h
  ortools/base/logging.cc
  ortools/base/logging.h
  ortools/base/logging_utilities.cc
  ortools/base/logging_utilities.h
  ortools/base/macros.h
  ortools/base/raw_logging.cc
  ortools/base/raw_logging.h
  ortools/base/sysinfo.cc
  ortools/base/sysinfo.h
  ortools/base/vlog_is_on.cc
  ortools/base/vlog_is_on.h
  ortools/glop/basis_representation.cc
  ortools/glop/basis_representation.h
  ortools/glop/dual_edge_norms.cc
  ortools/glop/dual_edge_norms.h
  ortools/glop/entering_variable.cc
  ortools/glop/entering_variable.h
  ortools/glop/initial_basis.cc
  ortools/glop/initial_basis.h
  ortools/glop/lp_solver.cc
  ortools/glop/lp_solver.h
  ortools/glop/lu_factorization.cc
  ortools/glop/lu_factorization.h
  ortools/glop/markowitz.cc
  ortools/glop/markowitz.h
  ortools/glop/preprocessor.cc
  ortools/glop/preprocessor.h
  ortools/glop/primal_edge_norms.cc
  ortools/glop/primal_edge_norms.h
  ortools/glop/reduced_costs.cc
  ortools/glop/reduced_costs.h
  ortools/glop/revised_simplex.cc
  ortools/glop/revised_simplex.h
  ortools/glop/status.cc
  ortools/glop/status.h
  ortools/glop/update_row.cc
  ortools/glop/update_row.h
  ortools/glop/variable_values.cc
  ortools/glop/variable_values.h
  ortools/glop/variables_info.cc
  ortools/glop/variables_info.h
  ortools/lp_data/lp_data.cc
  ortools/lp_data/lp_data.h
  ortools/lp_data/lp_data_utils.cc
  ortools/lp_data/lp_data_utils.h
  ortools/lp_data/lp_print_utils.cc
  ortools/lp_data/lp_print_utils.h
  ortools/lp_data/lp_types.cc
  ortools/lp_data/lp_types.h
  ortools/lp_data/lp_utils.cc
  ortools/lp_data/lp_utils.h
  ortools/lp_data/matrix_scaler.cc
  ortools/lp_data/matrix_scaler.h
  ortools/lp_data/matrix_utils.cc
  ortools/lp_data/matrix_utils.h
  ortools/lp_data/proto_utils.cc
  ortools/lp_data/proto_utils.h
  ortools/lp_data/sparse.cc
  ortools/lp_data/sparse.h
  ortools/lp_data/sparse_column.cc
  ortools/port/sysinfo.h
  ortools/port/sysinfo.cc
  ortools/util/file_util.cc
  ortools/util/file_util.h
  ortools/util/fp_utils.cc
  ortools/util/fp_utils.h
  ortools/util/logging.cc
  ortools/util/logging.h
  ortools/util/rational_approximation.cc
  ortools/util/rational_approximation.h
  ortools/util/stats.cc
  ortools/util/stats.h
  ortools/util/time_limit.cc
  ortools/util/time_limit.h
  )
if(BUILD_LP_PARSER)
  target_sources(glop PRIVATE
    ortools/base/case.cc
    ortools/base/case.h
    ortools/lp_data/lp_parser.cc
    ortools/lp_data/lp_parser.h)
endif()

if(WIN32)
  list(APPEND GLOP_COMPILE_DEFINITIONS "__WIN32__")
endif()
if(MSVC)
  list(APPEND GLOP_COMPILE_OPTIONS
    "/bigobj" # Allow big object
    "/DNOMINMAX"
    "/DWIN32_LEAN_AND_MEAN=1"
    "/D_CRT_SECURE_NO_WARNINGS"
    "/D_CRT_SECURE_NO_DEPRECATE"
    "/MP" # Build with multiple processes
    "/DNDEBUG"
    )
  # MSVC warning suppressions
  list(APPEND GLOP_COMPILE_OPTIONS
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
  list(APPEND GLOP_COMPILE_OPTIONS "-fwrapv")
endif()

# Includes
target_include_directories(glop PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
  )

# Compile options
set_target_properties(glop PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  )
target_compile_features(glop PUBLIC cxx_std_17)
target_compile_definitions(glop PUBLIC ${GLOP_COMPILE_DEFINITIONS})
target_compile_options(glop PUBLIC ${GLOP_COMPILE_OPTIONS})

# Properties
if(NOT APPLE)
  set_target_properties(glop PROPERTIES VERSION ${PROJECT_VERSION})
else()
  # Clang don't support version x.y.z with z > 255
  set_target_properties(glop PROPERTIES
    INSTALL_RPATH "@loader_path"
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
endif()
set_target_properties(glop PROPERTIES
  SOVERSION ${PROJECT_VERSION_MAJOR}
  POSITION_INDEPENDENT_CODE ON
  INTERFACE_POSITION_INDEPENDENT_CODE ON
)
set_target_properties(glop PROPERTIES INTERFACE_glop_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(glop PROPERTIES COMPATIBLE_INTERFACE_STRING glop_MAJOR_VERSION)

# Dependencies
target_sources(glop PRIVATE $<TARGET_OBJECTS:glop_proto>)
add_dependencies(glop glop_proto)

target_link_libraries(glop PUBLIC
  absl::memory
  absl::hash
  absl::flags
  absl::status
  absl::time
  absl::strings
  absl::statusor
  absl::str_format
  protobuf::libprotobuf
  ${RE2_DEPS}
  )
if(WIN32)
  #target_link_libraries(glop PUBLIC psapi.lib ws2_32.lib)
endif()

# ALIAS
add_library(${PROJECT_NAME}::glop ALIAS glop)

# Install rules
include(GNUInstallDirs)
include(GenerateExportHeader)
GENERATE_EXPORT_HEADER(glop)
install(FILES ${PROJECT_BINARY_DIR}/glop_export.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/glop
  COMPONENT Devel)

install(TARGETS glop
  EXPORT glopTargets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

install(EXPORT glopTargets
  NAMESPACE ortools::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glop)

# glop headers
install(DIRECTORY ortools/glop
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools
  COMPONENT Devel
  FILES_MATCHING
  PATTERN "*.h")
# dependencies headers
install(FILES
  ortools/base/accurate_sum.h
  ortools/base/basictypes.h
  ortools/base/commandlineflags.h
  ortools/base/file.h
  ortools/base/hash.h
  ortools/base/int_type.h
  ortools/base/strong_vector.h
  ortools/base/integral_types.h
  ortools/base/log_severity.h
  ortools/base/logging.h
  ortools/base/logging_export.h
  ortools/base/logging_utilities.h
  ortools/base/macros.h
  ortools/base/raw_logging.h
  ortools/base/recordio.h
  ortools/base/sysinfo.h
  ortools/base/timer.h
  ortools/base/vlog_is_on.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/base
  COMPONENT Devel)
install(FILES
  ortools/lp_data/lp_data.h
  ortools/lp_data/permutation.h
  ortools/lp_data/scattered_vector.h
  ortools/lp_data/sparse_column.h
  ortools/lp_data/sparse_row.h
  ortools/lp_data/sparse_vector.h
  ortools/lp_data/lp_types.h
  ortools/lp_data/lp_utils.h
  ortools/lp_data/lp_print_utils.h
  ortools/lp_data/matrix_scaler.h
  ortools/lp_data/matrix_utils.h
  ortools/lp_data/proto_utils.h
  ortools/lp_data/sparse.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/lp_data
  COMPONENT Devel)
install(FILES
  ortools/graph/iterators.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/graph
  COMPONENT Devel)
install(FILES
  ortools/port/sysinfo.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/port
  COMPONENT Devel)
install(FILES
  ortools/util/bitset.h
  ortools/util/file_util.h
  ortools/util/fp_utils.h
  ortools/util/logging.h
  ortools/util/random_engine.h
  ortools/util/rational_approximation.h
  ortools/util/return_macros.h
  ortools/util/running_stat.h
  ortools/util/stats.h
  ortools/util/time_limit.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/util
  COMPONENT Devel)
# proto headers
install(FILES
  ${PROJECT_BINARY_DIR}/ortools/glop/parameters.pb.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/glop
  COMPONENT Devel)
install(FILES
  ${PROJECT_BINARY_DIR}/ortools/linear_solver/linear_solver.pb.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/linear_solver
  COMPONENT Devel)
install(FILES
  ${PROJECT_BINARY_DIR}/ortools/util/optional_boolean.pb.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ortools/util
  COMPONENT Devel)

include(CMakePackageConfigHelpers)
string (TOUPPER "glop" PACKAGE_PREFIX)
configure_package_config_file(cmake/glopConfig.cmake.in
  "${PROJECT_BINARY_DIR}/glopConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/glop"
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/glopConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
  )
install(
  FILES
  "${PROJECT_BINARY_DIR}/glopConfig.cmake"
  "${PROJECT_BINARY_DIR}/glopConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/glop"
  COMPONENT Devel)

# Build glop samples
add_subdirectory(ortools/glop/samples)
