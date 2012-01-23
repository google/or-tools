// Copyright 2010-2012 Google
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

  // IntVar[] helper class.
  public static class IntVarArrayHelper
  {
    // All Different
    public static Constraint AllDifferent(this IntVar[] vars)
    {
      Solver solver = GetSolver(vars);
      return solver.MakeAllDifferent(vars);
    }
    // sum of all vars.
    public static IntExpr Sum(this IntVar[] vars)
    {
      Solver solver = GetSolver(vars);
      return solver.MakeSum(vars);
    }

    // scalar product
    public static IntExpr ScalProd(this IntVar[] vars, long[] coefs)
    {
      Solver solver = GetSolver(vars);
      return solver.MakeScalProd(vars, coefs);
    }

    // scalar product
    public static IntExpr ScalProd(this IntVar[] vars, int[] coefs)
    {
      Solver solver = GetSolver(vars);
      return solver.MakeScalProd(vars, coefs);
    }

    // get solver from array of integer variables
    private static Solver GetSolver(IntVar[] vars)
    {
      if (vars == null || vars.Length <= 0)
        throw new ArgumentException("Array <vars> cannot be null or empty");

      return vars[0].solver();
    }
    public static IntExpr Element(this IntVar[] array, IntExpr index) {
      return index.solver().MakeElement(array, index.Var());
    }

  }
}  // namespace Google.OrTools.ConstraintSolver