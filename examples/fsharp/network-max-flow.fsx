#I "./lib"
#load "Google.OrTools.FSharp.fsx"

open Google.OrTools.Graph
open Google.OrTools.FSharp

printfn "Max Flow Problem"
let  numNodes = 6;
let numArcs = 9;
let tails = [0; 0; 0; 0; 1; 2; 3; 3; 4]
let heads = [1; 2; 3; 4; 3; 4; 4; 5; 5]
let capacities = [5L; 8L; 5L; 3L; 4L; 5L; 6L; 6L; 4L]
let expectedFlows = [4; 4; 2; 0; 4; 4; 0; 6; 4]
let expectedTotalFlow = 10;
let maxFlow = new MaxFlow()

for i=0 to (numArcs-1) do
  let arc = maxFlow.AddArcWithCapacity(tails.[i], heads.[i], capacities.[i])
  if (arc <> i) then
    failwith "Internal error"

let source = 0;
let sink = numNodes - 1;
printfn "Solving max flow with %i nodes, and %i arcs, source=%i, sink=%i " numNodes numArcs source sink
let solveStatus = maxFlow.Solve(source, sink)

match solveStatus with
| MaximumFlow.Optimal ->
    let totalFlow = maxFlow.OptimalFlow();
    printfn "total computed flow %i, expected = %i" totalFlow expectedTotalFlow

    for i=1 to numArcs do
      printfn "Arc %i (%i -> %i), capacity=%i, computed=%i, expected=%i" i (maxFlow.Head(i-1)) (maxFlow.Tail(i-1)) (maxFlow.Capacity(i-1)) (maxFlow.Flow(i-1)) (expectedFlows.[i-1])
| _ ->
  printfn "Solving the max flow problem failed. Solver status: %i" solveStatus


