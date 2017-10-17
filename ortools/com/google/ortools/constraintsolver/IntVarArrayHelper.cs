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

// IntVar[] helper class.
public static class IntVarArrayHelper
{
  // All Different
  public static Constraint AllDifferent(this IntVar[] vars)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeAllDifferent(vars);
  }
  // Allowed assignment
  public static Constraint AllowedAssignments(this IntVar[] vars,
                                              IntTupleSet tuples)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeAllowedAssignments(vars, tuples);
  }
  // sum of all vars.
  public static IntExpr Sum(this IntVar[] vars)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeSum(vars);
  }
  // sum of all constraints.
  public static IntExpr Sum(this IConstraintWithStatus[] cts)
  {
    Solver solver = GetSolver(cts);
    IntVar[] vars = new IntVar[cts.Length];
    for (int i = 0; i < cts.Length; ++i)
    {
      vars[i] = cts[i].Var();
    }
    return solver.MakeSum(vars);
  }
  public static IntExpr Sum(this IntExpr[] exprs)
  {
    Solver solver = GetSolver(exprs);
    IntVar[] vars = new IntVar[exprs.Length];
    for (int i = 0; i < exprs.Length; ++i)
    {
      vars[i] = exprs[i].Var();
    }
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
  // get solver from array of integer expressions
  private static Solver GetSolver(IntExpr[] expressions)
  {
    if (expressions == null || expressions.Length <= 0)
      throw new ArgumentException("Array <expr> cannot be null or empty");

    return expressions[0].solver();
  }
  private static Solver GetSolver(IConstraintWithStatus[] cts)
  {
    if (cts == null || cts.Length <= 0)
      throw new ArgumentException("Array <cts> cannot be null or empty");

    return cts[0].solver();
  }
  public static IntExpr Element(this IntVar[] array, IntExpr index) {
    return index.solver().MakeElement(array, index.Var());
  }
  // min of all vars.
  public static IntExpr Min(this IntVar[] vars)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeMin(vars);
  }
  // min of all vars.
  public static IntExpr Max(this IntVar[] vars)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeMax(vars);
  }
  // count of all vars.
  public static Constraint Count(this IntVar[] vars, long value, long count)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeCount(vars, value, count);
  }
  // count of all vars.
  public static Constraint Count(this IntVar[] vars,
                                 long value,
                                 IntExpr count)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeCount(vars, value, count.Var());
  }
  public static Constraint Distribute(this IntVar[] vars,
                                      long[] values,
                                      IntVar[] cards)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeDistribute(vars, values, cards);
  }
  public static Constraint Distribute(this IntVar[] vars,
                                      int[] values,
                                      IntVar[] cards)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeDistribute(vars, values, cards);
  }
  public static Constraint Distribute(this IntVar[] vars,
                                      IntVar[] cards)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeDistribute(vars, cards);
  }
  public static Constraint Distribute(this IntVar[] vars,
                                      long card_min,
                                      long card_max,
                                      long card_size)
  {
    Solver solver = GetSolver(vars);
    return solver.MakeDistribute(vars, card_min, card_max, card_size);
  }
  public static Constraint Transition(this IntVar[] vars,
                                      IntTupleSet transitions,
                                      long initial_state,
                                      long[] final_states) {
    Solver solver = GetSolver(vars);
    return solver.MakeTransitionConstraint(vars,
                                           transitions,
                                           initial_state,
                                           final_states);
  }
  public static Constraint Transition(this IntVar[] vars,
                                      IntTupleSet transitions,
                                      long initial_state,
                                      int[] final_states) {
    Solver solver = GetSolver(vars);
    return solver.MakeTransitionConstraint(vars,
                                           transitions,
                                           initial_state,
                                           final_states);
  }

  // Matrix API
  public static IntVar[] Flatten(this IntVar[,] vars)
  {
    int rows = vars.GetLength(0);
    int cols = vars.GetLength(1);
    IntVar[] flat = new IntVar[cols * rows];
    for(int i = 0; i < rows; i++) {
      for(int j = 0; j < cols; j++) {
        flat[i * cols + j] = vars[i, j];
      }
    }
    return flat;
  }
}


// TODO(user): Try to move this code back to the .swig with @define macros.
public partial class IntVarVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<IntVar>
#endif
{
  // cast from C# IntVar array
  public static implicit operator IntVarVector(IntVar[] inVal) {
    var outVal= new IntVarVector();
    foreach (IntVar element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# IntVar array
  public static implicit operator IntVar[](IntVarVector inVal) {
    var outVal= new IntVar[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class SearchMonitorVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<SearchMonitor>
#endif
{
  // cast from C# SearchMonitor array
  public static implicit operator SearchMonitorVector(SearchMonitor[] inVal) {
    var outVal= new SearchMonitorVector();
    foreach (SearchMonitor element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# SearchMonitor array
  public static implicit operator SearchMonitor[](SearchMonitorVector inVal) {
    var outVal= new SearchMonitor[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class DecisionBuilderVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<DecisionBuilder>
#endif
{
  // cast from C# DecisionBuilder array
  public static implicit operator DecisionBuilderVector(DecisionBuilder[] inVal) {
    var outVal= new DecisionBuilderVector();
    foreach (DecisionBuilder element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# DecisionBuilder array
  public static implicit operator DecisionBuilder[](DecisionBuilderVector inVal) {
    var outVal= new DecisionBuilder[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class IntervalVarVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<IntervalVar>
#endif
{
  // cast from C# IntervalVar array
  public static implicit operator IntervalVarVector(IntervalVar[] inVal) {
    var outVal= new IntervalVarVector();
    foreach (IntervalVar element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# IntervalVar array
  public static implicit operator IntervalVar[](IntervalVarVector inVal) {
    var outVal= new IntervalVar[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class SequenceVarVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<SequenceVar>
#endif
{
  // cast from C# SequenceVar array
  public static implicit operator SequenceVarVector(SequenceVar[] inVal) {
    var outVal= new SequenceVarVector();
    foreach (SequenceVar element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# SequenceVar array
  public static implicit operator SequenceVar[](SequenceVarVector inVal) {
    var outVal= new SequenceVar[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class LocalSearchOperatorVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<LocalSearchOperator>
#endif
{
  // cast from C# LocalSearchOperator array
  public static implicit operator LocalSearchOperatorVector(LocalSearchOperator[] inVal) {
    var outVal= new LocalSearchOperatorVector();
    foreach (LocalSearchOperator element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# LocalSearchOperator array
  public static implicit operator LocalSearchOperator[](LocalSearchOperatorVector inVal) {
    var outVal= new LocalSearchOperator[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class LocalSearchFilterVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<LocalSearchFilter>
#endif
{
  // cast from C# LocalSearchFilter array
  public static implicit operator LocalSearchFilterVector(LocalSearchFilter[] inVal) {
    var outVal= new LocalSearchFilterVector();
    foreach (LocalSearchFilter element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# LocalSearchFilter array
  public static implicit operator LocalSearchFilter[](LocalSearchFilterVector inVal) {
    var outVal= new LocalSearchFilter[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class SymmetryBreakerVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<SymmetryBreaker>
#endif
{
  // cast from C# SymmetryBreaker array
  public static implicit operator SymmetryBreakerVector(SymmetryBreaker[] inVal) {
    var outVal= new SymmetryBreakerVector();
    foreach (SymmetryBreaker element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# SymmetryBreaker array
  public static implicit operator SymmetryBreaker[](SymmetryBreakerVector inVal) {
    var outVal= new SymmetryBreaker[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}
}  // namespace Google.OrTools.ConstraintSolver
