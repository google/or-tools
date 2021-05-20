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
using Google.OrTools.LinearSolver;

namespace Google.OrTools.Tests
{
    public class LinearSolverTest
    {
        [Fact]
        public void VarOperator()
        {
            Solver solver = Solver.CreateSolver("CLP");
            Variable x = solver.MakeNumVar(0.0, 100.0, "x");
            Assert.Equal(0.0, x.Lb());
            Assert.Equal(100.0, x.Ub());

            Constraint ct1 = solver.Add(x >= 1);
            Assert.Equal(1.0, ct1.GetCoefficient(x));
            Assert.Equal(1.0, ct1.Lb());
            Assert.Equal(double.PositiveInfinity, ct1.Ub());

            Constraint ct2 = solver.Add(x <= 1);
            Assert.Equal(1.0, ct2.GetCoefficient(x));
            Assert.Equal(double.NegativeInfinity, ct2.Lb());
            Assert.Equal(1.0, ct2.Ub());

            Constraint ct3 = solver.Add(x == 1);
            Assert.Equal(1.0, ct3.GetCoefficient(x));
            Assert.Equal(1.0, ct3.Lb());
            Assert.Equal(1.0, ct3.Ub());

            Constraint ct4 = solver.Add(1 >= x);
            Assert.Equal(1.0, ct4.GetCoefficient(x));
            Assert.Equal(double.NegativeInfinity, ct4.Lb());
            Assert.Equal(1.0, ct4.Ub());

            Constraint ct5 = solver.Add(1 <= x);
            Assert.Equal(1.0, ct5.GetCoefficient(x));
            Assert.Equal(1.0, ct5.Lb());
            Assert.Equal(double.PositiveInfinity, ct5.Ub());

            Constraint ct6 = solver.Add(1 == x);
            Assert.Equal(1.0, ct6.GetCoefficient(x));
            Assert.Equal(1.0, ct6.Lb());
            Assert.Equal(1.0, ct6.Ub());
        }

        [Fact]
        public void VarAddition()
        {
            Solver solver = Solver.CreateSolver("CLP");
            Variable x = solver.MakeNumVar(0.0, 100.0, "x");
            Assert.Equal(0.0, x.Lb());
            Assert.Equal(100.0, x.Ub());

            Variable y = solver.MakeNumVar(0.0, 100.0, "y");
            Assert.Equal(0.0, y.Lb());
            Assert.Equal(100.0, y.Ub());

            Constraint ct1 = solver.Add(x + y == 1);
            Assert.Equal(1.0, ct1.GetCoefficient(x));
            Assert.Equal(1.0, ct1.GetCoefficient(y));

            Constraint ct2 = solver.Add(x + x == 1);
            Assert.Equal(2.0, ct2.GetCoefficient(x));

            Constraint ct3 = solver.Add(x + (y + x) == 1);
            Assert.Equal(2.0, ct3.GetCoefficient(x));
            Assert.Equal(1.0, ct3.GetCoefficient(y));

            Constraint ct4 = solver.Add(x + (y + x + 3) == 1);
            Assert.Equal(2.0, ct4.GetCoefficient(x));
            Assert.Equal(1.0, ct4.GetCoefficient(y));
            Assert.Equal(-2.0, ct4.Lb());
            Assert.Equal(-2.0, ct4.Ub());
        }

        [Fact]
        public void VarMultiplication()
        {
            Solver solver = Solver.CreateSolver("CLP");
            Variable x = solver.MakeNumVar(0.0, 100.0, "x");
            Assert.Equal(0.0, x.Lb());
            Assert.Equal(100.0, x.Ub());

            Variable y = solver.MakeNumVar(0.0, 100.0, "y");
            Assert.Equal(0.0, y.Lb());
            Assert.Equal(100.0, y.Ub());

            Constraint ct1 = solver.Add(3 * x == 1);
            Assert.Equal(3.0, ct1.GetCoefficient(x));

            Constraint ct2 = solver.Add(x * 3 == 1);
            Assert.Equal(3.0, ct2.GetCoefficient(x));

            Constraint ct3 = solver.Add(x + (2 * y + 3 * x) == 1);
            Assert.Equal(4.0, ct3.GetCoefficient(x));
            Assert.Equal(2.0, ct3.GetCoefficient(y));

            Constraint ct4 = solver.Add(x + 5 * (y + x + 3) == 1);
            Assert.Equal(6.0, ct4.GetCoefficient(x));
            Assert.Equal(5.0, ct4.GetCoefficient(y));
            Assert.Equal(-14.0, ct4.Lb());
            Assert.Equal(-14.0, ct4.Ub());

            Constraint ct5 = solver.Add(x + (2 * y + x + 3) * 3 == 1);
            Assert.Equal(4.0, ct5.GetCoefficient(x));
            Assert.Equal(6.0, ct5.GetCoefficient(y));
            Assert.Equal(-8.0, ct5.Lb());
            Assert.Equal(-8.0, ct5.Ub());
        }

        [Fact]
        public void BinaryOperator()
        {
            Solver solver = Solver.CreateSolver("CLP");
            Variable x = solver.MakeNumVar(0.0, 100.0, "x");
            Assert.Equal(0.0, x.Lb());
            Assert.Equal(100.0, x.Ub());

            Variable y = solver.MakeNumVar(0.0, 100.0, "y");
            Assert.Equal(0.0, y.Lb());
            Assert.Equal(100.0, y.Ub());

            Constraint ct1 = solver.Add(x == y);
            Assert.Equal(1.0, ct1.GetCoefficient(x));
            Assert.Equal(-1.0, ct1.GetCoefficient(y));

            Constraint ct2 = solver.Add(x == 3 * y + 5);
            Assert.Equal(1.0, ct2.GetCoefficient(x));
            Assert.Equal(-3.0, ct2.GetCoefficient(y));
            Assert.Equal(5.0, ct2.Lb());
            Assert.Equal(5.0, ct2.Ub());

            Constraint ct3 = solver.Add(2 * x - 9 == y);
            Assert.Equal(2.0, ct3.GetCoefficient(x));
            Assert.Equal(-1.0, ct3.GetCoefficient(y));
            Assert.Equal(9.0, ct3.Lb());
            Assert.Equal(9.0, ct3.Ub());

            Assert.True(x == x);
            Assert.True(!(x != x));
            Assert.True((x != y));
            Assert.True(!(x == y));
        }

        [Fact]
        public void Inequalities()
        {
            Solver solver = Solver.CreateSolver("CLP");
            Variable x = solver.MakeNumVar(0.0, 100.0, "x");
            Assert.Equal(0.0, x.Lb());
            Assert.Equal(100.0, x.Ub());

            Variable y = solver.MakeNumVar(0.0, 100.0, "y");
            Assert.Equal(0.0, y.Lb());
            Assert.Equal(100.0, y.Ub());

            Constraint ct1 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) >= 3);
            Assert.Equal(7.0, ct1.GetCoefficient(x));
            Assert.Equal(5.0, ct1.GetCoefficient(y));
            Assert.Equal(2.0, ct1.Lb());
            Assert.Equal(double.PositiveInfinity, ct1.Ub());

            Constraint ct2 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) <= 3);
            Assert.Equal(7.0, ct2.GetCoefficient(x));
            Assert.Equal(5.0, ct2.GetCoefficient(y));
            Assert.Equal(double.NegativeInfinity, ct2.Lb());
            Assert.Equal(2.0, ct2.Ub());

            Constraint ct3 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) >= 3 - x - y);
            Assert.Equal(8.0, ct3.GetCoefficient(x));
            Assert.Equal(6.0, ct3.GetCoefficient(y));
            Assert.Equal(2.0, ct3.Lb());
            Assert.Equal(double.PositiveInfinity, ct3.Ub());

            Constraint ct4 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) <= -x - y + 3);
            Assert.Equal(8.0, ct4.GetCoefficient(x));
            Assert.Equal(6.0, ct4.GetCoefficient(y));
            Assert.Equal(double.NegativeInfinity, ct4.Lb());
            Assert.Equal(2.0, ct4.Ub());
        }

        [Fact]
        public void SumArray()
        {
            Solver solver = Solver.CreateSolver("CLP");

            Variable[] x = solver.MakeBoolVarArray(10, "x");
            Constraint ct1 = solver.Add(x.Sum() == 3);
            Assert.Equal(1.0, ct1.GetCoefficient(x[0]));

            Constraint ct2 = solver.Add(-2 * x.Sum() == 3);
            Assert.Equal(-2.0, ct2.GetCoefficient(x[0]));

            LinearExpr[] array = new LinearExpr[] { x[0] + 2.0, x[0] + 3, x[0] + 4 };
            Constraint ct3 = solver.Add(array.Sum() == 1);
            Assert.Equal(3.0, ct3.GetCoefficient(x[0]));
            Assert.Equal(-8.0, ct3.Lb());
            Assert.Equal(-8.0, ct3.Ub());
        }

        [Fact]
        public void Objective()
        {
            Solver solver = Solver.CreateSolver("CLP");
            Variable x = solver.MakeNumVar(0.0, 100.0, "x");
            Assert.Equal(0.0, x.Lb());
            Assert.Equal(100.0, x.Ub());

            Variable y = solver.MakeNumVar(0.0, 100.0, "y");
            Assert.Equal(0.0, y.Lb());
            Assert.Equal(100.0, y.Ub());

            solver.Maximize(x);
            Assert.Equal(0.0, solver.Objective().Offset());
            Assert.Equal(1.0, solver.Objective().GetCoefficient(x));
            Assert.True(solver.Objective().Maximization());

            solver.Minimize(-x - 2 * y + 3);
            Assert.Equal(3.0, solver.Objective().Offset());
            Assert.Equal(-1.0, solver.Objective().GetCoefficient(x));
            Assert.Equal(-2.0, solver.Objective().GetCoefficient(y));
            Assert.True(solver.Objective().Minimization());
        }

        void SolveAndPrint(in Solver solver, in Variable[] variables, in Constraint[] constraints)
        {
            Console.WriteLine($"Number of variables = {solver.NumVariables()}");
            Console.WriteLine($"Number of constraints = {solver.NumConstraints()}");

            Solver.ResultStatus resultStatus = solver.Solve();
            // Check that the problem has an optimal solution.
            if (resultStatus != Solver.ResultStatus.OPTIMAL)
            {
                Console.WriteLine("The problem does not have an optimal solution!");
            }
            else
            {
                Console.WriteLine("Solution:");
                foreach (Variable var in variables)
                {
                    Console.WriteLine($"{var.Name()} = {var.SolutionValue()}");
                }
                Console.WriteLine($"Optimal objective value = {solver.Objective().Value()}");
                Console.WriteLine("");
                Console.WriteLine("Advanced usage:");
                Console.WriteLine($"Problem solved in {solver.WallTime()} milliseconds");
                Console.WriteLine($"Problem solved in {solver.Iterations()} iterations");
                foreach (Variable var in variables)
                {
                    Console.WriteLine($"{var.Name()}: reduced cost {var.ReducedCost()}");
                }

                double[] activities = solver.ComputeConstraintActivities();
                foreach (Constraint ct in constraints)
                {
                    Console.WriteLine($"{ct.Name()}: dual value = {ct.DualValue()}",
                                      $" activity = {activities[ct.Index()]}");
                }
            }
        }

        void RunLinearProgrammingExample(in String problemType)
        {
            Console.WriteLine($"------ Linear programming example with {problemType} ------");

            Solver solver = Solver.CreateSolver(problemType);
            if (solver == null)
                return;

            // x and y are continuous non-negative variables.
            Variable x = solver.MakeNumVar(0.0, double.PositiveInfinity, "x");
            Variable y = solver.MakeNumVar(0.0, double.PositiveInfinity, "y");

            // Objectif function: Maximize 3x + 4y.
            Objective objective = solver.Objective();
            objective.SetCoefficient(x, 3);
            objective.SetCoefficient(y, 4);
            objective.SetMaximization();

            // x + 2y <= 14.
            Constraint c0 = solver.MakeConstraint(double.NegativeInfinity, 14.0, "c0");
            c0.SetCoefficient(x, 1);
            c0.SetCoefficient(y, 2);

            // 3x - y >= 0.
            Constraint c1 = solver.MakeConstraint(0.0, double.PositiveInfinity, "c1");
            c1.SetCoefficient(x, 3);
            c1.SetCoefficient(y, -1);

            // x - y <= 2.
            Constraint c2 = solver.MakeConstraint(double.NegativeInfinity, 2.0, "c2");
            c2.SetCoefficient(x, 1);
            c2.SetCoefficient(y, -1);

            SolveAndPrint(solver, new Variable[] { x, y }, new Constraint[] { c0, c1, c2 });
        }
        void RunMixedIntegerProgrammingExample(in String problemType)
        {
            Console.WriteLine($"------ Mixed integer programming example with {problemType} ------");

            Solver solver = Solver.CreateSolver(problemType);
            if (solver == null)
                return;

            // x and y are integers non-negative variables.
            Variable x = solver.MakeIntVar(0.0, double.PositiveInfinity, "x");
            Variable y = solver.MakeIntVar(0.0, double.PositiveInfinity, "y");

            // Objectif function: Maximize x + 10 * y.
            Objective objective = solver.Objective();
            objective.SetCoefficient(x, 1);
            objective.SetCoefficient(y, 10);
            objective.SetMaximization();

            // x + 7 * y <= 17.5.
            Constraint c0 = solver.MakeConstraint(double.NegativeInfinity, 17.5, "c0");
            c0.SetCoefficient(x, 1);
            c0.SetCoefficient(y, 7);

            // x <= 3.5.
            Constraint c1 = solver.MakeConstraint(double.NegativeInfinity, 3.5, "c1");
            c1.SetCoefficient(x, 1);
            c1.SetCoefficient(y, 0);

            SolveAndPrint(solver, new Variable[] { x, y }, new Constraint[] { c0, c1 });
        }
        void RunBooleanProgrammingExample(in String problemType)
        {
            Console.WriteLine($"------ Boolean programming example with {problemType} ------");

            Solver solver = Solver.CreateSolver(problemType);
            if (solver == null)
                return;

            // x and y are boolean variables.
            Variable x = solver.MakeBoolVar("x");
            Variable y = solver.MakeBoolVar("y");

            // Objectif function: Maximize 2 * x + y.
            Objective objective = solver.Objective();
            objective.SetCoefficient(x, 2);
            objective.SetCoefficient(y, 1);
            objective.SetMinimization();

            // 1 <= x + 2 * y <= 3.
            Constraint c0 = solver.MakeConstraint(1, 3, "c0");
            c0.SetCoefficient(x, 1);
            c0.SetCoefficient(y, 2);

            SolveAndPrint(solver, new Variable[] { x, y }, new Constraint[] { c0 });
        }

        [Fact]
        public void OptimizationProblemType()
        {
            RunLinearProgrammingExample("GLOP");
            RunLinearProgrammingExample("GLPK_LP");
            RunLinearProgrammingExample("CLP");
            RunLinearProgrammingExample("GUROBI_LP");

            RunMixedIntegerProgrammingExample("GLPK");
            RunMixedIntegerProgrammingExample("CBC");
            RunMixedIntegerProgrammingExample("SCIP");
            RunMixedIntegerProgrammingExample("SAT");

            RunBooleanProgrammingExample("SAT");
            RunBooleanProgrammingExample("BOP");
        }

        [Fact]
        static void testSetHintAndSolverGetters()
        {
            Console.WriteLine("testSetHintAndSolverGetters");
            Solver solver = Solver.CreateSolver("glop");
            // x and y are continuous non-negative variables.
            Variable x = solver.MakeIntVar(0.0, double.PositiveInfinity, "x");
            Variable y = solver.MakeIntVar(0.0, double.PositiveInfinity, "y");

            // Objectif function: Maximize x + 10 * y.
            Objective objective = solver.Objective();
            objective.SetCoefficient(x, 1);
            objective.SetCoefficient(y, 10);
            objective.SetMaximization();

            // x + 7 * y <= 17.5.
            Constraint c0 = solver.MakeConstraint(double.NegativeInfinity, 17.5, "c0");
            c0.SetCoefficient(x, 1);
            c0.SetCoefficient(y, 7);

            // x <= 3.5.
            Constraint c1 = solver.MakeConstraint(double.NegativeInfinity, 3.5, "c1");
            c1.SetCoefficient(x, 1);
            c1.SetCoefficient(y, 0);

            Constraint[] constraints = solver.constraints();
            Assert.Equal(constraints.Length, 2);
            Variable[] variables = solver.variables();
            Assert.Equal(variables.Length, 2);

            solver.SetHint(new Variable[] { x, y }, new double[] { 2.0, 3.0 });
        }
    }
} // namespace Google.OrTools.Tests
