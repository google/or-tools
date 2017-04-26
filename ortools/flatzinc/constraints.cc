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

#include "ortools/flatzinc/constraints.h"

#include <string>
#include <unordered_set>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/flatzinc/flatzinc_constraints.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/sat_constraint.h"
#include "ortools/flatzinc/solver_data.h"
#include "ortools/util/string_array.h"

DECLARE_bool(fz_use_sat);

// TODO(user): minizinc 2.0 support: arg_sort, geost
// TODO(user): Do we need to support knapsack and network_flow?
// TODO(user): Support alternative, span, disjunctive, cumulative with
//                optional variables.

namespace operations_research {
namespace {

void AddConstraint(Solver* s, fz::Constraint* ct, Constraint* cte) {
  FZVLOG << "  - post " << cte->DebugString() << FZENDL;
  s->AddConstraint(cte);
}

void PostBooleanSumInRange(SatPropagator* sat, Solver* solver,
                           const std::vector<IntVar*>& variables,
                           int64 range_min, int64 range_max) {
  // TODO(user): Use sat_solver::AddLinearConstraint()
  const int64 size = variables.size();
  range_min = std::max(0LL, range_min);
  range_max = std::min(size, range_max);
  int true_vars = 0;
  std::vector<IntVar*> alt;
  for (int i = 0; i < size; ++i) {
    if (!variables[i]->Bound()) {
      alt.push_back(variables[i]);
    } else if (variables[i]->Min() == 1) {
      true_vars++;
    }
  }
  const int possible_vars = alt.size();
  range_min -= true_vars;
  range_max -= true_vars;

  if (range_max < 0 || range_min > possible_vars) {
    Constraint* const ct = solver->MakeFalseConstraint();
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  } else if (range_min <= 0 && range_max >= possible_vars) {
    FZVLOG << "  - ignore true constraint" << FZENDL;
  } else if (FLAGS_fz_use_sat &&
             AddSumInRange(sat, alt, range_min, range_max)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_fz_use_sat && range_min == 0 && range_max == 1 &&
             AddAtMostOne(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_fz_use_sat && range_min == 0 && range_max == size - 1 &&
             AddAtMostNMinusOne(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_fz_use_sat && range_min == 1 && range_max == 1 &&
             AddBoolOrArrayEqualTrue(sat, alt) && AddAtMostOne(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_fz_use_sat && range_min == 1 && range_max == possible_vars &&
             AddBoolOrArrayEqualTrue(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else {
    Constraint* const ct =
        MakeBooleanSumInRange(solver, alt, range_min, range_max);
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  }
}

void PostIsBooleanSumInRange(SatPropagator* sat, Solver* solver,
                             const std::vector<IntVar*>& variables,
                             int64 range_min, int64 range_max, IntVar* target) {
  const int64 size = variables.size();
  range_min = std::max(0LL, range_min);
  range_max = std::min(size, range_max);
  int true_vars = 0;
  int possible_vars = 0;
  for (int i = 0; i < size; ++i) {
    if (variables[i]->Max() == 1) {
      possible_vars++;
      if (variables[i]->Min() == 1) {
        true_vars++;
      }
    }
  }
  if (true_vars > range_max || possible_vars < range_min) {
    target->SetValue(0);
    FZVLOG << "  - set target to 0" << FZENDL;
  } else if (true_vars >= range_min && possible_vars <= range_max) {
    target->SetValue(1);
    FZVLOG << "  - set target to 1" << FZENDL;
  } else if (FLAGS_fz_use_sat && range_min == size &&
             AddBoolAndArrayEqVar(sat, variables, target)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_fz_use_sat && range_max == 0 &&
             AddBoolOrArrayEqVar(sat, variables,
                                 solver->MakeDifference(1, target)->Var())) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_fz_use_sat && range_min == 1 && range_max == size &&
             AddBoolOrArrayEqVar(sat, variables, target)) {
    FZVLOG << "  - posted to sat" << FZENDL;
    // TODO(user): Implement range_min == 0 and range_max = size - 1.
  } else {
    Constraint* const ct = MakeIsBooleanSumInRange(solver, variables, range_min,
                                                   range_max, target);
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  }
}

void PostIsBooleanSumDifferent(SatPropagator* sat, Solver* solver,
                               const std::vector<IntVar*>& variables,
                               int64 value, IntVar* target) {
  const int64 size = variables.size();
  if (value == 0) {
    PostIsBooleanSumInRange(sat, solver, variables, 1, size, target);
  } else if (value == size) {
    PostIsBooleanSumInRange(sat, solver, variables, 0, size - 1, target);
  } else {
    Constraint* const ct =
        solver->MakeIsDifferentCstCt(solver->MakeSum(variables), value, target);
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  }
}

void ExtractAllDifferentInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  const std::vector<IntVar*> vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  Constraint* const constraint = s->MakeAllDifferent(vars, vars.size() < 100);
  AddConstraint(s, ct, constraint);
}

void ExtractAlldifferentExcept0(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  const std::vector<IntVar*> vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  AddConstraint(s, ct, s->MakeAllDifferentExcept(vars, 0));
}

void ExtractAmong(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> tmp_sum;
  const std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  for (IntVar* const var : tmp_vars) {
    const fz::Argument& arg = ct->arguments[2];
    switch (arg.type) {
      case fz::Argument::INT_VALUE: {
        tmp_sum.push_back(solver->MakeIsEqualCstVar(var, arg.values[0]));
        break;
      }
      case fz::Argument::INT_INTERVAL: {
        if (var->Min() < arg.values[0] || var->Max() > arg.values[1]) {
          tmp_sum.push_back(
              solver->MakeIsBetweenVar(var, arg.values[0], arg.values[1]));
        }
        break;
      }
      case fz::Argument::INT_LIST: {
        tmp_sum.push_back(solver->MakeIsMemberVar(var, arg.values));
        break;
      }
      default: { LOG(FATAL) << "Invalid constraint " << ct->DebugString(); }
    }
  }
  if (ct->arguments[0].HasOneValue()) {
    const int64 count = ct->arguments[0].Value();
    Constraint* const constraint = solver->MakeSumEquality(tmp_sum, count);
    AddConstraint(solver, ct, constraint);
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[0])->Var();
    Constraint* const constraint = solver->MakeSumEquality(tmp_sum, count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractAtMostInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const int64 max_count = ct->arguments[0].Value();
  const int64 value = ct->arguments[2].Value();
  const std::vector<IntVar*> vars =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  Constraint* const constraint = solver->MakeAtMost(vars, value, max_count);
  AddConstraint(solver, ct, constraint);
}

void ExtractArrayBoolAnd(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> variables;
  std::unordered_set<IntExpr*> added;
  const std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  for (IntVar* const to_add : tmp_vars) {
    if (!ContainsKey(added, to_add) && to_add->Min() != 1) {
      variables.push_back(to_add);
      added.insert(to_add);
    }
  }
  if (ct->target_variable != nullptr) {
    IntExpr* const boolvar = solver->MakeMin(variables);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << boolvar->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, boolvar);
  } else if (ct->arguments[1].HasOneValue() && ct->arguments[1].Value() == 1) {
    FZVLOG << "  - forcing array_bool_and to 1" << FZENDL;
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(1);
    }
  } else {
    if (ct->arguments[1].HasOneValue()) {
      if (ct->arguments[1].Value() == 0) {
        if (FLAGS_fz_use_sat &&
            AddBoolAndArrayEqualFalse(data->Sat(), variables)) {
          FZVLOG << "  - posted to sat" << FZENDL;
        } else {
          Constraint* const constraint =
              solver->MakeSumLessOrEqual(variables, variables.size() - 1);
          AddConstraint(solver, ct, constraint);
        }
      } else {
        Constraint* const constraint =
            solver->MakeSumEquality(variables, variables.size());
        AddConstraint(solver, ct, constraint);
      }
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[1])->Var();
      if (FLAGS_fz_use_sat &&
          AddBoolAndArrayEqVar(data->Sat(), variables, boolvar)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeMinEquality(variables, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractArrayBoolOr(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> variables;
  std::unordered_set<IntExpr*> added;
  const std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  for (IntVar* const to_add : tmp_vars) {
    if (!ContainsKey(added, to_add) && to_add->Max() != 0) {
      variables.push_back(to_add);
      added.insert(to_add);
    }
  }
  if (ct->target_variable != nullptr) {
    IntExpr* const boolvar = solver->MakeMax(variables);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << boolvar->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, boolvar);
  } else if (ct->arguments[1].HasOneValue() && ct->arguments[1].Value() == 0) {
    FZVLOG << "  - forcing array_bool_or to 0" << FZENDL;
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(0);
    }
  } else {
    if (ct->arguments[1].HasOneValue()) {
      if (ct->arguments[1].Value() == 1) {
        if (FLAGS_fz_use_sat &&
            AddBoolOrArrayEqualTrue(data->Sat(), variables)) {
          FZVLOG << "  - posted to sat" << FZENDL;
        } else {
          Constraint* const constraint =
              solver->MakeSumGreaterOrEqual(variables, 1);
          AddConstraint(solver, ct, constraint);
        }
      } else {
        Constraint* const constraint =
            solver->MakeSumEquality(variables, Zero());
        AddConstraint(solver, ct, constraint);
      }
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[1])->Var();
      if (FLAGS_fz_use_sat &&
          AddBoolOrArrayEqVar(data->Sat(), variables, boolvar)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeMaxEquality(variables, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractArrayBoolXor(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  std::vector<IntVar*> variables;
  bool even = true;
  for (IntVar* const var : tmp_vars) {
    if (var->Max() == 1) {
      if (var->Min() == 1) {
        even = !even;
      } else {
        variables.push_back(var);
      }
    }
  }
  switch (variables.size()) {
    case 0: {
      Constraint* const constraint =
          even ? solver->MakeFalseConstraint() : solver->MakeTrueConstraint();
      AddConstraint(solver, ct, constraint);
      break;
    }
    case 1: {
      Constraint* const constraint =
          solver->MakeEquality(variables.front(), even);
      AddConstraint(solver, ct, constraint);
      break;
    }
    case 2: {
      if (even) {
        if (FLAGS_fz_use_sat &&
            AddBoolNot(data->Sat(), variables[0], variables[1])) {
          FZVLOG << "  - posted to sat" << FZENDL;
        } else {
          PostBooleanSumInRange(data->Sat(), solver, variables, 1, 1);
        }
      } else {
        if (FLAGS_fz_use_sat &&
            AddBoolEq(data->Sat(), variables[0], variables[1])) {
          FZVLOG << "  - posted to sat" << FZENDL;
        } else {
          variables.push_back(solver->MakeIntConst(1));
          Constraint* const constraint = MakeBooleanSumOdd(solver, variables);
          AddConstraint(solver, ct, constraint);
        }
      }
      break;
    }
    default: {
      if (!even) {
        variables.push_back(solver->MakeIntConst(1));
      }
      Constraint* const constraint = MakeBooleanSumOdd(solver, variables);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractArrayIntElement(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ct->arguments[0].type == fz::Argument::INT_VAR_REF) {
    IntExpr* const index = data->GetOrCreateExpression(ct->arguments[0]);
    const std::vector<int64>& values = ct->arguments[1].values;
    const int64 imin = std::max(index->Min(), 1LL);
    const int64 imax = std::min<int64>(index->Max(), values.size());
    IntVar* const shifted_index = solver->MakeSum(index, -imin)->Var();
    const int64 size = imax - imin + 1;
    std::vector<int64> coefficients(size);
    for (int i = 0; i < size; ++i) {
      coefficients[i] = values[i + imin - 1];
    }
    if (ct->target_variable != nullptr) {
      DCHECK_EQ(ct->arguments[2].Var(), ct->target_variable);
      IntExpr* const target = solver->MakeElement(coefficients, shifted_index);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    } else {
      IntVar* const target =
          data->GetOrCreateExpression(ct->arguments[2])->Var();
      Constraint* const constraint =
          solver->MakeElementEquality(coefficients, shifted_index, target);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    CHECK_EQ(2, ct->arguments[0].variables.size());
    CHECK_EQ(5, ct->arguments.size());
    CHECK(ct->target_variable == nullptr);
    IntVar* const index1 = data->Extract(ct->arguments[0].variables[0])->Var();
    IntVar* const index2 = data->Extract(ct->arguments[0].variables[1])->Var();
    const int64 coef1 = ct->arguments[3].values[0];
    const int64 coef2 = ct->arguments[3].values[1];
    const int64 offset = ct->arguments[4].values[0];
    const std::vector<int64>& values = ct->arguments[1].values;
    std::unique_ptr<IntVarIterator> domain1(index1->MakeDomainIterator(false));
    std::unique_ptr<IntVarIterator> domain2(index2->MakeDomainIterator(false));
    IntTupleSet tuples(3);
    for (domain1->Init(); domain1->Ok(); domain1->Next()) {
      const int64 v1 = domain1->Value();
      for (domain2->Init(); domain2->Ok(); domain2->Next()) {
        const int64 v2 = domain2->Value();
        const int64 index = v1 * coef1 + v2 * coef2 + offset - 1;
        if (index >= 0 && index < values.size()) {
          tuples.Insert3(v1, v2, values[index]);
        }
      }
    }
    std::vector<IntVar*> variables;
    variables.push_back(index1);
    variables.push_back(index2);
    IntVar* const target = data->GetOrCreateExpression(ct->arguments[2])->Var();
    variables.push_back(target);
    Constraint* const constraint =
        solver->MakeAllowedAssignments(variables, tuples);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractArrayVarIntElement(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const index = data->GetOrCreateExpression(ct->arguments[0]);
  const int64 array_size = ct->arguments[1].variables.size();
  const int64 imin = std::max(index->Min(), 1LL);
  const int64 imax = std::min(index->Max(), array_size);
  IntVar* const shifted_index = solver->MakeSum(index, -imin)->Var();
  const std::vector<IntVar*> vars =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  const int64 size = imax - imin + 1;
  std::vector<IntVar*> var_array(size);
  for (int i = 0; i < size; ++i) {
    var_array[i] = vars[i + imin - 1];
  }

  if (ct->target_variable != nullptr) {
    DCHECK_EQ(ct->arguments[2].Var(), ct->target_variable);
    IntExpr* const target = solver->MakeElement(var_array, shifted_index);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    Constraint* constraint = nullptr;
    if (ct->arguments[2].HasOneValue()) {
      const int64 target = ct->arguments[2].Value();
      if (data->IsAllDifferent(ct->arguments[1].variables)) {
        constraint =
            solver->MakeIndexOfConstraint(var_array, shifted_index, target);
      } else {
        constraint =
            solver->MakeElementEquality(var_array, shifted_index, target);
      }
    } else {
      IntVar* const target =
          data->GetOrCreateExpression(ct->arguments[2])->Var();
      constraint =
          solver->MakeElementEquality(var_array, shifted_index, target);
    }
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractBoolAnd(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMin(left, right);
    FZVLOG << "  - creating " << ct->arguments[2].DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    if (FLAGS_fz_use_sat && AddBoolAndEqVar(data->Sat(), left, right, target)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeMin(left, right), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractBoolClause(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> positive_variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<IntVar*> negative_variables =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  std::vector<IntVar*> vars;
  for (IntVar* const var : positive_variables) {
    if (var->Bound() && var->Min() == 1) {  // True constraint
      AddConstraint(solver, ct, solver->MakeTrueConstraint());
      return;
    } else if (!var->Bound()) {
      vars.push_back(var);
    }
  }
  for (IntVar* const var : negative_variables) {
    if (var->Bound() && var->Max() == 0) {  // True constraint
      AddConstraint(solver, ct, solver->MakeTrueConstraint());
      return;
    } else if (!var->Bound()) {
      vars.push_back(solver->MakeDifference(1, var)->Var());
    }
  }
  if (FLAGS_fz_use_sat && AddBoolOrArrayEqualTrue(data->Sat(), vars)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else {
    Constraint* const constraint = solver->MakeSumGreaterOrEqual(vars, 1);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractBoolNot(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ct->target_variable != nullptr) {
    if (ct->target_variable == ct->arguments[1].Var()) {
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
      IntExpr* const target = solver->MakeDifference(1, left);
      FZVLOG << "  - creating " << ct->arguments[1].DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    } else {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      IntExpr* const target = solver->MakeDifference(1, right);
      FZVLOG << "  - creating " << ct->arguments[0].DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    }
  } else {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
    if (FLAGS_fz_use_sat && AddBoolNot(data->Sat(), left, right)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeDifference(1, left), right);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractBoolOr(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMax(left, right);
    FZVLOG << "  - creating " << ct->arguments[2].DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    if (FLAGS_fz_use_sat && AddBoolOrEqVar(data->Sat(), left, right, target)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeMax(left, right), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractBoolXor(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
  IntVar* const target = data->GetOrCreateExpression(ct->arguments[2])->Var();
  if (FLAGS_fz_use_sat && AddBoolIsNEqVar(data->Sat(), left, right, target)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else {
    Constraint* const constraint =
        solver->MakeIsEqualCstCt(solver->MakeSum(left, right), 1, target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCircuit(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const int size = tmp_vars.size();
  bool found_zero = false;
  bool found_size = false;
  for (IntVar* const var : tmp_vars) {
    if (var->Min() == 0) {
      found_zero = true;
    }
    if (var->Max() == size) {
      found_size = true;
    }
  }
  std::vector<IntVar*> variables;
  if (found_zero && !found_size) {  // Variables values are 0 based.
    variables = tmp_vars;
  } else {  // Variables values are 1 based.
    variables.resize(size);
    for (int i = 0; i < size; ++i) {
      // Create variables. Account for 1-based array indexing.
      variables[i] = solver->MakeSum(tmp_vars[i], -1)->Var();
    }
  }
  Constraint* const constraint = solver->MakeCircuit(variables);
  AddConstraint(solver, ct, constraint);
}

// Creates an [ct->arguments[0][i]->Var() == ct->arguments[1] for all i].
// It is optimized for different cases:
//   - ct->arguments[0] is constant and ct->arguments[1] has one value.
//   - ct->arguments[1] has one value
//   - generic case.
// This is used by all CreateCountXXX functions.
std::vector<IntVar*> BuildCount(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> tmp_sum;
  if (ct->arguments[0].variables.empty()) {
    if (ct->arguments[1].HasOneValue()) {
      const int64 value = ct->arguments[1].Value();
      for (const int64 v : ct->arguments[0].values) {
        if (v == value) {
          tmp_sum.push_back(solver->MakeIntConst(1));
        }
      }
    } else {
      IntVar* const count_var =
          data->GetOrCreateExpression(ct->arguments[1])->Var();
      tmp_sum.push_back(
          solver->MakeIsMemberVar(count_var, ct->arguments[0].values));
    }
  } else {
    if (ct->arguments[1].HasOneValue()) {
      const int64 value = ct->arguments[1].Value();
      for (fz::IntegerVariable* const fzvar : ct->arguments[0].variables) {
        IntVar* const var =
            solver->MakeIsEqualCstVar(data->Extract(fzvar), value);
        if (var->Max() == 1) {
          tmp_sum.push_back(var);
        }
      }
    } else {
      IntVar* const value =
          data->GetOrCreateExpression(ct->arguments[1])->Var();
      for (fz::IntegerVariable* const fzvar : ct->arguments[0].variables) {
        IntVar* const var = solver->MakeIsEqualVar(data->Extract(fzvar), value);
        if (var->Max() == 1) {
          tmp_sum.push_back(var);
        }
      }
    }
  }
  return tmp_sum;
}

void ExtractCountEq(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(data, ct);
  if (ct->arguments[2].HasOneValue()) {
    const int64 count = ct->arguments[2].Value();
    PostBooleanSumInRange(data->Sat(), solver, tmp_sum, count, count);
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[2])->Var();
    Constraint* const constraint = solver->MakeSumEquality(tmp_sum, count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountGeq(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(data, ct);
  if (ct->arguments[2].HasOneValue()) {
    const int64 count = ct->arguments[2].Value();
    PostBooleanSumInRange(data->Sat(), solver, tmp_sum, count, tmp_sum.size());
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[2])->Var();
    Constraint* const constraint =
        solver->MakeGreaterOrEqual(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountGt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(data, ct);
  if (ct->arguments[2].HasOneValue()) {
    const int64 count = ct->arguments[2].Value();
    PostBooleanSumInRange(data->Sat(), solver, tmp_sum, count + 1,
                          tmp_sum.size());
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[2])->Var();
    Constraint* const constraint =
        solver->MakeGreater(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountLeq(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ct->arguments[2].HasOneValue() && ct->arguments[1].HasOneValue()) {
    // At most in disguise.
    const int64 max_count = ct->arguments[2].Value();
    const int64 value = ct->arguments[1].Value();
    const std::vector<IntVar*> vars =
        data->GetOrCreateVariableArray(ct->arguments[0]);
    Constraint* const constraint = solver->MakeAtMost(vars, value, max_count);
    AddConstraint(solver, ct, constraint);
  }

  const std::vector<IntVar*> tmp_sum = BuildCount(data, ct);
  if (ct->arguments[2].HasOneValue()) {
    const int64 count = ct->arguments[2].Value();
    PostBooleanSumInRange(data->Sat(), solver, tmp_sum, 0, count);
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[2])->Var();
    Constraint* const constraint =
        solver->MakeLessOrEqual(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountLt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ct->arguments[2].HasOneValue() && ct->arguments[1].HasOneValue()) {
    // At most in disguise.
    const int64 max_count = ct->arguments[2].Value();
    const int64 value = ct->arguments[1].Value();
    const std::vector<IntVar*> vars =
        data->GetOrCreateVariableArray(ct->arguments[0]);
    Constraint* const constraint =
        solver->MakeAtMost(vars, value, max_count - 1);
    AddConstraint(solver, ct, constraint);
  }

  const std::vector<IntVar*> tmp_sum = BuildCount(data, ct);
  if (ct->arguments[2].HasOneValue()) {
    const int64 count = ct->arguments[2].Value();
    PostBooleanSumInRange(data->Sat(), solver, tmp_sum, 0, count - 1);
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[2])->Var();
    Constraint* const constraint =
        solver->MakeLess(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountNeq(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(data, ct);
  if (ct->arguments[2].HasOneValue()) {
    const int64 count = ct->arguments[2].Value();
    if (count == 0) {
      PostBooleanSumInRange(data->Sat(), solver, tmp_sum, 1, tmp_sum.size());
    } else if (count == tmp_sum.size()) {
      PostBooleanSumInRange(data->Sat(), solver, tmp_sum, 0,
                            tmp_sum.size() - 1);
    } else {
      Constraint* const constraint =
          solver->MakeNonEquality(solver->MakeSum(tmp_sum), count);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[2])->Var();
    Constraint* const constraint =
        solver->MakeNonEquality(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(data, ct);
  IntVar* const boolvar = data->GetOrCreateExpression(ct->arguments[3])->Var();
  if (ct->arguments[2].HasOneValue()) {
    const int64 count = ct->arguments[2].Value();
    PostIsBooleanSumInRange(data->Sat(), solver, tmp_sum, count, count,
                            boolvar);
  } else {
    IntVar* const count = data->GetOrCreateExpression(ct->arguments[2])->Var();
    Constraint* const constraint =
        solver->MakeIsEqualCt(solver->MakeSum(tmp_sum), count, boolvar);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractPerformedAndDemands(Solver* const solver,
                                const std::vector<IntVar*>& vars,
                                std::vector<IntVar*>* performed,
                                std::vector<int64>* demands) {
  performed->clear();
  demands->clear();
  for (IntVar* const var : vars) {
    if (var->Bound()) {
      performed->push_back(solver->MakeIntConst(1));
      demands->push_back(var->Min());
    } else if (var->Max() == 1) {
      performed->push_back(var);
      demands->push_back(1);
    } else {
      IntExpr* sub = nullptr;
      int64 coef = 1;
      CHECK(solver->IsProduct(var, &sub, &coef));
      performed->push_back(sub->Var());
      demands->push_back(coef);
    }
  }
}

// Recognize a demand of the form boolean_var * constant.
// In the context of cumulative, this can be interpreted as a task with fixed
// demand, and a performed variable 'boolean_var'.
bool IsHiddenPerformed(fz::SolverData* data,
                       const std::vector<fz::IntegerVariable*>& fz_vars) {
  for (fz::IntegerVariable* const fz_var : fz_vars) {
    IntVar* const var = data->Extract(fz_var)->Var();
    if (var->Size() > 2 || (var->Size() == 2 && var->Min() != 0)) {
      return false;
    }
    if (!var->Bound() && var->Max() != 1) {
      IntExpr* sub = nullptr;
      int64 coef = 1;
      if (!data->solver()->IsProduct(var, &sub, &coef)) {
        return false;
      }
      if (coef != var->Max()) {
        return false;
      }
      CHECK_EQ(1, sub->Max());
    }
  }
  return true;
}

void ExtractCumulative(fz::SolverData* data, fz::Constraint* ct) {
  // This constraint has many possible translation into the CP library.
  // First we parse the arguments.
  Solver* const solver = data->solver();
  // Parse start variables.
  const std::vector<IntVar*> start_variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);

  // Parse durations.
  std::vector<int64> fixed_durations;
  std::vector<IntVar*> variable_durations;
  if (ct->arguments[1].type == fz::Argument::INT_LIST) {
    fixed_durations = ct->arguments[1].values;
  } else {
    variable_durations = data->GetOrCreateVariableArray(ct->arguments[1]);
    if (AreAllBound(variable_durations)) {
      FillValues(variable_durations, &fixed_durations);
      variable_durations.clear();
    }
  }

  // Parse demands.
  std::vector<int64> fixed_demands;
  std::vector<IntVar*> variable_demands;
  if (ct->arguments[2].type == fz::Argument::INT_LIST) {
    fixed_demands = ct->arguments[2].values;
  } else {
    variable_demands = data->GetOrCreateVariableArray(ct->arguments[2]);
    if (AreAllBound(variable_demands)) {
      FillValues(variable_demands, &fixed_demands);
      variable_demands.clear();
    }
  }

  // Parse capacity.
  int64 fixed_capacity = 0;
  IntVar* variable_capacity = nullptr;
  if (ct->arguments[3].HasOneValue()) {
    fixed_capacity = ct->arguments[3].Value();
  } else {
    variable_capacity = data->GetOrCreateExpression(ct->arguments[3])->Var();
  }

  // Special case. We will not create the interval variables.
  if (!fixed_durations.empty() && !fixed_demands.empty() &&
      AreAllOnes(fixed_durations) && variable_capacity == nullptr &&
      AreAllGreaterOrEqual(fixed_demands, fixed_capacity / 2 + 1)) {
    // Hidden all different.
    Constraint* const constraint = solver->MakeAllDifferent(start_variables);
    AddConstraint(solver, ct, constraint);
    return;
  }

  // Special case. Durations are fixed, demands are boolean, capacity
  // is one.  We can transform the cumulative into a disjunctive with
  // optional interval variables.
  if (!fixed_durations.empty() && fixed_demands.empty() &&
      IsHiddenPerformed(data, ct->arguments[2].variables) &&
      variable_capacity == nullptr && fixed_capacity == 1) {
    std::vector<IntVar*> performed_variables;
    ExtractPerformedAndDemands(solver, variable_demands, &performed_variables,
                               &fixed_demands);
    std::vector<IntervalVar*> intervals;
    intervals.reserve(start_variables.size());
    for (int i = 0; i < start_variables.size(); ++i) {
      if (fixed_demands[i] == 1) {
        intervals.push_back(solver->MakeFixedDurationIntervalVar(
            start_variables[i], fixed_durations[i], performed_variables[i],
            start_variables[i]->name()));
      }
    }
    if (intervals.size() > 1) {
      Constraint* const constraint =
          solver->MakeDisjunctiveConstraint(intervals, "");
      AddConstraint(solver, ct, constraint);
    }
    return;
  }

  // Back to regular case. Let's create the interval variables.
  Constraint* constraint = nullptr;
  std::vector<IntervalVar*> intervals;
  if (!fixed_durations.empty()) {
    for (int i = 0; i < start_variables.size(); ++i) {
      IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
          start_variables[i], fixed_durations[i], start_variables[i]->name());
      intervals.push_back(interval);
    }
  } else {
    for (int i = 0; i < start_variables.size(); ++i) {
      IntVar* const start = start_variables[i];
      IntVar* const duration = variable_durations[i];
      IntervalVar* const interval =
          MakePerformedIntervalVar(solver, start, duration, start->name());
      intervals.push_back(interval);
    }
  }

  if (!fixed_demands.empty()) {
    // Demands are fixed.
    if (variable_capacity == nullptr) {
      constraint = AreAllGreaterOrEqual(fixed_demands, fixed_capacity / 2 + 1)
                       ? solver->MakeDisjunctiveConstraint(intervals, "")
                       : solver->MakeCumulative(intervals, fixed_demands,
                                                fixed_capacity, "");
    } else {
      // Capacity is variable.
      constraint = solver->MakeCumulative(intervals, fixed_demands,
                                          variable_capacity, "");
    }
  } else {
    // Demands are variables.
    if (variable_capacity == nullptr) {
      // Capacity is fixed.
      constraint = solver->MakeCumulative(intervals, variable_demands,
                                          fixed_capacity, "");
    } else {
      // Capacity is variable.
      // Constraint* const constraint2 = MakeVariableCumulative(
      //     solver, start_variables, variable_durations, variable_demands,
      //     variable_capacity);
      // AddConstraint(solver, ct, constraint2);

      constraint = solver->MakeCumulative(intervals, variable_demands,
                                          variable_capacity, "");
    }
  }
  AddConstraint(solver, ct, constraint);
}

void ExtractDiffn(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> x_variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<IntVar*> y_variables =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  if (ct->arguments[2].type == fz::Argument::INT_LIST &&
      ct->arguments[3].type == fz::Argument::INT_LIST) {
    const std::vector<int64>& x_sizes = ct->arguments[2].values;
    const std::vector<int64>& y_sizes = ct->arguments[3].values;
    Constraint* const constraint = solver->MakeNonOverlappingBoxesConstraint(
        x_variables, y_variables, x_sizes, y_sizes);
    AddConstraint(solver, ct, constraint);
  } else {
    const std::vector<IntVar*> x_sizes =
        data->GetOrCreateVariableArray(ct->arguments[2]);
    const std::vector<IntVar*> y_sizes =
        data->GetOrCreateVariableArray(ct->arguments[3]);
    Constraint* const constraint = solver->MakeNonOverlappingBoxesConstraint(
        x_variables, y_variables, x_sizes, y_sizes);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractDiffnK(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> flat_x =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<IntVar*> flat_dx =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  const int num_boxes = ct->arguments[2].Value();
  const int num_dims = ct->arguments[3].Value();
  std::vector<std::vector<IntVar*>> x(num_boxes);
  std::vector<std::vector<IntVar*>> dx(num_boxes);
  int count = 0;
  for (int b = 0; b < num_boxes; ++b) {
    x[b].resize(num_dims);
    dx[b].resize(num_dims);
    for (int d = 0; d < num_dims; ++d) {
      x[b][d] = flat_x[count];
      dx[b][d] = flat_dx[count];
      count++;
    }
  }
  Constraint* const constraint = MakeKDiffn(solver, x, dx, true);
  AddConstraint(solver, ct, constraint);
}

void ExtractDiffnNonStrict(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> x_variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<IntVar*> y_variables =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  if (ct->arguments[2].type == fz::Argument::INT_LIST &&
      ct->arguments[3].type == fz::Argument::INT_LIST) {
    const std::vector<int64>& x_sizes = ct->arguments[2].values;
    const std::vector<int64>& y_sizes = ct->arguments[3].values;
    Constraint* const constraint =
        solver->MakeNonOverlappingNonStrictBoxesConstraint(
            x_variables, y_variables, x_sizes, y_sizes);
    AddConstraint(solver, ct, constraint);
  } else {
    const std::vector<IntVar*> x_sizes =
        data->GetOrCreateVariableArray(ct->arguments[2]);
    const std::vector<IntVar*> y_sizes =
        data->GetOrCreateVariableArray(ct->arguments[3]);
    Constraint* const constraint =
        solver->MakeNonOverlappingNonStrictBoxesConstraint(
            x_variables, y_variables, x_sizes, y_sizes);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractDiffnNonStrictK(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> flat_x =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<IntVar*> flat_dx =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  const int num_boxes = ct->arguments[2].Value();
  const int num_dims = ct->arguments[3].Value();
  std::vector<std::vector<IntVar*>> x(num_boxes);
  std::vector<std::vector<IntVar*>> dx(num_boxes);
  int count = 0;
  for (int b = 0; b < num_boxes; ++b) {
    x[b].resize(num_dims);
    dx[b].resize(num_dims);
    for (int d = 0; d < num_dims; ++d) {
      x[b][d] = flat_x[count];
      dx[b][d] = flat_dx[count];
      count++;
    }
  }
  Constraint* const constraint = MakeKDiffn(solver, x, dx, false);
  AddConstraint(solver, ct, constraint);
}

void ExtractDisjunctive(fz::SolverData* data, fz::Constraint* ct) {
  // This constraint has many possible translation into the CP library.
  // First we parse the arguments.
  Solver* const solver = data->solver();
  // Parse start variables.
  const std::vector<IntVar*> start_variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);

  // Parse durations.
  std::vector<int64> fixed_durations;
  std::vector<IntVar*> variable_durations;
  if (ct->arguments[1].type == fz::Argument::INT_LIST) {
    fixed_durations = ct->arguments[1].values;
  } else {
    variable_durations = data->GetOrCreateVariableArray(ct->arguments[1]);
    if (AreAllBound(variable_durations)) {
      FillValues(variable_durations, &fixed_durations);
      variable_durations.clear();
    }
  }

  // Special case. We will not create the interval variables.
  if (!fixed_durations.empty() && AreAllOnes(fixed_durations)) {
    // Hidden all different.
    Constraint* const constraint = solver->MakeAllDifferent(start_variables);
    AddConstraint(solver, ct, constraint);
    return;
  }

  // Back to regular case. Let's create the interval variables.
  std::vector<IntervalVar*> intervals;
  if (!fixed_durations.empty()) {
    for (int i = 0; i < start_variables.size(); ++i) {
      IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
          start_variables[i], fixed_durations[i], start_variables[i]->name());
      intervals.push_back(interval);
    }
  } else {
    for (int i = 0; i < start_variables.size(); ++i) {
      IntVar* const start = start_variables[i];
      IntVar* const duration = variable_durations[i];
      IntervalVar* const interval =
          MakePerformedIntervalVar(solver, start, duration, start->name());
      intervals.push_back(interval);
    }
  }
  Constraint* const constraint =
      solver->MakeDisjunctiveConstraint(intervals, "");
  AddConstraint(solver, ct, constraint);
}

void ExtractDisjunctiveStrict(fz::SolverData* data, fz::Constraint* ct) {
  // This constraint has many possible translation into the CP library.
  // First we parse the arguments.
  Solver* const solver = data->solver();
  // Parse start variables.
  const std::vector<IntVar*> start_variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);

  // Parse durations.
  std::vector<int64> fixed_durations;
  std::vector<IntVar*> variable_durations;
  if (ct->arguments[1].type == fz::Argument::INT_LIST) {
    fixed_durations = ct->arguments[1].values;
  } else {
    variable_durations = data->GetOrCreateVariableArray(ct->arguments[1]);
    if (AreAllBound(variable_durations)) {
      FillValues(variable_durations, &fixed_durations);
      variable_durations.clear();
    }
  }

  // Special case. We will not create the interval variables.
  if (!fixed_durations.empty() && AreAllOnes(fixed_durations)) {
    // Hidden all different.
    Constraint* const constraint = solver->MakeAllDifferent(start_variables);
    AddConstraint(solver, ct, constraint);
    return;
  }

  // Back to regular case. Let's create the interval variables.
  std::vector<IntervalVar*> intervals;
  if (!fixed_durations.empty()) {
    for (int i = 0; i < start_variables.size(); ++i) {
      IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
          start_variables[i], fixed_durations[i], start_variables[i]->name());
      intervals.push_back(interval);
    }
  } else {
    for (int i = 0; i < start_variables.size(); ++i) {
      IntVar* const start = start_variables[i];
      IntVar* const duration = variable_durations[i];
      IntervalVar* const interval =
          MakePerformedIntervalVar(solver, start, duration, start->name());
      intervals.push_back(interval);
    }
  }
  Constraint* const constraint =
      solver->MakeStrictDisjunctiveConstraint(intervals, "");
  AddConstraint(solver, ct, constraint);
}

void ExtractFalseConstraint(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  Constraint* const constraint = solver->MakeFalseConstraint();
  AddConstraint(solver, ct, constraint);
}

void ExtractGlobalCardinality(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<int64> values = ct->arguments[1].values;
  std::vector<IntVar*> variables;
  const std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  for (IntVar* const var : tmp_vars) {
    for (int v = 0; v < values.size(); ++v) {
      if (var->Contains(values[v])) {
        variables.push_back(var);
        break;
      }
    }
  }
  const std::vector<IntVar*> cards =
      data->GetOrCreateVariableArray(ct->arguments[2]);
  Constraint* const constraint =
      solver->MakeDistribute(variables, values, cards);
  AddConstraint(solver, ct, constraint);
  Constraint* const constraint2 =
      solver->MakeSumLessOrEqual(cards, variables.size());
  AddConstraint(solver, ct, constraint2);
}

void ExtractGlobalCardinalityClosed(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<int64> values = ct->arguments[1].values;
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);

  const std::vector<IntVar*> cards =
      data->GetOrCreateVariableArray(ct->arguments[2]);
  Constraint* const constraint =
      solver->MakeDistribute(variables, values, cards);
  AddConstraint(solver, ct, constraint);
  for (IntVar* const var : variables) {
    Constraint* const constraint2 = solver->MakeMemberCt(var, values);
    AddConstraint(solver, ct, constraint2);
  }

  Constraint* const constraint3 =
      solver->MakeSumEquality(cards, variables.size());
  AddConstraint(solver, ct, constraint3);
}

void ExtractGlobalCardinalityLowUp(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<int64> values = ct->arguments[1].values;
  std::vector<IntVar*> variables;
  const std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  for (IntVar* const var : tmp_vars) {
    for (int v = 0; v < values.size(); ++v) {
      if (var->Contains(values[v])) {
        variables.push_back(var);
        break;
      }
    }
  }
  const std::vector<int64>& low = ct->arguments[2].values;
  const std::vector<int64>& up = ct->arguments[3].values;
  Constraint* const constraint =
      solver->MakeDistribute(variables, values, low, up);
  AddConstraint(solver, ct, constraint);
}

void ExtractGlobalCardinalityLowUpClosed(fz::SolverData* data,
                                         fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<int64> values = ct->arguments[1].values;
  const std::vector<int64>& low = ct->arguments[2].values;
  const std::vector<int64>& up = ct->arguments[3].values;
  Constraint* const constraint =
      solver->MakeDistribute(variables, values, low, up);
  AddConstraint(solver, ct, constraint);
  for (IntVar* const var : variables) {
    Constraint* const constraint2 = solver->MakeMemberCt(var, values);
    AddConstraint(solver, ct, constraint2);
  }
}

void ExtractGlobalCardinalityOld(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<IntVar*> cards =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  Constraint* const constraint = solver->MakeDistribute(variables, cards);
  AddConstraint(solver, ct, constraint);
}

void ExtractIntAbs(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeAbs(left);
    FZVLOG << "  - creating " << ct->arguments[1].DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else if (ct->arguments[1].HasOneValue()) {
    const int64 value = ct->arguments[1].Value();
    std::vector<int64> values(2);
    values[0] = -value;
    values[1] = value;
    Constraint* const constraint = solver->MakeMemberCt(left, values);
    AddConstraint(solver, ct, constraint);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[1]);
    Constraint* const constraint =
        solver->MakeAbsEquality(left->Var(), target->Var());
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntDiv(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  if (ct->target_variable != nullptr) {
    if (!ct->arguments[1].HasOneValue()) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      IntExpr* const target = solver->MakeDiv(left, right);
      FZVLOG << "  - creating " << ct->arguments[2].DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    } else {
      const int64 value = ct->arguments[1].Value();
      IntExpr* const target = solver->MakeDiv(left, value);
      FZVLOG << "  - creating " << ct->arguments[2].DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    }
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    if (!ct->arguments[1].HasOneValue()) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeDiv(left, right), target);
      AddConstraint(solver, ct, constraint);
    } else {
      Constraint* const constraint = solver->MakeEquality(
          solver->MakeDiv(left, ct->arguments[1].Value()), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractIntEq(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  if (ct->arguments[0].type == fz::Argument::INT_VAR_REF) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      if (FLAGS_fz_use_sat && AddBoolEq(data->Sat(), left, right)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeEquality(left, right));
      }
    } else {
      const int64 right = ct->arguments[1].Value();
      AddConstraint(s, ct, s->MakeEquality(left, right));
    }
  } else {
    const int64 left = ct->arguments[0].Value();
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeEquality(right, left));
    } else {
      const int64 right = ct->arguments[1].Value();
      if (left != right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntEqReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ct->target_variable != nullptr) {
    CHECK_EQ(ct->target_variable, ct->arguments[2].Var());
    if (ct->arguments[1].HasOneValue()) {
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
      const int64 value = ct->arguments[1].Value();
      IntVar* const boolvar = solver->MakeIsEqualCstVar(left, value);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, boolvar);
    } else if (ct->arguments[0].HasOneValue()) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      const int64 value = ct->arguments[0].Value();
      IntVar* const boolvar = solver->MakeIsEqualCstVar(right, value);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      IntVar* tmp_var = nullptr;
      bool tmp_neg = 0;
      bool success = false;
      if (FLAGS_fz_use_sat && solver->IsBooleanVar(left, &tmp_var, &tmp_neg) &&
          solver->IsBooleanVar(right, &tmp_var, &tmp_neg)) {
        // Try to post to sat.
        IntVar* const boolvar = solver->MakeBoolVar();
        if (AddIntEqReif(data->Sat(), left, right, boolvar)) {
          FZVLOG << "  - posted to sat" << FZENDL;
          FZVLOG << "  - creating " << ct->target_variable->DebugString()
                 << " := " << boolvar->DebugString() << FZENDL;
          data->SetExtracted(ct->target_variable, boolvar);
          success = true;
        }
      }
      if (!success) {
        IntVar* const boolvar = solver->MakeIsEqualVar(
            left, data->GetOrCreateExpression(ct->arguments[1]));
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      }
    }
  } else {
    IntVar* const boolvar =
        data->GetOrCreateExpression(ct->arguments[2])->Var();
    if (ct->arguments[1].HasOneValue()) {
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
      const int64 value = ct->arguments[1].Value();
      Constraint* const constraint =
          solver->MakeIsEqualCstCt(left, value, boolvar);
      AddConstraint(solver, ct, constraint);
    } else if (ct->arguments[0].HasOneValue()) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      const int64 value = ct->arguments[0].Value();
      Constraint* const constraint =
          solver->MakeIsEqualCstCt(right, value, boolvar);
      AddConstraint(solver, ct, constraint);
    } else {
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
      IntExpr* const right =
          data->GetOrCreateExpression(ct->arguments[1])->Var();
      if (FLAGS_fz_use_sat && AddIntEqReif(data->Sat(), left, right, boolvar)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeIsEqualCt(left, right, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractIntGe(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  if (ct->arguments[0].type == fz::Argument::INT_VAR_REF) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      if (FLAGS_fz_use_sat && AddBoolLe(data->Sat(), right, left)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeGreaterOrEqual(left, right));
      }
    } else {
      const int64 right = ct->arguments[1].Value();
      AddConstraint(s, ct, s->MakeGreaterOrEqual(left, right));
    }
  } else {
    const int64 left = ct->arguments[0].Value();
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeLessOrEqual(right, left));
    } else {
      const int64 right = ct->arguments[1].Value();
      if (left < right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

#define EXTRACT_INT_XX_REIF(Op, Rev)                                        \
  Solver* const solver = data->solver();                                    \
  if (ct->target_variable != nullptr) {                                     \
    IntVar* boolvar = nullptr;                                              \
    if (ct->arguments[0].HasOneValue()) {                                   \
      const int64 left = ct->arguments[0].Value();                          \
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]); \
      boolvar = solver->MakeIs##Rev##CstVar(right, left);                   \
    } else if (ct->arguments[1].HasOneValue()) {                            \
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);  \
      const int64 right = ct->arguments[1].Value();                         \
      boolvar = solver->MakeIs##Op##CstVar(left, right);                    \
    } else {                                                                \
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);  \
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]); \
      boolvar = solver->MakeIs##Op##Var(left, right);                       \
    }                                                                       \
    FZVLOG << "  - creating " << ct->target_variable->DebugString()         \
           << " := " << boolvar->DebugString() << FZENDL;                   \
    data->SetExtracted(ct->target_variable, boolvar);                       \
  } else {                                                                  \
    IntVar* boolvar = nullptr;                                              \
    Constraint* constraint = nullptr;                                       \
    if (ct->arguments[0].HasOneValue()) {                                   \
      const int64 left = ct->arguments[0].Value();                          \
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]); \
      if (right->IsVar()) {                                                 \
        boolvar = solver->MakeIs##Rev##CstVar(right, left);                 \
      } else {                                                              \
        boolvar = data->GetOrCreateExpression(ct->arguments[2])->Var();     \
        constraint = solver->MakeIs##Rev##CstCt(right, left, boolvar);      \
      }                                                                     \
    } else if (ct->arguments[1].HasOneValue()) {                            \
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);  \
      const int64 right = ct->arguments[1].Value();                         \
      if (left->IsVar()) {                                                  \
        boolvar = solver->MakeIs##Op##CstVar(left, right);                  \
      } else {                                                              \
        boolvar = data->GetOrCreateExpression(ct->arguments[2])->Var();     \
        constraint = solver->MakeIs##Op##CstCt(left, right, boolvar);       \
      }                                                                     \
    } else {                                                                \
      IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);  \
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]); \
      boolvar = data->GetOrCreateExpression(ct->arguments[2])->Var();       \
      constraint = solver->MakeIs##Op##Ct(left, right, boolvar);            \
    }                                                                       \
    if (constraint != nullptr) {                                            \
      AddConstraint(solver, ct, constraint);                                \
    } else {                                                                \
      IntVar* const previous =                                              \
          data->GetOrCreateExpression(ct->arguments[2])->Var();             \
      FZVLOG << "  - creating and linking " << boolvar->DebugString()       \
             << " to " << previous->DebugString() << FZENDL;                \
      if (FLAGS_fz_use_sat && AddBoolEq(data->Sat(), boolvar, previous)) {  \
        FZVLOG << "  - posted to sat" << FZENDL;                            \
      } else {                                                              \
        Constraint* const constraint =                                      \
            solver->MakeEquality(boolvar, previous);                        \
        AddConstraint(solver, ct, constraint);                              \
      }                                                                     \
    }                                                                       \
  }

void ExtractIntGeReif(fz::SolverData* data, fz::Constraint* ct) {
  EXTRACT_INT_XX_REIF(GreaterOrEqual, LessOrEqual)
}

void ExtractIntGt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  if (ct->arguments[0].type == fz::Argument::INT_VAR_REF) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeGreater(left, right));
    } else {
      const int64 right = ct->arguments[1].Value();
      AddConstraint(s, ct, s->MakeGreater(left, right));
    }
  } else {
    const int64 left = ct->arguments[0].Value();
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeLess(right, left));
    } else {
      const int64 right = ct->arguments[1].Value();
      if (left <= right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntGtReif(fz::SolverData* data, fz::Constraint* ct) {
  EXTRACT_INT_XX_REIF(Greater, Less);
}

void ExtractIntLe(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  if (ct->arguments[0].type == fz::Argument::INT_VAR_REF) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      if (FLAGS_fz_use_sat && AddBoolLe(data->Sat(), left, right)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeLessOrEqual(left, right));
      }
    } else {
      const int64 right = ct->arguments[1].Value();
      AddConstraint(s, ct, s->MakeLessOrEqual(left, right));
    }
  } else {
    const int64 left = ct->arguments[0].Value();
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeGreaterOrEqual(right, left));
    } else {
      const int64 right = ct->arguments[1].Value();
      if (left > right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntLeReif(fz::SolverData* data, fz::Constraint* ct) {
  EXTRACT_INT_XX_REIF(LessOrEqual, GreaterOrEqual)
}

void ExtractIntLt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  if (ct->arguments[0].type == fz::Argument::INT_VAR_REF) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeLess(left, right));
    } else {
      const int64 right = ct->arguments[1].Value();
      AddConstraint(s, ct, s->MakeLess(left, right));
    }
  } else {
    const int64 left = ct->arguments[0].Value();
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeGreater(right, left));
    } else {
      const int64 right = ct->arguments[1].Value();
      if (left >= right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntLtReif(fz::SolverData* data, fz::Constraint* ct) {
  EXTRACT_INT_XX_REIF(Less, Greater)
}

#undef EXTRACT_INT_XX_REIF

void ParseShortIntLin(fz::SolverData* data, fz::Constraint* ct, IntExpr** left,
                      IntExpr** right) {
  Solver* const solver = data->solver();
  const std::vector<fz::IntegerVariable*>& fzvars = ct->arguments[1].variables;
  const std::vector<int64>& coefficients = ct->arguments[0].values;
  int64 rhs = ct->arguments[2].Value();
  const int size = ct->arguments[0].values.size();
  *left = nullptr;
  *right = nullptr;

  if (fzvars.empty() && size != 0) {
    // We have a constant array.
    CHECK_EQ(ct->arguments[1].values.size(), size);
    int64 result = 0;
    for (int i = 0; i < size; ++i) {
      result += coefficients[i] * ct->arguments[1].values[i];
    }
    *left = solver->MakeIntConst(result);
    *right = solver->MakeIntConst(rhs);
    return;
  }

  switch (size) {
    case 0: {
      *left = solver->MakeIntConst(0);
      *right = solver->MakeIntConst(rhs);
      break;
    }
    case 1: {
      *left = solver->MakeProd(data->Extract(fzvars[0]), coefficients[0]);
      *right = solver->MakeIntConst(rhs);
      break;
    }
    case 2: {
      IntExpr* const e1 = data->Extract(fzvars[0]);
      IntExpr* const e2 = data->Extract(fzvars[1]);
      const int64 c1 = coefficients[0];
      const int64 c2 = coefficients[1];
      if (c1 > 0) {
        if (c2 > 0) {
          *left = solver->MakeProd(e1, c1);
          *right = solver->MakeDifference(rhs, solver->MakeProd(e2, c2));
        } else {
          *left = solver->MakeProd(e1, c1);
          *right = solver->MakeSum(solver->MakeProd(e2, -c2), rhs);
        }
      } else if (c2 > 0) {
        *left = solver->MakeProd(e2, c2);
        *right = solver->MakeSum(solver->MakeProd(e1, -c1), rhs);
      } else {
        *left = solver->MakeDifference(-rhs, solver->MakeProd(e2, -c2));
        *right = solver->MakeProd(e1, -c1);
      }
      break;
    }
    case 3: {
      IntExpr* const e1 = data->Extract(fzvars[0]);
      IntExpr* const e2 = data->Extract(fzvars[1]);
      IntExpr* const e3 = data->Extract(fzvars[2]);
      const int64 c1 = coefficients[0];
      const int64 c2 = coefficients[1];
      const int64 c3 = coefficients[2];
      if (c1 > 0 && c2 > 0 && c3 > 0) {
        *left =
            solver->MakeSum(solver->MakeProd(e1, c1), solver->MakeProd(e2, c2));
        *right = solver->MakeDifference(rhs, solver->MakeProd(e3, c3));

      } else if (c1 < 0 && c2 > 0 && c3 > 0) {
        *left =
            solver->MakeSum(solver->MakeProd(e2, c2), solver->MakeProd(e3, c3));
        *right = solver->MakeSum(solver->MakeProd(e1, -c1), rhs);
      } else if (c1 > 0 && c2 < 0 && c3 < 0) {
        *left = solver->MakeSum(solver->MakeProd(e1, c1), -rhs);
        *right = solver->MakeSum(solver->MakeProd(e2, -c2),
                                 solver->MakeProd(e3, -c3));
      } else if (c1 > 0 && c2 < 0 && c3 > 0) {
        *left =
            solver->MakeSum(solver->MakeProd(e1, c1), solver->MakeProd(e3, c3));
        *right = solver->MakeSum(solver->MakeProd(e2, -c2), rhs);
      } else if (c1 > 0 && c2 > 0 && c3 < 0) {
        *left =
            solver->MakeSum(solver->MakeProd(e1, c1), solver->MakeProd(e2, c2));
        *right = solver->MakeSum(solver->MakeProd(e3, -c3), rhs);
      } else if (c1 < 0 && c2 < 0 && c3 > 0) {
        *left = solver->MakeSum(solver->MakeProd(e3, c3), -rhs);
        *right = solver->MakeSum(solver->MakeProd(e1, -c1),
                                 solver->MakeProd(e2, -c2));
      } else if (c1 < 0 && c2 > 0 && c3 < 0) {
        *left = solver->MakeSum(solver->MakeProd(e2, c2), -rhs);
        *right = solver->MakeSum(solver->MakeProd(e1, -c1),
                                 solver->MakeProd(e3, -c3));
      } else {
        DCHECK_LE(c1, 0);
        DCHECK_LE(c2, 0);
        DCHECK_LE(c3, 0);
        *left = solver->MakeDifference(-rhs, solver->MakeProd(e3, -c3));
        *right = solver->MakeSum(solver->MakeProd(e1, -c1),
                                 solver->MakeProd(e2, -c2));
      }
      break;
    }
    default: { LOG(FATAL) << "Too many terms in " << ct->DebugString(); }
  }
}

void ParseLongIntLin(fz::SolverData* data, fz::Constraint* ct,
                     std::vector<IntVar*>* vars, std::vector<int64>* coeffs,
                     int64* rhs) {
  CHECK(vars != nullptr);
  CHECK(coeffs != nullptr);
  CHECK(rhs != nullptr);
  const std::vector<fz::IntegerVariable*>& fzvars = ct->arguments[1].variables;
  const std::vector<int64>& coefficients = ct->arguments[0].values;
  *rhs = ct->arguments[2].values[0];
  const int size = fzvars.size();

  for (int i = 0; i < size; ++i) {
    const int64 coef = coefficients[i];
    IntVar* const var = data->Extract(fzvars[i])->Var();
    if (coef != 0 && (var->Min() != 0 || var->Max() != 0)) {
      if (var->Bound()) {
        (*rhs) -= var->Min() * coef;
      } else {
        coeffs->push_back(coef);
        vars->push_back(var);
      }
    }
  }
}

bool AreAllExtractedAsVariables(
    fz::SolverData* data, const std::vector<fz::IntegerVariable*>& fz_vars) {
  for (fz::IntegerVariable* const fz_var : fz_vars) {
    IntExpr* const expr = data->Extract(fz_var);
    if (!expr->IsVar()) {
      return false;
    }
  }
  return true;
}

bool AreAllVariablesBoolean(fz::SolverData* data, fz::Constraint* ct) {
  for (fz::IntegerVariable* const fz_var : ct->arguments[1].variables) {
    IntVar* var = data->Extract(fz_var)->Var();
    if (var->Min() < 0 || var->Max() > 1) {
      return false;
    }
  }
  return true;
}

bool ExtractLinAsShort(fz::SolverData* data, fz::Constraint* ct) {
  const int size = ct->arguments[0].values.size();
  if (ct->arguments[1].variables.empty()) {
    // Constant linear scalprods will be treated correctly by ParseShortLin.
    return true;
  }
  switch (size) {
    case 0:
      return true;
    case 1:
      return true;
    case 2:
    case 3: {
      if (AreAllOnes(ct->arguments[0].values) &&
          AreAllExtractedAsVariables(data, ct->arguments[1].variables) &&
          AreAllVariablesBoolean(data, ct)) {
        return false;
      } else {
        return true;
      }
    }
    default: { return false; }
  }
}

void ExtractIntLinEq(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<fz::IntegerVariable*>& fzvars = ct->arguments[1].variables;
  const std::vector<int64>& coefficients = ct->arguments[0].values;
  int64 rhs = ct->arguments[2].Value();
  const int size = ct->arguments[0].values.size();
  if (ct->target_variable != nullptr) {
    if (size == 2) {
      IntExpr* other = nullptr;
      int64 other_coef = 0;
      if (ct->target_variable == fzvars[0] && coefficients[0] == -1) {
        other = data->Extract(fzvars[1]);
        other_coef = coefficients[1];
      } else if (ct->target_variable == fzvars[1] && coefficients[1] == -1) {
        other = data->Extract(fzvars[0]);
        other_coef = coefficients[0];
      } else {
        LOG(FATAL) << "Invalid constraint " << ct->DebugString();
      }

      IntExpr* const target =
          solver->MakeSum(solver->MakeProd(other, other_coef), -rhs);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    } else {
      std::vector<int64> new_coefficients;
      std::vector<IntVar*> variables;
      int64 constant = 0;
      for (int i = 0; i < size; ++i) {
        if (fzvars[i] == ct->target_variable) {
          CHECK_EQ(-1, coefficients[i]);
        } else if (fzvars[i]->domain.HasOneValue()) {
          constant += coefficients[i] * fzvars[i]->domain.Min();
        } else {
          const int64 coef = coefficients[i];
          IntVar* const var = data->Extract(fzvars[i])->Var();
          if (coef != 0 && (var->Min() != 0 || var->Max() != 0)) {
            new_coefficients.push_back(coef);
            variables.push_back(var);
          }
        }
      }
      IntExpr* const target = solver->MakeSum(
          solver->MakeScalProd(variables, new_coefficients), constant - rhs);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    }
  } else {
    Constraint* constraint = nullptr;
    if (ExtractLinAsShort(data, ct)) {
      IntExpr* left = nullptr;
      IntExpr* right = nullptr;
      ParseShortIntLin(data, ct, &left, &right);
      constraint = solver->MakeEquality(left, right);
    } else {
      std::vector<IntVar*> vars;
      std::vector<int64> coeffs;
      ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostBooleanSumInRange(data->Sat(), solver, vars, rhs, rhs);
        return;
      } else {
        constraint = solver->MakeScalProdEquality(vars, coeffs, rhs);
      }
    }
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntLinEqReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ExtractLinAsShort(data, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(data, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsEqualVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      Constraint* const constraint =
          solver->MakeIsEqualCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        IntVar* const boolvar = solver->MakeBoolVar();
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        PostIsBooleanSumInRange(data->Sat(), solver, vars, rhs, rhs, boolvar);
        data->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar =
            solver->MakeIsEqualCstVar(solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumInRange(data->Sat(), solver, vars, rhs, rhs, boolvar);
      } else {
        Constraint* const constraint = solver->MakeIsEqualCstCt(
            solver->MakeScalProd(vars, coeffs), rhs, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractIntLinGe(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const int size = ct->arguments[0].values.size();
  if (ExtractLinAsShort(data, ct)) {
    // Checks if it is not a hidden or.
    if (ct->arguments[2].Value() == 1 && AreAllOnes(ct->arguments[0].values)) {
      // Good candidate.
      bool ok = true;
      for (fz::IntegerVariable* const var : ct->arguments[1].variables) {
        IntExpr* const expr = data->Extract(var);
        if (expr->Min() < 0 || expr->Max() > 1 || !expr->IsVar()) {
          ok = false;
          break;
        }
      }
      if (ok) {
        std::vector<IntVar*> vars;
        std::vector<int64> coeffs;
        int64 rhs = 0;
        ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
        PostBooleanSumInRange(data->Sat(), solver, vars, rhs, size);
        return;
      }
    }
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(data, ct, &left, &right);
    AddConstraint(solver, ct, solver->MakeGreaterOrEqual(left, right));
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
    if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
      PostBooleanSumInRange(data->Sat(), solver, vars, rhs, size);
    } else {
      AddConstraint(solver, ct,
                    solver->MakeScalProdGreaterOrEqual(vars, coeffs, rhs));
    }
  }
}

void ExtractIntLinGeReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const int size = ct->arguments[0].values.size();
  if (ExtractLinAsShort(data, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(data, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsGreaterOrEqualVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      Constraint* const constraint =
          solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) &&
          (AreAllOnes(coeffs) || (rhs == 1 && AreAllPositive(coeffs)))) {
        IntVar* const boolvar = solver->MakeBoolVar();
        PostIsBooleanSumInRange(data->Sat(), solver, vars, rhs, size, boolvar);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar = solver->MakeIsGreaterOrEqualCstVar(
            solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumInRange(data->Sat(), solver, vars, rhs, size, boolvar);
      } else {
        Constraint* const constraint = solver->MakeIsGreaterOrEqualCstCt(
            solver->MakeScalProd(vars, coeffs), rhs, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

bool PostHiddenClause(SatPropagator* const sat,
                      const std::vector<int64>& coeffs,
                      const std::vector<IntVar*>& vars) {
  std::vector<IntVar*> others;
  others.reserve(vars.size() - 1);
  if (coeffs[0] != 1) {
    return false;
  }
  for (int i = 1; i < coeffs.size(); ++i) {
    if (coeffs[i] != -1) {
      return false;
    }
    others.push_back(vars[i]);
  }
  return AddSumBoolArrayGreaterEqVar(sat, others, vars[0]);
}

bool PostHiddenLeMax(SatPropagator* const sat, const std::vector<int64>& coeffs,
                     const std::vector<IntVar*>& vars) {
  std::vector<IntVar*> others;
  others.reserve(vars.size() - 1);
  if (coeffs[0] > 1 - vars.size()) {
    return false;
  }
  for (int i = 1; i < coeffs.size(); ++i) {
    if (coeffs[i] != 1) {
      return false;
    }
    others.push_back(vars[i]);
  }
  return AddMaxBoolArrayLessEqVar(sat, others, vars[0]);
}

void ExtractIntLinLe(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ExtractLinAsShort(data, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(data, ct, &left, &right);
    AddConstraint(solver, ct, solver->MakeLessOrEqual(left, right));
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
    if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
      PostBooleanSumInRange(data->Sat(), solver, vars, 0, rhs);
    } else if (FLAGS_fz_use_sat && AreAllBooleans(vars) && rhs == 0 &&
               PostHiddenClause(data->Sat(), coeffs, vars)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else if (FLAGS_fz_use_sat && AreAllBooleans(vars) && rhs == 0 &&
               PostHiddenLeMax(data->Sat(), coeffs, vars)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      AddConstraint(solver, ct,
                    solver->MakeScalProdLessOrEqual(vars, coeffs, rhs));
    }
  }
}

void ExtractIntLinLeReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ExtractLinAsShort(data, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(data, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsLessOrEqualVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      Constraint* const constraint =
          solver->MakeIsLessOrEqualCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) &&
          (AreAllOnes(coeffs) || (rhs == 0 && AreAllPositive(coeffs)))) {
        IntVar* const boolvar = solver->MakeBoolVar();
        PostIsBooleanSumInRange(data->Sat(), solver, vars, 0, rhs, boolvar);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar = solver->MakeIsLessOrEqualCstVar(
            solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumInRange(data->Sat(), solver, vars, 0, rhs, boolvar);
      } else if (rhs == 0 && AreAllPositive(coeffs) && AreAllBooleans(vars)) {
        // Special case. this is or(vars) = not(boolvar).
        PostIsBooleanSumInRange(data->Sat(), solver, vars, 0, 0, boolvar);
      } else if (rhs < 0 && AreAllPositive(coeffs) &&
                 IsArrayInRange(vars, 0LL, kint64max)) {
        // Trivial failure.
        boolvar->SetValue(0);
        FZVLOG << "  - set target to 0" << FZENDL;
      } else {
        Constraint* const constraint = solver->MakeIsLessOrEqualCstCt(
            solver->MakeScalProd(vars, coeffs), rhs, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractIntLinNe(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ExtractLinAsShort(data, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(data, ct, &left, &right);
    AddConstraint(solver, ct, solver->MakeNonEquality(left, right));
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
    AddConstraint(solver, ct, solver->MakeNonEquality(
                                  solver->MakeScalProd(vars, coeffs), rhs));
  }
}

void ExtractIntLinNeReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ExtractLinAsShort(data, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(data, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsDifferentVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      Constraint* const constraint =
          solver->MakeIsDifferentCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(data, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        IntVar* const boolvar = solver->MakeBoolVar();
        PostIsBooleanSumDifferent(data->Sat(), solver, vars, rhs, boolvar);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar = solver->MakeIsDifferentCstVar(
            solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar =
          data->GetOrCreateExpression(ct->arguments[3])->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumDifferent(data->Sat(), solver, vars, rhs, boolvar);
      } else {
        Constraint* const constraint = solver->MakeIsDifferentCstCt(
            solver->MakeScalProd(vars, coeffs), rhs, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractIntMax(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMax(left, right);
    FZVLOG << "  - creating " << ct->arguments[2].DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeMax(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntMin(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMin(left, right);
    FZVLOG << "  - creating " << ct->arguments[2].DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeMin(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntMinus(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeDifference(left, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeDifference(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntMod(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  if (ct->target_variable != nullptr) {
    if (!ct->arguments[1].HasOneValue()) {
      IntExpr* const mod = data->GetOrCreateExpression(ct->arguments[1]);
      IntExpr* const target = solver->MakeModulo(left, mod);
      FZVLOG << "  - creating " << ct->arguments[2].DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    } else {
      const int64 mod = ct->arguments[1].Value();
      IntExpr* const target = solver->MakeModulo(left, mod);
      FZVLOG << "  - creating " << ct->arguments[2].DebugString()
             << " := " << target->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, target);
    }
  } else if (ct->arguments[2].HasOneValue()) {
    const int64 target = ct->arguments[2].Value();
    if (!ct->arguments[1].HasOneValue()) {
      IntExpr* const mod = data->GetOrCreateExpression(ct->arguments[1]);
      Constraint* const constraint =
          MakeFixedModulo(solver, left->Var(), mod->Var(), target);
      AddConstraint(solver, ct, constraint);
    } else {
      const int64 mod = ct->arguments[1].Value();
      Constraint* constraint = nullptr;
      if (mod == 2) {
        switch (target) {
          case 0: {
            constraint = MakeVariableEven(solver, left->Var());
            break;
          }
          case 1: {
            constraint = MakeVariableOdd(solver, left->Var());
            break;
          }
          default: {
            constraint = solver->MakeFalseConstraint();
            break;
          }
        }
      } else {
        constraint =
            solver->MakeEquality(solver->MakeModulo(left, mod), target);
      }
      AddConstraint(solver, ct, constraint);
    }
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    if (!ct->arguments[1].HasOneValue()) {
      IntExpr* const mod = data->GetOrCreateExpression(ct->arguments[1]);
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeModulo(left, mod), target);
      AddConstraint(solver, ct, constraint);
    } else {
      const int64 mod = ct->arguments[1].Value();
      Constraint* constraint = nullptr;
      constraint = solver->MakeEquality(solver->MakeModulo(left, mod), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractIntNe(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  if (ct->arguments[0].type == fz::Argument::INT_VAR_REF) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      if (FLAGS_fz_use_sat && AddBoolNot(data->Sat(), left, right)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeNonEquality(left, right));
      }
    } else {
      const int64 right = ct->arguments[1].Value();
      AddConstraint(s, ct, s->MakeNonEquality(left, right));
    }
  } else {
    const int64 left = ct->arguments[0].Value();
    if (ct->arguments[1].type == fz::Argument::INT_VAR_REF) {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      AddConstraint(s, ct, s->MakeNonEquality(right, left));
    } else {
      const int64 right = ct->arguments[1].Value();
      if (left == right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntNeReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  if (ct->target_variable != nullptr) {
    CHECK_EQ(ct->target_variable, ct->arguments[2].Var());
    if (ct->arguments[1].HasOneValue()) {
      IntVar* const boolvar =
          solver->MakeIsDifferentCstVar(left, ct->arguments[1].Value());
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      data->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
      IntVar* tmp_var = nullptr;
      bool tmp_neg = 0;
      bool success = false;
      if (FLAGS_fz_use_sat && solver->IsBooleanVar(left, &tmp_var, &tmp_neg) &&
          solver->IsBooleanVar(right, &tmp_var, &tmp_neg)) {
        // Try to post to sat.
        IntVar* const boolvar = solver->MakeBoolVar();
        if (AddIntNeReif(data->Sat(), left, right, boolvar)) {
          FZVLOG << "  - posted to sat" << FZENDL;
          FZVLOG << "  - creating " << ct->target_variable->DebugString()
                 << " := " << boolvar->DebugString() << FZENDL;
          data->SetExtracted(ct->target_variable, boolvar);
          success = true;
        }
      }
      if (!success) {
        IntVar* const boolvar = solver->MakeIsDifferentVar(
            left, data->GetOrCreateExpression(ct->arguments[1]));
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        data->SetExtracted(ct->target_variable, boolvar);
      }
    }
  } else {
    IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1])->Var();
    IntVar* const boolvar =
        data->GetOrCreateExpression(ct->arguments[2])->Var();
    if (FLAGS_fz_use_sat && AddIntEqReif(data->Sat(), left, right, boolvar)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeIsDifferentCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractIntNegate(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeOpposite(left);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeOpposite(left), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntPlus(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (!ct->arguments[0].variables.empty() &&
      ct->target_variable == ct->arguments[0].variables[0]) {
    IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    IntExpr* const left = solver->MakeDifference(target, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << left->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, left);
  } else if (!ct->arguments[1].variables.empty() &&
             ct->target_variable == ct->arguments[1].variables[0]) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    IntExpr* const right = solver->MakeDifference(target, left);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << right->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, right);
  } else if (!ct->arguments[2].variables.empty() &&
             ct->target_variable == ct->arguments[2].variables[0]) {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
    IntExpr* const target = solver->MakeSum(left, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
    IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeSum(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntTimes(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const left = data->GetOrCreateExpression(ct->arguments[0]);
  IntExpr* const right = data->GetOrCreateExpression(ct->arguments[1]);
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeProd(left, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = data->GetOrCreateExpression(ct->arguments[2]);
    if (FLAGS_fz_use_sat && AddBoolAndEqVar(data->Sat(), left, right, target)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeProd(left, right), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractInverse(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> left;
  // Account for 1 based arrays.
  left.push_back(solver->MakeIntConst(0));
  std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  left.insert(left.end(), tmp_vars.begin(), tmp_vars.end());

  std::vector<IntVar*> right;
  // Account for 1 based arrays.
  right.push_back(solver->MakeIntConst(0));
  tmp_vars = data->GetOrCreateVariableArray(ct->arguments[1]);
  right.insert(right.end(), tmp_vars.begin(), tmp_vars.end());

  Constraint* const constraint =
      solver->MakeInversePermutationConstraint(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractLexLessInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> left = data->GetOrCreateVariableArray(ct->arguments[0]);
  std::vector<IntVar*> right = data->GetOrCreateVariableArray(ct->arguments[1]);
  Constraint* const constraint = solver->MakeLexicalLess(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractLexLesseqInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  std::vector<IntVar*> left = data->GetOrCreateVariableArray(ct->arguments[0]);
  std::vector<IntVar*> right = data->GetOrCreateVariableArray(ct->arguments[1]);
  Constraint* const constraint = solver->MakeLexicalLessOrEqual(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractMaximumArgInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  IntVar* const index = data->GetOrCreateExpression(ct->arguments[1])->Var();
  Constraint* const constraint =
      solver->MakeIndexOfFirstMaxValueConstraint(index, variables);
  AddConstraint(solver, ct, constraint);
}

void ExtractMaximumInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntVar* const target = data->GetOrCreateExpression(ct->arguments[0])->Var();
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  Constraint* const constraint = solver->MakeMaxEquality(variables, target);
  AddConstraint(solver, ct, constraint);
}

void ExtractMinimumArgInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  IntVar* const index = data->GetOrCreateExpression(ct->arguments[1])->Var();
  Constraint* const constraint =
      solver->MakeIndexOfFirstMinValueConstraint(index, variables);
  AddConstraint(solver, ct, constraint);
}

void ExtractMinimumInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  if (ct->target_variable != nullptr && ct->arguments[1].variables.size() < 3) {
    IntExpr* target = nullptr;
    switch (ct->arguments[1].variables.size()) {
      case 0: {
        target = solver->MakeIntConst(0);
        break;
      }
      case 1: {
        target = data->Extract(ct->arguments[1].variables[0]);
        break;
      }
      case 2: {
        IntExpr* const e0 = data->Extract(ct->arguments[1].variables[0]);
        IntExpr* const e1 = data->Extract(ct->arguments[1].variables[1]);
        target = solver->MakeMin(e0, e1);
        break;
      }
      default: {
        target =
            solver->MakeMin(data->GetOrCreateVariableArray(ct->arguments[1]));
        break;
      }
    }
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    data->SetExtracted(ct->target_variable, target);
  } else {
    IntVar* const target = data->GetOrCreateExpression(ct->arguments[0])->Var();
    const std::vector<IntVar*> variables =
        data->GetOrCreateVariableArray(ct->arguments[1]);
    Constraint* const constraint = solver->MakeMinEquality(variables, target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractNvalue(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();

  const std::vector<IntVar*> vars =
      data->GetOrCreateVariableArray(ct->arguments[1]);

  int64 lb = kint64max;
  int64 ub = kint64min;
  for (IntVar* const var : vars) {
    lb = std::min(lb, var->Min());
    ub = std::max(ub, var->Max());
  }

  int csize = ub - lb + 1;
  int64 always_true_cards = 0;
  std::vector<IntVar*> cards;
  for (int b = 0; b < csize; ++b) {
    const int value = lb + b;
    std::vector<IntVar*> contributors;
    bool always_true = false;
    for (IntVar* const var : vars) {
      if (var->Contains(value)) {
        if (var->Bound()) {
          always_true = true;
          break;
        } else {
          contributors.push_back(var->IsEqual(value));
        }
      }
    }
    if (always_true) {
      always_true_cards++;
    } else if (contributors.size() == 1) {
      cards.push_back(contributors.back());
    } else if (contributors.size() > 1) {
      IntVar* const contribution = solver->MakeBoolVar();
      if (FLAGS_fz_use_sat &&
          AddBoolOrArrayEqVar(data->Sat(), contributors, contribution)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeMaxEquality(contributors, contribution);
        AddConstraint(solver, ct, constraint);
      }
      cards.push_back(contribution);
    }
  }
  if (ct->arguments[0].HasOneValue()) {
    const int64 card = ct->arguments[0].Value() - always_true_cards;
    PostBooleanSumInRange(data->Sat(), solver, cards, card, card);
  } else {
    IntVar* const card = data->GetOrCreateExpression(ct->arguments[0])->Var();
    Constraint* const constraint = solver->MakeSumEquality(
        cards, solver->MakeSum(card, -always_true_cards)->Var());
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractRegular(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();

  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const int64 num_states = ct->arguments[1].Value();
  const int64 num_values = ct->arguments[2].Value();

  const std::vector<int64>& array_transitions = ct->arguments[3].values;
  IntTupleSet tuples(3);
  int count = 0;
  for (int64 q = 1; q <= num_states; ++q) {
    for (int64 s = 1; s <= num_values; ++s) {
      const int64 next = array_transitions[count++];
      if (next != 0) {
        tuples.Insert3(q, s, next);
      }
    }
  }

  const int64 initial_state = ct->arguments[4].Value();

  std::vector<int64> final_states;
  switch (ct->arguments[5].type) {
    case fz::Argument::INT_VALUE: {
      final_states.push_back(ct->arguments[5].values[0]);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      for (int v = ct->arguments[5].values[0]; v <= ct->arguments[5].values[1];
           ++v) {
        final_states.push_back(v);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      final_states = ct->arguments[5].values;
      break;
    }
    default: { LOG(FATAL) << "Wrong constraint " << ct->DebugString(); }
  }
  Constraint* const constraint = solver->MakeTransitionConstraint(
      variables, tuples, initial_state, final_states);
  AddConstraint(solver, ct, constraint);
}

void ExtractRegularNfa(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();

  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const int64 num_states = ct->arguments[1].Value();
  const int64 num_values = ct->arguments[2].Value();

  const std::vector<fz::Domain>& array_transitions = ct->arguments[3].domains;
  IntTupleSet tuples(3);
  int count = 0;
  for (int64 q = 1; q <= num_states; ++q) {
    for (int64 s = 1; s <= num_values; ++s) {
      const fz::Domain& next = array_transitions[count++];
      if (next.is_interval) {
        for (int64 v = next.values[0]; v <= next.values[1]; ++v) {
          if (v != 0) {
            tuples.Insert3(q, s, v);
          }
        }
      } else {
        for (int64 v : next.values) {
          if (v != 0) {
            tuples.Insert3(q, s, v);
          }
        }
      }
    }
  }

  const int64 initial_state = ct->arguments[4].Value();

  std::vector<int64> final_states;
  switch (ct->arguments[5].type) {
    case fz::Argument::INT_VALUE: {
      final_states.push_back(ct->arguments[5].values[0]);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      for (int v = ct->arguments[5].values[0]; v <= ct->arguments[5].values[1];
           ++v) {
        final_states.push_back(v);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      final_states = ct->arguments[5].values;
      break;
    }
    default: { LOG(FATAL) << "Wrong constraint " << ct->DebugString(); }
  }
  Constraint* const constraint = solver->MakeTransitionConstraint(
      variables, tuples, initial_state, final_states);
  AddConstraint(solver, ct, constraint);
}

void ExtractSetIn(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const expr = data->GetOrCreateExpression(ct->arguments[0]);
  const fz::Argument& arg = ct->arguments[1];
  switch (arg.type) {
    case fz::Argument::INT_VALUE: {
      Constraint* const constraint = solver->MakeEquality(expr, arg.values[0]);
      AddConstraint(solver, ct, constraint);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      if (expr->Min() < arg.values[0] || expr->Max() > arg.values[1]) {
        Constraint* const constraint =
            solver->MakeBetweenCt(expr, arg.values[0], arg.values[1]);
        AddConstraint(solver, ct, constraint);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      Constraint* const constraint = solver->MakeMemberCt(expr, arg.values);
      AddConstraint(solver, ct, constraint);
      break;
    }
    default: { LOG(FATAL) << "Invalid constraint " << ct->DebugString(); }
  }
}

void ExtractSetNotIn(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const expr = data->GetOrCreateExpression(ct->arguments[0]);
  const fz::Argument& arg = ct->arguments[1];
  switch (arg.type) {
    case fz::Argument::INT_VALUE: {
      Constraint* const constraint =
          solver->MakeNonEquality(expr, arg.values[0]);
      AddConstraint(solver, ct, constraint);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      if (expr->Min() < arg.values[0] || expr->Max() > arg.values[1]) {
        Constraint* const constraint =
            solver->MakeNotBetweenCt(expr, arg.values[0], arg.values[1]);
        AddConstraint(solver, ct, constraint);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      Constraint* const constraint = solver->MakeNotMemberCt(expr, arg.values);
      AddConstraint(solver, ct, constraint);
      break;
    }
    default: { LOG(FATAL) << "Invalid constraint " << ct->DebugString(); }
  }
}

void ExtractSetInReif(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  IntExpr* const expr = data->GetOrCreateExpression(ct->arguments[0]);
  IntVar* const target = data->GetOrCreateExpression(ct->arguments[2])->Var();
  const fz::Argument& arg = ct->arguments[1];
  switch (arg.type) {
    case fz::Argument::INT_VALUE: {
      Constraint* const constraint =
          solver->MakeIsEqualCstCt(expr, arg.values[0], target);
      AddConstraint(solver, ct, constraint);
      break;
    }
    case fz::Argument::INT_INTERVAL: {
      if (expr->Min() < arg.values[0] || expr->Max() > arg.values[1]) {
        Constraint* const constraint =
            solver->MakeIsBetweenCt(expr, arg.values[0], arg.values[1], target);
        AddConstraint(solver, ct, constraint);
      } else {
        Constraint* const constraint = solver->MakeEquality(target, 1);
        AddConstraint(solver, ct, constraint);
      }
      break;
    }
    case fz::Argument::INT_LIST: {
      Constraint* const constraint =
          solver->MakeIsMemberCt(expr, arg.values, target);
      AddConstraint(solver, ct, constraint);
      break;
    }
    default: { LOG(FATAL) << "Invalid constraint " << ct->DebugString(); }
  }
}

void ExtractSlidingSum(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const int64 low = ct->arguments[0].Value();
  const int64 up = ct->arguments[1].Value();
  const int64 seq = ct->arguments[2].Value();
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[3]);
  for (int i = 0; i < variables.size() - seq; ++i) {
    std::vector<IntVar*> tmp(seq);
    for (int k = 0; k < seq; ++k) {
      tmp[k] = variables[i + k];
    }
    IntVar* const sum_var = solver->MakeSum(tmp)->Var();
    sum_var->SetRange(low, up);
  }
}

void ExtractSort(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> left =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const std::vector<IntVar*> right =
      data->GetOrCreateVariableArray(ct->arguments[1]);
  Constraint* const constraint = solver->MakeSortingConstraint(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractSubCircuit(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> tmp_vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const int size = tmp_vars.size();
  bool found_zero = false;
  bool found_size = false;
  for (IntVar* const var : tmp_vars) {
    if (var->Min() == 0) {
      found_zero = true;
    }
    if (var->Max() == size) {
      found_size = true;
    }
  }
  std::vector<IntVar*> variables;
  if (found_zero && !found_size) {  // Variables values are 0 based.
    variables = tmp_vars;
  } else {  // Variables values are 1 based.
    variables.resize(size);
    for (int i = 0; i < size; ++i) {
      // Create variables. Account for 1-based array indexing.
      variables[i] = solver->MakeSum(tmp_vars[i], -1)->Var();
    }
  }
  Constraint* const constraint = solver->MakeSubCircuit(variables);
  AddConstraint(solver, ct, constraint);
}

void ExtractTableInt(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const solver = data->solver();
  const std::vector<IntVar*> variables =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  const int size = variables.size();
  IntTupleSet tuples(size);
  const std::vector<int64>& t = ct->arguments[1].values;
  const int t_size = t.size();
  DCHECK_EQ(0, t_size % size);
  const int num_tuples = t_size / size;
  std::vector<int64> one_tuple(size);
  for (int tuple_index = 0; tuple_index < num_tuples; ++tuple_index) {
    for (int var_index = 0; var_index < size; ++var_index) {
      one_tuple[var_index] = t[tuple_index * size + var_index];
    }
    tuples.Insert(one_tuple);
  }
  Constraint* const constraint =
      solver->MakeAllowedAssignments(variables, tuples);
  AddConstraint(solver, ct, constraint);
}

void ExtractSymmetricAllDifferent(fz::SolverData* data, fz::Constraint* ct) {
  Solver* const s = data->solver();
  const std::vector<IntVar*> vars =
      data->GetOrCreateVariableArray(ct->arguments[0]);
  Constraint* const constraint =
      s->MakeInversePermutationConstraint(vars, vars);
  AddConstraint(s, ct, constraint);
}
}  // namespace

namespace fz {
void ExtractConstraint(SolverData* data, Constraint* ct) {
  FZVLOG << "Extracting " << ct->DebugString() << std::endl;
  const std::string& type = ct->type;
  if (type == "all_different_int") {
    ExtractAllDifferentInt(data, ct);
  } else if (type == "alldifferent_except_0") {
    ExtractAlldifferentExcept0(data, ct);
  } else if (type == "among") {
    ExtractAmong(data, ct);
  } else if (type == "array_bool_and") {
    ExtractArrayBoolAnd(data, ct);
  } else if (type == "array_bool_element") {
    ExtractArrayIntElement(data, ct);
  } else if (type == "array_bool_or") {
    ExtractArrayBoolOr(data, ct);
  } else if (type == "array_bool_xor") {
    ExtractArrayBoolXor(data, ct);
  } else if (type == "array_int_element") {
    ExtractArrayIntElement(data, ct);
  } else if (type == "array_var_bool_element") {
    ExtractArrayVarIntElement(data, ct);
  } else if (type == "array_var_int_element") {
    ExtractArrayVarIntElement(data, ct);
  } else if (type == "at_most_int") {
    ExtractAtMostInt(data, ct);
  } else if (type == "bool_and") {
    ExtractBoolAnd(data, ct);
  } else if (type == "bool_clause") {
    ExtractBoolClause(data, ct);
  } else if (type == "bool_eq" || type == "bool2int") {
    ExtractIntEq(data, ct);
  } else if (type == "bool_eq_reif") {
    ExtractIntEqReif(data, ct);
  } else if (type == "bool_ge") {
    ExtractIntGe(data, ct);
  } else if (type == "bool_ge_reif") {
    ExtractIntGeReif(data, ct);
  } else if (type == "bool_gt") {
    ExtractIntGt(data, ct);
  } else if (type == "bool_gt_reif") {
    ExtractIntGtReif(data, ct);
  } else if (type == "bool_le") {
    ExtractIntLe(data, ct);
  } else if (type == "bool_le_reif") {
    ExtractIntLeReif(data, ct);
  } else if (type == "bool_left_imp") {
    ExtractIntLe(data, ct);
  } else if (type == "bool_lin_eq") {
    ExtractIntLinEq(data, ct);
  } else if (type == "bool_lin_le") {
    ExtractIntLinLe(data, ct);
  } else if (type == "bool_lt") {
    ExtractIntLt(data, ct);
  } else if (type == "bool_lt_reif") {
    ExtractIntLtReif(data, ct);
  } else if (type == "bool_ne") {
    ExtractIntNe(data, ct);
  } else if (type == "bool_ne_reif") {
    ExtractIntNeReif(data, ct);
  } else if (type == "bool_not") {
    ExtractBoolNot(data, ct);
  } else if (type == "bool_or") {
    ExtractBoolOr(data, ct);
  } else if (type == "bool_right_imp") {
    ExtractIntGe(data, ct);
  } else if (type == "bool_xor") {
    ExtractBoolXor(data, ct);
  } else if (type == "circuit") {
    ExtractCircuit(data, ct);
  } else if (type == "count_eq" || type == "count") {
    ExtractCountEq(data, ct);
  } else if (type == "count_geq") {
    ExtractCountGeq(data, ct);
  } else if (type == "count_gt") {
    ExtractCountGt(data, ct);
  } else if (type == "count_leq") {
    ExtractCountLeq(data, ct);
  } else if (type == "count_lt") {
    ExtractCountLt(data, ct);
  } else if (type == "count_neq") {
    ExtractCountNeq(data, ct);
  } else if (type == "count_reif") {
    ExtractCountReif(data, ct);
  } else if (type == "cumulative" || type == "var_cumulative" ||
             type == "variable_cumulative" || type == "fixed_cumulative") {
    ExtractCumulative(data, ct);
  } else if (type == "diffn") {
    ExtractDiffn(data, ct);
  } else if (type == "diffn_k_with_sizes") {
    ExtractDiffnK(data, ct);
  } else if (type == "diffn_nonstrict") {
    ExtractDiffnNonStrict(data, ct);
  } else if (type == "diffn_nonstrict_k_with_sizes") {
    ExtractDiffnNonStrictK(data, ct);
  } else if (type == "disjunctive") {
    ExtractDisjunctive(data, ct);
  } else if (type == "disjunctive_strict") {
    ExtractDisjunctiveStrict(data, ct);
  } else if (type == "false_constraint") {
    ExtractFalseConstraint(data, ct);
  } else if (type == "global_cardinality") {
    ExtractGlobalCardinality(data, ct);
  } else if (type == "global_cardinality_closed") {
    ExtractGlobalCardinalityClosed(data, ct);
  } else if (type == "global_cardinality_low_up") {
    ExtractGlobalCardinalityLowUp(data, ct);
  } else if (type == "global_cardinality_low_up_closed") {
    ExtractGlobalCardinalityLowUpClosed(data, ct);
  } else if (type == "global_cardinality_old") {
    ExtractGlobalCardinalityOld(data, ct);
  } else if (type == "int_abs") {
    ExtractIntAbs(data, ct);
  } else if (type == "int_div") {
    ExtractIntDiv(data, ct);
  } else if (type == "int_eq") {
    ExtractIntEq(data, ct);
  } else if (type == "int_eq_reif") {
    ExtractIntEqReif(data, ct);
  } else if (type == "int_ge") {
    ExtractIntGe(data, ct);
  } else if (type == "int_ge_reif") {
    ExtractIntGeReif(data, ct);
  } else if (type == "int_gt") {
    ExtractIntGt(data, ct);
  } else if (type == "int_gt_reif") {
    ExtractIntGtReif(data, ct);
  } else if (type == "int_le") {
    ExtractIntLe(data, ct);
  } else if (type == "int_le_reif") {
    ExtractIntLeReif(data, ct);
  } else if (type == "int_lin_eq") {
    ExtractIntLinEq(data, ct);
  } else if (type == "int_lin_eq_reif") {
    ExtractIntLinEqReif(data, ct);
  } else if (type == "int_lin_ge") {
    ExtractIntLinGe(data, ct);
  } else if (type == "int_lin_ge_reif") {
    ExtractIntLinGeReif(data, ct);
  } else if (type == "int_lin_le") {
    ExtractIntLinLe(data, ct);
  } else if (type == "int_lin_le_reif") {
    ExtractIntLinLeReif(data, ct);
  } else if (type == "int_lin_ne") {
    ExtractIntLinNe(data, ct);
  } else if (type == "int_lin_ne_reif") {
    ExtractIntLinNeReif(data, ct);
  } else if (type == "int_lt") {
    ExtractIntLt(data, ct);
  } else if (type == "int_lt_reif") {
    ExtractIntLtReif(data, ct);
  } else if (type == "int_max") {
    ExtractIntMax(data, ct);
  } else if (type == "int_min") {
    ExtractIntMin(data, ct);
  } else if (type == "int_minus") {
    ExtractIntMinus(data, ct);
  } else if (type == "int_mod") {
    ExtractIntMod(data, ct);
  } else if (type == "int_ne") {
    ExtractIntNe(data, ct);
  } else if (type == "int_ne_reif") {
    ExtractIntNeReif(data, ct);
  } else if (type == "int_negate") {
    ExtractIntNegate(data, ct);
  } else if (type == "int_plus") {
    ExtractIntPlus(data, ct);
  } else if (type == "int_times") {
    ExtractIntTimes(data, ct);
  } else if (type == "inverse") {
    ExtractInverse(data, ct);
  } else if (type == "lex_less_bool" || type == "lex_less_int") {
    ExtractLexLessInt(data, ct);
  } else if (type == "lex_lesseq_bool" || type == "lex_lesseq_int") {
    ExtractLexLesseqInt(data, ct);
  } else if (type == "maximum_arg_int") {
    ExtractMaximumArgInt(data, ct);
  } else if (type == "maximum_int" || type == "array_int_maximum") {
    ExtractMaximumInt(data, ct);
  } else if (type == "minimum_arg_int") {
    ExtractMinimumArgInt(data, ct);
  } else if (type == "minimum_int" || type == "array_int_minimum") {
    ExtractMinimumInt(data, ct);
  } else if (type == "nvalue") {
    ExtractNvalue(data, ct);
  } else if (type == "regular") {
    ExtractRegular(data, ct);
  } else if (type == "regular_nfa") {
    ExtractRegularNfa(data, ct);
  } else if (type == "set_in" || type == "int_in") {
    ExtractSetIn(data, ct);
  } else if (type == "set_not_in" || type == "int_not_in") {
    ExtractSetNotIn(data, ct);
  } else if (type == "set_in_reif") {
    ExtractSetInReif(data, ct);
  } else if (type == "sliding_sum") {
    ExtractSlidingSum(data, ct);
  } else if (type == "sort") {
    ExtractSort(data, ct);
  } else if (type == "subcircuit") {
    ExtractSubCircuit(data, ct);
  } else if (type == "symmetric_all_different") {
    ExtractSymmetricAllDifferent(data, ct);
  } else if (type == "table_bool" || type == "table_int") {
    ExtractTableInt(data, ct);
  } else if (type == "true_constraint") {
    // Nothing to do.
  } else {
    LOG(FATAL) << "Unknown predicate: " << type;
  }
}
}  // namespace fz
}  // namespace operations_research
