//
// Copyright 2012 Hakan Kjellerstrand
//
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
using System.Collections;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class BusSchedule
{
    /**
     *
     * Bus scheduling.
     *
     * Minimize number of buses in timeslots.
     *
     * Problem from Taha "Introduction to Operations Research", page 58.
     *
     * This is a slightly more general model than Taha's.
     *
     * Also see, http://www.hakank.org/or-tools/bus_schedule.py
     *
     */
    private static long Solve(long num_buses_check = 0)
    {
        Solver solver = new Solver("BusSchedule");

        //
        // data
        //
        int time_slots = 6;
        int[] demands = { 8, 10, 7, 12, 4, 4 };
        int max_num = demands.Sum();

        //
        // Decision variables
        //

        // How many buses start the schedule at time slot t
        IntVar[] x = solver.MakeIntVarArray(time_slots, 0, max_num, "x");
        // Total number of buses
        IntVar num_buses = x.Sum().VarWithName("num_buses");

        //
        // Constraints
        //

        // Meet the demands for this and the next time slot.
        for (int i = 0; i < time_slots - 1; i++)
        {
            solver.Add(x[i] + x[i + 1] >= demands[i]);
        }

        // The demand "around the clock"
        solver.Add(x[time_slots - 1] + x[0] - demands[time_slots - 1] == 0);

        // For showing all solutions of minimal number of buses
        if (num_buses_check > 0)
        {
            solver.Add(num_buses == num_buses_check);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        if (num_buses_check == 0)
        {
            // Minimize num_buses
            OptimizeVar obj = num_buses.Minimize(1);
            solver.NewSearch(db, obj);
        }
        else
        {
            solver.NewSearch(db);
        }

        long result = 0;
        while (solver.NextSolution())
        {
            result = num_buses.Value();
            Console.Write("x: ");
            for (int i = 0; i < time_slots; i++)
            {
                Console.Write("{0,2} ", x[i].Value());
            }
            Console.WriteLine("num_buses: " + num_buses.Value());
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();

        return result;
    }

    public static void Main(String[] args)
    {
        Console.WriteLine("Check for minimum number of buses: ");
        long num_buses = Solve();
        Console.WriteLine("\n... got {0} as minimal value.", num_buses);
        Console.WriteLine("\nAll solutions: ", num_buses);
        num_buses = Solve(num_buses);
    }
}
