IF(BUILD_PYTHON)
    SET_PROPERTY(SOURCE knapsack_solver.i PROPERTY CPLUSPLUS ON)
    SET_PROPERTY(SOURCE knapsack_solver.i PROPERTY SWIG_MODULE_NAME pywrapknapsack_solver)

    SWIG_ADD_LIBRARY(pywrapknapsack_solver TYPE MODULE LANGUAGE python SOURCES knapsack_solver.i)
    SWIG_LINK_LIBRARIES(pywrapknapsack_solver ${PROJECT_NAME})

    INSTALL(TARGETS _pywrapknapsack_solver DESTINATION ${PYTHON_MODULE_PATH})
ENDIF(BUILD_PYTHON)