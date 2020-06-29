(*
  Copyright 2012 Hakan Kjellerstrand

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*)

open Google.OrTools.ConstraintSolver

let solver = new Solver("Diet");

let price = [|50; 20; 30; 80|] // in cents

// requirements for each nutrition type
let limits = [|500; 6; 10; 8|]
let products = [|"A"; "B"; "C"; "D"|]

// nutritions for each product
let calories  = [|400L; 200L; 150L; 500L|]
let chocolate = [|3L; 2L; 0L; 0L|]
let sugar     = [|2L; 2L; 4L; 4L|]
let fat       = [|2L; 4L; 1L; 5L|]

let x = solver.MakeIntVarArray(products.Length, 0L, 100L, "x")
let cost = x.ScalProd(price).Var()

solver.Add( x.ScalProd(calories).solver().MakeGreaterOrEqual(x.ScalProd(calories), limits.[0]) )
solver.Add( x.ScalProd(chocolate).solver().MakeGreaterOrEqual(x.ScalProd(chocolate), limits.[1]))
solver.Add( x.ScalProd(sugar).solver().MakeGreaterOrEqual(x.ScalProd(sugar), limits.[2]))
solver.Add( x.ScalProd(fat).solver().MakeGreaterOrEqual(x.ScalProd(fat), limits.[3]))

let obj = cost.Minimize(int64(1))
let db = solver.MakePhase(new IntVarVector(x), Solver.CHOOSE_PATH, Solver.ASSIGN_MIN_VALUE)

solver.NewSearch(db, obj)
while (solver.NextSolution()) do
  printfn "cost: %d" (cost.Value())
  printfn "Products: "
  for i=0 to (products.Length-1) do
    printfn "%s: %d" products.[i] (x.[i].Value())


printfn "\nSolutions: %d" (solver.Solutions())
printfn "WallTime: %d ms" (solver.WallTime())
printfn "Failures: %d" (solver.Failures())
printfn "Branches: %d " (solver.Branches())

solver.EndSearch();
