ADD_LIBRARY(${PROJECT_NAME}examples SHARED
        cvrptw_lib.cc
        parse_dimacs_assignment.cc
        fap_model_printer.cc
        fap_parser.cc
        fap_utilities.cc)
TARGET_LINK_LIBRARIES(${PROJECT_NAME}examples ${PROJECT_NAME})

INSTALL(TARGETS ${PROJECT_NAME}examples
            EXPORT ${PROJECT_NAME}_Exports
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

ADD_DEPENDENCIES(${PROJECT_NAME}examples ${PROJECT_NAME}Proto)

FOREACH(EXECUTABLE
        costas_array
        cryptarithm
        cvrp_disjoint_tw
        cvrptw
        cvrptw_with_breaks
        cvrptw_with_refueling
        cvrptw_with_resources
        cvrptw_with_stop_times_and_resources
        dimacs_assignment
        dobble_ls
        flow_api
        frequency_assignment_problem
        golomb
        integer_programming
        jobshop
        jobshop_earlytardy
        jobshop_ls
        jobshop_sat
        linear_assignment_api
        linear_programming
        linear_solver_protocol_buffers
        ls_api
        magic_square
        model_util
        mps_driver
        multidim_knapsack
        network_routing
        nqueens
        pdptw
        rcpsp_sat
        shift_minimization_sat
        solve
        sports_scheduling
        strawberry_fields_with_column_generation
        tsp
        weighted_tardiness_sat)
    ADD_EXECUTABLE(${EXECUTABLE} ${EXECUTABLE}.cc)
    TARGET_LINK_LIBRARIES(${EXECUTABLE} ${PROJECT_NAME}examples ${PROJECT_NAME})
    INSTALL(TARGETS ${EXECUTABLE} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDFOREACH()
