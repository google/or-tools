if(NOT BUILD_SAMPLES)
	return()
endif()

if(BUILD_CXX)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
  endif()

  include(GNUInstallDirs)
  foreach(EXECUTABLE IN ITEMS
      assignment_mip
      bin_packing_mip
      integer_programming_example
      linear_programming_example
      mip_var_array
      multiple_knapsack_mip
      simple_lp_program
      simple_mip_program)
    add_executable(${EXECUTABLE} ${EXECUTABLE}.cc)
    target_include_directories(${EXECUTABLE} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${EXECUTABLE} PRIVATE cxx_std_17)
    target_link_libraries(${EXECUTABLE} PRIVATE ortools::ortools)
    install(TARGETS ${EXECUTABLE})
  endforeach()
endif()

if(BUILD_PYTHON)
endif()

if(BUILD_JAVA)
endif()

if(BUILD_DOTNET)
endif()
