if(NOT BUILD_PYTHON_EXAMPLES)
  return()
endif()

file(GLOB PYTHON_SRCS "*.py")
# Remove too long examples
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/arc_flow_cutting_stock_sat.py") # too long
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/bus_driver_scheduling_sat.py") # too long
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/cover_rectangle_sat.py") # too long
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/cvrptw_plot.py") # depend on numpy
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/qubo_sat.py") # too long

foreach(FILE_NAME IN LISTS PYTHON_SRCS)
  add_python_example(${FILE_NAME})
endforeach()
