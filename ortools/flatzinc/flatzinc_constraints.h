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

#ifndef OR_TOOLS_FLATZINC_FLATZINC_CONSTRAINTS_H_
#define OR_TOOLS_FLATZINC_FLATZINC_CONSTRAINTS_H_

// Additional constraints included in the minizinc specifications, but not
// general enough to be in the CP library.

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

// This function will precompute all valid combinations of variables values that
// satisfies  Sum(variables[i] * coefficients[i]) == rhs, and it will create
// an AllowedAssignment constraint to enforce it.
Constraint* MakeStrongScalProdEquality(Solver* const solver,
                                       const std::vector<IntVar*>& variables,
                                       const std::vector<int64>& coefficients,
                                       int64 rhs);

// Creates a constraints that represents:
//   target == (sum(variables) in [range_min..range_max]).
Constraint* MakeIsBooleanSumInRange(Solver* const solver,
                                    const std::vector<IntVar*>& variables,
                                    int64 range_min, int64 range_max,
                                    IntVar* const target);

// Creates the constraint sum(variables) in [range_min..range_max].
Constraint* MakeBooleanSumInRange(Solver* const solver,
                                  const std::vector<IntVar*>& variables,
                                  int64 range_min, int64 range_max);

// Creates the constraint sum(variables) is odd.
Constraint* MakeBooleanSumOdd(Solver* const solver,
                              const std::vector<IntVar*>& variables);

// Creates a constraint var is odd.
Constraint* MakeVariableOdd(Solver* const s, IntVar* const var);

// Creates a constraint var is even.
Constraint* MakeVariableEven(Solver* const s, IntVar* const var);

// Creates a constraint var % mod == residual.
Constraint* MakeFixedModulo(Solver* const s, IntVar* const var,
                            IntVar* const mod, int64 residual);

// Creates a performed interval variable with the given start and duration
// variables.
IntervalVar* MakePerformedIntervalVar(Solver* const solver, IntVar* const start,
                                      IntVar* const duration, const std::string& n);

// Creates a n-dimensional constraints that enforces that k boxes (n dimension)
// do not overlap in space.
// The origin of box i is (x[i][0], ..., x[i][n - 1]).
// The dimension of the box i in dimension j is dx[i][j].
Constraint* MakeKDiffn(Solver* solver,
                       const std::vector<std::vector<IntVar*>>& x,
                       const std::vector<std::vector<IntVar*>>& dx,
                       bool strict);
}  // namespace operations_research
#endif  // OR_TOOLS_FLATZINC_FLATZINC_CONSTRAINTS_H_
