// Copyright 2010-2012 Google
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

%include util/data.swig

// Include the file we want to wrap a first time.
%{
#include "algorithms/knapsack_solver.h"
%}

#if defined(SWIGJAVA)
%rename (bestSolutionContains) operations_research::KnapsackSolver::BestSolutionContains;
%rename (getName) operations_research::KnapsackSolver::GetName;
%rename (init) operations_research::KnapsackSolver::Init;
%rename (solve) operations_research::KnapsackSolver::Solve;
%rename (useReduction) operations_research::KnapsackSolver::use_reduction;
%rename (setUseReduction) operations_research::KnapsackSolver::set_use_reduction;
#endif  // SWIGJAVA

#if defined(SWIGCSHARP)
%rename (UseReduction) operations_research::KnapsackSolver::use_reduction;
%rename (SetUseReduction) operations_research::KnapsackSolver::set_use_reduction;
#endif  // SWIGCSHARP

%include "algorithms/knapsack_solver.h"
