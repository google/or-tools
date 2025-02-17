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

using System;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.Sat;

/// <summary>
/// Fill a 60x50 rectangle exactly using a minimum number of non-overlapping squares."""
/// </summary>
class CoverRectangleSat
{
    static int sizeX = 60;
    static int sizeY = 50;

    static bool CoverRectangle(int numSquares)
    {
        CpModel model = new CpModel();

        var areas = new List<IntVar>();
        var sizes = new List<IntVar>();
        var xIntervals = new List<IntervalVar>();
        var yIntervals = new List<IntervalVar>();
        var xStarts = new List<IntVar>();
        var yStarts = new List<IntVar>();

        // Creates intervals for the NoOverlap2D and size variables.
        foreach (var i in Enumerable.Range(0, numSquares))
        {
            var size = model.NewIntVar(1, sizeY, String.Format("size_{0}", i));
            var startX = model.NewIntVar(0, sizeX, String.Format("startX_{0}", i));
            var endX = model.NewIntVar(0, sizeX, String.Format("endX_{0}", i));
            var startY = model.NewIntVar(0, sizeY, String.Format("startY_{0}", i));
            var endY = model.NewIntVar(0, sizeY, String.Format("endY_{0}", i));

            var intervalX = model.NewIntervalVar(startX, size, endX, String.Format("intervalX_{0}", i));
            var intervalY = model.NewIntervalVar(startY, size, endY, String.Format("intervalY_{0}", i));

            var area = model.NewIntVar(1, sizeY * sizeY, String.Format("area_{0}", i));
            model.AddMultiplicationEquality(area, size, size);

            areas.Add(area);
            xIntervals.Add(intervalX);
            yIntervals.Add(intervalY);
            sizes.Add(size);
            xStarts.Add(startX);
            yStarts.Add(startY);
        }

        // Main constraint.
        NoOverlap2dConstraint noOverlap2d = model.AddNoOverlap2D();
        foreach (var i in Enumerable.Range(0, numSquares))
        {
            noOverlap2d.AddRectangle(xIntervals[i], yIntervals[i]);
        }

        // Redundant constraints.
        model.AddCumulative(sizeY).AddDemands(xIntervals, sizes);
        model.AddCumulative(sizeX).AddDemands(yIntervals, sizes);

        // Forces the rectangle to be exactly covered.
        model.Add(LinearExpr.Sum(areas) == sizeX * sizeY);

        // Symmetry breaking 1: sizes are ordered.
        foreach (var i in Enumerable.Range(0, numSquares - 1))
        {
            model.Add(sizes[i] <= sizes[i + 1]);

            //  Define same to be true iff sizes[i] == sizes[i + 1]
            var same = model.NewBoolVar("");
            model.Add(sizes[i] == sizes[i + 1]).OnlyEnforceIf(same);
            model.Add(sizes[i] < sizes[i + 1]).OnlyEnforceIf(same.Not());

            // Tie break with starts.
            model.Add(xStarts[i] <= xStarts[i + 1]).OnlyEnforceIf(same);
        }

        // Symmetry breaking 2: first square in one quadrant.
        model.Add(xStarts[0] < (sizeX + 1) / 2);
        model.Add(yStarts[0] < (sizeY + 1) / 2);

        // Creates a solver and solves.
        var solver = new CpSolver();
        solver.StringParameters = "num_search_workers:16, log_search_progress: false, max_time_in_seconds:10";
        var status = solver.Solve(model);
        Console.WriteLine(string.Format("{0} found in {1:0.00}s", status, solver.WallTime()));

        // Prints solution.
        bool solution_found = status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible;
        if (solution_found)
        {
            char[][] output = new char[sizeY][];
            foreach (var y in Enumerable.Range(0, sizeY))
            {

                output[y] = new char[sizeX];
                foreach (var x in Enumerable.Range(0, sizeX))
                {
                    output[y][x] = ' ';
                }
            }

            foreach (var s in Enumerable.Range(0, numSquares))
            {
                int startX = (int)solver.Value(xStarts[s]);
                int startY = (int)solver.Value(yStarts[s]);
                int size = (int)solver.Value(sizes[s]);
                char c = (char)(65 + s);
                foreach (var x in Enumerable.Range(startX, size))
                {
                    foreach (var y in Enumerable.Range(startY, size))
                    {
                        if (output[y][x] != ' ')
                        {
                            Console.WriteLine(string.Format("Error at position x={0} y{1}, found {2}", x, y, output[y][x]));
                        }
                        output[y][x] = c;
                    }
                }
            }
            foreach (var y in Enumerable.Range(0, sizeY))
            {
                Console.WriteLine(new String(output[y], 0, sizeX));
            }
        }
        return solution_found;
    }

    static void Main()
    {
        foreach  (int numSquares in Enumerable.Range(1, 15))
        {
            Console.WriteLine("Trying with size = {0}", numSquares);
            if (CoverRectangle(numSquares)) break;
        }
    }    
}
