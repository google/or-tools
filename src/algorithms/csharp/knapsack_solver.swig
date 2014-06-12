// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include util/csharp/data.swig

// Include the file we want to wrap a first time.
%{
#include "algorithms/knapsack_solver.h"
%}

%rename (UseReduction) operations_research::KnapsackSolver::use_reduction;
%rename (SetUseReduction) operations_research::KnapsackSolver::set_use_reduction;

%include "algorithms/knapsack_solver.h"
