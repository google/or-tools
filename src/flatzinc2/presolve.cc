// Copyright 2010-2013 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in comÏpliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS% IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "flatzinc2/presolve.h"

#include <algorithm>
#include <hash_map>
#include <hash_set>

#include "base/map_util.h"
#include "base/strutil.h"

DECLARE_bool(fz_logging);
DECLARE_bool(fz_verbose);

namespace operations_research {

namespace {
bool IsBooleanVar(FzIntegerVariable* var) {
  return var->Min() == 0 && var->Max() == 1;
}

bool IsBooleanValue(int64 value) { return value == 0 || value == 1; }
}  // namespace

// For the author's reference, here is an indicative list of presolve rules
// that
// should eventually be implemented.
//
// Presolve rule:
//   - table_int -> intersect variables domains with tuple set.
//
// TODO(user):
//   - store dependency graph of constraints -> variable to speed up presolve.
//   - use the same dependency graph to speed up variable substitution.
//   - add more check when presolving out a variable or a constraint.

// ----- Presolve rules -----

// Note on documentation
//
// In order to document presolve rules, we will use the following naming
// convention:
//   - x, x1, xi, y, y1, yi denote integer variables
//   - b, b1, bi denote boolean variables
//   - c, c1, ci denote integer constants
//   - t, t1, ti denote boolean constants
//   - => x after a constraint denotes the target variable of this constraint.
// Arguments are listed in order.

// Propagates cast constraint.
// Input : bool2int(b, x) => x
// Action: replace all instances of x by b in the model.
// Output: inactive constraint.
bool FzPresolver::PresolveBool2Int(FzConstraint* ct) {
  if (ct->Arg(0).HasOneValue() || ct->Arg(1).HasOneValue()) {
    FZVLOG << "Change bound bool2int into int_eq";
    ct->type = "int_eq";
    return true;
  } else {
    ct->MarkAsInactive();
    MarkVariablesAsEquivalent(ct->Arg(0).Var(), ct->Arg(1).Var());
    return true;
  }
}

// Presolve equality constraint: int_eq
//
// Rule 1:
// Input : int_eq(x, 0) && x == y - z (stored in difference_map_).
// Output: int_eq(y, z)
//
// Rule 2:
// Input : int_eq(x, c)
// Action: Reduce domain of x to {c}
// Output: inactive constraint.
//
// Rule 3:
// Input : int_eq(x1, x2)
// Action: Pick x1 or x2, and replace all occurrences by the other.
// Output: inactive constraint.
//
// Rule 4:
// Input : int_eq(c, x)
// Action: Reduce domain of x to {c}
// Output: inactive constraint.
//
// Rule 5:
// Input : int_eq(c1, c2)
// Output: inactive constraint if c1 == c2
bool FzPresolver::PresolveIntEq(FzConstraint* ct) {
  // Rule 1
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF &&
      ct->Arg(1).type == FzArgument::INT_VALUE && ct->Arg(1).Value() == 0 &&
      ContainsKey(difference_map_, ct->Arg(0).Var())) {
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    ct->Arg(0).Var()->domain.IntersectWithInterval(0, 0);

    FZVLOG << "  - transform null differences in " << ct->DebugString()
           << FZENDL;
    const std::pair<FzIntegerVariable*, FzIntegerVariable*>& diff =
        FindOrDie(difference_map_, ct->Arg(0).Var());
    ct->MutableArg(0)->variables[0] = diff.first;
    ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
    ct->MutableArg(1)->values.clear();
    ct->MutableArg(1)->variables.push_back(diff.second);
    return true;
  }
  if (ct->Arg(0).IsVariable()) {
    if (ct->Arg(1).HasOneValue()) {
      // Rule 2.
      const int64 value = ct->Arg(1).Value();
      FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
      ct->Arg(0).Var()->domain.IntersectWithInterval(value, value);
      ct->MarkAsInactive();
      return true;
    } else if (ct->Arg(1).IsVariable()) {
      // Rule 3.
      MarkVariablesAsEquivalent(ct->Arg(0).Var(), ct->Arg(1).Var());
      ct->MarkAsInactive();
      return true;
    }
  } else if (ct->Arg(0).HasOneValue()) {  // Arg0 is an integer value.
    const int64 value = ct->Arg(0).Value();
    if (ct->Arg(1).IsVariable()) {
      // Rule 4.
      FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
      ct->Arg(1).Var()->domain.IntersectWithInterval(value, value);
      ct->MarkAsInactive();
      return true;
    } else if (ct->Arg(1).HasOneValue() && value == ct->Arg(1).Value()) {
      // Rule 5.
      // No-op, removing.
      ct->MarkAsInactive();
      return false;
    }
  }
  return false;
}

// Propagates inequality constraint.
// Input : int_ne(x, c) or int_ne(c, x)
// Action: remove c from the domain of x.
// Output: inactive constraint if the removal was successful.
bool FzPresolver::PresolveIntNe(FzConstraint* ct) {
  if (ct->presolve_propagation_done) {
    return false;
  }
  if ((ct->Arg(0).IsVariable() && ct->Arg(1).HasOneValue() &&
       ct->Arg(0).Var()->domain.RemoveValue(ct->Arg(1).Value())) ||
      (ct->Arg(1).IsVariable() && ct->Arg(0).HasOneValue() &&
       ct->Arg(1).Var()->domain.RemoveValue(ct->Arg(0).Value()))) {
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    ct->MarkAsInactive();
    ct->presolve_propagation_done = true;
    return true;
  }
  return false;
}

// Bound propagation on comparisons: int_le, bool_le, int_lt, bool_lt,
//                                   int_ge, bool_ge, int_gt, bool_gt.
// Rule 1:
// Input : int_xx(x, c) or int_xx(c, x) or bool_xx(x, c) or bool_xx(c, x)
//          with xx == lt, le, gt, ge
// Action: Reduce domain of x.
// Output: constraint is inactive.
//
// Rule 2:
// Input : int_xx(x, y) or bool_xx(x, y) with xx == lt, le, gt, ge.
// Action: Reduce domain of x and y.
bool FzPresolver::PresolveInequalities(FzConstraint* ct) {
  const std::string& id = ct->type;
  if (ct->Arg(0).IsVariable() && ct->Arg(1).HasOneValue()) {
    // Rule 1.
    FzIntegerVariable* const var = ct->Arg(0).Var();
    const int64 value = ct->Arg(1).Value();
    if (id == "int_le" || id == "bool_le") {
      var->domain.IntersectWithInterval(kint64min, value);
    } else if (id == "int_lt" || id == "bool_lt") {
      var->domain.IntersectWithInterval(kint64min, value - 1);
    } else if (id == "int_ge" || id == "bool_ge") {
      var->domain.IntersectWithInterval(value, kint64max);
    } else if (id == "int_gt" || id == "bool_gt") {
      var->domain.IntersectWithInterval(value + 1, kint64max);
    }
    ct->MarkAsInactive();
    return true;
  } else if (ct->Arg(0).HasOneValue() && ct->Arg(1).IsVariable()) {
    // Rule 1.
    FzIntegerVariable* const var = ct->Arg(1).Var();
    const int64 value = ct->Arg(0).Value();
    if (id == "int_le" || id == "bool_le") {
      var->domain.IntersectWithInterval(value, kint64max);
    } else if (id == "int_lt" || id == "bool_lt") {
      var->domain.IntersectWithInterval(value + 1, kint64max);
    } else if (id == "int_ge" || id == "bool_ge") {
      var->domain.IntersectWithInterval(kint64min, value);
    } else if (id == "int_gt" || id == "bool_gt") {
      var->domain.IntersectWithInterval(kint64min, value - 1);
    }
    ct->MarkAsInactive();
    return true;
  }
  // Rule 2.
  FzIntegerVariable* const left = ct->Arg(0).Var();
  const int64 left_min = left->Min();
  const int64 left_max = left->Max();
  FzIntegerVariable* const right = ct->Arg(1).Var();
  const int64 right_min = right->Min();
  const int64 right_max = right->Max();
  bool modified = false;
  if (id == "int_le" || id == "bool_le") {
    left->domain.IntersectWithInterval(kint64min, right_max);
    right->domain.IntersectWithInterval(left_min, kint64max);
    modified = left_max > right_max || right_min < left_min;
  } else if (id == "int_lt" || id == "bool_lt") {
    left->domain.IntersectWithInterval(kint64min, right_max - 1);
    right->domain.IntersectWithInterval(left_min + 1, kint64max);
    modified = left_max >= right_max || right_min <= left_min;
  } else if (id == "int_ge" || id == "bool_ge") {
    left->domain.IntersectWithInterval(right_min, kint64max);
    right->domain.IntersectWithInterval(kint64min, left_max);
    modified = right_max > left_max || left_min < right_min;
  } else if (id == "int_gt" || id == "bool_gt") {
    left->domain.IntersectWithInterval(right_min + 1, kint64max);
    right->domain.IntersectWithInterval(kint64min, left_max - 1);
    modified = right_max >= left_max || left_min <= right_min;
  }
  return modified;
}

// A reified constraint is a constraint that has been casted into a boolean
// variable that represents its status.
// Thus x == 3 can be reified into b == (x == 3).
//
// Rule 1:
// Input : int_xx_reif(arg1, arg2, true) or
//         int_lin_xx_reif(arg1, arg2, c, true)
//         with xx = eq, ne, le, lt, ge, gt
// Output: int_xx(arg1, arg2) or int_lin_xx(arg1, arg2, c)
//
// Rule 2:
// Input : int_xx_reif(arg1, arg2, falsee) or
//         int_lin_xx_reif(arg1, arg2, c, false)
//         with xx = eq, ne, le, lt, ge, gt
// Output: int_yy(arg1, arg2) or int_lin_yy(arg1, arg2, c)
//         with yy = opposite(xx). i.e. eq -> ne, le -> gt...
void FzPresolver::Unreify(FzConstraint* ct) {
  const std::string& id = ct->type;
  const int last_argument = ct->arguments.size() - 1;
  ct->type.resize(id.size() - 5);
  ct->RemoveTargetVariable();
  if (ct->Arg(last_argument).Value() == 1) {
    // Rule 1.
    FZVLOG << "Unreify " << ct->DebugString() << FZENDL;
    ct->RemoveTargetVariable();
    ct->arguments.pop_back();
  } else {
    // Rule 2.
    FZVLOG << "Unreify and inverse " << ct->DebugString() << FZENDL;
    ct->RemoveTargetVariable();
    ct->arguments.pop_back();
    if (id == "int_eq")
      ct->type = "int_ne";
    else if (id == "int_ne")
      ct->type = "int_eq";
    else if (id == "int_ge")
      ct->type = "int_lt";
    else if (id == "int_gt")
      ct->type = "int_le";
    else if (id == "int_le")
      ct->type = "int_gt";
    else if (id == "int_lt")
      ct->type = "int_ge";
    else if (id == "int_lin_eq")
      ct->type = "int_lin_ne";
    else if (id == "int_lin_ne")
      ct->type = "int_lin_eq";
    else if (id == "int_lin_ge")
      ct->type = "int_lin_lt";
    else if (id == "int_lin_gt")
      ct->type = "int_lin_le";
    else if (id == "int_lin_le")
      ct->type = "int_lin_gt";
    else if (id == "int_lin_lt")
      ct->type = "int_lin_ge";
    else if (id == "bool_eq")
      ct->type = "bool_ne";
    else if (id == "bool_ne")
      ct->type = "bool_eq";
    else if (id == "bool_ge")
      ct->type = "bool_lt";
    else if (id == "bool_gt")
      ct->type = "bool_le";
    else if (id == "bool_le")
      ct->type = "bool_gt";
    else if (id == "bool_lt")
      ct->type = "bool_ge";
  }
}

// Propagates the values of set_in
// Input : set_in(x, [c1..c2]) or set_in(x, {c1, .., cn})
// Action: Intersect the domain of x with the set of values.
// Output: inactive constraint.
bool FzPresolver::PresolveSetIn(FzConstraint* ct) {
  if (ct->Arg(0).IsVariable()) {
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    IntersectDomainWith(&ct->Arg(0).Var()->domain, ct->Arg(1));
    ct->MarkAsInactive();
    // TODO(user): Returns true iff the intersection yielded some domain
    // reduction.
    return true;
  }
  return false;
}

// Propagates bound product.
// Input : int_times(c1, c2, x)
// Action: reduce domain of x to {c1 * c2}
// Output: inactive constraint.
bool FzPresolver::PresolveIntTimes(FzConstraint* ct) {
  if (ct->Arg(0).HasOneValue() && ct->Arg(1).HasOneValue() &&
      ct->Arg(2).IsVariable() && !ct->presolve_propagation_done) {
    FZVLOG << " Propagate " << ct->DebugString() << FZENDL;
    const int64 value = ct->Arg(0).Value() * ct->Arg(1).Value();
    ct->presolve_propagation_done = true;
    if (ct->Arg(2).Var()->domain.Contains(value)) {
      ct->Arg(2).Var()->domain.IntersectWithInterval(value, value);
      ct->MarkAsInactive();
      return true;
    }
  }
  return false;
}

// Propagates bound product.
// Input : int_div(c1, c2, x)
// Action: reduce domain of x to {c1 / c2}
// Output: inactive constraint.
bool FzPresolver::PresolveIntDiv(FzConstraint* ct) {
  if (ct->Arg(0).HasOneValue() && ct->Arg(1).HasOneValue() &&
      ct->Arg(2).IsVariable() && !ct->presolve_propagation_done) {
    FZVLOG << " Propagate " << ct->DebugString() << FZENDL;
    const int64 value = ct->Arg(0).Value() / ct->Arg(1).Value();
    ct->presolve_propagation_done = true;
    if (ct->Arg(2).Var()->domain.Contains(value)) {
      ct->Arg(2).Var()->domain.IntersectWithInterval(value, value);
      ct->MarkAsInactive();
      return true;
    }
  }
  return false;
}

// Simplifies and reduces array_bool_or
//
// Rule 1:
// Input : array_bool_or([b1], b2)
// Output: bool_eq(b1, b2)
//
// Rule 2:
// Input : array_bool_or(b1, .., bn], false) or
//         array_bool_or(b1, .., bn], b0) with b0 assigned to false
// Action: Assign false to b1, .., bn
// Output: inactive constraint.
//
// Rule 3:
// Input : array_bool_or(b1, .., true, .., bn], b0)
// Action: Assign b0 to true
// Output: inactive constraint.
//
// Rule 4:
// Input : array_bool_or(false, .., false], b0)
// Action: Assign b0 to false
// Output: inactive constraint.
//
// Rule 5:
// Input : array_bool_or(b1, .., false, bn], b0) or
//         array_bool_or(b1, .., bi, .., bn], b0) with bi assigned to false
// Action: Remove bi or the false value
// Output: array_bool_or(b1, .., bi-1, bi+1, .., bn], b0)
bool FzPresolver::PresolveArrayBoolOr(FzConstraint* ct) {
  if (ct->Arg(0).variables.size() == 1) {
    // Rule 1.
    FZVLOG << "Rewrite " << ct->DebugString() << FZENDL;
    ct->type = "bool_eq";
    ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
    return true;
  }
  if (!ct->presolve_propagation_done && ct->Arg(1).HasOneValue() &&
      ct->Arg(1).Value() == 0) {
    // Rule 2.
    for (const FzIntegerVariable* const var : ct->Arg(0).variables) {
      if (!var->domain.Contains(0)) {
        return false;
      }
    }
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    for (FzIntegerVariable* const var : ct->Arg(0).variables) {
      var->domain.IntersectWithInterval(0, 0);
    }
    ct->MarkAsInactive();
    return true;
  }
  int num_fixed_to_true = 0;
  std::vector<FzIntegerVariable*> unbound;
  for (FzIntegerVariable* const var : ct->Arg(0).variables) {
    if (var->domain.IsSingleton()) {
      const int64 value = var->Min();
      if (value == 1) {
        num_fixed_to_true++;
      }
    } else {
      unbound.push_back(var);
    }
  }
  if (num_fixed_to_true > 0) {
    // Rule 3.
    if (!ct->Arg(1).HasOneValue()) {
      FZVLOG << "Propagate boolvar to true in " << ct->DebugString() << FZENDL;
      ct->Arg(1).variables[0]->domain.IntersectWithInterval(1, 1);
      ct->MarkAsInactive();
      return true;
    } else if (ct->Arg(1).HasOneValue() && ct->Arg(1).Value() == 1) {
      ct->MarkAsInactive();
      return true;
    }
    return false;
  } else if (unbound.empty()) {
    // Rule 4.
    if (!ct->Arg(1).HasOneValue()) {
      FZVLOG << "Propagate boolvar to false in " << ct->DebugString() << FZENDL;
      ct->Arg(1).variables[0]->domain.IntersectWithInterval(0, 0);
      ct->MarkAsInactive();
      return true;
    }
    return false;
  } else if (unbound.size() < ct->Arg(0).variables.size()) {
    // Rule 5.
    FZVLOG << "Reduce array on " << ct->DebugString() << FZENDL;
    ct->MutableArg(0)->variables.swap(unbound);
    return true;
  }
  return false;
}

// Simplifies and reduces array_bool_and
//
// Rule 1:
// Input : array_bool_and([b1], b2)
// Output: bool_eq(b1, b2)
//
// Rule 2:
// Input : array_bool_and(b1, .., bn], true) or
//         array_bool_and(b1, .., bn], b0) with b0 assigned to true
// Action: Assign b1, .., bn to true
// Output: inactive constraint.
//
// Rule 3:
// Input : array_bool_and(b1, .., false, .., bn], b0)
// Action: Assign b0 to false
// Output: inactive constraint.
//
// Rule 4:
// Input : array_bool_and(true, .., true], b0)
// Action: Assign b0 to true
// Output: inactive constraint.
//
// Rule 5:
// Input : array_bool_and(b1, .., true, bn], b0) or
//         array_bool_and(b1, .., bi, .., bn], b0) with bi assigned to true
// Action: Remove bi or the true value
// Output: array_bool_and(b1, .., bi-1, bi+1, .., bn], b0)
bool FzPresolver::PresolveArrayBoolAnd(FzConstraint* ct) {
  if (ct->Arg(0).variables.size() == 1) {
    // Rule 1.
    FZVLOG << "Rewrite " << ct->DebugString() << FZENDL;
    ct->type = "bool_eq";
    ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
    return true;
  }
  if (!ct->presolve_propagation_done && ct->Arg(1).HasOneValue() &&
      ct->Arg(1).Value() == 1) {
    // Rule 2.
    for (const FzIntegerVariable* const var : ct->Arg(0).variables) {
      if (!var->domain.Contains(1)) {
        return false;
      }
    }
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    for (FzIntegerVariable* const var : ct->Arg(0).variables) {
      var->domain.IntersectWithInterval(1, 1);
    }
    ct->presolve_propagation_done = true;
    ct->MarkAsInactive();
    return true;
  }
  int num_fixed_to_false = 0;
  std::vector<FzIntegerVariable*> unbound;
  for (FzIntegerVariable* const var : ct->Arg(0).variables) {
    if (var->domain.IsSingleton()) {
      const int64 value = var->Min();
      if (value == 0) {
        num_fixed_to_false++;
      }
    } else {
      unbound.push_back(var);
    }
  }
  if (num_fixed_to_false > 0) {
    if (!ct->Arg(1).HasOneValue()) {
      // Rule 3.
      FZVLOG << "Propagate boolvar to false in " << ct->DebugString() << FZENDL;
      ct->Arg(1).variables[0]->domain.IntersectWithInterval(0, 0);
      ct->MarkAsInactive();
      return true;
    } else if (ct->Arg(1).HasOneValue() && ct->Arg(1).Value() == 0) {
      ct->MarkAsInactive();
      return true;
    }
    return false;
  } else if (unbound.empty()) {
    // Rule 4.
    if (!ct->Arg(1).HasOneValue()) {
      FZVLOG << "Propagate boolvar to true in " << ct->DebugString() << FZENDL;
      ct->Arg(1).variables[0]->domain.IntersectWithInterval(1, 1);
      ct->MarkAsInactive();
      return true;
    }
    return false;
  } else if (unbound.size() < ct->Arg(0).variables.size()) {
    FZVLOG << "Reduce array on " << ct->DebugString() << FZENDL;
    ct->MutableArg(0)->variables.swap(unbound);
    return true;
  }
  return false;
}

// Presolves : bool_eq_reif, bool_ne_reif
//   - rewrites int_eq_reif(b1, true, b2) into bool_eq(b1, b2)
//   - rewrites int_eq_reif(b1, false, b2) into bool_not(b1, b2)
//   - rewrites int_ne_reif(b1, true, b2) into bool_bot(b1, b2)
//   - rewrites int_ne_reif(b1, false, b2) into bool_eq(b1, b2)
bool FzPresolver::PresolveBoolEqNeReif(FzConstraint* ct) {
  if (ct->Arg(1).HasOneValue()) {
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    const int64 value = ct->Arg(1).Value();
    // Remove boolean value argument.
    ct->arguments[1] = ct->Arg(2);
    ct->arguments.pop_back();
    // Change type.
    ct->type = ((ct->type == "bool_eq_reif" && value == 1) ||
                (ct->type == "bool_ne_reif" && value == 0))
                   ? "bool_eq"
                   : "bool_not";
    return true;
  }
  return false;
}

// Transform int_lin_gt into int_lin_ge.
// Input : int_lin_gt(arg1, arg2, c)
// Output: int_lin_ge(arg1, arg2, c + 1)
bool FzPresolver::PresolveIntLinGt(FzConstraint* ct) {
  FZVLOG << "Transform " << ct->DebugString() << " into int_lin_ge" << FZENDL;
  CHECK_EQ(FzArgument::INT_VALUE, ct->Arg(2).type);
  ct->MutableArg(2)->values[0]++;
  ct->type = "int_lin_ge";
  return true;
}

// Transform int_lin_lt into int_lin_le.
// Input : int_lin_lt(arg1, arg2, c)
// Output: int_lin_le(arg1, arg2, c - 1)
bool FzPresolver::PresolveIntLinLt(FzConstraint* ct) {
  FZVLOG << "Transform " << ct->DebugString() << " into int_lin_le" << FZENDL;
  CHECK_EQ(FzArgument::INT_VALUE, ct->Arg(2).type);
  ct->MutableArg(2)->values[0]--;
  ct->type = "int_lin_le";
  return true;
}

// Input: int_lin_le([-k, 1, 1, .., 1], [b, b1, .., bn], 0) with k >= n
// Output: array_bool_or([b1, .., bn], k)

bool FzPresolver::FindHiddenArrayBoolOr(FzConstraint* ct) {
  if (ct->Arg(2).Value() != 0 ||
      ct->Arg(0).values[0] > -(ct->Arg(0).values.size() - 1) ||
      !IsBooleanVar(ct->Arg(1).variables[0])) {
    return false;
  }
  for (int i = 1; i < ct->Arg(0).values.size(); ++i) {
    if (!IsBooleanValue(ct->Arg(0).values[i]) ||
        !IsBooleanVar(ct->Arg(1).variables[i])) {
      return false;
    }
  }
  FZVLOG << "Rewrite " << ct->DebugString() << " into array_bool_or" << FZENDL;
  ct->type = "array_bool_or";
  ct->MutableArg(0)->values.clear();
  ct->MutableArg(0)->type = FzArgument::INT_VAR_REF_ARRAY;
  ct->MutableArg(0)->variables.resize(ct->Arg(1).variables.size() - 1);
  for (int i = 0; i < ct->Arg(1).variables.size() - 1; ++i) {
    ct->MutableArg(0)->variables[i] = ct->Arg(1).variables[i + 1];
  }
  ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
  ct->MutableArg(1)->variables.resize(1);
  ct->arguments.pop_back();
  FZVLOG << "  -> " << ct->DebugString() << FZENDL;
  return true;
}

// Simplifies linear equations of size 1.
// Input : int_lin_xx([x], [c1], c2) and int_lin_xx_reif([x], [c1], c2, b)
//         with c1 == 1 and xx = eq, ne, lt, le, gt, ge
// Output: int_xx(x, c2) and int_xx_reif(x, c2, b)
bool FzPresolver::SimplifyUnaryLinear(FzConstraint* ct) {
  if (ct->Arg(0).values.size() == 1 && ct->Arg(0).values[0] == 1) {
    FZVLOG << "Remove linear part in " << ct->DebugString() << FZENDL;
    // transform arguments.
    ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
    ct->MutableArg(0)->values.clear();
    ct->MutableArg(0)->variables.push_back(ct->Arg(1).variables[0]);
    ct->MutableArg(1)->type = FzArgument::INT_VALUE;
    ct->MutableArg(1)->variables.clear();
    ct->MutableArg(1)->values.push_back(ct->Arg(2).values[0]);
    if (ct->arguments.size() == 4) {
      ct->arguments[2] = ct->Arg(3);
    }
    ct->arguments.pop_back();
    // Change type.
    ct->type.erase(3, 4);
    FZVLOG << "  - " << ct->DebugString() << FZENDL;
    return true;
  }
  return false;
}

// Returns false if an overflow occured.
bool ComputeLinBounds(FzConstraint* ct, int64* lb, int64* ub) {
  *lb = 0;
  *ub = 0;
  for (int i = 0; i < ct->Arg(0).values.size(); ++i) {
    const FzIntegerVariable* const var = ct->Arg(1).variables[i];
    const int64 coef = ct->Arg(0).values[i];
    if (coef == 0) continue;
    if (var->Min() == kint64min || var->Max() == kint64max) {
      return false;
    }
    const int64 vmin = var->Min();
    const int64 vmax = var->Max();
    // TODO(user): use saturated arithmetic.
    if (coef > 0) {
      *lb += vmin * coef;
      *ub += vmax * coef;
    } else {
      *lb += vmax * coef;
      *ub += vmin * coef;
    }
  }
  return true;
}

// Presolve: Check bounds of int_lin_eq_reif w.r.t. the boolean variable.
// Input : int_lin_eq_reif([x1, .., xn], [c1, .., cn], c0, b)
// Action: compute min and max of sum(x1 * c2) and
//         assign true to b is min and max == c0, or
//         assign false to b if min > c0 or max < c0
bool FzPresolver::CheckIntLinReifBounds(FzConstraint* ct) {
  int64 lb = 0;
  int64 ub = 0;
  if (!ComputeLinBounds(ct, &lb, &ub)) {
    return false;
  }
  const int64 value = ct->Arg(2).Value();
  if (ct->type == "int_lin_eq_reif") {
    if (value < lb || value > ub) {
      FZVLOG << "Assign boolean to false in " << ct->DebugString() << FZENDL;
      ct->Arg(3).Var()->domain.IntersectWithInterval(0, 0);
      ct->MarkAsInactive();
      return true;
    } else if (value == lb && value == ub) {
      FZVLOG << "Assign boolean to true in " << ct->DebugString() << FZENDL;
      ct->Arg(3).Var()->domain.IntersectWithInterval(1, 1);
      ct->MarkAsInactive();
      return true;
    }
  }
  return false;
}

// Marks target variable: int_lin_eq
//
// Rule 1:
// Input : int_lin_eq([x1, x2], [-1, c2], c0)
// Output: int_lin_eq([x1, x2], [-1, c2], c0) => x1
//
// Rule 2:
// Input : int_lin_eq([x1, x2], [c1, -1], c0)
// Output: int_lin_eq([x1, x2], [c1, -1], c0) => x2
bool FzPresolver::CreateLinearTarget(FzConstraint* ct) {
  if (ct->target_variable != nullptr) return false;

  if (ct->Arg(0).values.size() == 2 && ct->Arg(0).values[0] == -1 &&
      ct->Arg(1).variables[0]->defining_constraint == nullptr) {
    // Rule 1.
    FZVLOG << "Mark first variable of " << ct->DebugString() << " as target"
           << FZENDL;
    FzIntegerVariable* const var = ct->Arg(1).variables[0];
    var->defining_constraint = ct;
    ct->target_variable = var;
    return true;
  }
  if (ct->Arg(0).values.size() == 2 && ct->Arg(0).values[1] == -1 &&
      ct->Arg(1).variables[1]->defining_constraint == nullptr) {
    // Rule 2.
    FZVLOG << "Mark second variable of " << ct->DebugString() << " as target"
           << FZENDL;
    FzIntegerVariable* const var = ct->Arg(1).variables[1];
    var->defining_constraint = ct;
    ct->target_variable = var;
    return true;
  }
  return false;
}

// Propagates: array_int_element
// Rule 1:
// Input : array_int_element(x, [c1, .., cn], y)
// Output: array_int_element(x, [c1, .., cm], y) if all cm+1, .., cn are not
//         the domain of y.
//
// Rule 2:
// Input : array_int_element(x, [c1, .., cn], y)
// Action: Intersect the domain of x with the set of values.
bool FzPresolver::PresolveArrayIntElement(FzConstraint* ct) {
  if (ct->Arg(0).variables.size() == 1) {
    if (!ct->Arg(0).HasOneValue()) {
      // Rule 1.
      const int64 target_min = ct->Arg(2).HasOneValue()
                                   ? ct->Arg(2).Value()
                                   : ct->Arg(2).Var()->Min();
      const int64 target_max = ct->Arg(2).HasOneValue()
                                   ? ct->Arg(2).Value()
                                   : ct->Arg(2).Var()->Max();

      int64 current_index = ct->Arg(1).values.size();
      current_index = std::min(ct->Arg(0).Var()->Max(), current_index);

      while (current_index >= 1) {
        const int64 value = ct->Arg(1).values[current_index - 1];
        if (value < target_min || value > target_max) {
          current_index--;
        } else {
          break;
        }
      }
      if (current_index < ct->Arg(0).Var()->Max()) {
        FZVLOG << "Filter index of " << ct->DebugString() << " to [1 .. "
               << current_index << "]" << FZENDL;
        ct->Arg(0).Var()->domain.IntersectWithInterval(1, current_index);
        FZVLOG << "  - reduce array to size " << current_index << FZENDL;
        ct->MutableArg(1)->values.resize(current_index);
        return true;
      }
    }
  }
  if (ct->Arg(2).IsVariable() && !ct->presolve_propagation_done) {
    // Rule 2.
    FZVLOG << "Propagate domain on " << ct->DebugString() << FZENDL;
    IntersectDomainWith(&ct->Arg(2).Var()->domain, ct->Arg(1));
    ct->presolve_propagation_done = true;
    return true;
  }
  return false;
}

// Reverses a linear constraint: with negative coefficients.
// Input : int_lin_xxx([-c1, .., -cn], [x1, .., xn], c0) or
//         int_lin_xxx_reif([-c1, .., -cn], [x1, .., xn], c0, b) or
//         with c1, cn > 0
// Output: int_lin_yyy([c1, .., cn], [c1, .., cn], c0) or
//         int_lin_yyy_reif([c1, .., cn], [c1, .., cn], c0, b)
//         with yyy is the opposite of xxx (eq -> eq, ne -> ne, le -> ge,
//                                          lt -> gt, ge -> le, gt -> lt)
bool FzPresolver::PresolveLinear(FzConstraint* ct) {
  if (ct->Arg(0).values.empty()) {
    return false;
  }
  for (const int64 coef : ct->Arg(0).values) {
    if (coef > 0) {
      return false;
    }
  }
  if (ct->target_variable != nullptr) {
    for (FzIntegerVariable* const var : ct->Arg(1).variables) {
      if (var == ct->target_variable) {
        return false;
      }
    }
  }
  FZVLOG << "Reverse " << ct->DebugString() << FZENDL;
  for (int64& coef : ct->MutableArg(0)->values) {
    coef *= -1;
  }
  ct->MutableArg(2)->values[0] *= -1;
  const std::string& id = ct->type;
  if (id == "int_lin_le") {
    ct->type = "int_lin_ge";
  } else if (id == "int_lin_lt") {
    ct->type = "int_lin_gt";
  } else if (id == "int_lin_le") {
    ct->type = "int_lin_ge";
  } else if (id == "int_lin_lt") {
    ct->type = "int_lin_gt";
  } else if (id == "int_lin_le_reif") {
    ct->type = "int_lin_ge_reif";
  } else if (id == "int_lin_ge_reif") {
    ct->type = "int_lin_le_reif";
  }
  return true;
}

// Bound propagation: int_lin_eq, int_lin_le, int_lin_ge
//   - If a scalar product only contains positive constant and variables, then
//   we
//     can propagate an upper bound on all variables.
//   - If a scalar product has one term, we can also propagate a lower bound.
bool FzPresolver::PropagatePositiveLinear(FzConstraint* ct, bool upper) {
  const int64 rhs = ct->Arg(2).Value();
  if (ct->presolve_propagation_done || rhs < 0) {
    return false;
  }
  for (const int64 coef : ct->Arg(0).values) {
    if (coef < 0) {
      return false;
    }
  }
  for (FzIntegerVariable* const var : ct->Arg(1).variables) {
    if (var->Min() < 0) {
      return false;
    }
  }
  bool modified = false;
  if (upper) {
    FZVLOG << "Bound propagation on " << ct->DebugString() << FZENDL;
    for (int i = 0; i < ct->Arg(0).values.size(); ++i) {
      const int64 coef = ct->Arg(0).values[i];
      if (coef > 0) {
        FzIntegerVariable* const var = ct->Arg(1).variables[i];
        const int64 bound = rhs / coef;
        if (upper && bound < var->Max()) {
          FZVLOG << "  - intersect " << var->DebugString() << " with [0 .. "
                 << bound << "]" << FZENDL;
          var->domain.IntersectWithInterval(0, bound);
          modified = true;
        }
      }
    }
  } else if (ct->Arg(0).values.size() == 1 && ct->Arg(0).values[0] > 0) {
    const int64 coef = ct->Arg(0).values[0];
    FzIntegerVariable* const var = ct->Arg(1).variables[0];
    const int64 bound = (rhs + coef - 1) / coef;
    if (bound > var->Min()) {
      FZVLOG << "  - intersect " << var->DebugString() << " with [" << bound
             << " .. INT_MAX]" << FZENDL;
      var->domain.IntersectWithInterval(bound, kint64max);
      ct->MarkAsInactive();
      modified = true;
    }
  }
  ct->presolve_propagation_done = true;
  return modified;
}

// Minizinc flattens 2d element constraints (x = A[y][z]) into 1d element
// constraint with an affine mapping between y, z and the new index.
// This rule stores the mapping to reconstruct the 2d element constraint.
bool FzPresolver::PresolveStoreMapping(FzConstraint* ct) {
  if (ct->Arg(0).values.size() == 2 &&
      ct->Arg(1).variables[0] == ct->target_variable &&
      ct->Arg(0).values[0] == -1 &&
      !ContainsKey(affine_map_, ct->target_variable) &&
      ct->strong_propagation) {
    affine_map_[ct->target_variable] = AffineMapping(
        ct->Arg(1).variables[1], ct->Arg(0).values[1], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
    return true;
  }
  if (ct->Arg(0).values.size() == 3 &&
      ct->Arg(1).variables[0] == ct->target_variable &&
      ct->Arg(0).values[0] == -1 && ct->Arg(0).values[2] == 1 &&
      !ContainsKey(flatten_map_, ct->target_variable) &&
      ct->strong_propagation) {
    flatten_map_[ct->target_variable] =
        FlatteningMapping(ct->Arg(1).variables[1], ct->Arg(0).values[1],
                          ct->Arg(1).variables[2], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
    //    ct->MarkAsInactive();
    return true;
  } else if (ct->Arg(0).values.size() == 3 &&
             ct->Arg(1).variables[0] == ct->target_variable &&
             ct->Arg(0).values[0] == -1 && ct->Arg(0).values[1] == 1 &&
             !ContainsKey(flatten_map_, ct->target_variable) &&
             ct->strong_propagation) {
    flatten_map_[ct->target_variable] =
        FlatteningMapping(ct->Arg(1).variables[2], ct->Arg(0).values[2],
                          ct->Arg(1).variables[1], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
    //    ct->MarkAsInactive();
    return true;
  }
  return false;
}

namespace {
bool IsIncreasingContiguous(const std::vector<int64>& values) {
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] != values[i] + 1) {
      return false;
    }
  }
  return true;
}
}  // namespace

// Rewrite array element: array_int_element:
//   - If the index var is an affine mapping: do the inverse affine
//     transformation on the array of values.
//   - If the index var is the result of the flattening of 2 variables into 1,
//     the create the table constraints that maps the values, with the 2
//     variables, removing the need to the intermediate index variable.
//   - If the values are increasing and contiguous, rewrite into a simple
//     translation.
bool FzPresolver::PresolveSimplifyElement(FzConstraint* ct) {
  if (ct->Arg(0).variables.size() > 1) {
    return false;
  }
  FzIntegerVariable* const index_var = ct->Arg(0).Var();
  if (ContainsKey(affine_map_, index_var)) {
    const AffineMapping& mapping = affine_map_[index_var];
    const FzDomain& domain = mapping.variable->domain;
    if (domain.is_interval && domain.values.empty()) {
      // Invalid case. Ignore it.
      return false;
    }
    if (domain.values[0] == 0 && mapping.coefficient == 1 &&
        mapping.offset > 1 && index_var->domain.is_interval) {
      FZVLOG << "Reduce " << ct->DebugString() << FZENDL;
      // Simple translation
      const int offset = mapping.offset - 1;
      const int size = ct->Arg(1).values.size();
      for (int i = 0; i < size - offset; ++i) {
        ct->MutableArg(1)->values[i] = ct->Arg(1).values[i + offset];
      }
      ct->MutableArg(1)->values.resize(size - offset);
      affine_map_[index_var].constraint->MutableArg(2)->values[0] = -1;
      affine_map_[index_var].offset = 1;
      index_var->domain.values[0] -= offset;
      index_var->domain.values[1] -= offset;
      return true;
    } else if (mapping.offset + mapping.coefficient > 0 &&
               domain.values[0] == 1) {
      const std::vector<int64>& values = ct->Arg(1).values;
      std::vector<int64> new_values;
      for (int64 i = domain.values.front(); i <= domain.values.back(); ++i) {
        const int64 index = i * mapping.coefficient + mapping.offset - 1;
        if (index < 0) {
          return false;
        }
        if (index > values.size()) {
          break;
        }
        new_values.push_back(values[index]);
      }
      // Rewrite constraint.
      FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
      ct->MutableArg(0)->variables[0] = mapping.variable;
      ct->Arg(0).variables[0]->domain.IntersectWithInterval(1,
                                                            new_values.size());
      // TODO(user): Encapsulate argument setters.
      ct->MutableArg(1)->values.swap(new_values);
      if (ct->Arg(1).values.size() == 1) {
        ct->MutableArg(1)->type = FzArgument::INT_VALUE;
      }
      // Reset propagate flag.
      ct->presolve_propagation_done = false;
      // Mark old index var and affine constraint as presolved out.
      mapping.constraint->MarkAsInactive();
      index_var->active = false;
      FZVLOG << "  -> " << ct->DebugString() << FZENDL;
      return true;
    }
  }
  if (ContainsKey(flatten_map_, index_var)) {
    FZVLOG << "Rewrite " << ct->DebugString() << " as a 2d element" << FZENDL;
    const FlatteningMapping& mapping = flatten_map_[index_var];
    // Rewrite constraint.
    ct->MutableArg(0)->variables[0] = mapping.variable1;
    ct->MutableArg(0)->variables.push_back(mapping.variable2);
    ct->MutableArg(0)->type = FzArgument::INT_VAR_REF_ARRAY;
    std::vector<int64> coefs;
    coefs.push_back(mapping.coefficient);
    coefs.push_back(1);
    ct->arguments.push_back(FzArgument::IntegerList(coefs));
    ct->arguments.push_back(FzArgument::IntegerValue(mapping.offset));
    if (ct->target_variable != nullptr) {
      ct->RemoveTargetVariable();
    }
    index_var->active = false;
    mapping.constraint->MarkAsInactive();
    // TODO(user): Check if presolve is valid.
    return true;
  }
  if (index_var->domain.IsSingleton()) {
    FZVLOG << "Rewrite " << ct->DebugString() << FZENDL;
    const int64 index = index_var->domain.values[0] - 1;
    const int64 value = ct->Arg(1).values[index];
    // Rewrite as equality.
    ct->type = "int_eq";
    ct->MutableArg(0)->variables.clear();
    ct->MutableArg(0)->values.push_back(value);
    ct->MutableArg(0)->type = FzArgument::INT_VALUE;
    *ct->MutableArg(1) = ct->Arg(2);
    ct->arguments.pop_back();
    FZVLOG << "  -> " << ct->DebugString() << FZENDL;
    return true;
  }
  if (index_var->domain.is_interval && index_var->domain.values.size() == 2 &&
      index_var->Max() < ct->Arg(1).values.size()) {
    // Reduce array of values.
    ct->MutableArg(1)->values.resize(index_var->Max());
    ct->presolve_propagation_done = false;
    FZVLOG << "Reduce array on " << ct->DebugString() << FZENDL;
    return true;
  }
  if (IsIncreasingContiguous(ct->Arg(1).values)) {
    const int64 start = ct->Arg(1).values.front();
    FzIntegerVariable* const index = ct->Arg(0).Var();
    FzIntegerVariable* const target = ct->Arg(2).Var();
    FZVLOG << "Linearize " << ct->DebugString() << FZENDL;

    if (start == 1) {
      ct->type = "int_eq";
      ct->arguments[1] = ct->Arg(2);
      ct->arguments.pop_back();
    } else {
      // Rewrite constraint into a int_lin_eq
      ct->type = "int_lin_eq";
      ct->MutableArg(0)->type = FzArgument::INT_LIST;
      ct->MutableArg(0)->variables.clear();
      ct->MutableArg(0)->values.push_back(-1);
      ct->MutableArg(0)->values.push_back(1);
      ct->MutableArg(1)->type = FzArgument::INT_VAR_REF_ARRAY;
      ct->MutableArg(1)->values.clear();
      ct->MutableArg(1)->variables.push_back(target);
      ct->MutableArg(1)->variables.push_back(index);
      ct->MutableArg(2)->type = FzArgument::INT_VALUE;
      ct->MutableArg(2)->variables.clear();
      ct->MutableArg(2)->values.push_back(1 - start);
    }

    FZVLOG << "  -> " << ct->DebugString() << FZENDL;
    return true;
  }
  return false;
}

// Simplifies array_var_int_element
//   - Reconstructs the 2d element constraint in case the mapping only
//   contains 1 variable. (e.g. x = A[2][y]).
//   - Change to array_int_element if all variables are bound.
bool FzPresolver::PresolveSimplifyExprElement(FzConstraint* ct) {
  bool all_integers = true;
  for (FzIntegerVariable* const var : ct->Arg(1).variables) {
    if (!var->domain.IsSingleton()) {
      all_integers = false;
      break;
    }
  }
  if (all_integers) {
    FZVLOG << "Change " << ct->DebugString() << " to array_int_element"
           << FZENDL;
    ct->type = "array_int_element";
    ct->MutableArg(1)->type = FzArgument::INT_LIST;
    for (int i = 0; i < ct->Arg(1).variables.size(); ++i) {
      ct->MutableArg(1)->values.push_back(ct->Arg(1).variables[i]->Min());
    }
    ct->MutableArg(1)->variables.clear();
    return true;
  }
  FzIntegerVariable* const index_var = ct->Arg(0).Var();
  if (index_var->domain.IsSingleton()) {
    // Arrays are 1 based.
    const int64 position = index_var->Min() - 1;
    FzIntegerVariable* const expr = ct->Arg(1).variables[position];
    // Index is fixed, rewrite constraint into an equality.
    FZVLOG << "Rewrite " << ct->DebugString() << FZENDL;
    ct->type = "int_eq";
    ct->MutableArg(0)->variables[0] = expr;
    ct->arguments[1] = ct->Arg(2);
    ct->arguments.pop_back();
    return true;
  } else if (ContainsKey(affine_map_, index_var)) {
    const AffineMapping& mapping = affine_map_[index_var];
    const FzDomain& domain = mapping.variable->domain;
    if ((domain.is_interval && domain.values.empty()) ||
        domain.values[0] != 1 || mapping.offset + mapping.coefficient <= 0) {
      // Invalid case. Ignore it.
      return false;
    }
    const std::vector<FzIntegerVariable*>& vars = ct->Arg(1).variables;
    std::vector<FzIntegerVariable*> new_vars;
    for (int64 i = domain.values.front(); i <= domain.values.back(); ++i) {
      const int64 index = i * mapping.coefficient + mapping.offset - 1;
      if (index < 0) {
        return false;
      }
      if (index >= vars.size()) {
        break;
      }
      new_vars.push_back(vars[index]);
    }
    // Rewrite constraint.
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    ct->MutableArg(0)->variables[0] = mapping.variable;
    // TODO(user): Encapsulate argument setters.
    ct->MutableArg(1)->variables.swap(new_vars);
    // Reset propagate flag.
    ct->presolve_propagation_done = false;
    FZVLOG << "    into  " << ct->DebugString() << FZENDL;
    // Mark old index var and affine constraint as presolved out.
    mapping.constraint->MarkAsInactive();
    index_var->active = false;
    return true;
  }
  if (index_var->domain.is_interval && index_var->domain.values.size() == 2 &&
      index_var->Max() < ct->Arg(1).variables.size()) {
    // Reduce array of variables.
    ct->MutableArg(1)->variables.resize(index_var->Max());
    ct->presolve_propagation_done = false;
    FZVLOG << "Reduce array on " << ct->DebugString() << FZENDL;
    return true;
  }
  return false;
}

// Propagate reified comparison: int_eq_reif, int_ge_reif, int_le_reif:
//
// Rule1:
// Input : int_xx_reif(x, x, b) or bool_eq_reif(b1, b1, b)
// Action: Set b to true if xx in {le, ge, eq}, or false otherwise.
// Output: inactive constraint.
//
// Rule 2:
// Input : int_xx_reif(x, c, b) or bool_xx_reif(b1, t, b) or
//         int_xx_reif(c, x, b) or bool_xx_reif(t, b2, b)
// Action: Assign b to true or false if this can be decided from the of x and
//         c, or the comparison of b1/b2 with t.
// Output: inactive constraint of b was assigned a value.
bool FzPresolver::PropagateReifiedComparisons(FzConstraint* ct) {
  const std::string& id = ct->type;
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF &&
      ct->Arg(1).type == FzArgument::INT_VAR_REF &&
      ct->Arg(0).variables[0] == ct->Arg(1).variables[0]) {
    // Rule 1.
    const bool value =
        (id == "int_eq_reif" || id == "int_ge_reif" || id == "int_le_reif" ||
         id == "bool_eq_reif" || id == "bool_ge_reif" || id == "bool_le_reif");
    if ((ct->Arg(2).HasOneValue() && ct->Arg(2).Value() == value) ||
        !ct->Arg(2).HasOneValue()) {
      FZVLOG << "Propagate boolvar from " << ct->DebugString() << " to "
             << value << FZENDL;
      CHECK_EQ(FzArgument::INT_VAR_REF, ct->Arg(2).type);
      ct->Arg(2).variables[0]->domain.IntersectWithInterval(value, value);
      ct->MarkAsInactive();
      return true;
    }
  }
  FzIntegerVariable* var = nullptr;
  int64 value = 0;
  bool reverse = false;
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF && ct->Arg(1).HasOneValue()) {
    var = ct->Arg(0).Var();
    value = ct->Arg(1).Value();
  } else if (ct->Arg(1).type == FzArgument::INT_VAR_REF &&
             ct->Arg(0).HasOneValue()) {
    var = ct->Arg(1).Var();
    value = ct->Arg(0).Value();
    reverse = true;
  }
  if (var != nullptr) {
    int state = 2;  // 0 force_false, 1 force true, 2 unknown.
    if (id == "int_eq_reif" || id == "bool_eq_reif") {
      if (var->domain.Contains(value)) {
        if (var->domain.IsSingleton()) {
          state = 1;
        }
      } else {
        state = 0;
      }
    } else if (id == "int_ne_reif" || id == "bool_ne_reif") {
      if (var->domain.Contains(value)) {
        if (var->domain.IsSingleton()) {
          state = 0;
        }
      } else {
        state = 1;
      }
    } else if ((((id == "int_lt_reif" || id == "bool_lt_reif") && reverse) ||
                ((id == "int_gt_reif" || id == "bool_gt_reif") && !reverse)) &&
               !var->IsAllInt64()) {  // int_gt
      if (var->Min() > value) {
        state = 1;
      } else if (var->Max() <= value) {
        state = 0;
      }
    } else if ((((id == "int_lt_reif" || id == "bool_lt_reif") && !reverse) ||
                ((id == "int_gt_reif" || id == "bool_gt_reif") && reverse)) &&
               !var->IsAllInt64()) {  // int_lt
      if (var->Max() < value) {
        state = 1;
      } else if (var->Min() >= value) {
        state = 0;
      }
    } else if ((((id == "int_le_reif" || id == "bool_le_reif") && reverse) ||
                ((id == "int_ge_reif" || id == "bool_ge_reif") && !reverse)) &&
               !var->IsAllInt64()) {  // int_ge
      if (var->Min() >= value) {
        state = 1;
      } else if (var->Max() < value) {
        state = 0;
      }
    } else if ((((id == "int_le_reif" || id == "bool_le_reif") && !reverse) ||
                ((id == "int_ge_reif" || id == "bool_ge_reif") && reverse)) &&
               !var->IsAllInt64()) {  // int_le
      if (var->Max() <= value) {
        state = 1;
      } else if (var->Min() > value) {
        state = 0;
      }
    }
    if (state != 2) {
      FZVLOG << "Assign boolvar to " << state << " in " << ct->DebugString()
             << FZENDL;
      ct->Arg(2).Var()->domain.IntersectWithInterval(state, state);
      ct->MarkAsInactive();
      return true;
    }
  }
  return false;
}

// Remove abs from int_le_reif.
// Input : int_le_reif(x, 0, b) or int_le_reif(x,c, b) with x == abs(y)
// Output: int_eq_reif(y, 0, b) or set_in_reif(y, [-c, c], b)
bool FzPresolver::RemoveAbsFromIntLinReif(FzConstraint* ct) {
  FZVLOG << "Remove abs() from " << ct->DebugString() << FZENDL;
  ct->MutableArg(0)->variables[0] = abs_map_[ct->Arg(0).Var()];
  const int64 value = ct->Arg(1).Value();
  if (value == 0) {
    ct->type = "int_eq_reif";
    return true;
  } else {
    ct->type = "set_in_reif";
    ct->MutableArg(1)->type = FzArgument::INT_INTERVAL;
    ct->MutableArg(1)->values[0] = -value;
    ct->MutableArg(1)->values.push_back(value);
    // set_in_reif does not implement reification.
    ct->RemoveTargetVariable();
    return true;
  }
}

// Propagates bool_not
//
// Rule 1:
// Input : bool_not(t, b)
// Action: assign not(t) to b
// Output: inactive constraint.
//
// Rule 2:
// Input : bool_not(b, t)
// Action: assign not(t) to b
// Output: inactive constraint.
//
// Rule 3:
// Input : bool_not(b1, b2)
// Output: bool_not(b1, b2) => b1 if b1 is not already a target variable.
//
// Rule 4:
// Input : bool_not(b1, b2)
// Output: bool_not(b1, b2) => b2 if b2 is not already a target variable.
bool FzPresolver::PresolveBoolNot(FzConstraint* ct) {
  if (ct->Arg(0).HasOneValue() && ct->Arg(1).IsVariable()) {
    const int64 value = ct->Arg(0).Value() == 0;
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    ct->Arg(1).Var()->domain.IntersectWithInterval(value, value);
    ct->MarkAsInactive();
    return true;
  } else if (ct->Arg(1).HasOneValue() && ct->Arg(0).IsVariable()) {
    const int64 value = ct->Arg(1).Value() == 0;
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    ct->Arg(0).Var()->domain.IntersectWithInterval(value, value);
    ct->MarkAsInactive();
    return true;
  } else if (ct->target_variable == nullptr &&
             ct->Arg(0).Var()->defining_constraint == nullptr) {
    FZVLOG << "Insert target variable in " << ct->DebugString() << FZENDL;
    FzIntegerVariable* const var = ct->Arg(0).Var();
    ct->target_variable = var;
    var->defining_constraint = ct;
    return true;
  } else if (ct->target_variable == nullptr &&
             ct->Arg(1).Var()->defining_constraint == nullptr) {
    FZVLOG << "Insert target variable in " << ct->DebugString() << FZENDL;
    FzIntegerVariable* const var = ct->Arg(1).Var();
    ct->target_variable = var;
    var->defining_constraint = ct;
    return true;
  }
  return false;
}

// Simplify boolean formula: int_lin_eq
//
// Rule 1:
// Input : int_lin_eq_reif([1, 1], [b1, b2], 1, b0)
// Output: bool_ne_reif(b1, b2, b0)
//
// Rule 2:
// Input : int_lin_eq_reif([1, 1], [false, b2], 1, b0)
// Output: bool_eq(b2, b0)
//
// Rule 3:
// Input : int_lin_eq_reif([1, 1], [true, b2], 1, b0)
// Output: bool_not(b2, b0)
//
// Rule 4:
// Input : int_lin_eq_reif([1, 1], [b1, false], 1, b0)
// Output: bool_eq(b1, b0)
//
// Rule 5:
// Input : int_lin_eq_reif([1, 1], [b1, true], 1, b0)
// Output: bool_not(b1, b0)
bool FzPresolver::SimplifyIntLinEqReif(FzConstraint* ct) {
  if (ct->Arg(0).values.size() == 2 && ct->Arg(0).values[0] == 1 &&
      ct->Arg(0).values[1] == 1 && ct->Arg(2).Value() == 1) {
    FzIntegerVariable* const left = ct->Arg(1).variables[0];
    FzIntegerVariable* const right = ct->Arg(1).variables[1];
    FzIntegerVariable* const target = ct->Arg(3).Var();
    if (IsBooleanVar(ct->Arg(1).variables[0]) &&
        IsBooleanVar(ct->Arg(1).variables[1])) {
      // Rule 1.
      FZVLOG << "Rewrite " << ct->DebugString() << " to bool_ne_reif" << FZENDL;
      ct->type = "bool_ne_reif";
      ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
      ct->MutableArg(0)->values.clear();
      ct->MutableArg(0)->variables.push_back(left);
      ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
      ct->MutableArg(1)->variables.clear();
      ct->MutableArg(1)->variables.push_back(right);
      ct->MutableArg(2)->type = FzArgument::INT_VAR_REF;
      ct->MutableArg(2)->values.clear();
      ct->MutableArg(2)->variables.push_back(target);
      ct->arguments.pop_back();
      FZVLOG << " -> " << ct->DebugString() << FZENDL;
      return true;
    }
    if (IsBooleanVar(right) && left->HasOneValue() &&
        IsBooleanValue(left->Min())) {
      if (left->Min() == 0) {
        // Rule 2.
        FZVLOG << "Rewrite " << ct->DebugString() << " to bool_eq" << FZENDL;
        ct->type = "bool_eq";
        ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(0)->values.clear();
        ct->MutableArg(0)->variables.push_back(right);
        ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(1)->variables.clear();
        ct->MutableArg(1)->variables.push_back(target);
        ct->arguments.pop_back();
        ct->arguments.pop_back();
        FZVLOG << " -> " << ct->DebugString() << FZENDL;
        return true;
      } else {
        // Rule 3.
        FZVLOG << "Rewrite " << ct->DebugString() << " to bool_not" << FZENDL;
        ct->type = "bool_not";
        ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(0)->values.clear();
        ct->MutableArg(0)->variables.push_back(right);
        ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(1)->variables.clear();
        ct->MutableArg(1)->variables.push_back(target);
        ct->arguments.pop_back();
        ct->arguments.pop_back();
        FZVLOG << " -> " << ct->DebugString() << FZENDL;
        return true;
      }
    } else if (IsBooleanVar(left) && right->HasOneValue() &&
               IsBooleanValue(right->Min())) {
      if (right->Min() == 0) {
        // Rule 4.
        FZVLOG << "Rewrite " << ct->DebugString() << " to bool_eq" << FZENDL;
        ct->type = "bool_eq";
        ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(0)->values.clear();
        ct->MutableArg(0)->variables.push_back(left);
        ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(1)->variables.clear();
        ct->MutableArg(1)->variables.push_back(target);
        ct->arguments.pop_back();
        ct->arguments.pop_back();
        FZVLOG << " -> " << ct->DebugString() << FZENDL;
        return true;
      } else {
        // Rule 5.
        FZVLOG << "Rewrite " << ct->DebugString() << " to bool_not" << FZENDL;
        ct->type = "bool_not";
        ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(0)->values.clear();
        ct->MutableArg(0)->variables.push_back(left);
        ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(1)->variables.clear();
        ct->MutableArg(1)->variables.push_back(target);
        ct->arguments.pop_back();
        ct->arguments.pop_back();
        FZVLOG << " -> " << ct->DebugString() << FZENDL;
        return true;
      }
    }
  }
  return false;
}

// Remove target variable from int_mod if bound.
//
// Input : int_mod(x1, x2, x3)  => x3
// Output: int_mod(x1, x2, x3) if x3 has only one value.
//
bool FzPresolver::PresolveIntMod(FzConstraint* ct) {
  if (ct->target_variable != nullptr &&
      ct->Arg(2).Var() == ct->target_variable && ct->Arg(2).HasOneValue()) {
    ct->target_variable->defining_constraint = nullptr;
    ct->target_variable = nullptr;
    return true;
  }
  return false;
}

// Main presolve rule caller.
bool FzPresolver::PresolveOneConstraint(FzConstraint* ct) {
  bool changed = false;
  const std::string& id = ct->type;
  const int num_arguments = ct->arguments.size();
  if (HasSuffixString(id, "_reif") &&
      ct->Arg(num_arguments - 1).HasOneValue()) {
    Unreify(ct);
    changed = true;
  }
  if (id == "bool2int") changed |= PresolveBool2Int(ct);
  if (id == "int_le" || id == "int_lt" || id == "int_ge" || id == "int_gt" ||
      id == "bool_le" || id == "bool_lt" || id == "bool_ge" ||
      id == "bool_gt") {
    changed |= PresolveInequalities(ct);
  }
  if (id == "int_abs" && !ContainsKey(abs_map_, ct->Arg(1).Var())) {
    // Stores abs() map.
    FZVLOG << "Stores abs map for " << ct->DebugString() << FZENDL;
    abs_map_[ct->Arg(1).Var()] = ct->Arg(0).Var();
    changed = true;
  }
  if ((id == "int_eq_reif" || id == "int_ne_reif" || id == "int_ne") &&
      ct->Arg(1).HasOneValue() && ct->Arg(1).Value() == 0 &&
      ContainsKey(abs_map_, ct->Arg(0).Var())) {
    // Simplifies int_eq and int_ne with abs
    // Input : int_eq(x, 0) or int_ne(x, 0) with x == abs(y)
    // Output: int_eq(y, 0) or int_ne(y, 0)
    FZVLOG << "Remove abs() from " << ct->DebugString() << FZENDL;
    ct->MutableArg(0)->variables[0] = abs_map_[ct->Arg(0).Var()];
    changed = true;
  }
  if ((id == "int_le_reif") && ct->Arg(1).HasOneValue() &&
      ContainsKey(abs_map_, ct->Arg(0).Var())) {
    changed |= RemoveAbsFromIntLinReif(ct);
  }
  if (id == "int_eq" || id == "bool_eq") changed |= PresolveIntEq(ct);
  if (id == "int_ne" || id == "bool_not") changed |= PresolveIntNe(ct);
  if (id == "set_in") changed |= PresolveSetIn(ct);
  if (id == "array_bool_and") changed |= PresolveArrayBoolAnd(ct);
  if (id == "array_bool_or") changed |= PresolveArrayBoolOr(ct);
  if (id == "bool_eq_reif" || id == "bool_ne_reif") {
    changed |= PresolveBoolEqNeReif(ct);
  }
  if (id == "bool_not") {
    changed |= PresolveBoolNot(ct);
  }
  if (id == "int_lin_le") {
    changed |= FindHiddenArrayBoolOr(ct);
  }
  if (id == "int_div") changed |= PresolveIntDiv(ct);
  if (id == "int_times") changed |= PresolveIntTimes(ct);
  if (id == "int_lin_gt") changed |= PresolveIntLinGt(ct);
  if (id == "int_lin_lt") changed |= PresolveIntLinLt(ct);
  if (HasPrefixString(id, "int_lin_")) {
    changed |= PresolveLinear(ct);
    changed |= SimplifyUnaryLinear(ct);
  }
  if (id == "int_lin_eq" || id == "int_lin_le" || id == "int_lin_ge") {
    const bool upper = !(id == "int_lin_ge");
    changed |= PropagatePositiveLinear(ct, upper);
  }
  if (id == "int_lin_eq") {
    changed |= CreateLinearTarget(ct);
  }
  if (id == "int_lin_eq") changed |= PresolveStoreMapping(ct);
  if (id == "int_lin_eq_reif") changed |= CheckIntLinReifBounds(ct);
  if (id == "int_lin_eq_reif") changed |= SimplifyIntLinEqReif(ct);
  if (id == "array_int_element") {
    changed |= PresolveSimplifyElement(ct);
  }
  // Type could have changed in the previous rule. We need to test again.
  if (id == "array_int_element") {
    changed |= PresolveArrayIntElement(ct);
  }
  if (id == "array_var_int_element") {
    changed |= PresolveSimplifyExprElement(ct);
  }
  if (id == "int_eq_reif" || id == "int_ne_reif" || id == "int_le_reif" ||
      id == "int_lt_reif" || id == "int_ge_reif" || id == "int_gt_reif" ||
      id == "bool_eq_reif" || id == "bool_ne_reif" || id == "bool_le_reif" ||
      id == "bool_lt_reif" || id == "bool_ge_reif" || id == "bool_gt_reif") {
    changed |= PropagateReifiedComparisons(ct);
  }
  if (id == "int_mod") {
    changed |= PresolveIntMod(ct);
  }
  return changed;
}

void FzPresolver::StoreDifference(FzConstraint* ct) {
  if (ct->Arg(2).Value() == 0 && ct->Arg(0).values.size() == 3) {
    // Looking for a difference var.
    if ((ct->Arg(0).values[0] == 1 && ct->Arg(0).values[1] == -1 &&
         ct->Arg(0).values[2] == 1) ||
        (ct->Arg(0).values[0] == -1 && ct->Arg(0).values[1] == 1 &&
         ct->Arg(0).values[2] == -1)) {
      FZVLOG << "Store differences from " << ct->DebugString() << FZENDL;
      difference_map_[ct->Arg(1).variables[0]] =
          std::make_pair(ct->Arg(1).variables[2], ct->Arg(1).variables[1]);
      difference_map_[ct->Arg(1).variables[2]] =
          std::make_pair(ct->Arg(1).variables[0], ct->Arg(1).variables[1]);
    }
  }
}

void FzPresolver::FirstPassModelScan(FzModel* model) {
  for (FzConstraint* const ct : model->constraints()) {
    if (!ct->active) continue;
    if (ct->type == "int_lin_eq") {
      StoreDifference(ct);
    }
  }

  // Collect decision variables.
  std::vector<FzIntegerVariable*> vars;
  for (const FzAnnotation& ann : model->search_annotations()) {
    ann.GetAllIntegerVariables(&vars);
  }
  decision_variables_.insert(vars.begin(), vars.end());
}

bool FzPresolver::Run(FzModel* model) {
  FirstPassModelScan(model);

  bool changed_since_start = false;
  // Let's presolve the bool2int predicates first.
  for (FzConstraint* const ct : model->constraints()) {
    if (ct->active && ct->type == "bool2int") {
      changed_since_start |= PresolveBool2Int(ct);
    }
  }
  if (!var_representative_map_.empty()) {
    // Some new substitutions were introduced. Let's process them.
    SubstituteEverywhere(model);
    var_representative_map_.clear();
  }

  // Apply the rest of the presolve rules.
  for (;;) {
    bool changed = false;
    var_representative_map_.clear();
    for (FzConstraint* const ct : model->constraints()) {
      if (ct->active) {
        changed |= PresolveOneConstraint(ct);
      }
      if (!var_representative_map_.empty()) {
        break;
      }
    }
    if (!var_representative_map_.empty()) {
      // Some new substitutions were introduced. Let's process them.
      DCHECK(changed);
      changed = true;  // To be safe in opt mode.
      SubstituteEverywhere(model);
      var_representative_map_.clear();
    }
    changed_since_start |= changed;
    if (!changed) break;
  }
  return changed_since_start;
}

// ----- Substitution support -----

void FzPresolver::MarkVariablesAsEquivalent(FzIntegerVariable* from,
                                            FzIntegerVariable* to) {
  CHECK(from != nullptr);
  CHECK(to != nullptr);
  // Apply the substitutions, if any.
  from = FindRepresentativeOfVar(from);
  to = FindRepresentativeOfVar(to);
  if (to->temporary) {
    // Let's switch to keep a non temporary as representative.
    FzIntegerVariable* tmp = to;
    to = from;
    from = tmp;
  }
  if (from != to) {
    FZVLOG << "Mark " << from->DebugString() << " as equivalent to "
           << to->DebugString() << FZENDL;
    if (from->defining_constraint != nullptr &&
        to->defining_constraint != nullptr) {
      FZVLOG << "  - break target_variable on "
             << from->defining_constraint->DebugString() << FZENDL;
      from->defining_constraint->RemoveTargetVariable();
    }
    CHECK(to->Merge(from->name, from->domain, from->defining_constraint,
                    from->temporary));
    from->active = false;
    var_representative_map_[from] = to;
  }
}

FzIntegerVariable* FzPresolver::FindRepresentativeOfVar(
    FzIntegerVariable* var) {
  if (var == nullptr) return nullptr;
  FzIntegerVariable* start_var = var;
  // First loop: find the top parent.
  for (;;) {
    FzIntegerVariable* parent =
        FindWithDefault(var_representative_map_, var, var);
    if (parent == var) break;
    var = parent;
  }
  // Second loop: attach all the path to the top parent.
  while (start_var != var) {
    FzIntegerVariable* const parent = var_representative_map_[start_var];
    var_representative_map_[start_var] = var;
    start_var = parent;
  }
  return FindWithDefault(var_representative_map_, var, var);
}

void FzPresolver::SubstituteEverywhere(FzModel* model) {
  // Rewrite the constraints.
  for (FzConstraint* const ct : model->constraints()) {
    if (ct != nullptr && ct->active) {
      for (int i = 0; i < ct->arguments.size(); ++i) {
        FzArgument* argument = &ct->arguments[i];
        switch (argument->type) {
          case FzArgument::INT_VAR_REF:
          case FzArgument::INT_VAR_REF_ARRAY: {
            for (int i = 0; i < argument->variables.size(); ++i) {
              argument->variables[i] =
                  FindRepresentativeOfVar(argument->variables[i]);
            }
            break;
          }
          default: {}
        }
      }
      ct->target_variable = FindRepresentativeOfVar(ct->target_variable);
    }
  }
  // Rewrite the search.
  for (FzAnnotation* const ann : model->mutable_search_annotations()) {
    SubstituteAnnotation(ann);
  }
  // Rewrite the output.
  for (FzOnSolutionOutput* const output : model->mutable_output()) {
    output->variable = FindRepresentativeOfVar(output->variable);
    for (int i = 0; i < output->flat_variables.size(); ++i) {
      output->flat_variables[i] =
          FindRepresentativeOfVar(output->flat_variables[i]);
    }
  }
  // Do not forget to merge domain that could have evolved asynchronously
  // during presolve.
  for (const auto& iter : var_representative_map_) {
    iter.second->domain.IntersectWithFzDomain(iter.first->domain);
  }
}

void FzPresolver::SubstituteAnnotation(FzAnnotation* ann) {
  // TODO(user): Remove recursion.
  switch (ann->type) {
    case FzAnnotation::ANNOTATION_LIST:
    case FzAnnotation::FUNCTION_CALL: {
      for (int i = 0; i < ann->annotations.size(); ++i) {
        SubstituteAnnotation(&ann->annotations[i]);
      }
      break;
    }
    case FzAnnotation::INT_VAR_REF:
    case FzAnnotation::INT_VAR_REF_ARRAY: {
      for (int i = 0; i < ann->variables.size(); ++i) {
        ann->variables[i] = FindRepresentativeOfVar(ann->variables[i]);
      }
      break;
    }
    default: {}
  }
}

// ----- Helpers -----

void FzPresolver::IntersectDomainWith(FzDomain* domain, const FzArgument& arg) {
  switch (arg.type) {
    case FzArgument::INT_VALUE: {
      const int64 value = arg.Value();
      domain->IntersectWithInterval(value, value);
      break;
    }
    case FzArgument::INT_INTERVAL: {
      domain->IntersectWithInterval(arg.values[0], arg.values[1]);
      break;
    }
    case FzArgument::INT_LIST: {
      domain->IntersectWithListOfIntegers(arg.values);
      break;
    }
    default: { LOG(FATAL) << "Wrong domain type" << arg.DebugString(); }
  }
}

// ----- Clean up model -----

namespace {
void Regroup(FzConstraint* start, const std::vector<FzIntegerVariable*>& chain,
             const std::vector<FzIntegerVariable*>& carry_over) {
  // End of chain, reconstruct.
  FzIntegerVariable* const out = carry_over.back();
  start->arguments.pop_back();
  start->MutableArg(0)->variables[0] = out;
  start->MutableArg(1)->type = FzArgument::INT_VAR_REF_ARRAY;
  start->MutableArg(1)->variables = chain;
  const std::string old_type = start->type;
  start->type = start->type == "int_min" ? "minimum_int" : "maximum_int";
  start->target_variable = out;
  out->defining_constraint = start;
  for (FzIntegerVariable* const var : carry_over) {
    if (var != carry_over.back()) {
      var->active = false;
    }
  }
  FZVLOG << "Regroup chain of " << old_type << " into " << start->DebugString()
         << FZENDL;
}

void CheckRegroupStart(FzConstraint* ct, FzConstraint** start,
                       std::vector<FzIntegerVariable*>* chain,
                       std::vector<FzIntegerVariable*>* carry_over) {
  if ((ct->type == "int_min" || ct->type == "int_max") &&
      ct->Arg(0).Var() == ct->Arg(1).Var()) {
    // This is the start of the chain.
    *start = ct;
    chain->push_back(ct->Arg(0).Var());
    carry_over->push_back(ct->Arg(2).Var());
    carry_over->back()->defining_constraint = nullptr;
  }
}
}  // namespace

void FzPresolver::CleanUpModelForTheCpSolver(FzModel* model, bool use_sat) {
  // First pass.
  for (FzConstraint* const ct : model->constraints()) {
    const std::string& id = ct->type;
    // Remove ignored annotations on int_lin_eq.
    if (id == "int_lin_eq" && ct->strong_propagation) {
      if (ct->Arg(0).values.size() > 3) {
        // We will use a table constraint. Remove the target variable flag.
        FZVLOG << "Remove target_variable from " << ct->DebugString() << FZENDL;
        ct->RemoveTargetVariable();
      }
    }
    // Remove target variables from constraints passed to SAT.
    if (use_sat && ct->target_variable != nullptr &&
        (id == "array_bool_and" || id == "array_bool_or" ||
         ((id == "bool_eq_reif" || id == "bool_ne_reif") &&
          !ct->Arg(1).HasOneValue()) ||
         id == "bool_le_reif" || id == "bool_ge_reif")) {
      ct->RemoveTargetVariable();
    }
    // Remove target variables from constraints that will not implement it.
    if (id == "count_reif" || id == "set_in_reif") {
      ct->RemoveTargetVariable();
    }
  }
  // Second pass.
  for (FzConstraint* const ct : model->constraints()) {
    const std::string& id = ct->type;
    // Create new target variables with unused boolean variables.
    if (ct->target_variable == nullptr &&
        (id == "int_lin_eq_reif" || id == "int_lin_ne_reif" ||
         id == "int_lin_ge_reif" || id == "int_lin_le_reif" ||
         id == "int_lin_gt_reif" || id == "int_lin_lt_reif" ||
         id == "int_eq_reif" || id == "int_ne_reif" || id == "int_le_reif" ||
         id == "int_ge_reif" || id == "int_lt_reif" || id == "int_gt_reif")) {
      FzIntegerVariable* const bool_var = ct->Arg(2).Var();
      if (bool_var != nullptr && bool_var->defining_constraint == nullptr) {
        FZVLOG << "Create target_variable on " << ct->DebugString() << FZENDL;
        ct->target_variable = bool_var;
        bool_var->defining_constraint = ct;
      }
    }
  }
  // Regroup int_min and int_max into maximum_int and maximum_int.
  // The minizinc to flatzinc expander will transform x = std::max([v1, .., vn])
  // into:
  //   tmp1 = std::max(v1, v1)
  //   tmp2 = std::max(v2, tmp1)
  //   tmp3 = std::max(v3, tmp2)
  // ...
  // This code reconstructs the initial std::min(array) or std::max(array).
  FzConstraint* start = nullptr;
  std::vector<FzIntegerVariable*> chain;
  std::vector<FzIntegerVariable*> carry_over;
  hash_map<const FzIntegerVariable*,
           hash_set<const FzConstraint*>> var_to_constraint;
  for (FzConstraint* const ct : model->constraints()) {
    for (const FzArgument& arg : ct->arguments) {
      for (FzIntegerVariable* const var : arg.variables) {
        var_to_constraint[var].insert(ct);
      }
    }
  }
  for (FzConstraint* const ct : model->constraints()) {
    if (start == nullptr) {
      CheckRegroupStart(ct, &start, &chain, &carry_over);
    } else if (ct->type == start->type &&
               ct->Arg(1).Var() == carry_over.back() &&
               var_to_constraint[ct->Arg(2).Var()].size() <= 2) {
      chain.push_back(ct->Arg(0).Var());
      carry_over.push_back(ct->Arg(2).Var());
      ct->active = false;
      ct->target_variable = nullptr;
      carry_over.back()->defining_constraint = nullptr;
    } else {
      Regroup(start, chain, carry_over);
      // Clean
      start = nullptr;
      chain.clear();
      carry_over.clear();
      // Check again ct.
      CheckRegroupStart(ct, &start, &chain, &carry_over);
    }
  }
  // Checks left over from the loop.
  if (start != nullptr) {
    Regroup(start, chain, carry_over);
  }
}
}  // namespace operations_research
