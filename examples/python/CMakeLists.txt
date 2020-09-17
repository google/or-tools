if(NOT BUILD_PYTHON_EXAMPLES)
	return()
endif()

file(GLOB PYTHON_SRCS "*.py")
# Remove too long examples, or not working
# ISSUE: Seems to works using Makefile based build
# ???: not tested even in Makefile based build (i.e. unmaintained)
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/appointments.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/arc_flow_cutting_stock_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/assignment_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/assignment_with_constraints_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/balance_group_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/bus_driver_scheduling_flow_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/bus_driver_scheduling_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/chemical_balance_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/clustering_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/cover_rectangle_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/cvrptw_plot.py") # ??? (depend on numpy)
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/jobshop_with_maintenance_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/nqueens_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/prize_collecting_tsp_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/flexible_job_shop_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/gate_scheduling_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/qubo_sat.py") # too long
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/hidato_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/jobshop_ft06_distance_sat.py") # ISSSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/jobshop_ft06_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/random_tsp.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/rcpsp_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/reallocate_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/shift_scheduling_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/single_machine_scheduling_with_setup_release_due_dates_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/steel_mill_slab_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/sudoku_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/task_allocation_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/tasks_and_workers_assignment_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/tsp_sat.py") # ???
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/vendor_scheduling_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/wedding_optimal_chart_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/worker_schedule_sat.py") # ISSUE
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/zebra_sat.py") # ISSUE

foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    get_filename_component(NAME ${FILE_NAME} NAME_WE)
    add_test(python_${NAME} ${VENV_Python_EXECUTABLE} ${FILE_NAME})
endforeach()

install(DIRECTORY .
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ortools/examples/python
  FILES_MATCHING PATTERN "*.py")
