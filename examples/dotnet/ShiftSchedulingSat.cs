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

/// <summary>
/// Creates a shift scheduling problem and solves it
/// </summary>
public class ShiftSchedulingSat
{
    static void Main(string[] args)
    {
        SolveShiftScheduling();
    }

    static void SolveShiftScheduling()
    {
        int numEmployees = 8;
        int numWeeks = 3;
        var shifts = new[] { "O", "M", "A", "N" };

        // Fixed assignment: (employee, shift, day).
        // This fixes the first 2 days of the schedule.
        var fixedAssignments = new(int Employee, int Shift, int Day)[] {
            (0, 0, 0), (1, 0, 0), (2, 1, 0), (3, 1, 0), (4, 2, 0), (5, 2, 0), (6, 2, 3), (7, 3, 0),
            (0, 1, 1), (1, 1, 1), (2, 2, 1), (3, 2, 1), (4, 2, 1), (5, 0, 1), (6, 0, 1), (7, 3, 1),
        };

        // Request: (employee, shift, day, weight)
        // A negative weight indicates that the employee desire this assignment.
        var requests = new(int Employee, int Shift, int Day,
                           int Weight)[] {// Employee 3 wants the first Saturday off.
                                          (3, 0, 5, -2),
                                          // Employee 5 wants a night shift on the second Thursday.
                                          (5, 3, 10, -2),
                                          // Employee 2 does not want a night shift on the first Friday.
                                          (2, 3, 4, 4)
        };

        // Shift constraints on continuous sequence :
        //     (shift, hard_min, soft_min, min_penalty,
        // soft_max, hard_max, max_penalty)
        var shiftConstraints =
            new(int Shift, int HardMin, int SoftMin, int MinPenalty, int SoftMax, int HardMax, int MaxPenalty)[] {
                // One or two consecutive days of rest, this is a hard constraint.
                (0, 1, 1, 0, 2, 2, 0),
                // Between 2 and 3 consecutive days of night shifts, 1 and 4 are
                // possible but penalized.
                (3, 1, 2, 20, 3, 4, 5),
            };

        // Weekly sum constraints on shifts days:
        //     (shift, hardMin, softMin, minPenalty,
        // softMax, hardMax, maxPenalty)
        var weeklySumConstraints =
            new(int Shift, int HardMin, int SoftMin, int MinPenalty, int SoftMax, int HardMax, int MaxPenalty)[] {
                // Constraints on rests per week.
                (0, 1, 2, 7, 2, 3, 4),
                // At least 1 night shift per week (penalized). At most 4 (hard).
                (3, 0, 1, 3, 4, 4, 0),
            };

        // Penalized transitions:
        //     (previous_shift, next_shift, penalty (0 means forbidden))
        var penalizedTransitions = new(int PreviousShift, int NextShift, int Penalty)[] {
            // Afternoon to night has a penalty of 4.
            (2, 3, 4),
            // Night to morning is forbidden.
            (3, 1, 0),
        };

        // daily demands for work shifts (morning, afternon, night) for each day
        // of the week starting on Monday.
        var weeklyCoverDemands = new int[][] {
            new[] { 2, 3, 1 }, // Monday
            new[] { 2, 3, 1 }, // Tuesday
            new[] { 2, 2, 2 }, // Wednesday
            new[] { 2, 3, 1 }, // Thursday
            new[] { 2, 2, 2 }, // Friday
            new[] { 1, 2, 3 }, // Saturday
            new[] { 1, 3, 1 }, // Sunday
        };

        // Penalty for exceeding the cover constraint per shift type.
        var excessCoverPenalties = new[] { 2, 2, 5 };

        var numDays = numWeeks * 7;
        var numShifts = shifts.Length;

        var model = new CpModel();

        IntVar[,,] work = new IntVar[numEmployees, numShifts, numDays];

        foreach (int e in Range(numEmployees))
        {
            foreach (int s in Range(numShifts))
            {
                foreach (int d in Range(numDays))
                {
                    work[e, s, d] = model.NewBoolVar($"work{e}_{s}_{d}");
                }
            }
        }

        // Linear terms of the objective in a minimization context.
        var objIntVars = new List<IntVar>();
        var objIntCoeffs = new List<int>();
        var objBoolVars = new List<IntVar>();
        var objBoolCoeffs = new List<int>();

        // Exactly one shift per day.
        foreach (int e in Range(numEmployees))
        {
            foreach (int d in Range(numDays))
            {
                var temp = new IntVar[numShifts];
                foreach (int s in Range(numShifts))
                {
                    temp[s] = work[e, s, d];
                }

                model.Add(LinearExpr.Sum(temp) == 1);
            }
        }

        // Fixed assignments.
        foreach (var (e, s, d) in fixedAssignments)
        {
            model.Add(work[e, s, d] == 1);
        }

        // Employee requests
        foreach (var (e, s, d, w) in requests)
        {
            objBoolVars.Add(work[e, s, d]);
            objBoolCoeffs.Add(w);
        }

        // Shift constraints
        foreach (var constraint in shiftConstraints)
        {
            foreach (int e in Range(numEmployees))
            {
                var works = new IntVar[numDays];
                foreach (int d in Range(numDays))
                {
                    works[d] = work[e, constraint.Shift, d];
                }

                var (variables, coeffs) = AddSoftSequenceConstraint(
                    model, works, constraint.HardMin, constraint.SoftMin, constraint.MinPenalty, constraint.SoftMax,
                    constraint.HardMax, constraint.MaxPenalty,
                    $"shift_constraint(employee {e}, shift {constraint.Shift}");

                objBoolVars.AddRange(variables);
                objBoolCoeffs.AddRange(coeffs);
            }
        }

        // Weekly sum constraints
        foreach (var constraint in weeklySumConstraints)
        {
            foreach (int e in Range(numEmployees))
            {
                foreach (int w in Range(numWeeks))
                {
                    var works = new IntVar[7];

                    foreach (int d in Range(7))
                    {
                        works[d] = work[e, constraint.Shift, d + w * 7];
                    }

                    var (variables, coeffs) = AddSoftSumConstraint(
                        model, works, constraint.HardMin, constraint.SoftMin, constraint.MinPenalty, constraint.SoftMax,
                        constraint.HardMax, constraint.MaxPenalty,
                        $"weekly_sum_constraint(employee {e}, shift {constraint.Shift}, week {w}");

                    objBoolVars.AddRange(variables);
                    objBoolCoeffs.AddRange(coeffs);
                }
            }
        }

        // Penalized transitions
        foreach (var penalizedTransition in penalizedTransitions)
        {
            foreach (int e in Range(numEmployees))
            {
                foreach (int d in Range(numDays - 1))
                {
                    var transition = new List<ILiteral>() { work[e, penalizedTransition.PreviousShift, d].Not(),
                                                            work[e, penalizedTransition.NextShift, d + 1].Not() };

                    if (penalizedTransition.Penalty == 0)
                    {
                        model.AddBoolOr(transition);
                    }
                    else
                    {
                        var transVar = model.NewBoolVar($"transition (employee {e}, day={d}");
                        transition.Add(transVar);
                        model.AddBoolOr(transition);
                        objBoolVars.Add(transVar);
                        objBoolCoeffs.Add(penalizedTransition.Penalty);
                    }
                }
            }
        }

        // Cover constraints
        foreach (int s in Range(1, numShifts))
        {
            foreach (int w in Range(numWeeks))
            {
                foreach (int d in Range(7))
                {
                    var works = new IntVar[numEmployees];
                    foreach (int e in Range(numEmployees))
                    {
                        works[e] = work[e, s, w * 7 + d];
                    }

                    // Ignore off shift
                    var minDemand = weeklyCoverDemands[d][s - 1];
                    var worked = model.NewIntVar(minDemand, numEmployees, "");
                    model.Add(LinearExpr.Sum(works) == worked);

                    var overPenalty = excessCoverPenalties[s - 1];
                    if (overPenalty > 0)
                    {
                        var name = $"excess_demand(shift={s}, week={w}, day={d}";
                        var excess = model.NewIntVar(0, numEmployees - minDemand, name);
                        model.Add(excess == worked - minDemand);
                        objIntVars.Add(excess);
                        objIntCoeffs.Add(overPenalty);
                    }
                }
            }
        }

        // Objective
        var objBoolSum = LinearExpr.ScalProd(objBoolVars, objBoolCoeffs);
        var objIntSum = LinearExpr.ScalProd(objIntVars, objIntCoeffs);

        model.Minimize(objBoolSum + objIntSum);

        // Solve model
        var solver = new CpSolver();
        solver.StringParameters = "num_search_workers:8, log_search_progress: true, max_time_in_seconds:30";

        var status = solver.Solve(model);

        // Print solution
        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine();
            var header = "          ";
            for (int w = 0; w < numWeeks; w++)
            {
                header += "M T W T F S S ";
            }

            Console.WriteLine(header);

            foreach (int e in Range(numEmployees))
            {
                var schedule = "";
                foreach (int d in Range(numDays))
                {
                    foreach (int s in Range(numShifts))
                    {
                        if (solver.BooleanValue(work[e, s, d]))
                        {
                            schedule += shifts[s] + " ";
                        }
                    }
                }

                Console.WriteLine($"worker {e}: {schedule}");
            }

            Console.WriteLine();
            Console.WriteLine("Penalties:");

            foreach (var (i, var) in objBoolVars.Select((x, i) => (i, x)))
            {
                if (solver.BooleanValue(var))
                {
                    var penalty = objBoolCoeffs[i];
                    if (penalty > 0)
                    {
                        Console.WriteLine($"  {var.Name()} violated, penalty={penalty}");
                    }
                    else
                    {
                        Console.WriteLine($"  {var.Name()} fulfilled, gain={-penalty}");
                    }
                }
            }

            foreach (var (i, var) in objIntVars.Select((x, i) => (i, x)))
            {
                if (solver.Value(var) > 0)
                {
                    Console.WriteLine(
                        $"  {var.Name()} violated by {solver.Value(var)}, linear penalty={objIntCoeffs[i]}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("Statistics");
            Console.WriteLine($"  - status          : {status}");
            Console.WriteLine($"  - conflicts       : {solver.NumConflicts()}");
            Console.WriteLine($"  - branches        : {solver.NumBranches()}");
            Console.WriteLine($"  - wall time       : {solver.WallTime()}");
        }
    }

    /// <summary>
    /// Filters an isolated sub-sequence of variables assigned to True.
    /// Extract the span of Boolean variables[start, start + length), negate them,
    /// and if there is variables to the left / right of this span, surround the
    /// span by them in non negated form.
    /// </summary>
    /// <param name="works">A list of variables to extract the span from.</param>
    /// <param name="start">The start to the span.</param>
    /// <param name="length">The length of the span.</param>
    /// <returns>An array of variables which conjunction will be false if the
    /// sub-list is assigned to True, and correctly bounded by variables assigned
    /// to False, or by the start or end of works.</returns>
    static ILiteral[] NegatedBoundedSpan(IntVar[] works, int start, int length)
    {
        var sequence = new List<ILiteral>();

        if (start > 0)
            sequence.Add(works[start - 1]);

        foreach (var i in Range(length))
            sequence.Add(works[start + i].Not());

        if (start + length < works.Length)
            sequence.Add(works[start + length]);

        return sequence.ToArray();
    }

    /// <summary>
    /// Sequence constraint on true variables with soft and hard bounds.
    /// This constraint look at every maximal contiguous sequence of variables
    /// assigned to true. If forbids sequence of length &lt; hardMin or &gt;
    /// hardMax. Then it creates penalty terms if the length is &lt; softMin or
    /// &gt; softMax.
    /// </summary>
    /// <param name="model">The sequence constraint is built on this
    /// model.</param> <param name="works">A list of Boolean variables.</param>
    /// <param name="hardMin">Any sequence of true variables must have a length of
    /// at least hardMin.</param> <param name="softMin">Any sequence should have a
    /// length of at least softMin, or a linear penalty on the delta will be added
    /// to the objective.</param> <param name="minCost">The coefficient of the
    /// linear penalty if the length is less than softMin.</param> <param
    /// name="softMax">Any sequence should have a length of at most softMax, or a
    /// linear penalty on the delta will be added to the objective.</param> <param
    /// name="hardMax">Any sequence of true variables must have a length of at
    /// most hardMax.</param> <param name="maxCost">The coefficient of the linear
    /// penalty if the length is more than softMax.</param> <param name="prefix">A
    /// base name for penalty literals.</param> <returns>A tuple (costLiterals,
    /// costCoefficients) containing the different penalties created by the
    /// sequence constraint.</returns>
    static (IntVar[] costLiterals, int[] costCoefficients)
        AddSoftSequenceConstraint(CpModel model, IntVar[] works, int hardMin, int softMin, int minCost, int softMax,
                                  int hardMax, int maxCost, string prefix)
    {
        var costLiterals = new List<IntVar>();
        var costCoefficients = new List<int>();

        // Forbid sequences that are too short.
        foreach (var length in Range(1, hardMin))
        {
            foreach (var start in Range(works.Length - length + 1))
            {
                model.AddBoolOr(NegatedBoundedSpan(works, start, length));
            }
        }

        // Penalize sequences that are below the soft limit.

        if (minCost > 0)
        {
            foreach (var length in Range(hardMin, softMin))
            {
                foreach (var start in Range(works.Length - length + 1))
                {
                    var span = NegatedBoundedSpan(works, start, length).ToList();
                    var name = $": under_span(start={start}, length={length})";
                    var lit = model.NewBoolVar(prefix + name);
                    span.Add(lit);
                    model.AddBoolOr(span);
                    costLiterals.Add(lit);
                    // We filter exactly the sequence with a short length.
                    // The penalty is proportional to the delta with softMin.
                    costCoefficients.Add(minCost * (softMin - length));
                }
            }
        }

        // Penalize sequences that are above the soft limit.
        if (maxCost > 0)
        {
            foreach (var length in Range(softMax + 1, hardMax + 1))
            {
                foreach (var start in Range(works.Length - length + 1))
                {
                    var span = NegatedBoundedSpan(works, start, length).ToList();
                    var name = $": over_span(start={start}, length={length})";
                    var lit = model.NewBoolVar(prefix + name);
                    span.Add(lit);
                    model.AddBoolOr(span);
                    costLiterals.Add(lit);
                    // Cost paid is max_cost * excess length.
                    costCoefficients.Add(maxCost * (length - softMax));
                }
            }
        }

        // Just forbid any sequence of true variables with length hardMax + 1
        foreach (var start in Range(works.Length - hardMax))
        {
            var temp = new List<ILiteral>();

            foreach (var i in Range(start, start + hardMax + 1))
            {
                temp.Add(works[i].Not());
            }

            model.AddBoolOr(temp);
        }

        return (costLiterals.ToArray(), costCoefficients.ToArray());
    }

    /// <summary>
    /// Sum constraint with soft and hard bounds.
    /// This constraint counts the variables assigned to true from works.
    /// If forbids sum &lt; hardMin or &gt; hardMax.
    /// Then it creates penalty terms if the sum is &lt; softMin or &gt; softMax.
    /// </summary>
    /// <param name="model">The sequence constraint is built on this
    /// model.</param> <param name="works">A list of Boolean variables.</param>
    /// <param name="hardMin">Any sequence of true variables must have a length of
    /// at least hardMin.</param> <param name="softMin">Any sequence should have a
    /// length of at least softMin, or a linear penalty on the delta will be added
    /// to the objective.</param> <param name="minCost">The coefficient of the
    /// linear penalty if the length is less than softMin.</param> <param
    /// name="softMax">Any sequence should have a length of at most softMax, or a
    /// linear penalty on the delta will be added to the objective.</param> <param
    /// name="hardMax">Any sequence of true variables must have a length of at
    /// most hardMax.</param> <param name="maxCost">The coefficient of the linear
    /// penalty if the length is more than softMax.</param> <param name="prefix">A
    /// base name for penalty literals.</param> <returns>A tuple (costVariables,
    /// costCoefficients) containing the different penalties created by the
    /// sequence constraint.</returns>
    static (IntVar[] costVariables, int[] costCoefficients)
        AddSoftSumConstraint(CpModel model, IntVar[] works, int hardMin, int softMin, int minCost, int softMax,
                             int hardMax, int maxCost, string prefix)
    {
        var costVariables = new List<IntVar>();
        var costCoefficients = new List<int>();
        var sumVar = model.NewIntVar(hardMin, hardMax, "");
        // This adds the hard constraints on the sum.
        model.Add(sumVar == LinearExpr.Sum(works));

        var zero = model.NewConstant(0);

        // Penalize sums below the soft_min target.

        if (softMin > hardMin && minCost > 0)
        {
            var delta = model.NewIntVar(-works.Length, works.Length, "");
            model.Add(delta == (softMin - sumVar));
            var excess = model.NewIntVar(0, works.Length, prefix + ": under_sum");
            model.AddMaxEquality(excess, new[] { delta, zero });
            costVariables.Add(excess);
            costCoefficients.Add(minCost);
        }

        // Penalize sums above the soft_max target.
        if (softMax < hardMax && maxCost > 0)
        {
            var delta = model.NewIntVar(-works.Length, works.Length, "");
            model.Add(delta == sumVar - softMax);
            var excess = model.NewIntVar(0, works.Length, prefix + ": over_sum");
            model.AddMaxEquality(excess, new[] { delta, zero });
            costVariables.Add(excess);
            costCoefficients.Add(maxCost);
        }

        return (costVariables.ToArray(), costCoefficients.ToArray());
    }

    /// <summary>
    /// C# equivalent of Python range (start, stop)
    /// </summary>
    /// <param name="start">The inclusive start.</param>
    /// <param name="stop">The exclusive stop.</param>
    /// <returns>A sequence of integers.</returns>
    static IEnumerable<int> Range(int start, int stop)
    {
        foreach (var i in Enumerable.Range(start, stop - start))
            yield return i;
    }

    /// <summary>
    /// C# equivalent of Python range (stop)
    /// </summary>
    /// <param name="stop">The exclusive stop.</param>
    /// <returns>A sequence of integers.</returns>
    static IEnumerable<int> Range(int stop)
    {
        return Range(0, stop);
    }
}
