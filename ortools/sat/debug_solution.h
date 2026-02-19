// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_DEBUG_SOLUTION_H_
#define ORTOOLS_SAT_DEBUG_SOLUTION_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

// A model singleton used for debugging. If this is set in the model, then we
// can check that various derived constraint do not exclude this solution (if
// it is a known optimal solution for instance).
class DebugSolution {
 public:
  explicit DebugSolution(Model* model)
      : shared_response_(model->GetOrCreate<SharedResponseManager>()),
        logger_(model->GetOrCreate<SolverLogger>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        mapping_(model->GetOrCreate<CpModelMapping>()),
        trivial_literals_(model->GetOrCreate<TrivialLiterals>()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        objective_def_(model->GetOrCreate<ObjectiveDefinition>()),
        encoder_(model->GetOrCreate<IntegerEncoder>()),
        name_(model->Name()) {}

  void SynchronizeWithShared(const CpModelProto& model_proto);

  const std::vector<Literal>& boolean_solution() const {
    return boolean_solution_;
  }

  bool IsBooleanSolution() const {
    if (boolean_solution_.empty()) return false;
    if (inner_objective_value_ != kMinIntegerValue) {
      return false;
    }
    return boolean_solution_.size() == proto_values_.size();
  }

  bool CheckClause(absl::Span<const Literal> clause,
                   absl::Span<const IntegerLiteral> integers) const;

  bool CheckCut(const LinearConstraint& cut, bool only_check_ub) const;

  const util_intops::StrongVector<IntegerVariable, IntegerValue>&
  IntegerVariableValues() const {
    return ivar_values_;
  }

 private:
  SharedResponseManager* shared_response_;
  SolverLogger* logger_;
  IntegerTrail* integer_trail_;
  CpModelMapping* mapping_;
  TrivialLiterals* trivial_literals_;
  SatSolver* sat_solver_;
  ObjectiveDefinition* objective_def_;
  IntegerEncoder* encoder_;
  std::string name_;

  bool IsLookingForSolutionBetterThanDebugSolution() const {
    if (inner_objective_value_ == kMinIntegerValue) return false;
    if (shared_response_->BestSolutionInnerObjectiveValue() <=
        inner_objective_value_) {
      return true;
    }
    return false;
  }

  // This is filled from proto_values at load-time, and using the
  // cp_model_mapping, we cache the solution of the integer variables that are
  // mapped. Note that it is possible that not all integer variable are
  // mapped.
  //
  // TODO(user): When this happen we should be able to infer the value of
  // these derived variable in the solution. For now, we only do that for the
  // objective variable.
  util_intops::StrongVector<IntegerVariable, bool> ivar_has_value_;
  util_intops::StrongVector<IntegerVariable, IntegerValue> ivar_values_;

  std::vector<Literal> boolean_solution_;

  // This is the value of all proto variables.
  // It should be of the same size of the PRESOLVED model and should
  // correspond to a solution to the presolved model.
  std::vector<int64_t> proto_values_;

  IntegerValue inner_objective_value_ = kMinIntegerValue;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_DEBUG_SOLUTION_H_
