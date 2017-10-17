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

#ifndef OR_TOOLS_BOP_BOP_TYPES_H_
#define OR_TOOLS_BOP_BOP_TYPES_H_

#include "ortools/base/basictypes.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"

namespace operations_research {
namespace bop {
DEFINE_INT_TYPE(ConstraintIndex, int);
DEFINE_INT_TYPE(EntryIndex, int);
DEFINE_INT_TYPE(SearchIndex, int);
DEFINE_INT_TYPE(TermIndex, int);
DEFINE_INT_TYPE(VariableIndex, int);
DEFINE_INT_TYPE(SolverTimeStamp, int64);

// Status of the solve of Bop.
enum class BopSolveStatus {
  // The solver found the proven optimal solution.
  OPTIMAL_SOLUTION_FOUND,

  // The solver found a solution, but it is not proven to be the optimal
  // solution.
  FEASIBLE_SOLUTION_FOUND,

  // The solver didn't find any solution.
  NO_SOLUTION_FOUND,

  // The problem is infeasible.
  INFEASIBLE_PROBLEM,

  // The problem is invalid.
  INVALID_PROBLEM
};

inline std::string GetSolveStatusString(BopSolveStatus status) {
  switch (status) {
    case BopSolveStatus::OPTIMAL_SOLUTION_FOUND:
      return "OPTIMAL_SOLUTION_FOUND";
    case BopSolveStatus::FEASIBLE_SOLUTION_FOUND:
      return "FEASIBLE_SOLUTION_FOUND";
    case BopSolveStatus::NO_SOLUTION_FOUND:
      return "NO_SOLUTION_FOUND";
    case BopSolveStatus::INFEASIBLE_PROBLEM:
      return "INFEASIBLE_PROBLEM";
    case BopSolveStatus::INVALID_PROBLEM:
      return "INVALID_PROBLEM";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  return "UNKNOWN Status";
}
inline std::ostream& operator<<(std::ostream& os, BopSolveStatus status) {
  os << GetSolveStatusString(status);
  return os;
}

// TODO(user): Remove.
DEFINE_INT_TYPE(SparseIndex, int);
struct BopConstraintTerm {
  BopConstraintTerm(VariableIndex _var_id, int64 _weight)
      : var_id(_var_id), search_id(0), weight(_weight) {}

  VariableIndex var_id;
  SearchIndex search_id;
  int64 weight;

  bool operator<(const BopConstraintTerm& other) const {
    return search_id < other.search_id;
  }
};
typedef ITIVector<SparseIndex, BopConstraintTerm> BopConstraintTerms;

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_TYPES_H_
