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
  let svr = new Solver("Volsay3", solverType.Id);

  let products = ["Gas"; "Chloride"]
  let components = ["nitrogen"; "hydrogen"; "chlorine"]

  let demand = [[1.0;3.0;0.0]; [1.0;4.0;1.0]] // column vectors
  let stock = [50.0;180.0;40.0] // upper bound constraints for each component
  let profit = [30.0;40.0]

  // Variables
  let production = [ for i in 0 .. (products.Length-1) -> svr.MakeNumVar(0.0, 10000.0, products.[i]) ]

  // Constraints
  
  // generate column index selectors
  let cols = [ for i in 0 .. (demand.Length-1) -> i ]

  for row = 0 to (components.Length-1) do
      // generate constraint operands based on indices
      let constraintOperands = List.map (fun c ->  production.[c] * demand.[c].[row]) cols
      let linearExp = List.reduce (+) constraintOperands
      
      // create the constraint
      let rc = RangeConstraint(linearExp, Double.NegativeInfinity, stock.[row])
      svr.Add(rc) |> ignore
      

  // Objective
  let objectiveOperands = List.map (fun c ->  profit.[c] * production.[c]) cols
  let objectiveExp = List.reduce (+) objectiveOperands
  svr.Maximize(objectiveExp)

  let resultStatus = svr.Solve();

  match resultStatus with
  | status when status <> Solver.OPTIMAL ->
      printfn "The problem does not have an optimal solution!"
      exit 0
  | _ ->
      printfn "\nProblem solved in %d milliseconds" (svr.WallTime())
      printfn "Iterations: %i\n" (svr.Iterations())

  printfn "Objective: %f" (svr.Objective().Value())
  products |> List.iteri (fun i x -> printfn "%-10s: %f ReducedCost: %f" x ((production.[i]).SolutionValue()) ((production.[i]).ReducedCost()) )

solver LinearProgramming.CLP
