// Copyright 2010-2014 Google
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

// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include base/base.swig

/* allow partial c# classes */
%typemap(csclassmodifiers) SWIGTYPE "public partial class"

// Include the file we want to wrap a first time.
%{
#include "algorithms/knapsack_solver.h"
%}

%include "std_vector.i"

%template(KInt64Vector) std::vector<int64>;
%template(KInt64VectorVector) std::vector<std::vector<int64> >;

%rename (UseReduction) operations_research::KnapsackSolver::use_reduction;
%rename (SetUseReduction) operations_research::KnapsackSolver::set_use_reduction;


%include "algorithms/knapsack_solver.h"
