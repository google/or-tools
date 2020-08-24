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
  foreach(
    EXECUTABLE IN
    ITEMS assignment_sat
          binpacking_problem_sat
          bool_or_sample_sat
          channeling_sample_sat
          cp_is_fun_sat
          earliness_tardiness_cost_sample_sat
          interval_sample_sat
          literal_sample_sat
          multiple_knapsack_sat
          no_overlap_sample_sat
          optional_interval_sample_sat
          rabbits_and_pheasants_sat
          ranking_sample_sat
          reified_sample_sat
          search_for_all_solutions_sample_sat
          simple_sat_program
          solution_hinting_sample_sat
          solve_and_print_intermediate_solutions_sample_sat
          solve_with_time_limit_sample_sat
          step_function_sample_sat
          stop_after_n_solutions_sample_sat)
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
