// Copyright 2018 Google LLC
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
open System
open Google.OrTools.FSharp
open Google.OrTools.LinearSolver

let SimpleProgramFSharp =
  let svr = Solver.CreateSolver(LinearProgramming.GLOP.ToString())

  // Create the variable x and y.
  let x = svr.MakeNumVar(0.0, 1.0, "x")
  let y = svr.MakeNumVar(0.0, 2.0, "y")
  // Create the objective function, x + y.
  let objective = svr.Objective()
  objective.SetCoefficient(x, 1.0)
  objective.SetCoefficient(y, 1.0)
  objective.SetMaximization()
  // Call the solver and display the results.
  svr.Solve() |> ignore
  printfn "Solution:"
  printfn "x = %f" (x.SolutionValue())
  printfn "y = %f" (y.SolutionValue())

[<EntryPoint>]
let main =
  SimpleProgramFSharp
  exit 0
// [END program]
