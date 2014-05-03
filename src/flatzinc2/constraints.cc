// Copyright 2010-2013 Google
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
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/hash.h"
#include "flatzinc2/flatzinc_constraints.h"
#include "flatzinc2/model.h"
#include "flatzinc2/search.h"
#include "flatzinc2/solver.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

DECLARE_bool(verbose_logging);

namespace operations_research {
void ExtractAllDifferentInt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(0));
  Constraint* const constraint = s->MakeAllDifferent(vars, vars.size() < 100);
  FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
  s->AddConstraint(constraint);
}

void ExtractAlldifferentExcept0(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(0));
  s->AddConstraint(s->MakeAllDifferentExcept(vars, 0));
}

void ExtractArrayBoolAnd(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<FzIntegerVariable*>& vars = ct->Arg(0).variables;
  const int size = vars.size();
  std::vector<IntVar*> variables;
  hash_set<IntExpr*> added;
  for (FzIntegerVariable* var : vars) {
    IntVar* const to_add = fzsolver->Extract(var)->Var();
    if (!ContainsKey(added, to_add) && to_add->Max() != 0) {
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
        // if (FLAGS_use_sat &&
        //     AddBoolAndArrayEqualFalse(model->Sat(), variables)) {
        //   VLOG(2) << "  - posted to sat";
        // } else {
          Constraint* const constraint =
              solver->MakeSumGreaterOrEqual(variables, 1);
          FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
          solver->AddConstraint(constraint);
        // }
      } else {
        Constraint* const constraint =
            solver->MakeSumEquality(variables, variables.size());
        FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
        solver->AddConstraint(constraint);
      }
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(1))->Var();
      // if (FLAGS_use_sat &&
      //     AddBoolAndArrayEqVar(model->Sat(), variables, boolvar)) {
      //   VLOG(2) << "  - posted to sat";
      // } else {
        Constraint* const constraint =
            solver->MakeMinEquality(variables, boolvar);
        FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
        solver->AddConstraint(constraint);
      // }
    }
  }
}

void ExtractArrayBoolOr(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<FzIntegerVariable*>& vars = ct->Arg(0).variables;
  const int size = vars.size();
  std::vector<IntVar*> variables;
  hash_set<IntExpr*> added;
  for (FzIntegerVariable* var : vars) {
    IntVar* const to_add = fzsolver->Extract(var)->Var();
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
        //   if (FLAGS_use_sat && AddBoolOrArrayEqualTrue(fzsolver->Sat(), variables)) {
        //     VLOG(2) << "  - posted to sat";
        //   } else {
        Constraint* const constraint =
            solver->MakeSumGreaterOrEqual(variables, 1);
        FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
        solver->AddConstraint(constraint);
        // }
      } else {
        Constraint* const constraint =
            solver->MakeSumEquality(variables, Zero());
        FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
        solver->AddConstraint(constraint);
      }
    } else {
      IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(1))->Var();
      // if (FLAGS_use_sat &&
      //     AddBoolOrArrayEqVar(fzsolver->Sat(), variables, boolvar)) {
      //   FZVLOG << "  - posted to sat" << FZENDL;
      // } else {
        Constraint* const constraint =
            solver->MakeMaxEquality(variables, boolvar);
        FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
        solver->AddConstraint(constraint);
      // }
    }
  }
}

void ExtractArrayBoolXor(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractArrayIntElement(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver =  fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const index = fzsolver->GetExpression(ct->Arg(0));
    const std::vector<int64>& values = ct->Arg(1).values;
    const int64 imin = std::max(index->Min(), 1LL);
    const int64 imax =
        std::min(index->Max(), static_cast<int64>(values.size()) + 1LL);
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
      Constraint* const ct =
          solver->MakeElementEquality(coefficients, shifted_index, target);
      FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
      solver->AddConstraint(ct);
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
    FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
    solver->AddConstraint(constraint);
  }
}

void ExtractArrayVarIntElement(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver =  fzsolver->solver();
  IntExpr* const index = fzsolver->GetExpression(ct->Arg(0));
  const std::vector<IntVar*> vars = fzsolver->GetVariableArray(ct->Arg(1));
  const int64 imin = std::max(index->Min(), 1LL);
  const int64 imax =
      std::min(index->Max(), static_cast<int64>(vars.size()) + 1LL);
  IntVar* const shifted_index = solver->MakeSum(index, -imin)->Var();
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
    FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
    solver->AddConstraint(constraint);
  }
}

void ExtractBoolAnd(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolClause(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolLeftImp(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolNot(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolOr(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolRightImp(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolXor(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCircuit(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountEq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  const std::vector<FzIntegerVariable*>& array_variables =
      ct->Arg(0).variables;
  const int size = array_variables.size();
  std::vector<IntVar*> tmp_sum;
  if (ct->Arg(1).HasOneValue()) {
    const int64 value = ct->Arg(1).Value();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          fzsolver->Extract(array_variables[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = fzsolver->GetExpression(ct->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          fzsolver->Extract(array_variables[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  if (ct->Arg(2).HasOneValue()) {
    const int64 count = ct->Arg(2).Value();
    PostBooleanSumInRange(solver, tmp_sum, count, count);
  } else {
    IntVar* const count = fzsolver->GetExpression(ct->Arg(2))->Var();
    if (count->Bound()) {
      const int64 fcount = count->Min();
      PostBooleanSumInRange(solver, tmp_sum, fcount, fcount);
    } else {
      Constraint* const constraint = solver->MakeSumEquality(tmp_sum, count);
      FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
      solver->AddConstraint(constraint);
    }
  }
}

void ExtractCountGeq(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountGt(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountLeq(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountLt(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountNeq(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractDiffn(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractFixedCumulative(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinality(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityClosed(FzSolver* fzsolver,
                                    FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityLowUp(FzSolver* fzsolver,
                                   FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityLowUpClosed(FzSolver* fzsolver,
                                         FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityOld(FzSolver* fzsolver,
                                 FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntAbs(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntDiv(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntEq(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeEquality(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      s->AddConstraint(s->MakeEquality(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeEquality(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left != right) {
        s->AddConstraint(s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntEqReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  if (ct->target_variable != nullptr) {
    CHECK_EQ(ct->target_variable, ct->Arg(2).Var());
    IntVar* const boolvar =
        ct->Arg(1).HasOneValue()
            ? solver->MakeIsEqualCstVar(left, ct->Arg(1).Value())
            : solver->MakeIsEqualVar(left,
                                     fzsolver->GetExpression(ct->Arg(1)));
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << boolvar->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, boolvar);
  } else {
    IntExpr* const right = fzsolver->GetExpression(ct->Arg(1))->Var();
    IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint = solver->MakeIsEqualCt(left, right, boolvar);
    FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
    solver->AddConstraint(constraint);
  }
}

void ExtractIntGe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeGreaterOrEqual(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      s->AddConstraint(s->MakeGreaterOrEqual(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeLessOrEqual(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left < right) {
        s->AddConstraint(s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntGeReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntGt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeGreater(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      s->AddConstraint(s->MakeGreater(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeLess(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left <= right) {
        s->AddConstraint(s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntGtReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntIn(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeLessOrEqual(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      s->AddConstraint(s->MakeLessOrEqual(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeGreaterOrEqual(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left > right) {
        s->AddConstraint(s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntLeReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
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
        LOG(FATAL) << "Wrong ct for " << ct->DebugString();
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
        if (fzvars[i]->domain.IsSingleton()) {
          constant += coefficients[i] * fzvars[i]->domain.values[0];
        } else if (fzvars[i] == ct->target_variable) {
          CHECK_EQ(-1, coefficients[i]);
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
    switch (size) {
      case 0: {
        constraint = rhs == 0 ? solver->MakeTrueConstraint()
                              : solver->MakeFalseConstraint();
        break;
      }
      case 1: {
        IntExpr* const e1 = fzsolver->Extract(fzvars[0]);
        const int64 c1 = coefficients[0];
        constraint = solver->MakeEquality(solver->MakeProd(e1, c1), rhs);
        break;
      }
      case 2: {
        IntExpr* const e1 = fzsolver->Extract(fzvars[0]);
        IntExpr* const e2 = fzsolver->Extract(fzvars[1]);
        const int64 c1 = coefficients[0];
        const int64 c2 = coefficients[1];
        if (c1 > 0) {
          if (c2 > 0) {
            constraint = solver->MakeEquality(
                solver->MakeProd(e1, c1),
                solver->MakeDifference(rhs, solver->MakeProd(e2, c2)));
          } else {
            constraint = solver->MakeEquality(
                solver->MakeProd(e1, c1),
                solver->MakeSum(solver->MakeProd(e2, -c2), rhs));
          }
        } else if (c2 > 0) {
          constraint = solver->MakeEquality(
              solver->MakeProd(e2, c2),
              solver->MakeSum(solver->MakeProd(e1, -c1), rhs));
        } else {
          constraint = solver->MakeEquality(
              solver->MakeProd(e1, -c1),
              solver->MakeDifference(-rhs, solver->MakeProd(e2, -c2)));
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
        if (ct->strong_propagation) {
          std::vector<IntVar*> variables;
          variables.push_back(e1->Var());
          variables.push_back(e2->Var());
          variables.push_back(e3->Var());
          constraint =
              MakeStrongScalProdEquality(solver, variables, coefficients, rhs);
        } else {
          if (c1 < 0 && c2 > 0 && c3 > 0) {
            constraint = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e2, c2),
                                solver->MakeProd(e3, c3)),
                solver->MakeSum(solver->MakeProd(e1, -c1), rhs));
          } else if (c1 > 0 && c2 < 0 && c3 > 0) {
            constraint = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, c1),
                                solver->MakeProd(e3, c3)),
                solver->MakeSum(solver->MakeProd(e2, -c2), rhs));
          } else if (c1 > 0 && c2 > 0 && c3 < 0) {
            constraint = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, c1),
                                solver->MakeProd(e2, c2)),
                solver->MakeSum(solver->MakeProd(e3, -c3), rhs));
          } else if (c1 < 0 && c2 < 0 && c3 > 0) {
            constraint = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, -c1),
                                solver->MakeProd(e2, -c2)),
                solver->MakeSum(solver->MakeProd(e3, c3), -rhs));
          } else {
            constraint = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, c1),
                                solver->MakeProd(e2, c2)),
                solver->MakeDifference(rhs, solver->MakeProd(e3, c3)));
          }
        }
        break;
      }
      default: {
        std::vector<int64> new_coefficients;
        std::vector<IntVar*> variables;
        for (int i = 0; i < size; ++i) {
          const int64 coef = coefficients[i];
          IntVar* const var = fzsolver->Extract(fzvars[i])->Var();
          if (coef != 0 && (var->Min() != 0 || var->Max() != 0)) {
            if (var->Bound()) {
              rhs -= var->Min() * coef;
            } else {
              new_coefficients.push_back(coef);
              variables.push_back(var);
            }
          }
        }
        if (AreAllBooleans(variables) && AreAllOnes(coefficients)) {
          PostBooleanSumInRange(solver, variables, rhs, rhs);
          return;
        } else {
          constraint =
              solver->MakeScalProdEquality(variables, new_coefficients, rhs);
        }
      }
    }
    FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
    solver->AddConstraint(constraint);
  }
}

void ExtractIntLinEqReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinGe(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinGeReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinLe(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinLeReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinNe(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinNeReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLt(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeLess(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      s->AddConstraint(s->MakeLess(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeGreater(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left >= right) {
        s->AddConstraint(s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntLtReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMax(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMin(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMinus(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMod(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntNe(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const s = fzsolver->solver();
  if (ct->Arg(0).type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeNonEquality(left, right));
    } else {
      const int64 right = ct->Arg(1).Value();
      s->AddConstraint(s->MakeNonEquality(left, right));
    }
  } else {
    const int64 left = ct->Arg(0).Value();
    if (ct->Arg(1).type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = fzsolver->GetExpression(ct->Arg(1));
      s->AddConstraint(s->MakeNonEquality(right, left));
    } else {
      const int64 right = ct->Arg(1).Value();
      if (left == right) {
        s->AddConstraint(s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntNeReif(FzSolver* fzsolver, FzConstraint* ct) {
  Solver* const solver = fzsolver->solver();
  IntExpr* const left = fzsolver->GetExpression(ct->Arg(0));
  if (ct->target_variable != nullptr) {
    CHECK_EQ(ct->target_variable, ct->Arg(2).Var());
    IntVar* const boolvar =
        ct->Arg(1).HasOneValue()
            ? solver->MakeIsDifferentCstVar(left, ct->Arg(1).Value())
            : solver->MakeIsDifferentVar(left,
                                         fzsolver->GetExpression(ct->Arg(1)));
    FZVLOG << "  - creating " << ct->target_variable->DebugString()
           << " := " << boolvar->DebugString() << FZENDL;
    fzsolver->SetExtracted(ct->target_variable, boolvar);
  } else {
    IntExpr* const right = fzsolver->GetExpression(ct->Arg(1))->Var();
    IntVar* const boolvar = fzsolver->GetExpression(ct->Arg(2))->Var();
    Constraint* const constraint =
        solver->MakeIsDifferentCt(left, right, boolvar);
    FZVLOG << "  - posted " << constraint->DebugString() << FZENDL;
    solver->AddConstraint(constraint);
  }
}

void ExtractIntNegate(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntPlus(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntTimes(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractInverse(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLessBool(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLessInt(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLesseqBool(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLesseqInt(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractMaximumInt(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractMinimumInt(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractNvalue(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractRegular(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSetIn(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSetInReif(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSlidingSum(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSort(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractTableBool(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractTableInt(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractTrueConstraint(FzSolver* fzsolver, FzConstraint* ct) {}

void ExtractVarCumulative(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractVariableCumulative(FzSolver* fzsolver, FzConstraint* ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void FzSolver::ExtractConstraint(FzConstraint* ct) {
  FZVLOG << "Extracting " << ct->DebugString() << std::endl;
  const std::string& type = ct->type;
  if (type == "all_different_int") {
    ExtractAllDifferentInt(this, ct);
  } else if (type == "alldifferent_except_0") {
    ExtractAlldifferentExcept0(this, ct);
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
  } else if (type == "bool_eq") {
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
    ExtractBoolLeftImp(this, ct);
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
    ExtractBoolRightImp(this, ct);
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
  } else if (type == "diffn") {
    ExtractDiffn(this, ct);
  } else if (type == "fixed_cumulative") {
    ExtractFixedCumulative(this, ct);
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
  } else if (type == "int_in") {
    ExtractIntIn(this, ct);
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
  } else if (type == "lex_less_bool") {
    ExtractLexLessBool(this, ct);
  } else if (type == "lex_less_int") {
    ExtractLexLessInt(this, ct);
  } else if (type == "lex_lesseq_bool") {
    ExtractLexLesseqBool(this, ct);
  } else if (type == "lex_lesseq_int") {
    ExtractLexLesseqInt(this, ct);
  } else if (type == "maximum_int") {
    ExtractMaximumInt(this, ct);
  } else if (type == "minimum_int") {
    ExtractMinimumInt(this, ct);
  } else if (type == "nvalue") {
    ExtractNvalue(this, ct);
  } else if (type == "regular") {
    ExtractRegular(this, ct);
  } else if (type == "set_in") {
    ExtractSetIn(this, ct);
  } else if (type == "set_in_reif") {
    ExtractSetInReif(this, ct);
  } else if (type == "sliding_sum") {
    ExtractSlidingSum(this, ct);
  } else if (type == "sort") {
    ExtractSort(this, ct);
  } else if (type == "table_bool") {
    ExtractTableBool(this, ct);
  } else if (type == "table_int") {
    ExtractTableInt(this, ct);
  } else if (type == "true_constraint") {
    ExtractTrueConstraint(this, ct);
  } else if (type == "var_cumulative") {
    ExtractVarCumulative(this, ct);
  } else if (type == "variable_cumulative") {
    ExtractVariableCumulative(this, ct);
  }
}
}  // namespace operations_research
