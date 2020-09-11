if(NOT BUILD_SAMPLES)
  return()
endif()

if(BUILD_CXX_SAMPLES)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH
        "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
  endif()

  include(GNUInstallDirs)
  foreach(
    EXECUTABLE IN
    ITEMS minimal_jobshop_cp
          nurses_cp
          rabbits_and_pheasants_cp
          simple_cp_program
          simple_ls_program
          simple_routing_program
          tsp
          tsp_circuit_board
          tsp_cities
          tsp_distance_matrix
          vrp_capacity
          vrp
          vrp_drop_nodes
          vrp_global_span
          vrp_initial_routes
          vrp_pickup_delivery
          vrp_pickup_delivery_fifo
          vrp_pickup_delivery_lifo
          vrp_resources
          vrp_starts_ends
          vrp_time_windows
          vrp_with_time_limit)
    add_executable(${EXECUTABLE} ${EXECUTABLE}.cc)
    target_include_directories(${EXECUTABLE} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${EXECUTABLE} PRIVATE cxx_std_17)
    target_link_libraries(${EXECUTABLE} PRIVATE ortools::ortools)
    install(TARGETS ${EXECUTABLE})
    add_test(NAME cxx_${EXECUTABLE} COMMAND ${EXECUTABLE})
  endforeach()
endif()

if(BUILD_PYTHON_SAMPLES)

endif()

if(BUILD_JAVA_SAMPLES)

endif()

if(BUILD_DOTNET_SAMPLES)

endif()
