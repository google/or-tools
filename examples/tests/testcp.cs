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

  static void ConstructorsTest()
  {
    Console.WriteLine("TestConstructors");
    Solver solver = new Solver("test");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    Constraint c1 = x == 2;
    Console.WriteLine(c1.ToString());
    Constraint c2 = x >= 2;
    Console.WriteLine(c2.ToString());
    Constraint c3 = x > 2;
    Console.WriteLine(c3.ToString());
    Constraint c4 = x <= 2;
    Console.WriteLine(c4.ToString());
    Constraint c5 = x < 2;
    Console.WriteLine(c5.ToString());
    Constraint c6 = x != 2;
    Console.WriteLine(c6.ToString());
  }

  static void ConstraintWithExprTest()
  {
    Console.WriteLine("TestConstraintWithExpr");
    Solver solver = new Solver("test");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    IntVar y = solver.MakeIntVar(0, 10, "y");
    Constraint c1 = x == 2;
    Constraint c2 = y == 3;
    IntExpr e2a = c1 + 1;
    Console.WriteLine(e2a.ToString());
    IntExpr e2b = 1 + c1;
    Console.WriteLine(e2b.ToString());
    IntExpr e2c = c1 - 1;
    Console.WriteLine(e2c.ToString());
    IntExpr e2d = 1 - c1;
    Console.WriteLine(e2d.ToString());
    IntExpr e2e = c1 * 2;
    Console.WriteLine(e2e.ToString());
    IntExpr e2f = 2 * c1;
    Console.WriteLine(e2f.ToString());
    IntExpr e3a = c1 + y;
    Console.WriteLine(e3a.ToString());
    IntExpr e3b = y + c1;
    Console.WriteLine(e3b.ToString());
    IntExpr e3c = c1 - y;
    Console.WriteLine(e3c.ToString());
    IntExpr e3d = y - c1;
    Console.WriteLine(e3d.ToString());
    IntExpr e3e = c1 * y;
    Console.WriteLine(e3e.ToString());
    IntExpr e3f = y * c1;
    Console.WriteLine(e3f.ToString());
    IntExpr e4 = -c1;
    Console.WriteLine(e4.ToString());
    IntExpr e5 = c1 / 4;
    Console.WriteLine(e5.ToString());
    IntExpr e6 = c1.Abs();
    Console.WriteLine(e6.ToString());
    IntExpr e7 = c1.Square();
    Console.WriteLine(e7.ToString());
    Constraint c8a = c1 == 1;
    Console.WriteLine(c8a.ToString());
    Constraint c8b = 1 == c1;
    Console.WriteLine(c8b.ToString());
    Constraint c8c = c1 != 1;
    Console.WriteLine(c8c.ToString());
    Constraint c8d = 1 != c1;
    Console.WriteLine(c8d.ToString());
    Constraint c8e = c1 >= 1;
    Console.WriteLine(c8e.ToString());
    Constraint c8f = 1 >= c1;
    Console.WriteLine(c8f.ToString());
    Constraint c8g = c1 > 1;
    Console.WriteLine(c8g.ToString());
    Constraint c8h = 1 > c1;
    Console.WriteLine(c8h.ToString());
    Constraint c8i = c1 <= 1;
    Console.WriteLine(c8i.ToString());
    Constraint c8j = 1 <= c1;
    Console.WriteLine(c8j.ToString());
    Constraint c8k = c1 < 1;
    Console.WriteLine(c8k.ToString());
    Constraint c8l = 1 < c1;
    Console.WriteLine(c8l.ToString());
    Constraint c9a = c1 == y;
    Console.WriteLine(c9a.ToString());
    Constraint c9b = y == c1;
    Console.WriteLine(c9b.ToString());
    Constraint c9c = c1 != y;
    Console.WriteLine(c9c.ToString());
    Constraint c9d = y != c1;
    Console.WriteLine(c9d.ToString());
    Constraint c9e = c1 >= y;
    Console.WriteLine(c9e.ToString());
    Constraint c9f = y >= c1;
    Console.WriteLine(c9f.ToString());
    Constraint c9g = c1 > y;
    Console.WriteLine(c9g.ToString());
    Constraint c9h = y > c1;
    Console.WriteLine(c9h.ToString());
    Constraint c9i = c1 <= y;
    Console.WriteLine(c9i.ToString());
    Constraint c9j = y <= c1;
    Console.WriteLine(c9j.ToString());
    Constraint c9k = c1 < y;
    Console.WriteLine(c9k.ToString());
    Constraint c9l = y < c1;
    Console.WriteLine(c9l.ToString());
    Constraint c10a = c1 == c2;
    Console.WriteLine(c10a.ToString());
    Constraint c10c = c1 != c2;
    Console.WriteLine(c10c.ToString());
    Constraint c10e = c1 >= c2;
    Console.WriteLine(c10e.ToString());
    Constraint c10g = c1 > c2;
    Console.WriteLine(c10g.ToString());
    Constraint c10i = c1 <= c2;
    Console.WriteLine(c10i.ToString());
    Constraint c10k = c1 < c2;
    Console.WriteLine(c10k.ToString());
    IntExpr e11a = c1 + (y == 2);
    Console.WriteLine(e11a.ToString());
    IntExpr e11b = (y == 2) + c1;
    Console.WriteLine(e11b.ToString());
    IntExpr e11c = c1 - (y == 2);
    Console.WriteLine(e11c.ToString());
    IntExpr e11d = (y == 2) - c1;
    Console.WriteLine(e11d.ToString());
    IntExpr e11e = c1 * (y == 2);
    Console.WriteLine(e11e.ToString());
    IntExpr e11f = (y == 2) * c1;
    Console.WriteLine(e11f.ToString());
    Constraint c12a = c1 == (y == 2);
    Console.WriteLine(c12a.ToString());
    Constraint c12b = (y == 2) == c1;
    Console.WriteLine(c12b.ToString());
    Constraint c12c = c1 != (y == 2);
    Console.WriteLine(c12c.ToString());
    Constraint c12d = (y == 2) != c1;
    Console.WriteLine(c12d.ToString());
    Constraint c12e = c1 >= (y == 2);
    Console.WriteLine(c12e.ToString());
    Constraint c12f = (y == 2) >= c1;
    Console.WriteLine(c12f.ToString());
    Constraint c12g = c1 > (y == 2);
    Console.WriteLine(c12g.ToString());
    Constraint c12h = (y == 2) > c1;
    Console.WriteLine(c12h.ToString());
    Constraint c12i = c1 <= (y == 2);
    Console.WriteLine(c12i.ToString());
    Constraint c12j = (y == 2) <= c1;
    Console.WriteLine(c12j.ToString());
    Constraint c12k = c1 < (y == 2);
    Console.WriteLine(c12k.ToString());
    Constraint c12l = (y == 2) < c1;
    Console.WriteLine(c12l.ToString());
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

  static void CpModelTest() {
    Solver solver = new Solver("TestConstraint");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    IntVar y = solver.MakeIntVar(0, 10, "y");
    solver.Add(x + y == 5);
    CpModel model = solver.ExportModel();

    Console.WriteLine(model);

    Solver copy = new Solver("loader");
    copy.LoadModel(model);
    CpModelLoader loader = copy.ModelLoader();
    IntVar xc = loader.IntegerExpression(0).Var();
    IntVar yc = loader.IntegerExpression(1).Var();
    DecisionBuilder db = copy.MakePhase(new IntVar[]{xc, yc},
                                        Solver.CHOOSE_FIRST_UNBOUND,
                                        Solver.ASSIGN_MIN_VALUE);
    copy.NewSearch(db);
    while (copy.NextSolution()) {
      Console.WriteLine("xc = " + xc.Value() + ", yx = " + yc.Value());
    }
    copy.EndSearch();
  }

    static void CpSimpleLoadModelTest() {

    CpModel expected;

    const string constraintName = "equation";
    const string constraintText = "((x(0..10) + y(0..10)) == 5)";

    // Make sure that resources are isolated in the using block.
    using (var s = new Solver("TestConstraint"))
    {
      var x = s.MakeIntVar(0, 10, "x");
      var y = s.MakeIntVar(0, 10, "y");
      Check(x.Name() == "x", "x variable name incorrect.");
      Check(y.Name() == "y", "y variable name incorrect.");
      var c = x + y == 5;
      //      c.Cst.SetName(constraintName);
      //      Check(c.Cst.Name() == constraintName, "Constraint name incorrect.");
      Check(c.Cst.ToString() == constraintText, "Constraint is incorrect.");
      s.Add(c);
      expected = s.ExportModel();
      Console.WriteLine("Expected model string after export: {0}", expected);
    }

    // While interesting, this is not very useful nor especially typical use case scenario.
    using (var s = new Solver("TestConstraint"))
    {
      s.LoadModel(expected);
      Check(s.Constraints() == 1, "Incorrect number of constraints.");
      var actual = s.ExportModel();
      Console.WriteLine("Actual model string after load: {0}", actual);
      // Even the simple example should PASS this I think, but it is not currently.
      Check(expected.ToString() == actual.ToString(), "Model string incorrect.");
      var loader = s.ModelLoader();
      var x = loader.IntegerExpression(0).Var();
      var y = loader.IntegerExpression(1).Var();
      Check(!ReferenceEquals(null, x), "x variable not found after loaded model.");
      Check(!ReferenceEquals(null, y), "y variable not found after loaded model.");
      {
        // Sanity check that what we loaded from the Proto is actually correct according to what we expected
        var c = x + y == 5;
        Check(c.Cst.ToString() == constraintText, "Constraint is incorrect.");
      }
      var db = s.MakePhase(x, y, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
      s.NewSearch(db);
      while (s.NextSolution()) {
        Console.WriteLine("x = {0}, y = {1}", x.Value(), y.Value());
      }
      s.EndSearch();
    }
  }

  static void CpLoadModelAfterSearchTest() {

    CpModel expected;

    const string constraintName = "equation";
    const string constraintText = "((x(0..10) + y(0..10)) == 5)";

    // Make sure that resources are isolated in the using block.
    using (var s = new Solver("TestConstraint"))
    {
      var x = s.MakeIntVar(0, 10, "x");
      var y = s.MakeIntVar(0, 10, "y");
      Check(x.Name() == "x", "x variable name incorrect.");
      Check(y.Name() == "y", "y variable name incorrect.");
      var c = x + y == 5;
//      c.Cst.SetName(constraintName);
//      Check(c.Cst.Name() == constraintName, "Constraint name incorrect.");
      Check(c.Cst.ToString() == constraintText, "Constraint is incorrect.");
      s.Add(c);
      // TODO: TBD: support solution collector?
      var db = s.MakePhase(x, y, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
      Check(!ReferenceEquals(null, db), "Expected a valid Decision Builder");
      s.NewSearch(db);
      var count = 0;
      while (s.NextSolution()) {
        Console.WriteLine("x = {0}, y = {1}", x.Value(), y.Value());
        ++count;
        // Break after we found at least one solution.
        break;
      }
      s.EndSearch();
      Check(count > 0, "Must find at least one solution.");
      // TODO: TBD: export with monitors and/or decision builder?
      expected = s.ExportModel();
      Console.WriteLine("Expected model string after export: {0}", expected);
    }

    // While interesting, this is not very useful nor especially typical use case scenario.
    using (var s = new Solver("TestConstraint"))
    {
      // TODO: TBD: load with monitors and/or decision builder?
      s.LoadModel(expected);
      // This is the first test that should PASS when loading; however, it FAILS because the Constraint is NOT loaded as it is a "TrueConstraint()"
      Check(s.Constraints() == 1, "Incorrect number of constraints.");
      var actual = s.ExportModel();
      Console.WriteLine("Actual model string after load: {0}", actual);
      // Should also be correct after re-load, but I suspect isn't even close, but I could be wrong.
      Check(expected.ToString() == actual.ToString(), "Model string incorrect.");
      var loader = s.ModelLoader();
      var x = loader.IntegerExpression(0).Var();
      var y = loader.IntegerExpression(1).Var();
      Check(!ReferenceEquals(null, x), "x variable not found after loaded model.");
      Check(!ReferenceEquals(null, y), "y variable not found after loaded model.");
      {
        // Do this sanity check that what we loaded is actually what we expected should load.
        var c = x + y == 5;
        // Further documented verification, provided we got this far, and/or with "proper" unit test Assertions...
        Check(c.Cst.ToString() == constraintText, "Constraint is incorrect.");
        Check(c.Cst.ToString() != "TrueConstraint()", "Constraint is incorrect.");
      }
      // TODO: TBD: support solution collector?
      // Should pick up where we left off.
      var db = s.MakePhase(x, y, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
      s.NewSearch(db);
      while (s.NextSolution()) {
        Console.WriteLine("x = {0}, y = {1}", x.Value(), y.Value());
      }
      s.EndSearch();
    }
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
    CpModelTest();
    CpSimpleLoadModelTest();
    CpLoadModelAfterSearchTest();
    if (error_count_ != 0) {
      Console.WriteLine("Found " + error_count_ + " errors.");
      Environment.Exit(1);
    }
  }
}
