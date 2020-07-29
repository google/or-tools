file(GLOB _SRCS "*.h" "*.cc")

if(USE_SCIP)
  list(APPEND _SRCS ${LPI_GLOP_SRC})
endif()

set(NAME ${PROJECT_NAME}_linear_solver)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
  )
# CMake < 3.12: CMakeError: Object library may not link to anything.
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::memory absl::container absl::strings absl::str_format
  gflags::gflags glog::glog
  protobuf::libprotobuf
  $<$<BOOL:${USE_COINOR}>:Coin::Cbc>
  $<$<BOOL:${USE_XPRESS}>:XPRESS::XPRESS>
  $<$<BOOL:${USE_XPRESS}>:CPLEX::CPLEX>
  libscip
  ${PROJECT_NAME}::proto)

add_library(${PROJECT_NAME}::linear_solver ALIAS ${NAME})

if (USE_XPRESS)
	target_include_directories(${NAME} PUBLIC ${XPRESSDIR}/include)
	if(UNIX)
		target_link_libraries(${NAME} PUBLIC ${XPRESSDIR}/lib/libxprs.so)
	elseif(MSVC)
		target_link_libraries(${NAME} PUBLIC ${XPRESSDIR}/lib/xprs.lib)
	endif()
endif(USE_XPRESS)

if (USE_SIRIUS)
	target_link_libraries(${NAME} PUBLIC sirius_solver)
endif(USE_SIRIUS)

if (USE_CPLEX)
	target_include_directories(${NAME} PUBLIC
		${CPLEXDIR}/include
	)
	if(UNIX)
		target_link_libraries(${NAME} PUBLIC ${CPLEXDIR}/lib/x64_windows_vs2015/stat_mda/cplex1270.a)
	elseif(MSVC)
		target_link_libraries(${NAME} PUBLIC ${CPLEXDIR}/lib/x64_windows_vs2015/stat_mda/cplex1270.lib)
	endif()
endif(USE_CPLEX)
