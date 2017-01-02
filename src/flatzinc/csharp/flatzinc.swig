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
#include "flatzinc/solver.h"
#include "flatzinc/reporting.h"

DECLARE_bool(fz_logging);
%}

%include "std_vector.i"
%template(Int64Vector) std::vector<int64>;
%template(AnnotationVector) std::vector<operations_research::fz::Annotation>;
%template(ArgumentVector) std::vector<operations_research::fz::Argument>;
%template(ConstraintVector) std::vector<operations_research::fz::Constraint*>;
%template(DomainVector) std::vector<operations_research::fz::Domain>;
%template(SolutionOutputSpecsVector) std::vector<operations_research::fz::SolutionOutputSpecs>;
%template(SolutionOutputSpecsBoundsVector) std::vector<operations_research::fz::SolutionOutputSpecs::Bounds>;
%template(IntegerVariableVector) std::vector<operations_research::fz::IntegerVariable*>;

%extend operations_research::fz::Model {
  bool LoadFromFile(const std::string& filename) {
    return operations_research::fz::ParseFlatzincFile(filename, $self);
  }
  void PresolveForCp(bool verbose) {
    FLAGS_fz_logging = verbose;
    operations_research::fz::Presolver presolve;
    presolve.CleanUpModelForTheCpSolver($self, true);
    presolve.Run($self);
  }
  void PrintStatistics() {
    const bool log = FLAGS_fz_logging;
    FLAGS_fz_logging = true;
    operations_research::fz::ModelStatistics stats(*$self);
    stats.BuildStatistics();
    stats.PrintStatistics();
    FLAGS_fz_logging = log;
  }
}

%ignore operations_research::fz::Solver::Solve(
            operations_research::fz::FlatzincParameters,
            operations_research::fz::SearchReportingInterface*);
%ignore operations_research::fz::Solver::ReportInconsistentModel(
            const operations_research::fz::Model&,
            operations_research::fz::FlatzincParameters,
            operations_research::fz::SearchReportingInterface*);
%extend operations_research::fz::Solver {
  void Solve(const FlatzincParameters& parameters) {
    const int num_solutions = parameters.num_solutions == 0 ?
      (parameters.all_solutions ? kint32max : 1) : parameters.num_solutions;
    operations_research::fz::SilentMonoThreadReporting reporting(
        parameters.all_solutions, num_solutions);
    $self->Extract();
    $self->Solve(parameters, &reporting);
  }
}

%rename (ToString) *::DebugString;

%include "flatzinc/model.h"
%include "flatzinc/reporting.h"
%include "flatzinc/solver.h"
