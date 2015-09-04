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

#include "flatzinc/presolve.h"
#include "util/saturated_arithmetic.h"
#include "base/strutil.h"
#include "base/map_util.h"

DECLARE_bool(fz_logging);
DECLARE_bool(fz_verbose);
namespace operations_research {

namespace {
// TODO(user): accept variables fixed to 0 or 1.
bool Has01Values(FzIntegerVariable* var) {
  return var->Min() == 0 && var->Max() == 1;
}

bool Is0Or1(int64 value) { return !(value & 0xfffffffffffffffe); }

template <class T>
bool IsArrayBoolean(const std::vector<T>& values) {
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != 0 && values[i] != 1) {
      return false;
    }
  }
  return true;
}

template <class T>
bool OnlyOne0OrOnlyOne1(const std::vector<T>& values) {
  int num_zero = 0;
  int num_one = 0;
  for (T val : values) {
    if (val) {
      num_one++;
    } else {
      num_zero++;
    }
    if (num_one > 1 && num_zero > 1) {
      return false;
    }
  }
  return true;
}
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
// Rule 1:
// Input: bool2int(b, c) or bool2int(t, x)
// Output: int_eq(...)
//
// Rule 2:
// Input: bool2int(b, x)
// Action: Replace all instances of x by b.
// Output: inactive constraint
bool FzPresolver::PresolveBool2Int(FzConstraint* ct) {
  if (ct->Arg(0).HasOneValue() || ct->Arg(1).HasOneValue()) {
    // Rule 1.
    FZVLOG << "Changing bound bool2int into int_eq";
    ct->type = "int_eq";
    return true;
  } else {
    // Rule 2.
    ct->MarkAsInactive();
    AddVariableSubstition(ct->Arg(1).Var(), ct->Arg(0).Var());
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
// Action: Pick x1 or x2, and replace all occurrences by the other. The prefered
//         direction is replace x2 by x1, unless x2 is already the target
//         variable of another constraint, because a variable cannot be the
//         target of 2 constraints.
// Output: inactive constraint.
//
// Rule 4:
// Input : int_eq(c, x)
// Action: Reduce domain of x to {c}
// Output: inactive constraint.
//
// Rule 5:
// Input : int_eq(c1, c2)
// Output: inactive constraint if c1 == c2, and do nothing if c1 != c2.
// TODO(user): reorder rules?
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
      ct->MarkAsInactive();
      AddVariableSubstition(ct->Arg(0).Var(), ct->Arg(1).Var());
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
// Output: inactive constraint if the removal was successful
//         (domain is not too large to remove a value).
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
// Output: constraint is still active.
bool FzPresolver::PresolveInequalities(FzConstraint* ct) {
  const std::string& id = ct->type;
  if (ct->Arg(0).IsVariable() && ct->Arg(1).HasOneValue()) {
    // Rule 1 where the 'var' is the left operand, eg. var <= 5
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
    // Rule 1 where the 'var' is the right operand, eg 5 <= var
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
// Input : int_xx_reif(arg1, arg2, false) or
//         int_lin_xx_reif(arg1, arg2, c, false)
//         with xx = eq, ne, le, lt, ge, gt
// Output: int_yy(arg1, arg2) or int_lin_yy(arg1, arg2, c)
//         with yy = opposite(xx). i.e. eq -> ne, le -> gt...
void FzPresolver::Unreify(FzConstraint* ct) {
  const int last_argument = ct->arguments.size() - 1;
  DCHECK(HasSuffixString(ct->type, "_reif")) << ct->DebugString();
  ct->type.resize(ct->type.size() - 5);
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
    // Extract the 'operation' suffix of ct->type ("le", "eq", ...); i.e. the
    // last two characters.
    DCHECK_GT(ct->type.size(), 3);
    const std::string op = ct->type.substr(ct->type.size() - 2);
    ct->type.resize(ct->type.size() - 2);
    DCHECK(ct->type == "int_" || ct->type == "bool_" || ct->type == "int_lin_")
        << ct->type;
    // Now, change "op" to the inverse operation. The prefix of ct->type is
    // unchanged.
    if (op == "ne")
      ct->type += "eq";
    else if (op == "eq")
      ct->type += "ne";
    else if (op == "le")
      ct->type += "gt";
    else if (op == "lt")
      ct->type += "ge";
    else if (op == "ge")
      ct->type += "lt";
    else if (op == "gt")
      ct->type += "le";
  }
}

// Propagates the values of set_in
// Input : set_in(x, [c1..c2]) or set_in(x, {c1, .., cn})
// Action: Intersect the domain of x with the set of values.
// Output: inactive constraint.
// note: set_in(x1, {x2, ...}) is plain illegal so we don't bother with it.
bool FzPresolver::PresolveSetIn(FzConstraint* ct) {
  if (ct->Arg(0).IsVariable()) {
    // IntersectDomainWith() will DCHECK that the second argument is a set
    // of constant values.
    FZVLOG << "Propagate " << ct->DebugString() << FZENDL;
    IntersectDomainWith(ct->Arg(1), &ct->Arg(0).Var()->domain);
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
    const int64 safe_value = CapProd(ct->Arg(0).Value(), ct->Arg(1).Value());
    if (value == safe_value) {
      ct->presolve_propagation_done = true;
      if (ct->Arg(2).Var()->domain.Contains(value)) {
        ct->Arg(2).Var()->domain.IntersectWithInterval(value, value);
        ct->MarkAsInactive();
        return true;
      } else {
        FZVLOG << "  - product is not compatible with variable domain, "
               << "ignoring presolve" << FZENDL;
        // TODO(user): Treat failure correctly.
      }
    } else {
      FZVLOG << "  - product overflows, ignoring presolve" << FZENDL;
      // TODO(user): Treat overflow correctly.
    }
  }
  return false;
}

// Propagates bound division.
// Input : int_div(c1, c2, x) (c2 != 0)
// Action: reduce domain of x to {c1 / c2}
// Output: inactive constraint.
bool FzPresolver::PresolveIntDiv(FzConstraint* ct) {
  if (ct->Arg(0).HasOneValue() && ct->Arg(1).HasOneValue() &&
      ct->Arg(2).IsVariable() && !ct->presolve_propagation_done &&
      ct->Arg(1).Value() != 0) {
    FZVLOG << " Propagate " << ct->DebugString() << FZENDL;
    const int64 value = ct->Arg(0).Value() / ct->Arg(1).Value();
    ct->presolve_propagation_done = true;
    if (ct->Arg(2).Var()->domain.Contains(value)) {
      ct->Arg(2).Var()->domain.IntersectWithInterval(value, value);
      ct->MarkAsInactive();
      return true;
    } else {
      FZVLOG << "  - division is not compatible with variable domain, "
             << "ignoring presolve" << FZENDL;
      // TODO(user): Treat failure correctly.
    }
  }
  // TODO(user): Catch c2 = 0 case and set the model to invalid.
  return false;
}

// Simplifies and reduces array_bool_or
//
// Rule 1:
// Input : array_bool_or([b1], b2)
// Output: bool_eq(b1, b2)
//
// Rule 2:
// Input : array_bool_or([b1, .., bn], false) or
//         array_bool_or([b1, .., bn], b0) with b0 assigned to false
// Action: Assign false to b1, .., bn
// Output: inactive constraint.
//
// Rule 3:
// Input : array_bool_or([b1, .., true, .., bn], b0)
// Action: Assign b0 to true
// Output: inactive constraint.
//
// Rule 4:
// Input : array_bool_or([false, .., false], b0), the array can be empty.
// Action: Assign b0 to false
// Output: inactive constraint.
//
// Rule 5:
// Input : array_bool_or([b1, .., false, bn], b0) or
//         array_bool_or([b1, .., bi, .., bn], b0) with bi assigned to false
// Action: Remove variables assigned to false values, or false constants.
// Output: array_bool_or([b1, .., bi-1, bi+1, .., bn], b0)
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
    // TODO(user): Support empty domains correctly, and remove this test.
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
  bool has_bound_true_value = false;
  std::vector<FzIntegerVariable*> unbound;
  for (FzIntegerVariable* const var : ct->Arg(0).variables) {
    if (var->HasOneValue()) {
      has_bound_true_value |= var->Min() == 1;
    } else {
      unbound.push_back(var);
    }
  }
  if (has_bound_true_value) {
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
    // TODO(user): Simplify code once we support empty domains.
  }
  if (unbound.empty()) {
    // Rule 4.
    if (!ct->Arg(1).HasOneValue()) {
      // TODO(user): Simplify code once we support empty domains.
      FZVLOG << "Propagate boolvar to false in " << ct->DebugString() << FZENDL;
      ct->Arg(1).variables[0]->domain.IntersectWithInterval(0, 0);
      ct->MarkAsInactive();
      return true;
    }
    return false;
  }
  if (unbound.size() < ct->Arg(0).variables.size()) {
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
// Input : array_bool_and([b1, .., bn], true)
// Action: Assign b1, .., bn to true
// Output: inactive constraint.
//
// Rule 3:
// Input : array_bool_and([b1, .., false, .., bn], b0)
// Action: Assign b0 to false
// Output: inactive constraint.
//
// Rule 4:
// Input : array_bool_and([true, .., true], b0)
// Action: Assign b0 to true
// Output: inactive constraint.
//
// Rule 5:
// Input : array_bool_and([b1, .., true, bn], b0)
// Action: Remove all the true values.
// Output: array_bool_and([b1, .., bi-1, bi+1, .., bn], b0)
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
    // TODO(user): Simplify the code once we support empty domains.
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
  int has_bound_false_value = 0;
  std::vector<FzIntegerVariable*> unbound;
  for (FzIntegerVariable* const var : ct->Arg(0).variables) {
    if (var->HasOneValue()) {
      has_bound_false_value |= var->Max() == 0;
    } else {
      unbound.push_back(var);
    }
  }
  if (has_bound_false_value) {
    // TODO(user): Simplify the code once we support empty domains.
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
  }
  if (unbound.empty()) {
    // Rule 4.
    if (!ct->Arg(1).HasOneValue()) {
      FZVLOG << "Propagate boolvar to true in " << ct->DebugString() << FZENDL;
      ct->Arg(1).variables[0]->domain.IntersectWithInterval(1, 1);
      ct->MarkAsInactive();
      return true;
    }
    return false;
  }
  if (unbound.size() < ct->Arg(0).variables.size()) {
    FZVLOG << "Reduce array on " << ct->DebugString() << FZENDL;
    ct->MutableArg(0)->variables.swap(unbound);
    return true;
  }
  return false;
}

// Simplifies bool_XX_reif(b1, b2, b3)  (which means b3 = (b1 XX b2)) when the
// middle value is bound.
// Input: bool_XX_reif(b1, t, b2), where XX is "eq" or "ne".
// Output: bool_YY(b1, b2) where YY is "eq" or "not" depending on XX and t.
bool FzPresolver::PresolveBoolEqNeReif(FzConstraint* ct) {
  DCHECK(ct->type == "bool_eq_reif" || ct->type == "bool_ne_reif");
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

// Transform int_lin_gt (which means ScalProd(arg1[], arg2[]) > c) into
// int_lin_ge.
// Input : int_lin_gt(arg1, arg2, c)
// Output: int_lin_ge(arg1, arg2, c + 1)
bool FzPresolver::PresolveIntLinGt(FzConstraint* ct) {
  FZVLOG << "Transform " << ct->DebugString() << " into int_lin_ge" << FZENDL;
  CHECK_EQ(FzArgument::INT_VALUE, ct->Arg(2).type);
  if (ct->Arg(2).Value() != kint64max) {
    ct->MutableArg(2)->values[0]++;
    ct->type = "int_lin_ge";
    return true;
  }
  // TODO(user): fail (the model is impossible: a * b > kint64max can be
  // considered as impossible; because it would imply an overflow; which we
  // reject.
  return false;
}

// Transform int_lin_lt into int_lin_le.
// Input : int_lin_lt(arg1, arg2, c)
// Output: int_lin_le(arg1, arg2, c - 1)
bool FzPresolver::PresolveIntLinLt(FzConstraint* ct) {
  FZVLOG << "Transform " << ct->DebugString() << " into int_lin_le" << FZENDL;
  CHECK_EQ(FzArgument::INT_VALUE, ct->Arg(2).type);
  if (ct->Arg(2).Value() != kint64min) {
    ct->MutableArg(2)->values[0]--;
    ct->type = "int_lin_le";
    return true;
  }
  // TODO(user): fail (the model is impossible: a * b < kint64min can be
  // considered as impossible; because it would imply an overflow; which we
  // reject.
  return false;
}

// Simplifies linear equations of size 1, i.e. c1 * x = c2.
// Input : int_lin_xx([c1], [x], c2) and int_lin_xx_reif([c1], [x], c2, b)
//         with (c1 == 1 or c2 % c1 == 0) and xx = eq, ne, lt, le, gt, ge
// Output: int_xx(x, c2 / c1) and int_xx_reif(x, c2 / c1, b)
bool FzPresolver::SimplifyUnaryLinear(FzConstraint* ct) {
  const int64 coefficient = ct->Arg(0).values[0];
  const int64 rhs = ct->Arg(2).Value();
  if (ct->Arg(0).values.size() == 1 &&
      (coefficient == 1 || (coefficient > 0 && rhs % coefficient == 0))) {
    // TODO(user): Support coefficient = 0.
    // TODO(user): Support coefficient < 0 (and reverse the inequalities).
    // TODO(user): Support rhs % coefficient != 0, and no the correct
    // rounding in the case of inequalities, of false model in the case of
    // equalities.
    FZVLOG << "Remove linear part in " << ct->DebugString() << FZENDL;
    // transform arguments.
    ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
    ct->MutableArg(0)->values.clear();
    ct->MutableArg(0)->variables.push_back(ct->Arg(1).variables[0]);
    ct->MutableArg(1)->type = FzArgument::INT_VALUE;
    ct->MutableArg(1)->variables.clear();
    ct->MutableArg(1)->values.push_back(rhs / coefficient);
    if (ct->arguments.size() == 4) {
      DCHECK(HasSuffixString(ct->type, "_reif"));
      ct->arguments[2] = ct->Arg(3);
    }
    ct->arguments.pop_back();
    // Change type (remove "_lin" part).
    DCHECK(ct->type.size() >= 8 && ct->type.substr(3, 4) == "_lin");
    ct->type.erase(3, 4);
    FZVLOG << "  - " << ct->DebugString() << FZENDL;
    return true;
  }
  return false;
}

// Simplifies linear equations of size 2, i.e. x - y = 0.
// Input : int_lin_xx([1, -1], [x1, x2], 0) and
//         int_lin_xx_reif([1, -1], [x1, x2], 0, b)
//         xx = eq, ne, lt, le, gt, ge
// Output: int_xx(x1, x2) and int_xx_reif(x, x2, b)
bool FzPresolver::SimplifyBinaryLinear(FzConstraint* ct) {
  const int64 rhs = ct->Arg(2).Value();
  if (ct->Arg(0).values.size() != 2 || rhs != 0 ||
      ct->Arg(1).variables.size() == 0) {
    return false;
  }

  FzIntegerVariable* first = nullptr;
  FzIntegerVariable* second = nullptr;
  if (ct->Arg(0).values[0] == 1 && ct->Arg(0).values[1] == -1) {
    first = ct->Arg(1).variables[0];
    second = ct->Arg(1).variables[1];
  } else if (ct->Arg(0).values[0] == -1 && ct->Arg(0).values[1] == 1) {
    first = ct->Arg(1).variables[1];
    second = ct->Arg(1).variables[0];
  } else {
    return false;
  }

  FZVLOG << "Remove linear part in " << ct->DebugString() << FZENDL;
  ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
  ct->MutableArg(0)->values.clear();
  ct->MutableArg(0)->variables.push_back(first);
  ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
  ct->MutableArg(1)->variables.clear();
  ct->MutableArg(1)->variables.push_back(second);
  if (ct->arguments.size() == 4) {
    DCHECK(HasSuffixString(ct->type, "_reif"));
    ct->arguments[2] = ct->Arg(3);
  }
  ct->arguments.pop_back();
  // Change type (remove "_lin" part).
  DCHECK(ct->type.size() >= 8 && ct->type.substr(3, 4) == "_lin");
  ct->type.erase(3, 4);
  FZVLOG << "  - " << ct->DebugString() << FZENDL;
  return true;
}

// Returns false if an overflow occured.
// Used by CheckIntLinReifBounds() below: compute the bounds of the scalar
// product. If an integer overflow occurs the method returns false.
namespace {
bool ComputeLinBounds(const std::vector<int64>& coefficients,
                      const std::vector<FzIntegerVariable*>& variables, int64* lb,
                      int64* ub) {
  CHECK_EQ(coefficients.size(), variables.size()) << "Wrong constraint";
  *lb = 0;
  *ub = 0;
  for (int i = 0; i < coefficients.size(); ++i) {
    const FzIntegerVariable* const var = variables[i];
    const int64 coef = coefficients[i];
    if (coef == 0) continue;
    if (var->Min() == kint64min || var->Max() == kint64max) {
      return false;
    }
    const int64 vmin = var->Min();
    const int64 vmax = var->Max();
    const int64 min_delta =
        coef > 0 ? CapProd(vmin, coef) : CapProd(vmax, coef);
    const int64 max_delta =
        coef > 0 ? CapProd(vmax, coef) : CapProd(vmin, coef);
    *lb = CapAdd(*lb, min_delta);
    *ub = CapAdd(*ub, max_delta);
    if (*lb == kint64min || min_delta == kint64min || min_delta == kint64max ||
        max_delta == kint64min || max_delta == kint64max || *ub == kint64max) {
      // Overflow
      return false;
    }
  }
  return true;
}
}  // namespace

// Presolve: Check bounds of int_lin_eq_reif w.r.t. the boolean variable.
// Input : int_lin_eq_reif([c1, .., cn], [x1, .., xn], c0, b)
// Action: compute min and max of sum(x1 * c2) and
//         assign true to b is min == max == c0, or
//         assign false to b if min > c0 or max < c0,
//         or do nothing and keep the constraint active.
bool FzPresolver::CheckIntLinReifBounds(FzConstraint* ct) {
  DCHECK_EQ(ct->type, "int_lin_eq_reif");
  int64 lb = 0;
  int64 ub = 0;
  if (!ComputeLinBounds(ct->Arg(0).values, ct->Arg(1).variables, &lb, &ub)) {
    FZVLOG << "Overflow found when presolving " << ct->DebugString() << FZENDL;
    return false;
  }
  const int64 value = ct->Arg(2).Value();
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
  return false;
}

// Marks target variable: int_lin_eq
// On two-variable linear equality constraints of the form -x + c0 * y = c1;
// mark x as the "target" of the constraint, i.e. the variable that is "defined"
// by the constraint. We do that only if the constraint doesn't already have a
// target variable and if x doesn't have a defining constraint.
//
// Rule 1:
// Input : int_lin_eq([[-1, c2], x1, x2], c0)
// Output: int_lin_eq([-1, c2], [x1, x2], c0) => x1, mark x1.
//
// Rule 2:
// Input : int_lin_eq([c1, -1], [x1, x2], c0)
// Output: int_lin_eq([c1, -1], [x1, x2], c0) => x2, mark x2.
bool FzPresolver::CreateLinearTarget(FzConstraint* ct) {
  if (ct->target_variable != nullptr) return false;

  for (const int var_index : {0, 1}) {
    if (ct->Arg(0).values.size() == 2 && ct->Arg(0).values[var_index] == -1 &&
        ct->Arg(1).variables[var_index]->defining_constraint == nullptr &&
        !ct->Arg(1).variables[var_index]->HasOneValue()) {
      // Rule 1.
      FZVLOG << "Mark variable " << var_index << " of " << ct->DebugString()
             << " as target" << FZENDL;
      FzIntegerVariable* const var = ct->Arg(1).variables[var_index];
      var->defining_constraint = ct;
      ct->target_variable = var;
      return true;
    }
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

      int64 last_index = ct->Arg(1).values.size();
      last_index = std::min(ct->Arg(0).Var()->Max(), last_index);

      while (last_index >= 1) {
        const int64 value = ct->Arg(1).values[last_index - 1];
        if (value < target_min || value > target_max) {
          last_index--;
        } else {
          break;
        }
      }

      int64 first_index = 1;
      first_index = std::max(ct->Arg(0).Var()->Min(), first_index);
      while (first_index <= last_index) {
        const int64 value = ct->Arg(1).values[first_index - 1];
        if (value < target_min || value > target_max) {
          first_index++;
        } else {
          break;
        }
      }

      if (last_index < ct->Arg(0).Var()->Max() ||
          first_index > ct->Arg(0).Var()->Min()) {
        FZVLOG << "Filter index of " << ct->DebugString() << " to ["
               << first_index << " .. " << last_index << "]" << FZENDL;
        ct->Arg(0).Var()->domain.IntersectWithInterval(first_index, last_index);
        FZVLOG << "  - reduce array to size " << last_index << FZENDL;
        ct->MutableArg(1)->values.resize(last_index);
        return true;
      }
    }
  }
  if (ct->Arg(2).IsVariable() && !ct->presolve_propagation_done) {
    // Rule 2.
    FZVLOG << "Propagate domain on " << ct->DebugString() << FZENDL;
    IntersectDomainWith(ct->Arg(1), &ct->Arg(2).Var()->domain);
    ct->presolve_propagation_done = true;
    return true;
  }
  return false;
}

// Reverses a linear constraint: with negative coefficients.
// Rule 1:
// Input : int_lin_xxx([-c1, .., -cn], [x1, .., xn], c0) or
//         int_lin_xxx_reif([-c1, .., -cn], [x1, .., xn], c0, b) or
//         with c1, cn > 0
// Output: int_lin_yyy([c1, .., cn], [c1, .., cn], c0) or
//         int_lin_yyy_reif([c1, .., cn], [c1, .., cn], c0, b)
//         with yyy is the opposite of xxx (eq -> eq, ne -> ne, le -> ge,
//                                          lt -> gt, ge -> le, gt -> lt)
//
// Rule 2:
// Input: int_lin_xxx[[c1, .., cn], [c'1, .., c'n], c0]  (no variables)
// Output: inactive or false constraint.
bool FzPresolver::PresolveLinear(FzConstraint* ct) {
  if (ct->Arg(0).values.empty()) {
    return false;
  }
  // Rule 2.
  if (ct->Arg(1).variables.empty()) {
    FZVLOG << "Rewrite constant linear equation " << ct->DebugString()
           << FZENDL;
    CHECK(!ct->Arg(1).values.empty());
    int64 scalprod = 0;
    for (int i = 0; i < ct->Arg(0).values.size(); ++i) {
      scalprod += ct->Arg(0).values[i] * ct->Arg(1).values[i];
    }
    const int64 rhs = ct->Arg(2).Value();
    if (ct->type == "int_lin_eq") {
      if (scalprod == rhs) {
        ct->MarkAsInactive();
      } else {
        ct->type = "false_constraint";
      }
    } else if (ct->type == "int_lin_eq_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(FzArgument::IntegerValue(scalprod == rhs));
    } else if (ct->type == "int_lin_ge") {
      if (scalprod >= rhs) {
        ct->MarkAsInactive();
      } else {
        ct->type = "false_constraint";
      }
    } else if (ct->type == "int_lin_ge_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(FzArgument::IntegerValue(scalprod >= rhs));
    } else if (ct->type == "int_lin_le") {
      if (scalprod <= rhs) {
        ct->MarkAsInactive();
      } else {
        ct->type = "false_constraint";
      }
    } else if (ct->type == "int_lin_le_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(FzArgument::IntegerValue(scalprod <= rhs));
    } else if (ct->type == "int_lin_ne") {
      if (scalprod != rhs) {
        ct->MarkAsInactive();
      } else {
        ct->type = "false_constraint";
      }
    } else if (ct->type == "int_lin_ne_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(FzArgument::IntegerValue(scalprod != rhs));
    }
    FZVLOG << "  - into " << ct->DebugString() << FZENDL;
    return true;
  }

  // Rule 1.
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
  if (ct->type == "int_lin_le") {
    ct->type = "int_lin_ge";
  } else if (ct->type == "int_lin_lt") {
    ct->type = "int_lin_gt";
  } else if (ct->type == "int_lin_le") {
    ct->type = "int_lin_ge";
  } else if (ct->type == "int_lin_lt") {
    ct->type = "int_lin_gt";
  } else if (ct->type == "int_lin_le_reif") {
    ct->type = "int_lin_ge_reif";
  } else if (ct->type == "int_lin_ge_reif") {
    ct->type = "int_lin_le_reif";
  }
  return true;
}

// Regroup linear term with the same variable.
// Input : int_lin_xxx([c1, .., cn], [x1, .., xn], c0) with xi = xj
// Output: int_lin_xxx([c1, .., ci + cj, .., cn], [x1, .., xi, .., xn], c0)
bool FzPresolver::RegroupLinear(FzConstraint* ct) {
  if (ct->Arg(1).variables.empty()) {
    // Only constants, or size == 0.
    return false;
  }
  hash_map<const FzIntegerVariable*, int64> coefficients;
  const int original_size = ct->Arg(0).values.size();
  for (int i = 0; i < original_size; ++i) {
    coefficients[ct->Arg(1).variables[i]] += ct->Arg(0).values[i];
  }
  if (coefficients.size() != original_size) {  // Duplicate variables.
    FZVLOG << "Regroup variables in " << ct->DebugString() << FZENDL;
    hash_set<const FzIntegerVariable*> processed;
    int index = 0;
    for (int i = 0; i < original_size; ++i) {
      FzIntegerVariable* fz_var = ct->Arg(1).variables[i];
      if (!ContainsKey(processed, fz_var)) {
        processed.insert(fz_var);
        ct->MutableArg(1)->variables[index] = fz_var;
        ct->MutableArg(0)->values[index] = coefficients[fz_var];
        index++;
      }
    }
    CHECK_EQ(index, coefficients.size());
    ct->MutableArg(0)->values.resize(index);
    ct->MutableArg(1)->variables.resize(index);
    return true;
  }
  return false;
}

// Bound propagation: int_lin_eq, int_lin_le, int_lin_ge
//
// Rule 1:
// Input : int_lin_xx([c1, .., cn], [x1, .., xn],  c0) with ci >= 0 and
//         xi are variables with positive domain.
// Action: if xx = eq or le, intersect the domain of xi with [0, c0 / ci]
//
// Rule 2:
// Input : int_lin_xx([c1], [x1], c0) with c1 >= 0, and xx = eq, ge.
// Action: intersect the domain of x1 with [c0/c1, kint64max]
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
    // Rule 1.
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
    // Rule 2.
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
// This mapping can involve 1 or 2 variables dependening if y or z in A[y][z]
// is a constant in the model).
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
  if (ct->Arg(0).values.size() == 2 &&
      ct->Arg(1).variables[1] == ct->target_variable &&
      ct->Arg(0).values[1] == -1 &&
      !ContainsKey(affine_map_, ct->target_variable)) {
    affine_map_[ct->target_variable] = AffineMapping(
        ct->Arg(1).variables[0], ct->Arg(0).values[0], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
    return true;
  }
  if (ct->Arg(0).values.size() == 3 &&
      ct->Arg(1).variables[0] == ct->target_variable &&
      ct->Arg(0).values[0] == -1 && ct->Arg(0).values[2] == 1 &&
      !ContainsKey(array2d_index_map_, ct->target_variable) &&
      ct->strong_propagation) {
    array2d_index_map_[ct->target_variable] =
        Array2DIndexMapping(ct->Arg(1).variables[1], ct->Arg(0).values[1],
                            ct->Arg(1).variables[2], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
    return true;
  }
  if (ct->Arg(0).values.size() == 3 &&
             ct->Arg(1).variables[0] == ct->target_variable &&
             ct->Arg(0).values[0] == -1 && ct->Arg(0).values[1] == 1 &&
             !ContainsKey(array2d_index_map_, ct->target_variable) &&
             ct->strong_propagation) {
    array2d_index_map_[ct->target_variable] =
        Array2DIndexMapping(ct->Arg(1).variables[2], ct->Arg(0).values[2],
                            ct->Arg(1).variables[1], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
    return true;
  }
  if (ct->Arg(0).values.size() == 3 &&
      ct->Arg(1).variables[2] == ct->target_variable &&
      ct->Arg(0).values[2] == -1 && ct->Arg(0).values[1] == 1 &&
      !ContainsKey(array2d_index_map_, ct->target_variable)) {
    array2d_index_map_[ct->target_variable] =
        Array2DIndexMapping(ct->Arg(1).variables[0], ct->Arg(0).values[0],
                            ct->Arg(1).variables[1], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
    return true;
  }
  if (ct->Arg(0).values.size() == 3 &&
             ct->Arg(1).variables[2] == ct->target_variable &&
             ct->Arg(0).values[2] == -1 && ct->Arg(0).values[0] == 1 &&
             !ContainsKey(array2d_index_map_, ct->target_variable)) {
    array2d_index_map_[ct->target_variable] =
        Array2DIndexMapping(ct->Arg(1).variables[1], ct->Arg(0).values[1],
                            ct->Arg(1).variables[0], -ct->Arg(2).Value(), ct);
    FZVLOG << "Store affine mapping info for " << ct->DebugString() << FZENDL;
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
//
// Rule1:
// Input : array_int_element(x0, [c1, .., cn], y) with x0 = a * x + b
// Output: array_int_element(x, [c_a1, .., c_am], b) with a * i = b = ai
//
// Rule 2:
// Input : array_int_element(x, [c1, .., cn], y) with x = a * x1 + x2 + b
// Output: array_int_element([x1, x2], [c_a1, .., c_am], b, [a, b])
//         to be interpreted by the extraction process.
// Rule3:
// Input : array_int_element(x, [c1, .., cn], y) with x fixed one value.
// Output: int_eq(b, c_x.Value())
//
// Rule 4:
// Input : array_int_element(x, [c1, .., cn], y) with x0 ci = c0 + i
// Output: int_lin_eq([-1, 1], [y, x], 1 - c)  (e.g. y = x + c - 1)
bool FzPresolver::PresolveSimplifyElement(FzConstraint* ct) {
  if (ct->Arg(0).variables.size() > 1) {
    return false;
  }
  FzIntegerVariable* const index_var = ct->Arg(0).Var();
  if (ContainsKey(affine_map_, index_var)) {
    // Rule 1.
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
    } else  if (mapping.offset + mapping.coefficient > 0 &&
                domain.values[0] > 0) {
      const std::vector<int64>& values = ct->Arg(1).values;
      std::vector<int64> new_values;
      for (int64 i = 1; i <= domain.values.back(); ++i) {
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
  if (ContainsKey(array2d_index_map_, index_var)) {
    FZVLOG << "Rewrite " << ct->DebugString() << " as a 2d element" << FZENDL;
    const Array2DIndexMapping& mapping = array2d_index_map_[index_var];
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
  if (index_var->HasOneValue()) {
    // Rule 3.
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
    // Rule 4.
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
//
// Rule1:
// Input : array_var_int_element(x0, [x1, .., xn], y) with xi(1..n) having one
//         value
// Output: array_int_element(x0, [x1.Value(), .., xn.Value()], y)
//
// Rule2:
// Input : array_var_int_element(x0, [x1, .., xn], y) with x0 = a * x + b
// Output: array_var_int_element(x, [x_a1, .., x_an], b) with a * i = b = ai
bool FzPresolver::PresolveSimplifyExprElement(FzConstraint* ct) {
  bool all_integers = true;
  for (FzIntegerVariable* const var : ct->Arg(1).variables) {
    if (!var->HasOneValue()) {
      all_integers = false;
      break;
    }
  }
  if (all_integers) {
    // Rule 1:
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
  if (index_var->HasOneValue()) {
    // Rule 2.
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
    if ((ct->Arg(2).HasOneValue() &&
	 ct->Arg(2).Value() == static_cast<int64>(value)) ||
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
        if (var->HasOneValue()) {
          state = 1;
        }
      } else {
        state = 0;
      }
    } else if (id == "int_ne_reif" || id == "bool_ne_reif") {
      if (var->domain.Contains(value)) {
        if (var->HasOneValue()) {
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

bool FzPresolver::StoreIntEqReif(FzConstraint* ct) {
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF &&
      ct->Arg(1).type == FzArgument::INT_VAR_REF &&
      ct->Arg(2).type == FzArgument::INT_VAR_REF) {
    FzIntegerVariable* const first = ct->Arg(0).Var();
    FzIntegerVariable* const second = ct->Arg(1).Var();
    FzIntegerVariable* const boolvar = ct->Arg(2).Var();
    if (ContainsKey(int_eq_reif_map_, first) &&
        ContainsKey(int_eq_reif_map_[first], second)) {
      return false;
    }
    FZVLOG << "Store eq_var info for " << ct->DebugString() << FZENDL;
    int_eq_reif_map_[first][second] = boolvar;
    int_eq_reif_map_[second][first] = boolvar;
    return true;
  }
  return false;
}

bool FzPresolver::SimplifyIntNeReif(FzConstraint* ct) {
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF &&
      ct->Arg(1).type == FzArgument::INT_VAR_REF &&
      ct->Arg(2).type == FzArgument::INT_VAR_REF &&
      ContainsKey(int_eq_reif_map_, ct->Arg(0).Var()) &&
      ContainsKey(int_eq_reif_map_[ct->Arg(0).Var()], ct->Arg(1).Var())) {
    FZVLOG << "Merge " << ct->DebugString() << " with opposite constraint"
           << FZENDL;
    FzIntegerVariable* const opposite =
        int_eq_reif_map_[ct->Arg(0).Var()][ct->Arg(1).Var()];
    ct->MutableArg(0)->variables[0] = opposite;
    ct->MutableArg(1)->variables[0] = ct->Arg(2).Var();
    ct->arguments.pop_back();
    ct->type = "bool_not";
    FZVLOG << "  -> " << ct->DebugString() << FZENDL;
    return true;
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

// Propagate bool_xor
// Rule 1:
// Input : bool_xor(t, b1, b2)
// Action: bool_not(b1, b2) if t = true, bool_eq(b1, b2) if t = false.
//
// Rule 2:
// Input : bool_xor(b1, t, b2)
// Action: bool_not(b1, b2) if t = true, bool_eq(b1, b2) if t = false.
//
// Rule 3:
// Input : bool_xor(b1, b2, t)
// Action: bool_not(b1, b2) if t = true, bool_eq(b1, b2) if t = false.
bool FzPresolver::PresolveBoolXor(FzConstraint* ct) {
  if (ct->Arg(0).HasOneValue()) {
    // Rule 1.
    const int64 value = ct->Arg(0).Value();
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    ct->arguments[0] = ct->arguments[1];
    ct->arguments[1] = ct->arguments[2];
    ct->arguments.pop_back();
    ct->type = value == 1 ? "bool_not" : "bool_eq";
    FZVLOG << "   -> " << ct->DebugString() << FZENDL;
    return true;
  }
  if (ct->Arg(1).HasOneValue()) {
    // Rule 2.
    const int64 value = ct->Arg(1).Value();
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    ct->arguments[1] = ct->arguments[2];
    ct->arguments.pop_back();
    ct->type = value == 1 ? "bool_not" : "bool_eq";
    FZVLOG << "   -> " << ct->DebugString() << FZENDL;
    return true;
  }
  if (ct->Arg(2).HasOneValue()) {
    // Rule 3.
    const int64 value = ct->Arg(2).Value();
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    ct->arguments.pop_back();
    ct->type = value == 1 ? "bool_not" : "bool_eq";
    FZVLOG << "   -> " << ct->DebugString() << FZENDL;
    return true;
  }
  return false;
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
             ct->Arg(0).Var()->defining_constraint == nullptr &&
             !ct->Arg(0).Var()->HasOneValue()) {
    FZVLOG << "Insert target variable in " << ct->DebugString() << FZENDL;
    FzIntegerVariable* const var = ct->Arg(0).Var();
    ct->target_variable = var;
    var->defining_constraint = ct;
    return true;
  } else if (ct->target_variable == nullptr &&
             ct->Arg(1).Var()->defining_constraint == nullptr &&
             !ct->Arg(1).Var()->HasOneValue()) {
    FZVLOG << "Insert target variable in " << ct->DebugString() << FZENDL;
    FzIntegerVariable* const var = ct->Arg(1).Var();
    ct->target_variable = var;
    var->defining_constraint = ct;
    return true;
  }
  return false;
}

// Simplify bool_clause
//
// Rule 1:
// Input: bool_clause([b1][b2])
// Output: bool_le(b2, b1)
//
// Rule 2:
// Input: bool_clause([t][b])
// Output: Mark constraint as inactive if t is true.
//         bool_eq(b, false) if t is false.
//
// Rule 3:
// Input: bool_clause([b1, .., bn][t])
// Output: Mark constraint as inactive if t is false.
//         array_array_or([b1, .. ,bn]) if t is true.
bool FzPresolver::PresolveBoolClause(FzConstraint* ct) {
  // Rule 1.
  if (ct->Arg(0).variables.size() == 1 && ct->Arg(1).variables.size() == 1) {
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    std::swap(ct->MutableArg(0)->variables[0], ct->MutableArg(1)->variables[0]);
    ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
    ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
    ct->type = "bool_le";
    FZVLOG << "  to " << ct->DebugString() << FZENDL;
    return true;
  }
  // Rule 2.
  if (ct->Arg(0).variables.size() == 0 && ct->Arg(0).values.size() == 1 &&
      ct->Arg(1).variables.size() == 1) {
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    const int64 value = ct->Arg(0).values.front();
    if (value) {
      ct->MarkAsInactive();
      return true;
    } else {
      ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
      ct->MutableArg(0)->variables = ct->Arg(1).variables;
      ct->MutableArg(0)->values.clear();
      ct->MutableArg(1)->type = FzArgument::INT_VALUE;
      ct->MutableArg(1)->variables.clear();
      ct->MutableArg(1)->values.push_back(0);
      ct->type = "bool_eq";
      FZVLOG << "  to " << ct->DebugString() << FZENDL;
      return true;
    }
  }
  // Rule 3.
  if (ct->Arg(1).variables.size() == 0 && ct->Arg(1).values.size() == 1) {
    FZVLOG << "Simplify " << ct->DebugString() << FZENDL;
    const int64 value = ct->Arg(1).values.front();
    if (value) {
      if (ct->Arg(0).variables.size() > 1) {
        ct->type = "array_bool_or";
        FZVLOG << "  to " << ct->DebugString() << FZENDL;
        return true;
      } else if (ct->Arg(0).variables.size() == 1) {
        ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(1)->type = FzArgument::INT_VALUE;
        ct->type = "bool_eq";
        FZVLOG << "  to " << ct->DebugString() << FZENDL;
        return true;
      }
    } else {
      ct->MarkAsInactive();
      return true;
    }
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
    if (Has01Values(ct->Arg(1).variables[0]) &&
        Has01Values(ct->Arg(1).variables[1])) {
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
    if (Has01Values(right) && left->HasOneValue() && Is0Or1(left->Min())) {
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
    } else if (Has01Values(left) && right->HasOneValue() &&
               Is0Or1(right->Min())) {
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
  if (id == "int_eq_reif") {
    changed |= StoreIntEqReif(ct);
  }
  if (id == "int_ne_reif") {
    changed |= SimplifyIntNeReif(ct);
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
  if (id == "bool_xor") {
    changed |= PresolveBoolXor(ct);
  }
  if (id == "bool_not") {
    changed |= PresolveBoolNot(ct);
  }
  if (id == "bool_clause") {
    changed |= PresolveBoolClause(ct);
  }
  if (id == "int_div") changed |= PresolveIntDiv(ct);
  if (id == "int_times") changed |= PresolveIntTimes(ct);
  if (id == "int_lin_gt") changed |= PresolveIntLinGt(ct);
  if (id == "int_lin_lt") changed |= PresolveIntLinLt(ct);
  if (HasPrefixString(id, "int_lin_")) {
    changed |= PresolveLinear(ct);
  }
  // type can have changed after the presolve.
  if (HasPrefixString(id, "int_lin_")) {
    changed |= RegroupLinear(ct);
    changed |= SimplifyUnaryLinear(ct);
    changed |= SimplifyBinaryLinear(ct);
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
  // Last rule: if the target variable of a constraint is fixed, removed it
  // the target part.
  if (ct->target_variable != nullptr && ct->target_variable->HasOneValue()) {
    FZVLOG << "Remove target variable from " << ct->DebugString()
           << " as it is fixed to a single value" << FZENDL;
    ct->target_variable->defining_constraint = nullptr;
    ct->target_variable = nullptr;
    changed = true;
  }
  return changed;
}

// Stores all pairs of variables appearing in an int_ne(x, y) constraint.
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

void FzPresolver::MergeIntEqNe(FzModel* model) {
  hash_map<const FzIntegerVariable*, hash_map<int64, FzIntegerVariable*>>
      int_eq_reif_map;
  hash_map<const FzIntegerVariable*, hash_map<int64, FzIntegerVariable*>>
      int_ne_reif_map;
  for (FzConstraint* const ct : model->constraints()) {
    if (!ct->active) continue;
    if (ct->type == "int_eq_reif" && ct->Arg(2).values.empty()) {
      FzIntegerVariable* var = nullptr;
      int64 value = 0;
      if (ct->Arg(0).values.empty() && ct->Arg(1).variables.empty()) {
        var = ct->Arg(0).Var();
        value = ct->Arg(1).Value();
      } else if (ct->Arg(1).values.empty() && ct->Arg(0).variables.empty()) {
        var = ct->Arg(1).Var();
        value = ct->Arg(0).Value();
      }
      if (var != nullptr) {
        FzIntegerVariable* boolvar = ct->Arg(2).Var();
        FzIntegerVariable* stored = FindPtrOrNull(int_eq_reif_map[var], value);
        if (stored == nullptr) {
          int_eq_reif_map[var][value] = boolvar;
        } else {
          FZVLOG << "Merge " << ct->DebugString() << FZENDL;
          ct->type = "bool_eq";
          ct->arguments.clear();
          ct->arguments.push_back(FzArgument::IntVarRef(stored));
          ct->arguments.push_back(FzArgument::IntVarRef(boolvar));
          FZVLOG << "  -> " << ct->DebugString() << FZENDL;
        }
      }
    }

    if (ct->type == "int_ne_reif" && ct->Arg(2).values.empty()) {
      FzIntegerVariable* var = nullptr;
      int64 value = 0;
      if (ct->Arg(0).values.empty() && ct->Arg(1).variables.empty()) {
        var = ct->Arg(0).Var();
        value = ct->Arg(1).Value();
      } else if (ct->Arg(1).values.empty() && ct->Arg(0).variables.empty()) {
        var = ct->Arg(1).Var();
        value = ct->Arg(0).Value();
      }
      if (var != nullptr) {
        FzIntegerVariable* boolvar = ct->Arg(2).Var();
        FzIntegerVariable* stored = FindPtrOrNull(int_ne_reif_map[var], value);
        if (stored == nullptr) {
          int_ne_reif_map[var][value] = boolvar;
        } else {
          FZVLOG << "Merge " << ct->DebugString() << FZENDL;
          ct->type = "bool_eq";
          ct->arguments.clear();
          ct->arguments.push_back(FzArgument::IntVarRef(stored));
          ct->arguments.push_back(FzArgument::IntVarRef(boolvar));
          FZVLOG << "  -> " << ct->DebugString() << FZENDL;
        }
      }
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
  // Rebuild var_constraint map if empty.
  if (var_to_constraints_.empty()) {
    for (FzConstraint* const ct : model->constraints()) {
      for (const FzArgument& arg : ct->arguments) {
        for (FzIntegerVariable* const var : arg.variables) {
          var_to_constraints_[var].insert(ct);
        }
      }
    }
  }

  FirstPassModelScan(model);

  MergeIntEqNe(model);

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

void FzPresolver::AddVariableSubstition(FzIntegerVariable* from,
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
  // Collected impacted constraints.
  hash_set<FzConstraint*> impacted;
  for (const auto& p : var_representative_map_) {
    const hash_set<FzConstraint*>& contains = var_to_constraints_[p.first];
    impacted.insert(contains.begin(), contains.end());
  }
  // Rewrite the constraints.
  for (FzConstraint* const ct : impacted) {
    if (ct != nullptr && ct->active) {
      for (int i = 0; i < ct->arguments.size(); ++i) {
        FzArgument* argument = &ct->arguments[i];
        switch (argument->type) {
          case FzArgument::INT_VAR_REF:
          case FzArgument::INT_VAR_REF_ARRAY: {
            for (int i = 0; i < argument->variables.size(); ++i) {
              FzIntegerVariable* const old_var = argument->variables[i];
              FzIntegerVariable* const new_var =
                  FindRepresentativeOfVar(old_var);
              if (new_var != old_var) {
                argument->variables[i] = new_var;
                var_to_constraints_[new_var].insert(ct);
              }
            }
            break;
          }
          default: {}
        }
      }
      // No need to update var_to_constraints, it should have been done already
      // in the arguments of the constraints.
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

void FzPresolver::IntersectDomainWith(const FzArgument& arg, FzDomain* domain) {
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

// Weight:
//  - *_reif: arity
//  - otherwise arity + 100.
int SortWeight(FzConstraint* ct) {
  int arity = HasSuffixString(ct->type, "_reif") ? 0 : 100;
  for (const FzArgument& arg : ct->arguments) {
    arity += arg.variables.size();
  }
  return arity;
}

void CleanUpVariableWithMultipleDefiningConstraints(FzModel* model) {
  hash_map<FzIntegerVariable*, std::vector<FzConstraint*>> ct_var_map;
  for (FzConstraint* const ct : model->constraints()) {
    if (ct->target_variable != nullptr) {
      ct_var_map[ct->target_variable].push_back(ct);
    }
  }

  for (auto& ct_list : ct_var_map) {
    if (ct_list.second.size() > 1) {
      // Sort by number of variables in the constraint. Prefer smaller ones.
      std::sort(ct_list.second.begin(), ct_list.second.end(),
                [](FzConstraint* c1, FzConstraint* c2) {
                  return SortWeight(c1) < SortWeight(c2);
                });
      // Keep the first constraint as the defining one.
      for (int pos = 1; pos < ct_list.second.size(); ++pos) {
        FzConstraint* const ct = ct_list.second[pos];
        FZVLOG << "Remove duplicate target from " << ct->DebugString()
               << FZENDL;
        ct_list.first->defining_constraint = ct;
        ct_list.second[pos]->RemoveTargetVariable();
      }
      // Reset the defining constraint.
      ct_list.first->defining_constraint = ct_list.second[0];
    }
  }
}

bool AreOnesFollowedByMinusOne(const std::vector<int64>& coeffs) {
  for (int i = 0; i < coeffs.size() - 1; ++i) {
    if (coeffs[i] != 1) {
      return false;
    }
  }
  return coeffs.back() == -1;
}

template <class T>
bool IsStrictPrefix(const std::vector<T>& v1, const std::vector<T>& v2) {
  if (v1.size() >= v2.size()) {
    return false;
  }
  for (int i = 0; i < v1.size(); ++i) {
    if (v1[i] != v2[i]) {
      return false;
    }
  }
  return true;
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
    if (id == "int_lin_eq" && ct->target_variable != nullptr) {
      FzIntegerVariable* const var = ct->target_variable;
      for (int i = 0; i < ct->Arg(0).values.size(); ++i) {
        if (ct->Arg(1).variables[i] == var) {
          if (ct->Arg(0).values[i] == -1) {
            break;
          } else if (ct->Arg(0).values[i] == 1) {
            FZVLOG << "Reverse " << ct->DebugString() << FZENDL;
            ct->MutableArg(2)->values[0] *= -1;
            for (int j = 0; j < ct->Arg(0).values.size(); ++j) {
              ct->MutableArg(0)->values[j] *= -1;
            }
            break;
          }
        }
      }
    }
    if (id == "array_var_int_element") {
      if (ct->target_variable != nullptr) {
        hash_set<FzIntegerVariable*> variables_in_array;
        for (FzIntegerVariable* const var : ct->Arg(1).variables) {
          variables_in_array.insert(var);
        }
        if (ContainsKey(variables_in_array, ct->target_variable)) {
          FZVLOG << "Remove target variable from " << ct->DebugString()
                 << " as it appears in the array of variables" << FZENDL;
          ct->RemoveTargetVariable();
        }
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
    // Remove target variables from element constraint.
    if ((id == "array_int_element" &&
         (!IsArrayBoolean(ct->Arg(1).values) ||
          !OnlyOne0OrOnlyOne1(ct->Arg(1).values))) ||
        id == "array_var_int_element") {
      ct->RemoveTargetVariable();
    }
  }

  // Clean up variables with multiple defining constraints.
  CleanUpVariableWithMultipleDefiningConstraints(model);

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
  var_to_constraints_.clear();
  for (FzConstraint* const ct : model->constraints()) {
    for (const FzArgument& arg : ct->arguments) {
      for (FzIntegerVariable* const var : arg.variables) {
        var_to_constraints_[var].insert(ct);
      }
    }
  }

  // First version. The start is recognized by the double var in the max.
  //   tmp1 = std::max(v1, v1)
  for (FzConstraint* const ct : model->constraints()) {
    if (start == nullptr) {
      CheckRegroupStart(ct, &start, &chain, &carry_over);
    } else if (ct->type == start->type &&
               ct->Arg(1).Var() == carry_over.back() &&
               var_to_constraints_[ct->Arg(0).Var()].size() <= 2) {
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

  // Regroup increasing sequence of int_lin_eq([1,..,1,-1], [x1, ..., xn, yn])
  // into sequence of int_plus(x1, x2, y2), int_plus(y2, x3, y3)...
  std::vector<FzIntegerVariable*> current_variables;
  FzIntegerVariable* target_variable = nullptr;
  FzConstraint* first_constraint = nullptr;
  for (FzConstraint* const ct : model->constraints()) {
    if (target_variable == nullptr) {
      if (ct->type == "int_lin_eq" && ct->Arg(0).values.size() == 3 &&
          AreOnesFollowedByMinusOne(ct->Arg(0).values) &&
          ct->Arg(1).values.empty() && ct->Arg(2).Value() == 0) {
        FZVLOG << "Recognize assignment " << ct->DebugString() << FZENDL;
        current_variables = ct->Arg(1).variables;
        target_variable = current_variables.back();
        current_variables.pop_back();
        first_constraint = ct;
      }
    } else {
      if (ct->type == "int_lin_eq" &&
          AreOnesFollowedByMinusOne(ct->Arg(0).values) &&
          ct->Arg(0).values.size() == current_variables.size() + 2 &&
          IsStrictPrefix(current_variables, ct->Arg(1).variables)) {
        FZVLOG << "Recognize hidden int_plus " << ct->DebugString() << FZENDL;
        current_variables = ct->Arg(1).variables;
        // Rewrite ct into int_plus.
        ct->type = "int_plus";
        ct->MutableArg(0)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(0)->values.clear();
        ct->MutableArg(0)->variables.push_back(target_variable);
        ct->MutableArg(1)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(1)->variables.clear();
        ct->MutableArg(1)->variables.push_back(
            current_variables[current_variables.size() - 2]);
        ct->MutableArg(2)->type = FzArgument::INT_VAR_REF;
        ct->MutableArg(2)->values.clear();
        ct->MutableArg(2)->variables.push_back(current_variables.back());
        target_variable = current_variables.back();
        current_variables.pop_back();
        // We remove the target variable to force the variable to be created
        // To break the linear sweep during propagation.
        ct->RemoveTargetVariable();
        FZVLOG << "  -> " << ct->DebugString() << FZENDL;
        // We clean the first constraint too.
        if (first_constraint != nullptr) {
          first_constraint->RemoveTargetVariable();
          first_constraint = nullptr;
        }
      } else {
        current_variables.clear();
        target_variable = nullptr;
      }
    }
  }
}
}  // namespace operations_research
