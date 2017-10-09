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

#I "../../bin"
#r "Google.OrTools.dll"

#I "./lib"
#load "Google.OrTools.FSharp.fsx"

open System
open Google.OrTools.FSharp
open Google.OrTools.LinearSolver

let solver (solverType:LinearProgramming) =
  let svr = new Solver("Volsay", solverType.Id);

  let gas = svr.MakeNumVar(0.0, 100000.0, "Gas")
  let chloride = svr.MakeNumVar(0.0, 100000.0, "Cloride")

  let c1 = svr.MakeConstraint(Double.NegativeInfinity, 50.0)
  c1.SetCoefficient(gas, 1.0)
  c1.SetCoefficient(chloride, 1.0)

  let c2 = svr.MakeConstraint(Double.NegativeInfinity, 180.0)
  c1.SetCoefficient(gas, 3.0)
  c1.SetCoefficient(chloride, 4.0)

  svr.Maximize(40.0 * gas + 50.0 * chloride)

  let resultStatus = svr.Solve();

  // Check that the problem has an optimal solution.
  match resultStatus with
  | status when status <> Solver.OPTIMAL ->
      printfn "The problem does not have an optimal solution!"
      exit 0
  | _ ->
      printfn "Problem solved in %i milliseconds" (svr.WallTime())
      printfn "Iterations: %i" (svr.Iterations())

  printfn "Objective: %f" (svr.Objective().Value())
  printfn "Gas      : %f ReducedCost: %f" (gas.SolutionValue()) (gas.ReducedCost())
  printfn "Chloride : %f ReducedCost: %f" (chloride.SolutionValue()) (chloride.ReducedCost())
  printfn "c1       : DualValue: %f" (c1.DualValue())
  printfn "c2       : DualValue: %f" (c2.DualValue())


solver LinearProgramming.CLP
