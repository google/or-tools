// Copyright 2010-2014 Google
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
open Google.OrTools.FSharp
open Google.OrTools.LinearSolver

let solver solverType =
  let svr = Solver.CreateSolver("IntegerProgramming", solverType.ToString())

  // x1 and x2 are integer non-negative variables.
  let x1 = svr.MakeIntVar(0.0, Double.PositiveInfinity, "x1")
  let x2 = svr.MakeIntVar(0.0, Double.PositiveInfinity, "x2")

  // Minimize x1 + 2 * x2.
  let objective = svr.Objective()
  objective.SetMinimization()
  objective.SetCoefficient(x1, 1.0)
  objective.SetCoefficient(x2, 2.0)

  // 2 * x2 + 3 * x1 >= 17.
  let ct = svr.MakeConstraint(17.0, Double.PositiveInfinity)
  ct.SetCoefficient(x1, 3.0)
  ct.SetCoefficient(x2, 2.0)

  let resultStatus = svr.Solve()

  // Check that the problem has an optimal solution.
  match resultStatus with
  | status when status <> Solver.OPTIMAL ->
      printfn "The problem does not have an optimal solution!"
      exit 0
  | _ ->
      printfn "Problem solved in %i milliseconds" (svr.WallTime())

  // The objective value of the solution.
  printfn "Optimal objective value = %f" (objective.Value())

  // The value of each variable in the solution.
  printfn "x1 = %f" (x1.SolutionValue())
  printfn "x2 = %f" (x2.SolutionValue())

  printfn "Advanced usage:"
  printfn "Problem solved in %i branch-and-bound nodes" (svr.Nodes())


// printfn "---- Integer programming example with %A ----" IntegerProgramming.GLPK
// solver IntegerProgramming.GLPK

printfn "---- Linear programming example with %A ----" IntegerProgramming.CBC
solver IntegerProgramming.CBC

// printfn "---- Linear programming example with %A ----" IntegerProgramming.SCIP
// solver IntegerProgramming.SCIP
