// Copyright 2010-2025 Google LLC
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
using System.IO;
using System.Linq;
using Google.OrTools.Sat;
// [END import]

public class NursesSat
{
    // [START solution_printer]
    public class SolutionPrinter : CpSolverSolutionCallback
    {
        public SolutionPrinter(int[] allNurses, int[] allDays, int[] allShifts,
                               Dictionary<(int, int, int), BoolVar> shifts, int limit)
        {
            solutionCount_ = 0;
            allNurses_ = allNurses;
            allDays_ = allDays;
            allShifts_ = allShifts;
            shifts_ = shifts;
            solutionLimit_ = limit;
        }

        public override void OnSolutionCallback()
        {
            Console.WriteLine($"Solution #{solutionCount_}:");
            foreach (int d in allDays_)
            {
                Console.WriteLine($"Day {d}");
                foreach (int n in allNurses_)
                {
                    bool isWorking = false;
                    foreach (int s in allShifts_)
                    {
                        if (Value(shifts_[(n, d, s)]) == 1L)
                        {
                            isWorking = true;
                            Console.WriteLine($"  Nurse {n} work shift {s}");
                        }
                    }
                    if (!isWorking)
                    {
                        Console.WriteLine($"  Nurse {d} does not work");
                    }
                }
            }
            solutionCount_++;
            if (solutionCount_ >= solutionLimit_)
            {
                Console.WriteLine($"Stop search after {solutionLimit_} solutions");
                StopSearch();
            }
        }

        public int SolutionCount()
        {
            return solutionCount_;
        }

        private int solutionCount_;
        private int[] allNurses_;
        private int[] allDays_;
        private int[] allShifts_;
        private Dictionary<(int, int, int), BoolVar> shifts_;
        private int solutionLimit_;
    }
    // [END solution_printer]

    public static void Main(String[] args)
    {
        // [START data]
        const int numNurses = 4;
        const int numDays = 3;
        const int numShifts = 3;

        int[] allNurses = Enumerable.Range(0, numNurses).ToArray();
        int[] allDays = Enumerable.Range(0, numDays).ToArray();
        int[] allShifts = Enumerable.Range(0, numShifts).ToArray();
        // [END data]

        // Creates the model.
        // [START model]
        CpModel model = new CpModel();
        model.Model.Variables.Capacity = numNurses * numDays * numShifts;
        // [END model]

        // Creates shift variables.
        // shifts[(n, d, s)]: nurse 'n' works shift 's' on day 'd'.
        // [START variables]
        Dictionary<(int, int, int), BoolVar> shifts =
            new Dictionary<(int, int, int), BoolVar>(numNurses * numDays * numShifts);
        foreach (int n in allNurses)
        {
            foreach (int d in allDays)
            {
                foreach (int s in allShifts)
                {
                    shifts.Add((n, d, s), model.NewBoolVar($"shifts_n{n}d{d}s{s}"));
                }
            }
        }
        // [END variables]

        // Each shift is assigned to exactly one nurse in the schedule period.
        // [START exactly_one_nurse]
        List<ILiteral> literals = new List<ILiteral>();
        foreach (int d in allDays)
        {
            foreach (int s in allShifts)
            {
                foreach (int n in allNurses)
                {
                    literals.Add(shifts[(n, d, s)]);
                }
                model.AddExactlyOne(literals);
                literals.Clear();
            }
        }
        // [END exactly_one_nurse]

        // Each nurse works at most one shift per day.
        // [START at_most_one_shift]
        foreach (int n in allNurses)
        {
            foreach (int d in allDays)
            {
                foreach (int s in allShifts)
                {
                    literals.Add(shifts[(n, d, s)]);
                }
                model.AddAtMostOne(literals);
                literals.Clear();
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

        List<IntVar> shiftsWorked = new List<IntVar>();
        foreach (int n in allNurses)
        {
            foreach (int d in allDays)
            {
                foreach (int s in allShifts)
                {
                    shiftsWorked.Add(shifts[(n, d, s)]);
                }
            }
            model.AddLinearConstraint(LinearExpr.Sum(shiftsWorked), minShiftsPerNurse, maxShiftsPerNurse);
            shiftsWorked.Clear();
        }
        // [END assign_nurses_evenly]

        // [START parameters]
        CpSolver solver = new CpSolver();
        // Tell the solver to enumerate all solutions.
        solver.StringParameters += "linearization_level:0 " + "enumerate_all_solutions:true ";
        // [END parameters]

        // Display the first five solutions.
        // [START solution_printer_instantiate]
        const int solutionLimit = 5;
        SolutionPrinter cb = new SolutionPrinter(allNurses, allDays, allShifts, shifts, solutionLimit);
        // [END solution_printer_instantiate]

        // Solve
        // [START solve]
        CpSolverStatus status = solver.Solve(model, cb);
        Console.WriteLine($"Solve status: {status}");
        // [END solve]

        // [START statistics]
        Console.WriteLine("Statistics");
        Console.WriteLine($"  conflicts: {solver.NumConflicts()}");
        Console.WriteLine($"  branches : {solver.NumBranches()}");
        Console.WriteLine($"  wall time: {solver.WallTime()}s");
        // [END statistics]
    }
}
// [END program]
