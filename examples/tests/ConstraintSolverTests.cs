using System;
using Xunit;
using Google.OrTools.ConstraintSolver;

namespace Google.OrTools.Tests {
  public
    class ConstraintSolverTest {
      [Fact]
        public void Constructors() {
          Solver solver = new Solver("ConstructorsTest");
          IntVar x = solver.MakeIntVar(3, 7, "x");
          Assert.Equal("x(3..7)", x.ToString());
          Constraint c1 = x == 5;
          Assert.Equal("(x(3..7) == 5)", c1.ToString());
          Constraint c2 = x >= 5;
          Assert.Equal("(x(3..7) >= 5)", c2.ToString());
          Constraint c3 = x > 5;
          Assert.Equal("(x(3..7) >= 6)", c3.ToString());
          Constraint c4 = x <= 5;
          Assert.Equal("(x(3..7) <= 5)", c4.ToString());
          Constraint c5 = x < 5;
          Assert.Equal("(x(3..7) <= 4)", c5.ToString());
          Constraint c6 = x != 5;
          Assert.Equal("(x(3..7) != 5)", c6.ToString());
          Constraint c7 = x == 2;
          Assert.Equal("FalseConstraint()", c7.ToString());
        }

      [Fact]
        public void ConstraintWithExpr() {
          Solver solver = new Solver("ConstraintWithExprTest");
          IntVar x = solver.MakeIntVar(3, 13, "x");
          Assert.Equal("x(3..13)", x.ToString());
          IntVar y = solver.MakeIntVar(5, 17, "y");
          Assert.Equal("y(5..17)", y.ToString());

          Constraint c1 = x == 7;
          Assert.Equal("(x(3..13) == 7)", c1.ToString());

          // arithmetic operator with value
          IntExpr e2a = c1 + 1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) + 1)", e2a.ToString());
          IntExpr e2b = 1 + c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) + 1)", e2b.ToString());
          IntExpr e2c = c1 - 1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) + -1)", e2c.ToString());
          IntExpr e2d = 1 - c1;
          Assert.Equal("Not(Watch<x == 7>(0 .. 1))", e2d.ToString());
          IntExpr e2e = c1 * 2;
          Assert.Equal("(Watch<x == 7>(0 .. 1) * 2)", e2e.ToString());
          IntExpr e2f = 2 * c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) * 2)", e2f.ToString());
          IntExpr e2g = c1 / 4;
          Assert.Equal("(Watch<x == 7>(0 .. 1) div 4)", e2g.ToString());

          // arithmetic operator with IntVar
          IntExpr e3a = c1 + y;
          Assert.Equal("(Watch<x == 7>(0 .. 1) + y(5..17))", e3a.ToString());
          IntExpr e3b = y + c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) + y(5..17))", e3b.ToString());
          IntExpr e3c = c1 - y;
          Assert.Equal("(Watch<x == 7>(0 .. 1) - y(5..17))", e3c.ToString());
          IntExpr e3d = y - c1;
          Assert.Equal("(y(5..17) - Watch<x == 7>(0 .. 1))", e3d.ToString());
          IntExpr e3e = c1 * y;
          Assert.Equal("(Watch<x == 7>(0 .. 1) * y(5..17))", e3e.ToString());
          IntExpr e3f = y * c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) * y(5..17))", e3f.ToString());

          // arithmetic operator with an IntExpr
          IntExpr e11a = c1 + (y == 11);
          Assert.Equal("(Watch<x == 7>(0 .. 1) + Watch<y == 11>(0 .. 1))", e11a.ToString());
          IntExpr e11b = (y == 11) + c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) + Watch<y == 11>(0 .. 1))", e11b.ToString());
          IntExpr e11c = c1 - (y == 11);
          Assert.Equal("(Watch<x == 7>(0 .. 1) - Watch<y == 11>(0 .. 1))", e11c.ToString());
          IntExpr e11d = (y == 11) - c1;
          Assert.Equal("(Watch<y == 11>(0 .. 1) - Watch<x == 7>(0 .. 1))", e11d.ToString());
          IntExpr e11e = c1 * (y == 11);
          Assert.Equal("(Watch<x == 7>(0 .. 1) * Watch<y == 11>(0 .. 1))", e11e.ToString());
          IntExpr e11f = (y == 11) * c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) * Watch<y == 11>(0 .. 1))", e11f.ToString());

          // Unary operator
          IntExpr e4 = -c1;
          Assert.Equal("-(Watch<x == 7>(0 .. 1))", e4.ToString());
          IntExpr e6 = c1.Abs();
          Assert.Equal("Watch<x == 7>(0 .. 1)", e6.ToString());
          IntExpr e7 = c1.Square();
          Assert.Equal("IntSquare(Watch<x == 7>(0 .. 1))", e7.ToString());

          // Relational operator with a value
          Constraint c8a = c1 == 1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) == 1)", c8a.ToString());
          Constraint c8b = 1 == c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) == 1)", c8b.ToString());
          Constraint c8c = c1 != 1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) != 1)", c8c.ToString());
          Constraint c8d = 1 != c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) != 1)", c8d.ToString());
          Constraint c8e = c1 >= 1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) >= 1)", c8e.ToString());
          Constraint c8f = 1 >= c1;
          Assert.Equal("TrueConstraint()", c8f.ToString());
          Constraint c8g = c1 > 1;
          Assert.Equal("FalseConstraint()", c8g.ToString());
          Constraint c8h = 1 > c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) <= 0)", c8h.ToString());
          Constraint c8i = c1 <= 1;
          Assert.Equal("TrueConstraint()", c8i.ToString());
          Constraint c8j = 1 <= c1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) >= 1)", c8j.ToString());
          Constraint c8k = c1 < 1;
          Assert.Equal("(Watch<x == 7>(0 .. 1) <= 0)", c8k.ToString());
          Constraint c8l = 1 < c1;
          Assert.Equal("FalseConstraint()", c8l.ToString());

          // Relational operator with an IntVar
          Constraint c9a = c1 == y;
          Assert.Equal("Watch<x == 7>(0 .. 1) == y(5..17)", c9a.ToString());
          Constraint c9b = y == c1;
          Assert.Equal("y(5..17) == Watch<x == 7>(0 .. 1)", c9b.ToString());
          Constraint c9c = c1 != y;
          Assert.Equal("Watch<x == 7>(0 .. 1) != y(5..17)", c9c.ToString());
          Constraint c9d = y != c1;
          Assert.Equal("y(5..17) != Watch<x == 7>(0 .. 1)", c9d.ToString());
          Constraint c9e = c1 >= y;
          Assert.Equal("y(5..17) <= Watch<x == 7>(0 .. 1)", c9e.ToString());
          Constraint c9f = y >= c1;
          Assert.Equal("Watch<x == 7>(0 .. 1) <= y(5..17)", c9f.ToString());
          Constraint c9g = c1 > y;
          Assert.Equal("y(5..17) < Watch<x == 7>(0 .. 1)", c9g.ToString());
          Constraint c9h = y > c1;
          Assert.Equal("Watch<x == 7>(0 .. 1) < y(5..17)", c9h.ToString());
          Constraint c9i = c1 <= y;
          Assert.Equal("Watch<x == 7>(0 .. 1) <= y(5..17)", c9i.ToString());
          Constraint c9j = y <= c1;
          Assert.Equal("y(5..17) <= Watch<x == 7>(0 .. 1)", c9j.ToString());
          Constraint c9k = c1 < y;
          Assert.Equal("Watch<x == 7>(0 .. 1) < y(5..17)", c9k.ToString());
          Constraint c9l = y < c1;
          Assert.Equal("y(5..17) < Watch<x == 7>(0 .. 1)", c9l.ToString());

          // relational operator with a Constraint
          Constraint c2 = y == 11;
          Assert.Equal("(y(5..17) == 11)", c2.ToString());
          Constraint c10a = c1 == c2;
          Assert.Equal("Watch<x == 7>(0 .. 1) == Watch<y == 11>(0 .. 1)", c10a.ToString());
          Constraint c10c = c1 != c2;
          Assert.Equal("Watch<x == 7>(0 .. 1) != Watch<y == 11>(0 .. 1)", c10c.ToString());
          Constraint c10e = c1 >= c2;
          Assert.Equal("Watch<y == 11>(0 .. 1) <= Watch<x == 7>(0 .. 1)", c10e.ToString());
          Constraint c10g = c1 > c2;
          Assert.Equal("Watch<y == 11>(0 .. 1) < Watch<x == 7>(0 .. 1)", c10g.ToString());
          Constraint c10i = c1 <= c2;
          Assert.Equal("Watch<x == 7>(0 .. 1) <= Watch<y == 11>(0 .. 1)", c10i.ToString());
          Constraint c10k = c1 < c2;
          Assert.Equal("Watch<x == 7>(0 .. 1) < Watch<y == 11>(0 .. 1)", c10k.ToString());

          // relational operator with an IntExpr
          Constraint c12a = c1 == (y == 11);
          Assert.Equal("Watch<x == 7>(0 .. 1) == Watch<y == 11>(0 .. 1)", c12a.ToString());
          Constraint c12b = (y == 11) == c1;
          Assert.Equal("Watch<y == 11>(0 .. 1) == Watch<x == 7>(0 .. 1)", c12b.ToString());
          Constraint c12c = c1 != (y == 11);
          Assert.Equal("Watch<x == 7>(0 .. 1) != Watch<y == 11>(0 .. 1)", c12c.ToString());
          Constraint c12d = (y == 11) != c1;
          Assert.Equal("Watch<y == 11>(0 .. 1) != Watch<x == 7>(0 .. 1)", c12d.ToString());
          Constraint c12e = c1 >= (y == 11);
          Assert.Equal("Watch<y == 11>(0 .. 1) <= Watch<x == 7>(0 .. 1)", c12e.ToString());
          Constraint c12f = (y == 11) >= c1;
          Assert.Equal("Watch<x == 7>(0 .. 1) <= Watch<y == 11>(0 .. 1)", c12f.ToString());
          Constraint c12g = c1 > (y == 11);
          Assert.Equal("Watch<y == 11>(0 .. 1) < Watch<x == 7>(0 .. 1)", c12g.ToString());
          Constraint c12h = (y == 11) > c1;
          Assert.Equal("Watch<x == 7>(0 .. 1) < Watch<y == 11>(0 .. 1)", c12h.ToString());
          Constraint c12i = c1 <= (y == 11);
          Assert.Equal("Watch<x == 7>(0 .. 1) <= Watch<y == 11>(0 .. 1)", c12i.ToString());
          Constraint c12j = (y == 11) <= c1;
          Assert.Equal("Watch<y == 11>(0 .. 1) <= Watch<x == 7>(0 .. 1)", c12j.ToString());
          Constraint c12k = c1 < (y == 11);
          Assert.Equal("Watch<x == 7>(0 .. 1) < Watch<y == 11>(0 .. 1)", c12k.ToString());
          Constraint c12l = (y == 11) < c1;
          Assert.Equal("Watch<y == 11>(0 .. 1) < Watch<x == 7>(0 .. 1)", c12l.ToString());
        }

      // TODO(mizux): Improve search log tests; currently only tests coverage.
      void RunSearchLog(in SearchMonitor searchlog) {
        searchlog.EnterSearch();
        searchlog.ExitSearch();
        searchlog.AcceptSolution();
        searchlog.AtSolution();
        searchlog.BeginFail();
        searchlog.NoMoreSolutions();
        searchlog.BeginInitialPropagation();
        searchlog.EndInitialPropagation();
      }

      [Fact]
        public void SearchLog() {
          Solver solver = new Solver("TestSearchLog");
          IntVar var = solver.MakeIntVar(1, 1, "Variable");
          OptimizeVar objective = solver.MakeMinimize(var, 1);
          SearchMonitor searchlog = solver.MakeSearchLog(0);
          RunSearchLog(in searchlog);
        }

      [Theory][InlineData(false)][InlineData(true)]
        public void SearchLogWithCallback(bool callGC) {
          Solver solver = new Solver("TestSearchLog");
          IntVar var = solver.MakeIntVar(1, 1, "Variable");
          OptimizeVar objective = solver.MakeMinimize(var, 1);
          int count = 0;
          SearchMonitor searchlog = solver.MakeSearchLog(
              0,  // branch period
              () => {
              count++;
              return "display callback...";
              });
          if (callGC) {
            GC.Collect();
          }
          RunSearchLog(in searchlog);
          Assert.Equal(1, count);
        }

      [Theory][InlineData(false)][InlineData(true)]
        public void SearchLogWithObjectiveAndCallback(bool callGC) {
          Solver solver = new Solver("TestSearchLog");
          IntVar var = solver.MakeIntVar(1, 1, "Variable");
          OptimizeVar objective = solver.MakeMinimize(var, 1);
          int count = 0;
          SearchMonitor searchlog = solver.MakeSearchLog(
              0,          // branch period
              objective,  // objective var to monitor
              () => {
              count++;
              return "OptimizeVar display callback";
              });
          if (callGC) {
            GC.Collect();
          }
          RunSearchLog(in searchlog);
          Assert.Equal(1, count);
        }

      [Theory][InlineData(false)][InlineData(true)]
        public void SearchLogWithIntVarAndCallback(bool callGC) {
          Solver solver = new Solver("TestSearchLog");
          IntVar var = solver.MakeIntVar(1, 1, "Variable");
          OptimizeVar objective = solver.MakeMinimize(var, 1);
          int count = 0;
          SearchMonitor searchlog = solver.MakeSearchLog(
              0,    // branch period
              var,  // objective var to monitor
              () => {
              count++;
              return "IntVar display callback";
              });
          if (callGC) {
            GC.Collect();
          }
          RunSearchLog(in searchlog);
          Assert.Equal(1, count);
        }
    }
}  // namespace Google.OrTools.Tests
