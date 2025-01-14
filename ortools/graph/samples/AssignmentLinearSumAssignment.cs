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
using System.Linq;
using Google.OrTools.Graph;
// [END import]

public class AssignmentLinearSumAssignment
{
    static void Main()
    {
        // [START solver]
        LinearSumAssignment assignment = new LinearSumAssignment();
        // [END solver]

        // [START data]
        int[,] costs = {
            { 90, 76, 75, 70 },
            { 35, 85, 55, 65 },
            { 125, 95, 90, 105 },
            { 45, 110, 95, 115 },
        };
        int numWorkers = 4;
        int[] allWorkers = Enumerable.Range(0, numWorkers).ToArray();
        int numTasks = 4;
        int[] allTasks = Enumerable.Range(0, numTasks).ToArray();
        // [END data]

        // [START constraints]
        // Add each arc.
        foreach (int w in allWorkers)
        {
            foreach (int t in allTasks)
            {
                if (costs[w, t] != 0)
                {
                    assignment.AddArcWithCost(w, t, costs[w, t]);
                }
            }
        }
        // [END constraints]

        // [START solve]
        LinearSumAssignment.Status status = assignment.Solve();
        // [END solve]

        // [START print_solution]
        if (status == LinearSumAssignment.Status.OPTIMAL)
        {
            Console.WriteLine($"Total cost: {assignment.OptimalCost()}.");
            foreach (int worker in allWorkers)
            {
                Console.WriteLine($"Worker {worker} assigned to task {assignment.RightMate(worker)}. " +
                                  $"Cost: {assignment.AssignmentCost(worker)}.");
            }
        }
        else
        {
            Console.WriteLine("Solving the linear assignment problem failed.");
            Console.WriteLine($"Solver status: {status}.");
        }
        // [END print_solution]
    }
}
// [END program]
