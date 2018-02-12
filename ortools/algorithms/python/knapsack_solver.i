// Copyright 2010-2017 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Python wrapping of ../knapsack_solver.h. See that file.
//
// USAGE EXAMPLES:
// - examples/python/knapsack.py
// - ./pywrapknapsack_solver_test.py

%include "ortools/base/base.i"

%import "ortools/util/python/vector.i"

%{
#include "ortools/algorithms/knapsack_solver.h"
%}

%ignoreall
%unignore operations_research;
%unignore operations_research::KnapsackSolver;
%unignore operations_research::KnapsackSolver::KnapsackSolver;
%unignore operations_research::KnapsackSolver::~KnapsackSolver;
%unignore operations_research::KnapsackSolver::Init;
%unignore operations_research::KnapsackSolver::Solve;
// TODO(user): unit test BestSolutionContains.
%unignore operations_research::KnapsackSolver::BestSolutionContains;
%unignore operations_research::KnapsackSolver::set_use_reduction;

%unignore operations_research::KnapsackSolver::SolverType;
%unignore operations_research::KnapsackSolver::
          KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER;
%unignore operations_research::KnapsackSolver::KNAPSACK_BRUTE_FORCE_SOLVER;
%unignore operations_research::KnapsackSolver::KNAPSACK_64ITEMS_SOLVER;
%unignore operations_research::KnapsackSolver::
          KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER;
%unignore operations_research::KnapsackSolver::
          KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER;
%unignore operations_research::KnapsackSolver::
          KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER;

%include "ortools/algorithms/knapsack_solver.h"

%unignoreall
