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

#ifndef OR_TOOLS_FLATZINC_PRESOLVE_H_
#define OR_TOOLS_FLATZINC_PRESOLVE_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/base/hash.h"
#include "ortools/flatzinc/model.h"

namespace operations_research {
namespace fz {
// The Presolver "pre-solves" a Model by applying some iterative
// transformations to it, which may simplify and/or shrink the model.
//
// TODO(user): Error reporting of unfeasible models.
class Presolver {
 public:
  // Recursively apply all the pre-solve rules to the model, until exhaustion.
  // The reduced model will:
  // - Have some unused variables.
  // - Have some unused constraints (marked as inactive).
  // - Have some modified constraints (for example, they will no longer
  //   refer to unused variables).
  //
  // TODO(user): compute on the fly, and add an API to access the set of
  // unused variables.
  //
  // This method returns true iff some transformations were applied to the
  // model.
  // TODO(user): Returns the number of rules applied instead.
  bool Run(Model* model);

  // Cleans the model for the CP solver.
  // In particular, it knows if we use a sat solver inside the CP
  // solver. In that case, for Boolean constraints, it remove the link
  // (defining_constraint, target_variable) for Boolean constraints.
  void CleanUpModelForTheCpSolver(Model* model, bool use_sat);

  // This method is public for tests.
  void PresolveOneConstraint(Constraint* ct);

 private:
  enum RuleStatus {
    NOT_CHANGED = 0,       // Constraint has not changed.
    CONTEXT_CHANGED,       // The constraint has not changed, but some mapping,
                           // or some variables have been updated.
    CONSTRAINT_REWRITTEN,  // The constraint has been rewritten.
    CONSTRAINT_ALWAYS_FALSE,  // The constraint is always false.
    CONSTRAINT_ALWAYS_TRUE,  // The constraint is always true, and now inactive.
  };

  // This struct stores the affine mapping of one variable:
  // it represents new_var = var * coefficient + offset. It also stores the
  // constraint that defines this mapping.
  struct AffineMapping {
    IntegerVariable* variable;
    int64 coefficient;
    int64 offset;
    Constraint* constraint;

    AffineMapping()
        : variable(nullptr), coefficient(0), offset(0), constraint(nullptr) {}
    AffineMapping(IntegerVariable* v, int64 c, int64 o, Constraint* ct)
        : variable(v), coefficient(c), offset(o), constraint(ct) {}
  };

  // This struct stores the mapping of two index variables (of a 2D array; not
  // included here) onto a single index variable (of the flattened 1D array).
  // The original 2D array could be trimmed in the process; so we also need an
  // offset.
  // Eg. new_index_var = index_var1 * int_coeff + index_var2 + int_offset
  struct Array2DIndexMapping {
    IntegerVariable* variable1;
    int64 coefficient;
    IntegerVariable* variable2;
    int64 offset;
    Constraint* constraint;

    Array2DIndexMapping()
        : variable1(nullptr),
          coefficient(0),
          variable2(nullptr),
          offset(0),
          constraint(nullptr) {}
    Array2DIndexMapping(IntegerVariable* v1, int64 c, IntegerVariable* v2,
                        int64 o, Constraint* ct)
        : variable1(v1),
          coefficient(c),
          variable2(v2),
          offset(o),
          constraint(ct) {}
  };

  // First pass of model scanning. Useful to get information that will
  // prevent some destructive modifications of the model.
  void FirstPassModelScan(Model* model);
  // This rule is applied globally in the first pass because maintaining the
  // associated data structures w.r.t. variable substitutions would be
  // expensive.
  void MergeIntEqNe(Model* model);

  // This regroups all int_ne, find cliques, and replace them with
  // all_different_int constraints.
  bool RegroupDifferent(Model* model);

  // Parse constraint x == y - z (and z == y - x) and store the info.
  // It will be useful to transform x == 0 into x == z in the first case.
  void StoreDifference(Constraint* ct);

  // Substitution support.
  void SubstituteEverywhere(Model* model);
  void SubstituteAnnotation(Annotation* ann);

  // Presolve rules. They returns true iff some presolve has been
  // performed. These methods are called by the PresolveOneConstraint() method.
  RuleStatus PresolveBool2Int(Constraint* ct, std::string* log);
  RuleStatus PresolveIntEq(Constraint* ct, std::string* log);
  RuleStatus Unreify(Constraint* ct, std::string* log);
  RuleStatus PresolveInequalities(Constraint* ct, std::string* log);
  RuleStatus PresolveIntNe(Constraint* ct, std::string* log);
  RuleStatus PresolveSetNotIn(Constraint* ct, std::string* log);
  RuleStatus PresolveSetIn(Constraint* ct, std::string* log);
  RuleStatus PresolveSetInReif(Constraint* ct, std::string* log);
  RuleStatus PresolveArrayBoolAnd(Constraint* ct, std::string* log);
  RuleStatus PresolveArrayBoolOr(Constraint* ct, std::string* log);
  RuleStatus PresolveBoolEqNeReif(Constraint* ct, std::string* log);
  RuleStatus PresolveArrayIntElement(Constraint* ct, std::string* log);
  RuleStatus PresolveIntDiv(Constraint* ct, std::string* log);
  RuleStatus PresolveIntTimes(Constraint* ct, std::string* log);
  RuleStatus PresolveIntLinGt(Constraint* ct, std::string* log);
  RuleStatus PresolveIntLinLt(Constraint* ct, std::string* log);
  RuleStatus PresolveLinear(Constraint* ct, std::string* log);
  RuleStatus RegroupLinear(Constraint* ct, std::string* log);
  RuleStatus SimplifyLinear(Constraint* ct, std::string* log);
  RuleStatus PropagatePositiveLinear(Constraint* ct, std::string* log);
  RuleStatus PresolveStoreMapping(Constraint* ct, std::string* log);
  RuleStatus PresolveSimplifyElement(Constraint* ct, std::string* log);
  RuleStatus PresolveSimplifyExprElement(Constraint* ct, std::string* log);
  RuleStatus PropagateReifiedComparisons(Constraint* ct, std::string* log);
  RuleStatus StoreAbs(Constraint* ct, std::string* log);
  RuleStatus RemoveAbsFromIntLeReif(Constraint* ct, std::string* log);
  RuleStatus RemoveAbsFromIntEqNeReif(Constraint* ct, std::string* log);
  RuleStatus SimplifyUnaryLinear(Constraint* ct, std::string* log);
  RuleStatus SimplifyBinaryLinear(Constraint* ct, std::string* log);
  RuleStatus CheckIntLinReifBounds(Constraint* ct, std::string* log);
  RuleStatus CreateLinearTarget(Constraint* ct, std::string* log);
  RuleStatus PresolveBoolNot(Constraint* ct, std::string* log);
  RuleStatus PresolveBoolXor(Constraint* ct, std::string* log);
  RuleStatus SimplifyIntLinEqReif(Constraint* ct, std::string* log);
  RuleStatus PresolveIntMod(Constraint* ct, std::string* log);
  RuleStatus PresolveBoolClause(Constraint* ct, std::string* log);
  RuleStatus StoreIntEqReif(Constraint* ct, std::string* log);
  RuleStatus SimplifyIntNeReif(Constraint* ct, std::string* log);
  RuleStatus PresolveTableInt(Constraint* ct, std::string* log);
  RuleStatus PresolveRegular(Constraint* ct, std::string* log);
  RuleStatus PresolveDiffN(Constraint* ct, std::string* log);

  // Helpers.
  bool IntersectVarWithArg(IntegerVariable* var, const Argument& arg);
  bool IntersectVarWithSingleton(IntegerVariable* var, int64 value);
  bool IntersectVarWithInterval(IntegerVariable* var, int64 imin, int64 imax);
  bool RemoveValue(IntegerVariable* var, int64 value);
  void AddConstraintToMapping(Constraint* ct);
  void RemoveConstraintFromMapping(Constraint* ct);

  // Mark changed variables.
  void MarkChangedVariable(IntegerVariable* var);

  // This method wraps each rule, calls it and log its effect.
  void ApplyRule(
      Constraint* ct, const std::string& rule_name,
      const std::function<RuleStatus(Constraint* ct, std::string*)>& rule);

  // The presolver will discover some equivalence classes of variables [two
  // variable are equivalent when replacing one by the other leads to the same
  // logical model]. We will store them here, using a Union-find data structure.
  // See http://en.wikipedia.org/wiki/Disjoint-set_data_structure.
  // Note that the equivalence is directed. We prefer to replace all instances
  // of 'from' with 'to', rather than the opposite.
  void AddVariableSubstition(IntegerVariable* from, IntegerVariable* to);
  IntegerVariable* FindRepresentativeOfVar(IntegerVariable* var);
  std::unordered_map<const IntegerVariable*, IntegerVariable*>
      var_representative_map_;

  // Stores abs_map_[x] = y if x = abs(y).
  std::unordered_map<const IntegerVariable*, IntegerVariable*> abs_map_;

  // Stores affine_map_[x] = a * y + b.
  std::unordered_map<const IntegerVariable*, AffineMapping> affine_map_;

  // Stores array2d_index_map_[z] = a * x + y + b.
  std::unordered_map<const IntegerVariable*, Array2DIndexMapping>
      array2d_index_map_;

  // Stores x == (y - z).
  std::unordered_map<const IntegerVariable*,
                     std::pair<IntegerVariable*, IntegerVariable*>>
      difference_map_;

  // Stores (x == y) == b
  std::unordered_map<const IntegerVariable*,
                     std::unordered_map<IntegerVariable*, IntegerVariable*>>
      int_eq_reif_map_;

  // Stores all variables defined in the search annotations.
  std::unordered_set<const IntegerVariable*> decision_variables_;

  // For all variables, stores all constraints it appears in.
  std::unordered_map<const IntegerVariable*, std::unordered_set<Constraint*>>
      var_to_constraints_;

  // Count applications of presolve rules. Use a sorted map for reporting
  // purposes.
  std::map<std::string, int> successful_rules_;

  // Store changed objects.
  std::unordered_set<IntegerVariable*> changed_variables_;
  std::unordered_set<Constraint*> changed_constraints_;
};
}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_PRESOLVE_H_
