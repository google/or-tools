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

namespace Google.OrTools.ConstraintSolver {
using System;
using System.Collections.Generic;

public partial class Solver : IDisposable {
  public IntVar[] MakeIntVarArray(int count, long min, long max) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(min, max);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long min, long max, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(min, max, var_name);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long[] values) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(values);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long[] values, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(values, var_name);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, int[] values) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(values);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, int[] values, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(values, var_name);
    }
    return array;
  }

  public IntVar[] MakeBoolVarArray(int count) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeBoolVar();
    }
    return array;
  }

  public IntVar[] MakeBoolVarArray(int count, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeBoolVar(var_name);
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, long min, long max) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(min, max);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    long min, long max, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(min, max, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, long[] values) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(values);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    long[] values, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(values, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, int[] values) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(values);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    int[] values, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(values, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeBoolVarMatrix(int rows, int cols) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeBoolVar();
      }
    }
    return array;
  }

  public IntVar[,] MakeBoolVarMatrix(int rows, int cols, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeBoolVar(var_name);
      }
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         bool optional) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              "");
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              name + i);
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long[] start_min,
                                                         long[] start_max,
                                                         long[] duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min[i],
                                              start_max[i],
                                              duration[i],
                                              optional,
                                              name + i);
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         int[] start_min,
                                                         int[] start_max,
                                                         int[] duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min[i],
                                              start_max[i],
                                              duration[i],
                                              optional,
                                              name + i);
    }
    return array;
  }
  public IntervalVar[] MakeFixedDurationIntervalVarArray(IntVar[] starts,
                                                         int[] durations,
                                                         string name) {
    int count = starts.Length;
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(starts[i],
                                              durations[i],
                                              name + i);
    }
    return array;
  }
  public IntervalVar[] MakeFixedDurationIntervalVarArray(IntVar[] starts,
                                                         long[] durations,
                                                         string name) {
    int count = starts.Length;
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(starts[i],
                                              durations[i],
                                              name + i);
    }
    return array;
  }
  public void NewSearch(DecisionBuilder db) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    NewSearchAux(db);
  }

  public void NewSearch(DecisionBuilder db, SearchMonitor sm1) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    NewSearchAux(db, sm1);
  }


  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    NewSearchAux(db, sm1, sm2);
  }

  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2,
                        SearchMonitor sm3) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    pinned_search_monitors_.Add(sm3);
    NewSearchAux(db, sm1, sm2, sm3);
  }

  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2,
                        SearchMonitor sm3,
                        SearchMonitor sm4) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    pinned_search_monitors_.Add(sm3);
    pinned_search_monitors_.Add(sm4);
    NewSearchAux(db, sm1, sm2, sm3, sm4);
  }

  public void NewSearch(DecisionBuilder db, SearchMonitor[] monitors) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.AddRange(monitors);
    NewSearchAux(db, monitors);
  }

  public void EndSearch() {
    pinned_decision_builder_ = null;
    pinned_search_monitors_.Clear();
    EndSearchAux();
  }

  private System.Collections.Generic.List<SearchMonitor> pinned_search_monitors_
      = new System.Collections.Generic.List<SearchMonitor>();
  private DecisionBuilder pinned_decision_builder_;
}

public partial class IntExpr : PropagationBaseObject {
  public static IntExpr operator+(IntExpr a, IntExpr b) {
    return a.solver().MakeSum(a, b);
  }
  public static IntExpr operator+(IntExpr a, long v) {
    return a.solver().MakeSum(a, v);
  }
  public static IntExpr operator+(long v, IntExpr a) {
    return a.solver().MakeSum(a, v);
  }
  public static IntExpr operator-(IntExpr a, IntExpr b) {
    return a.solver().MakeDifference(a, b);
  }
  public static IntExpr operator-(IntExpr a, long v) {
    return a.solver().MakeSum(a, -v);
  }
  public static IntExpr operator-(long v, IntExpr a) {
    return a.solver().MakeDifference(v, a);
  }
  public static IntExpr operator*(IntExpr a, IntExpr b) {
    return a.solver().MakeProd(a, b);
  }
  public static IntExpr operator*(IntExpr a, long v) {
    return a.solver().MakeProd(a, v);
  }
  public static IntExpr operator*(long v, IntExpr a) {
    return a.solver().MakeProd(a, v);
  }
  public static IntExpr operator/(IntExpr a, long v) {
    return a.solver().MakeDiv(a, v);
  }
  public static IntExpr operator%(IntExpr a, long v) {
    return a.solver().MakeModulo(a, v);
  }
  public static IntExpr operator-(IntExpr a) {
    return a.solver().MakeOpposite(a);
  }
  public IntExpr Abs() {
    return this.solver().MakeAbs(this);
  }
  public IntExpr Square() {
    return this.solver().MakeSquare(this);
  }
  public static IntExprEquality operator ==(IntExpr a, IntExpr b) {
    return new IntExprEquality(a, b, true);
  }
  public static IntExprEquality operator !=(IntExpr a, IntExpr b) {
    return new IntExprEquality(a, b, false);
  }
  public static WrappedConstraint operator ==(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeEquality(a, v));
  }
  public static WrappedConstraint operator !=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator >=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a, v));
  }
  public static WrappedConstraint operator >(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeGreater(a, v));
  }
  public static WrappedConstraint operator <=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a, v));
  }
  public static WrappedConstraint operator <(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeLess(a, v));
  }
  public static WrappedConstraint operator >=(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator >(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <=(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), b.Var()));
  }
}

public partial class Constraint : PropagationBaseObject, IConstraintWithStatus {
  public static implicit operator IntVar(Constraint eq)
  {
    return eq.Var();
  }

  public static implicit operator IntExpr(Constraint eq)
  {
    return eq.Var();
  }
  public static IntExpr operator+(Constraint a, Constraint b) {
    return a.solver().MakeSum(a.Var(), b.Var());
  }
  public static IntExpr operator+(Constraint a, long v) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator+(long v, Constraint a) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator-(Constraint a, Constraint b) {
    return a.solver().MakeDifference(a.Var(), b.Var());
  }
  public static IntExpr operator-(Constraint a, long v) {
    return a.solver().MakeSum(a.Var(), -v);
  }
  public static IntExpr operator-(long v, Constraint a) {
    return a.solver().MakeDifference(v, a.Var());
  }
  public static IntExpr operator*(Constraint a, Constraint b) {
    return a.solver().MakeProd(a.Var(), b.Var());
  }
  public static IntExpr operator*(Constraint a, long v) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator*(long v, Constraint a) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator/(Constraint a, long v) {
    return a.solver().MakeDiv(a.Var(), v);
  }
  public static IntExpr operator-(Constraint a) {
    return a.solver().MakeOpposite(a.Var());
  }
  public IntExpr Abs() {
    return this.solver().MakeAbs(this.Var());
  }
  public IntExpr Square() {
    return this.solver().MakeSquare(this.Var());
  }
  public static WrappedConstraint operator ==(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator ==(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator >=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >=(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator >(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <=(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), b.Var()));
  }
  public static ConstraintEquality operator ==(Constraint a, Constraint b) {
    return new ConstraintEquality(a, b, true);
  }
  public static ConstraintEquality operator !=(Constraint a, Constraint b) {
    return new ConstraintEquality(a, b, false);
  }
}
}  // namespace Google.OrTools.ConstraintSolver
