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

#ifndef OR_TOOLS_FLATZINC_SAT_CONSTRAINTS_H_
#define OR_TOOLS_FLATZINC_SAT_CONSTRAINTS_H_

#include "constraint_solver/constraint_solver.h"

namespace operations_research {
class SatPropagator;

SatPropagator* MakeSatPropagator(Solver* const solver);

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
}  // namespace operations_research
#endif  // OR_TOOLS_FLATZINC_SAT_CONSTRAINTS_H_
