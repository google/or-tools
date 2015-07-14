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

%include "base/base.swig"

// Include the file we want to wrap a first time.
%{
#include "flatzinc/model.h"
#include "flatzinc/parser.h"
#include "flatzinc/presolve.h"
#include "flatzinc/search.h"
#include "flatzinc/solver.h"

DECLARE_bool(fz_logging);
%}

%include "std_vector.i"
%template(FzInt64Vector) std::vector<int64>;
%template(FzAnnotationVector) std::vector<operations_research::FzAnnotation>;
%template(FzArgumentVector) std::vector<operations_research::FzArgument>;
%template(FzConstraintVector) std::vector<operations_research::FzConstraint*>;
%template(FzDomainVector) std::vector<operations_research::FzDomain>;
%template(FzOnSolutionOutputVector) std::vector<operations_research::FzOnSolutionOutput>;
%template(FzIntegerVariableVector) std::vector<operations_research::FzIntegerVariable*>;

%extend operations_research::FzModel {
  bool LoadFromFile(const std::string& filename) {
    return operations_research::ParseFlatzincFile(filename, $self);
  }
  void PresolveForCp() {
    operations_research::FzPresolver presolve;
    presolve.CleanUpModelForTheCpSolver($self, true);
    presolve.Run($self);
  }
  void PrintStatistics() {
    const bool log = FLAGS_fz_logging;
    FLAGS_fz_logging = true;
    operations_research::FzModelStatistics stats(*$self);
    stats.PrintStatistics();
    FLAGS_fz_logging = log;
  }
}

%extend operations_research::FzSolver {
  void SequentialSolve(const operations_research::FzSolverParameters& params) {
    std::unique_ptr<operations_research::FzParallelSupportInterface>
        parallel_support(operations_research::MakeSequentialSupport(
            params.all_solutions, params.num_solutions));
    if ($self->Extract()) {
      $self->Solve(params, parallel_support.get());
    }
  }
}

%rename (ToString) *::DebugString;

%include "flatzinc/model.h"
%include "flatzinc/search.h"
%include "flatzinc/solver.h"
