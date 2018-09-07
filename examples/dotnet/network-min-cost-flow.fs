#I "../../bin"
#I "./packages/NETStandard.Library.2.0.2/build/netstandard2.0/ref"

#r "Google.OrTools.dll"
#r "Google.OrTools.FSharp.dll"
#r "netstandard.dll"

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
  printfn "Solving the min cost flow problem failed. Solver status: %i" solveStatus

