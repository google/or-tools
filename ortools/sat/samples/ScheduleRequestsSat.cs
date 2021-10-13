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

// [START program]
// [START import]
using System;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.Sat;
// [END import]

public class ScheduleRequestsSat
{
    public static void Main(String[] args)
    {
        // [START data]
        const int numNurses = 5;
        const int numDays = 7;
        const int numShifts = 3;

        int[] allNurses = Enumerable.Range(0, numNurses).ToArray();
        int[] allDays = Enumerable.Range(0, numDays).ToArray();
        int[] allShifts = Enumerable.Range(0, numShifts).ToArray();

        int[,,] shiftRequests = new int[,,] {
            {
                { 0, 0, 1 },
                { 0, 0, 0 },
                { 0, 0, 0 },
                { 0, 0, 0 },
                { 0, 0, 1 },
                { 0, 1, 0 },
                { 0, 0, 1 },
            },
            {
                { 0, 0, 0 },
                { 0, 0, 0 },
                { 0, 1, 0 },
                { 0, 1, 0 },
                { 1, 0, 0 },
                { 0, 0, 0 },
                { 0, 0, 1 },
            },
            {
                { 0, 1, 0 },
                { 0, 1, 0 },
                { 0, 0, 0 },
                { 1, 0, 0 },
                { 0, 0, 0 },
                { 0, 1, 0 },
                { 0, 0, 0 },
            },
            {
                { 0, 0, 1 },
                { 0, 0, 0 },
                { 1, 0, 0 },
                { 0, 1, 0 },
                { 0, 0, 0 },
                { 1, 0, 0 },
                { 0, 0, 0 },
            },
            {
                { 0, 0, 0 },
                { 0, 0, 1 },
                { 0, 1, 0 },
                { 0, 0, 0 },
                { 1, 0, 0 },
                { 0, 1, 0 },
                { 0, 0, 0 },
            },
        };
        // [END data]

        // Creates the model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Creates shift variables.
        // shifts[(n, d, s)]: nurse 'n' works shift 's' on day 'd'.
        // [START variables]
        Dictionary<Tuple<int, int, int>, IntVar> shifts = new Dictionary<Tuple<int, int, int>, IntVar>();
        foreach (int n in allNurses)
        {
            foreach (int d in allDays)
            {
                foreach (int s in allShifts)
                {
                    shifts.Add(Tuple.Create(n, d, s), model.NewBoolVar($"shifts_n{n}d{d}s{s}"));
                }
            }
        }
        // [END variables]

        // Each shift is assigned to exactly one nurse in the schedule period.
        // [START exactly_one_nurse]
        foreach (int d in allDays)
        {
            foreach (int s in allShifts)
            {
                IntVar[] x = new IntVar[numNurses];
                foreach (int n in allNurses)
                {
                    var key = Tuple.Create(n, d, s);
                    x[n] = shifts[key];
                }
                model.Add(LinearExpr.Sum(x) == 1);
            }
        }
        // [END exactly_one_nurse]

        // Each nurse works at most one shift per day.
        // [START at_most_one_shift]
        foreach (int n in allNurses)
        {
            foreach (int d in allDays)
            {
                IntVar[] x = new IntVar[numShifts];
                foreach (int s in allShifts)
                {
                    var key = Tuple.Create(n, d, s);
                    x[s] = shifts[key];
                }
                model.Add(LinearExpr.Sum(x) <= 1);
            }
        }
        // [END at_most_one_shift]

        // [START assign_nurses_evenly]
        // Try to distribute the shifts evenly, so that each nurse works
        // minShiftsPerNurse shifts. If this is not possible, because the total
        // number of shifts is not divisible by the number of nurses, some nurses will
        // be assigned one more shift.
        int minShiftsPerNurse = (numShifts * numDays) / numNurses;
        int maxShiftsPerNurse;
        if ((numShifts * numDays) % numNurses == 0)
        {
            maxShiftsPerNurse = minShiftsPerNurse;
        }
        else
        {
            maxShiftsPerNurse = minShiftsPerNurse + 1;
        }
        foreach (int n in allNurses)
        {
            IntVar[] numShiftsWorked = new IntVar[numDays * numShifts];
            foreach (int d in allDays)
            {
                foreach (int s in allShifts)
                {
                    var key = Tuple.Create(n, d, s);
                    numShiftsWorked[d * numShifts + s] = shifts[key];
                }
            }
            model.AddLinearConstraint(LinearExpr.Sum(numShiftsWorked), minShiftsPerNurse, maxShiftsPerNurse);
        }
        // [END assign_nurses_evenly]

        // [START objective]
        IntVar[] flatShifts = new IntVar[numNurses * numDays * numShifts];
        int[] flatShiftRequests = new int[numNurses * numDays * numShifts];
        foreach (int n in allNurses)
        {
            foreach (int d in allDays)
            {
                foreach (int s in allShifts)
                {
                    var key = Tuple.Create(n, d, s);
                    flatShifts[n * numDays * numShifts + d * numShifts + s] = shifts[key];
                    flatShiftRequests[n * numDays * numShifts + d * numShifts + s] = shiftRequests[n, d, s];
                }
            }
        }
        model.Maximize(LinearExpr.ScalProd(flatShifts, flatShiftRequests));
        // [END objective]

        // Solve
        // [START solve]
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Console.WriteLine($"Solve status: {status}");
        // [END solve]

        // [START print_solution]
        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine("Solution:");
            foreach (int d in allDays)
            {
                Console.WriteLine($"Day {d}");
                foreach (int n in allNurses)
                {
                    bool isWorking = false;
                    foreach (int s in allShifts)
                    {
                        var key = Tuple.Create(n, d, s);
                        if (solver.Value(shifts[key]) == 1L)
                        {
                            if (shiftRequests[n, d, s] == 1)
                            {
                                Console.WriteLine($"  Nurse {n} work shift {s} (requested).");
                            }
                            else
                            {
                                Console.WriteLine($"  Nurse {n} work shift {s} (not requested).");
                            }
                        }
                    }
                }
            }
            Console.WriteLine(
                $"Number of shift requests met = {solver.ObjectiveValue} (out of {numNurses * minShiftsPerNurse}).");
        }
        else
        {
            Console.WriteLine("No solution found.");
        }
        // [END print_solution]

        // [START statistics]
        Console.WriteLine("Statistics");
        Console.WriteLine($"  conflicts: {solver.NumConflicts()}");
        Console.WriteLine($"  branches : {solver.NumBranches()}");
        Console.WriteLine($"  wall time: {solver.WallTime()}s");
        // [END statistics]
    }
}
// [END program]
