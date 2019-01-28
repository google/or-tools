// Copyright 2010-2018 Google LLC
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

using System;
using Google.OrTools.ConstraintSolver;

public class CsTestCpOperator
{
  // TODO(user): Add proper tests.

  static int error_count_ = 0;

  static void Check(bool test, String message)
  {
    if (!test)
    {
      Console.WriteLine("Error: " + message);
      error_count_++;
    }
  }

  static void CheckLongEq(long v1, long v2, String message)
  {
    if (v1 != v2)
    {
      Console.WriteLine("Error: " + v1 + " != " + v2 + " " + message);
      error_count_++;
    }
  }

  static void CheckIntRange(IntVar var, long vmin, long vmax) {
    if (var.Min() != vmin) {
      Console.WriteLine("Error: " + var.ToString() +
                        " does not have the expected min : " + vmin);
      error_count_++;
    }
    if (var.Max() != vmax) {
      Console.WriteLine("Error: " + var.ToString() +
                        " does not have the expected max : " + vmax);
      error_count_++;
    }
  }

  static void WrappedConstraintWithExprTest()
  {
    Console.WriteLine("TestWrappedConstraintWithExpr");
    Solver solver = new Solver("test");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    IntVar y = solver.MakeIntVar(0, 10, "y");
    IntExpr e2a = (x == 1) + 1;
    Console.WriteLine(e2a.ToString());
    IntExpr e2b = 1 + (x == 1);
    Console.WriteLine(e2b.ToString());
    IntExpr e2c = (x == 1) - 1;
    Console.WriteLine(e2c.ToString());
    IntExpr e2d = 1 - (x == 1);
    Console.WriteLine(e2d.ToString());
    IntExpr e2e = (x == 1) * 2;
    Console.WriteLine(e2e.ToString());
    IntExpr e2f = 2 * (x == 1);
    Console.WriteLine(e2f.ToString());
    IntExpr e3a = (x == 1) + y;
    Console.WriteLine(e3a.ToString());
    IntExpr e3b = y + (x == 1);
    Console.WriteLine(e3b.ToString());
    IntExpr e3c = (x == 1) - y;
    Console.WriteLine(e3c.ToString());
    IntExpr e3d = y - (x == 1);
    Console.WriteLine(e3d.ToString());
    IntExpr e3e = (x == 1) * y;
    Console.WriteLine(e3e.ToString());
    IntExpr e3f = y * (x == 1);
    Console.WriteLine(e3f.ToString());
    IntExpr e4 = -(x == 1);
    Console.WriteLine(e4.ToString());
    IntExpr e5 = (x == 1) / 4;
    Console.WriteLine(e5.ToString());
    IntExpr e6 = (x == 1).Abs();
    Console.WriteLine(e6.ToString());
    IntExpr e7 = (x == 1).Square();
    Console.WriteLine(e7.ToString());
    Constraint c8a = (x == 1) == 1;
    Console.WriteLine(c8a.ToString());
    Constraint c8b = 1 == (x == 1);
    Console.WriteLine(c8b.ToString());
    Constraint c8c = (x == 1) != 1;
    Console.WriteLine(c8c.ToString());
    Constraint c8d = 1 != (x == 1);
    Console.WriteLine(c8d.ToString());
    Constraint c8e = (x == 1) >= 1;
    Console.WriteLine(c8e.ToString());
    Constraint c8f = 1 >= (x == 1);
    Console.WriteLine(c8f.ToString());
    Constraint c8g = (x == 1) > 1;
    Console.WriteLine(c8g.ToString());
    Constraint c8h = 1 > (x == 1);
    Console.WriteLine(c8h.ToString());
    Constraint c8i = (x == 1) <= 1;
    Console.WriteLine(c8i.ToString());
    Constraint c8j = 1 <= (x == 1);
    Console.WriteLine(c8j.ToString());
    Constraint c8k = (x == 1) < 1;
    Console.WriteLine(c8k.ToString());
    Constraint c8l = 1 < (x == 1);
    Console.WriteLine(c8l.ToString());
    Constraint c9a = (x == 1) == y;
    Console.WriteLine(c9a.ToString());
    Constraint c9b = y == (x == 1);
    Console.WriteLine(c9b.ToString());
    Constraint c9c = (x == 1) != y;
    Console.WriteLine(c9c.ToString());
    Constraint c9d = y != (x == 1);
    Console.WriteLine(c9d.ToString());
    Constraint c9e = (x == 1) >= y;
    Console.WriteLine(c9e.ToString());
    Constraint c9f = y >= (x == 1);
    Console.WriteLine(c9f.ToString());
    Constraint c9g = (x == 1) > y;
    Console.WriteLine(c9g.ToString());
    Constraint c9h = y > (x == 1);
    Console.WriteLine(c9h.ToString());
    Constraint c9i = (x == 1) <= y;
    Console.WriteLine(c9i.ToString());
    Constraint c9j = y <= (x == 1);
    Console.WriteLine(c9j.ToString());
    Constraint c9k = (x == 1) < y;
    Console.WriteLine(c9k.ToString());
    Constraint c9l = y < (x == 1);
    Console.WriteLine(c9l.ToString());
    Constraint c10a = (x == 1) == (y == 2);
    Console.WriteLine(c10a.ToString());
    Constraint c10c = (x == 1) != (y == 2);
    Console.WriteLine(c10c.ToString());
    Constraint c10e = (x == 1) >= (y == 2);
    Console.WriteLine(c10e.ToString());
    Constraint c10g = (x == 1) > (y == 2);
    Console.WriteLine(c10g.ToString());
    Constraint c10i = (x == 1) <= (y == 2);
    Console.WriteLine(c10i.ToString());
    Constraint c10k = (x == 1) < (y == 2);
    Console.WriteLine(c10k.ToString());
  }

  static void BaseEqualityWithExprTest()
  {
    Console.WriteLine("TestBaseEqualityWithExpr");
    Solver solver = new Solver("test");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    IntVar y = solver.MakeIntVar(0, 10, "y");
    IntExpr e2a = (x >= 1) + 1;
    Console.WriteLine(e2a.ToString());
    IntExpr e2b = 1 + (x >= 1);
    Console.WriteLine(e2b.ToString());
    IntExpr e2c = (x >= 1) - 1;
    Console.WriteLine(e2c.ToString());
    IntExpr e2d = 1 - (x >= 1);
    Console.WriteLine(e2d.ToString());
    IntExpr e2e = (x >= 1) * 2;
    Console.WriteLine(e2e.ToString());
    IntExpr e2f = 2 * (x >= 1);
    Console.WriteLine(e2f.ToString());
    IntExpr e3a = (x >= 1) + y;
    Console.WriteLine(e3a.ToString());
    IntExpr e3b = y + (x >= 1);
    Console.WriteLine(e3b.ToString());
    IntExpr e3c = (x >= 1) - y;
    Console.WriteLine(e3c.ToString());
    IntExpr e3d = y - (x >= 1);
    Console.WriteLine(e3d.ToString());
    IntExpr e3e = (x >= 1) * y;
    Console.WriteLine(e3e.ToString());
    IntExpr e3f = y * (x >= 1);
    Console.WriteLine(e3f.ToString());
    IntExpr e4 = -(x >= 1);
    Console.WriteLine(e4.ToString());
    IntExpr e5 = (x >= 1) / 4;
    Console.WriteLine(e5.ToString());
    IntExpr e6 = (x >= 1).Abs();
    Console.WriteLine(e6.ToString());
    IntExpr e7 = (x >= 1).Square();
    Console.WriteLine(e7.ToString());
    Constraint c8a = (x >= 1) >= 1;
    Console.WriteLine(c8a.ToString());
    Constraint c8b = 1 >= (x >= 1);
    Console.WriteLine(c8b.ToString());
    Constraint c8c = (x >= 1) != 1;
    Console.WriteLine(c8c.ToString());
    Constraint c8d = 1 != (x >= 1);
    Console.WriteLine(c8d.ToString());
    Constraint c8e = (x >= 1) >= 1;
    Console.WriteLine(c8e.ToString());
    Constraint c8f = 1 >= (x >= 1);
    Console.WriteLine(c8f.ToString());
    Constraint c8g = (x >= 1) > 1;
    Console.WriteLine(c8g.ToString());
    Constraint c8h = 1 > (x >= 1);
    Console.WriteLine(c8h.ToString());
    Constraint c8i = (x >= 1) <= 1;
    Console.WriteLine(c8i.ToString());
    Constraint c8j = 1 <= (x >= 1);
    Console.WriteLine(c8j.ToString());
    Constraint c8k = (x >= 1) < 1;
    Console.WriteLine(c8k.ToString());
    Constraint c8l = 1 < (x >= 1);
    Console.WriteLine(c8l.ToString());
    Constraint c9a = (x >= 1) >= y;
    Console.WriteLine(c9a.ToString());
    Constraint c9b = y >= (x >= 1);
    Console.WriteLine(c9b.ToString());
    Constraint c9c = (x >= 1) != y;
    Console.WriteLine(c9c.ToString());
    Constraint c9d = y != (x >= 1);
    Console.WriteLine(c9d.ToString());
    Constraint c9e = (x >= 1) >= y;
    Console.WriteLine(c9e.ToString());
    Constraint c9f = y >= (x >= 1);
    Console.WriteLine(c9f.ToString());
    Constraint c9g = (x >= 1) > y;
    Console.WriteLine(c9g.ToString());
    Constraint c9h = y > (x >= 1);
    Console.WriteLine(c9h.ToString());
    Constraint c9i = (x >= 1) <= y;
    Console.WriteLine(c9i.ToString());
    Constraint c9j = y <= (x >= 1);
    Console.WriteLine(c9j.ToString());
    Constraint c9k = (x >= 1) < y;
    Console.WriteLine(c9k.ToString());
    Constraint c9l = y < (x >= 1);
    Console.WriteLine(c9l.ToString());
    Constraint c10a = (x >= 1) >= (y >= 2);
    Console.WriteLine(c10a.ToString());
    Constraint c10c = (x >= 1) != (y >= 2);
    Console.WriteLine(c10c.ToString());
    Constraint c10e = (x >= 1) >= (y >= 2);
    Console.WriteLine(c10e.ToString());
    Constraint c10g = (x >= 1) > (y >= 2);
    Console.WriteLine(c10g.ToString());
    Constraint c10i = (x >= 1) <= (y >= 2);
    Console.WriteLine(c10i.ToString());
    Constraint c10k = (x >= 1) < (y >= 2);
    Console.WriteLine(c10k.ToString());
  }

  static void DowncastTest()
  {
    Solver solver = new Solver("TestDowncast");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    IntExpr e = x + 1;
    IntVar y = e.Var();
    Console.WriteLine(y.ToString());
  }

  static void SequenceTest()
  {
    Solver solver = new Solver("TestSequence");
    IntervalVar[] intervals =
        solver.MakeFixedDurationIntervalVarArray(10, 0, 10, 5, false, "task");
    DisjunctiveConstraint disjunctive = intervals.Disjunctive("Sequence");
    SequenceVar var = disjunctive.SequenceVar();
    Assignment ass = solver.MakeAssignment();
    ass.Add(var);
    ass.SetForwardSequence(var, new int[] { 1, 3, 5 });
    int[] seq = ass.ForwardSequence(var);
    Console.WriteLine(seq.Length);
  }

  // A simple demon that simply sets the maximum of a fixed IntVar to 10 when
  // it's being called.
  class SetMaxDemon : NetDemon {
    public SetMaxDemon(IntVar x) {
      x_ = x;
    }

    public override void Run(Solver s) {
      x_.SetMax(10);
    }

    private IntVar x_;
  }

  static void DemonTest() {
    Solver solver = new Solver("DemonTest");
    IntVar x = solver.MakeIntVar(new int[] {2, 4, -1, 6, 11, 10}, "x");
    NetDemon demon = new SetMaxDemon(x);
    CheckLongEq(x.Max(), 11, "Bad test setup");
    demon.Run(solver);
    CheckLongEq(x.Max(), 10, "The demon did not run.");
  }

  // This constraint has a single target variable x. It enforces x >= 5 upon
  // InitialPropagate() and invokes the SetMaxDemon when x changes its range.
  class SetMinAndMaxConstraint : NetConstraint {
    public SetMinAndMaxConstraint(Solver solver, IntVar x) : base(solver) {
      x_ = x;
    }

    public override void Post() {
      // Always store the demon in the constraint to avoid it being reclaimed
      // by the GC.
      demon_ = new SetMaxDemon(x_);
      x_.WhenBound(demon_);
    }

    public override void InitialPropagate() {
      x_.SetMin(5);
    }

    private IntVar x_;
    private Demon demon_;
  }

  static void ConstraintTest() {
    Solver solver = new Solver("TestConstraint");
    IntVar x = solver.MakeIntVar(new int[] {2, 4, -1, 6, 11, 10}, "x");
    Constraint ct = new SetMinAndMaxConstraint(solver, x);
    solver.Add(ct);
    DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MIN_VALUE);
    solver.NewSearch(db);
    CheckIntRange(x, -1, 11);
    Check(solver.NextSolution(), "NextSolution() failed");
    CheckIntRange(x, 6, 6);
    Check(solver.NextSolution(), "NextSolution() failed");
    CheckIntRange(x, 10, 10);
    Check(!solver.NextSolution(), "Not expecting a third solution");
    solver.EndSearch();
  }
  // // This constraint has a single target variable x. It enforces x >= 5,
  // but only at the leaf of the search tree: it doesn't change x's bounds,
  // and simply fails if x is bound and is < 5.
  class DumbGreaterOrEqualToFive : NetConstraint {
    public DumbGreaterOrEqualToFive(Solver solver, IntVar x) : base(solver) {
      x_ = x;
    }

    public override void Post() {
      demon_ = solver().MakeConstraintInitialPropagateCallback(this);
      x_.WhenRange(demon_);
    }

    public override void InitialPropagate() {
      if (x_.Bound()) {
        if (x_.Value() < 5) {
          solver().Fail();
        }
      }
    }

    private IntVar x_;
    private Demon demon_;
  }

  static void FailingConstraintTest() {
    Solver solver = new Solver("TestConstraint");
    IntVar x = solver.MakeIntVar(new int[] {2, 4, -1, 6, 11, 10}, "x");
    Constraint ct = new DumbGreaterOrEqualToFive(solver, x);
    solver.Add(ct);
    DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MIN_VALUE);
    solver.NewSearch(db);
    Check(solver.NextSolution(), "NextSolution failed");
    CheckLongEq(6, x.Min(), "Min not set");
    solver.EndSearch();
  }

  static void DomainIteratorTest() {
    Solver solver = new Solver("TestConstraint");
    IntVar x = solver.MakeIntVar(new int[] {2, 4, -1, 6, 11, 10}, "x");
    int count = 0;
    foreach (long value in x.GetDomain()) {
      count++;
    }
    CheckLongEq(count, (long)x.Size(),
                "GetDomain() iterated on an unexpected number of values.");
  }

  class CountHoles : NetDemon {
    public CountHoles(IntVar x) {
      x_ = x;
      count_ = 0;
    }

    public override void Run(Solver s) {
      foreach (long removed in x_.GetHoles()) {
        count_++;
      }
    }

    public int count() {
      return count_;
    }

    private IntVar x_;
    private int count_;
  }

  class RemoveThreeValues : NetConstraint {
    public RemoveThreeValues(Solver solver, IntVar x) : base(solver) {
      x_ = x;
    }

    public override void Post() {
      demon_ = new CountHoles(x_);
      x_.WhenDomain(demon_);
    }

    public override void InitialPropagate() {
      x_.RemoveValues(new long[] {3, 5, 7});
    }

    public int count() {
      return demon_.count();
    }

    private IntVar x_;
    private CountHoles demon_;
  }

  static void HoleIteratorTest() {
    Solver solver = new Solver("TestConstraint");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    RemoveThreeValues ct = new RemoveThreeValues(solver, x);
    solver.Add(ct);
    DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MIN_VALUE);
    solver.Solve(db);
    CheckLongEq(3, ct.count(), "Something went wrong, either in the " +
                "GetHoles() iterator, or the WhenDomain() demon invocation.");
  }

  static void Main() {
    ConstructorsTest();
    ConstraintWithExprTest();
    WrappedConstraintWithExprTest();
    BaseEqualityWithExprTest();
    DowncastTest();
    SequenceTest();
    DemonTest();
    ConstraintTest();
    FailingConstraintTest();
    DomainIteratorTest();
    HoleIteratorTest();
    if (error_count_ != 0) {
      Console.WriteLine("Found " + error_count_ + " errors.");
      Environment.Exit(1);
    }
  }
}
