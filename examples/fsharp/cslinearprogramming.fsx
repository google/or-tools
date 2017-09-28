// Copyright 2010-2017 Google
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

#I "../../bin"
#r "Google.OrTools.dll"

#I "./lib"
#load "Google.OrTools.FSharp.fsx"

open System
open Google.OrTools.LinearSolver
open Google.OrTools.FSharp

let solver solverType =
  let svr = Solver.CreateSolver("IntegerProgramming", solverType.ToString())

  // x1, x2 and x3 are continuous non-negative variables.
  let x1 = svr.MakeNumVar(0.0, Double.PositiveInfinity, "x1")
  let x2 = svr.MakeNumVar(0.0, Double.PositiveInfinity, "x2")
  let x3 = svr.MakeNumVar(0.0, Double.PositiveInfinity, "x3")

  // Maximize 10 * x1 + 6 * x2 + 4 * x3.
  let objective = svr.Objective()
  objective.SetCoefficient(x1, 10.0)
  objective.SetCoefficient(x2, 6.0)
  objective.SetCoefficient(x3, 4.0)
  objective.SetMaximization()

  // x1 + x2 + x3 <= 100.
  let c0 = svr.MakeConstraint(Double.NegativeInfinity, 100.0)
  c0.SetCoefficient(x1, 1.0)
  c0.SetCoefficient(x2, 1.0)
  c0.SetCoefficient(x3, 1.0)

  // 10 * x1 + 4 * x2 + 5 * x3 <= 600.
  let c1 = svr.MakeConstraint(Double.NegativeInfinity, 600.0)
  c1.SetCoefficient(x1, 10.0)
  c1.SetCoefficient(x2, 4.0)
  c1.SetCoefficient(x3, 5.0)

  // 2 * x1 + 2 * x2 + 6 * x3 <= 300.
  let c2 = svr.MakeConstraint(Double.NegativeInfinity, 300.0)
  c2.SetCoefficient(x1, 2.0)
  c2.SetCoefficient(x2, 2.0)
  c2.SetCoefficient(x3, 6.0)

  printfn "Number of variables = %i" (svr.NumVariables())
  printfn "Number of constraints = %i" (svr.NumConstraints())

  let resultStatus = svr.Solve()

  // Check that the problem has an optimal solution.
  match resultStatus with
  | status when status <> Solver.OPTIMAL ->
      printfn "The problem does not have an optimal solution!"
      exit 0
  | _ ->
      printfn "Problem solved in %i milliseconds" (svr.WallTime())

  // The objective value of the solution.
  printfn "Optimal objective value = %f" (svr.Objective().Value())

  // The value of each variable in the solution.
  printfn "x1 = %f" (x1.SolutionValue())
  printfn "x2 = %f" (x2.SolutionValue())
  printfn "x3 = %f" (x3.SolutionValue())

  printfn "Advanced usage:"
  let activities = svr.ComputeConstraintActivities();

  printfn "Problem solved in %i iterations" (svr.Iterations())
  printfn "x1: reduced cost = %f" (x1.ReducedCost())
  printfn "x2: reduced cost = %f" (x2.ReducedCost())
  printfn "x3: reduced cost = %f" (x3.ReducedCost())
  printfn "c0: dual value = %f" (c0.DualValue())
  printfn "    activity = %f" (activities.[c0.Index()])
  printfn "c1: dual value = %f" (c1.DualValue())
  printfn "    activity = %f" (activities.[c1.Index()])
  printfn "c2: dual value = %f" (c2.DualValue())
  printfn "    activity = %f" (activities.[c2.Index()])


printfn "---- Linear programming example with %A  ----" LinearProgramming.GLOP
solver LinearProgramming.GLOP

// printfn "---- Linear programming example with %A ----" LinearProgramming.GLPK
// solver LinearProgramming.GLPK

printfn "---- Linear programming example with %A ----" LinearProgramming.CLP
solver LinearProgramming.CLP
