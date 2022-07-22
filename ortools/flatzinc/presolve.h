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

#ifndef OR_TOOLS_FLATZINC_PRESOLVE_H_
#define OR_TOOLS_FLATZINC_PRESOLVE_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/match.h"
#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/flatzinc/model.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace fz {
// The Presolver "pre-solves" a Model by applying some iterative
// transformations to it, which may simplify and/or shrink the model.
//
// TODO(user): Error reporting of unfeasible models.
class Presolver {
 public:
  explicit Presolver(SolverLogger* logger) : logger_(logger) {}
  // Recursively apply all the pre-solve rules to the model, until exhaustion.
  // The reduced model will:
  // - Have some unused variables.
  // - Have some unused constraints (marked as inactive).
  // - Have some modified constraints (for example, they will no longer
  //   refer to unused variables).
  void Run(Model* model);

 private:
  // This struct stores the affine mapping of one variable:
  // it represents new_var = var * coefficient + offset. It also stores the
  // constraint that defines this mapping.
  struct AffineMapping {
    Variable* variable;
    int64_t coefficient;
    int64_t offset;
    Constraint* constraint;

    AffineMapping()
        : variable(nullptr), coefficient(0), offset(0), constraint(nullptr) {}
    AffineMapping(Variable* v, int64_t c, int64_t o, Constraint* ct)
        : variable(v), coefficient(c), offset(o), constraint(ct) {}
  };

  // This struct stores the mapping of two index variables (of a 2D array; not
  // included here) onto a single index variable (of the flattened 1D array).
  // The original 2D array could be trimmed in the process; so we also need an
  // offset.
  // Eg. new_index_var = index_var1 * int_coeff + index_var2 + int_offset
  struct Array2DIndexMapping {
    Variable* variable1;
    int64_t coefficient;
    Variable* variable2;
    int64_t offset;
    Constraint* constraint;

    Array2DIndexMapping()
        : variable1(nullptr),
          coefficient(0),
          variable2(nullptr),
          offset(0),
          constraint(nullptr) {}
    Array2DIndexMapping(Variable* v1, int64_t c, Variable* v2, int64_t o,
                        Constraint* ct)
        : variable1(v1),
          coefficient(c),
          variable2(v2),
          offset(o),
          constraint(ct) {}
  };

  // Substitution support.
  void SubstituteEverywhere(Model* model);
  void SubstituteAnnotation(Annotation* ann);

  // Presolve rules.
  void PresolveBool2Int(Constraint* ct);
  void PresolveStoreAffineMapping(Constraint* ct);
  void PresolveStoreFlatteningMapping(Constraint* ct);
  void PresolveSimplifyElement(Constraint* ct);
  void PresolveSimplifyExprElement(Constraint* ct);

  // Helpers.
  void UpdateRuleStats(const std::string& rule_name) {
    successful_rules_[rule_name]++;
  }

  // The presolver will discover some equivalence classes of variables [two
  // variable are equivalent when replacing one by the other leads to the same
  // logical model]. We will store them here, using a Union-find data structure.
  // See http://en.wikipedia.org/wiki/Disjoint-set_data_structure.
  // Note that the equivalence is directed. We prefer to replace all instances
  // of 'from' with 'to', rather than the opposite.
  void AddVariableSubstitution(Variable* from, Variable* to);
  Variable* FindRepresentativeOfVar(Variable* var);
  absl::flat_hash_map<const Variable*, Variable*> var_representative_map_;
  std::vector<Variable*> var_representative_vector_;

  // Stores affine_map_[x] = a * y + b.
  absl::flat_hash_map<const Variable*, AffineMapping> affine_map_;

  // Stores array2d_index_map_[z] = a * x + y + b.
  absl::flat_hash_map<const Variable*, Array2DIndexMapping> array2d_index_map_;

  // Count applications of presolve rules. Use a sorted map for reporting
  // purposes.
  std::map<std::string, int> successful_rules_;

  SolverLogger* logger_;
};
}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_PRESOLVE_H_
