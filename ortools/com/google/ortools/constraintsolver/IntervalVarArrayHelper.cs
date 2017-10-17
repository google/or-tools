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

namespace Google.OrTools.ConstraintSolver
{
  using System;
  using System.Collections.Generic;

  // IntervalVar[] helper class.
  public static class IntervalVarArrayHelper
  {
    // get solver from array of interval variables
    private static Solver GetSolver(IntervalVar[] vars)
    {
      if (vars == null || vars.Length <= 0)
        throw new ArgumentException("Array <vars> cannot be null or empty");

      return vars[0].solver();
    }
    public static DisjunctiveConstraint Disjunctive(this IntervalVar[] vars,
                                                    String name)
    {
      Solver solver = GetSolver(vars);
      return solver.MakeDisjunctiveConstraint(vars, name);
    }
    public static Constraint Cumulative(this IntervalVar[] vars,
                                        long[] demands,
                                        long capacity,
                                        String name)
    {
      Solver solver = GetSolver(vars);
      return solver.MakeCumulative(vars, demands, capacity, name);
    }
    public static Constraint Cumulative(this IntervalVar[] vars,
                                        int[] demands,
                                        long capacity,
                                        String name)
    {
      Solver solver = GetSolver(vars);
      return solver.MakeCumulative(vars, demands, capacity, name);
    }
  }
}  // namespace Google.OrTools.ConstraintSolver
