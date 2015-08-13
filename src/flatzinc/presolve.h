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

#ifndef OR_TOOLS_FLATZINC_PRESOLVE_H_
#define OR_TOOLS_FLATZINC_PRESOLVE_H_

#include <string>
#include "base/hash.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/hash.h"
#include "flatzinc/model.h"

namespace operations_research {
// The FzPresolver "pre-solves" a FzModel by applying some iterative
// transformations to it, which may simplify and/or reduce the model.
class FzPresolver {
 public:
  // Recursively apply all the pre-solve rules to the model, until exhaustion.
  // The reduced model will:
  // - Have some unused variables
  // - Have some unused constraints (marked as inactive).
  // - Have some modified constraints (for example, they will no longer
  //   refer to unused variables)
  // TODO(user): compute on the fly, and add an API to access the set of
  // unused variables.
  //
  // This method returns true iff some transformations were applied to the
  // model.
  // TODO(user): Returns the number of rules applied instead.
  bool Run(FzModel* model);

  // TODO(user): Error reporting of unfeasible models.

  // Cleans the model for the CP solver.
  // In particular, it knows about the sat connection and will remove the link
  // (defining_constraint, target_variable) for boolean constraints.
  void CleanUpModelForTheCpSolver(FzModel* model, bool use_sat);

 private:
  // This struct stores the affine mapping of one variable:
  // it represents new_var = var * coefficient + offset. It also stores the
  // constraint that defines this mapping.
  struct AffineMapping {
    FzIntegerVariable* variable;
    int64 coefficient;
    int64 offset;
    FzConstraint* constraint;

    AffineMapping()
        : variable(nullptr), coefficient(0), offset(0), constraint(nullptr) {}
    AffineMapping(FzIntegerVariable* v, int64 c, int64 o, FzConstraint* ct)
        : variable(v), coefficient(c), offset(o), constraint(ct) {}
  };

  // This struct stores the mapping of two index variables (of a 2D array; not
  // included here) onto a single index variable (of the flattened 1D array).
  // The original 2D array could be trimmed in the process; so we also need an
  // offset.
  // Eg. new_index_var = index_var1 * int_coeff + index_var2 + int_offset
  struct Array2DIndexMapping {
    FzIntegerVariable* variable1;
    int64 coefficient;
    FzIntegerVariable* variable2;
    int64 offset;
    FzConstraint* constraint;

    Array2DIndexMapping()
        : variable1(nullptr),
          coefficient(0),
          variable2(nullptr),
          offset(0),
          constraint(nullptr) {}
    Array2DIndexMapping(FzIntegerVariable* v1, int64 c, FzIntegerVariable* v2,
                        int64 o, FzConstraint* ct)
        : variable1(v1),
          coefficient(c),
          variable2(v2),
          offset(o),
          constraint(ct) {}
  };

  // First pass of model scanning. Useful to get information that will
  // prevent some destructive modifications of the model.
  void FirstPassModelScan(FzModel* model);
  // This rule is applied globally in the first pass because maintaining the
  // associated data structures w.r.t. variable substitutions would be
  // expensive.
  void MergeIntEqNe(FzModel* model);

  // First pass scan helpers.
  void StoreDifference(FzConstraint* ct);

  // Returns true iff the model was modified.
  bool PresolveOneConstraint(FzConstraint* ct);

  // Substitution support.
  void SubstituteEverywhere(FzModel* model);
  void SubstituteAnnotation(FzAnnotation* ann);

  // Presolve rules. They returns true iff that some presolve has been
  // performed. These methods are called by the PresolveOneConstraint() method.
  bool PresolveBool2Int(FzConstraint* ct);
  bool PresolveIntEq(FzConstraint* ct);
  void Unreify(FzConstraint* ct);
  bool PresolveInequalities(FzConstraint* ct);
  bool PresolveIntNe(FzConstraint* ct);
  bool PresolveSetIn(FzConstraint* ct);
  bool PresolveArrayBoolAnd(FzConstraint* ct);
  bool PresolveArrayBoolOr(FzConstraint* ct);
  bool PresolveBoolEqNeReif(FzConstraint* ct);
  bool PresolveArrayIntElement(FzConstraint* ct);
  bool PresolveIntDiv(FzConstraint* ct);
  bool PresolveIntTimes(FzConstraint* ct);
  bool PresolveIntLinGt(FzConstraint* ct);
  bool PresolveIntLinLt(FzConstraint* ct);
  bool PresolveLinear(FzConstraint* ct);
  bool RegroupLinear(FzConstraint* ct);
  bool PropagatePositiveLinear(FzConstraint* ct, bool upper);
  bool PresolveStoreMapping(FzConstraint* ct);
  bool PresolveSimplifyElement(FzConstraint* ct);
  bool PresolveSimplifyExprElement(FzConstraint* ct);
  bool PropagateReifiedComparisons(FzConstraint* ct);
  bool RemoveAbsFromIntLinReif(FzConstraint* ct);
  bool SimplifyUnaryLinear(FzConstraint* ct);
  bool SimplifyBinaryLinear(FzConstraint* ct);
  bool CheckIntLinReifBounds(FzConstraint* ct);
  bool CreateLinearTarget(FzConstraint* ct);
  bool PresolveBoolNot(FzConstraint* ct);
  bool PresolveBoolXor(FzConstraint* ct);
  bool SimplifyIntLinEqReif(FzConstraint* ct);
  bool PresolveIntMod(FzConstraint* ct);
  bool PresolveBoolClause(FzConstraint* ct);
  bool StoreIntEqReif(FzConstraint* ct);
  bool SimplifyIntNeReif(FzConstraint* ct);

  // Helpers.
  void IntersectDomainWith(const FzArgument& arg, FzDomain* domain);

  // The presolver will discover some equivalence classes of variables [two
  // variable are equivalent when replacing one by the other leads to the same
  // logical model]. We will store them here, using a Union-find data structure.
  // See http://en.wikipedia.org/wiki/Disjoint-set_data_structure.
  // Note that the equivalence is directed. We prefer to replace all instances
  // of 'from' with 'to', rather than the opposite.
  void AddVariableSubstition(FzIntegerVariable* from, FzIntegerVariable* to);
  FzIntegerVariable* FindRepresentativeOfVar(FzIntegerVariable* var);
  hash_map<const FzIntegerVariable*, FzIntegerVariable*>
      var_representative_map_;

  // Stores abs_map_[x] = y if x = abs(y).
  hash_map<const FzIntegerVariable*, FzIntegerVariable*> abs_map_;

  // Stores affine_map_[x] = a * y + b.
  hash_map<const FzIntegerVariable*, AffineMapping> affine_map_;

  // Stores array2d_index_map_[z] = a * x + y + b.
  hash_map<const FzIntegerVariable*, Array2DIndexMapping> array2d_index_map_;

  // Stores x == (y - z).
  hash_map<const FzIntegerVariable*,
           std::pair<FzIntegerVariable*, FzIntegerVariable*>> difference_map_;

  // Stores (x == y) == b
  hash_map<FzIntegerVariable*, hash_map<FzIntegerVariable*, FzIntegerVariable*>>
      int_eq_reif_map_;

  // Stores all variables defined in the search annotations.
  hash_set<FzIntegerVariable*> decision_variables_;

  // For all variables, stores all constraints it appears in.
  hash_map<const FzIntegerVariable*, hash_set<FzConstraint*>>
      var_to_constraints_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_PRESOLVE_H_
