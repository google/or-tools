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
using System.Collections.Generic;
using Google.OrTools.Sat;

public class RankingSampleSat
{
    static void RankTasks(CpModel model, IntVar[] starts, ILiteral[] presences, IntVar[] ranks)
    {
        int num_tasks = starts.Length;

        // Creates precedence variables between pairs of intervals.
        ILiteral[,] precedences = new ILiteral[num_tasks, num_tasks];
        for (int i = 0; i < num_tasks; ++i)
        {
            for (int j = 0; j < num_tasks; ++j)
            {
                if (i == j)
                {
                    precedences[i, i] = presences[i];
                }
                else
                {
                    IntVar prec = model.NewBoolVar(String.Format("{0} before {1}", i, j));
                    precedences[i, j] = prec;
                    model.Add(starts[i] < starts[j]).OnlyEnforceIf(prec);
                }
            }
        }

        // Treats optional intervals.
        for (int i = 0; i < num_tasks - 1; ++i)
        {
            for (int j = i + 1; j < num_tasks; ++j)
            {
                List<ILiteral> tmp_array = new List<ILiteral>();
                tmp_array.Add(precedences[i, j]);
                tmp_array.Add(precedences[j, i]);
                tmp_array.Add(presences[i].Not());
                // Makes sure that if i is not performed, all precedences are false.
                model.AddImplication(presences[i].Not(), precedences[i, j].Not());
                model.AddImplication(presences[i].Not(), precedences[j, i].Not());
                tmp_array.Add(presences[j].Not());
                // Makes sure that if j is not performed, all precedences are false.
                model.AddImplication(presences[j].Not(), precedences[i, j].Not());
                model.AddImplication(presences[j].Not(), precedences[j, i].Not());
                // The following bool_or will enforce that for any two intervals:
                //    i precedes j or j precedes i or at least one interval is not
                //        performed.
                model.AddBoolOr(tmp_array);
                // Redundant constraint: it propagates early that at most one precedence
                // is true.
                model.AddImplication(precedences[i, j], precedences[j, i].Not());
                model.AddImplication(precedences[j, i], precedences[i, j].Not());
            }
        }

        // Links precedences and ranks.
        for (int i = 0; i < num_tasks; ++i)
        {
            IntVar[] tmp_array = new IntVar[num_tasks];
            for (int j = 0; j < num_tasks; ++j)
            {
                tmp_array[j] = (IntVar)precedences[j, i];
            }
            model.Add(ranks[i] == LinearExpr.Sum(tmp_array) - 1);
        }
    }

    static void Main()
    {
        CpModel model = new CpModel();
        // Three weeks.
        int horizon = 100;
        int num_tasks = 4;

        IntVar[] starts = new IntVar[num_tasks];
        IntVar[] ends = new IntVar[num_tasks];
        IntervalVar[] intervals = new IntervalVar[num_tasks];
        ILiteral[] presences = new ILiteral[num_tasks];
        IntVar[] ranks = new IntVar[num_tasks];

        IntVar true_var = model.NewConstant(1);

        // Creates intervals, half of them are optional.
        for (int t = 0; t < num_tasks; ++t)
        {
            starts[t] = model.NewIntVar(0, horizon, String.Format("start_{0}", t));
            int duration = t + 1;
            ends[t] = model.NewIntVar(0, horizon, String.Format("end_{0}", t));
            if (t < num_tasks / 2)
            {
                intervals[t] = model.NewIntervalVar(starts[t], duration, ends[t], String.Format("interval_{0}", t));
                presences[t] = true_var;
            }
            else
            {
                presences[t] = model.NewBoolVar(String.Format("presence_{0}", t));
                intervals[t] = model.NewOptionalIntervalVar(starts[t], duration, ends[t], presences[t],
                                                            String.Format("o_interval_{0}", t));
            }

            // Ranks = -1 if and only if the tasks is not performed.
            ranks[t] = model.NewIntVar(-1, num_tasks - 1, String.Format("rank_{0}", t));
        }

        // Adds NoOverlap constraint.
        model.AddNoOverlap(intervals);

        // Adds ranking constraint.
        RankTasks(model, starts, presences, ranks);

        // Adds a constraint on ranks.
        model.Add(ranks[0] < ranks[1]);

        // Creates makespan variable.
        IntVar makespan = model.NewIntVar(0, horizon, "makespan");
        for (int t = 0; t < num_tasks; ++t)
        {
            model.Add(ends[t] <= makespan).OnlyEnforceIf(presences[t]);
        }
        // Minimizes makespan - fixed gain per tasks performed.
        // As the fixed cost is less that the duration of the last interval,
        // the solver will not perform the last interval.
        IntVar[] presences_as_int_vars = new IntVar[num_tasks];
        for (int t = 0; t < num_tasks; ++t)
        {
            presences_as_int_vars[t] = (IntVar)presences[t];
        }
        model.Minimize(2 * makespan - 7 * LinearExpr.Sum(presences_as_int_vars));

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine(String.Format("Optimal cost: {0}", solver.ObjectiveValue));
            Console.WriteLine(String.Format("Makespan: {0}", solver.Value(makespan)));
            for (int t = 0; t < num_tasks; ++t)
            {
                if (solver.BooleanValue(presences[t]))
                {
                    Console.WriteLine(String.Format("Task {0} starts at {1} with rank {2}", t, solver.Value(starts[t]),
                                                    solver.Value(ranks[t])));
                }
                else
                {
                    Console.WriteLine(
                        String.Format("Task {0} in not performed and ranked at {1}", t, solver.Value(ranks[t])));
                }
            }
        }
        else
        {
            Console.WriteLine(String.Format("Solver exited with nonoptimal status: {0}", status));
        }
    }
}
