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

#ifndef OR_TOOLS_FLATZINC_SAT_CONSTRAINT_H_
#define OR_TOOLS_FLATZINC_SAT_CONSTRAINT_H_

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
class SatPropagator;

// Creates a special instance of CP constraint that connects to a sat solver.
// Ownership of the constraint belongs to the CP solver.
SatPropagator* MakeSatPropagator(Solver* solver);

// All the functions below add the constraint described by the function name to
// a SatPropagator. All the IntExpr or IntVar must refer to Boolean variables,
// if not the functions will return false.

bool AddBoolEq(SatPropagator* sat, IntExpr* left, IntExpr* right);

bool AddBoolLe(SatPropagator* sat, IntExpr* left, IntExpr* right);

bool AddBoolNot(SatPropagator* sat, IntExpr* left, IntExpr* right);

bool AddBoolAndArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                          IntExpr* target);

bool AddBoolOrArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                         IntExpr* target);

bool AddSumBoolArrayGreaterEqVar(SatPropagator* sat,
                                 const std::vector<IntVar*>& vars,
                                 IntExpr* target);

bool AddMaxBoolArrayLessEqVar(SatPropagator* sat,
                              const std::vector<IntVar*>& vars,
                              IntExpr* target);

bool AddBoolAndEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                     IntExpr* target);

bool AddBoolIsNEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                     IntExpr* target);

bool AddBoolIsLeVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target);

bool AddBoolOrEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target);

bool AddBoolIsEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target);

bool AddBoolOrArrayEqualTrue(SatPropagator* sat,
                             const std::vector<IntVar*>& vars);

bool AddBoolAndArrayEqualFalse(SatPropagator* sat,
                               const std::vector<IntVar*>& vars);

bool AddAtMostOne(SatPropagator* sat, const std::vector<IntVar*>& vars);

bool AddAtMostNMinusOne(SatPropagator* sat, const std::vector<IntVar*>& vars);

bool AddArrayXor(SatPropagator* sat, const std::vector<IntVar*>& vars);

bool AddIntEqReif(SatPropagator* sat, IntExpr* left, IntExpr* right,
                  IntExpr* target);
bool AddIntNeReif(SatPropagator* sat, IntExpr* left, IntExpr* right,
                  IntExpr* target);

bool AddSumInRange(SatPropagator* sat, const std::vector<IntVar*>& vars,
                   int64 range_min, int64 range_max);

}  // namespace operations_research
#endif  // OR_TOOLS_FLATZINC_SAT_CONSTRAINT_H_
