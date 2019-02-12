#ifndef OR_TOOLS_BASE_OPTIMIZATION_SUITES_H_
#define OR_TOOLS_BASE_OPTIMIZATION_SUITES_H_

#if defined(USE_GUROBI) || defined(USE_SCIP) || defined(USE_GLPK) || defined(USE_CBC) || defined(USE_CPLEX)
#define MIP_SOLVER_WITH_SOS_CONSTRAINTS   
#endif

#endif
