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
using System.Linq;
using Google.OrTools.Sat;

public class NurseSolutionObserver : CpSolverSolutionCallback
{
    public NurseSolutionObserver(IntVar[,,] shifts, int num_nurses, int num_days, int num_shifts, HashSet<int> to_print,
                                 int last_solution_explored)
    {
        shifts_ = shifts;
        num_nurses_ = num_nurses;
        num_days_ = num_days;
        num_shifts_ = num_shifts;
        to_print_ = to_print;
        last_solution_explored_ = last_solution_explored;
    }

    public override void OnSolutionCallback()
    {
        solution_count_++;
        if (to_print_.Contains(solution_count_))
        {
            Console.WriteLine(String.Format("Solution #{0}: time = {1:.02} s", solution_count_, WallTime()));
            for (int d = 0; d < num_days_; ++d)
            {
                Console.WriteLine(String.Format("Day #{0}", d));
                for (int n = 0; n < num_nurses_; ++n)
                {
                    for (int s = 0; s < num_shifts_; ++s)
                    {
                        if (BooleanValue(shifts_[n, d, s]))
                        {
                            Console.WriteLine(String.Format("  Nurse #{0} is working shift #{1}", n, s));
                        }
                    }
                }
            }
        }
        if (solution_count_ >= last_solution_explored_)
        {
            StopSearch();
        }
    }

    public int SolutionCount()
    {
        return solution_count_;
    }

    private int solution_count_;
    private IntVar[,,] shifts_;
    private int num_nurses_;
    private int num_days_;
    private int num_shifts_;
    private HashSet<int> to_print_;
    private int last_solution_explored_;
}

public class NursesSat
{
    static void Main()
    {
        // Data.
        int num_nurses = 4;
        // Nurse assigned to shift 0 means not working that day.
        int num_shifts = 4;
        int num_days = 7;

        var all_nurses = Enumerable.Range(0, num_nurses);
        var all_shifts = Enumerable.Range(0, num_shifts);
        var all_working_shifts = Enumerable.Range(1, num_shifts - 1);
        var all_days = Enumerable.Range(0, num_days);

        // Creates the model.
        CpModel model = new CpModel();

        // Creates shift variables.
        // shift[n, d, s]: nurse "n" works shift "s" on day "d".
        IntVar[,,] shift = new IntVar[num_nurses, num_days, num_shifts];
        foreach (int n in all_nurses)
        {
            foreach (int d in all_days)
            {
                foreach (int s in all_shifts)
                {
                    shift[n, d, s] = model.NewBoolVar(String.Format("shift_n{0}d{1}s{2}", n, d, s));
                }
            }
        }

        // Makes assignments different on each day, that is each shift is
        // assigned at most one nurse. As we have the same number of
        // nurses and shifts, then each day, each shift is assigned to
        // exactly one nurse.
        foreach (int d in all_days)
        {
            foreach (int s in all_shifts)
            {
                IntVar[] tmp = new IntVar[num_nurses];
                foreach (int n in all_nurses)
                {
                    tmp[n] = shift[n, d, s];
                }
                model.Add(LinearExpr.Sum(tmp) == 1);
            }
        }

        // Nurses do 1 shift per day.
        foreach (int n in all_nurses)
        {
            foreach (int d in all_days)
            {
                IntVar[] tmp = new IntVar[num_shifts];
                foreach (int s in all_shifts)
                {
                    tmp[s] = shift[n, d, s];
                }
                model.Add(LinearExpr.Sum(tmp) == 1);
            }
        }

        // Each nurse works 5 or 6 days in a week.
        // That is each nurse works shift 0 at most 2 times.
        foreach (int n in all_nurses)
        {
            IntVar[] tmp = new IntVar[num_days];
            foreach (int d in all_days)
            {
                tmp[d] = shift[n, d, 0];
            }
            model.AddLinearConstraint(LinearExpr.Sum(tmp), 1, 2);
        }

        // works_shift[(n, s)] is 1 if nurse n works shift s at least one day in
        // the week.
        IntVar[,] works_shift = new IntVar[num_nurses, num_shifts];
        foreach (int n in all_nurses)
        {
            foreach (int s in all_shifts)
            {
                works_shift[n, s] = model.NewBoolVar(String.Format("works_shift_n{0}s{1}", n, s));
                IntVar[] tmp = new IntVar[num_days];
                foreach (int d in all_days)
                {
                    tmp[d] = shift[n, d, s];
                }
                model.AddMaxEquality(works_shift[n, s], tmp);
            }
        }

        // For each working shift, at most 2 nurses are assigned to that shift
        // during the week.
        foreach (int s in all_working_shifts)
        {
            IntVar[] tmp = new IntVar[num_nurses];
            foreach (int n in all_nurses)
            {
                tmp[n] = works_shift[n, s];
            }
            model.Add(LinearExpr.Sum(tmp) <= 2);
        }

        // If a nurse works shifts 2 or 3 on, she must also work that
        // shift the previous day or the following day.  This means that
        // on a given day and shift, either she does not work that shift
        // on that day, or she works that shift on the day before, or the
        // day after.
        foreach (int n in all_nurses)
        {
            for (int s = 2; s <= 3; ++s)
            {
                foreach (int d in all_days)
                {
                    int yesterday = d == 0 ? num_days - 1 : d - 1;
                    int tomorrow = d == num_days - 1 ? 0 : d + 1;
                    model.AddBoolOr(
                        new ILiteral[] { shift[n, yesterday, s], shift[n, d, s].Not(), shift[n, tomorrow, s] });
                }
            }
        }

        // Creates the solver and solve.
        CpSolver solver = new CpSolver();
        // Display a few solutions picked at random.
        HashSet<int> to_print = new HashSet<int>();
        to_print.Add(200);
        to_print.Add(410);
        to_print.Add(650);
        NurseSolutionObserver cb = new NurseSolutionObserver(shift, num_nurses, num_days, num_shifts, to_print, 650);
        solver.StringParameters = "linearization_level:2 enumerate_all_solutions:true";
        CpSolverStatus status = solver.Solve(model, cb);

        // Statistics.
        Console.WriteLine("Statistics");
        Console.WriteLine(String.Format("  - solve status    : {0}", status));
        Console.WriteLine("  - conflicts       : " + solver.NumConflicts());
        Console.WriteLine("  - branches        : " + solver.NumBranches());
        Console.WriteLine("  - wall time       : " + solver.WallTime() + " ms");
        Console.WriteLine("  - #solutions      : " + cb.SolutionCount());
    }
}
