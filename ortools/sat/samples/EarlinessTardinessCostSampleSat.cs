// Copyright 2010-2022 Google LLC
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
using Google.OrTools.Sat;
using Google.OrTools.Util;

public class VarArraySolutionPrinter : CpSolverSolutionCallback
{
    public VarArraySolutionPrinter(IntVar[] variables)
    {
        variables_ = variables;
    }

    public override void OnSolutionCallback()
    {
        {
            foreach (IntVar v in variables_)
            {
                Console.Write(String.Format("{0}={1} ", v.ToString(), Value(v)));
            }
            Console.WriteLine();
        }
    }

    private IntVar[] variables_;
}

public class EarlinessTardinessCostSampleSat
{
    static void Main()
    {
        long earliness_date = 5;
        long earliness_cost = 8;
        long lateness_date = 15;
        long lateness_cost = 12;

        // Create the CP-SAT model.
        CpModel model = new CpModel();

        // Declare our primary variable.
        IntVar x = model.NewIntVar(0, 20, "x");

        // Create the expression variable and implement the piecewise linear
        // function.
        //
        //  \        /
        //   \______/
        //   ed    ld
        //
        long large_constant = 1000;
        IntVar expr = model.NewIntVar(0, large_constant, "expr");

        // Link together expr and x through s1, s2, and s3.
        model.AddMaxEquality(expr, new LinearExpr[] { earliness_cost * (earliness_date - x), model.NewConstant(0),
                                                      lateness_cost * (x - lateness_date) });

        // Search for x values in increasing order.
        model.AddDecisionStrategy(new IntVar[] { x }, DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
                                  DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

        // Create the solver.
        CpSolver solver = new CpSolver();

        // Force solver to follow the decision strategy exactly.
        // Tell the solver to search for all solutions.
        solver.StringParameters = "search_branching:FIXED_SEARCH, enumerate_all_solutions:true";

        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] { x, expr });
        solver.Solve(model, cb);
    }
}
