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

#ifndef OR_TOOLS_FLATZINC_FLATZINC_CONSTRAINTS_H_
#define OR_TOOLS_FLATZINC_FLATZINC_CONSTRAINTS_H_

#include <map>
#include <cassert>

#include "old_flatzinc/parser.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {
class SatPropagator;
bool AddBoolEq(SatPropagator* const sat, IntExpr* const left,
               IntExpr* const right);

bool AddBoolLe(SatPropagator* const sat, IntExpr* const left,
               IntExpr* const right);

bool AddBoolNot(SatPropagator* const sat, IntExpr* const left,
                IntExpr* const right);

bool AddBoolAndArrayEqVar(SatPropagator* const sat, const std::vector<IntVar*>& vars,
                          IntExpr* const target);

bool AddBoolOrArrayEqVar(SatPropagator* const sat, const std::vector<IntVar*>& vars,
                         IntExpr* const target);

bool AddSumBoolArrayGreaterEqVar(SatPropagator* const sat,
                                 const std::vector<IntVar*>& vars,
                                 IntExpr* const target);

bool AddSumBoolArrayLessEqKVar(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars,
                               IntExpr* const target);

bool AddBoolAndEqVar(SatPropagator* const sat, IntExpr* const left,
                     IntExpr* const right, IntExpr* const target);

bool AddBoolIsNEqVar(SatPropagator* const sat, IntExpr* const left,
                     IntExpr* const right, IntExpr* const target);

bool AddBoolIsLeVar(SatPropagator* const sat, IntExpr* const left,
                    IntExpr* const right, IntExpr* const target);

bool AddBoolOrEqVar(SatPropagator* const sat, IntExpr* const left,
                    IntExpr* const right, IntExpr* const target);

bool AddBoolIsEqVar(SatPropagator* const sat, IntExpr* const left,
                    IntExpr* const right, IntExpr* const target);

bool AddBoolOrArrayEqualTrue(SatPropagator* const sat,
                             const std::vector<IntVar*>& vars);

bool AddBoolAndArrayEqualFalse(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars);

bool AddAtMostOne(SatPropagator* const sat, const std::vector<IntVar*>& vars);

bool AddAtMostNMinusOne(SatPropagator* const sat, const std::vector<IntVar*>& vars);

bool AddArrayXor(SatPropagator* const sat, const std::vector<IntVar*>& vars);

bool DeclareVariable(SatPropagator* const sat, IntVar* const var);

Constraint* MakeStrongScalProdEquality(Solver* const solver,
                                       const std::vector<IntVar*>& variables,
                                       const std::vector<int64>& coefficients,
                                       int64 rhs);

Constraint* MakeIsBooleanSumInRange(Solver* const solver,
                                    const std::vector<IntVar*>& variables,
                                    int64 range_min, int64 range_max,
                                    IntVar* const target);

Constraint* MakeBooleanSumInRange(Solver* const solver,
                                  const std::vector<IntVar*>& variables,
                                  int64 range_min, int64 range_max);

Constraint* MakeBooleanSumOdd(Solver* const solver,
                              const std::vector<IntVar*>& variables);

Constraint* MakeVariableCumulative(Solver* const solver,
                                   const std::vector<IntVar*>& starts,
                                   const std::vector<IntVar*>& durations,
                                   const std::vector<IntVar*>& usages,
                                   IntVar* const capacity);

Constraint* MakeVariableOdd(Solver* const s, IntVar* const var);
Constraint* MakeVariableEven(Solver* const s, IntVar* const var);

void PostIsBooleanSumInRange(FlatZincModel* const model, CtSpec* const spec,
                             const std::vector<IntVar*>& variables, int64 range_min,
                             int64 range_max, IntVar* const target);

void PostBooleanSumInRange(FlatZincModel* const model, CtSpec* const spec,
                           const std::vector<IntVar*>& variables, int64 range_min,
                           int64 range_max);
}       // namespace operations_research
#endif  // OR_TOOLS_FLATZINC_FLATZINC_CONSTRAINTS_H_
