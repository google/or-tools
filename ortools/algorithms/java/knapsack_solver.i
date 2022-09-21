// Copyright 2010-2022 Google LLC
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

// Java wrapping of ../knapsack_solver.h. See that file.
//
// USAGE EXAMPLE (which is also used as unit test):
// - java/com/google/ortools/algorithms/samples/Knapsack.java
//
// TODO(user): test all lines marked "untested".

%include "enums.swg"

%include "ortools/base/base.i"
%import "ortools/util/java/vector.i"

%{
#include "ortools/algorithms/knapsack_solver.h"
%}

%ignoreall

%unignore operations_research;
%unignore operations_research::KnapsackSolver;
%unignore operations_research::KnapsackSolver::KnapsackSolver;
%unignore operations_research::KnapsackSolver::~KnapsackSolver;
%rename (init) operations_research::KnapsackSolver::Init;
%rename (solve) operations_research::KnapsackSolver::Solve;
%rename (bestSolutionContains)
    operations_research::KnapsackSolver::BestSolutionContains;  // untested
%rename (isSolutionOptimal) operations_research::KnapsackSolver::IsSolutionOptimal;  // untested
%rename (getName) operations_research::KnapsackSolver::GetName;  // untested
%rename (useReduction) operations_research::KnapsackSolver::use_reduction;  // untested
%rename (setUseReduction) operations_research::KnapsackSolver::set_use_reduction;  // untested
%rename (setTimeLimit) operations_research::KnapsackSolver::set_time_limit;  // untested

%unignore operations_research::KnapsackSolver::SolverType;
%unignore operations_research::KnapsackSolver::KNAPSACK_BRUTE_FORCE_SOLVER;
%unignore operations_research::KnapsackSolver::KNAPSACK_64ITEMS_SOLVER;
%unignore operations_research::KnapsackSolver::KNAPSACK_DIVIDE_AND_CONQUER_SOLVER;
%unignore operations_research::KnapsackSolver::KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER;
%unignore operations_research::KnapsackSolver::KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER;  // untested
%unignore operations_research::KnapsackSolver::KNAPSACK_MULTIDIMENSION_GLPK_MIP_SOLVER;  // untested
%unignore operations_research::KnapsackSolver::KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER;

%include "ortools/algorithms/knapsack_solver.h"

%unignoreall
