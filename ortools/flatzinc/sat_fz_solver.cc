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

#include "ortools/flatzinc/sat_fz_solver.h"

#include <unordered_map>
#include <limits>
#include "ortools/base/stringprintf.h"
#include "ortools/base/timer.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/flatzinc/checker.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/flow_costs.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/table.h"
#include "ortools/util/sorted_interval_list.h"

DEFINE_bool(fz_use_lp_constraint, true,
            "Use LP solver glop to enforce all linear inequalities at once.");

namespace operations_research {
namespace sat {
namespace {

// Hold the sat::Model and the correspondance between flatzinc and sat vars.
struct SatModel {
  Model model;

  // A flatzinc Boolean variable can appear in both maps if a constraint needs
  // its integer representation as a 0-1 variable. Such an IntegerVariable is
  // created lazily by LookupVar() when a constraint is requesting it.
  std::unordered_map<fz::IntegerVariable*, IntegerVariable> var_map;
  std::unordered_map<fz::IntegerVariable*, Literal> bool_map;

  // Utility functions to convert an fz::Argument to sat::IntegerVariable(s).
  IntegerVariable LookupConstant(int64 value);
  IntegerVariable LookupVar(fz::IntegerVariable* var);
  IntegerVariable LookupVar(const fz::Argument& argument);
  std::vector<IntegerVariable> LookupVars(const fz::Argument& argument);

  // Utility functions to convert a Boolean fz::Argument to sat::Literal(s).
  bool IsBoolean(fz::IntegerVariable* argument) const;
  bool IsBoolean(const fz::Argument& argument) const;
  Literal GetTrueLiteral(fz::IntegerVariable* var) const;
  Literal GetTrueLiteral(const fz::Argument& argument) const;
  std::vector<Literal> GetTrueLiterals(const fz::Argument& argument) const;
  std::vector<Literal> GetFalseLiterals(const fz::Argument& argument) const;

  // Returns the full domain Boolean encoding of the given variable (encoding it
  // if not already done).
  std::vector<IntegerEncoder::ValueLiteralPair> FullEncoding(IntegerVariable v);

  // Returns the value of the given variable in the current assigment. It must
  // be assigned, otherwise this will crash.
  int64 Value(fz::IntegerVariable* var) const;
};

IntegerVariable SatModel::LookupConstant(const int64 value) {
  return model.Add(ConstantIntegerVariable(value));
}

IntegerVariable SatModel::LookupVar(fz::IntegerVariable* var) {
  CHECK(!var->domain.HasOneValue());
  if (ContainsKey(var_map, var)) return FindOrDie(var_map, var);
  CHECK_EQ(var->domain.Min(), 0);
  CHECK_EQ(var->domain.Max(), 1);

  // Otherwise, this must be a Boolean and we must construct the IntegerVariable
  // associated with it.
  const Literal lit = FindOrDie(bool_map, var);
  const IntegerVariable int_var = model.Add(NewIntegerVariable(0, 1));
  InsertOrDie(&var_map, var, int_var);
  model.GetOrCreate<IntegerEncoder>()->FullyEncodeVariableUsingGivenLiterals(
      int_var, {lit.Negated(), lit}, {IntegerValue(0), IntegerValue(1)});
  return int_var;
}

IntegerVariable SatModel::LookupVar(const fz::Argument& argument) {
  if (argument.HasOneValue()) return LookupConstant(argument.Value());
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF);
  return LookupVar(argument.variables[0]);
}

std::vector<IntegerVariable> SatModel::LookupVars(
    const fz::Argument& argument) {
  std::vector<IntegerVariable> result;
  if (argument.type == fz::Argument::VOID_ARGUMENT) return result;

  if (argument.type == fz::Argument::INT_LIST) {
    for (int64 value : argument.values) {
      result.push_back(LookupConstant(value));
    }
  } else {
    CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF_ARRAY);
    for (fz::IntegerVariable* var : argument.variables) {
      if (var->domain.HasOneValue()) {
        result.push_back(LookupConstant(var->domain.Value()));
      } else {
        result.push_back(LookupVar(var));
      }
    }
  }
  return result;
}

std::vector<IntegerEncoder::ValueLiteralPair> SatModel::FullEncoding(
    IntegerVariable var) {
  return model.Add(FullyEncodeVariable(var));
}

bool SatModel::IsBoolean(fz::IntegerVariable* var) const {
  return ContainsKey(bool_map, var);
}

bool SatModel::IsBoolean(const fz::Argument& argument) const {
  if (argument.type != fz::Argument::INT_VAR_REF) return false;
  return ContainsKey(bool_map, argument.variables[0]);
}

Literal SatModel::GetTrueLiteral(fz::IntegerVariable* var) const {
  CHECK(!var->domain.HasOneValue());
  return FindOrDie(bool_map, var);
}

Literal SatModel::GetTrueLiteral(const fz::Argument& argument) const {
  CHECK(!argument.HasOneValue());
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF);
  return FindOrDie(bool_map, argument.variables[0]);
}

std::vector<Literal> SatModel::GetTrueLiterals(
    const fz::Argument& argument) const {
  std::vector<Literal> literals;
  if (argument.type == fz::Argument::VOID_ARGUMENT) return literals;
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF_ARRAY);
  for (fz::IntegerVariable* var : argument.variables) {
    literals.push_back(GetTrueLiteral(var));
  }
  return literals;
}

std::vector<Literal> SatModel::GetFalseLiterals(
    const fz::Argument& argument) const {
  std::vector<Literal> literals;
  if (argument.type == fz::Argument::VOID_ARGUMENT) return literals;
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF_ARRAY);
  for (fz::IntegerVariable* var : argument.variables) {
    literals.push_back(GetTrueLiteral(var).Negated());
  }
  return literals;
}

int64 SatModel::Value(fz::IntegerVariable* var) const {
  if (var->domain.HasOneValue()) {
    return var->domain.Value();
  }
  if (ContainsKey(bool_map, var)) {
    return model.Get(sat::Value(FindOrDie(bool_map, var)));
  }
  return model.Get(sat::Value(FindOrDie(var_map, var)));
}

// =============================================================================
// Constraints extraction.
// =============================================================================

void ExtractBoolEq(const fz::Constraint& ct, SatModel* m) {
  const Literal a = m->GetTrueLiteral(ct.arguments[0]);
  const Literal b = m->GetTrueLiteral(ct.arguments[1]);
  m->model.Add(Equality(a, b));
}

void ExtractBoolEqNeReif(bool is_eq, const fz::Constraint& ct, SatModel* m) {
  const Literal a = m->GetTrueLiteral(ct.arguments[0]);
  const Literal b = m->GetTrueLiteral(ct.arguments[1]);
  Literal r = m->GetTrueLiteral(ct.arguments[2]);
  if (!is_eq) r = r.Negated();

  // We exclude 101, 011, 110 and 000.
  m->model.Add(ClauseConstraint({a.Negated(), b, r.Negated()}));
  m->model.Add(ClauseConstraint({a, b.Negated(), r.Negated()}));
  m->model.Add(ClauseConstraint({a.Negated(), b.Negated(), r}));
  m->model.Add(ClauseConstraint({a, b, r}));
}

void ExtractBoolNe(const fz::Constraint& ct, SatModel* m) {
  const Literal a = m->GetTrueLiteral(ct.arguments[0]);
  const Literal b = m->GetTrueLiteral(ct.arguments[1]);
  m->model.Add(Equality(a, b.Negated()));
}

void ExtractBoolLe(const fz::Constraint& ct, SatModel* m) {
  const Literal a = m->GetTrueLiteral(ct.arguments[0]);
  const Literal b = m->GetTrueLiteral(ct.arguments[1]);
  m->model.Add(Implication(a, b));
}

void ExtractBoolLeLtReif(bool is_le, const fz::Constraint& ct, SatModel* m) {
  Literal a = m->GetTrueLiteral(ct.arguments[0]);
  Literal b = m->GetTrueLiteral(ct.arguments[1]);
  Literal r = m->GetTrueLiteral(ct.arguments[2]);
  if (!is_le) {
    // The negation of r <=> (a <= b) is not(r) <=> (a > b)
    r = r.Negated();
    std::swap(a, b);
  }
  m->model.Add(ReifiedBoolLe(a, b, r));
}

void ExtractBoolClause(const fz::Constraint& ct, SatModel* m) {
  std::vector<Literal> positive = m->GetTrueLiterals(ct.arguments[0]);
  const std::vector<Literal> negative = m->GetFalseLiterals(ct.arguments[1]);
  positive.insert(positive.end(), negative.begin(), negative.end());
  m->model.Add(ClauseConstraint(positive));
}

void ExtractArrayBoolAnd(const fz::Constraint& ct, SatModel* m) {
  if (ct.arguments[1].HasOneValue()) {
    CHECK_EQ(0, ct.arguments[1].Value()) << "Other case should be presolved.";
    m->model.Add(ClauseConstraint(m->GetFalseLiterals(ct.arguments[0])));
  } else {
    const Literal r = m->GetTrueLiteral(ct.arguments[1]);
    m->model.Add(ReifiedBoolAnd(m->GetTrueLiterals(ct.arguments[0]), r));
  }
}

void ExtractArrayBoolOr(const fz::Constraint& ct, SatModel* m) {
  if (ct.arguments[1].HasOneValue()) {
    CHECK_EQ(ct.arguments[1].Value(), 1) << "Other case should be presolved.";
    m->model.Add(ClauseConstraint(m->GetTrueLiterals(ct.arguments[0])));
  } else {
    const Literal r = m->GetTrueLiteral(ct.arguments[1]);
    m->model.Add(ReifiedBoolOr(m->GetTrueLiterals(ct.arguments[0]), r));
  }
}

void ExtractArrayBoolXor(const fz::Constraint& ct, SatModel* m) {
  bool sum = false;
  std::vector<Literal> literals;
  for (fz::IntegerVariable* var : ct.arguments[0].variables) {
    if (var->domain.HasOneValue()) {
      sum ^= (var->domain.Value() == 1);
    } else {
      literals.push_back(m->GetTrueLiteral(var));
    }
  }
  m->model.Add(LiteralXorIs(literals, !sum));
}

void ExtractIntMin(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  const IntegerVariable c = m->LookupVar(ct.arguments[2]);
  m->model.Add(IsEqualToMinOf(c, {a, b}));
}

void ExtractArrayIntMinimum(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable min = m->LookupVar(ct.arguments[0]);
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  m->model.Add(IsEqualToMinOf(min, vars));
}

void ExtractArrayIntMaximum(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable max = m->LookupVar(ct.arguments[0]);
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  m->model.Add(IsEqualToMaxOf(max, vars));
}

void ExtractIntAbs(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable v = m->LookupVar(ct.arguments[0]);
  const IntegerVariable abs = m->LookupVar(ct.arguments[1]);
  m->model.Add(IsEqualToMaxOf(abs, {v, NegationOf(v)}));
}

void ExtractIntMax(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  const IntegerVariable max = m->LookupVar(ct.arguments[2]);
  m->model.Add(IsEqualToMaxOf(max, {a, b}));
}

void ExtractIntTimes(const fz::Constraint& ct, SatModel* m) {
  // TODO(user): Many constraint could be optimized in the same way.
  // especially the int_eq_reif between bool and so on.
  if (m->IsBoolean(ct.arguments[0]) && m->IsBoolean(ct.arguments[1]) &&
      m->IsBoolean(ct.arguments[2])) {
    const Literal a = m->GetTrueLiteral(ct.arguments[0]);
    const Literal b = m->GetTrueLiteral(ct.arguments[1]);
    const Literal c = m->GetTrueLiteral(ct.arguments[2]);
    m->model.Add(ReifiedBoolAnd({a, b}, c));
    return;
  }
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  const IntegerVariable c = m->LookupVar(ct.arguments[2]);
  m->model.Add(ProductConstraint(a, b, c));
}

void ExtractIntDiv(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  const IntegerVariable c = m->LookupVar(ct.arguments[2]);
  m->model.Add(DivisionConstraint(a, b, c));
}

void ExtractIntPlus(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  const IntegerVariable c = m->LookupVar(ct.arguments[2]);
  m->model.Add(FixedWeightedSum({a, b, c}, std::vector<int>{1, 1, -1}, 0));
}

void ExtractIntEq(const fz::Constraint& ct, SatModel* m) {
  // TODO(user): use the full encoding if available?
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  m->model.Add(Equality(a, b));
}

void ExtractIntNe(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  IntegerEncoder* encoder = m->model.GetOrCreate<IntegerEncoder>();
  if (!encoder->VariableIsFullyEncoded(a) ||
      !encoder->VariableIsFullyEncoded(b)) {
    m->model.Add(NotEqual(a, b));
  } else {
    m->model.Add(AllDifferent({a, b}));
  }
}

void ExtractIntLe(const fz::Constraint& ct, SatModel* m) {
  if (m->IsBoolean(ct.arguments[0]) && m->IsBoolean(ct.arguments[1])) {
    const Literal a = m->GetTrueLiteral(ct.arguments[0]);
    const Literal b = m->GetTrueLiteral(ct.arguments[1]);
    m->model.Add(Implication(a, b));
    return;
  }
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  m->model.Add(LowerOrEqual(a, b));
}

void ExtractIntGe(const fz::Constraint& ct, SatModel* m) {
  if (m->IsBoolean(ct.arguments[0]) && m->IsBoolean(ct.arguments[1])) {
    const Literal a = m->GetTrueLiteral(ct.arguments[0]);
    const Literal b = m->GetTrueLiteral(ct.arguments[1]);
    m->model.Add(Implication(b, a));
    return;
  }
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  m->model.Add(GreaterOrEqual(a, b));
}

void ExtractIntLeGeReif(bool is_le, const fz::Constraint& ct, SatModel* m) {
  CHECK(!ct.arguments[2].HasOneValue()) << "Should be presolved.";
  const Literal r = m->GetTrueLiteral(ct.arguments[2]);

  if (m->IsBoolean(ct.arguments[0]) && m->IsBoolean(ct.arguments[1])) {
    Literal a = m->GetTrueLiteral(ct.arguments[0]);
    Literal b = m->GetTrueLiteral(ct.arguments[1]);
    if (!is_le) std::swap(a, b);
    m->model.Add(ReifiedBoolLe(a, b, r));
    return;
  }

  if (ct.arguments[1].HasOneValue()) {
    if (ct.arguments[0].HasOneValue()) {
      if (is_le) {
        if (ct.arguments[0].Value() <= ct.arguments[1].Value()) {
          m->model.Add(ClauseConstraint({r}));
        } else {
          m->model.Add(ClauseConstraint({r.Negated()}));
        }
      } else {
        if (ct.arguments[0].Value() >= ct.arguments[1].Value()) {
          m->model.Add(ClauseConstraint({r}));
        } else {
          m->model.Add(ClauseConstraint({r.Negated()}));
        }
      }
      FZLOG << "Should be presolved: " << ct.DebugString() << FZENDL;
      return;
    }
    const IntegerVariable a = m->LookupVar(ct.arguments[0]);
    const IntegerValue value(ct.arguments[1].Value());
    IntegerLiteral i_lit = is_le ? IntegerLiteral::LowerOrEqual(a, value)
                                 : IntegerLiteral::GreaterOrEqual(a, value);
    m->model.Add(Equality(i_lit, r));
  } else if (ct.arguments[0].HasOneValue()) {
    const IntegerValue value(ct.arguments[0].Value());
    const IntegerVariable b = m->LookupVar(ct.arguments[1]);
    IntegerLiteral i_lit = is_le ? IntegerLiteral::GreaterOrEqual(b, value)
                                 : IntegerLiteral::LowerOrEqual(b, value);
    m->model.Add(Equality(i_lit, r));
  } else {
    IntegerVariable a = m->LookupVar(ct.arguments[0]);
    IntegerVariable b = m->LookupVar(ct.arguments[1]);
    if (!is_le) std::swap(a, b);
    m->model.Add(ReifiedLowerOrEqualWithOffset(a, b, 0, r));
  }
}

void ExtractIntLt(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable a = m->LookupVar(ct.arguments[0]);
  const IntegerVariable b = m->LookupVar(ct.arguments[1]);
  m->model.Add(LowerOrEqualWithOffset(a, b, 1));  // a + 1 <= b
}

// TODO(user): the code can probably be shared by ExtractIntLeGeReif() and
// we can easily support Gt.
void ExtractIntLtReif(const fz::Constraint& ct, SatModel* m) {
  CHECK(!ct.arguments[2].HasOneValue()) << "Should be presolved.";
  const Literal is_lt = m->GetTrueLiteral(ct.arguments[2]);

  if (ct.arguments[1].HasOneValue()) {
    CHECK(!ct.arguments[0].HasOneValue()) << "Should be presolved.";
    const IntegerVariable a = m->LookupVar(ct.arguments[0]);
    const IntegerValue value(ct.arguments[1].Value() - 1);
    m->model.Add(Equality(IntegerLiteral::LowerOrEqual(a, value), is_lt));
  } else if (ct.arguments[0].HasOneValue()) {
    const IntegerValue value(ct.arguments[0].Value() + 1);
    const IntegerVariable b = m->LookupVar(ct.arguments[1]);
    m->model.Add(Equality(IntegerLiteral::GreaterOrEqual(b, value), is_lt));
  } else {
    const IntegerVariable a = m->LookupVar(ct.arguments[0]);
    const IntegerVariable b = m->LookupVar(ct.arguments[1]);
    m->model.Add(ReifiedLowerOrEqualWithOffset(a, b, 1, is_lt));
  }
}

// Returns a non-empty vector if the constraint sum vars[i] * coeff[i] can be
// written as a sum of literal (eventually negating the variable) by replacing a
// variable -B by (not(B) - 1) and updates the given rhs.
//
// TODO(user): Do that in the presolve?
std::vector<Literal> IsSumOfLiteral(const fz::Argument& vars,
                                    const std::vector<int64>& coeffs,
                                    int64* rhs, SatModel* m) {
  const int n = coeffs.size();
  std::vector<Literal> result;
  for (int i = 0; i < n; ++i) {
    if (!m->IsBoolean(vars.variables[i])) return std::vector<Literal>();
    if (coeffs[i] == 1) {
      result.push_back(m->GetTrueLiteral(vars.variables[i]));
    } else if (coeffs[i] == -1) {
      result.push_back(m->GetTrueLiteral(vars.variables[i]).Negated());
      (*rhs)++;  // we replace -B by (not(B) - 1);
    } else {
      return std::vector<Literal>();
    }
  }
  CHECK_GE(*rhs, 0) << "Should be presolved.";
  CHECK_LE(*rhs, n) << "Should be presolved.";
  return result;
}

void AddLinearConstraintToLP(const std::vector<IntegerVariable>& vars,
                             const std::vector<int64>& coeffs, double lb,
                             double ub, SatModel* m) {
  LinearProgrammingConstraint* lp =
      m->model.GetOrCreate<LinearProgrammingConstraint>();
  const LinearProgrammingConstraint::ConstraintIndex ct =
      lp->CreateNewConstraint(lb, ub);
  for (int i = 0; i < vars.size(); i++) {
    lp->SetCoefficient(ct, vars[i], coeffs[i]);
  }
}

void ExtractIntLinEq(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  m->model.Add(FixedWeightedSum(vars, coeffs, rhs));

  if (FLAGS_fz_use_lp_constraint) {
    const double value = static_cast<double>(rhs);
    AddLinearConstraintToLP(vars, coeffs, value, value, m);
  }
}

void ExtractIntLinNe(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  m->model.Add(WeightedSumNotEqual(vars, coeffs, rhs));
}

void ExtractIntLinLe(const fz::Constraint& ct, SatModel* m) {
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  int64 new_rhs = rhs;
  std::vector<Literal> lits =
      IsSumOfLiteral(ct.arguments[1], coeffs, &new_rhs, m);
  if (!lits.empty() && new_rhs == coeffs.size() - 1) {
    // Not all literal can be true.
    for (Literal& ref : lits) ref = ref.Negated();
    m->model.Add(ClauseConstraint(lits));
  } else if (!lits.empty() && new_rhs == 0) {
    // Every literal must be false.
    for (const Literal l : lits) m->model.Add(ClauseConstraint({l.Negated()}));
  } else {
    const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
    m->model.Add(WeightedSumLowerOrEqual(vars, coeffs, rhs));

    if (FLAGS_fz_use_lp_constraint) {
      AddLinearConstraintToLP(vars, coeffs,
                              -std::numeric_limits<double>::infinity(),
                              static_cast<double>(rhs), m);
    }
  }
}

void ExtractIntLinGe(const fz::Constraint& ct, SatModel* m) {
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  int64 new_rhs = rhs;
  const std::vector<Literal> lits =
      IsSumOfLiteral(ct.arguments[1], coeffs, &new_rhs, m);
  if (!lits.empty() && new_rhs == 1) {
    // Not all literal can be false.
    m->model.Add(ClauseConstraint(lits));
  } else if (!lits.empty() && new_rhs == coeffs.size()) {
    // Every literal must be true.
    for (const Literal l : lits) m->model.Add(ClauseConstraint({l}));
  } else {
    const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
    m->model.Add(WeightedSumGreaterOrEqual(vars, coeffs, rhs));

    if (FLAGS_fz_use_lp_constraint) {
      AddLinearConstraintToLP(vars, coeffs, static_cast<double>(rhs),
                              std::numeric_limits<double>::infinity(), m);
    }
  }
}

void ExtractIntLinLeReif(const fz::Constraint& ct, SatModel* m) {
  const Literal r = m->GetTrueLiteral(ct.arguments[3]);
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  int64 new_rhs = rhs;
  const std::vector<Literal> lits =
      IsSumOfLiteral(ct.arguments[1], coeffs, &new_rhs, m);
  if (!lits.empty() && new_rhs == coeffs.size() - 1) {
    m->model.Add(ReifiedBoolAnd(lits, r.Negated()));
  } else if (!lits.empty() && new_rhs == 0) {
    m->model.Add(ReifiedBoolOr(lits, r.Negated()));
  } else {
    const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
    m->model.Add(WeightedSumLowerOrEqualReif(r, vars, coeffs, rhs));
  }
}

void ExtractIntLinGeReif(const fz::Constraint& ct, SatModel* m) {
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  const Literal r = m->GetTrueLiteral(ct.arguments[3]);
  int64 new_rhs = rhs;
  const std::vector<Literal> lits =
      IsSumOfLiteral(ct.arguments[1], coeffs, &new_rhs, m);
  if (!lits.empty() && new_rhs == 1) {
    m->model.Add(ReifiedBoolOr(lits, r));
  } else if (!lits.empty() && new_rhs == coeffs.size()) {
    m->model.Add(ReifiedBoolAnd(lits, r));
  } else {
    const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
    m->model.Add(WeightedSumGreaterOrEqualReif(r, vars, coeffs, rhs));
  }
}

void ExtractIntLinEqReif(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  const Literal r = m->GetTrueLiteral(ct.arguments[3]);
  m->model.Add(FixedWeightedSumReif(r, vars, coeffs, rhs));
}

void ExtractIntLinNeReif(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  const std::vector<int64>& coeffs = ct.arguments[0].values;
  const int64 rhs = ct.arguments[2].values[0];
  const Literal r = m->GetTrueLiteral(ct.arguments[3]);
  m->model.Add(FixedWeightedSumReif(r.Negated(), vars, coeffs, rhs));
}

// r => (a == cte).
void ImpliesEqualityToConstant(bool reverse_implication, IntegerVariable a,
                               int64 cte, Literal r, SatModel* m) {
  if (m->model.Get(IsFixed(a))) {
    if (m->model.Get(Value(a)) == IntegerValue(cte)) {
      if (reverse_implication) {
        m->model.GetOrCreate<SatSolver>()->AddUnitClause(r);
      }
    } else {
      m->model.GetOrCreate<SatSolver>()->AddUnitClause(r.Negated());
    }
    return;
  }

  // TODO(user): Simply do that all the time?
  // TODO(user): No need to create a literal that is trivially true or false!
  IntegerEncoder* encoder = m->model.GetOrCreate<IntegerEncoder>();
  if (!encoder->VariableIsFullyEncoded(a)) {
    if (reverse_implication) {
      m->model.Add(ReifiedInInterval(a, cte, cte, r));
    } else {
      const Literal ge = encoder->GetOrCreateAssociatedLiteral(
          IntegerLiteral::GreaterOrEqual(a, IntegerValue(cte)));
      const Literal le = encoder->GetOrCreateAssociatedLiteral(
          IntegerLiteral::LowerOrEqual(a, IntegerValue(cte)));
      m->model.Add(Implication(r, ge));
      m->model.Add(Implication(r, le));
    }
    return;
  }

  for (const auto pair : m->FullEncoding(a)) {
    if (pair.value == IntegerValue(cte)) {
      // Lit is equal to pair.literal.
      //
      // TODO(user): We could just use the same variable for this instead of
      // creating two and then making them equals.
      if (reverse_implication) {
        m->model.Add(Equality(r, pair.literal));
      } else {
        m->model.Add(Implication(r, pair.literal));
      }
      return;
    }
  }

  // Value is not found, the literal must be false.
  m->model.GetOrCreate<SatSolver>()->AddUnitClause(r.Negated());
}

// r => (a == b), and if reverse_implication is true, we have the other way
// around too.
//
// TODO(user): move this and ImpliesEqualityToConstant() under .../sat/ and unit
// test it!
void ImpliesEquality(bool reverse_implication, Literal r, IntegerVariable a,
                     IntegerVariable b, SatModel* m) {
  if (m->model.Get(IsFixed(a))) {
    ImpliesEqualityToConstant(reverse_implication, b, m->model.Get(Value(a)), r,
                              m);
    return;
  }
  if (m->model.Get(IsFixed(b))) {
    ImpliesEqualityToConstant(reverse_implication, a, m->model.Get(Value(b)), r,
                              m);
    return;
  }

  // TODO(user): Do that all the time?
  IntegerEncoder* encoder = m->model.GetOrCreate<IntegerEncoder>();
  if (!encoder->VariableIsFullyEncoded(a) ||
      !encoder->VariableIsFullyEncoded(b)) {
    if (reverse_implication) {
      m->model.Add(ReifiedEquality(a, b, r));
    } else if (a != b) {
      // If a == b, r can take any value.
      m->model.Add(ConditionalLowerOrEqualWithOffset(a, b, 0, r));
      m->model.Add(ConditionalLowerOrEqualWithOffset(b, a, 0, r));
    }
    return;
  }

  std::unordered_map<IntegerValue, std::vector<Literal>> by_value;
  for (const auto p : m->FullEncoding(a)) {
    by_value[p.value].push_back(p.literal);
  }
  for (const auto p : m->FullEncoding(b)) {
    by_value[p.value].push_back(p.literal);
  }
  for (const auto& p : by_value) {
    if (p.second.size() == 1) {
      // This value appear in only one of the variable, so if this value is
      // true then r must be false.
      m->model.Add(Implication(p.second[0], r.Negated()));
    } else {
      CHECK_EQ(p.second.size(), 2);
      const Literal a = p.second[0];
      const Literal b = p.second[1];
      // This value is common:
      // - a & b => r
      // - a & not(b) => not(r)
      // - not(a) & b => not(r)
      if (reverse_implication) {
        m->model.Add(ClauseConstraint({a.Negated(), b.Negated(), r}));
      }
      m->model.Add(ClauseConstraint({a.Negated(), b, r.Negated()}));
      m->model.Add(ClauseConstraint({a, b.Negated(), r.Negated()}));
    }
  }
}

void ExtractIntEqNeReif(const fz::Constraint& ct, bool eq, SatModel* m) {
  // The Eq or Ne version are the same up to the sign of the "eq" literal.
  Literal is_eq = m->GetTrueLiteral(ct.arguments[2]);
  if (!eq) is_eq = is_eq.Negated();

  if (ct.arguments[0].HasOneValue()) {
    ImpliesEqualityToConstant(/*reverse_implication=*/true,
                              m->LookupVar(ct.arguments[1]),
                              ct.arguments[0].Value(), is_eq, m);
    return;
  }

  if (ct.arguments[1].HasOneValue()) {
    ImpliesEqualityToConstant(/*reverse_implication=*/true,
                              m->LookupVar(ct.arguments[0]),
                              ct.arguments[1].Value(), is_eq, m);
    return;
  }

  // General case. This is exercised by the grid-colouring problems.
  ImpliesEquality(/*reverse_implication=*/true, is_eq,
                  m->LookupVar(ct.arguments[0]), m->LookupVar(ct.arguments[1]),
                  m);
}

// Special case added by the presolve (not in flatzinc). We encode this as a
// table constraint.
//
// TODO(user): is this the more efficient? we could at least optimize the table
// code to not create row literals when not needed.
void ExtractArray2dIntElement(const fz::Constraint& ct, SatModel* m) {
  CHECK_EQ(2, ct.arguments[0].variables.size());
  CHECK_EQ(5, ct.arguments.size());

  // the constraint is:
  //   values[coeff1 * vars[0] + coeff2 * vars[1] + offset] == target.
  std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[0]);
  const std::vector<int64>& values = ct.arguments[1].values;
  const int64 coeff1 = ct.arguments[3].values[0];
  const int64 coeff2 = ct.arguments[3].values[1];
  const int64 offset = ct.arguments[4].values[0] - 1;

  std::vector<std::vector<int64>> tuples;
  const auto encoding1 = m->FullEncoding(vars[0]);
  const auto encoding2 = m->FullEncoding(vars[1]);
  for (const auto& entry1 : encoding1) {
    const int64 v1 = entry1.value.value();
    for (const auto& entry2 : encoding2) {
      const int64 v2 = entry2.value.value();
      const int index = coeff1 * v1 + coeff2 * v2 + offset;
      CHECK_GE(index, 0);
      CHECK_LT(index, values.size());
      tuples.push_back({v1, v2, values[index]});
    }
  }
  vars.push_back(m->LookupVar(ct.arguments[2]));
  m->model.Add(TableConstraint(vars, tuples));
}

// TODO(user): move this logic in some model function under .../sat/ and unit
// test it! Or adapt the table constraint? this is like a table with 1 columns,
// the row literal beeing the one of ct.arguments[0].
void ExtractArrayIntElement(const fz::Constraint& ct, SatModel* m) {
  if (ct.arguments[0].type != fz::Argument::INT_VAR_REF) {
    return ExtractArray2dIntElement(ct, m);
  }

  std::map<int64, std::vector<Literal>> value_to_literals;
  {
    const auto encoding = m->FullEncoding(m->LookupVar(ct.arguments[0]));
    const std::vector<int64>& values = ct.arguments[1].values;
    if (encoding.size() != values.size()) {
      FZVLOG << "array_int_element could have been slightly presolved."
             << FZENDL;
    }
    for (const auto literal_value : encoding) {
      const int i = literal_value.value.value() - 1;  // minizinc use 1-index.
      CHECK_GE(i, 0);
      CHECK_LT(i, values.size());
      value_to_literals[values[i]].push_back(literal_value.literal);
    }
  }

  std::map<IntegerValue, Literal> target_by_value;
  const IntegerVariable target = m->LookupVar(ct.arguments[2]);
  for (const auto p : m->FullEncoding(target)) {
    target_by_value[p.value] = p.literal;
  }

  for (auto entry : value_to_literals) {
    // target == OR(entry.second), same as ExtractBoolOr().
    const Literal r = FindOrDie(target_by_value, IntegerValue(entry.first));
    for (const Literal literal : entry.second) {
      m->model.Add(Implication(literal, r));
    }

    // Note that this clause is not striclty needed because all the other
    // value of target will be false and so only the literals in entry.second
    // can be true out of all the literal of the argument 0.
    // TODO(user): remove?
    entry.second.push_back(r.Negated());
    m->model.Add(ClauseConstraint(entry.second));

    // We remove the entry from target_by_value to see if they all appear.
    target_by_value.erase(IntegerValue(entry.first));
  }

  if (!target_by_value.empty()) {
    FZLOG << "array_int_element could have been presolved." << FZENDL;
    for (const auto& entry : target_by_value) {
      m->model.GetOrCreate<SatSolver>()->AddUnitClause(entry.second.Negated());
    }
  }
}

// vars[i] == t.
void ExtractArrayVarIntElement(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[1]);
  const IntegerVariable t = m->LookupVar(ct.arguments[2]);

  CHECK(!ct.arguments[0].HasOneValue()) << "Should have been presolved.";
  const IntegerVariable index_var = m->LookupVar(ct.arguments[0]);
  if (m->model.Get(IsFixed(index_var))) {
    // TODO(user): use the full encoding if available.
    m->model.Add(Equality(vars[m->model.Get(Value(index_var)) - 1], t));
    return;
  }

  const auto encoding = m->FullEncoding(index_var);
  if (encoding.size() != vars.size()) {
    FZVLOG << "array_var_int_element could have been slightly presolved."
           << FZENDL;
  }

  std::vector<Literal> selectors;
  std::vector<IntegerVariable> possible_vars;
  for (const auto literal_value : encoding) {
    const int i = literal_value.value.value() - 1;  // minizinc use 1-index.
    CHECK_GE(i, 0);
    CHECK_LT(i, vars.size());
    possible_vars.push_back(vars[i]);
    selectors.push_back(literal_value.literal);
    ImpliesEquality(/*reverse_implication=*/false, literal_value.literal,
                    vars[i], t, m);
  }

  // TODO(user): make a IsOneOfVar() support the full propagation.
  m->model.Add(PartialIsOneOfVar(t, possible_vars, selectors));
}

void ExtractRegular(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[0]);
  const int64 num_states = ct.arguments[1].Value();
  const int64 num_values = ct.arguments[2].Value();

  const std::vector<int64>& next = ct.arguments[3].values;
  std::vector<std::vector<int64>> transitions;
  int count = 0;
  for (int i = 1; i <= num_states; ++i) {
    for (int j = 1; j <= num_values; ++j) {
      transitions.push_back({i, j, next[count++]});
    }
  }

  const int64 initial_state = ct.arguments[4].Value();

  std::vector<int64> final_states;
  switch (ct.arguments[5].type) {
    case fz::Argument::INT_VALUE: {
      final_states.push_back(ct.arguments[5].values[0]);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      for (int v = ct.arguments[5].values[0]; v <= ct.arguments[5].values[1];
           ++v) {
        final_states.push_back(v);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      final_states = ct.arguments[5].values;
      break;
    }
    default: { LOG(FATAL) << "Wrong constraint " << ct.DebugString(); }
  }

  m->model.Add(
      TransitionConstraint(vars, transitions, initial_state, final_states));
}

void ExtractTableInt(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[0]);
  const std::vector<int64>& t = ct.arguments[1].values;
  const int num_vars = vars.size();
  const int num_tuples = t.size() / num_vars;
  std::vector<std::vector<int64>> tuples(num_tuples);
  int count = 0;
  for (int i = 0; i < num_tuples; ++i) {
    for (int j = 0; j < num_vars; ++j) {
      tuples[i].push_back(t[count++]);
    }
  }
  m->model.Add(TableConstraint(vars, tuples));
}

void ExtractSetInReif(const fz::Constraint& ct, SatModel* m) {
  const IntegerVariable var = m->LookupVar(ct.arguments[0]);
  const Literal in_set = m->GetTrueLiteral(ct.arguments[2]);
  CHECK(!ct.arguments[0].HasOneValue()) << "Should be presolved: "
                                        << ct.DebugString();
  if (ct.arguments[1].HasOneValue()) {
    FZLOG << "Could have been presolved in int_eq_reif: " << ct.DebugString()
          << FZENDL;
  }
  if (ct.arguments[1].type == fz::Argument::INT_LIST) {
    std::set<int64> values(ct.arguments[1].values.begin(),
                      ct.arguments[1].values.end());
    const auto encoding = m->FullEncoding(var);
    for (const auto& literal_value : encoding) {
      if (ContainsKey(values, literal_value.value.value())) {
        m->model.Add(Implication(literal_value.literal, in_set));
      } else {
        m->model.Add(Implication(literal_value.literal, in_set.Negated()));
      }
    }
  } else if (ct.arguments[1].type == fz::Argument::INT_INTERVAL) {
    m->model.Add(ReifiedInInterval(var, ct.arguments[1].values[0],
                                   ct.arguments[1].values[1], in_set));
  } else {
    LOG(FATAL) << "Argument type not supported: " << ct.arguments[1].type;
  }
}

void ExtractAllDifferentInt(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[0]);
  IntegerEncoder* encoder = m->model.GetOrCreate<IntegerEncoder>();
  bool all_variables_are_encoded = true;
  for (const IntegerVariable v : vars) {
    if (!encoder->VariableIsFullyEncoded(v)) {
      all_variables_are_encoded = false;
      break;
    }
  }
  if (all_variables_are_encoded) {
    m->model.Add(AllDifferent(vars));
  } else {
    m->model.Add(AllDifferentOnBounds(vars));
  }
}

void ExtractDiffN(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> x = m->LookupVars(ct.arguments[0]);
  const std::vector<IntegerVariable> y = m->LookupVars(ct.arguments[1]);
  if (ct.arguments[2].type == fz::Argument::INT_LIST &&
      ct.arguments[3].type == fz::Argument::INT_LIST) {
    m->model.Add(StrictNonOverlappingFixedSizeRectangles(
        x, y, ct.arguments[2].values, ct.arguments[3].values));
  } else {
    const std::vector<IntegerVariable> dx = m->LookupVars(ct.arguments[2]);
    const std::vector<IntegerVariable> dy = m->LookupVars(ct.arguments[3]);
    m->model.Add(StrictNonOverlappingRectangles(x, y, dx, dy));
  }
}

void ExtractDiffNNonStrict(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> x = m->LookupVars(ct.arguments[0]);
  const std::vector<IntegerVariable> y = m->LookupVars(ct.arguments[1]);
  if (ct.arguments[2].type == fz::Argument::INT_LIST &&
      ct.arguments[3].type == fz::Argument::INT_LIST) {
    m->model.Add(NonOverlappingFixedSizeRectangles(x, y, ct.arguments[2].values,
                                                   ct.arguments[3].values));
  } else {
    const std::vector<IntegerVariable> dx = m->LookupVars(ct.arguments[2]);
    const std::vector<IntegerVariable> dy = m->LookupVars(ct.arguments[3]);
    m->model.Add(NonOverlappingRectangles(x, y, dx, dy));
  }
}

void ExtractCumulative(const fz::Constraint& ct, SatModel* m) {
  const std::vector<IntegerVariable> starts = m->LookupVars(ct.arguments[0]);
  const std::vector<IntegerVariable> durations = m->LookupVars(ct.arguments[1]);
  const std::vector<IntegerVariable> demands = m->LookupVars(ct.arguments[2]);
  const IntegerVariable capacity = m->LookupVar(ct.arguments[3]);

  // Convert the couple (starts, duration) into an interval variable.
  std::vector<IntervalVariable> intervals;
  for (int i = 0; i < starts.size(); ++i) {
    intervals.push_back(
        m->model.Add(NewIntervalFromStartAndSizeVars(starts[i], durations[i])));
  }

  m->model.Add(Cumulative(intervals, demands, capacity));
}

void ExtractCircuit(const fz::Constraint& ct, bool allow_subcircuit,
                    SatModel* m) {
  bool found_zero = false;
  bool found_size = false;
  for (fz::IntegerVariable* const var : ct.arguments[0].variables) {
    if (var->domain.Min() == 0) {
      found_zero = true;
    }
    if (var->domain.Max() == ct.arguments[0].variables.size()) {
      found_size = true;
    }
  }
  // Are array 1 based or 0 based.
  const int offset = found_zero && !found_size ? 0 : 1;

  const std::vector<IntegerVariable> vars = m->LookupVars(ct.arguments[0]);
  std::vector<std::vector<LiteralIndex>> graph(
      vars.size(), std::vector<LiteralIndex>(vars.size(), kFalseLiteralIndex));
  for (int i = 0; i < vars.size(); ++i) {
    if (m->model.Get(IsFixed(vars[i]))) {
      graph[i][m->model.Get(Value(vars[i])) - offset] = kTrueLiteralIndex;
    } else {
      const auto encoding = m->FullEncoding(vars[i]);
      for (const auto& entry : encoding) {
        graph[i][entry.value.value() - offset] = entry.literal.Index();
      }
    }
  }
  m->model.Add(allow_subcircuit ? SubcircuitConstraint(graph)
                                : CircuitConstraint(graph));
}

// network_flow(arcs, balance, flow)
// network_flow_cost(arcs, balance, weight, flow, cost)
void ExtractNetworkFlow(const fz::Constraint& ct, SatModel* m) {
  const bool has_cost = ct.type == "network_flow_cost";
  const std::vector<IntegerVariable> flow =
      m->LookupVars(ct.arguments[has_cost ? 3 : 2]);

  // First, encode the flow conservation constraints as sums for performance:
  // updating balance variables is done faster locally.
  const int num_nodes = ct.arguments[1].values.size();
  std::vector<std::vector<IntegerVariable>> flows_per_node(num_nodes);
  std::vector<std::vector<int>> coeffs_per_node(num_nodes);

  const int num_arcs = ct.arguments[0].values.size() / 2;
  for (int arc = 0; arc < num_arcs; arc++) {
    const int tail = ct.arguments[0].values[2 * arc] - 1;
    flows_per_node[tail].push_back(flow[arc]);
    coeffs_per_node[tail].push_back(1);

    const int head = ct.arguments[0].values[2 * arc + 1] - 1;
    flows_per_node[head].push_back(flow[arc]);
    coeffs_per_node[head].push_back(-1);
  }

  for (int node = 0; node < num_nodes; node++) {
    m->model.Add(FixedWeightedSum(flows_per_node[node], coeffs_per_node[node],
                                  ct.arguments[1].values[node]));
  }

  if (has_cost) {
    std::vector<IntegerVariable> filtered_flows;
    std::vector<int64> filtered_costs;

    for (int arc = 0; arc < num_arcs; arc++) {
      const int64 weight = ct.arguments[2].values[arc];
      if (weight == 0) continue;

      filtered_flows.push_back(flow[arc]);
      filtered_costs.push_back(weight);
    }

    filtered_flows.push_back(m->LookupVar(ct.arguments[4]));
    filtered_costs.push_back(-1);

    m->model.Add(FixedWeightedSum(filtered_flows, filtered_costs, 0));
  }

  // Then pass the problem to global FlowCosts constraint.
  std::vector<IntegerVariable> balance;
  for (const int value : ct.arguments[1].values) {
    balance.push_back(m->model.Add(ConstantIntegerVariable(value)));
  }

  const std::vector<int64> arcs = ct.arguments[0].values;
  std::vector<int> tails;
  std::vector<int> heads;
  for (int arc = 0; arc < num_arcs; arc++) {
    tails.push_back(arcs[2 * arc] - 1);
    heads.push_back(arcs[2 * arc + 1] - 1);
  }

  std::vector<std::vector<int>> weights_per_cost_type;
  if (has_cost) {
    std::vector<int> weights;
    for (const int64 value : ct.arguments[2].values) {
      weights.push_back(static_cast<int>(value));
    }
    weights_per_cost_type.push_back(weights);
  }

  std::vector<IntegerVariable> total_costs_per_cost_type;
  if (has_cost) {
    total_costs_per_cost_type.push_back(m->LookupVar(ct.arguments[4]));
  }

  m->model.Add(FlowCostsConstraint(balance, flow, tails, heads,
                                   weights_per_cost_type,
                                   total_costs_per_cost_type));
}

// Returns false iff the constraint type is not supported.
bool ExtractConstraint(const fz::Constraint& ct, SatModel* m) {
  if (ct.type == "bool_eq") {
    ExtractBoolEq(ct, m);
  } else if (ct.type == "bool_eq_reif") {
    ExtractBoolEqNeReif(/*is_eq=*/true, ct, m);
  } else if (ct.type == "bool_ne" || ct.type == "bool_not") {
    ExtractBoolNe(ct, m);
  } else if (ct.type == "bool_ne_reif") {
    ExtractBoolEqNeReif(/*is_eq=*/false, ct, m);
  } else if (ct.type == "bool_le") {
    ExtractBoolLe(ct, m);
  } else if (ct.type == "bool_le_reif") {
    ExtractBoolLeLtReif(/*is_le=*/true, ct, m);
  } else if (ct.type == "bool_lt_reif") {
    ExtractBoolLeLtReif(/*is_le=*/false, ct, m);
  } else if (ct.type == "bool_clause") {
    ExtractBoolClause(ct, m);
  } else if (ct.type == "array_bool_and") {
    ExtractArrayBoolAnd(ct, m);
  } else if (ct.type == "array_bool_or") {
    ExtractArrayBoolOr(ct, m);
  } else if (ct.type == "array_bool_xor") {
    ExtractArrayBoolXor(ct, m);
  } else if (ct.type == "int_min") {
    ExtractIntMin(ct, m);
  } else if (ct.type == "int_abs") {
    ExtractIntAbs(ct, m);
  } else if (ct.type == "int_max") {
    ExtractIntMax(ct, m);
  } else if (ct.type == "int_times") {
    ExtractIntTimes(ct, m);
  } else if (ct.type == "int_div") {
    ExtractIntDiv(ct, m);
  } else if (ct.type == "int_plus") {
    ExtractIntPlus(ct, m);
  } else if (ct.type == "array_int_minimum" || ct.type == "minimum_int") {
    ExtractArrayIntMinimum(ct, m);
  } else if (ct.type == "array_int_maximum" || ct.type == "maximum_int") {
    ExtractArrayIntMaximum(ct, m);
  } else if (ct.type == "array_int_element" ||
             ct.type == "array_bool_element") {
    ExtractArrayIntElement(ct, m);
  } else if (ct.type == "array_var_int_element" ||
             ct.type == "array_var_bool_element") {
    ExtractArrayVarIntElement(ct, m);
  } else if (ct.type == "all_different_int") {
    ExtractAllDifferentInt(ct, m);
  } else if (ct.type == "int_eq" || ct.type == "bool2int") {
    ExtractIntEq(ct, m);
  } else if (ct.type == "int_ne") {
    ExtractIntNe(ct, m);
  } else if (ct.type == "int_le") {
    ExtractIntLe(ct, m);
  } else if (ct.type == "int_ge") {
    ExtractIntGe(ct, m);
  } else if (ct.type == "int_lt") {
    ExtractIntLt(ct, m);
  } else if (ct.type == "int_le_reif") {
    ExtractIntLeGeReif(/*is_le=*/true, ct, m);
  } else if (ct.type == "int_ge_reif") {
    ExtractIntLeGeReif(/*is_le=*/false, ct, m);
  } else if (ct.type == "int_lt_reif") {
    ExtractIntLtReif(ct, m);
  } else if (ct.type == "int_eq_reif") {
    ExtractIntEqNeReif(ct, /*eq=*/true, m);
  } else if (ct.type == "int_ne_reif") {
    ExtractIntEqNeReif(ct, /*eq=*/false, m);
  } else if (ct.type == "int_lin_eq") {
    ExtractIntLinEq(ct, m);
  } else if (ct.type == "int_lin_ne") {
    ExtractIntLinNe(ct, m);
  } else if (ct.type == "int_lin_le") {
    ExtractIntLinLe(ct, m);
  } else if (ct.type == "int_lin_ge") {
    ExtractIntLinGe(ct, m);
  } else if (ct.type == "int_lin_eq_reif") {
    ExtractIntLinEqReif(ct, m);
  } else if (ct.type == "int_lin_ne_reif") {
    ExtractIntLinNeReif(ct, m);
  } else if (ct.type == "int_lin_le_reif") {
    ExtractIntLinLeReif(ct, m);
  } else if (ct.type == "int_lin_ge_reif") {
    ExtractIntLinGeReif(ct, m);
  } else if (ct.type == "circuit") {
    ExtractCircuit(ct, /*allow_subcircuit=*/false, m);
  } else if (ct.type == "subcircuit") {
    ExtractCircuit(ct, /*allow_subcircuit=*/true, m);
  } else if (ct.type == "regular") {
    ExtractRegular(ct, m);
  } else if (ct.type == "table_int") {
    ExtractTableInt(ct, m);
  } else if (ct.type == "set_in_reif") {
    ExtractSetInReif(ct, m);
  } else if (ct.type == "diffn") {
    ExtractDiffN(ct, m);
  } else if (ct.type == "diffn_nonstrict") {
    ExtractDiffNNonStrict(ct, m);
  } else if (ct.type == "cumulative" || ct.type == "var_cumulative" ||
             ct.type == "variable_cumulative" ||
             ct.type == "fixed_cumulative") {
    ExtractCumulative(ct, m);
  } else if (ct.type == "network_flow" || ct.type == "network_flow_cost") {
    ExtractNetworkFlow(ct, m);
  } else if (ct.type == "false_constraint") {
    m->model.GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
  } else {
    return false;
  }
  return true;
}

// =============================================================================
// SAT/CP flatzinc solver.
// =============================================================================

// The format is fixed in the flatzinc specification.
std::string SolutionString(const SatModel& m,
                      const fz::SolutionOutputSpecs& output) {
  if (output.variable != nullptr) {
    const int64 value = m.Value(output.variable);
    if (output.display_as_boolean) {
      return StringPrintf("%s = %s;", output.name.c_str(),
                          value == 1 ? "true" : "false");
    } else {
      return StringPrintf("%s = %" GG_LL_FORMAT "d;", output.name.c_str(),
                          value);
    }
  } else {
    const int bound_size = output.bounds.size();
    std::string result =
        StringPrintf("%s = array%dd(", output.name.c_str(), bound_size);
    for (int i = 0; i < bound_size; ++i) {
      if (output.bounds[i].max_value != 0) {
        result.append(StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d, ",
                                   output.bounds[i].min_value,
                                   output.bounds[i].max_value));
      } else {
        result.append("{},");
      }
    }
    result.append("[");
    for (int i = 0; i < output.flat_variables.size(); ++i) {
      const int64 value = m.Value(output.flat_variables[i]);
      if (output.display_as_boolean) {
        result.append(StringPrintf(value ? "true" : "false"));
      } else {
        StrAppend(&result, value);
      }
      if (i != output.flat_variables.size() - 1) {
        result.append(", ");
      }
    }
    result.append("]);");
    return result;
  }
  return "";
}

std::string CheckSolutionAndGetFzString(const fz::Model& fz_model,
                                   const SatModel& m) {
  CHECK(CheckSolution(fz_model,
                      [&m](fz::IntegerVariable* v) { return m.Value(v); }));

  std::string solution_string;
  for (const fz::SolutionOutputSpecs& output : fz_model.output()) {
    solution_string.append(SolutionString(m, output));
    solution_string.append("\n");
  }
  return solution_string + "----------\n";
}

}  // namespace

void SolveWithSat(const fz::Model& fz_model, const fz::FlatzincParameters& p,
                  bool* interrupt_solve) {
  // Timing.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  SatModel m;
  std::unique_ptr<TimeLimit> time_limit;
  if (p.time_limit_in_ms > 0) {
    time_limit.reset(new TimeLimit(p.time_limit_in_ms * 1e-3));
  } else {
    time_limit = TimeLimit::Infinite();
  }
  time_limit->RegisterExternalBooleanAsLimit(interrupt_solve);
  m.model.SetSingleton(std::move(time_limit));

  // Process the bool_not constraints to avoid creating extra Boolean variables.
  std::unordered_map<fz::IntegerVariable*, fz::IntegerVariable*> not_map;
  for (fz::Constraint* ct : fz_model.constraints()) {
    if (ct != nullptr && ct->active &&
        (ct->type == "bool_not" || ct->type == "bool_ne")) {
      not_map[ct->arguments[0].Var()] = ct->arguments[1].Var();
      not_map[ct->arguments[1].Var()] = ct->arguments[0].Var();
    }
  }

  // Extract all the variables.
  int num_constants = 0;
  int num_variables_with_two_values = 0;
  std::set<int64> constant_values;
  std::map<std::string, int> num_vars_per_domains;
  FZLOG << "Extracting " << fz_model.variables().size() << " variables. "
        << FZENDL;
  int num_capped_variables = 0;
  for (fz::IntegerVariable* var : fz_model.variables()) {
    if (!var->active) continue;

    // Will be encoded as a constant lazily as needed.
    if (var->domain.HasOneValue()) {
      ++num_constants;
      constant_values.insert(var->domain.Value());
      continue;
    }

    const int64 safe_min =
        var->domain.Min() == kint64min ? kint32min : var->domain.Min();
    const int64 safe_max =
        var->domain.Max() == kint64max ? kint32max : var->domain.Max();
    if (safe_min != var->domain.Min() || safe_max != var->domain.Max()) {
      num_capped_variables++;
    }

    // Special case for Boolean. We don't automatically create the associated
    // integer variable. It will only be created if a constraint needs to see
    // the Boolean variable as an IntegerVariable
    if (var->domain.Min() == 0 && var->domain.Max() == 1) {
      const Literal literal =
          ContainsKey(not_map, var) && ContainsKey(m.bool_map, not_map[var])
              ? m.bool_map[not_map[var]].Negated()
              : Literal(m.model.Add(NewBooleanVariable()), true);
      InsertOrDie(&m.bool_map, var, literal);
      continue;
    }

    // Create the associated sat::IntegerVariable. Note that it will be lazily
    // fully-encoded by the propagators that need it, except for the variables
    // with just two values because it seems more efficient to do so.
    //
    // TODO(user): Experiment more with proactive full-encoding. Chuffed seems
    // to fully encode all variables with a small domain.
    std::string domain_as_string;
    bool only_two_values = false;
    if (var->domain.is_interval) {
      only_two_values = (safe_min + 1 == safe_max);
      domain_as_string = ClosedInterval({safe_min, safe_max}).DebugString();
      InsertOrDie(&m.var_map, var,
                  m.model.Add(NewIntegerVariable(safe_min, safe_max)));
    } else {
      only_two_values = (var->domain.values.size() == 2);
      const std::vector<ClosedInterval> domain =
          SortedDisjointIntervalsFromValues(var->domain.values);
      InsertOrDie(&m.var_map, var, m.model.Add(NewIntegerVariable(domain)));
      domain_as_string = IntervalsAsString(domain);
    }
    num_vars_per_domains[domain_as_string]++;

    if (only_two_values) {
      ++num_variables_with_two_values;
      m.model.Add(FullyEncodeVariable(m.LookupVar(var)));
    }
  }
  for (const auto& entry : num_vars_per_domains) {
    FZLOG << " - " << entry.second << " vars in " << entry.first << FZENDL;
  }
  FZLOG << " - " << num_constants << " constants in {"
        << strings::Join(constant_values, ",") << "}." << FZENDL;
  if (num_capped_variables > 0) {
    FZLOG << " - " << num_capped_variables
          << " variables have been capped to fit into [int32min .. int32max]"
          << FZENDL;
  }

  // Extract all the constraints.
  FZLOG << "Extracting " << fz_model.constraints().size() << " constraints. "
        << FZENDL;
  std::set<std::string> unsupported_types;
  Trail* trail = m.model.GetOrCreate<Trail>();
  for (fz::Constraint* ct : fz_model.constraints()) {
    if (ct != nullptr && ct->active) {
      const int old_num_fixed = trail->Index();
      FZVLOG << "Extracting '" << ct->type << "'." << FZENDL;
      if (!ExtractConstraint(*ct, &m)) {
        unsupported_types.insert(ct->type);
      }

      // We propagate after each new Boolean constraint but not the integer
      // ones. So we call Propagate() manually here. TODO(user): Do that
      // automatically?
      m.model.GetOrCreate<SatSolver>()->Propagate();
      if (trail->Index() > old_num_fixed) {
        FZVLOG << "Constraint fixed " << trail->Index() - old_num_fixed
               << " Boolean variable(s): " << ct->DebugString() << FZENDL;
      }
      if (m.model.GetOrCreate<SatSolver>()->IsModelUnsat()) {
        FZLOG << "UNSAT during extraction (after adding '" << ct->type << "')."
              << FZENDL;
        break;
      }
    }
  }
  if (!unsupported_types.empty()) {
    FZLOG << "There are unsupported constraints types in this model: "
          << FZENDL;
    for (const std::string& type : unsupported_types) {
      FZLOG << " - " << type << FZENDL;
    }
    return;
  }

  // Use LinearProgrammingConstraint only if there was a linear inequality,
  // i.e. if it is already instantiated in the model.
  if (FLAGS_fz_use_lp_constraint &&
      m.model.Get<LinearProgrammingConstraint>() != nullptr) {
    LinearProgrammingConstraint* lp =
        m.model.GetOrCreate<LinearProgrammingConstraint>();
    lp->RegisterWith(m.model.GetOrCreate<GenericLiteralWatcher>());
  }

  // Some stats.
  {
    int num_bool_as_int = 0;
    for (auto entry : m.bool_map) {
      if (ContainsKey(m.var_map, entry.first)) ++num_bool_as_int;
    }
    int num_fully_encoded_variables = 0;
    for (int i = 0;
         i < m.model.GetOrCreate<IntegerTrail>()->NumIntegerVariables(); ++i) {
      if (m.model.Get<IntegerEncoder>()->VariableIsFullyEncoded(
              IntegerVariable(i))) {
        ++num_fully_encoded_variables;
      }
    }
    // We divide by two because of the automatically created NegationOf() var.
    FZLOG << "Num integer variables = "
          << m.model.GetOrCreate<IntegerTrail>()->NumIntegerVariables() / 2
          << " (" << num_bool_as_int << " Booleans)." << FZENDL;
    FZLOG << "Num fully encoded variable = " << num_fully_encoded_variables / 2
          << FZENDL;
    FZLOG << "Num initial SAT variables = "
          << m.model.Get<SatSolver>()->NumVariables() << " ("
          << m.model.Get<SatSolver>()->LiteralTrail().Index() << " fixed)."
          << FZENDL;
    FZLOG << "Num vars with 2 values = " << num_variables_with_two_values
          << FZENDL;
    FZLOG << "Num constants = "
          << m.model.Get<IntegerTrail>()->NumConstantVariables() << FZENDL;
    FZLOG << "Num integer propagators = "
          << m.model.GetOrCreate<GenericLiteralWatcher>()->NumPropagators()
          << FZENDL;
  }

  int num_solutions = 0;
  int64 best_objective = 0;
  std::string solutions_string;
  std::string search_status;

  // Important: we use the order of the variable from flatzinc with the
  // non-defined variable first. In particular we don't want to iterate on
  // m.var_map which order is randomized!
  //
  // TODO(user): We could restrict these if we are sure all the other variables
  // will be fixed once these are fixed.
  std::vector<IntegerVariable> decision_vars;
  for (fz::IntegerVariable* var : fz_model.variables()) {
    if (!var->active || var->domain.HasOneValue()) continue;
    if (var->defining_constraint != nullptr) continue;
    if (ContainsKey(m.bool_map, var)) continue;
    decision_vars.push_back(FindOrDie(m.var_map, var));
  }
  for (fz::IntegerVariable* var : fz_model.variables()) {
    if (!var->active || var->domain.HasOneValue()) continue;
    if (var->defining_constraint == nullptr) continue;
    if (ContainsKey(m.bool_map, var)) continue;
    decision_vars.push_back(FindOrDie(m.var_map, var));
  }

  // TODO(user): deal with other search parameters.
  FZLOG << "Solving..." << FZENDL;
  SatSolver::Status status;
  if (fz_model.objective() == nullptr) {
    // Decision problem.
    while (num_solutions < p.num_solutions) {
      status = SolveIntegerProblemWithLazyEncoding(
          /*assumptions=*/{},
          FirstUnassignedVarAtItsMinHeuristic(decision_vars, &m.model),
          &m.model);

      if (status == SatSolver::MODEL_SAT) {
        ++num_solutions;
        FZLOG << "Solution #" << num_solutions
              << " num_bool:" << m.model.Get<SatSolver>()->NumVariables()
              << FZENDL;
        solutions_string += CheckSolutionAndGetFzString(fz_model, m);
        if (num_solutions < p.num_solutions) {
          m.model.Add(ExcludeCurrentSolutionAndBacktrack());
        }
        continue;
      }

      if (status == SatSolver::MODEL_UNSAT) {
        if (num_solutions == 0) {
          search_status = "=====UNSATISFIABLE=====";
        }
        break;
      }

      // Limit reached.
      break;
    }
  } else {
    // Optimization problem.
    const IntegerVariable objective_var = m.LookupVar(fz_model.objective());
    status = MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
        /*log_info=*/false,
        fz_model.maximize() ? NegationOf(objective_var) : objective_var,
        FirstUnassignedVarAtItsMinHeuristic(decision_vars, &m.model),
        [objective_var, &num_solutions, &m, &fz_model, &best_objective,
         &solutions_string](const Model& sat_model) {
          num_solutions++;
          best_objective = sat_model.Get(LowerBound(objective_var));
          FZLOG << "Solution #" << num_solutions << " obj:" << best_objective
                << " num_bool:" << sat_model.Get<SatSolver>()->NumVariables()
                << FZENDL;
          solutions_string = CheckSolutionAndGetFzString(fz_model, m);
        },
        &m.model);
    if (num_solutions > 0) {
      search_status = "==========";
    } else {
      search_status = "=====UNSATISFIABLE=====";
    }
  }

  if (fz_model.objective() == nullptr) {
    FZLOG << "Status: " << status << FZENDL;
    FZLOG << "Objective: NA" << FZENDL;
    FZLOG << "Best_bound: NA" << FZENDL;
  } else {
    m.model.GetOrCreate<SatSolver>()->Backtrack(0);
    const IntegerVariable objective_var = m.LookupVar(fz_model.objective());
    int64 best_bound =
        m.model.Get(fz_model.maximize() ? UpperBound(objective_var)
                                        : LowerBound(objective_var));
    if (num_solutions == 0) {
      FZLOG << "Status: " << status << FZENDL;
      FZLOG << "Objective: NA" << FZENDL;
    } else {
      if (status == SatSolver::MODEL_SAT) {
        FZLOG << "Status: OPTIMAL" << FZENDL;

        // We need this because even if we proved unsat, that doesn't mean we
        // propagated the best bound to its current value.
        best_bound = best_objective;
      } else {
        FZLOG << "Status: " << status << FZENDL;
      }
      FZLOG << "Objective: " << best_objective << FZENDL;
    }
    FZLOG << "Best_bound: " << best_bound;
  }
  FZLOG << "Booleans: " << m.model.Get<SatSolver>()->NumVariables() << FZENDL;
  FZLOG << "Conflicts: " << m.model.Get<SatSolver>()->num_failures() << FZENDL;
  FZLOG << "Branches: " << m.model.Get<SatSolver>()->num_branches() << FZENDL;
  FZLOG << "Propagations: " << m.model.Get<SatSolver>()->num_propagations()
        << FZENDL;
  FZLOG << "Walltime: " << wall_timer.Get() << FZENDL;
  FZLOG << "Usertime: " << user_timer.Get() << FZENDL;
  FZLOG << "Deterministic_time: "
        << m.model.Get<SatSolver>()->deterministic_time() << FZENDL;

  if (status == SatSolver::LIMIT_REACHED) {
    search_status = "%% LIMIT_REACHED";
  }

  // Print the solution(s).
  std::cout << solutions_string;
  if (!search_status.empty()) {
    std::cout << search_status << std::endl;
  }
}

}  // namespace sat
}  // namespace operations_research
