// Copyright 2010-2021 Google LLC
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
using Xunit;
using Google.OrTools.ConstraintSolver;
using static Google.OrTools.ConstraintSolver.operations_research_constraint_solver;

namespace Google.OrTools.Tests
{
    public class ConstraintSolverTest
    {
        [Fact]
        public void IntVectorToInt64Vector()
        {
            int[] input = { 5, 11, 17 };
            long[] output = ToInt64Vector(input);
            Assert.Equal(3, output.Length);
            Assert.Equal(5, output[0]);
            Assert.Equal(11, output[1]);
            Assert.Equal(17, output[2]);
        }

        [Fact]
        public void IntVarConstructor()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 7, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(7, x.Max());
            Assert.Equal("x(3..7)", x.ToString());
        }

        [Fact]
        public void ConstraintConstructor()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 7, "x");
            Assert.Equal("x(3..7)", x.ToString());

            // Unary operator
            Constraint c0 = (x == 5);
            Assert.Equal("(x(3..7) == 5)", c0.ToString());
            IntExpr e1 = -c0;
            Assert.Equal("-(Watch<x == 5>(0 .. 1))", e1.ToString());
            IntExpr e2 = c0.Abs();
            Assert.Equal("Watch<x == 5>(0 .. 1)", e2.ToString());
            IntExpr e3 = c0.Square();
            Assert.Equal("IntSquare(Watch<x == 5>(0 .. 1))", e3.ToString());

            // Relational operator with a scalar
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
        public void IntExprConstructor()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            // Unary Operator
            IntExpr e1 = -(x == 7);
            Assert.Equal("-(Watch<x == 7>(0 .. 1))", e1.ToString());
            IntExpr e2 = (x == 7).Abs();
            Assert.Equal("Watch<x == 7>(0 .. 1)", e2.ToString());
            IntExpr e3 = (x == 7).Square();
            Assert.Equal("IntSquare(Watch<x == 7>(0 .. 1))", e3.ToString());
        }

        [Fact]
        public void GreaterIntExprConstructor()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            // Unary Operator
            IntExpr e1 = -(x >= 7);
            Assert.Equal("-(Watch<x >= 7>(0 .. 1))", e1.ToString());
            IntExpr e2 = (x >= 7).Abs();
            Assert.Equal("Watch<x >= 7>(0 .. 1)", e2.ToString());
            IntExpr e3 = (x >= 7).Square();
            Assert.Equal("IntSquare(Watch<x >= 7>(0 .. 1))", e3.ToString());
        }

        [Fact]
        public void ConstraintAndScalar()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            Constraint c1 = (x == 7);
            Assert.Equal("(x(3..13) == 7)", c1.ToString());

            // Arithmetic operator with a scalar
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

            // Relational operator with a scalar
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
        }

        [Fact]
        public void ConstraintAndIntVar()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            Constraint c1 = x == 7;
            Assert.Equal("(x(3..13) == 7)", c1.ToString());

            IntVar y = solver.MakeIntVar(5, 17, "y");
            Assert.Equal(5, y.Min());
            Assert.Equal(17, y.Max());

            // Arithmetic operator with IntVar
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
        }

        [Fact]
        public void ConstraintAndIntExpr()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());
            Constraint c1 = x == 7;
            Assert.Equal("(x(3..13) == 7)", c1.ToString());

            IntVar y = solver.MakeIntVar(5, 17, "y");
            Assert.Equal(5, y.Min());
            Assert.Equal(17, y.Max());

            // Arithmetic operator with an IntExpr
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

            // Relational operator with an IntExpr
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

        [Fact]
        public void ConstraintAndConstraint()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());
            Constraint c1 = x == 7;
            Assert.Equal("(x(3..13) == 7)", c1.ToString());

            IntVar y = solver.MakeIntVar(5, 17, "y");
            Assert.Equal(5, y.Min());
            Assert.Equal(17, y.Max());
            Constraint c2 = y == 11;
            Assert.Equal("(y(5..17) == 11)", c2.ToString());

            // Relational operator with a Constraint
            Constraint c10a = c1 == c2;
            Assert.Equal("Watch<x == 7>(0 .. 1) == Watch<y == 11>(0 .. 1)", c10a.ToString());

            Constraint c10b = c1 != c2;
            Assert.Equal("Watch<x == 7>(0 .. 1) != Watch<y == 11>(0 .. 1)", c10b.ToString());

            Constraint c10c = c1 <= c2;
            Assert.Equal("Watch<x == 7>(0 .. 1) <= Watch<y == 11>(0 .. 1)", c10c.ToString());
            Constraint c10d = c1 >= c2;
            Assert.Equal("Watch<y == 11>(0 .. 1) <= Watch<x == 7>(0 .. 1)", c10d.ToString());

            Constraint c10e = c1 > c2;
            Assert.Equal("Watch<y == 11>(0 .. 1) < Watch<x == 7>(0 .. 1)", c10e.ToString());
            Constraint c10f = c1 < c2;
            Assert.Equal("Watch<x == 7>(0 .. 1) < Watch<y == 11>(0 .. 1)", c10f.ToString());
        }

        [Fact]
        public void IntExprAndScalar()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            // Arithmetic operator with a scalar
            IntExpr e2a = (x == 7) + 1;
            Assert.Equal("(Watch<x == 7>(0 .. 1) + 1)", e2a.ToString());
            IntExpr e2b = 1 + (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) + 1)", e2b.ToString());

            IntExpr e2c = (x == 7) - 1;
            Assert.Equal("(Watch<x == 7>(0 .. 1) + -1)", e2c.ToString());
            IntExpr e2d = 1 - (x == 7);
            Assert.Equal("Not(Watch<x == 7>(0 .. 1))", e2d.ToString());

            IntExpr e2e = (x == 7) * 2;
            Assert.Equal("(Watch<x == 7>(0 .. 1) * 2)", e2e.ToString());
            IntExpr e2f = 2 * (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) * 2)", e2f.ToString());

            IntExpr e2g = (x == 7) / 4;
            Assert.Equal("(Watch<x == 7>(0 .. 1) div 4)", e2g.ToString());

            // Relational operator with a scalar
            Constraint c8a = (x == 7) == 1;
            Assert.Equal("(Watch<x == 7>(0 .. 1) == 1)", c8a.ToString());
            Constraint c8b = 1 == (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) == 1)", c8b.ToString());

            Constraint c8c = (x == 7) != 1;
            Assert.Equal("(Watch<x == 7>(0 .. 1) != 1)", c8c.ToString());
            Constraint c8d = 1 != (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) != 1)", c8d.ToString());

            Constraint c8e = (x == 7) >= 1;
            Assert.Equal("(Watch<x == 7>(0 .. 1) >= 1)", c8e.ToString());
            Constraint c8f = 1 >= (x == 7);
            Assert.Equal("TrueConstraint()", c8f.ToString());

            Constraint c8g = (x == 7) > 1;
            Assert.Equal("FalseConstraint()", c8g.ToString());
            Constraint c8h = 1 > (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) <= 0)", c8h.ToString());

            Constraint c8i = (x == 7) <= 1;
            Assert.Equal("TrueConstraint()", c8i.ToString());
            Constraint c8j = 1 <= (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) >= 1)", c8j.ToString());

            Constraint c8k = (x == 7) < 1;
            Assert.Equal("(Watch<x == 7>(0 .. 1) <= 0)", c8k.ToString());
            Constraint c8l = 1 < (x == 7);
            Assert.Equal("FalseConstraint()", c8l.ToString());
        }

        [Fact]
        public void IntExprAndIntVar()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            IntVar y = solver.MakeIntVar(5, 17, "y");
            Assert.Equal(5, y.Min());
            Assert.Equal(17, y.Max());

            // Arithmetic operator with IntVar
            IntExpr e3a = (x == 7) + y;
            Assert.Equal("(Watch<x == 7>(0 .. 1) + y(5..17))", e3a.ToString());
            IntExpr e3b = y + (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) + y(5..17))", e3b.ToString());

            IntExpr e3c = (x == 7) - y;
            Assert.Equal("(Watch<x == 7>(0 .. 1) - y(5..17))", e3c.ToString());
            IntExpr e3d = y - (x == 7);
            Assert.Equal("(y(5..17) - Watch<x == 7>(0 .. 1))", e3d.ToString());

            IntExpr e3e = (x == 7) * y;
            Assert.Equal("(Watch<x == 7>(0 .. 1) * y(5..17))", e3e.ToString());
            IntExpr e3f = y * (x == 7);
            Assert.Equal("(Watch<x == 7>(0 .. 1) * y(5..17))", e3f.ToString());

            // Relational operator with an IntVar
            Constraint c9a = (x == 7) == y;
            Assert.Equal("Watch<x == 7>(0 .. 1) == y(5..17)", c9a.ToString());
            Constraint c9b = y == (x == 7);
            Assert.Equal("y(5..17) == Watch<x == 7>(0 .. 1)", c9b.ToString());

            Constraint c9c = (x == 7) != y;
            Assert.Equal("Watch<x == 7>(0 .. 1) != y(5..17)", c9c.ToString());
            Constraint c9d = y != (x == 7);
            Assert.Equal("y(5..17) != Watch<x == 7>(0 .. 1)", c9d.ToString());

            Constraint c9e = (x == 7) >= y;
            Assert.Equal("y(5..17) <= Watch<x == 7>(0 .. 1)", c9e.ToString());
            Constraint c9f = y >= (x == 7);
            Assert.Equal("Watch<x == 7>(0 .. 1) <= y(5..17)", c9f.ToString());

            Constraint c9g = (x == 7) > y;
            Assert.Equal("y(5..17) < Watch<x == 7>(0 .. 1)", c9g.ToString());
            Constraint c9h = y > (x == 7);
            Assert.Equal("Watch<x == 7>(0 .. 1) < y(5..17)", c9h.ToString());

            Constraint c9i = (x == 7) <= y;
            Assert.Equal("Watch<x == 7>(0 .. 1) <= y(5..17)", c9i.ToString());
            Constraint c9j = y <= (x == 7);
            Assert.Equal("y(5..17) <= Watch<x == 7>(0 .. 1)", c9j.ToString());

            Constraint c9k = (x == 7) < y;
            Assert.Equal("Watch<x == 7>(0 .. 1) < y(5..17)", c9k.ToString());
            Constraint c9l = y < (x == 7);
            Assert.Equal("y(5..17) < Watch<x == 7>(0 .. 1)", c9l.ToString());
        }

        [Fact]
        public void IntExprAndIntExpr()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            IntVar y = solver.MakeIntVar(5, 17, "y");
            Assert.Equal(5, y.Min());
            Assert.Equal(17, y.Max());

            // Relational operator between IntExpr
            Constraint c10a = (x == 7) == (y == 11);
            Assert.Equal("Watch<x == 7>(0 .. 1) == Watch<y == 11>(0 .. 1)", c10a.ToString());

            Constraint c10b = (x == 7) != (y == 11);
            Assert.Equal("Watch<x == 7>(0 .. 1) != Watch<y == 11>(0 .. 1)", c10b.ToString());

            Constraint c10c = (x == 7) <= (y == 11);
            Assert.Equal("Watch<x == 7>(0 .. 1) <= Watch<y == 11>(0 .. 1)", c10c.ToString());
            Constraint c10d = (x == 7) >= (y == 11);
            Assert.Equal("Watch<y == 11>(0 .. 1) <= Watch<x == 7>(0 .. 1)", c10d.ToString());

            Constraint c10e = (x == 7) > (y == 11);
            Assert.Equal("Watch<y == 11>(0 .. 1) < Watch<x == 7>(0 .. 1)", c10e.ToString());
            Constraint c10f = (x == 7) < (y == 11);
            Assert.Equal("Watch<x == 7>(0 .. 1) < Watch<y == 11>(0 .. 1)", c10f.ToString());
        }

        [Fact]
        public void GreaterIntExprAndScalar()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            // Arithmetic operator with a scalar
            IntExpr e2a = (x >= 7) + 1;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) + 1)", e2a.ToString());
            IntExpr e2b = 1 + (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) + 1)", e2b.ToString());

            IntExpr e2c = (x >= 7) - 1;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) + -1)", e2c.ToString());
            IntExpr e2d = 1 - (x >= 7);
            Assert.Equal("Not(Watch<x >= 7>(0 .. 1))", e2d.ToString());

            IntExpr e2e = (x >= 7) * 2;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) * 2)", e2e.ToString());
            IntExpr e2f = 2 * (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) * 2)", e2f.ToString());

            // Relational operator with a scalar
            Constraint c8a = (x >= 7) == 1;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) == 1)", c8a.ToString());
            Constraint c8b = 1 == (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) == 1)", c8b.ToString());

            Constraint c8c = (x >= 7) != 1;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) != 1)", c8c.ToString());
            Constraint c8d = 1 != (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) != 1)", c8d.ToString());

            Constraint c8e = (x >= 7) >= 1;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) >= 1)", c8e.ToString());
            Constraint c8f = 1 >= (x >= 7);
            Assert.Equal("TrueConstraint()", c8f.ToString());

            Constraint c8g = (x >= 7) > 1;
            Assert.Equal("FalseConstraint()", c8g.ToString());
            Constraint c8h = 1 > (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) <= 0)", c8h.ToString());

            Constraint c8i = (x >= 7) <= 1;
            Assert.Equal("TrueConstraint()", c8i.ToString());
            Constraint c8j = 1 <= (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) >= 1)", c8j.ToString());

            Constraint c8k = (x >= 7) < 1;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) <= 0)", c8k.ToString());
            Constraint c8l = 1 < (x >= 7);
            Assert.Equal("FalseConstraint()", c8l.ToString());
        }

        [Fact]
        public void GreaterIntExprAndIntVar()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            IntVar y = solver.MakeIntVar(5, 17, "y");
            Assert.Equal(5, y.Min());
            Assert.Equal(17, y.Max());

            // Arithmetic operator with IntVar
            IntExpr e3a = (x >= 7) + y;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) + y(5..17))", e3a.ToString());
            IntExpr e3b = y + (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) + y(5..17))", e3b.ToString());

            IntExpr e3c = (x >= 7) - y;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) - y(5..17))", e3c.ToString());
            IntExpr e3d = y - (x >= 7);
            Assert.Equal("(y(5..17) - Watch<x >= 7>(0 .. 1))", e3d.ToString());

            IntExpr e3e = (x >= 7) * y;
            Assert.Equal("(Watch<x >= 7>(0 .. 1) * y(5..17))", e3e.ToString());
            IntExpr e3f = y * (x >= 7);
            Assert.Equal("(Watch<x >= 7>(0 .. 1) * y(5..17))", e3f.ToString());

            // Relational operator with an IntVar
            Constraint c9a = (x >= 7) == y;
            Assert.Equal("Watch<x >= 7>(0 .. 1) == y(5..17)", c9a.ToString());
            Constraint c9b = y == (x >= 7);
            Assert.Equal("y(5..17) == Watch<x >= 7>(0 .. 1)", c9b.ToString());

            Constraint c9c = (x >= 7) != y;
            Assert.Equal("Watch<x >= 7>(0 .. 1) != y(5..17)", c9c.ToString());
            Constraint c9d = y != (x >= 7);
            Assert.Equal("y(5..17) != Watch<x >= 7>(0 .. 1)", c9d.ToString());

            Constraint c9e = (x >= 7) >= y;
            Assert.Equal("y(5..17) <= Watch<x >= 7>(0 .. 1)", c9e.ToString());
            Constraint c9f = y >= (x >= 7);
            Assert.Equal("Watch<x >= 7>(0 .. 1) <= y(5..17)", c9f.ToString());

            Constraint c9g = (x >= 7) > y;
            Assert.Equal("y(5..17) < Watch<x >= 7>(0 .. 1)", c9g.ToString());
            Constraint c9h = y > (x >= 7);
            Assert.Equal("Watch<x >= 7>(0 .. 1) < y(5..17)", c9h.ToString());

            Constraint c9i = (x >= 7) <= y;
            Assert.Equal("Watch<x >= 7>(0 .. 1) <= y(5..17)", c9i.ToString());
            Constraint c9j = y <= (x >= 7);
            Assert.Equal("y(5..17) <= Watch<x >= 7>(0 .. 1)", c9j.ToString());

            Constraint c9k = (x >= 7) < y;
            Assert.Equal("Watch<x >= 7>(0 .. 1) < y(5..17)", c9k.ToString());
            Constraint c9l = y < (x >= 7);
            Assert.Equal("y(5..17) < Watch<x >= 7>(0 .. 1)", c9l.ToString());
        }

        [Fact]
        public void GreaterIntExprAndGreaterIntExpr()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(3, 13, "x");
            Assert.Equal(3, x.Min());
            Assert.Equal(13, x.Max());

            IntVar y = solver.MakeIntVar(5, 17, "y");
            Assert.Equal(5, y.Min());
            Assert.Equal(17, y.Max());

            // Relational operator between IntExpr
            Constraint c10a = (x >= 7) == (y >= 11);
            Assert.Equal("Watch<x >= 7>(0 .. 1) == Watch<y >= 11>(0 .. 1)", c10a.ToString());

            Constraint c10b = (x >= 7) != (y >= 11);
            Assert.Equal("Watch<x >= 7>(0 .. 1) != Watch<y >= 11>(0 .. 1)", c10b.ToString());

            Constraint c10c = (x >= 7) <= (y >= 11);
            Assert.Equal("Watch<x >= 7>(0 .. 1) <= Watch<y >= 11>(0 .. 1)", c10c.ToString());
            Constraint c10d = (x >= 7) >= (y >= 11);
            Assert.Equal("Watch<y >= 11>(0 .. 1) <= Watch<x >= 7>(0 .. 1)", c10d.ToString());

            Constraint c10e = (x >= 7) > (y >= 11);
            Assert.Equal("Watch<y >= 11>(0 .. 1) < Watch<x >= 7>(0 .. 1)", c10e.ToString());
            Constraint c10f = (x >= 7) < (y >= 11);
            Assert.Equal("Watch<x >= 7>(0 .. 1) < Watch<y >= 11>(0 .. 1)", c10f.ToString());
        }

        [Fact]
        public void Downcast()
        {
            Solver solver = new Solver("Solver");
            IntVar x = solver.MakeIntVar(2, 17, "x");
            IntExpr e = x + 5;
            IntVar y = e.Var();
            Assert.Equal("(x(2..17) + 5)", y.ToString());
        }

        [Fact]
        public void Sequence()
        {
            Solver solver = new Solver("Solver");
            IntervalVar[] intervals = solver.MakeFixedDurationIntervalVarArray(10, 0, 10, 5, false, "task");
            DisjunctiveConstraint disjunctive = intervals.Disjunctive("Sequence");
            SequenceVar var = disjunctive.SequenceVar();
            Assignment ass = solver.MakeAssignment();
            ass.Add(var);
            ass.SetForwardSequence(var, new int[] { 1, 3, 5 });
            int[] seq = ass.ForwardSequence(var);
            Assert.Equal(3, seq.Length);
            Assert.Equal(1, seq[0]);
            Assert.Equal(3, seq[1]);
            Assert.Equal(5, seq[2]);
        }

        // A simple demon that simply sets the maximum of a fixed IntVar to 10 when
        // it's being called.
        class SetMaxDemon : NetDemon
        {
            public SetMaxDemon(IntVar x)
            {
                x_ = x;
            }
            public override void Run(Solver s)
            {
                x_.SetMax(10);
            }
            private IntVar x_;
        }

        [Fact]
        public void Demon()
        {
            Solver solver = new Solver("DemonTest");
            IntVar x = solver.MakeIntVar(new int[] { 2, 4, -1, 6, 11, 10 }, "x");
            NetDemon demon = new SetMaxDemon(x);
            Assert.Equal(11, x.Max());
            demon.Run(solver);
            Assert.Equal(10, x.Max());
        }

        // This constraint has a single target variable x. It enforces x >= 5 upon
        // InitialPropagate() and invokes the SetMaxDemon when x changes its range.
        class SetMinAndMaxConstraint : NetConstraint
        {
            public SetMinAndMaxConstraint(Solver solver, IntVar x) : base(solver)
            {
                x_ = x;
            }
            public override void Post()
            {
                // Always store the demon in the constraint to avoid it being reclaimed
                // by the GC.
                demon_ = new SetMaxDemon(x_);
                x_.WhenBound(demon_);
            }
            public override void InitialPropagate()
            {
                x_.SetMin(5);
            }
            private IntVar x_;
            private Demon demon_;
        }

        [Fact]
        public void MinAndMaxConstraint()
        {
            Solver solver = new Solver("TestConstraint");
            IntVar x = solver.MakeIntVar(new int[] { 2, 4, -1, 6, 11, 10 }, "x");
            Constraint ct = new SetMinAndMaxConstraint(solver, x);
            solver.Add(ct);
            DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
            solver.NewSearch(db);
            Assert.Equal(-1, x.Min());
            Assert.Equal(11, x.Max());

            Assert.True(solver.NextSolution());
            Assert.Equal(6, x.Min());
            Assert.Equal(6, x.Max());

            Assert.True(solver.NextSolution());
            Assert.Equal(10, x.Min());
            Assert.Equal(10, x.Max());

            Assert.False(solver.NextSolution());
            solver.EndSearch();
        }

        // // This constraint has a single target variable x. It enforces x >= 5,
        // but only at the leaf of the search tree: it doesn't change x's bounds,
        // and simply fails if x is bound and is < 5.
        class DumbGreaterOrEqualToFive : NetConstraint
        {
            public DumbGreaterOrEqualToFive(Solver solver, IntVar x) : base(solver)
            {
                x_ = x;
            }
            public override void Post()
            {
                demon_ = solver().MakeConstraintInitialPropagateCallback(this);
                x_.WhenRange(demon_);
            }
            public override void InitialPropagate()
            {
                if (x_.Bound())
                {
                    if (x_.Value() < 5)
                    {
                        solver().Fail();
                    }
                }
            }
            private IntVar x_;
            private Demon demon_;
        }

        [Fact]
        public void FailingConstraint()
        {
            Solver solver = new Solver("TestConstraint");
            IntVar x = solver.MakeIntVar(new int[] { 2, 4, -1, 6, 11, 10 }, "x");
            Constraint ct = new DumbGreaterOrEqualToFive(solver, x);
            solver.Add(ct);
            DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
            solver.NewSearch(db);
            Assert.True(solver.NextSolution());
            Assert.Equal(6, x.Min());
            solver.EndSearch();
        }

        [Fact]
        public void DomainIterator()
        {
            Solver solver = new Solver("TestConstraint");
            IntVar x = solver.MakeIntVar(new int[] { 2, 4, -1, 6, 11, 10 }, "x");
            ulong count = 0;
            foreach (long value in x.GetDomain())
            {
                count++;
            }
            Assert.Equal(count, x.Size());
        }

        class CountHoles : NetDemon
        {
            public CountHoles(IntVar x)
            {
                x_ = x;
                count_ = 0;
            }
            public override void Run(Solver s)
            {
                foreach (long removed in x_.GetHoles())
                {
                    count_++;
                }
            }
            public int count()
            {
                return count_;
            }
            private IntVar x_;
            private int count_;
        }

        class RemoveThreeValues : NetConstraint
        {
            public RemoveThreeValues(Solver solver, IntVar x) : base(solver)
            {
                x_ = x;
            }
            public override void Post()
            {
                demon_ = new CountHoles(x_);
                x_.WhenDomain(demon_);
            }
            public override void InitialPropagate()
            {
                x_.RemoveValues(new long[] { 3, 5, 7 });
            }
            public int count()
            {
                return demon_.count();
            }
            private IntVar x_;
            private CountHoles demon_;
        }

        [Fact]
        public void HoleIteratorTest()
        {
            Solver solver = new Solver("TestConstraint");
            IntVar x = solver.MakeIntVar(0, 10, "x");
            RemoveThreeValues ct = new RemoveThreeValues(solver, x);
            solver.Add(ct);
            DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
            solver.Solve(db);
            Assert.Equal(3, ct.count());
        }

        // TODO(user): Improve search log tests; currently only tests coverage.
        void RunSearchLog(in SearchMonitor searchlog)
        {
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
        public void SearchLog()
        {
            Solver solver = new Solver("TestSearchLog");
            IntVar var = solver.MakeIntVar(1, 1, "Variable");
            OptimizeVar objective = solver.MakeMinimize(var, 1);
            SearchMonitor searchlog = solver.MakeSearchLog(0);
            RunSearchLog(in searchlog);
            GC.KeepAlive(solver);
        }

        [Theory]
        [InlineData(false)]
        [InlineData(true)]
        public void SearchLogWithCallback(bool callGC)
        {
            Solver solver = new Solver("TestSearchLog");
            IntVar var = solver.MakeIntVar(1, 1, "Variable");
            OptimizeVar objective = solver.MakeMinimize(var, 1);
            int count = 0;
            SearchMonitor searchlog = solver.MakeSearchLog(0, // branch period
                                                           () => {
                                                               count++;
                                                               return "display callback...";
                                                           });
            if (callGC)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
            RunSearchLog(in searchlog);
            GC.KeepAlive(solver);
            Assert.Equal(1, count);
        }

        [Theory]
        [InlineData(false)]
        [InlineData(true)]
        public void SearchLogWithObjectiveAndCallback(bool callGC)
        {
            Solver solver = new Solver("TestSearchLog");
            IntVar var = solver.MakeIntVar(1, 1, "Variable");
            OptimizeVar objective = solver.MakeMinimize(var, 1);
            int count = 0;
            SearchMonitor searchlog = solver.MakeSearchLog(0,         // branch period
                                                           objective, // objective var to monitor
                                                           () => {
                                                               count++;
                                                               return "OptimizeVar display callback";
                                                           });
            if (callGC)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
            RunSearchLog(in searchlog);
            GC.KeepAlive(solver);
            Assert.Equal(1, count);
        }

        [Theory]
        [InlineData(false)]
        [InlineData(true)]
        public void SearchLogWithIntVarAndCallback(bool callGC)
        {
            Solver solver = new Solver("TestSearchLog");
            IntVar var = solver.MakeIntVar(1, 1, "Variable");
            OptimizeVar objective = solver.MakeMinimize(var, 1);
            int count = 0;
            SearchMonitor searchlog = solver.MakeSearchLog(0,   // branch period
                                                           var, // int var to monitor
                                                           () => {
                                                               count++;
                                                               return "IntVar display callback";
                                                           });
            if (callGC)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
            RunSearchLog(in searchlog);
            GC.KeepAlive(solver);
            Assert.Equal(1, count);
        }
    }
} // namespace Google.OrTools.Tests
