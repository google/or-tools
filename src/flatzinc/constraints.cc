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

#include <string>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/hash.h"
#include "flatzinc/flatzinc_constraints.h"
#include "flatzinc/model.h"
#include "flatzinc/sat_constraint.h"
#include "flatzinc/search.h"
#include "flatzinc/solver.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

DECLARE_bool(use_sat);
DECLARE_bool(fz_verbose);

// TODO (minizinc 2.0 support):
//  - arg_sort
//  - geost
// Not necessary?:
//  - knapsack
//  - network_flow
// Done:
//  - arg_max, arg_min
//  - disjunctive:
//  - regular with NFAs
//  - symmetric all different
//  - strict diffn, kdiffn, strict disjunctive,
//  - k-dimensional diffn
// Later:
//   - optional scheduling constraints: alternative, span, disjunctive,
//     cumulative
//   - functional versions of many global constraints

namespace operations_research {
namespace {
void AddConstraint(Solver* s, FzConstraint* ct, Constraint* cte) {
  FZVLOG << "  - post " << cte->DebugString() << FZENDL;
  s->AddConstraint(cte);
}

void ExtractAllDifferentInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(0));
  Constraint* const constraint = s->MakeAllDifferent(vars, vars.size() < 100);
  AddConstraint(s, ct, constraint);
}

void ExtractAlldifferentExcept0(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(0));
  AddConstraint(s, ct, s->MakeAllDifferentExcept(vars, 0));
}

void ExtractAmong(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> tmp_sum;
  const std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(1));
  for (IntVar* const var : tmp_vars) {
    const FzArgument& arg = ct->Arg(2);
    switch (arg.type) {
      case FzArgument::INT_VALUE: {
        tmp_sum.push_back(solver->MakeIsEqualCstVar(var, arg.values[0]));
        break;
      }
      case FzArgument::INT_INTERVAL: {
        if (var->Min() < arg.values[0] || var->Max() > arg.values[1]) {
          tmp_sum.push_back(
              solver->MakeIsBetweenVar(var, arg.values[0], arg.values[1]));
        }
        break;
      }
      case FzArgument::INT_LIST: {
        tmp_sum.push_back(solver->MakeIsMemberVar(var, arg.values));
        break;
      }
      default: { LOG(FATAL) << "Invalid constraint " << ct->DebugString(); }
    }
  }
  if (ct->Arg(0).HasOneValue()) {
    const int64 count = ct->Arg(0).Value();
    Constraint* const constraint = solver->MakeSumEquality(tmp_sum, count);
    AddConstraint(solver, ct, constraint);
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(0))->Var();
    Constraint* const constraint = solver->MakeSumEquality(tmp_sum, count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractArrayBoolAnd(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> variables;
  hash_set<IntExpr*> added;
  const std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
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
    fzsolver->SetExtracted(ct->target_variable, boolvar);
  } else if (ct->Arg(1).HasOneValue() && ct->Arg(1).Value() == 1) {
    FZVLOG << "  - forcing array_bool_and to 1" << FZENDL;
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(1);
    }
  } else {
    if (ct->Arg(1).HasOneValue()) {
      if (ct->Arg(1).Value() == 0) {
        if (FLAGS_use_sat &&
            AddBoolAndArrayEqualFalse(fzsolver->Sat(), variables)) {
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
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(1))->Var();
      if (FLAGS_use_sat &&
          AddBoolAndArrayEqVar(fzsolver->Sat(), variables, boolvar)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeMinEquality(variables, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractArrayBoolOr(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> variables;
  hash_set<IntExpr*> added;
  const std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
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
    fzsolver->SetExtracted(ct->target_variable, boolvar);
  } else if (ct->Arg(1).HasOneValue() && ct->Arg(1).Value() == 0) {
    FZVLOG << "  - forcing array_bool_or to 0" << FZENDL;
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(0);
    }
  } else {
    if (ct->Arg(1).HasOneValue()) {
      if (ct->Arg(1).Value() == 1) {
        if (FLAGS_use_sat &&
            AddBoolOrArrayEqualTrue(fzsolver->Sat(), variables)) {
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
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(1))->Var();
      if (FLAGS_use_sat &&
          AddBoolOrArrayEqVar(fzsolver->Sat(), variables, boolvar)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeMaxEquality(variables, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractArrayBoolXor(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
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
        if (FLAGS_use_sat &&
            AddBoolNot(fzsolver->Sat(), variables[0], variables[1])) {
          FZVLOG << "  - posted to sat" << FZENDL;
        } else {
          PostBooleanSumInRange(fzsolver->Sat(), solver, variables, 1, 1);
        }
      } else {
        if (FLAGS_use_sat &&
            AddBoolEq(fzsolver->Sat(), variables[0], variables[1])) {
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

void ExtractArrayIntElement(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const index = fzsolver->GetExpression(ct->Arg(0));
    const std::vector<int64>& values = ct->Arg(1).values;
    const int64 imin = std::max(index->Min(), 1LL);
    const int64 imax = std::min<int64>(index->Max(), values.size());
    IntVar* const shifted_index = solver->MakeSum(index, -imin)->Var();
    const int64 size = imax - imin + 1;
    std::vector<int64> coefficients(size);
    for (int i = 0; i < size; ++i) {
      coefficients[i] = values[i + imin - 1];
    }
    if (ct->target_variable != nullptr) {
      DCHECK_EQ(ct->Arg(2).Var(), ct->target_variable);
      IntExpr* const target = solver->MakeElement(coefficients, shifted_index);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    } else {
      IntVar* const target = fzsolver->GetExpression(ct->Arg(2))->Var();
      Constraint* const constraint =
          solver->MakeElementEquality(coefficients, shifted_index, target);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    CHECK_EQ(2, ct->Arg(0).variables.size());
    CHECK_EQ(5, ct->arguments.size());
    CHECK(ct->target_variable == nullptr);
    IntVar* const index1 = fzsolver->Extract(ct->Arg(0).variables[0])->Var();
    IntVar* const index2 = fzsolver->Extract(ct->Arg(0).variables[1])->Var();
    const int64 coef1 = ct->Arg(3).values[0];
    const int64 coef2 = ct->Arg(3).values[1];
    const int64 offset = ct->Arg(4).values[0];
    const std::vector<int64>& values = ct->Arg(1).values;
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
    IntVar* const target = fzsolver->GetExpression(ct->Arg(2))->Var();
    variables.push_back(target);
    Constraint* const constraint =
        solver->MakeAllowedAssignments(variables, tuples);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractArrayVarIntElement(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const index = fzsolver->GetExpression(ct->Arg(0));
  const int64 array_size = ct->Arg(1).variables.size();
  const int64 imin = std::max(index->Min(), 1LL);
  const int64 imax = std::min(index->Max(), array_size);
  IntVar* const shifted_index = solver->MakeSum(index, -imin)->Var();
  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(1));
  const int64 size = imax - imin + 1;
  std::vector<IntVar*> var_array(size);
  for (int i = 0; i < size; ++i) {
    var_array[i] = vars[i + imin - 1];
  }

  if (ct->target_variable != nullptr) {
    DCHECK_EQ(ct->Arg(2).Var(), ct->target_variable);
    IntExpr* const target = solver->MakeElement(var_array, shifted_index);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    Constraint* constraint = nullptr;
    if (ct->Arg(2).HasOneValue()) {
      const int64 target = ct->Arg(2).Value();
      if (fzsolver->IsAllDifferent(ct->Arg(1).variables)) {
        constraint =
            solver->MakeIndexOfConstraint(var_array, shifted_index, target);
      } else {
        constraint =
            solver->MakeElementEquality(var_array, shifted_index, target);
      }
    } else {
      IntVar* const target = fzsolver->GetExpression(ct->Arg(2))->Var();
      constraint =
          solver->MakeElementEquality(var_array, shifted_index, target);
    }
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractBoolAnd(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMin(left, right);
    FZVLOG << "  - creating " << ct->Arg(2).DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    if (FLAGS_use_sat &&
        AddBoolAndEqVar(fzsolver->Sat(), left, right, target)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeMin(left, right), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractBoolClause(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> positive_variables = fzsolver->GetVariableArray(ct->Arg(0));
  const std::vector<IntVar*> negative_variables =
      fzsolver->GetVariableArray(ct->Arg(1));
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
  if (FLAGS_use_sat && AddBoolOrArrayEqualTrue(fzsolver->Sat(), vars)) {
    FZVLOG << "  - posted to sat";
  } else {
    Constraint* const constraint = solver->MakeSumGreaterOrEqual(vars, 1);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractBoolNot(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ct->target_variable != nullptr) {
    if (ct->target_variable == ct->Arg(1).Var()) {
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
      IntExpr* const target = solver->MakeDifference(1, left);
      FZVLOG << "  - creating " << ct->Arg(1).DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    } else {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      IntExpr* const target = solver->MakeDifference(1, right);
      FZVLOG << "  - creating " << ct->Arg(0).DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    }
  } else {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
    if (FLAGS_use_sat && AddBoolNot(fzsolver->Sat(), left, right)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeDifference(1, left), right);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractBoolOr(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMax(left, right);
    FZVLOG << "  - creating " << ct->Arg(2).DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    if (FLAGS_use_sat && AddBoolOrEqVar(fzsolver->Sat(), left, right, target)) {
      FZVLOG << "  - posted to sat";
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeMax(left, right), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractBoolXor(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
  IntVar* const target = fzsolver->GetExpression(ct->Arg(2))->Var();
  if (FLAGS_use_sat && AddBoolIsNEqVar(fzsolver->Sat(), left, right, target)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else {
    Constraint* const constraint =
        solver->MakeIsEqualCstCt(solver->MakeSum(left, right), 1, target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCircuit(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
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

std::vector<IntVar*> BuildCount(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> tmp_sum;
  if (ct->Arg(0).variables.empty()) {
    if (ct->Arg(1).HasOneValue()) {
      const int64 value = ct->Arg(1).Value();
      for (const int64 v : ct->Arg(0).values) {
        if (v == value) {
          tmp_sum.push_back(solver->MakeIntConst(1));
        }
      }
    } else {
      IntVar* const count_var = fzsolver->GetExpression(ct->Arg(1))->Var();
      tmp_sum.push_back(solver->MakeIsMemberVar(count_var, ct->Arg(0).values));
    }
  } else {
    if (ct->Arg(1).HasOneValue()) {
      const int64 value = ct->Arg(1).Value();
      for (FzIntegerVariable* const fzvar : ct->Arg(0).variables) {
        IntVar* const var =
            solver->MakeIsEqualCstVar(fzsolver->Extract(fzvar), value);
        if (var->Max() == 1) {
          tmp_sum.push_back(var);
        }
      }
    } else {
      IntVar* const value = fzsolver->GetExpression(ct->Arg(1))->Var();
      for (FzIntegerVariable* const fzvar : ct->Arg(0).variables) {
        IntVar* const var =
            solver->MakeIsEqualVar(fzsolver->Extract(fzvar), value);
        if (var->Max() == 1) {
          tmp_sum.push_back(var);
        }
      }
    }
  }
  return tmp_sum;
}

void ExtractCountEq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(fzsolver, ct);
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    PostBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, count, count);
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint = solver->MakeSumEquality(tmp_sum, count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountGeq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(fzsolver, ct);
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    PostBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, count,
                          tmp_sum.size());
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint =
        solver->MakeGreaterOrEqual(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountGt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(fzsolver, ct);
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    PostBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, count + 1,
                          tmp_sum.size());
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint =
        solver->MakeGreater(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountLeq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(fzsolver, ct);
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    PostBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, 0, count);
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint =
        solver->MakeLessOrEqual(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountLt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(fzsolver, ct);
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    PostBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, 0, count - 1);
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint =
        solver->MakeLess(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountNeq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(fzsolver, ct);
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    if (count == 0) {
      PostBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, 1,
                            tmp_sum.size());
    } else if (count == tmp_sum.size()) {
      PostBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, 0,
                            tmp_sum.size() - 1);
    } else {
      Constraint* const constraint =
          solver->MakeNonEquality(solver->MakeSum(tmp_sum), count);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint =
        solver->MakeNonEquality(solver->MakeSum(tmp_sum), count);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractCountReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_sum = BuildCount(fzsolver, ct);
  IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    PostIsBooleanSumInRange(fzsolver->Sat(), solver, tmp_sum, count, count,
                            boolvar);
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint =
        solver->MakeIsEqualCt(solver->MakeSum(tmp_sum), count, boolvar);
    AddConstraint(solver, ct, constraint);
  }
}

bool IsHiddenPerformed(FzSolver* fzsolver,
                       const std::vector<FzIntegerVariable*>& fz_vars) {
  for (FzIntegerVariable* const fz_var : fz_vars) {
    IntVar* const var = fzsolver->Extract(fz_var)->Var();
    if (var->Size() > 2 || (var->Size() == 2 && var->Min() != 0)) {
      return false;
    }
    if (!var->Bound() && var->Max() != 1) {
      IntExpr* sub = nullptr;
      int64 coef = 1;
      if (!fzsolver->solver()->IsProduct(var, &sub, &coef)) {
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

void ExtractCumulative(FzSolver* fzsolver, FzConstraint* ct) {
  // This constraint has many possible translation into the CP library.
  // First we parse the arguments.
  Solver* const solver = fzsolver->solver();
  // Parse start variables.
  const std::vector<IntVar*> start_variables =
      fzsolver->GetVariableArray(ct->Arg(0));

  // Parse durations.
  std::vector<int64> fixed_durations;
  std::vector<IntVar*> variable_durations;
  if (ct->Arg(1).type == FzArgument::INT_LIST) {
    fixed_durations = ct->Arg(1).values;
  } else {
    variable_durations = fzsolver->GetVariableArray(ct->Arg(1));
    if (AreAllBound(variable_durations)) {
      FillValues(variable_durations, &fixed_durations);
      variable_durations.clear();
    }
  }

  // Parse demands.
  std::vector<int64> fixed_demands;
  std::vector<IntVar*> variable_demands;
  if (ct->Arg(2).type == FzArgument::INT_LIST) {
    fixed_demands = ct->Arg(2).values;
  } else {
    variable_demands = fzsolver->GetVariableArray(ct->Arg(2));
    if (AreAllBound(variable_demands)) {
      FillValues(variable_demands, &fixed_demands);
      variable_demands.clear();
    }
  }

  // Parse capacity.
  int64 fixed_capacity = 0;
  IntVar* variable_capacity = nullptr;
  if (ct->Arg(3).HasOneValue()) {
    fixed_capacity = ct->Arg(3).Value();
  } else {
    variable_capacity = fzsolver->GetExpression(ct->Arg(3))->Var();
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
      IsHiddenPerformed(fzsolver, ct->Arg(2).variables) &&
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

void ExtractDiffn(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> x_variables = fzsolver->GetVariableArray(ct->Arg(0));
  const std::vector<IntVar*> y_variables = fzsolver->GetVariableArray(ct->Arg(1));
  if (ct->Arg(2).type == FzArgument::INT_LIST &&
      ct->Arg(3).type == FzArgument::INT_LIST) {
    const std::vector<int64>& x_sizes = ct->Arg(2).values;
    const std::vector<int64>& y_sizes = ct->Arg(3).values;
    Constraint* const constraint = solver->MakeNonOverlappingBoxesConstraint(
        x_variables, y_variables, x_sizes, y_sizes);
    AddConstraint(solver, ct, constraint);
  } else {
    const std::vector<IntVar*> x_sizes = fzsolver->GetVariableArray(ct->Arg(2));
    const std::vector<IntVar*> y_sizes = fzsolver->GetVariableArray(ct->Arg(3));
    Constraint* const constraint = solver->MakeNonOverlappingBoxesConstraint(
        x_variables, y_variables, x_sizes, y_sizes);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractDiffnK(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> flat_x = fzsolver->GetVariableArray(ct->Arg(0));
  const std::vector<IntVar*> flat_dx = fzsolver->GetVariableArray(ct->Arg(1));
  const int num_boxes = ct->Arg(2).Value();
  const int num_dims = ct->Arg(3).Value();
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

void ExtractDiffnNonStrict(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
    const std::vector<IntVar*> x_variables = fzsolver->GetVariableArray(ct->Arg(0));
    const std::vector<IntVar*> y_variables = fzsolver->GetVariableArray(ct->Arg(1));
    if (ct->Arg(2).type == FzArgument::INT_LIST &&
        ct->Arg(3).type == FzArgument::INT_LIST) {
      const std::vector<int64>& x_sizes = ct->Arg(2).values;
      const std::vector<int64>& y_sizes = ct->Arg(3).values;
      Constraint* const constraint =
          solver->MakeNonOverlappingNonStrictBoxesConstraint(
              x_variables, y_variables, x_sizes, y_sizes);
      AddConstraint(solver, ct, constraint);
    } else {
      const std::vector<IntVar*> x_sizes = fzsolver->GetVariableArray(ct->Arg(2));
      const std::vector<IntVar*> y_sizes = fzsolver->GetVariableArray(ct->Arg(3));
      Constraint* const constraint =
          solver->MakeNonOverlappingNonStrictBoxesConstraint(
              x_variables, y_variables, x_sizes, y_sizes);
      AddConstraint(solver, ct, constraint);
    }

}

void ExtractDiffnNonStrictK(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> flat_x = fzsolver->GetVariableArray(ct->Arg(0));
  const std::vector<IntVar*> flat_dx = fzsolver->GetVariableArray(ct->Arg(1));
  const int num_boxes = ct->Arg(2).Value();
  const int num_dims = ct->Arg(3).Value();
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

void ExtractDisjunctive(FzSolver* fzsolver, FzConstraint* ct) {
  // This constraint has many possible translation into the CP library.
  // First we parse the arguments.
  Solver* const solver = fzsolver->solver();
  // Parse start variables.
  const std::vector<IntVar*> start_variables =
      fzsolver->GetVariableArray(ct->Arg(0));

  // Parse durations.
  std::vector<int64> fixed_durations;
  std::vector<IntVar*> variable_durations;
  if (ct->Arg(1).type == FzArgument::INT_LIST) {
    fixed_durations = ct->Arg(1).values;
  } else {
    variable_durations = fzsolver->GetVariableArray(ct->Arg(1));
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

void ExtractDisjunctiveStrict(FzSolver* fzsolver, FzConstraint* ct) {
  // This constraint has many possible translation into the CP library.
  // First we parse the arguments.
  Solver* const solver = fzsolver->solver();
  // Parse start variables.
  const std::vector<IntVar*> start_variables =
      fzsolver->GetVariableArray(ct->Arg(0));

  // Parse durations.
  std::vector<int64> fixed_durations;
  std::vector<IntVar*> variable_durations;
  if (ct->Arg(1).type == FzArgument::INT_LIST) {
    fixed_durations = ct->Arg(1).values;
  } else {
    variable_durations = fzsolver->GetVariableArray(ct->Arg(1));
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

void ExtractFalseConstraint(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  Constraint* const constraint = solver->MakeFalseConstraint();
  AddConstraint(solver, ct, constraint);
}

void ExtractGlobalCardinality(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<int64> values = ct->Arg(1).values;
  std::vector<IntVar*> variables;
  const std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
  for (IntVar* const var : tmp_vars) {
    for (int v = 0; v < values.size(); ++v) {
      if (var->Contains(values[v])) {
        variables.push_back(var);
        break;
      }
    }
  }
  const std::vector<IntVar*> cards = fzsolver->GetVariableArray(ct->Arg(2));
  Constraint* const constraint =
      solver->MakeDistribute(variables, values, cards);
  AddConstraint(solver, ct, constraint);
  Constraint* const constraint2 =
      solver->MakeSumLessOrEqual(cards, variables.size());
  AddConstraint(solver, ct, constraint2);
}

void ExtractGlobalCardinalityClosed(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<int64> values = ct->Arg(1).values;
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));

  const std::vector<IntVar*> cards = fzsolver->GetVariableArray(ct->Arg(2));
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

void ExtractGlobalCardinalityLowUp(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<int64> values = ct->Arg(1).values;
  std::vector<IntVar*> variables;
  const std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
  for (IntVar* const var : tmp_vars) {
    for (int v = 0; v < values.size(); ++v) {
      if (var->Contains(values[v])) {
        variables.push_back(var);
        break;
      }
    }
  }
  const std::vector<int64>& low = ct->Arg(2).values;
  const std::vector<int64>& up = ct->Arg(3).values;
  Constraint* const constraint =
      solver->MakeDistribute(variables, values, low, up);
  AddConstraint(solver, ct, constraint);
}

void ExtractGlobalCardinalityLowUpClosed(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));
  const std::vector<int64> values = ct->Arg(1).values;
  const std::vector<int64>& low = ct->Arg(2).values;
  const std::vector<int64>& up = ct->Arg(3).values;
  Constraint* const constraint =
      solver->MakeDistribute(variables, values, low, up);
  AddConstraint(solver, ct, constraint);
  for (IntVar* const var : variables) {
    Constraint* const constraint2 = solver->MakeMemberCt(var, values);
    AddConstraint(solver, ct, constraint2);
  }
}

void ExtractGlobalCardinalityOld(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));
  const std::vector<IntVar*> cards = fzsolver->GetVariableArray(ct->Arg(1));
  Constraint* const constraint = solver->MakeDistribute(variables, cards);
  AddConstraint(solver, ct, constraint);
}

void ExtractIntAbs(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeAbs(left);
    FZVLOG << "  - creating " << ct->Arg(1).DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else if (ct->Arg(1).HasOneValue()) {
    const int64 value = ct->Arg(1).Value();
    std::vector<int64> values(2);
    values[0] = -value;
    values[1] = value;
    Constraint* const constraint = solver->MakeMemberCt(left, values);
    AddConstraint(solver, ct, constraint);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(1));
    Constraint* const constraint =
        solver->MakeAbsEquality(left->Var(), target->Var());
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntDiv(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  if (ct->target_variable != nullptr) {
    if (!ct->Arg(1).HasOneValue()) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      IntExpr* const target = solver->MakeDiv(left, right);
      FZVLOG << "  - creating " << ct->Arg(2).DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    } else {
      const int64 value = ct->Arg(1).Value();
      IntExpr* const target = solver->MakeDiv(left, value);
      FZVLOG << "  - creating " << ct->Arg(2).DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    }
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    if (!ct->Arg(1).HasOneValue()) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeDiv(left, right), target);
      AddConstraint(solver, ct, constraint);
    } else {
      Constraint* const constraint = solver->MakeEquality(
          solver->MakeDiv(left, ct->Arg(1).Value()), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractIntEq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      if (FLAGS_use_sat && AddBoolEq(fzsolver->Sat(), left, right)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeEquality(left, right));
      }
    } else {
      const int64 right = ct->Arg(1).Value();
      AddConstraint(s, ct, s->MakeEquality(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeEquality(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left != right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntEqReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ct->target_variable != nullptr) {
    CHECK_EQ(ct->target_variable, ct->Arg(2).Var());
    if (ct->Arg(1).HasOneValue()) {
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
      const int64 value = ct->Arg(1).Value();
      IntVar* const boolvar = solver->MakeIsEqualCstVar(left, value);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, boolvar);
    } else if (ct->Arg(0).HasOneValue()) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      const int64 value = ct->Arg(0).Value();
      IntVar* const boolvar = solver->MakeIsEqualCstVar(right, value);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      IntVar* tmp_var = nullptr;
      bool tmp_neg = 0;
      bool success = false;
      if (FLAGS_use_sat && solver->IsBooleanVar(left, &tmp_var, &tmp_neg) &&
          solver->IsBooleanVar(right, &tmp_var, &tmp_neg)) {
        // Try to post to sat.
        IntVar* const boolvar = solver->MakeBoolVar();
        if (AddIntEqReif(fzsolver->Sat(), left, right, boolvar)) {
          FZVLOG << "  - posted to sat" << FZENDL;
          FZVLOG << "  - creating " << ct->target_variable->DebugString()
                 << " := " << boolvar->DebugString() << FZENDL;
          fzsolver->SetExtracted(ct->target_variable, boolvar);
          success = true;
        }
      }
      if (!success) {
        IntVar* const boolvar =
            solver->MakeIsEqualVar(left, fzsolver->GetExpression(ct->Arg(1)));
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      }
    }
  } else {
    IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(2))->Var();
    if (ct->Arg(1).HasOneValue()) {
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
      const int64 value = ct->Arg(1).Value();
      Constraint* const constraint =
          solver->MakeIsEqualCstCt(left, value, boolvar);
      AddConstraint(solver, ct, constraint);
    } else if (ct->Arg(0).HasOneValue()) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      const int64 value = ct->Arg(0).Value();
      Constraint* const constraint =
          solver->MakeIsEqualCstCt(right, value, boolvar);
      AddConstraint(solver, ct, constraint);
    } else {
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1))->Var();
      if (FLAGS_use_sat &&
          AddIntEqReif(fzsolver->Sat(), left, right, boolvar)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeIsEqualCt(left, right, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractIntGe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      if (FLAGS_use_sat && AddBoolLe(fzsolver->Sat(), right, left)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeGreaterOrEqual(left, right));
      }
    } else {
      const int64 right = ct->Arg(1).Value();
      AddConstraint(s, ct, s->MakeGreaterOrEqual(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeLessOrEqual(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left < right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

#define EXTRACT_INT_XX_REIF(Op, Rev)                                        \
  Solver* const solver = fzsolver->solver();                                \
  if (ct->target_variable != nullptr) {                                     \
    IntVar* boolvar = nullptr;                                              \
    if (ct->Arg(0).HasOneValue()) {                                         \
      const int64 left = ct->Arg(0).Value();                                \
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));           \
      boolvar = solver->MakeIs##Rev##CstVar(right, left);                   \
    } else if (ct->Arg(1).HasOneValue()) {                                  \
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));            \
      const int64 right = ct->Arg(1).Value();                               \
      boolvar = solver->MakeIs##Op##CstVar(left, right);                    \
    } else {                                                                \
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));            \
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));           \
      boolvar = solver->MakeIs##Op##Var(left, right);                       \
    }                                                                       \
    FZVLOG << "  - creating " << ct->target_variable->DebugString()         \
           << " := " << boolvar->DebugString() << FZENDL;                   \
    fzsolver->SetExtracted(ct->target_variable, boolvar);                   \
  } else {                                                                  \
    IntVar* boolvar = nullptr;                                              \
    Constraint* constraint = nullptr;                                       \
    if (ct->Arg(0).HasOneValue()) {                                         \
      const int64 left = ct->Arg(0).Value();                                \
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));           \
      if (right->IsVar()) {                                                 \
        boolvar = solver->MakeIs##Rev##CstVar(right, left);                 \
      } else {                                                              \
        boolvar = fzsolver->GetExpression(ct->Arg(2))->Var();               \
        constraint = solver->MakeIs##Rev##CstCt(right, left, boolvar);      \
      }                                                                     \
    } else if (ct->Arg(1).HasOneValue()) {                                  \
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));            \
      const int64 right = ct->Arg(1).Value();                               \
      if (left->IsVar()) {                                                  \
        boolvar = solver->MakeIs##Op##CstVar(left, right);                  \
      } else {                                                              \
        boolvar = fzsolver->GetExpression(ct->Arg(2))->Var();               \
        constraint = solver->MakeIs##Op##CstCt(left, right, boolvar);       \
      }                                                                     \
    } else {                                                                \
      IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));            \
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));           \
      boolvar = fzsolver->GetExpression(ct->Arg(2))->Var();                 \
      constraint = solver->MakeIs##Op##Ct(left, right, boolvar);            \
    }                                                                       \
    if (constraint != nullptr) {                                            \
      AddConstraint(solver, ct, constraint);                                \
    } else {                                                                \
      IntVar* const previous = fzsolver->GetExpression(ct->Arg(2))->Var();  \
      FZVLOG << "  - creating and linking " << boolvar->DebugString()       \
             << " to " << previous->DebugString() << FZENDL;                \
      if (FLAGS_use_sat && AddBoolEq(fzsolver->Sat(), boolvar, previous)) { \
        FZVLOG << "  - posted to sat" << FZENDL;                            \
      } else {                                                              \
        Constraint* const constraint =                                      \
            solver->MakeEquality(boolvar, previous);                        \
        AddConstraint(solver, ct, constraint);                              \
      }                                                                     \
    }                                                                       \
  }

void ExtractIntGeReif(FzSolver* fzsolver, FzConstraint* ct) {
  EXTRACT_INT_XX_REIF(GreaterOrEqual, LessOrEqual)
}

void ExtractIntGt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeGreater(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      AddConstraint(s, ct, s->MakeGreater(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeLess(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left <= right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntGtReif(FzSolver* fzsolver, FzConstraint* ct) {
  EXTRACT_INT_XX_REIF(Greater, Less);
}

void ExtractIntLe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      if (FLAGS_use_sat && AddBoolLe(fzsolver->Sat(), left, right)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeLessOrEqual(left, right));
      }
    } else {
      const int64 right = ct->Arg(1).Value();
      AddConstraint(s, ct, s->MakeLessOrEqual(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeGreaterOrEqual(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left > right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntLeReif(FzSolver* fzsolver, FzConstraint* ct) {
  EXTRACT_INT_XX_REIF(LessOrEqual, GreaterOrEqual)
}

void ExtractIntLt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeLess(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      AddConstraint(s, ct, s->MakeLess(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeGreater(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left >= right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntLtReif(FzSolver* fzsolver, FzConstraint* ct) {
  EXTRACT_INT_XX_REIF(Less, Greater)
}

void ParseShortIntLin(FzSolver* fzsolver, FzConstraint* ct, IntExpr** left,
                      IntExpr** right) {
  Solver* const solver = fzsolver->solver();
  const std::vector<FzIntegerVariable*>& fzvars = ct->Arg(1).variables;
  const std::vector<int64>& coefficients = ct->Arg(0).values;
  int64 rhs = ct->Arg(2).Value();
  const int size = ct->Arg(0).values.size();
  *left = nullptr;
  *right = nullptr;

  switch (size) {
    case 0: {
      *left = solver->MakeIntConst(0);
      *right = solver->MakeIntConst(rhs);
      break;
    }
    case 1: {
      *left = solver->MakeProd(fzsolver->Extract(fzvars[0]), coefficients[0]);
      *right = solver->MakeIntConst(rhs);
      break;
    }
    case 2: {
      IntExpr* const e1 = fzsolver->Extract(fzvars[0]);
      IntExpr* const e2 = fzsolver->Extract(fzvars[1]);
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
      IntExpr* const e1 = fzsolver->Extract(fzvars[0]);
      IntExpr* const e2 = fzsolver->Extract(fzvars[1]);
      IntExpr* const e3 = fzsolver->Extract(fzvars[2]);
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
        *left = solver->MakeSum(solver->MakeProd(e1, c1), rhs);
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

void ParseLongIntLin(FzSolver* fzsolver, FzConstraint* ct,
                     std::vector<IntVar*>* vars, std::vector<int64>* coeffs, int64* rhs) {
  CHECK(vars != nullptr);
  CHECK(coeffs != nullptr);
  CHECK(rhs != nullptr);
  const std::vector<FzIntegerVariable*>& fzvars = ct->Arg(1).variables;
  const std::vector<int64>& coefficients = ct->Arg(0).values;
  *rhs = ct->Arg(2).values[0];
  const int size = fzvars.size();

  for (int i = 0; i < size; ++i) {
    const int64 coef = coefficients[i];
    IntVar* const var = fzsolver->Extract(fzvars[i])->Var();
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

bool AreAllExtractedAsVariables(FzSolver* const fzsolver,
                                const std::vector<FzIntegerVariable*>& fz_vars) {
  for (FzIntegerVariable* const fz_var : fz_vars) {
    IntExpr* const expr = fzsolver->Extract(fz_var);
    if (!expr->IsVar()) {
      return false;
    }
  }
  return true;
}

bool AreAllFzVariablesBoolean(FzSolver* fzsolver, FzConstraint* ct) {
  for (FzIntegerVariable* const fz_var : ct->Arg(1).variables) {
    IntVar* var = fzsolver->Extract(fz_var)->Var();
    if (var->Min() < 0 || var->Max() > 1) {
      return false;
    }
  }
  return true;
}

bool ExtractLinAsShort(FzSolver* fzsolver, FzConstraint* ct) {
  const int size = ct->Arg(0).values.size();
  if (ct->Arg(1).variables.empty()) {
    return false;
  }
  switch (size) {
    case 0:
      return true;
    case 1:
      return true;
    case 2:
    case 3: {
      if (AreAllOnes(ct->Arg(0).values) &&
          AreAllExtractedAsVariables(fzsolver, ct->Arg(1).variables) &&
          AreAllFzVariablesBoolean(fzsolver, ct)) {
        return false;
      } else {
        return true;
      }
    }
    default: { return false; }
  }
}

void ExtractIntLinEq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<FzIntegerVariable*>& fzvars = ct->Arg(1).variables;
  const std::vector<int64>& coefficients = ct->Arg(0).values;
  int64 rhs = ct->Arg(2).Value();
  const int size = ct->Arg(0).values.size();
  if (ct->target_variable != nullptr) {
    if (size == 2) {
      IntExpr* other = nullptr;
      int64 other_coef = 0;
      if (ct->target_variable == fzvars[0] && coefficients[0] == -1) {
        other = fzsolver->Extract(fzvars[1]);
        other_coef = coefficients[1];
      } else if (ct->target_variable == fzvars[1] && coefficients[1] == -1) {
        other = fzsolver->Extract(fzvars[0]);
        other_coef = coefficients[0];
      } else {
        LOG(FATAL) << "Invalid constraint " << ct->DebugString();
      }

      IntExpr* const target =
          solver->MakeSum(solver->MakeProd(other, other_coef), -rhs);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    } else {
      std::vector<int64> new_coefficients;
      std::vector<IntVar*> variables;
      int64 constant = 0;
      for (int i = 0; i < size; ++i) {
        if (fzvars[i] == ct->target_variable) {
          CHECK_EQ(-1, coefficients[i]);
        } else if (fzvars[i]->domain.IsSingleton()) {
          constant += coefficients[i] * fzvars[i]->Min();
        } else {
          const int64 coef = coefficients[i];
          IntVar* const var = fzsolver->Extract(fzvars[i])->Var();
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
      fzsolver->SetExtracted(ct->target_variable, target);
    }
  } else {
    Constraint* constraint = nullptr;
    if (ExtractLinAsShort(fzsolver, ct)) {
      IntExpr* left = nullptr;
      IntExpr* right = nullptr;
      ParseShortIntLin(fzsolver, ct, &left, &right);
      constraint = solver->MakeEquality(left, right);
    } else {
      std::vector<IntVar*> vars;
      std::vector<int64> coeffs;
      ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, rhs);
        return;
      } else {
        constraint = solver->MakeScalProdEquality(vars, coeffs, rhs);
      }
    }
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntLinEqReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ExtractLinAsShort(fzsolver, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(fzsolver, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsEqualVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      Constraint* const constraint =
          solver->MakeIsEqualCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        IntVar* const boolvar = solver->MakeBoolVar();
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        PostIsBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, rhs,
                                boolvar);
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar =
            solver->MakeIsEqualCstVar(solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, rhs,
                                boolvar);
      } else {
        Constraint* const constraint = solver->MakeIsEqualCstCt(
            solver->MakeScalProd(vars, coeffs), rhs, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractIntLinGe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const int size = ct->Arg(0).values.size();
  if (ExtractLinAsShort(fzsolver, ct)) {
    // Checks if it is not a hidden or.
    if (ct->Arg(2).Value() == 1 && AreAllOnes(ct->Arg(0).values)) {
      // Good candidate.
      bool ok = true;
      for (FzIntegerVariable* const var : ct->Arg(1).variables) {
        IntExpr* const expr = fzsolver->Extract(var);
        if (expr->Min() < 0 || expr->Max() > 1 || !expr->IsVar()) {
          ok = false;
          break;
        }
      }
      if (ok) {
        std::vector<IntVar*> vars;
        std::vector<int64> coeffs;
        int64 rhs = 0;
        ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
        PostBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, size);
        return;
      }
    }
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(fzsolver, ct, &left, &right);
    AddConstraint(solver, ct, solver->MakeGreaterOrEqual(left, right));
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
    if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
      PostBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, size);
    } else {
      AddConstraint(solver, ct,
                    solver->MakeScalProdGreaterOrEqual(vars, coeffs, rhs));
    }
  }
}

void ExtractIntLinGeReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const int size = ct->Arg(0).values.size();
  if (ExtractLinAsShort(fzsolver, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(fzsolver, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsGreaterOrEqualVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      Constraint* const constraint =
          solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) &&
          (AreAllOnes(coeffs) || (rhs == 1 && AreAllPositive(coeffs)))) {
        IntVar* const boolvar = solver->MakeBoolVar();
        PostIsBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, size,
                                boolvar);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar = solver->MakeIsGreaterOrEqualCstVar(
            solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, size,
                                boolvar);
      } else {
        Constraint* const constraint = solver->MakeIsGreaterOrEqualCstCt(
            solver->MakeScalProd(vars, coeffs), rhs, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

bool PostHiddenClause(SatPropagator* const sat, const std::vector<int64>& coeffs,
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

void ExtractIntLinLe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ExtractLinAsShort(fzsolver, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(fzsolver, ct, &left, &right);
    AddConstraint(solver, ct, solver->MakeLessOrEqual(left, right));
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
    if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
      PostBooleanSumInRange(fzsolver->Sat(), solver, vars, 0, rhs);
    } else if (FLAGS_use_sat && AreAllBooleans(vars) && rhs == 0 &&
               PostHiddenClause(fzsolver->Sat(), coeffs, vars)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else if (FLAGS_use_sat && AreAllBooleans(vars) && rhs == 0 &&
               PostHiddenLeMax(fzsolver->Sat(), coeffs, vars)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      AddConstraint(solver, ct,
                    solver->MakeScalProdLessOrEqual(vars, coeffs, rhs));
    }
  }
}

void ExtractIntLinLeReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ExtractLinAsShort(fzsolver, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(fzsolver, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsLessOrEqualVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      Constraint* const constraint =
          solver->MakeIsLessOrEqualCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) &&
          (AreAllOnes(coeffs) || (rhs == 0 && AreAllPositive(coeffs)))) {
        IntVar* const boolvar = solver->MakeBoolVar();
        PostIsBooleanSumInRange(fzsolver->Sat(), solver, vars, 0, rhs, boolvar);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar = solver->MakeIsLessOrEqualCstVar(
            solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumInRange(fzsolver->Sat(), solver, vars, 0, rhs, boolvar);
      } else if (rhs == 0 && AreAllPositive(coeffs) && AreAllBooleans(vars)) {
        // Special case. this is or(vars) = not(boolvar).
        PostIsBooleanSumInRange(fzsolver->Sat(), solver, vars, 0, 0, boolvar);
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

void ExtractIntLinNe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const int size = ct->Arg(0).values.size();
  if (ExtractLinAsShort(fzsolver, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(fzsolver, ct, &left, &right);
    AddConstraint(solver, ct, solver->MakeNonEquality(left, right));
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
    if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
      PostBooleanSumInRange(fzsolver->Sat(), solver, vars, rhs, size);
    } else {
      AddConstraint(solver, ct, solver->MakeNonEquality(
                                    solver->MakeScalProd(vars, coeffs), rhs));
    }
  }
}

void ExtractIntLinNeReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ExtractLinAsShort(fzsolver, ct)) {
    IntExpr* left = nullptr;
    IntExpr* right = nullptr;
    ParseShortIntLin(fzsolver, ct, &left, &right);
    if (ct->target_variable != nullptr) {
      IntVar* const boolvar = solver->MakeIsDifferentVar(left, right);
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      Constraint* const constraint =
          solver->MakeIsDifferentCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  } else {
    std::vector<IntVar*> vars;
    std::vector<int64> coeffs;
    int64 rhs = 0;
    ParseLongIntLin(fzsolver, ct, &vars, &coeffs, &rhs);
    if (ct->target_variable != nullptr) {
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        IntVar* const boolvar = solver->MakeBoolVar();
        PostIsBooleanSumDifferent(fzsolver->Sat(), solver, vars, rhs, boolvar);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      } else {
        IntVar* const boolvar = solver->MakeIsDifferentCstVar(
            solver->MakeScalProd(vars, coeffs), rhs);
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      }
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(3))->Var();
      if (AreAllBooleans(vars) && AreAllOnes(coeffs)) {
        PostIsBooleanSumDifferent(fzsolver->Sat(), solver, vars, rhs, boolvar);
      } else {
        Constraint* const constraint = solver->MakeIsDifferentCstCt(
            solver->MakeScalProd(vars, coeffs), rhs, boolvar);
        AddConstraint(solver, ct, constraint);
      }
    }
  }
}

void ExtractIntMax(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMax(left, right);
    FZVLOG << "  - creating " << ct->Arg(2).DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeMax(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntMin(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeMin(left, right);
    FZVLOG << "  - creating " << ct->Arg(2).DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeMin(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntMinus(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeDifference(left, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeDifference(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntMod(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  if (ct->target_variable != nullptr) {
    if (!ct->Arg(1).HasOneValue()) {
      IntExpr* const mod = fzsolver->GetExpression(ct->Arg(1));
      IntExpr* const target = solver->MakeModulo(left, mod);
      FZVLOG << "  - creating " << ct->Arg(2).DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    } else {
      const int64 mod = ct->Arg(1).Value();
      IntExpr* const target = solver->MakeModulo(left, mod);
      FZVLOG << "  - creating " << ct->Arg(2).DebugString()
             << " := " << target->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, target);
    }
  } else if (ct->Arg(2).HasOneValue()) {
    const int64 target = ct->Arg(2).Value();
    if (!ct->Arg(1).HasOneValue()) {
      IntExpr* const mod = fzsolver->GetExpression(ct->Arg(1));
      Constraint* const constraint =
          MakeBoundModulo(solver, left->Var(), mod->Var(), target);
      AddConstraint(solver, ct, constraint);
    } else {
      const int64 mod = ct->Arg(1).Value();
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
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    if (!ct->Arg(1).HasOneValue()) {
      IntExpr* const mod = fzsolver->GetExpression(ct->Arg(1));
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeModulo(left, mod), target);
      AddConstraint(solver, ct, constraint);
    } else {
      const int64 mod = ct->Arg(1).Value();
      Constraint* constraint = nullptr;
      constraint = solver->MakeEquality(solver->MakeModulo(left, mod), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractIntNe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      if (FLAGS_use_sat && AddBoolNot(fzsolver->Sat(), left, right)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        AddConstraint(s, ct, s->MakeNonEquality(left, right));
      }
    } else {
      const int64 right = ct->Arg(1).Value();
      AddConstraint(s, ct, s->MakeNonEquality(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      AddConstraint(s, ct, s->MakeNonEquality(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left == right) {
        AddConstraint(s, ct, s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntNeReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  if (ct->target_variable != nullptr) {
    CHECK_EQ(ct->target_variable, ct->Arg(2).Var());
    if (ct->Arg(1).HasOneValue()) {
      IntVar* const boolvar =
          solver->MakeIsDifferentCstVar(left, ct->Arg(1).Value());
      FZVLOG << "  - creating " << ct->target_variable->DebugString()
             << " := " << boolvar->DebugString() << FZENDL;
      fzsolver->SetExtracted(ct->target_variable, boolvar);
    } else {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      IntVar* tmp_var = nullptr;
      bool tmp_neg = 0;
      bool success = false;
      if (FLAGS_use_sat && solver->IsBooleanVar(left, &tmp_var, &tmp_neg) &&
          solver->IsBooleanVar(right, &tmp_var, &tmp_neg)) {
        // Try to post to sat.
        IntVar* const boolvar = solver->MakeBoolVar();
        if (AddIntNeReif(fzsolver->Sat(), left, right, boolvar)) {
          FZVLOG << "  - posted to sat" << FZENDL;
          FZVLOG << "  - creating " << ct->target_variable->DebugString()
                 << " := " << boolvar->DebugString() << FZENDL;
          fzsolver->SetExtracted(ct->target_variable, boolvar);
          success = true;
        }
      }
      if (!success) {
        IntVar* const boolvar = solver->MakeIsDifferentVar(
            left, fzsolver->GetExpression(ct->Arg(1)));
        FZVLOG << "  - creating " << ct->target_variable->DebugString()
               << " := " << boolvar->DebugString() << FZENDL;
        fzsolver->SetExtracted(ct->target_variable, boolvar);
      }
    }
  } else {
    IntExpr* const right = fzsolver->GetExpression(ct->Arg(1))->Var();
    IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(2))->Var();
    if (FLAGS_use_sat && AddIntEqReif(fzsolver->Sat(), left, right, boolvar)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeIsDifferentCt(left, right, boolvar);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractIntNegate(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeOpposite(left);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeOpposite(left), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntPlus(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (!ct->Arg(0).variables.empty() &&
      ct->target_variable == ct->Arg(0).variables[0]) {
    IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    IntExpr* const left = solver->MakeDifference(target, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << left->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, left);
  } else if (!ct->Arg(1).variables.empty() &&
             ct->target_variable == ct->Arg(1).variables[0]) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    IntExpr* const right = solver->MakeDifference(target, left);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << right->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, right);
  } else if (!ct->Arg(2).variables.empty() &&
             ct->target_variable == ct->Arg(2).variables[0]) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
    IntExpr* const target = solver->MakeSum(left, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    Constraint* const constraint =
        solver->MakeEquality(solver->MakeSum(left, right), target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractIntTimes(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
  if (ct->target_variable != nullptr) {
    IntExpr* const target = solver->MakeProd(left, right);
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntExpr* const target = fzsolver->GetExpression(ct->Arg(2));
    if (FLAGS_use_sat &&
        AddBoolAndEqVar(fzsolver->Sat(), left, right, target)) {
      FZVLOG << "  - posted to sat" << FZENDL;
    } else {
      Constraint* const constraint =
          solver->MakeEquality(solver->MakeProd(left, right), target);
      AddConstraint(solver, ct, constraint);
    }
  }
}

void ExtractInverse(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> left;
  // Account for 1 based arrays.
  left.push_back(solver->MakeIntConst(0));
  std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
  left.insert(left.end(), tmp_vars.begin(), tmp_vars.end());

  std::vector<IntVar*> right;
  // Account for 1 based arrays.
  right.push_back(solver->MakeIntConst(0));
  tmp_vars = fzsolver->GetVariableArray(ct->Arg(1));
  right.insert(right.end(), tmp_vars.begin(), tmp_vars.end());

  Constraint* const constraint =
      solver->MakeInversePermutationConstraint(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractLexLessInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> left = fzsolver->GetVariableArray(ct->Arg(0));
  std::vector<IntVar*> right = fzsolver->GetVariableArray(ct->Arg(1));
  Constraint* const constraint = solver->MakeLexicalLess(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractLexLesseqInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  std::vector<IntVar*> left = fzsolver->GetVariableArray(ct->Arg(0));
  std::vector<IntVar*> right = fzsolver->GetVariableArray(ct->Arg(1));
  Constraint* const constraint = solver->MakeLexicalLessOrEqual(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractMaximumArgInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));
  IntVar* const index = fzsolver->GetExpression(ct->Arg(1))->Var();
  Constraint* const constraint =
      solver->MakeIndexOfFirstMaxValueConstraint(index, variables);
  AddConstraint(solver, ct, constraint);
}

void ExtractMaximumInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntVar* const target = fzsolver->GetExpression(ct->Arg(0))->Var();
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(1));
  Constraint* const constraint = solver->MakeMaxEquality(variables, target);
  AddConstraint(solver, ct, constraint);
}

void ExtractMinimumArgInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));
  IntVar* const index = fzsolver->GetExpression(ct->Arg(1))->Var();
  Constraint* const constraint =
      solver->MakeIndexOfFirstMinValueConstraint(index, variables);
  AddConstraint(solver, ct, constraint);
}

void ExtractMinimumInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  if (ct->target_variable != nullptr && ct->Arg(1).variables.size() < 3) {
    IntExpr* target = nullptr;
    switch (ct->Arg(1).variables.size()) {
      case 0: {
        target = solver->MakeIntConst(0);
        break;
      }
      case 1: {
        target = fzsolver->Extract(ct->Arg(1).variables[0]);
        break;
      }
      case 2: {
        IntExpr* const e0 = fzsolver->Extract(ct->Arg(1).variables[0]);
        IntExpr* const e1 = fzsolver->Extract(ct->Arg(1).variables[1]);
        target = solver->MakeMin(e0, e1);
        break;
      }
      // case 3: {
      //   IntExpr* const e0 = fzsolver->Extract(ct->Arg(1).variables[0]);
      //   IntExpr* const e1 = fzsolver->Extract(ct->Arg(1).variables[1]);
      //   IntExpr* const e2 = fzsolver->Extract(ct->Arg(1).variables[2]);
      //   target = solver->MakeMin(solver->MakeMin(e0, e1), e2);
      //   break;
      // }
      // case 4: {
      //   IntExpr* const e0 = fzsolver->Extract(ct->Arg(1).variables[0]);
      //   IntExpr* const e1 = fzsolver->Extract(ct->Arg(1).variables[1]);
      //   IntExpr* const e2 = fzsolver->Extract(ct->Arg(1).variables[2]);
      //   IntExpr* const e3 = fzsolver->Extract(ct->Arg(1).variables[3]);
      //   target = solver->MakeMin(solver->MakeMin(e0, e1),
      //                            solver->MakeMin(e2, e3));
      //   break;
      // }
      default: {
        target = solver->MakeMin(fzsolver->GetVariableArray(ct->Arg(1)));
        break;
      }
    }
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << target->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, target);
  } else {
    IntVar* const target = fzsolver->GetExpression(ct->Arg(0))->Var();
    const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(1));
    Constraint* const constraint = solver->MakeMinEquality(variables, target);
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractNvalue(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();

  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(1));

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
      if (FLAGS_use_sat &&
          AddBoolOrArrayEqVar(fzsolver->Sat(), contributors, contribution)) {
        FZVLOG << "  - posted to sat" << FZENDL;
      } else {
        Constraint* const constraint =
            solver->MakeMaxEquality(contributors, contribution);
        AddConstraint(solver, ct, constraint);
      }
      cards.push_back(contribution);
    }
  }
  if (ct->Arg(0).HasOneValue()) {
    const int64 card = ct->Arg(0).Value() - always_true_cards;
    PostBooleanSumInRange(fzsolver->Sat(), solver, cards, card, card);
  } else {
    IntVar* const card = fzsolver->GetExpression(ct->Arg(0))->Var();
    Constraint* const constraint = solver->MakeSumEquality(
        cards, solver->MakeSum(card, -always_true_cards)->Var());
    AddConstraint(solver, ct, constraint);
  }
}

void ExtractRegular(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();

  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));
  const int64 num_states = ct->Arg(1).Value();
  const int64 num_values = ct->Arg(2).Value();

  const std::vector<int64>& array_transitions = ct->Arg(3).values;
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

  const int64 initial_state = ct->Arg(4).Value();

  std::vector<int64> final_states;
  switch (ct->Arg(5).type) {
    case FzArgument::INT_VALUE: {
      final_states.push_back(ct->Arg(5).values[0]);
      break;
    }
    case FzArgument::INT_INTERVAL: {
      for (int v = ct->Arg(5).values[0]; v <= ct->Arg(5).values[1]; ++v) {
        final_states.push_back(v);
      }
      break;
    }
    case FzArgument::INT_LIST: {
      final_states = ct->Arg(5).values;
      break;
    }
    default: { LOG(FATAL) << "Wrong constraint " << ct->DebugString(); }
  }
  Constraint* const constraint = solver->MakeTransitionConstraint(
      variables, tuples, initial_state, final_states);
  AddConstraint(solver, ct, constraint);
}

void ExtractRegularNfa(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();

  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));
  const int64 num_states = ct->Arg(1).Value();
  const int64 num_values = ct->Arg(2).Value();

  const std::vector<FzDomain>& array_transitions = ct->Arg(3).domains;
  IntTupleSet tuples(3);
  int count = 0;
  for (int64 q = 1; q <= num_states; ++q) {
    for (int64 s = 1; s <= num_values; ++s) {
      const FzDomain& next = array_transitions[count++];
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

  const int64 initial_state = ct->Arg(4).Value();

  std::vector<int64> final_states;
  switch (ct->Arg(5).type) {
    case FzArgument::INT_VALUE: {
      final_states.push_back(ct->Arg(5).values[0]);
      break;
    }
    case FzArgument::INT_INTERVAL: {
      for (int v = ct->Arg(5).values[0]; v <= ct->Arg(5).values[1]; ++v) {
        final_states.push_back(v);
      }
      break;
    }
    case FzArgument::INT_LIST: {
      final_states = ct->Arg(5).values;
      break;
    }
    default: { LOG(FATAL) << "Wrong constraint " << ct->DebugString(); }
  }
  Constraint* const constraint = solver->MakeTransitionConstraint(
      variables, tuples, initial_state, final_states);
  AddConstraint(solver, ct, constraint);
}

void ExtractSetIn(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const expr = fzsolver->GetExpression(ct->Arg(0));
  const FzArgument& arg = ct->Arg(1);
  switch (arg.type) {
    case FzArgument::INT_VALUE: {
      Constraint* const constraint = solver->MakeEquality(expr, arg.values[0]);
      AddConstraint(solver, ct, constraint);
      break;
    }
    case FzArgument::INT_INTERVAL: {
      if (expr->Min() < arg.values[0] || expr->Max() > arg.values[1]) {
        Constraint* const constraint =
            solver->MakeBetweenCt(expr, arg.values[0], arg.values[1]);
        AddConstraint(solver, ct, constraint);
      }
      break;
    }
    case FzArgument::INT_LIST: {
      Constraint* const constraint = solver->MakeMemberCt(expr, arg.values);
      AddConstraint(solver, ct, constraint);
      break;
    }
    default: { LOG(FATAL) << "Invalid constraint " << ct->DebugString(); }
  }
}

void ExtractSetInReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const expr = fzsolver->GetExpression(ct->Arg(0));
  IntVar* const target = fzsolver->GetExpression(ct->Arg(2))->Var();
  const FzArgument& arg = ct->Arg(1);
  switch (arg.type) {
    case FzArgument::INT_VALUE: {
      Constraint* const constraint =
          solver->MakeIsEqualCstCt(expr, arg.values[0], target);
      AddConstraint(solver, ct, constraint);
      break;
    }
    case FzArgument::INT_INTERVAL: {
      if (expr->Min() < arg.values[0] || expr->Max() > arg.values[1]) {
        Constraint* const constraint =
            solver->MakeIsBetweenCt(expr, arg.values[0], arg.values[1], target);
        AddConstraint(solver, ct, constraint);
      }
      break;
    }
    case FzArgument::INT_LIST: {
      Constraint* const constraint =
          solver->MakeIsMemberCt(expr, arg.values, target);
      AddConstraint(solver, ct, constraint);
      break;
    }
    default: { LOG(FATAL) << "Invalid constraint " << ct->DebugString(); }
  }
}

void ExtractSlidingSum(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const int64 low = ct->Arg(0).Value();
  const int64 up = ct->Arg(1).Value();
  const int64 seq = ct->Arg(2).Value();
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(3));
  for (int i = 0; i < variables.size() - seq; ++i) {
    std::vector<IntVar*> tmp(seq);
    for (int k = 0; k < seq; ++k) {
      tmp[k] = variables[i + k];
    }
    IntVar* const sum_var = solver->MakeSum(tmp)->Var();
    sum_var->SetRange(low, up);
  }
}

void ExtractSort(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> left = fzsolver->GetVariableArray(ct->Arg(0));
  const std::vector<IntVar*> right = fzsolver->GetVariableArray(ct->Arg(1));
  Constraint* const constraint = solver->MakeSortingConstraint(left, right);
  AddConstraint(solver, ct, constraint);
}

void ExtractSubCircuit(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> tmp_vars = fzsolver->GetVariableArray(ct->Arg(0));
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

void ExtractTableInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<IntVar*> variables = fzsolver->GetVariableArray(ct->Arg(0));
  const int size = variables.size();
  IntTupleSet tuples(size);
  const std::vector<int64>& t = ct->Arg(1).values;
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

void ExtractSymmetricAllDifferent(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(0));
  Constraint* const constraint =
      s->MakeInversePermutationConstraint(vars, vars);
  AddConstraint(s, ct, constraint);
}

void ExtractTrueConstraint(FzSolver* fzsolver, FzConstraint* ct) {}
}  // namespace

void FzSolver::ExtractConstraint(FzConstraint* ct) {
  FZVLOG << "Extracting " << ct->DebugString() << std::endl;
  const std::string& type = ct->type;
  if (type == "all_different_int") {
    ExtractAllDifferentInt(this, ct);
  } else if (type == "alldifferent_except_0") {
    ExtractAlldifferentExcept0(this, ct);
  } else if (type == "among") {
    ExtractAmong(this, ct);
  } else if (type == "array_bool_and") {
    ExtractArrayBoolAnd(this, ct);
  } else if (type == "array_bool_element") {
    ExtractArrayIntElement(this, ct);
  } else if (type == "array_bool_or") {
    ExtractArrayBoolOr(this, ct);
  } else if (type == "array_bool_xor") {
    ExtractArrayBoolXor(this, ct);
  } else if (type == "array_int_element") {
    ExtractArrayIntElement(this, ct);
  } else if (type == "array_var_bool_element") {
    ExtractArrayVarIntElement(this, ct);
  } else if (type == "array_var_int_element") {
    ExtractArrayVarIntElement(this, ct);
  } else if (type == "bool_and") {
    ExtractBoolAnd(this, ct);
  } else if (type == "bool_clause") {
    ExtractBoolClause(this, ct);
  } else if (type == "bool_eq" || type == "bool2int") {
    ExtractIntEq(this, ct);
  } else if (type == "bool_eq_reif") {
    ExtractIntEqReif(this, ct);
  } else if (type == "bool_ge") {
    ExtractIntGe(this, ct);
  } else if (type == "bool_ge_reif") {
    ExtractIntGeReif(this, ct);
  } else if (type == "bool_gt") {
    ExtractIntGt(this, ct);
  } else if (type == "bool_gt_reif") {
    ExtractIntGtReif(this, ct);
  } else if (type == "bool_le") {
    ExtractIntLe(this, ct);
  } else if (type == "bool_le_reif") {
    ExtractIntLeReif(this, ct);
  } else if (type == "bool_left_imp") {
    ExtractIntLe(this, ct);
  } else if (type == "bool_lin_eq") {
    ExtractIntLinEq(this, ct);
  } else if (type == "bool_lin_le") {
    ExtractIntLinLe(this, ct);
  } else if (type == "bool_lt") {
    ExtractIntLt(this, ct);
  } else if (type == "bool_lt_reif") {
    ExtractIntLtReif(this, ct);
  } else if (type == "bool_ne") {
    ExtractIntNe(this, ct);
  } else if (type == "bool_ne_reif") {
    ExtractIntNeReif(this, ct);
  } else if (type == "bool_not") {
    ExtractBoolNot(this, ct);
  } else if (type == "bool_or") {
    ExtractBoolOr(this, ct);
  } else if (type == "bool_right_imp") {
    ExtractIntGe(this, ct);
  } else if (type == "bool_xor") {
    ExtractBoolXor(this, ct);
  } else if (type == "circuit") {
    ExtractCircuit(this, ct);
  } else if (type == "count_eq" || type == "count") {
    ExtractCountEq(this, ct);
  } else if (type == "count_geq") {
    ExtractCountGeq(this, ct);
  } else if (type == "count_gt") {
    ExtractCountGt(this, ct);
  } else if (type == "count_leq") {
    ExtractCountLeq(this, ct);
  } else if (type == "count_lt") {
    ExtractCountLt(this, ct);
  } else if (type == "count_neq") {
    ExtractCountNeq(this, ct);
  } else if (type == "count_reif") {
    ExtractCountReif(this, ct);
  } else if (type == "cumulative" || type == "var_cumulative" ||
             type == "variable_cumulative" || type == "fixed_cumulative") {
    ExtractCumulative(this, ct);
  } else if (type == "diffn") {
    ExtractDiffn(this, ct);
  } else if (type == "diffn_k_with_sizes") {
    ExtractDiffnK(this, ct);
  } else if (type == "diffn_nonstrict") {
    ExtractDiffnNonStrict(this, ct);
  } else if (type == "diffn_nonstrict_k_with_sizes") {
    ExtractDiffnNonStrictK(this, ct);
  } else if (type == "disjunctive") {
    ExtractDisjunctive(this, ct);
  } else if (type == "disjunctive_strict") {
    ExtractDisjunctiveStrict(this, ct);
  } else if (type == "false_constraint") {
    ExtractFalseConstraint(this, ct);
  } else if (type == "global_cardinality") {
    ExtractGlobalCardinality(this, ct);
  } else if (type == "global_cardinality_closed") {
    ExtractGlobalCardinalityClosed(this, ct);
  } else if (type == "global_cardinality_low_up") {
    ExtractGlobalCardinalityLowUp(this, ct);
  } else if (type == "global_cardinality_low_up_closed") {
    ExtractGlobalCardinalityLowUpClosed(this, ct);
  } else if (type == "global_cardinality_old") {
    ExtractGlobalCardinalityOld(this, ct);
  } else if (type == "int_abs") {
    ExtractIntAbs(this, ct);
  } else if (type == "int_div") {
    ExtractIntDiv(this, ct);
  } else if (type == "int_eq") {
    ExtractIntEq(this, ct);
  } else if (type == "int_eq_reif") {
    ExtractIntEqReif(this, ct);
  } else if (type == "int_ge") {
    ExtractIntGe(this, ct);
  } else if (type == "int_ge_reif") {
    ExtractIntGeReif(this, ct);
  } else if (type == "int_gt") {
    ExtractIntGt(this, ct);
  } else if (type == "int_gt_reif") {
    ExtractIntGtReif(this, ct);
  } else if (type == "int_le") {
    ExtractIntLe(this, ct);
  } else if (type == "int_le_reif") {
    ExtractIntLeReif(this, ct);
  } else if (type == "int_lin_eq") {
    ExtractIntLinEq(this, ct);
  } else if (type == "int_lin_eq_reif") {
    ExtractIntLinEqReif(this, ct);
  } else if (type == "int_lin_ge") {
    ExtractIntLinGe(this, ct);
  } else if (type == "int_lin_ge_reif") {
    ExtractIntLinGeReif(this, ct);
  } else if (type == "int_lin_le") {
    ExtractIntLinLe(this, ct);
  } else if (type == "int_lin_le_reif") {
    ExtractIntLinLeReif(this, ct);
  } else if (type == "int_lin_ne") {
    ExtractIntLinNe(this, ct);
  } else if (type == "int_lin_ne_reif") {
    ExtractIntLinNeReif(this, ct);
  } else if (type == "int_lt") {
    ExtractIntLt(this, ct);
  } else if (type == "int_lt_reif") {
    ExtractIntLtReif(this, ct);
  } else if (type == "int_max") {
    ExtractIntMax(this, ct);
  } else if (type == "int_min") {
    ExtractIntMin(this, ct);
  } else if (type == "int_minus") {
    ExtractIntMinus(this, ct);
  } else if (type == "int_mod") {
    ExtractIntMod(this, ct);
  } else if (type == "int_ne") {
    ExtractIntNe(this, ct);
  } else if (type == "int_ne_reif") {
    ExtractIntNeReif(this, ct);
  } else if (type == "int_negate") {
    ExtractIntNegate(this, ct);
  } else if (type == "int_plus") {
    ExtractIntPlus(this, ct);
  } else if (type == "int_times") {
    ExtractIntTimes(this, ct);
  } else if (type == "inverse") {
    ExtractInverse(this, ct);
  } else if (type == "lex_less_bool" || type == "lex_less_int") {
    ExtractLexLessInt(this, ct);
  } else if (type == "lex_lesseq_bool" || type == "lex_lesseq_int") {
    ExtractLexLesseqInt(this, ct);
  } else if (type == "maximum_arg_int") {
    ExtractMaximumArgInt(this, ct);
  } else if (type == "maximum_int" || type == "array_int_maximum") {
    ExtractMaximumInt(this, ct);
  } else if (type == "minimum_arg_int") {
    ExtractMinimumArgInt(this, ct);
  } else if (type == "minimum_int" || type == "array_int_minimum") {
    ExtractMinimumInt(this, ct);
  } else if (type == "nvalue") {
    ExtractNvalue(this, ct);
  } else if (type == "regular") {
    ExtractRegular(this, ct);
  } else if (type == "regular_nfa") {
    ExtractRegularNfa(this, ct);
  } else if (type == "set_in" || type == "int_in") {
    ExtractSetIn(this, ct);
  } else if (type == "set_in_reif") {
    ExtractSetInReif(this, ct);
  } else if (type == "sliding_sum") {
    ExtractSlidingSum(this, ct);
  } else if (type == "sort") {
    ExtractSort(this, ct);
  } else if (type == "subcircuit") {
    ExtractSubCircuit(this, ct);
  } else if (type == "symmetric_all_different") {
    ExtractSymmetricAllDifferent(this, ct);
  } else if (type == "table_bool" || type == "table_int") {
    ExtractTableInt(this, ct);
  } else if (type == "true_constraint") {
    ExtractTrueConstraint(this, ct);
  } else {
    LOG(FATAL) << "Unknown predicate: " << type;
  }
}
}  // namespace operations_research
