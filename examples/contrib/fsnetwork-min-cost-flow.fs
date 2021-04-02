(*
  Copyright 2010-2021 Google
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

open Google.OrTools.Graph
open Google.OrTools.FSharp

printfn "Min Cost Flow Problem"
let numSources = 4
let numTargets = 4
let costs = array2D [
                      [90L; 75L; 75L; 80L];
                      [35L; 85L; 55L; 65L];
                      [125L; 95L; 90L; 105L];
                      [45L; 110L; 95L; 115L]
                    ]

let expectedCost = 275
let minCostFlow = new MinCostFlow()

for source=0 to (numSources-1) do
  for target=0 to (numTargets-1) do
    minCostFlow.AddArcWithCapacityAndUnitCost(source, numSources + target, 1L, costs.[source, target]) |> ignore

for source=0 to (numSources-1) do
  minCostFlow.SetNodeSupply(source, 1L)

for target=0 to (numTargets-1) do
  minCostFlow.SetNodeSupply(numSources + target, -1L);

printfn "Solving min cost flow with %i sources, and %i targets." numSources numTargets
let solveStatus = minCostFlow.Solve();

match solveStatus with
| x when x = MinimumCostFlowResult.Optimal.Id ->
  printfn "Total computed flow cost = %i, expected = %i" (minCostFlow.OptimalCost()) expectedCost
| _ ->
  printfn "Solving the min cost flow problem failed. Solver status: %s" (solveStatus.ToString("g"))
