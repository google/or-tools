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

#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {

// static
const double ScatteredColumnReference::kDenseThresholdForPreciseSum = 0.8;

std::string GetProblemStatusString(ProblemStatus problem_status) {
  switch (problem_status) {
    case ProblemStatus::OPTIMAL:
      return "OPTIMAL";
    case ProblemStatus::PRIMAL_INFEASIBLE:
      return "PRIMAL_INFEASIBLE";
    case ProblemStatus::DUAL_INFEASIBLE:
      return "DUAL_INFEASIBLE";
    case ProblemStatus::INFEASIBLE_OR_UNBOUNDED:
      return "INFEASIBLE_OR_UNBOUNDED";
    case ProblemStatus::PRIMAL_UNBOUNDED:
      return "PRIMAL_UNBOUNDED";
    case ProblemStatus::DUAL_UNBOUNDED:
      return "DUAL_UNBOUNDED";
    case ProblemStatus::INIT:
      return "INIT";
    case ProblemStatus::PRIMAL_FEASIBLE:
      return "PRIMAL_FEASIBLE";
    case ProblemStatus::DUAL_FEASIBLE:
      return "DUAL_FEASIBLE";
    case ProblemStatus::ABNORMAL:
      return "ABNORMAL";
    case ProblemStatus::INVALID_PROBLEM:
      return "INVALID_PROBLEM";
    case ProblemStatus::IMPRECISE:
      return "IMPRECISE";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid ProblemStatus " << static_cast<int>(problem_status);
  return "UNKNOWN ProblemStatus";
}

std::string GetVariableTypeString(VariableType variable_type) {
  switch (variable_type) {
    case VariableType::UNCONSTRAINED:
      return "UNCONSTRAINED";
    case VariableType::LOWER_BOUNDED:
      return "LOWER_BOUNDED";
    case VariableType::UPPER_BOUNDED:
      return "UPPER_BOUNDED";
    case VariableType::UPPER_AND_LOWER_BOUNDED:
      return "UPPER_AND_LOWER_BOUNDED";
    case VariableType::FIXED_VARIABLE:
      return "FIXED_VARIABLE";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid VariableType " << static_cast<int>(variable_type);
  return "UNKNOWN VariableType";
}

std::string GetVariableStatusString(VariableStatus status) {
  switch (status) {
    case VariableStatus::FREE:
      return "FREE";
    case VariableStatus::AT_LOWER_BOUND:
      return "AT_LOWER_BOUND";
    case VariableStatus::AT_UPPER_BOUND:
      return "AT_UPPER_BOUND";
    case VariableStatus::FIXED_VALUE:
      return "FIXED_VALUE";
    case VariableStatus::BASIC:
      return "BASIC";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid VariableStatus " << static_cast<int>(status);
  return "UNKNOWN VariableStatus";
}

std::string GetConstraintStatusString(ConstraintStatus status) {
  switch (status) {
    case ConstraintStatus::FREE:
      return "FREE";
    case ConstraintStatus::AT_LOWER_BOUND:
      return "AT_LOWER_BOUND";
    case ConstraintStatus::AT_UPPER_BOUND:
      return "AT_UPPER_BOUND";
    case ConstraintStatus::FIXED_VALUE:
      return "FIXED_VALUE";
    case ConstraintStatus::BASIC:
      return "BASIC";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid ConstraintStatus " << static_cast<int>(status);
  return "UNKNOWN ConstraintStatus";
}

ConstraintStatus VariableToConstraintStatus(VariableStatus status) {
  switch (status) {
    case VariableStatus::FREE:
      return ConstraintStatus::FREE;
    case VariableStatus::AT_LOWER_BOUND:
      return ConstraintStatus::AT_LOWER_BOUND;
    case VariableStatus::AT_UPPER_BOUND:
      return ConstraintStatus::AT_UPPER_BOUND;
    case VariableStatus::FIXED_VALUE:
      return ConstraintStatus::FIXED_VALUE;
    case VariableStatus::BASIC:
      return ConstraintStatus::BASIC;
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid VariableStatus " << static_cast<int>(status);
  // This will never be reached and is here only to guarantee compilation.
  return ConstraintStatus::FREE;
}



}  // namespace glop
}  // namespace operations_research
