// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include util/java/data.swig

// Include the file we want to wrap a first time.
%{
#include "algorithms/knapsack_solver.h"
%}

%rename (bestSolutionContains) operations_research::KnapsackSolver::BestSolutionContains;
%rename (getName) operations_research::KnapsackSolver::GetName;
%rename (init) operations_research::KnapsackSolver::Init;
%rename (solve) operations_research::KnapsackSolver::Solve;
%rename (useReduction) operations_research::KnapsackSolver::use_reduction;
%rename (setUseReduction) operations_research::KnapsackSolver::set_use_reduction;

%include "algorithms/knapsack_solver.h"
