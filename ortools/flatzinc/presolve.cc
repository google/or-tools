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

#include "ortools/flatzinc/presolve.h"

#include <map>
#include <set>
#include <unordered_set>

#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/base/strutil.h"
#include "ortools/base/string_view.h"
#include "ortools/base/map_util.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/graph/cliques.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/vector_map.h"

DEFINE_bool(fz_floats_are_ints, true,
            "Interpret floats as integers in all variables and constraints.");

namespace operations_research {
namespace fz {
namespace {
// TODO(user): accept variables fixed to 0 or 1.
bool Has01Values(IntegerVariable* var) {
  return var->domain.Min() == 0 && var->domain.Max() == 1;
}

bool Is0Or1(int64 value) { return !(value & ~1LL); }

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
bool AtMostOne0OrAtMostOne1(const std::vector<T>& values) {
  CHECK(IsArrayBoolean(values));
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

std::unordered_set<int64> GetValueSet(const Argument& arg) {
  std::unordered_set<int64> result;
  if (arg.HasOneValue()) {
    result.insert(arg.Value());
  } else {
    const Domain& domain = arg.Var()->domain;
    if (domain.is_interval && !domain.values.empty()) {
      for (int64 v = domain.values[0]; v <= domain.values[1]; ++v) {
        result.insert(v);
      }
    } else {
      result.insert(domain.values.begin(), domain.values.end());
    }
  }
  return result;
}

void SetConstraintAsIntEq(Constraint* ct, IntegerVariable* var, int64 value) {
  CHECK(var != nullptr);
  ct->type = "int_eq";
  ct->arguments.clear();
  ct->arguments.push_back(Argument::IntVarRef(var));
  ct->arguments.push_back(Argument::IntegerValue(value));
}

bool OverlapsAt(const Argument& array, int pos, const Argument& other) {
  if (array.type == Argument::INT_VAR_REF_ARRAY) {
    const Domain& domain = array.variables[pos]->domain;
    if (domain.IsAllInt64()) {
      return true;
    }
    switch (other.type) {
      case Argument::INT_VALUE: {
        return domain.Contains(other.Value());
      }
      case Argument::INT_INTERVAL: {
        return domain.OverlapsIntInterval(other.values[0], other.values[1]);
      }
      case Argument::INT_LIST: {
        return domain.OverlapsIntList(other.values);
      }
      case Argument::INT_VAR_REF: {
        return domain.OverlapsDomain(other.variables[0]->domain);
      }
      default: {
        LOG(FATAL) << "Case not supported in OverlapsAt";
        return false;
      }
    }
  } else if (array.type == Argument::INT_LIST) {
    const int64 value = array.values[pos];
    switch (other.type) {
      case Argument::INT_VALUE: {
        return value == other.values[0];
      }
      case Argument::INT_INTERVAL: {
        return other.values[0] <= value && value <= other.values[1];
      }
      case Argument::INT_LIST: {
        return std::find(other.values.begin(), other.values.end(), value) !=
               other.values.end();
      }
      case Argument::INT_VAR_REF: {
        return other.variables[0]->domain.Contains(value);
      }
      default: {
        LOG(FATAL) << "Case not supported in OverlapsAt";
        return false;
      }
    }
  } else {
    LOG(FATAL) << "First argument not supported in OverlapsAt";
    return false;
  }
}
}  // namespace

// For the author's reference, here is an indicative list of presolve rules
// that should eventually be implemented.
//
// Presolve rule:
//   - table_int -> intersect variables domains with tuple set.
//
// TODO(user):
//   - store dependency graph of constraints -> variable to speed up presolve.
//   - use the same dependency graph to speed up variable substitution.
//   - add more check when presolving out a variable or a constraint.

// ----- Rule helpers -----

void Presolver::ApplyRule(
    Constraint* ct, const std::string& rule_name,
    const std::function<Presolver::RuleStatus(Constraint* ct, std::string*)>& rule) {
  const std::string before = HASVLOG ? ct->DebugString() : "";
  std::string log;

  const RuleStatus status = rule(ct, &log);
  if (status != NOT_CHANGED) {
    successful_rules_[rule_name]++;
    if (HASVLOG) {
      FZVLOG << "Apply rule " << rule_name << " on " << before << FZENDL;
      if (!log.empty()) {
        FZVLOG << "  - log: " << log << FZENDL;
      }
    }
  }

  switch (status) {
    case NOT_CHANGED: {
      break;
    }
    case CONTEXT_CHANGED: {
      break;
    }
    case CONSTRAINT_REWRITTEN: {
      AddConstraintToMapping(ct);
      changed_constraints_.insert(ct);
      if (HASVLOG) {
        const std::string after = ct->DebugString();
        if (after != before) {
          FZVLOG << "  - constraint is modified to " << after << FZENDL;
        }
      }
      break;
    }
    case CONSTRAINT_ALWAYS_FALSE: {
      FZVLOG << "  - constraint is set to false";
      RemoveConstraintFromMapping(ct);
      ct->SetAsFalse();
      break;
    }
    case CONSTRAINT_ALWAYS_TRUE: {
      FZVLOG << "  - constraint is set to true";
      RemoveConstraintFromMapping(ct);
      ct->MarkAsInactive();
      break;
    }
  }
}

void Presolver::MarkChangedVariable(IntegerVariable* var) {
  changed_variables_.insert(var);
}

void Presolver::AddConstraintToMapping(Constraint* ct) {
  for (const Argument& arg : ct->arguments) {
    for (IntegerVariable* const var : arg.variables) {
      var_to_constraints_[var].insert(ct);
    }
  }
}

void Presolver::RemoveConstraintFromMapping(Constraint* ct) {
  for (const Argument& arg : ct->arguments) {
    for (IntegerVariable* const var : arg.variables) {
      var_to_constraints_[var].erase(ct);
    }
  }
}

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
Presolver::RuleStatus Presolver::PresolveBool2Int(Constraint* ct, std::string* log) {
  DCHECK_EQ(ct->type, "bool2int");
  if (ct->arguments[0].HasOneValue() || ct->arguments[1].HasOneValue()) {
    // Rule 1.
    log->append(
        "simplifying bool2int with one variable assigned to a single value");
    ct->type = "int_eq";
    return CONSTRAINT_REWRITTEN;
  } else {
    // Rule 2.
    AddVariableSubstition(ct->arguments[1].Var(), ct->arguments[0].Var());
    return CONSTRAINT_ALWAYS_TRUE;
  }
  return NOT_CHANGED;
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
// Action: Pick x1 or x2, and replace all occurrences by the other. The
//         preferred direction is replace x2 by x1, unless x2 is already the
//         target variable of another constraint, because a variable cannot be
//         the target of 2 constraints.
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
Presolver::RuleStatus Presolver::PresolveIntEq(Constraint* ct, std::string* log) {
  // Rule 1
  if (ct->arguments[0].IsVariable() && ct->arguments[1].HasOneValue() &&
      ct->arguments[1].Value() == 0 &&
      ContainsKey(difference_map_, ct->arguments[0].Var())) {
    log->append("propagate equality");
    IntersectVarWithSingleton(ct->arguments[0].Var(), 0);

    log->append(", transform null differences");
    const std::pair<IntegerVariable*, IntegerVariable*>& diff =
        FindOrDie(difference_map_, ct->arguments[0].Var());
    ct->arguments[0] = Argument::IntVarRef(diff.first);
    ct->arguments[1] = Argument::IntVarRef(diff.second);
    return CONSTRAINT_REWRITTEN;
  }
  if (ct->arguments[0].IsVariable()) {
    if (ct->arguments[1].HasOneValue()) {
      // Rule 2.
      const int64 value = ct->arguments[1].Value();
      log->append("propagate equality");
      IntersectVarWithSingleton(ct->arguments[0].Var(), value);
      return CONSTRAINT_ALWAYS_TRUE;
    } else if (ct->arguments[1].IsVariable()) {
      // Rule 3.
      AddVariableSubstition(ct->arguments[0].Var(), ct->arguments[1].Var());
      return CONSTRAINT_ALWAYS_TRUE;
    }
  } else if (ct->arguments[0].HasOneValue()) {  // Arg0 is an integer value.
    const int64 value = ct->arguments[0].Value();
    if (ct->arguments[1].IsVariable()) {
      // Rule 4.
      log->append("propagate equality");
      IntersectVarWithSingleton(ct->arguments[1].Var(), value);
      return CONSTRAINT_ALWAYS_TRUE;
    } else if (ct->arguments[1].HasOneValue() &&
               value == ct->arguments[1].Value()) {
      // Rule 5.
      // No-op, removing.
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }
  return NOT_CHANGED;
}

// Propagates inequality constraint.
//
// Rule 1:
// Input : int_ne(x, y), x and y not overlapping
// Action: Mark c as inactive.
//
// Rule 2:
// Input : int_ne(x, c) or int_ne(c, x)
// Action: remove c from the domain of x.
// Output: inactive constraint if the removal was successful
//         (domain is not too large to remove a value).
Presolver::RuleStatus Presolver::PresolveIntNe(Constraint* ct, std::string* log) {
  // Rule 1.
  if (ct->arguments[0].IsVariable() && ct->arguments[1].IsVariable()) {
    IntegerVariable* const left = ct->arguments[0].Var();
    IntegerVariable* const right = ct->arguments[1].Var();
    if (left->domain.Min() > right->domain.Max() ||
        left->domain.Max() < right->domain.Min()) {
      log->append("variable domains are not overlapping");
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }

  // Rule 2.
  if (ct->presolve_propagation_done) {
    return NOT_CHANGED;
  }
  Argument& a = ct->arguments[0];
  Argument& b = ct->arguments[1];
  if (a.IsVariable() && b.HasOneValue()) {
    if (!a.Var()->domain.Contains(b.Value())) {
      log->append("value is not in domain");
      return CONSTRAINT_ALWAYS_TRUE;
    }
    if (RemoveValue(a.Var(), b.Value())) {
      log->append("remove value from variable domain");
      return CONSTRAINT_ALWAYS_TRUE;
    }
  } else if (b.IsVariable() && a.HasOneValue()) {
    if (!b.Var()->domain.Contains(a.Value())) {
      log->append("value is not in domain");
      return CONSTRAINT_ALWAYS_TRUE;
    }
    if (RemoveValue(b.Var(), a.Value())) {
      log->append("remove value from variable domain");
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }
  return NOT_CHANGED;
}

// Bound propagation on comparisons: int_le, bool_le, int_lt, bool_lt,
//                                   int_ge, bool_ge, int_gt, bool_gt.
//
// Rule 1:
// Input : int_XX(c1, c2) or bool_xx(c1, c2) with xx = lt, le, gt, ge
// Output: True or False constraint
// Rule 2:
// Input : int_xx(x, c) or int_xx(c, x) or bool_xx(x, c) or bool_xx(c, x)
//          with xx == lt, le, gt, ge
// Action: Reduce domain of x.
// Output: constraint is inactive.
//
// Rule 3:
// Input : int_xx(x, y) or bool_xx(x, y) with xx == lt, le, gt, ge.
// Action: Reduce domain of x and y.
// Output: constraint is still active.
Presolver::RuleStatus Presolver::PresolveInequalities(Constraint* ct,
                                                      std::string* log) {
  const std::string& id = ct->type;
  if (ct->arguments[0].HasOneValue() && ct->arguments[1].HasOneValue()) {
    // Rule 1
    const int64 left = ct->arguments[0].Value();
    const int64 right = ct->arguments[1].Value();
    bool result = true;
    if (id == "int_le" || id == "bool_le") {
      result = left <= right;
    } else if (id == "int_lt" || id == "bool_lt") {
      result = left < right;
    } else if (id == "int_ge" || id == "bool_ge") {
      result = left >= right;
    } else if (id == "int_gt" || id == "bool_gt") {
      result = left > right;
    }
    if (result) {
      log->append("constraint trivially true");
      return CONSTRAINT_ALWAYS_TRUE;
    } else {
      log->append("constraint trivially false");
      return CONSTRAINT_ALWAYS_FALSE;
    }
  }

  if (ct->arguments[0].IsVariable() && ct->arguments[1].HasOneValue()) {
    // Rule 2 where the 'var' is the left operand, eg. var <= 5
    IntegerVariable* const var = ct->arguments[0].Var();
    const int64 value = ct->arguments[1].Value();
    bool changed = false;
    if (id == "int_le" || id == "bool_le") {
      changed = IntersectVarWithInterval(var, kint64min, value);
    } else if (id == "int_lt" || id == "bool_lt") {
      changed = IntersectVarWithInterval(var, kint64min, value - 1);
    } else if (id == "int_ge" || id == "bool_ge") {
      changed = IntersectVarWithInterval(var, value, kint64max);
    } else if (id == "int_gt" || id == "bool_gt") {
      changed = IntersectVarWithInterval(var, value + 1, kint64max);
    }
    if (changed) {
      log->append("propagate bounds");
    }
    return CONSTRAINT_ALWAYS_TRUE;
  } else if (ct->arguments[0].HasOneValue() && ct->arguments[1].IsVariable()) {
    // Rule 2 where the 'var' is the right operand, eg 5 <= var
    IntegerVariable* const var = ct->arguments[1].Var();
    const int64 value = ct->arguments[0].Value();
    bool changed = false;
    if (id == "int_le" || id == "bool_le") {
      changed = IntersectVarWithInterval(var, value, kint64max);
    } else if (id == "int_lt" || id == "bool_lt") {
      changed = IntersectVarWithInterval(var, value + 1, kint64max);
    } else if (id == "int_ge" || id == "bool_ge") {
      changed = IntersectVarWithInterval(var, kint64min, value);
    } else if (id == "int_gt" || id == "bool_gt") {
      changed = IntersectVarWithInterval(var, kint64min, value - 1);
    }
    if (changed) {
      log->append("propagate bounds");
    }
    return CONSTRAINT_ALWAYS_TRUE;
  }

  // Rule 3.
  IntegerVariable* const left = ct->arguments[0].Var();
  const int64 left_min = left->domain.Min();
  const int64 left_max = left->domain.Max();
  IntegerVariable* const right = ct->arguments[1].Var();
  const int64 right_min = right->domain.Min();
  const int64 right_max = right->domain.Max();
  if (id == "int_le" || id == "bool_le") {
    IntersectVarWithInterval(left, kint64min, right_max);
    IntersectVarWithInterval(right, left_min, kint64max);
  } else if (id == "int_lt" || id == "bool_lt") {
    IntersectVarWithInterval(left, kint64min, right_max - 1);
    IntersectVarWithInterval(right, left_min + 1, kint64max);
  } else if (id == "int_ge" || id == "bool_ge") {
    IntersectVarWithInterval(left, right_min, kint64max);
    IntersectVarWithInterval(right, kint64min, left_max);
  } else if (id == "int_gt" || id == "bool_gt") {
    IntersectVarWithInterval(left, right_min + 1, kint64max);
    IntersectVarWithInterval(right, kint64min, left_max - 1);
  }
  return NOT_CHANGED;
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
Presolver::RuleStatus Presolver::Unreify(Constraint* ct, std::string* log) {
  const Argument& last_argument = ct->arguments.back();
  if (!last_argument.HasOneValue()) {
    return NOT_CHANGED;
  }
  DCHECK(strings::EndsWith(ct->type, "_reif")) << ct->DebugString();
  ct->type.resize(ct->type.size() - 5);
  ct->RemoveTargetVariable();
  if (last_argument.Value() == 1) {
    // Rule 1.
    log->append("unreify constraint");
    ct->RemoveTargetVariable();
    ct->arguments.pop_back();
  } else if (ct->type == "set_in" || ct->type == "set_not_in") {
    // Rule 2.
    log->append("unreify and reverse set constraint");
    ct->RemoveTargetVariable();
    ct->arguments.pop_back();
    ct->type.resize(ct->type.size() - 2);
    ct->type += "not_in";
  } else {
    // Rule 2.
    log->append("unreify and reverse constraint");
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
  return CONSTRAINT_REWRITTEN;
}

// Propagates the values of set_in
// Input : set_in(x, [c1..c2]) or set_in(x, {c1, .., cn})
// Action: Intersect the domain of x with the set of values.
// Output: inactive constraint.
// note: set_in(x1, {x2, ...}) is plain illegal so we don't bother with it.
Presolver::RuleStatus Presolver::PresolveSetIn(Constraint* ct, std::string* log) {
  if (ct->arguments[0].IsVariable()) {
    // IntersectDomainWith() will DCHECK that the second argument is a set
    // of constant values.
    log->append("propagate set on variable domain");
    IntersectVarWithArg(ct->arguments[0].Var(), ct->arguments[1]);
    return CONSTRAINT_ALWAYS_TRUE;
  }
  return NOT_CHANGED;
}

// Propagates the values of set_not_in
// Input : set_not_in(x, [c1..c2]) or set_not_in(x, {c1, .., cn})
// Action: Remove the values from the domain of x.
Presolver::RuleStatus Presolver::PresolveSetNotIn(Constraint* ct, std::string* log) {
  if (ct->arguments[0].IsVariable()) {
    const Argument& arg = ct->arguments[1];
    IntegerVariable* const var = ct->arguments[0].Var();
    if (arg.HasOneValue()) {
      const int64 value = arg.Value();
      if (!var->domain.Contains(value)) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else if (RemoveValue(var, value)) {
        return CONSTRAINT_ALWAYS_TRUE;
      }
    } else {
      bool changed = false;
      bool succeed = true;
      if (arg.type == Argument::INT_INTERVAL) {
        for (int64 value = arg.values[0]; value <= arg.values[1]; ++value) {
          if (var->domain.Contains(value)) {
            if (var->domain.RemoveValue(value)) {
              changed = true;
            } else {
              succeed = false;
              break;
            }
          }
        }
      } else {
        CHECK_EQ(arg.type, Argument::INT_LIST);
        for (int64 value : arg.values) {
          if (var->domain.Contains(value)) {
            if (var->domain.RemoveValue(value)) {
              changed = true;
            } else {
              succeed = false;
              break;
            }
          }
        }
      }
      if (changed) {
        MarkChangedVariable(var);
      }
      return succeed ? CONSTRAINT_ALWAYS_TRUE : NOT_CHANGED;
    }
  }
  return NOT_CHANGED;
}

// Propagates the values of set_in_reif
// Input : set_in_reif(x, [c1..c2], b) or set_in_reif(x, {c1, .., cn}, b)
//
// Rule 1: decide b if it can be decided.
// Rule 2: replace by int_eq_reif or int_ne_reif if there is just one
//         alternative.
Presolver::RuleStatus Presolver::PresolveSetInReif(Constraint* ct,
                                                   std::string* log) {
  if (ct->arguments[0].IsVariable() || ct->arguments[0].HasOneValue()) {
    int in = 0;
    int64 first_in_value;
    int out = 0;
    int64 first_out_value;
    for (const int64 value : GetValueSet(ct->arguments[0])) {
      if (ct->arguments[1].Contains(value)) {
        in++;
        first_in_value = value;
      } else {
        out++;
        first_out_value = value;
      }
      // Early break.
      if (in > 1 && out > 1) {
        break;
      }
    }

    // Note that these rules still works if b is fixed.
    if (in == 0) {
      ct->RemoveArg(1);
      ct->type = "bool_eq";
      ct->arguments[0] = Argument::IntegerValue(0);
      return CONSTRAINT_REWRITTEN;
    } else if (out == 0) {
      ct->RemoveArg(1);
      ct->type = "bool_eq";
      ct->arguments[0] = Argument::IntegerValue(1);
      return CONSTRAINT_REWRITTEN;
    } else if (in == 1) {
      ct->type = "int_eq_reif";
      ct->arguments[1] = Argument::IntegerValue(first_in_value);
      return CONSTRAINT_REWRITTEN;
    } else if (out == 1) {
      ct->type = "int_ne_reif";
      ct->arguments[1] = Argument::IntegerValue(first_out_value);
      return CONSTRAINT_REWRITTEN;
    }
  }
  return NOT_CHANGED;
}

// Propagates bound product.
// Input : int_times(c1, c2, x)
// Action: reduce domain of x to {c1 * c2}
// Output: inactive constraint.
Presolver::RuleStatus Presolver::PresolveIntTimes(Constraint* ct, std::string* log) {
  if (ct->arguments[0].HasOneValue() && ct->arguments[1].HasOneValue() &&
      ct->arguments[2].IsVariable() && !ct->presolve_propagation_done) {
    log->append("propagate constants");
    const int64 value = ct->arguments[0].Value() * ct->arguments[1].Value();
    const int64 safe_value =
        CapProd(ct->arguments[0].Value(), ct->arguments[1].Value());
    if (value == safe_value) {
      ct->presolve_propagation_done = true;
      if (ct->arguments[2].Var()->domain.Contains(value)) {
        IntersectVarWithSingleton(ct->arguments[2].Var(), value);
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        log->append(
            "  - product is not compatible with variable domain, "
            "ignoring presolve");
        // TODO(user): Treat failure correctly.
      }
    } else {
      log->append("  - product overflows, ignoring presolve");
      // TODO(user): Treat overflow correctly.
    }
  }

  // Special case: multiplication by zero.
  if ((ct->arguments[0].HasOneValue() && ct->arguments[0].Value() == 0) ||
      (ct->arguments[1].HasOneValue() && ct->arguments[1].Value() == 0)) {
    ct->type = "int_eq";
    ct->arguments[0] = ct->arguments[2];
    ct->arguments.resize(1);
    ct->arguments.push_back(Argument::IntegerValue(0));
    return CONSTRAINT_REWRITTEN;
  }

  return NOT_CHANGED;
}

// Propagates bound division.
// Input : int_div(c1, c2, x) (c2 != 0)
// Action: reduce domain of x to {c1 / c2}
// Output: inactive constraint.
Presolver::RuleStatus Presolver::PresolveIntDiv(Constraint* ct, std::string* log) {
  if (ct->arguments[0].HasOneValue() && ct->arguments[1].HasOneValue() &&
      ct->arguments[2].IsVariable() && !ct->presolve_propagation_done &&
      ct->arguments[1].Value() != 0) {
    log->append("propagate constants");
    const int64 value = ct->arguments[0].Value() / ct->arguments[1].Value();
    ct->presolve_propagation_done = true;
    if (ct->arguments[2].Var()->domain.Contains(value)) {
      IntersectVarWithSingleton(ct->arguments[2].Var(), value);
      return CONSTRAINT_ALWAYS_TRUE;
    } else {
      log->append(
          "  - division is not compatible with variable domain, "
          "ignoring presolve");
      // TODO(user): Treat failure correctly.
    }
  }
  // TODO(user): Catch c2 = 0 case and set the model to invalid.
  return NOT_CHANGED;
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
Presolver::RuleStatus Presolver::PresolveArrayBoolOr(Constraint* ct,
                                                     std::string* log) {
  if (ct->arguments[0].variables.size() == 1) {
    // Rule 1.
    ct->type = "bool_eq";
    ct->arguments[0].type = Argument::INT_VAR_REF;
    return CONSTRAINT_REWRITTEN;
  }
  if (!ct->presolve_propagation_done && ct->arguments[1].HasOneValue() &&
      ct->arguments[1].Value() == 0) {
    // Rule 2.
    // TODO(user): Support empty domains correctly, and remove this test.
    for (const IntegerVariable* const var : ct->arguments[0].variables) {
      if (!var->domain.Contains(0)) {
        return NOT_CHANGED;
      }
    }
    log->append("propagate constants");
    for (IntegerVariable* const var : ct->arguments[0].variables) {
      IntersectVarWithSingleton(var, 0);
    }
    return CONSTRAINT_ALWAYS_TRUE;
  }
  bool has_bound_true_value = false;
  std::vector<IntegerVariable*> unbound;
  for (IntegerVariable* const var : ct->arguments[0].variables) {
    if (var->domain.HasOneValue()) {
      has_bound_true_value |= var->domain.Min() == 1;
    } else {
      unbound.push_back(var);
    }
  }
  if (has_bound_true_value) {
    // Rule 3.
    if (!ct->arguments[1].HasOneValue()) {
      log->append("propagate target variable to true");
      IntersectVarWithSingleton(ct->arguments[1].variables[0], 1);
      return CONSTRAINT_ALWAYS_TRUE;
    } else if (ct->arguments[1].HasOneValue() &&
               ct->arguments[1].Value() == 1) {
      return CONSTRAINT_ALWAYS_TRUE;
    }
    return NOT_CHANGED;
    // TODO(user): Simplify code once we support empty domains.
  }
  if (unbound.empty()) {
    // Rule 4.
    if (!ct->arguments[1].HasOneValue()) {
      // TODO(user): Simplify code once we support empty domains.
      log->append("propagate target variable to false");
      IntersectVarWithSingleton(ct->arguments[1].variables[0], 0);
      return CONSTRAINT_ALWAYS_TRUE;
    }
    return NOT_CHANGED;
  }
  if (unbound.size() < ct->arguments[0].variables.size()) {
    // Rule 5.
    log->append("Reduce array");
    ct->arguments[0].variables.swap(unbound);
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
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
Presolver::RuleStatus Presolver::PresolveArrayBoolAnd(Constraint* ct,
                                                      std::string* log) {
  if (ct->arguments[0].variables.size() == 1) {
    // Rule 1.
    ct->type = "bool_eq";
    ct->arguments[0].type = Argument::INT_VAR_REF;
    return CONSTRAINT_REWRITTEN;
  }
  if (!ct->presolve_propagation_done && ct->arguments[1].HasOneValue() &&
      ct->arguments[1].Value() == 1) {
    // Rule 2.
    // TODO(user): Simplify the code once we support empty domains.
    for (const IntegerVariable* const var : ct->arguments[0].variables) {
      if (!var->domain.Contains(1)) {
        return NOT_CHANGED;
      }
    }
    log->append("propagate constants");
    for (IntegerVariable* const var : ct->arguments[0].variables) {
      IntersectVarWithSingleton(var, 1);
    }
    ct->presolve_propagation_done = true;
    return CONSTRAINT_ALWAYS_TRUE;
  }
  int has_bound_false_value = 0;
  std::vector<IntegerVariable*> unbound;
  for (IntegerVariable* const var : ct->arguments[0].variables) {
    if (var->domain.HasOneValue()) {
      has_bound_false_value |= var->domain.Max() == 0;
    } else {
      unbound.push_back(var);
    }
  }
  if (has_bound_false_value) {
    // TODO(user): Simplify the code once we support empty domains.
    if (!ct->arguments[1].HasOneValue()) {
      // Rule 3.
      log->append("propagate target variable to false");
      IntersectVarWithSingleton(ct->arguments[1].variables[0], 0);
      return CONSTRAINT_ALWAYS_TRUE;
    }
    if (ct->arguments[1].HasOneValue() && ct->arguments[1].Value() == 0) {
      return CONSTRAINT_ALWAYS_TRUE;
    }
    return NOT_CHANGED;
  }
  if (unbound.empty()) {
    // Rule 4.
    if (!ct->arguments[1].HasOneValue()) {
      log->append("propagate target variable to true");
      IntersectVarWithSingleton(ct->arguments[1].variables[0], 1);
      return CONSTRAINT_ALWAYS_TRUE;
    }
    return NOT_CHANGED;
  }
  if (unbound.size() < ct->arguments[0].variables.size()) {
    log->append("reduce array");
    ct->arguments[0].variables.swap(unbound);
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
}

// Simplifies bool_XX_reif(b1, b2, b3)  (which means b3 = (b1 XX b2)) when the
// middle value is bound.
// Input: bool_XX_reif(b1, t, b2), where XX is "eq" or "ne".
// Output: bool_YY(b1, b2) where YY is "eq" or "not" depending on XX and t.
Presolver::RuleStatus Presolver::PresolveBoolEqNeReif(Constraint* ct,
                                                      std::string* log) {
  DCHECK(ct->type == "bool_eq_reif" || ct->type == "bool_ne_reif");
  if (ct->arguments[1].HasOneValue()) {
    log->append("simplify constraint");
    const int64 value = ct->arguments[1].Value();
    // Remove boolean value argument.
    ct->RemoveArg(1);
    // Change type.
    ct->type = ((ct->type == "bool_eq_reif" && value == 1) ||
                (ct->type == "bool_ne_reif" && value == 0))
                   ? "bool_eq"
                   : "bool_not";
    return CONSTRAINT_REWRITTEN;
  }
  if (ct->arguments[0].HasOneValue()) {
    log->append("simplify constraint");
    const int64 value = ct->arguments[0].Value();
    // Remove boolean value argument.
    ct->RemoveArg(0);
    // Change type.
    ct->type = ((ct->type == "bool_eq_reif" && value == 1) ||
                (ct->type == "bool_ne_reif" && value == 0))
                   ? "bool_eq"
                   : "bool_not";
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
}

// Transform int_lin_gt (which means ScalProd(arg1[], arg2[]) > c) into
// int_lin_ge.
// Input : int_lin_gt(arg1, arg2, c)
// Output: int_lin_ge(arg1, arg2, c + 1)
Presolver::RuleStatus Presolver::PresolveIntLinGt(Constraint* ct, std::string* log) {
  CHECK_EQ(Argument::INT_VALUE, ct->arguments[2].type);
  if (ct->arguments[2].Value() != kint64max) {
    ct->arguments[2].values[0]++;
    ct->type = "int_lin_ge";
    return CONSTRAINT_REWRITTEN;
  }
  // TODO(user): fail (the model is impossible: a * b > kint64max can be
  // considered as impossible; because it would imply an overflow; which we
  // reject.
  return NOT_CHANGED;
}

// Transform int_lin_lt into int_lin_le.
// Input : int_lin_lt(arg1, arg2, c)
// Output: int_lin_le(arg1, arg2, c - 1)
Presolver::RuleStatus Presolver::PresolveIntLinLt(Constraint* ct, std::string* log) {
  CHECK_EQ(Argument::INT_VALUE, ct->arguments[2].type);
  if (ct->arguments[2].Value() != kint64min) {
    ct->arguments[2].values[0]--;
    ct->type = "int_lin_le";
    return CONSTRAINT_REWRITTEN;
  }
  // TODO(user): fail (the model is impossible: a * b < kint64min can be
  // considered as impossible; because it would imply an overflow; which we
  // reject.
  return NOT_CHANGED;
}

// Simplifies linear equations of size 1, i.e. c1 * x = c2.
// Input : int_lin_xx([c1], [x], c2) and int_lin_xx_reif([c1], [x], c2, b)
//         with (c1 == 1 or c2 % c1 == 0) and xx = eq, ne, lt, le, gt, ge
// Output: int_xx(x, c2 / c1) and int_xx_reif(x, c2 / c1, b)
Presolver::RuleStatus Presolver::SimplifyUnaryLinear(Constraint* ct,
                                                     std::string* log) {
  if (!ct->arguments[0].HasOneValue() ||
      ct->arguments[1].variables.size() != 1) {
    return NOT_CHANGED;
  }
  const int64 coeff = ct->arguments[0].values.front();
  const int64 rhs = ct->arguments[2].Value();
  IntegerVariable* const var = ct->arguments[1].variables.front();
  const std::string op = ct->type.substr(8, 2);
  bool changed = false;
  int64 new_rhs = 0;

  if (coeff == 0) {
    ct->arguments[0].values.clear();
    ct->arguments[1].variables.clear();
    // Will be process by PresolveLinear.
    return CONSTRAINT_REWRITTEN;
  }

  if (op == "eq") {
    if (rhs % coeff == 0) {
      changed = true;
      new_rhs = rhs / coeff;
    } else {                            // Equality always false.
      if (ct->arguments.size() == 4) {  // reified version.
        SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 0);
        return CONSTRAINT_REWRITTEN;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
    }
  } else if (op == "ne") {
    if (rhs % coeff == 0) {
      changed = true;
      new_rhs = rhs / coeff;
    } else {                            // Equality always true.
      if (ct->arguments.size() == 4) {  // reified version.
        SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 1);
        return CONSTRAINT_REWRITTEN;
      } else {
        return CONSTRAINT_ALWAYS_TRUE;
      }
    }
  } else if (coeff >= 0 && rhs % coeff == 0) {
    // TODO(user): Support coefficient < 0 (and reverse the inequalities).
    // TODO(user): Support rhs % coefficient != 0, and do the correct
    // rounding in the case of inequalities.
    changed = true;
    new_rhs = rhs / coeff;
  }
  if (changed) {
    log->append("remove linear part");
    // transform arguments.
    ct->arguments[0] = Argument::IntVarRef(var);
    ct->arguments[1] = Argument::IntegerValue(new_rhs);
    ct->RemoveArg(2);
    // Change type (remove "_lin" part).
    DCHECK(ct->type.size() >= 8 && ct->type.substr(3, 4) == "_lin");
    ct->type.erase(3, 4);
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
}

// Simplifies linear equations of size 2, i.e. x - y = 0.
// Input : int_lin_xx([1, -1], [x1, x2], 0) and
//         int_lin_xx_reif([1, -1], [x1, x2], 0, b)
//         xx = eq, ne, lt, le, gt, ge
// Output: int_xx(x1, x2) and int_xx_reif(x, x2, b)
Presolver::RuleStatus Presolver::SimplifyBinaryLinear(Constraint* ct,
                                                      std::string* log) {
  const int64 rhs = ct->arguments[2].Value();
  if (ct->arguments[0].values.size() != 2 || rhs != 0 ||
      ct->arguments[1].variables.empty()) {
    return NOT_CHANGED;
  }

  IntegerVariable* first = nullptr;
  IntegerVariable* second = nullptr;
  if (ct->arguments[0].values[0] == 1 && ct->arguments[0].values[1] == -1) {
    first = ct->arguments[1].variables[0];
    second = ct->arguments[1].variables[1];
  } else if (ct->arguments[0].values[0] == -1 &&
             ct->arguments[0].values[1] == 1) {
    first = ct->arguments[1].variables[1];
    second = ct->arguments[1].variables[0];
  } else {
    return NOT_CHANGED;
  }

  log->append("remove linear part");
  ct->arguments[0] = Argument::IntVarRef(first);
  ct->arguments[1] = Argument::IntVarRef(second);
  ct->RemoveArg(2);
  // Change type (remove "_lin" part).
  DCHECK(ct->type.size() >= 8 && ct->type.substr(3, 4) == "_lin");
  ct->type.erase(3, 4);
  return CONSTRAINT_REWRITTEN;
}

// Returns false if an overflow occurred.
// Used by CheckIntLinReifBounds() below: compute the bounds of the scalar
// product. If an integer overflow occurs the method returns false.
namespace {
bool ComputeLinBounds(const std::vector<int64>& coefficients,
                      const std::vector<IntegerVariable*>& variables, int64* lb,
                      int64* ub) {
  CHECK_EQ(coefficients.size(), variables.size()) << "Wrong constraint";
  *lb = 0;
  *ub = 0;
  for (int i = 0; i < coefficients.size(); ++i) {
    const IntegerVariable* const var = variables[i];
    const int64 coef = coefficients[i];
    if (coef == 0) continue;
    if (var->domain.Min() == kint64min || var->domain.Max() == kint64max) {
      return false;
    }
    const int64 vmin = var->domain.Min();
    const int64 vmax = var->domain.Max();
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
Presolver::RuleStatus Presolver::CheckIntLinReifBounds(Constraint* ct,
                                                       std::string* log) {
  DCHECK_EQ(ct->type, "int_lin_eq_reif");
  int64 lb = 0;
  int64 ub = 0;
  if (!ComputeLinBounds(ct->arguments[0].values, ct->arguments[1].variables,
                        &lb, &ub)) {
    log->append("overflow found when presolving");
    return NOT_CHANGED;
  }
  const int64 value = ct->arguments[2].Value();
  if (value < lb || value > ub) {
    log->append("assign boolean to false");
    IntersectVarWithSingleton(ct->arguments[3].Var(), 0);
    return CONSTRAINT_ALWAYS_TRUE;
  } else if (value == lb && value == ub) {
    log->append("assign boolean to true");
    IntersectVarWithSingleton(ct->arguments[3].Var(), 1);
    return CONSTRAINT_ALWAYS_TRUE;
  }
  return NOT_CHANGED;
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
Presolver::RuleStatus Presolver::CreateLinearTarget(Constraint* ct,
                                                    std::string* log) {
  if (ct->target_variable != nullptr) return NOT_CHANGED;

  for (const int var_index : {0, 1}) {
    if (ct->arguments[0].values.size() == 2 &&
        ct->arguments[0].values[var_index] == -1 &&
        ct->arguments[1].variables[var_index]->defining_constraint == nullptr &&
        !ct->arguments[1].variables[var_index]->domain.HasOneValue()) {
      // Rule 1.
      StringAppendF(log, "mark variable index %i as target", var_index);
      IntegerVariable* const var = ct->arguments[1].variables[var_index];
      var->defining_constraint = ct;
      ct->target_variable = var;
      MarkChangedVariable(var);  // To force a rescan of users of this var.
      return CONSTRAINT_REWRITTEN;
    }
  }
  return NOT_CHANGED;
}

// Propagates: array_int_element
//
// Rule 1:
// Input: array_int_element(x, [c1, .., cn], y)
// Action: Intersect the domain of x with [1 .. n]
//
// Rule 2:
// Input : array_int_element(x, [c1, .., cn], y)
// Output: array_int_element(x, [c1, .., cm], y) if all cm+1, .., cn are not
//         the domain of y.
//
// Rule 3:
// Input : array_int_element(x, [c1, .., cn], y)
// Action: Intersect the domain of y with the set of values.
Presolver::RuleStatus Presolver::PresolveArrayIntElement(Constraint* ct,
                                                         std::string* log) {
  const int array_size = ct->arguments[1].values.size();
  if (ct->arguments[0].variables.size() == 1) {
    // Rule 1.
    if (ct->arguments[0].IsVariable() &&
        (ct->arguments[0].Var()->domain.Min() < 1 ||
         ct->arguments[0].Var()->domain.Max() > array_size)) {
      IntersectVarWithInterval(ct->arguments[0].Var(), 1, array_size);
    }

    // Rule 2.
    if (!ct->arguments[0].HasOneValue()) {
      const int64 target_min = ct->arguments[2].HasOneValue()
                                   ? ct->arguments[2].Value()
                                   : ct->arguments[2].Var()->domain.Min();
      const int64 target_max = ct->arguments[2].HasOneValue()
                                   ? ct->arguments[2].Value()
                                   : ct->arguments[2].Var()->domain.Max();

      int64 last_index = array_size;
      last_index = std::min(ct->arguments[0].Var()->domain.Max(), last_index);

      while (last_index >= 1) {
        const int64 value = ct->arguments[1].values[last_index - 1];
        if (value < target_min || value > target_max) {
          last_index--;
        } else {
          break;
        }
      }

      int64 first_index = 1;
      first_index = std::max(ct->arguments[0].Var()->domain.Min(), first_index);
      while (first_index <= last_index) {
        const int64 value = ct->arguments[1].values[first_index - 1];
        if (value < target_min || value > target_max) {
          first_index++;
        } else {
          break;
        }
      }

      if (last_index < ct->arguments[0].Var()->domain.Max() ||
          first_index > ct->arguments[0].Var()->domain.Min()) {
        StringAppendF(log, "filter index to [%" GG_LL_FORMAT "d..%" GG_LL_FORMAT
                           "d] and reduce array to size %" GG_LL_FORMAT "d",
                      first_index, last_index, last_index);
        IntersectVarWithInterval(ct->arguments[0].Var(), first_index,
                                 last_index);
        ct->arguments[1].values.resize(last_index);
        return CONSTRAINT_REWRITTEN;
      }
    }
  }

  // Rule 3.
  if (ct->arguments[0].IsVariable() && ct->arguments[2].IsVariable() &&
      !ct->presolve_propagation_done) {
    CHECK_EQ(Argument::INT_LIST, ct->arguments[1].type);
    log->append("propagate domain");
    std::set<int64> visited;
    for (int value : GetValueSet(ct->arguments[0])) {
      CHECK(value >= 1 && value <= array_size);
      visited.insert(ct->arguments[1].values[value - 1]);
    }

    std::vector<int64> sorted_values(visited.begin(), visited.end());
    const std::string before = ct->arguments[2].Var()->DebugString();
    if (ct->arguments[2].Var()->domain.IntersectWithListOfIntegers(
            sorted_values)) {
      MarkChangedVariable(ct->arguments[2].Var());
    }
    const std::string after = ct->arguments[2].Var()->DebugString();
    if (before != after) {
      log->append(StringPrintf(", reduce target variable from %s to %s",
                               before.c_str(), after.c_str()));
      ct->presolve_propagation_done = true;
      return CONSTRAINT_REWRITTEN;
    }
  }

  return NOT_CHANGED;
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
// Rule 2a:
// Input: int_lin_xxx[[c1, .., cn], [c'1, .., c'n], c0]  (no variables)
// Output: inactive or false constraint.
//
// Rule 2b:
// Input: int_lin_xxx[[], [], c0] or int_lin_xxx_reif([], [], c0)
// Output: inactive or false constraint.
//
// Rule 3:
// Input: int_lin_xxx_reif[[c1, .., cn], [c'1, .., c'n], c0]  (no variables)
// Output: bool_eq(c0, true or false).
//
// Rule 4:
// Input: int_lin_xxx([c1, .., cn], [x1,.., xk, .., xn], c0) with xk fixed
// Output: int_lin_xxx([c1, .., ck-1, ck+1, .., cn], [x1, xk-1, xk+1, .., xn],
//                     c0 - ck * xk.Value())
//
// TODO(user): The code is broken in case of integer-overflow.
Presolver::RuleStatus Presolver::PresolveLinear(Constraint* ct, std::string* log) {
  // Rule 2a and 2b.
  if (ct->arguments[0].values.empty() || ct->arguments[1].IsArrayOfValues()) {
    log->append("rewrite constant linear equation");
    int64 scalprod = 0;
    for (int i = 0; i < ct->arguments[0].values.size(); ++i) {
      scalprod += ct->arguments[0].values[i] * ct->arguments[1].ValueAt(i);
    }
    const int64 rhs = ct->arguments[2].Value();
    if (ct->type == "int_lin_eq") {
      if (scalprod == rhs) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
    } else if (ct->type == "int_lin_le") {
      if (scalprod <= rhs) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
    } else if (ct->type == "int_lin_ge") {
      if (scalprod >= rhs) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
    } else if (ct->type == "int_lin_ne") {
      if (scalprod != rhs) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
      // Rule 3
    } else if (ct->type == "int_lin_eq_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(Argument::IntegerValue(scalprod == rhs));
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_ge") {
      if (scalprod >= rhs) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
    } else if (ct->type == "int_lin_ge_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(Argument::IntegerValue(scalprod >= rhs));
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_le") {
      if (scalprod <= rhs) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
    } else if (ct->type == "int_lin_le_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(Argument::IntegerValue(scalprod <= rhs));
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_ne_reif") {
      ct->type = "bool_eq";
      ct->arguments[0] = ct->arguments[3];
      ct->arguments.resize(1);
      ct->arguments.push_back(Argument::IntegerValue(scalprod != rhs));
      return CONSTRAINT_REWRITTEN;
    }
  }

  if (ct->arguments[0].values.empty()) {
    return NOT_CHANGED;
  }
  // From now on, we assume the linear part is not empty.

  // Rule 4.
  if (!ct->arguments[1].variables.empty()) {
    const int size = ct->arguments[1].variables.size();
    std::vector<IntegerVariable*>& vars = ct->arguments[1].variables;
    std::vector<int64>& coeffs = ct->arguments[0].values;
    // We start by skipping over the non-fixed position. This is a
    // speed optimization.
    for (int position = 0; position < size; ++position) {
      if (!vars[position]->domain.HasOneValue()) {
        continue;
      }
      int new_size = position;
      int64 new_rhs = ct->arguments[2].Value();
      new_rhs -= coeffs[position] * vars[position]->domain.Value();
      for (int i = position + 1; i < size; ++i) {
        if (vars[i]->domain.HasOneValue()) {
          new_rhs -= coeffs[i] * vars[i]->domain.Value();
        } else {
          coeffs[new_size] = coeffs[i];
          vars[new_size] = vars[i];
          new_size++;
        }
      }
      vars.resize(new_size);
      coeffs.resize(new_size);
      ct->arguments[2] = Argument::IntegerValue(new_rhs);
      return CONSTRAINT_REWRITTEN;
    }
  }

  // Rule 1.
  for (const int64 coef : ct->arguments[0].values) {
    if (coef > 0) {
      return NOT_CHANGED;
    }
  }
  if (ct->target_variable != nullptr) {
    for (IntegerVariable* const var : ct->arguments[1].variables) {
      if (var == ct->target_variable) {
        return NOT_CHANGED;
      }
    }
  }
  log->append("reverse constraint");
  for (int64& coef : ct->arguments[0].values) {
    coef *= -1;
  }
  ct->arguments[2].values[0] *= -1;
  if (ct->type == "int_lin_le") {
    ct->type = "int_lin_ge";
  } else if (ct->type == "int_lin_lt") {
    ct->type = "int_lin_gt";
  } else if (ct->type == "int_lin_ge") {
    ct->type = "int_lin_le";
  } else if (ct->type == "int_lin_gt") {
    ct->type = "int_lin_lt";
  } else if (ct->type == "int_lin_le_reif") {
    ct->type = "int_lin_ge_reif";
  } else if (ct->type == "int_lin_ge_reif") {
    ct->type = "int_lin_le_reif";
  }
  return CONSTRAINT_REWRITTEN;
}

// Regroup linear term with the same variable.
// Input : int_lin_xxx([c1, .., cn], [x1, .., xn], c0) with xi = xj
// Output: int_lin_xxx([c1, .., ci + cj, .., cn], [x1, .., xi, .., xn], c0)
Presolver::RuleStatus Presolver::RegroupLinear(Constraint* ct, std::string* log) {
  if (ct->arguments[1].variables.empty()) {
    // Only constants, or size == 0.
    return NOT_CHANGED;
  }
  std::unordered_map<const IntegerVariable*, int64> coefficients;
  const int original_size = ct->arguments[0].values.size();
  for (int i = 0; i < original_size; ++i) {
    coefficients[ct->arguments[1].variables[i]] += ct->arguments[0].values[i];
  }
  if (coefficients.size() != original_size) {  // Duplicate variables.
    log->append("regroup variables");
    std::unordered_set<const IntegerVariable*> processed;
    int index = 0;
    int zero = 0;
    for (int i = 0; i < original_size; ++i) {
      IntegerVariable* fz_var = ct->arguments[1].variables[i];
      const int64 coefficient = coefficients[fz_var];
      if (!ContainsKey(processed, fz_var)) {
        processed.insert(fz_var);
        if (coefficient != 0) {
          ct->arguments[1].variables[index] = fz_var;
          ct->arguments[0].values[index] = coefficient;
          index++;
        } else {
          zero++;
        }
      }
    }
    CHECK_EQ(index + zero, coefficients.size());
    ct->arguments[0].values.resize(index);
    ct->arguments[1].variables.resize(index);
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
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
Presolver::RuleStatus Presolver::PropagatePositiveLinear(Constraint* ct,
                                                         std::string* log) {
  const int64 rhs = ct->arguments[2].Value();
  if (ct->presolve_propagation_done || rhs < 0 ||
      ct->arguments[1].variables.empty()) {
    return NOT_CHANGED;
  }
  for (const int64 coef : ct->arguments[0].values) {
    if (coef < 0) {
      return NOT_CHANGED;
    }
  }
  for (IntegerVariable* const var : ct->arguments[1].variables) {
    if (var->domain.Min() < 0) {
      return NOT_CHANGED;
    }
  }
  if (ct->type != "int_lin_ge") {
    // Rule 1.
    log->append("propagate constants");
    for (int i = 0; i < ct->arguments[0].values.size(); ++i) {
      const int64 coef = ct->arguments[0].values[i];
      if (coef > 0) {
        IntegerVariable* const var = ct->arguments[1].variables[i];
        const int64 bound = rhs / coef;
        if (bound < var->domain.Max()) {
          StringAppendF(log, ", intersect %s with [0..%" GG_LL_FORMAT "d]",
                        var->DebugString().c_str(), bound);
          IntersectVarWithInterval(var, 0, bound);
        }
      }
    }
  } else if (ct->arguments[0].values.size() == 1 &&
             ct->arguments[0].values[0] > 0) {
    // Rule 2.
    const int64 coef = ct->arguments[0].values[0];
    IntegerVariable* const var = ct->arguments[1].variables[0];
    const int64 bound = (rhs + coef - 1) / coef;
    if (bound > var->domain.Min()) {
      StringAppendF(log, ", intersect %s with [%" GG_LL_FORMAT "d .. INT_MAX]",
                    var->DebugString().c_str(), bound);
      IntersectVarWithInterval(var, bound, kint64max);
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }
  ct->presolve_propagation_done = true;
  return NOT_CHANGED;
}

// Input: int_lin_xx([c1, .., cn], [x1, .., xn],  rhs)
//
// Computes the bounds on the rhs.
// Rule1: remove always true/false constraint or fix the reif Boolean.
// Rule2: transform ne/eq to gt/ge/lt/le if rhs is at one bound of its domain.
Presolver::RuleStatus Presolver::SimplifyLinear(Constraint* ct, std::string* log) {
  const int64 rhs = ct->arguments[2].Value();
  if (ct->arguments[1].variables.empty()) return NOT_CHANGED;

  int64 rhs_min = 0;
  int64 rhs_max = 0;
  const int n = ct->arguments[0].values.size();
  for (int i = 0; i < n; ++i) {
    const int64 coeff = ct->arguments[0].values[i];
    const int64 add_min =
        coeff > 0 ? CapProd(coeff, ct->arguments[1].variables[i]->domain.Min())
                  : CapProd(coeff, ct->arguments[1].variables[i]->domain.Max());
    const int64 add_max =
        coeff > 0 ? CapProd(coeff, ct->arguments[1].variables[i]->domain.Max())
                  : CapProd(coeff, ct->arguments[1].variables[i]->domain.Min());
    if (rhs_min != kint64min) {
      rhs_min = CapAdd(rhs_min, add_min);
    }
    if (rhs_max != kint64max) {
      rhs_max = CapAdd(rhs_max, add_max);
    }
    if (rhs_min == kint64min && rhs_max == kint64max) {
      break;  // Early exit the loop.
    }
  }
  if (ct->type == "int_lin_ge") {
    if (rhs_min >= rhs) {
      return CONSTRAINT_ALWAYS_TRUE;
    } else if (rhs_max < rhs) {
      return CONSTRAINT_ALWAYS_FALSE;
    }
  }
  if (ct->type == "int_lin_ge_reif") {
    if (rhs_min >= rhs) {
      SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 1);
      return CONSTRAINT_REWRITTEN;
    } else if (rhs_max < rhs) {
      SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 0);
      return CONSTRAINT_REWRITTEN;
    }
  }
  if (ct->type == "int_lin_le") {
    if (rhs_min > rhs) {
      return CONSTRAINT_ALWAYS_FALSE;
    } else if (rhs_max <= rhs) {
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }
  if (ct->type == "int_lin_le_reif") {
    if (rhs_min > rhs) {
      SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 0);
      return CONSTRAINT_REWRITTEN;
    } else if (rhs_max <= rhs) {
      SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 1);
      return CONSTRAINT_REWRITTEN;
    }
  }
  if (rhs < rhs_min || rhs > rhs_max) {
    if (ct->type == "int_lin_eq") {
      return CONSTRAINT_ALWAYS_FALSE;
    } else if (ct->type == "int_lin_eq_reif") {
      SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 0);
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_ne") {
      return CONSTRAINT_ALWAYS_TRUE;
    } else if (ct->type == "int_lin_ne_reif") {
      SetConstraintAsIntEq(ct, ct->arguments[3].Var(), 1);
      return CONSTRAINT_REWRITTEN;
    }
  } else if (rhs == rhs_min) {
    if (ct->type == "int_lin_eq") {
      ct->type = "int_lin_le";
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_eq_reif") {
      ct->type = "int_lin_le_reif";
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_ne") {
      ct->type = "int_lin_ge";
      ct->arguments[2] = Argument::IntegerValue(rhs + 1);
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_ne_reif") {
      ct->type = "int_lin_ge_reif";
      ct->arguments[2] = Argument::IntegerValue(rhs + 1);
      return CONSTRAINT_REWRITTEN;
    }
  } else if (rhs == rhs_max) {
    if (ct->type == "int_lin_eq") {
      ct->type = "int_lin_ge";
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_eq_reif") {
      ct->type = "int_lin_ge_reif";
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_ne") {
      ct->type = "int_lin_le";
      ct->arguments[2] = Argument::IntegerValue(rhs - 1);
      return CONSTRAINT_REWRITTEN;
    } else if (ct->type == "int_lin_ne_reif") {
      ct->type = "int_lin_le_reif";
      ct->arguments[2] = Argument::IntegerValue(rhs - 1);
      return CONSTRAINT_REWRITTEN;
    }
  }
  return NOT_CHANGED;
}

// Minizinc flattens 2d element constraints (x = A[y][z]) into 1d element
// constraint with an affine mapping between y, z and the new index.
// This rule stores the mapping to reconstruct the 2d element constraint.
// This mapping can involve 1 or 2 variables dependening if y or z in A[y][z]
// is a constant in the model).
Presolver::RuleStatus Presolver::PresolveStoreMapping(Constraint* ct,
                                                      std::string* log) {
  if (ct->arguments[1].variables.empty()) {
    // Constant linear constraint (no variables).
  }
  if (ct->arguments[0].values.size() == 2 &&
      ct->arguments[1].variables[0] == ct->target_variable &&
      ct->arguments[0].values[0] == -1 &&
      !ContainsKey(affine_map_, ct->target_variable) &&
      ct->strong_propagation) {
    affine_map_[ct->target_variable] =
        AffineMapping(ct->arguments[1].variables[1], ct->arguments[0].values[1],
                      -ct->arguments[2].Value(), ct);
    MarkChangedVariable(ct->target_variable);
    log->append("store affine mapping");
    return CONTEXT_CHANGED;
  }
  if (ct->arguments[0].values.size() == 2 &&
      ct->arguments[1].variables[1] == ct->target_variable &&
      ct->arguments[0].values[1] == -1 &&
      !ContainsKey(affine_map_, ct->target_variable)) {
    affine_map_[ct->target_variable] =
        AffineMapping(ct->arguments[1].variables[0], ct->arguments[0].values[0],
                      -ct->arguments[2].Value(), ct);
    log->append("store affine mapping");
    MarkChangedVariable(ct->target_variable);
    return CONTEXT_CHANGED;
  }
  if (ct->arguments[0].values.size() == 3 &&
      ct->arguments[1].variables[0] == ct->target_variable &&
      ct->arguments[0].values[0] == -1 && ct->arguments[0].values[2] == 1 &&
      !ContainsKey(array2d_index_map_, ct->target_variable) &&
      ct->strong_propagation) {
    array2d_index_map_[ct->target_variable] = Array2DIndexMapping(
        ct->arguments[1].variables[1], ct->arguments[0].values[1],
        ct->arguments[1].variables[2], -ct->arguments[2].Value(), ct);
    log->append("store affine mapping");
    MarkChangedVariable(ct->target_variable);
    return CONTEXT_CHANGED;
  }
  if (ct->arguments[0].values.size() == 3 &&
      ct->arguments[1].variables[0] == ct->target_variable &&
      ct->arguments[0].values[0] == -1 && ct->arguments[0].values[1] == 1 &&
      !ContainsKey(array2d_index_map_, ct->target_variable) &&
      ct->strong_propagation) {
    array2d_index_map_[ct->target_variable] = Array2DIndexMapping(
        ct->arguments[1].variables[2], ct->arguments[0].values[2],
        ct->arguments[1].variables[1], -ct->arguments[2].Value(), ct);
    log->append("store affine mapping");
    MarkChangedVariable(ct->target_variable);
    return CONTEXT_CHANGED;
  }
  if (ct->arguments[0].values.size() == 3 &&
      ct->arguments[1].variables[2] == ct->target_variable &&
      ct->arguments[0].values[2] == -1 && ct->arguments[0].values[1] == 1 &&
      !ContainsKey(array2d_index_map_, ct->target_variable)) {
    array2d_index_map_[ct->target_variable] = Array2DIndexMapping(
        ct->arguments[1].variables[0], ct->arguments[0].values[0],
        ct->arguments[1].variables[1], -ct->arguments[2].Value(), ct);
    log->append("store affine mapping");
    MarkChangedVariable(ct->target_variable);
    return CONTEXT_CHANGED;
  }
  if (ct->arguments[0].values.size() == 3 &&
      ct->arguments[1].variables[2] == ct->target_variable &&
      ct->arguments[0].values[2] == -1 && ct->arguments[0].values[0] == 1 &&
      !ContainsKey(array2d_index_map_, ct->target_variable)) {
    array2d_index_map_[ct->target_variable] = Array2DIndexMapping(
        ct->arguments[1].variables[1], ct->arguments[0].values[1],
        ct->arguments[1].variables[0], -ct->arguments[2].Value(), ct);
    log->append("store affine mapping");
    MarkChangedVariable(ct->target_variable);
    return CONTEXT_CHANGED;
  }
  return NOT_CHANGED;
}

namespace {
bool IsIncreasingAndContiguous(const std::vector<int64>& values) {
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
// Rule 1a:
// Input : array_int_element(x, [c1, .., cn], y) with x fixed at one value.
// Output: int_eq(b, c_x.Value())
//
// Rule 1b:
// Input : array_int_element(x, [c1, .., cn], y) with y fixed at one value.
// Output: set_in(x, [i | c_i == y])
//
// Rule 2:
// Input : array_int_element(x0, [c1, .., cn], y) with x0 = a * x + b
// Output: array_int_element(x, [c_a1, .., c_am], b) with a * i = b = ai
//
// Rule 3:
// Input : array_int_element(x, [c1, .., cn], y) with x = a * x1 + x2 + b
// Output: array_int_element([x1, x2], [c_a1, .., c_am], b, [a, b])
//         to be interpreted by the extraction process.
//
// Rule 4:
// Input: array_int_element(x, [c1, .., cn], y)
// Output array_int_element(x, [c1, .., c{std::max(x)}], y)
//
// Rule 5:
// Input : array_int_element(x, [c1, .., cn], y) with x0 ci = c0 + i
// Output: int_lin_eq([-1, 1], [y, x], 1 - c)  (e.g. y = x + c - 1)
//
// Rule 6:
// Input : array_int_element(x, [c1, .., cn], y)
// Output: Remove unreachable values from x.
Presolver::RuleStatus Presolver::PresolveSimplifyElement(Constraint* ct,
                                                         std::string* log) {
  if (ct->arguments[0].variables.size() > 1) {
    return NOT_CHANGED;
  }

  // Rule 1a.
  if (ct->arguments[0].HasOneValue() && ct->arguments[0].Value() >= 1 &&
      ct->arguments[0].Value() <= ct->arguments[1].values.size()) {
    const int64 index = ct->arguments[0].Value() - 1;
    const int64 value = ct->arguments[1].values[index];
    // Rewrite as equality.
    if (ct->arguments[2].HasOneValue()) {
      const int64 target = ct->arguments[2].Value();
      if (value == target) {
        return CONSTRAINT_ALWAYS_TRUE;
      } else {
        return CONSTRAINT_ALWAYS_FALSE;
      }
    } else {
      SetConstraintAsIntEq(ct, ct->arguments[2].Var(), value);
      return CONSTRAINT_REWRITTEN;
    }
    return NOT_CHANGED;
  }

  // Rule 1b.
  if (ct->arguments[2].HasOneValue()) {
    const int64 target_value = ct->arguments[2].Value();

    // Extract the possible indices.
    std::vector<int64> indices;
    const int size = ct->arguments[1].values.size();
    for (int i = 0; i < size; ++i) {
      if (ct->arguments[1].values[i] == target_value) {
        indices.push_back(i + 1);  // 1-based.
      }
    }

    // Rewrite as set_in.
    ct->type = "set_in";
    ct->arguments[1] = Argument::IntegerList(indices);
    ct->RemoveArg(2);
    FZVLOG << "  -> " << ct->DebugString() << FZENDL;
    return CONSTRAINT_REWRITTEN;
  }

  IntegerVariable* const index_var = ct->arguments[0].Var();

  // Rule 2.
  if (ContainsKey(affine_map_, index_var)) {
    const AffineMapping& mapping = affine_map_[index_var];
    const Domain& domain = mapping.variable->domain;
    if (domain.is_interval && domain.values.empty()) {
      // Invalid case. Ignore it.
      return NOT_CHANGED;
    }
    if (domain.values[0] == 0 && mapping.coefficient == 1 &&
        mapping.offset > 1 && index_var->domain.is_interval) {
      log->append("reduce constraint");
      // Simple translation
      const int offset = mapping.offset - 1;
      const int size = ct->arguments[1].values.size();
      for (int i = 0; i < size - offset; ++i) {
        ct->arguments[1].values[i] = ct->arguments[1].values[i + offset];
      }
      ct->arguments[1].values.resize(size - offset);
      affine_map_[index_var].constraint->arguments[2].values[0] = -1;
      affine_map_[index_var].offset = 1;
      index_var->domain.values[0] -= offset;
      index_var->domain.values[1] -= offset;
      MarkChangedVariable(index_var);
      return CONSTRAINT_REWRITTEN;
    } else if (mapping.offset + mapping.coefficient > 0 &&
               domain.values[0] > 0) {
      const std::vector<int64>& values = ct->arguments[1].values;
      std::vector<int64> new_values;
      for (int64 i = 1; i <= domain.values.back(); ++i) {
        const int64 index = i * mapping.coefficient + mapping.offset - 1;
        if (index < 0) {
          return NOT_CHANGED;
        }
        if (index > values.size()) {
          break;
        }
        new_values.push_back(values[index]);
      }
      // Rewrite constraint.
      log->append("simplify constraint");
      ct->arguments[0].variables[0] = mapping.variable;
      IntersectVarWithInterval(ct->arguments[0].variables[0], 1,
                               new_values.size());
      // TODO(user): Encapsulate argument setters.
      ct->arguments[1].values.swap(new_values);
      if (ct->arguments[1].values.size() == 1) {
        ct->arguments[1].type = Argument::INT_VALUE;
      }
      // Reset propagate flag.
      ct->presolve_propagation_done = false;
      // Mark old index var and affine constraint as presolved out.
      mapping.constraint->MarkAsInactive();
      index_var->active = false;
      return CONSTRAINT_REWRITTEN;
    }
  }

  // Rule 3.
  if (ContainsKey(array2d_index_map_, index_var)) {
    log->append("rewrite as a 2d element");
    const Array2DIndexMapping& mapping = array2d_index_map_[index_var];
    // Rewrite constraint.
    ct->arguments[0] =
        Argument::IntVarRefArray({mapping.variable1, mapping.variable2});
    std::vector<int64> coefs;
    coefs.push_back(mapping.coefficient);
    coefs.push_back(1);
    ct->arguments.push_back(Argument::IntegerList(coefs));
    ct->arguments.push_back(Argument::IntegerValue(mapping.offset));
    if (ct->target_variable != nullptr) {
      ct->RemoveTargetVariable();
    }
    index_var->active = false;
    mapping.constraint->MarkAsInactive();
    return CONSTRAINT_REWRITTEN;
  }

  // Rule 4.
  if (index_var->domain.Max() < ct->arguments[1].values.size()) {
    // Reduce array of values.
    ct->arguments[1].values.resize(index_var->domain.Max());
    ct->presolve_propagation_done = false;
    log->append("reduce array");
    return CONSTRAINT_REWRITTEN;
  }

  // Rule 5.
  if (IsIncreasingAndContiguous(ct->arguments[1].values)) {
    const int64 start = ct->arguments[1].values.front();
    IntegerVariable* const index = ct->arguments[0].Var();
    IntegerVariable* const target = ct->arguments[2].Var();
    log->append("linearize constraint");

    if (start == 1) {
      ct->type = "int_eq";
      ct->RemoveArg(1);
    } else {
      // Rewrite constraint into a int_lin_eq
      ct->type = "int_lin_eq";
      ct->arguments[0] = Argument::IntegerList({-1, 1});
      ct->arguments[1] = Argument::IntVarRefArray({target, index});
      ct->arguments[2] = Argument::IntegerValue(1 - start);
    }
    return CONSTRAINT_REWRITTEN;
  }

  // Rule 6.
  if (ct->arguments[0].IsVariable()) {
    std::unordered_set<int64> all_values = GetValueSet(ct->arguments[2]);
    const std::vector<int64>& array = ct->arguments[1].values;
    const int array_size = array.size();
    if (!all_values.empty()) {
      const Domain& domain = ct->arguments[0].Var()->domain;
      std::vector<int64> to_keep;
      bool remove_some = false;
      if (domain.is_interval) {
        for (int64 v = std::max<int64>(1, domain.values[0]);
             v <= std::min<int64>(array_size, domain.values[1]); ++v) {
          const int64 value = array[v - 1];
          if (ContainsKey(all_values, value)) {
            to_keep.push_back(v);
          } else {
            remove_some = true;
          }
        }
      } else {
        for (int64 v : domain.values) {
          // We have not yet reduced the domain of the index.
          // TODO(user): Reorder presolve rules to propagate domain
          // of validity first.
          if (v < 1 || v > array_size) {
            remove_some = true;
          } else {
            const int64 value = array[v - 1];
            if (!ContainsKey(all_values, value)) {
              remove_some = true;
            } else {
              to_keep.push_back(v);
            }
          }
        }
      }
      if (remove_some) {
        if (ct->arguments[0].Var()->domain.IntersectWithListOfIntegers(
                to_keep)) {
          MarkChangedVariable(ct->arguments[0].Var());
        }
        log->append(
            StringPrintf("reduce index domain to %s",
                         ct->arguments[0].Var()->DebugString().c_str()));
      }
    }
  }

  return NOT_CHANGED;
}

// Simplifies array_var_int_element
//
// Rule1:
// Input : array_var_int_element(x0, [x1, .., xn], y) with xi(1..n) having one
//         value
// Output: array_int_element(x0, [x1.Value(), .., xn.Value()], y)
//
// Rule2:
// Input : array_var_int_element(x0, [x1, .., xn], y) with x0 fixed
// Output: equality betwen x_x0 and y.
//
// Rule3:
// Input : array_var_int_element(x0, [x1, .., xn], y) with x0 = a * x + b
// Output: array_var_int_element(x, [x_a1, .., x_an], b) with a * i = b = ai
//
// Rule4:
// Input : array_var_int_element(x0, [x1, .., xn], y)
// Output: remove from the domain of x0 the value for which whe know xi != y
Presolver::RuleStatus Presolver::PresolveSimplifyExprElement(Constraint* ct,
                                                             std::string* log) {
  // Rule 1.
  bool all_fixed = true;
  for (IntegerVariable* const var : ct->arguments[1].variables) {
    if (!var->domain.HasOneValue()) {
      all_fixed = false;
      break;
    }
  }
  if (all_fixed) {
    log->append("rewrite constraint as array_int_element");
    ct->type = "array_int_element";
    ct->arguments[1].type = Argument::INT_LIST;
    for (int i = 0; i < ct->arguments[1].variables.size(); ++i) {
      ct->arguments[1].values.push_back(
          ct->arguments[1].variables[i]->domain.Min());
    }
    ct->arguments[1].variables.clear();
    return CONSTRAINT_REWRITTEN;
  }

  // Rule 2.
  if (ct->arguments[0].HasOneValue()) {
    // Index is fixed, rewrite constraint into an equality.
    const int64 index = ct->arguments[0].Value() - 1;  // 1 based.
    log->append("simplify element as one index is constant");
    ct->type = "int_eq";
    ct->arguments[0] = Argument::IntVarRef(ct->arguments[1].variables[index]);
    ct->RemoveArg(1);
    return CONSTRAINT_REWRITTEN;
  }

  // Rule 3.
  IntegerVariable* const index_var = ct->arguments[0].Var();
  if (ContainsKey(affine_map_, index_var)) {
    const AffineMapping& mapping = affine_map_[index_var];
    const Domain& domain = mapping.variable->domain;
    if ((domain.is_interval && domain.values.empty()) ||
        domain.values[0] != 1 || mapping.offset + mapping.coefficient <= 0) {
      // Invalid case. Ignore it.
      return NOT_CHANGED;
    }
    const std::vector<IntegerVariable*>& vars = ct->arguments[1].variables;
    std::vector<IntegerVariable*> new_vars;
    for (int64 i = domain.values.front(); i <= domain.values.back(); ++i) {
      const int64 index = i * mapping.coefficient + mapping.offset - 1;
      if (index < 0) {
        return NOT_CHANGED;
      }
      if (index >= vars.size()) {
        break;
      }
      new_vars.push_back(vars[index]);
    }
    // Rewrite constraint.
    log->append("simplify constraint");
    ct->arguments[0].variables[0] = mapping.variable;
    // TODO(user): Encapsulate argument setters.
    ct->arguments[1].variables.swap(new_vars);
    // Reset propagate flag.
    ct->presolve_propagation_done = false;
    // Mark old index var and affine constraint as presolved out.
    mapping.constraint->MarkAsInactive();
    index_var->active = false;
    return CONSTRAINT_REWRITTEN;
  }
  if (index_var->domain.is_interval && index_var->domain.values.size() == 2 &&
      index_var->domain.Max() < ct->arguments[1].variables.size()) {
    // Reduce array of variables.
    ct->arguments[1].variables.resize(index_var->domain.Max());
    ct->presolve_propagation_done = false;
    log->append("reduce array");
    return CONSTRAINT_REWRITTEN;
  }

  // Rule 4.
  if (ct->arguments[0].IsVariable()) {
    const Domain& domain = ct->arguments[0].Var()->domain;
    std::vector<int64> to_keep;
    const int array_size = ct->arguments[1].variables.size();
    bool remove_some = false;
    if (domain.is_interval) {
      for (int64 v = std::max<int64>(1, domain.values[0]);
           v <= std::min<int64>(array_size, domain.values[1]); ++v) {
        if (OverlapsAt(ct->arguments[1], v - 1, ct->arguments[2])) {
          to_keep.push_back(v);
        } else {
          remove_some = true;
        }
      }
    } else {
      for (int64 v : domain.values) {
        if (v < 1 || v > array_size) {
          remove_some = true;
        } else {
          if (OverlapsAt(ct->arguments[1], v - 1, ct->arguments[2])) {
            to_keep.push_back(v);
          } else {
            remove_some = true;
          }
        }
      }
    }
    if (remove_some) {
      if (ct->arguments[0].Var()->domain.IntersectWithListOfIntegers(to_keep)) {
        MarkChangedVariable(ct->arguments[0].Var());
      }
      log->append(StringPrintf("reduce index domain to %s",
                               ct->arguments[0].Var()->DebugString().c_str()));
    }
  }

  return NOT_CHANGED;
}

// Propagate reified comparison: int_eq_reif, int_ge_reif, int_le_reif:
//
// Rule1:
// Input : int_xx_reif(x, x, b) or bool_eq_reif(b1, b1, b)
// Action: Set b to true if xx in {le, ge, eq}, or false otherwise.
// Output: inactive constraint.
//
// Rule 2:
// Input: int_eq_reif(b1, c, b0) or bool_eq_reif(b1, c, b0)
//        or int_eq_reif(c, b1, b0) or bool_eq_reif(c, b1, b0)
// Output: bool_eq(b1, b0) or bool_not(b1, b0) depending on the parity.
//
// Rule 3:
// Input : int_xx_reif(x, c, b) or bool_xx_reif(b1, t, b) or
//         int_xx_reif(c, x, b) or bool_xx_reif(t, b2, b)
// Action: Assign b to true or false if this can be decided from the of x and
//         c, or the comparison of b1/b2 with t.
// Output: inactive constraint of b was assigned a value.
//
// Rule 4:
// Input : int_xx_reif(x, y, b) or bool_xx_reif(b1, b1, b2).
// Action: Assign b to true or false if this can be decided from the domain of
//         x and y.
// Output: inactive constraint if b was assigned a value.
Presolver::RuleStatus Presolver::PropagateReifiedComparisons(Constraint* ct,
                                                             std::string* log) {
  const std::string& id = ct->type;
  if (ct->arguments[0].IsVariable() && ct->arguments[1].IsVariable() &&
      ct->arguments[0].variables[0] == ct->arguments[1].variables[0]) {
    // Rule 1.
    const bool value =
        (id == "int_eq_reif" || id == "int_ge_reif" || id == "int_le_reif" ||
         id == "bool_eq_reif" || id == "bool_ge_reif" || id == "bool_le_reif");
    if ((ct->arguments[2].HasOneValue() &&
         ct->arguments[2].Value() == static_cast<int64>(value)) ||
        !ct->arguments[2].HasOneValue()) {
      log->append("propagate boolvar to value");
      IntersectVarWithSingleton(ct->arguments[2].Var(), value);
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }

  // Rule 3, easy case. Both constants.
  if (ct->arguments[0].HasOneValue() && ct->arguments[1].HasOneValue()) {
    const int64 a = ct->arguments[0].Value();
    const int64 b = ct->arguments[1].Value();
    int state = 2;  // 0 force_false, 1 force true, 2 unknown.
    if (id == "int_eq_reif" || id == "bool_eq_reif") {
      state = (a == b);
    } else if (id == "int_ne_reif" || id == "bool_ne_reif") {
      state = (a != b);
    } else if (id == "int_lt_reif" || id == "bool_lt_reif") {
      state = (a < b);
    } else if (id == "int_gt_reif" || id == "bool_gt_reif") {
      state = (a > b);
    } else if (id == "int_le_reif" || id == "bool_le_reif") {
      state = (a <= b);
    } else if (id == "int_ge_reif" || id == "bool_ge_reif") {
      state = (a >= b);
    }
    if (state != 2) {
      StringAppendF(log, "assign boolvar to %s", state == 0 ? "false" : "true");
      IntersectVarWithSingleton(ct->arguments[2].Var(), state);
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }

  IntegerVariable* var = nullptr;
  int64 value = 0;
  bool reverse = false;
  if (ct->arguments[0].IsVariable() && ct->arguments[1].HasOneValue()) {
    var = ct->arguments[0].Var();
    value = ct->arguments[1].Value();
  } else if (ct->arguments[1].IsVariable() && ct->arguments[0].HasOneValue()) {
    var = ct->arguments[1].Var();
    value = ct->arguments[0].Value();
    reverse = true;
  }
  if (var != nullptr) {
    if (Has01Values(var) && (id == "int_eq_reif" || id == "int_ne_reif" ||
                             id == "bool_eq_reif" || id == "bool_ne_reif") &&
        (value == 0 || value == 1)) {
      // Rule 2.
      bool parity = (id == "int_eq_reif" || id == "bool_eq_reif");
      if (value == 0) {
        parity = !parity;
      }
      log->append("simplify constraint");
      Argument target = ct->arguments[2];
      ct->arguments.clear();
      ct->arguments.push_back(Argument::IntVarRef(var));
      ct->arguments.push_back(target);
      ct->type = parity ? "bool_eq" : "bool_not";
      return CONSTRAINT_REWRITTEN;
    } else {
      // Rule 3.
      int state = 2;  // 0 force_false, 1 force true, 2 unknown.
      if (id == "int_eq_reif" || id == "bool_eq_reif") {
        if (var->domain.Contains(value)) {
          if (var->domain.HasOneValue()) {
            state = 1;
          }
        } else {
          state = 0;
        }
      } else if (id == "int_ne_reif" || id == "bool_ne_reif") {
        if (var->domain.Contains(value)) {
          if (var->domain.HasOneValue()) {
            state = 0;
          }
        } else {
          state = 1;
        }
      } else if ((((id == "int_lt_reif" || id == "bool_lt_reif") && reverse) ||
                  ((id == "int_gt_reif" || id == "bool_gt_reif") &&
                   !reverse)) &&
                 !var->domain.IsAllInt64()) {  // int_gt
        if (var->domain.Min() > value) {
          state = 1;
        } else if (var->domain.Max() <= value) {
          state = 0;
        }
      } else if ((((id == "int_lt_reif" || id == "bool_lt_reif") && !reverse) ||
                  ((id == "int_gt_reif" || id == "bool_gt_reif") && reverse)) &&
                 !var->domain.IsAllInt64()) {  // int_lt
        if (var->domain.Max() < value) {
          state = 1;
        } else if (var->domain.Min() >= value) {
          state = 0;
        }
      } else if ((((id == "int_le_reif" || id == "bool_le_reif") && reverse) ||
                  ((id == "int_ge_reif" || id == "bool_ge_reif") &&
                   !reverse)) &&
                 !var->domain.IsAllInt64()) {  // int_ge
        if (var->domain.Min() >= value) {
          state = 1;
        } else if (var->domain.Max() < value) {
          state = 0;
        }
      } else if ((((id == "int_le_reif" || id == "bool_le_reif") && !reverse) ||
                  ((id == "int_ge_reif" || id == "bool_ge_reif") && reverse)) &&
                 !var->domain.IsAllInt64()) {  // int_le
        if (var->domain.Max() <= value) {
          state = 1;
        } else if (var->domain.Min() > value) {
          state = 0;
        }
      }
      if (state != 2) {
        StringAppendF(log, "assign boolvar to %s",
                      state == 0 ? "false" : "true");
        IntersectVarWithSingleton(ct->arguments[2].Var(), state);
        return CONSTRAINT_ALWAYS_TRUE;
      }
    }
  }

  // Rule 4.
  if (!ct->arguments[0].HasOneValue() && !ct->arguments[1].HasOneValue()) {
    const Domain& ld = ct->arguments[0].Var()->domain;
    const Domain& rd = ct->arguments[1].Var()->domain;
    int state = 2;  // 0 force_false, 1 force true, 2 unknown.
    if (id == "int_eq_reif" || id == "bool_eq_reif") {
      if (!ld.OverlapsDomain(rd)) {
        state = 0;
      }
    } else if (id == "int_ne_reif" || id == "bool_ne_reif") {
      // TODO(user): Test if the domain are disjoint.
      if (ld.Min() > rd.Max() || ld.Max() < rd.Min()) {
        state = 1;
      }
    } else if (id == "int_lt_reif" || id == "bool_lt_reif") {
      if (ld.Max() < rd.Min()) {
        state = 1;
      } else if (ld.Min() >= rd.Max()) {
        state = 0;
      }
    } else if (id == "int_gt_reif" || id == "bool_gt_reif") {
      if (ld.Max() <= rd.Min()) {
        state = 0;
      } else if (ld.Min() > rd.Max()) {
        state = 1;
      }
    } else if (id == "int_le_reif" || id == "bool_le_reif") {
      if (ld.Max() <= rd.Min()) {
        state = 1;
      } else if (ld.Min() > rd.Max()) {
        state = 0;
      }
    } else if (id == "int_ge_reif" || id == "bool_ge_reif") {
      if (ld.Max() < rd.Min()) {
        state = 0;
      } else if (ld.Min() >= rd.Max()) {
        state = 1;
      }
    }
    if (state != 2) {
      StringAppendF(log, "assign boolvar to %s", state == 0 ? "false" : "true");
      IntersectVarWithSingleton(ct->arguments[2].Var(), state);
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }
  return NOT_CHANGED;
}

// Stores the existence of int_eq_reif(x, y, b)
Presolver::RuleStatus Presolver::StoreIntEqReif(Constraint* ct, std::string* log) {
  if (ct->arguments[0].IsVariable() && ct->arguments[1].IsVariable() &&
      ct->arguments[2].IsVariable()) {
    IntegerVariable* const first = ct->arguments[0].Var();
    IntegerVariable* const second = ct->arguments[1].Var();
    IntegerVariable* const boolvar = ct->arguments[2].Var();
    if (ContainsKey(int_eq_reif_map_, first) &&
        ContainsKey(int_eq_reif_map_[first], second)) {
      return NOT_CHANGED;
    }
    log->append("store eq_var info");
    int_eq_reif_map_[first][second] = boolvar;
    int_eq_reif_map_[second][first] = boolvar;
    MarkChangedVariable(first);
    MarkChangedVariable(second);
    return CONTEXT_CHANGED;
  }
  return NOT_CHANGED;
}

// Merge symmetrical int_eq_reif and int_ne_reif
// Input: int_eq_reif(x, y, b1) && int_ne_reif(x, y, b2)
// Output: int_eq_reif(x, y, b1) && bool_not(b1, b2)
Presolver::RuleStatus Presolver::SimplifyIntNeReif(Constraint* ct,
                                                   std::string* log) {
  if (ct->arguments[0].IsVariable() && ct->arguments[1].IsVariable() &&
      ct->arguments[2].IsVariable() &&
      ContainsKey(int_eq_reif_map_, ct->arguments[0].Var()) &&
      ContainsKey(int_eq_reif_map_[ct->arguments[0].Var()],
                  ct->arguments[1].Var())) {
    log->append("merge constraint with opposite constraint");
    IntegerVariable* const opposite_boolvar =
        int_eq_reif_map_[ct->arguments[0].Var()][ct->arguments[1].Var()];
    ct->arguments[0] = Argument::IntVarRef(opposite_boolvar);
    ct->arguments[1] = Argument::IntVarRef(ct->arguments[2].Var());
    ct->RemoveArg(2);
    ct->type = "bool_not";
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
}

// Store the mapping x = abs(y) for future use.
Presolver::RuleStatus Presolver::StoreAbs(Constraint* ct, std::string* log) {
  if (!ContainsKey(abs_map_, ct->arguments[1].Var())) {
    // Stores abs() map.
    log->append("Store abs map");
    abs_map_[ct->arguments[1].Var()] = ct->arguments[0].Var();
    MarkChangedVariable(ct->arguments[1].Var());
    return CONTEXT_CHANGED;
  }
  return NOT_CHANGED;
}

// Remove abs from int_le_reif.
// Input : int_le_reif(x, 0, b) or int_le_reif(x,c, b) with x == abs(y)
// Output: int_eq_reif(y, 0, b) or set_in_reif(y, [-c, c], b)
Presolver::RuleStatus Presolver::RemoveAbsFromIntLeReif(Constraint* ct,
                                                        std::string* log) {
  if (ct->arguments[1].HasOneValue() &&
      ContainsKey(abs_map_, ct->arguments[0].Var())) {
    log->append("remove abs from constraint");
    ct->arguments[0].variables[0] = abs_map_[ct->arguments[0].Var()];
    const int64 value = ct->arguments[1].Value();
    if (value == 0) {
      ct->type = "int_eq_reif";
      return CONSTRAINT_REWRITTEN;
    } else {
      ct->type = "set_in_reif";
      ct->arguments[1] = Argument::Interval(-value, value);
      // set_in_reif does not implement reification.
      ct->RemoveTargetVariable();
      return CONSTRAINT_REWRITTEN;
    }
  }
  return NOT_CHANGED;
}

// Simplifies int_eq and int_ne[_reif] with abs
// Input : int_eq(x, 0) or int_ne(x, 0) with x == abs(y)
// Output: int_eq(y, 0) or int_ne(y, 0)
Presolver::RuleStatus Presolver::RemoveAbsFromIntEqNeReif(Constraint* ct,
                                                          std::string* log) {
  if (ct->arguments[1].HasOneValue() && ct->arguments[1].Value() == 0 &&
      ContainsKey(abs_map_, ct->arguments[0].Var())) {
    log->append("remove abs from constraint");
    ct->arguments[0].variables[0] = abs_map_[ct->arguments[0].Var()];
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
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
Presolver::RuleStatus Presolver::PresolveBoolXor(Constraint* ct, std::string* log) {
  if (ct->arguments[0].HasOneValue()) {
    // Rule 1.
    const int64 value = ct->arguments[0].Value();
    log->append("simplify constraint");
    ct->RemoveArg(0);
    ct->type = value == 1 ? "bool_not" : "bool_eq";
    return CONSTRAINT_REWRITTEN;
  }
  if (ct->arguments[1].HasOneValue()) {
    // Rule 2.
    const int64 value = ct->arguments[1].Value();
    log->append("simplify constraint");
    ct->RemoveArg(1);
    ct->type = value == 1 ? "bool_not" : "bool_eq";
    return CONSTRAINT_REWRITTEN;
  }
  if (ct->arguments[2].HasOneValue()) {
    // Rule 3.
    const int64 value = ct->arguments[2].Value();
    log->append("simplify constraint");
    ct->RemoveArg(2);
    ct->type = value == 1 ? "bool_not" : "bool_eq";
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
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
//
// Rule 5:
// Input : bool_not(c1, c2) (2 boolean constants)
// Output: inactive constraint or false constraint.
Presolver::RuleStatus Presolver::PresolveBoolNot(Constraint* ct, std::string* log) {
  if (ct->arguments[0].variables.empty() &&
      ct->arguments[1].variables.empty()) {
    return (ct->arguments[0].Value() != ct->arguments[1].Value())
               ? CONSTRAINT_ALWAYS_TRUE
               : CONSTRAINT_ALWAYS_FALSE;
  }
  if (ct->arguments[0].HasOneValue() && ct->arguments[1].IsVariable()) {
    const int64 value = ct->arguments[0].Value() == 0;
    log->append("propagate constants");
    IntersectVarWithSingleton(ct->arguments[1].Var(), value);
    return CONSTRAINT_ALWAYS_TRUE;
  } else if (ct->arguments[1].HasOneValue() && ct->arguments[0].IsVariable()) {
    const int64 value = ct->arguments[1].Value() == 0;
    log->append("propagate constants");
    IntersectVarWithSingleton(ct->arguments[0].Var(), value);
    return CONSTRAINT_ALWAYS_TRUE;
  } else if (ct->target_variable == nullptr &&
             ct->arguments[0].Var()->defining_constraint == nullptr &&
             !ct->arguments[0].Var()->domain.HasOneValue()) {
    log->append("set target variable");
    IntegerVariable* const var = ct->arguments[0].Var();
    ct->target_variable = var;
    var->defining_constraint = ct;
    MarkChangedVariable(var);
    return CONSTRAINT_REWRITTEN;
  } else if (ct->target_variable == nullptr &&
             ct->arguments[1].Var()->defining_constraint == nullptr &&
             !ct->arguments[1].Var()->domain.HasOneValue()) {
    log->append("set target variable");
    IntegerVariable* const var = ct->arguments[1].Var();
    ct->target_variable = var;
    var->defining_constraint = ct;
    MarkChangedVariable(var);
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
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
//
// Rule 4:
// Input: bool_clause([b1, .., bn][B1, .., Bm])
// Output: - remove all the bi fixed to false.
//         - if one of the bi is true, mark as inactive.
//         - remove all the Bi fixed to true.
//         - if one of the Bi is false, mark as inactive.
Presolver::RuleStatus Presolver::PresolveBoolClause(Constraint* ct,
                                                    std::string* log) {
  // Rule 1.
  if (ct->arguments[0].variables.size() == 1 &&
      ct->arguments[1].variables.size() == 1) {
    log->append("simplify constraint");
    std::swap(ct->arguments[0].variables[0], ct->arguments[1].variables[0]);
    ct->arguments[0].type = Argument::INT_VAR_REF;
    ct->arguments[1].type = Argument::INT_VAR_REF;
    ct->type = "bool_le";
    return CONSTRAINT_REWRITTEN;
  }
  // Rule 2.
  if (ct->arguments[0].variables.empty() &&
      ct->arguments[0].values.size() == 1 &&
      ct->arguments[1].variables.size() == 1) {
    log->append("simplify constraint");
    const int64 value = ct->arguments[0].values.front();
    if (value) {
      return CONSTRAINT_ALWAYS_TRUE;
    } else {
      ct->arguments[0] = Argument::IntVarRef(ct->arguments[1].Var());
      ct->arguments[1] = Argument::IntegerValue(0);
      ct->type = "bool_eq";
      return CONSTRAINT_REWRITTEN;
    }
  }
  // Rule 3.
  if (ct->arguments[1].HasOneValue()) {
    log->append("simplify constraint");
    if (ct->arguments[1].Value()) {
      if (ct->arguments[0].variables.size() > 1) {
        ct->type = "array_bool_or";
        return CONSTRAINT_REWRITTEN;
      } else if (ct->arguments[0].variables.size() == 1) {
        ct->arguments[0].type = Argument::INT_VAR_REF;
        ct->arguments[1].type = Argument::INT_VALUE;
        ct->type = "bool_eq";
        return CONSTRAINT_REWRITTEN;
      }
    } else {
      return CONSTRAINT_ALWAYS_TRUE;
    }
  }

  // Rule 4 (part 1).
  if (!ct->arguments[0].variables.empty()) {
    std::vector<IntegerVariable*> new_vars;
    for (IntegerVariable* var : ct->arguments[0].variables) {
      if (var->domain.HasOneValue()) {
        if (var->domain.Value() == 1) {
          return CONSTRAINT_ALWAYS_TRUE;
        }
      } else {
        new_vars.push_back(var);
      }
    }
    if (new_vars.size() < ct->arguments[0].variables.size()) {
      ct->arguments[0].variables.swap(new_vars);
      return CONSTRAINT_REWRITTEN;
    }
  }

  // Rule 4 (part 2).
  if (!ct->arguments[1].variables.empty()) {
    std::vector<IntegerVariable*> new_vars;
    for (IntegerVariable* var : ct->arguments[1].variables) {
      if (var->domain.HasOneValue()) {
        if (var->domain.Value() == 0) {
          return CONSTRAINT_ALWAYS_TRUE;
        }
      } else {
        new_vars.push_back(var);
      }
    }
    if (new_vars.size() < ct->arguments[1].variables.size()) {
      ct->arguments[1].variables.swap(new_vars);
      return CONSTRAINT_REWRITTEN;
    }
  }
  return NOT_CHANGED;
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
Presolver::RuleStatus Presolver::SimplifyIntLinEqReif(Constraint* ct,
                                                      std::string* log) {
  if (ct->arguments[0].values.size() == 2 && ct->arguments[0].values[0] == 1 &&
      ct->arguments[0].values[1] == 1 && ct->arguments[2].Value() == 1) {
    IntegerVariable* const left = ct->arguments[1].variables[0];
    IntegerVariable* const right = ct->arguments[1].variables[1];
    IntegerVariable* const target = ct->arguments[3].Var();
    if (Has01Values(ct->arguments[1].variables[0]) &&
        Has01Values(ct->arguments[1].variables[1])) {
      // Rule 1.
      log->append("rewrite constraint to bool_ne_reif");
      ct->type = "bool_ne_reif";
      ct->arguments[0] = Argument::IntVarRef(left);
      ct->arguments[1] = Argument::IntVarRef(right);
      ct->arguments[2] = Argument::IntVarRef(target);
      ct->RemoveArg(3);
      return CONSTRAINT_REWRITTEN;
    }
    if (Has01Values(right) && left->domain.HasOneValue() &&
        Is0Or1(left->domain.Min())) {
      if (left->domain.Min() == 0) {
        // Rule 2.
        log->append("rewrite constraint to bool_eq");
        ct->type = "bool_eq";
        ct->arguments[0] = Argument::IntVarRef(right);
        ct->arguments[1] = Argument::IntVarRef(target);
        ct->RemoveArg(3);
        ct->RemoveArg(2);
        return CONSTRAINT_REWRITTEN;
      } else {
        // Rule 3.
        log->append("rewrite constraint to bool_not");
        ct->type = "bool_not";
        ct->arguments[0] = Argument::IntVarRef(right);
        ct->arguments[1] = Argument::IntVarRef(target);
        ct->RemoveArg(3);
        ct->RemoveArg(2);
        return CONSTRAINT_REWRITTEN;
      }
    } else if (Has01Values(left) && right->domain.HasOneValue() &&
               Is0Or1(right->domain.Min())) {
      if (right->domain.Min() == 0) {
        // Rule 4.
        log->append("rewrite constraint to bool_eq");
        ct->type = "bool_eq";
        ct->arguments[0] = Argument::IntVarRef(left);
        ct->arguments[1] = Argument::IntVarRef(target);
        ct->RemoveArg(3);
        ct->RemoveArg(2);
        return CONSTRAINT_REWRITTEN;
      } else {
        // Rule 5.
        log->append("rewrite constraint to bool_not");
        ct->type = "bool_not";
        ct->arguments[0] = Argument::IntVarRef(left);
        ct->arguments[1] = Argument::IntVarRef(target);
        ct->RemoveArg(3);
        ct->RemoveArg(2);
        return CONSTRAINT_REWRITTEN;
      }
    }
  }
  return NOT_CHANGED;
}

// Remove target variable from int_mod if bound.
//
// Input : int_mod(x1, x2, x3)  => x3
// Output: int_mod(x1, x2, x3) if x3 has only one value.
Presolver::RuleStatus Presolver::PresolveIntMod(Constraint* ct, std::string* log) {
  if (ct->target_variable != nullptr &&
      ct->arguments[2].Var() == ct->target_variable &&
      ct->arguments[2].HasOneValue()) {
    MarkChangedVariable(ct->target_variable);
    ct->target_variable->defining_constraint = nullptr;
    ct->target_variable = nullptr;
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
}

// Remove invalid tuples, remove unreached values from domain variables.
//
Presolver::RuleStatus Presolver::PresolveTableInt(Constraint* ct, std::string* log) {
  if (ct->arguments[0].variables.empty()) {
    return NOT_CHANGED;
  }
  const int num_vars = ct->arguments[0].variables.size();
  CHECK_EQ(0, ct->arguments[1].values.size() % num_vars);
  const int num_tuples = ct->arguments[1].values.size() / num_vars;
  int ignored_tuples = 0;
  std::vector<int64> new_tuples;
  std::vector<std::unordered_set<int64>> next_values(num_vars);
  for (int t = 0; t < num_tuples; ++t) {
    std::vector<int64> tuple(
        ct->arguments[1].values.begin() + t * num_vars,
        ct->arguments[1].values.begin() + (t + 1) * num_vars);
    bool valid = true;
    for (int i = 0; i < num_vars; ++i) {
      if (!ct->arguments[0].variables[i]->domain.Contains(tuple[i])) {
        valid = false;
        break;
      }
    }
    if (valid) {
      for (int i = 0; i < num_vars; ++i) {
        next_values[i].insert(tuple[i]);
      }
      new_tuples.insert(new_tuples.end(), tuple.begin(), tuple.end());
    } else {
      ignored_tuples++;
    }
  }
  // Reduce variables domains.
  for (int var_index = 0; var_index < num_vars; ++var_index) {
    IntegerVariable* const var = ct->arguments[0].variables[var_index];
    // Store domain info to detect change.
    std::vector<int64> values(next_values[var_index].begin(),
                              next_values[var_index].end());
    if (var->domain.IntersectWithListOfIntegers(values)) {
      MarkChangedVariable(var);
    }
  }
  // Removed invalid tuples.
  if (ignored_tuples > 0) {
    log->append(StringPrintf("removed %i tuples", ignored_tuples));
    ct->arguments[1].values.swap(new_tuples);
    return CONSTRAINT_REWRITTEN;
  }

  return NOT_CHANGED;
}

Presolver::RuleStatus Presolver::PresolveRegular(Constraint* ct, std::string* log) {
  const std::vector<IntegerVariable*> vars = ct->arguments[0].variables;
  if (vars.empty()) {
    // TODO(user): presolve when all variables are instantiated.
    return NOT_CHANGED;
  }
  const int num_vars = vars.size();

  const int64 num_states = ct->arguments[1].Value();
  const int64 num_values = ct->arguments[2].Value();

  // Read transitions.
  const std::vector<int64>& array_transitions = ct->arguments[3].values;
  std::vector<std::vector<int64>> automata;
  int count = 0;
  for (int i = 1; i <= num_states; ++i) {
    for (int j = 1; j <= num_values; ++j) {
      automata.push_back({i, j, array_transitions[count++]});
    }
  }

  const int64 initial_state = ct->arguments[4].Value();

  std::unordered_set<int64> final_states;
  switch (ct->arguments[5].type) {
    case fz::Argument::INT_VALUE: {
      final_states.insert(ct->arguments[5].values[0]);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      for (int64 v = ct->arguments[5].values[0];
           v <= ct->arguments[5].values[1]; ++v) {
        final_states.insert(v);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      for (int64 value : ct->arguments[5].values) {
        final_states.insert(value);
      }
      break;
    }
    default: { LOG(FATAL) << "Wrong constraint " << ct->DebugString(); }
  }

  // Compute the set of reachable state at each time point.
  std::vector<std::unordered_set<int64>> reachable_states(num_vars + 1);
  reachable_states[0].insert(initial_state);
  reachable_states[num_vars] = {final_states.begin(), final_states.end()};

  // Forward.
  for (int time = 0; time + 1 < num_vars; ++time) {
    const Domain& domain = vars[time]->domain;
    for (const std::vector<int64>& transition : automata) {
      if (!ContainsKey(reachable_states[time], transition[0])) continue;
      if (!domain.Contains(transition[1])) continue;
      reachable_states[time + 1].insert(transition[2]);
    }
  }

  // Backward.
  for (int time = num_vars - 1; time > 0; --time) {
    std::unordered_set<int64> new_set;
    const Domain& domain = vars[time]->domain;
    for (const std::vector<int64>& transition : automata) {
      if (!ContainsKey(reachable_states[time], transition[0])) continue;
      if (!domain.Contains(transition[1])) continue;
      if (!ContainsKey(reachable_states[time + 1], transition[2])) continue;
      new_set.insert(transition[0]);
    }
    reachable_states[time].swap(new_set);
  }

  // Prune the variables.
  for (int time = 0; time < num_vars; ++time) {
    Domain& domain = vars[time]->domain;
    // Collect valid values.
    std::unordered_set<int64> reached_values;
    for (const std::vector<int64>& transition : automata) {
      if (!ContainsKey(reachable_states[time], transition[0])) continue;
      if (!domain.Contains(transition[1])) continue;
      if (!ContainsKey(reachable_states[time + 1], transition[2])) continue;
      reached_values.insert(transition[1]);
    }
    // Scan domain to check if we will remove values.
    std::vector<int64> to_keep;
    bool remove_some = false;
    if (domain.is_interval) {
      for (int64 v = domain.values[0]; v <= domain.values[1]; ++v) {
        if (ContainsKey(reached_values, v)) {
          to_keep.push_back(v);
        } else {
          remove_some = true;
        }
      }
    } else {
      for (int64 v : domain.values) {
        if (ContainsKey(reached_values, v)) {
          to_keep.push_back(v);
        } else {
          remove_some = true;
        }
      }
    }
    if (remove_some) {
      const std::string& before = HASVLOG ? vars[time]->DebugString() : "";
      domain.IntersectWithListOfIntegers(to_keep);
      MarkChangedVariable(vars[time]);
      if (HASVLOG) {
        StringAppendF(log, "reduce domain of variable %d from %s to %s; ", time,
                      before.c_str(), vars[time]->DebugString().c_str());
      }
    }
  }
  return NOT_CHANGED;
}

// Tranforms diffn into all_different_int when sizes and y positions are all 1.
//
// Input : diffn([x1, .. xn], [1, .., 1], [1, .., 1], [1, .., 1])
// Output: all_different_int([x1, .. xn])
Presolver::RuleStatus Presolver::PresolveDiffN(Constraint* ct, std::string* log) {
  const int size = ct->arguments[0].variables.size();
  if (size > 0 && ct->arguments[1].IsArrayOfValues() &&
      ct->arguments[2].IsArrayOfValues() &&
      ct->arguments[3].IsArrayOfValues()) {
    for (int i = 0; i < size; ++i) {
      if (ct->arguments[1].ValueAt(i) != 1) {
        return NOT_CHANGED;
      }
    }
    for (int i = 0; i < size; ++i) {
      if (ct->arguments[2].ValueAt(i) != 1) {
        return NOT_CHANGED;
      }
    }
    for (int i = 0; i < size; ++i) {
      if (ct->arguments[3].ValueAt(i) != 1) {
        return NOT_CHANGED;
      }
    }
    ct->type = "all_different_int";
    ct->arguments.resize(1);
    return CONSTRAINT_REWRITTEN;
  }
  return NOT_CHANGED;
}

#define CALL_TYPE(ct, t, method)                                 \
  if (ct->active && ct->type == t) {                             \
    ApplyRule(ct, #method, [this](Constraint* ct, std::string* log) { \
      return method(ct, log);                                    \
    });                                                          \
  }
#define CALL_PREFIX(ct, t, method)                               \
  if (ct->active && strings::StartsWith(ct->type, t)) {          \
    ApplyRule(ct, #method, [this](Constraint* ct, std::string* log) { \
      return method(ct, log);                                    \
    });                                                          \
  }
#define CALL_SUFFIX(ct, t, method)                               \
  if (ct->active && strings::EndsWith(ct->type, t)) {            \
    ApplyRule(ct, #method, [this](Constraint* ct, std::string* log) { \
      return method(ct, log);                                    \
    });                                                          \
  }

// Main presolve rule caller.
void Presolver::PresolveOneConstraint(Constraint* ct) {
  CALL_SUFFIX(ct, "_reif", Unreify);
  CALL_TYPE(ct, "bool2int", PresolveBool2Int);
  if (strings::StartsWith(ct->type, "int_")) {
    CALL_TYPE(ct, "int_le", PresolveInequalities);
    CALL_TYPE(ct, "int_lt", PresolveInequalities);
    CALL_TYPE(ct, "int_ge", PresolveInequalities);
    CALL_TYPE(ct, "int_gt", PresolveInequalities);
  }
  if (strings::StartsWith(ct->type, "bool_")) {
    CALL_TYPE(ct, "bool_le", PresolveInequalities);
    CALL_TYPE(ct, "bool_lt", PresolveInequalities);
    CALL_TYPE(ct, "bool_ge", PresolveInequalities);
    CALL_TYPE(ct, "bool_gt", PresolveInequalities);
  }

  CALL_TYPE(ct, "int_abs", StoreAbs);
  CALL_TYPE(ct, "int_eq_reif", StoreIntEqReif);
  CALL_TYPE(ct, "int_ne_reif", SimplifyIntNeReif);
  CALL_TYPE(ct, "int_eq_reif", RemoveAbsFromIntEqNeReif);
  CALL_TYPE(ct, "int_ne", RemoveAbsFromIntEqNeReif);
  CALL_TYPE(ct, "int_ne_reif", RemoveAbsFromIntEqNeReif);
  CALL_TYPE(ct, "set_in", PresolveSetIn);
  CALL_TYPE(ct, "set_not_in", PresolveSetNotIn);
  CALL_TYPE(ct, "set_in_reif", PresolveSetInReif);

  if (strings::StartsWith(ct->type, "int_lin_")) {
    CALL_TYPE(ct, "int_lin_gt", PresolveIntLinGt);
    CALL_TYPE(ct, "int_lin_lt", PresolveIntLinLt);
    CALL_PREFIX(ct, "int_lin_", SimplifyLinear);
    CALL_PREFIX(ct, "int_lin_", PresolveLinear);
    CALL_PREFIX(ct, "int_lin_", RegroupLinear);
    CALL_PREFIX(ct, "int_lin_", SimplifyUnaryLinear);
    CALL_PREFIX(ct, "int_lin_", SimplifyBinaryLinear);
    CALL_TYPE(ct, "int_lin_eq", PropagatePositiveLinear);
    CALL_TYPE(ct, "int_lin_le", PropagatePositiveLinear);
    CALL_TYPE(ct, "int_lin_ge", PropagatePositiveLinear);
    CALL_TYPE(ct, "int_lin_eq", CreateLinearTarget);
    CALL_TYPE(ct, "int_lin_eq", PresolveStoreMapping);
    CALL_TYPE(ct, "int_lin_eq_reif", CheckIntLinReifBounds);
    CALL_TYPE(ct, "int_lin_eq_reif", SimplifyIntLinEqReif);
  }

  if (strings::StartsWith(ct->type, "array_")) {
    CALL_TYPE(ct, "array_bool_and", PresolveArrayBoolAnd);
    CALL_TYPE(ct, "array_bool_or", PresolveArrayBoolOr);
    CALL_TYPE(ct, "array_int_element", PresolveSimplifyElement);
    CALL_TYPE(ct, "array_bool_element", PresolveSimplifyElement);
    CALL_TYPE(ct, "array_int_element", PresolveArrayIntElement);
    CALL_TYPE(ct, "array_var_int_element", PresolveSimplifyExprElement);
    CALL_TYPE(ct, "array_var_bool_element", PresolveSimplifyExprElement);
  }

  if (strings::StartsWith(ct->type, "int_")) {
    CALL_TYPE(ct, "int_div", PresolveIntDiv);
    CALL_TYPE(ct, "int_times", PresolveIntTimes);
    CALL_TYPE(ct, "int_eq", PresolveIntEq);
    CALL_TYPE(ct, "int_ne", PresolveIntNe);
    CALL_TYPE(ct, "int_eq_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "int_ne_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "int_le_reif", RemoveAbsFromIntLeReif);
    CALL_TYPE(ct, "int_le_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "int_lt_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "int_ge_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "int_gt_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "int_mod", PresolveIntMod);
  }

  if (strings::StartsWith(ct->type, "bool_")) {
    CALL_TYPE(ct, "bool_eq", PresolveIntEq);
    CALL_TYPE(ct, "bool_ne", PresolveIntNe);
    CALL_TYPE(ct, "bool_not", PresolveIntNe);
    CALL_TYPE(ct, "bool_eq_reif", PresolveBoolEqNeReif);
    CALL_TYPE(ct, "bool_ne_reif", PresolveBoolEqNeReif);
    CALL_TYPE(ct, "bool_xor", PresolveBoolXor);
    CALL_TYPE(ct, "bool_ne", PresolveBoolNot);
    CALL_TYPE(ct, "bool_not", PresolveBoolNot);
    CALL_TYPE(ct, "bool_clause", PresolveBoolClause);
    CALL_TYPE(ct, "bool_eq_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "bool_ne_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "bool_le_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "bool_lt_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "bool_ge_reif", PropagateReifiedComparisons);
    CALL_TYPE(ct, "bool_gt_reif", PropagateReifiedComparisons);
  }
  CALL_TYPE(ct, "table_int", PresolveTableInt);
  CALL_TYPE(ct, "diffn", PresolveDiffN);
  CALL_TYPE(ct, "regular", PresolveRegular);

  // Last rule: if the target variable of a constraint is fixed, removed it
  // the target part.
  if (ct->target_variable != nullptr &&
      ct->target_variable->domain.HasOneValue()) {
    FZVLOG << "Remove the target variable from " << ct->DebugString()
           << " as it is fixed to a single value" << FZENDL;
    MarkChangedVariable(ct->target_variable);
    ct->target_variable->defining_constraint = nullptr;
    ct->target_variable = nullptr;
  }
}

#undef CALL_TYPE
#undef CALL_PREFIX
#undef CALL_SUFFIX

// Stores all pairs of variables appearing in an int_ne(x, y) constraint.
void Presolver::StoreDifference(Constraint* ct) {
  if (ct->arguments[2].Value() == 0 && ct->arguments[0].values.size() == 3) {
    // Looking for a difference var.
    if ((ct->arguments[0].values[0] == 1 && ct->arguments[0].values[1] == -1 &&
         ct->arguments[0].values[2] == 1) ||
        (ct->arguments[0].values[0] == -1 && ct->arguments[0].values[1] == 1 &&
         ct->arguments[0].values[2] == -1)) {
      FZVLOG << "Store differences from " << ct->DebugString() << FZENDL;
      difference_map_[ct->arguments[1].variables[0]] = std::make_pair(
          ct->arguments[1].variables[2], ct->arguments[1].variables[1]);
      difference_map_[ct->arguments[1].variables[2]] = std::make_pair(
          ct->arguments[1].variables[0], ct->arguments[1].variables[1]);
    }
  }
}

void Presolver::MergeIntEqNe(Model* model) {
  std::unordered_map<const IntegerVariable*,
                     std::unordered_map<int64, IntegerVariable*>>
      int_eq_reif_map;
  std::unordered_map<const IntegerVariable*,
                     std::unordered_map<int64, IntegerVariable*>>
      int_ne_reif_map;
  for (Constraint* const ct : model->constraints()) {
    if (!ct->active) continue;
    if (ct->type == "int_eq_reif" && ct->arguments[2].values.empty()) {
      IntegerVariable* var = nullptr;
      int64 value = 0;
      if (ct->arguments[0].values.empty() &&
          ct->arguments[1].variables.empty()) {
        var = ct->arguments[0].Var();
        value = ct->arguments[1].Value();
      } else if (ct->arguments[1].values.empty() &&
                 ct->arguments[0].variables.empty()) {
        var = ct->arguments[1].Var();
        value = ct->arguments[0].Value();
      }
      if (var != nullptr) {
        IntegerVariable* boolvar = ct->arguments[2].Var();
        IntegerVariable* stored = FindPtrOrNull(int_eq_reif_map[var], value);
        if (stored == nullptr) {
          FZVLOG << "Store " << ct->DebugString() << FZENDL;
          int_eq_reif_map[var][value] = boolvar;
        } else {
          FZVLOG << "Merge " << ct->DebugString() << FZENDL;
          ct->MarkAsInactive();
          AddVariableSubstition(stored, boolvar);
          successful_rules_["MergeIntEqNe"]++;
        }
      }
    }

    if (ct->type == "int_ne_reif" && ct->arguments[2].values.empty()) {
      IntegerVariable* var = nullptr;
      int64 value = 0;
      if (ct->arguments[0].values.empty() &&
          ct->arguments[1].variables.empty()) {
        var = ct->arguments[0].Var();
        value = ct->arguments[1].Value();
      } else if (ct->arguments[1].values.empty() &&
                 ct->arguments[0].variables.empty()) {
        var = ct->arguments[1].Var();
        value = ct->arguments[0].Value();
      }
      if (var != nullptr) {
        IntegerVariable* boolvar = ct->arguments[2].Var();
        IntegerVariable* stored = FindPtrOrNull(int_ne_reif_map[var], value);
        if (stored == nullptr) {
          FZVLOG << "Store " << ct->DebugString() << FZENDL;
          int_ne_reif_map[var][value] = boolvar;
        } else {
          FZVLOG << "Merge " << ct->DebugString() << FZENDL;
          ct->MarkAsInactive();
          AddVariableSubstition(stored, boolvar);
          successful_rules_["MergeIntEqNe"]++;
        }
      }
    }
  }
}

void Presolver::FirstPassModelScan(Model* model) {
  for (Constraint* const ct : model->constraints()) {
    if (!ct->active) continue;
    if (ct->type == "int_lin_eq") {
      StoreDifference(ct);
    }
  }

  // Collect decision variables.
  std::vector<IntegerVariable*> vars;
  for (const Annotation& ann : model->search_annotations()) {
    ann.AppendAllIntegerVariables(&vars);
  }
  decision_variables_.insert(vars.begin(), vars.end());
}

namespace {
CliqueResponse StoreClique(const std::vector<int>& vec, std::vector<int>* out) {
  out->assign(vec.begin(), vec.end());
  // We do not care about singleton and one arc cliques.
  if (vec.size() > 2) {
    return CliqueResponse::STOP;
  } else {
    return CliqueResponse::CONTINUE;
  }
}

void PrintGraph(const std::vector<std::vector<bool>>& neighbors,
                int num_variables) {
  for (int i = 0; i < num_variables; ++i) {
    std::string out = StringPrintf("%i : [", i);
    bool found_one = false;
    for (int j = 0; j < num_variables; ++j) {
      if (neighbors[i][j]) {
        StringAppendF(&out, "%s %i", found_one ? "," : "", j);
        found_one = true;
      }
    }
    if (found_one) {
      FZLOG << out << "]" << FZENDL;
    }
  }
}
}  // namespace

bool Presolver::RegroupDifferent(Model* model) {
  std::vector<std::vector<bool>> neighbors;
  VectorMap<IntegerVariable*> variables_to_dense_index;

  std::vector<Constraint*> int_ne_constraints;
  for (Constraint* const ct : model->constraints()) {
    if (!ct->active) continue;
    if (ct->type == "int_ne" && !ct->arguments[0].HasOneValue() &&
        !ct->arguments[1].HasOneValue()) {
      IntegerVariable* const left = ct->arguments[0].Var();
      IntegerVariable* const right = ct->arguments[1].Var();
      variables_to_dense_index.Add(left);
      variables_to_dense_index.Add(right);
      int_ne_constraints.push_back(ct);
    }
  }

  if (int_ne_constraints.empty()) {
    // Nothing to do. Exit early.
    return false;
  }

  const int num_variables = variables_to_dense_index.size();
  neighbors.resize(num_variables);
  for (int i = 0; i < num_variables; ++i) {
    neighbors[i].resize(num_variables, false);
  }

  for (Constraint* const ct : int_ne_constraints) {
    const int left_index =
        variables_to_dense_index.IndexOrDie(ct->arguments[0].Var());
    const int right_index =
        variables_to_dense_index.IndexOrDie(ct->arguments[1].Var());
    neighbors[left_index][right_index] = true;
    neighbors[right_index][left_index] = true;
  }

  // Collect all cliques of size > 2. After finding one clique. We remove all
  // arcs belonging to this clique from the graph, and restart. This way, we
  // cover all arcs with cliques, instead of finding all maximal cliques.
  std::vector<std::vector<int>> all_cliques;
  for (;;) {
    std::vector<int> clique;
    BronKerboschAlgorithm<int> clique_finder(
        [&neighbors](int i, int j) { return neighbors[i][j]; }, num_variables,
        [&clique](const std::vector<int>& o) {
          return StoreClique(o, &clique);
        });

    const BronKerboschAlgorithmStatus status = clique_finder.Run();
    if (status == BronKerboschAlgorithmStatus::COMPLETED) {
      // We have found all cliques of size > 2. We can exit this loop.
      break;
    }
    CHECK_GT(clique.size(), 2);

    std::sort(clique.begin(), clique.end());
    for (int i = 0; i < clique.size() - 1; ++i) {
      for (int j = i + 1; j < clique.size(); ++j) {
        neighbors[clique[i]][clique[j]] = false;
        neighbors[clique[j]][clique[i]] = false;
      }
    }
    all_cliques.push_back(clique);
  }

  if (all_cliques.empty()) {
    return false;
  }

  // Note that the memory used is not greater that what we actually uses for all
  // the not-equal constraints in the first place.
  std::set<std::pair<int, int>> to_kill;
  std::map<std::pair<int, int>, std::vector<int>> replace_map;
  for (const auto& clique : all_cliques) {
    for (int i = 0; i < clique.size() - 1; ++i) {
      for (int j = i + 1; j < clique.size(); ++j) {
        const std::pair<int, int> p = std::make_pair(clique[i], clique[j]);
        if (i == 0 && j == 1) {
          replace_map[p] = clique;
        } else {
          to_kill.insert(p);
        }
      }
    }
  }

  // Modify the model.
  int killed = 0;
  int new_all_diffs = 0;
  for (Constraint* const ct : int_ne_constraints) {
    IntegerVariable* const left = ct->arguments[0].Var();
    IntegerVariable* const right = ct->arguments[1].Var();
    const int left_index = variables_to_dense_index.IndexOrDie(left);
    const int right_index = variables_to_dense_index.IndexOrDie(right);
    const std::pair<int, int> p = std::make_pair(
        std::min(left_index, right_index), std::max(left_index, right_index));
    if (ContainsKey(replace_map, p)) {
      FZVLOG << "Apply rule RegroupDifferent on " << ct->DebugString()
             << FZENDL;
      const std::vector<int>& rep = replace_map[p];
      ct->type = "all_different_int";
      std::vector<IntegerVariable*> vars(rep.size());
      for (int i = 0; i < rep.size(); ++i) {
        vars[i] = variables_to_dense_index[rep[i]];
      }
      ct->arguments[0] = Argument::IntVarRefArray(vars);
      ct->arguments.pop_back();
      new_all_diffs++;
      FZVLOG << "  - constraint is modified to " << ct->DebugString() << FZENDL;
    } else if (ContainsKey(to_kill, p)) {
      ct->MarkAsInactive();
      killed++;
    }
  }
  if (killed != 0) {
    FZLOG << "  - rule RegroupDifferent has created " << new_all_diffs
          << " all_different_int constraints and removed " << killed
          << " int_ne constraints" << FZENDL;
  }
  return killed > 0;
}

bool Presolver::Run(Model* model) {
  // Rebuild var_constraint map if empty.
  if (var_to_constraints_.empty()) {
    for (Constraint* const ct : model->constraints()) {
      AddConstraintToMapping(ct);
    }
  }

  FirstPassModelScan(model);

  MergeIntEqNe(model);
  if (!var_representative_map_.empty()) {
    // Some new substitutions were introduced. Let's process them.
    SubstituteEverywhere(model);
    var_representative_map_.clear();
  }

  bool changed_since_start = false;
  // Let's presolve the bool2int predicates first.
  for (Constraint* const ct : model->constraints()) {
    if (ct->active && ct->type == "bool2int") {
      std::string log;
      ApplyRule(ct, "PresolveBool2Int", [this](Constraint* ct, std::string* log) {
        return PresolveBool2Int(ct, log);
      });
    }
  }
  if (!var_representative_map_.empty()) {
    // Some new substitutions were introduced. Let's process them.
    SubstituteEverywhere(model);
    var_representative_map_.clear();
  }

  {
    FZVLOG << "  - processing initial model with "
           << model->constraints().size() << " constraints." << FZENDL;
    for (Constraint* const ct : model->constraints()) {
      if (ct->active) {
        changed_constraints_.erase(ct);  // Optim: remove from postponed queue.
        PresolveOneConstraint(ct);
        if (!ct->active || ct->type == "false_constraint") {
          changed_since_start = true;
        }
      }
      if (!var_representative_map_.empty()) {
        SubstituteEverywhere(model);
        var_representative_map_.clear();
      }
    }
    FZVLOG << "  - done" << FZENDL;
  }

  // Incremental part.
  int loops = 1;
  while (!changed_variables_.empty() || !changed_constraints_.empty()) {
    loops++;
    FZVLOG << "--- loop " << loops << FZENDL;
    changed_since_start = true;
    std::unordered_set<Constraint*> to_scan;

    for (IntegerVariable* var : changed_variables_) {
      for (Constraint* ct : var_to_constraints_[var]) {
        if (ct->active) {
          to_scan.insert(ct);
        }
      }
    }
    for (Constraint* ct : changed_constraints_) {
      if (ct->active) {
        to_scan.insert(ct);
      }
    }

    changed_variables_.clear();
    changed_constraints_.clear();
    var_representative_map_.clear();
    FZVLOG << "  - processing " << to_scan.size() << " constraints" << FZENDL;
    for (Constraint* const ct : to_scan) {
      if (!var_representative_map_.empty()) {
        changed_constraints_.insert(ct);  // Carry over to next round.
      } else if (ct->active) {
        PresolveOneConstraint(ct);
      }
    }
    if (!var_representative_map_.empty()) {
      // Some new substitutions were introduced. Let's process them.
      SubstituteEverywhere(model);
      var_representative_map_.clear();
    }
  }

  // Report presolve rules statistics.
  if (!successful_rules_.empty()) {
    FZLOG << "  - presolve looped " << loops << " times over the model"
          << FZENDL;
    for (const auto& rule : successful_rules_) {
      if (rule.second == 1) {
        FZLOG << "  - rule " << rule.first << " was applied 1 time" << FZENDL;
      } else {
        FZLOG << "  - rule " << rule.first << " was applied " << rule.second
              << " times" << FZENDL;
      }
    }
  }

  // Regroup int_ne into all_different_int.
  changed_since_start |= RegroupDifferent(model);

  return changed_since_start;
}

// ----- Substitution support -----

void Presolver::AddVariableSubstition(IntegerVariable* from,
                                      IntegerVariable* to) {
  CHECK(from != nullptr);
  CHECK(to != nullptr);
  // Apply the substitutions, if any.
  from = FindRepresentativeOfVar(from);
  to = FindRepresentativeOfVar(to);
  if (to->temporary) {
    // Let's switch to keep a non temporary as representative.
    IntegerVariable* tmp = to;
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

IntegerVariable* Presolver::FindRepresentativeOfVar(IntegerVariable* var) {
  if (var == nullptr) return nullptr;
  IntegerVariable* start_var = var;
  // First loop: find the top parent.
  for (;;) {
    IntegerVariable* parent =
        FindWithDefault(var_representative_map_, var, var);
    if (parent == var) break;
    var = parent;
  }
  // Second loop: attach all the path to the top parent.
  while (start_var != var) {
    IntegerVariable* const parent = var_representative_map_[start_var];
    var_representative_map_[start_var] = var;
    start_var = parent;
  }
  return FindWithDefault(var_representative_map_, var, var);
}

void Presolver::SubstituteEverywhere(Model* model) {
  // Collected impacted constraints.
  std::unordered_set<Constraint*> impacted;
  for (const auto& p : var_representative_map_) {
    const std::unordered_set<Constraint*>& contains =
        var_to_constraints_[p.first];
    impacted.insert(contains.begin(), contains.end());
  }
  // Rewrite the constraints.
  for (Constraint* const ct : impacted) {
    if (ct != nullptr && ct->active) {
      for (int i = 0; i < ct->arguments.size(); ++i) {
        Argument& argument = ct->arguments[i];
        switch (argument.type) {
          case Argument::INT_VAR_REF:
          case Argument::INT_VAR_REF_ARRAY: {
            for (int i = 0; i < argument.variables.size(); ++i) {
              IntegerVariable* const old_var = argument.variables[i];
              IntegerVariable* const new_var = FindRepresentativeOfVar(old_var);
              if (new_var != old_var) {
                argument.variables[i] = new_var;
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
  // Cleanup the outdated var_to_constraints sets.
  for (const auto& p : var_representative_map_) {
    var_to_constraints_[p.first].clear();
  }
  // Rewrite the search.
  for (Annotation* const ann : model->mutable_search_annotations()) {
    SubstituteAnnotation(ann);
  }
  // Rewrite the output.
  for (SolutionOutputSpecs* const output : model->mutable_output()) {
    output->variable = FindRepresentativeOfVar(output->variable);
    for (int i = 0; i < output->flat_variables.size(); ++i) {
      output->flat_variables[i] =
          FindRepresentativeOfVar(output->flat_variables[i]);
    }
  }
  // Do not forget to merge domain that could have evolved asynchronously
  // during presolve.
  for (const auto& iter : var_representative_map_) {
    iter.second->domain.IntersectWithDomain(iter.first->domain);
  }

  // Mark new variables for revisit.
  for (const auto& iter : var_representative_map_) {
    MarkChangedVariable(iter.second);
  }

  // Change the objective variable.
  IntegerVariable* const current_objective = model->objective();
  if (current_objective == nullptr) return;
  IntegerVariable* const new_objective =
      FindRepresentativeOfVar(current_objective);
  if (new_objective != current_objective) {
    model->SetObjective(new_objective);
  }
}

void Presolver::SubstituteAnnotation(Annotation* ann) {
  // TODO(user): Remove recursion.
  switch (ann->type) {
    case Annotation::ANNOTATION_LIST:
    case Annotation::FUNCTION_CALL: {
      for (int i = 0; i < ann->annotations.size(); ++i) {
        SubstituteAnnotation(&ann->annotations[i]);
      }
      break;
    }
    case Annotation::INT_VAR_REF:
    case Annotation::INT_VAR_REF_ARRAY: {
      for (int i = 0; i < ann->variables.size(); ++i) {
        ann->variables[i] = FindRepresentativeOfVar(ann->variables[i]);
      }
      break;
    }
    default: {}
  }
}

// ----- Helpers -----

bool Presolver::IntersectVarWithArg(IntegerVariable* var, const Argument& arg) {
  switch (arg.type) {
    case Argument::INT_VALUE: {
      const int64 value = arg.Value();
      if (var->domain.IntersectWithSingleton(value)) {
        MarkChangedVariable(var);
        return true;
      }
      break;
    }
    case Argument::INT_INTERVAL: {
      if (var->domain.IntersectWithInterval(arg.values[0], arg.values[1])) {
        MarkChangedVariable(var);
        return true;
      }
      break;
    }
    case Argument::INT_LIST: {
      if (var->domain.IntersectWithListOfIntegers(arg.values)) {
        MarkChangedVariable(var);
        return true;
      }
      break;
    }
    default: { LOG(FATAL) << "Wrong domain type" << arg.DebugString(); }
  }
  return false;
}

bool Presolver::IntersectVarWithSingleton(IntegerVariable* var, int64 value) {
  if (var->domain.IntersectWithSingleton(value)) {
    MarkChangedVariable(var);
    return true;
  }
  return false;
}

bool Presolver::IntersectVarWithInterval(IntegerVariable* var, int64 imin,
                                         int64 imax) {
  if (var->domain.IntersectWithInterval(imin, imax)) {
    MarkChangedVariable(var);
    return true;
  }
  return false;
}

bool Presolver::RemoveValue(IntegerVariable* var, int64 value) {
  if (var->domain.RemoveValue(value)) {
    MarkChangedVariable(var);
    return true;
  }
  return false;
}

// ----- Clean up model -----

namespace {
void Regroup(Constraint* start, const std::vector<IntegerVariable*>& chain,
             const std::vector<IntegerVariable*>& carry_over) {
  // End of chain, reconstruct.
  IntegerVariable* const out = carry_over.back();
  start->arguments.pop_back();
  start->arguments[0].variables[0] = out;
  start->arguments[1].type = Argument::INT_VAR_REF_ARRAY;
  start->arguments[1].variables = chain;
  const std::string old_type = start->type;
  start->type = start->type == "int_min" ? "minimum_int" : "maximum_int";
  start->target_variable = out;
  out->defining_constraint = start;
  for (IntegerVariable* const var : carry_over) {
    if (var != carry_over.back()) {
      var->active = false;
    }
  }
  FZVLOG << "Regroup chain of " << old_type << " into " << start->DebugString()
         << FZENDL;
}

void CheckRegroupStart(Constraint* ct, Constraint** start,
                       std::vector<IntegerVariable*>* chain,
                       std::vector<IntegerVariable*>* carry_over) {
  if ((ct->type == "int_min" || ct->type == "int_max") &&
      !ct->arguments[0].variables.empty() &&
      ct->arguments[0].Var() == ct->arguments[1].Var()) {
    // This is the start of the chain.
    *start = ct;
    chain->push_back(ct->arguments[0].Var());
    carry_over->push_back(ct->arguments[2].Var());
    carry_over->back()->defining_constraint = nullptr;
  }
}

// Weight:
//  - *_reif: arity
//  - otherwise arity + 100.
int SortWeight(Constraint* ct) {
  int arity = strings::EndsWith(ct->type, "_reif") ? 0 : 100;
  for (const Argument& arg : ct->arguments) {
    arity += arg.variables.size();
  }
  return arity;
}

void CleanUpVariableWithMultipleDefiningConstraints(Model* model) {
  std::unordered_map<IntegerVariable*, std::vector<Constraint*>> ct_var_map;
  for (Constraint* const ct : model->constraints()) {
    if (ct->target_variable != nullptr) {
      ct_var_map[ct->target_variable].push_back(ct);
    }
  }

  for (auto& ct_list : ct_var_map) {
    if (ct_list.second.size() > 1) {
      // Sort by number of variables in the constraint. Prefer smaller ones.
      std::sort(ct_list.second.begin(), ct_list.second.end(),
                [](Constraint* c1, Constraint* c2) {
                  return SortWeight(c1) < SortWeight(c2);
                });
      // Keep the first constraint as the defining one.
      for (int pos = 1; pos < ct_list.second.size(); ++pos) {
        Constraint* const ct = ct_list.second[pos];
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
  CHECK(!coeffs.empty());
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

void Presolver::CleanUpModelForTheCpSolver(Model* model, bool use_sat) {
  // First pass.
  for (Constraint* const ct : model->constraints()) {
    // Treat float variables as int variables, convert constraints to int.
    if (FLAGS_fz_floats_are_ints) {
      const std::string& id = ct->type;
      if (id == "int2float") {
        ct->type = "int_eq";
      } else if (id == "float_lin_le") {
        ct->type = "int_lin_le";
      } else if (id == "float_lin_eq") {
        ct->type = "int_lin_eq";
      }
    }
    const std::string& id = ct->type;
    // Remove ignored annotations on int_lin_eq.
    if (id == "int_lin_eq" && ct->strong_propagation) {
      if (ct->arguments[0].values.size() > 3) {
        // We will use a table constraint. Remove the target variable flag.
        FZVLOG << "Remove target_variable from " << ct->DebugString() << FZENDL;
        ct->RemoveTargetVariable();
      }
    }
    if (id == "int_lin_eq" && ct->target_variable != nullptr) {
      IntegerVariable* const var = ct->target_variable;
      for (int i = 0; i < ct->arguments[0].values.size(); ++i) {
        if (ct->arguments[1].variables[i] == var) {
          if (ct->arguments[0].values[i] == -1) {
            break;
          } else if (ct->arguments[0].values[i] == 1) {
            FZVLOG << "Reverse " << ct->DebugString() << FZENDL;
            ct->arguments[2].values[0] *= -1;
            for (int j = 0; j < ct->arguments[0].values.size(); ++j) {
              ct->arguments[0].values[j] *= -1;
            }
            break;
          }
        }
      }
    }
    if (id == "array_var_int_element") {
      if (ct->target_variable != nullptr) {
        std::unordered_set<IntegerVariable*> variables_in_array;
        for (IntegerVariable* const var : ct->arguments[1].variables) {
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
          !ct->arguments[1].HasOneValue()) ||
         id == "bool_le_reif" || id == "bool_ge_reif")) {
      ct->RemoveTargetVariable();
    }
    // Remove target variables from constraints that will not implement it.
    if (id == "count_reif" || id == "set_in_reif") {
      ct->RemoveTargetVariable();
    }
    // Remove target variables from element constraint.
    if ((id == "array_int_element" &&
         (!IsArrayBoolean(ct->arguments[1].values) ||
          !AtMostOne0OrAtMostOne1(ct->arguments[1].values))) ||
        id == "array_var_int_element") {
      ct->RemoveTargetVariable();
    }
  }

  // Clean up variables with multiple defining constraints.
  CleanUpVariableWithMultipleDefiningConstraints(model);

  // Second pass.
  for (Constraint* const ct : model->constraints()) {
    const std::string& id = ct->type;
    // Create new target variables with unused boolean variables.
    if (ct->target_variable == nullptr &&
        (id == "int_lin_eq_reif" || id == "int_lin_ne_reif" ||
         id == "int_lin_ge_reif" || id == "int_lin_le_reif" ||
         id == "int_lin_gt_reif" || id == "int_lin_lt_reif" ||
         id == "int_eq_reif" || id == "int_ne_reif" || id == "int_le_reif" ||
         id == "int_ge_reif" || id == "int_lt_reif" || id == "int_gt_reif")) {
      IntegerVariable* const bool_var = ct->arguments[2].Var();
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
  Constraint* start = nullptr;
  std::vector<IntegerVariable*> chain;
  std::vector<IntegerVariable*> carry_over;
  var_to_constraints_.clear();
  for (Constraint* const ct : model->constraints()) {
    for (const Argument& arg : ct->arguments) {
      for (IntegerVariable* const var : arg.variables) {
        var_to_constraints_[var].insert(ct);
      }
    }
  }

  // First version. The start is recognized by the double var in the max.
  //   tmp1 = std::max(v1, v1)
  for (Constraint* const ct : model->constraints()) {
    if (start == nullptr) {
      CheckRegroupStart(ct, &start, &chain, &carry_over);
    } else if (ct->type == start->type &&
               ct->arguments[1].Var() == carry_over.back() &&
               var_to_constraints_[ct->arguments[0].Var()].size() <= 2) {
      chain.push_back(ct->arguments[0].Var());
      carry_over.push_back(ct->arguments[2].Var());
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
  std::vector<IntegerVariable*> current_variables;
  IntegerVariable* target_variable = nullptr;
  Constraint* first_constraint = nullptr;
  for (Constraint* const ct : model->constraints()) {
    if (target_variable == nullptr) {
      if (ct->type == "int_lin_eq" && ct->arguments[0].values.size() == 3 &&
          AreOnesFollowedByMinusOne(ct->arguments[0].values) &&
          ct->arguments[1].values.empty() && ct->arguments[2].Value() == 0) {
        FZVLOG << "Recognize assignment " << ct->DebugString() << FZENDL;
        current_variables = ct->arguments[1].variables;
        target_variable = current_variables.back();
        current_variables.pop_back();
        first_constraint = ct;
      }
    } else {
      if (ct->type == "int_lin_eq" &&
          AreOnesFollowedByMinusOne(ct->arguments[0].values) &&
          ct->arguments[0].values.size() == current_variables.size() + 2 &&
          IsStrictPrefix(current_variables, ct->arguments[1].variables)) {
        FZVLOG << "Recognize hidden int_plus " << ct->DebugString() << FZENDL;
        current_variables = ct->arguments[1].variables;
        // Rewrite ct into int_plus.
        ct->type = "int_plus";
        ct->arguments.clear();
        ct->arguments.push_back(Argument::IntVarRef(target_variable));
        ct->arguments.push_back(Argument::IntVarRef(
            current_variables[current_variables.size() - 2]));
        ct->arguments.push_back(Argument::IntVarRef(current_variables.back()));
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
}  // namespace fz
}  // namespace operations_research
