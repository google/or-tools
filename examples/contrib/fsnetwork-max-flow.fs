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

  Answer:
    Solving max flow with 6 nodes, and 9 arcs, source=0, sink=5
    total computed flow 10, expected = 10
    Arc 1 (1 -> 0), capacity=5, computed=4, expected=4
    Arc 2 (2 -> 0), capacity=8, computed=4, expected=4
    Arc 3 (3 -> 0), capacity=5, computed=2, expected=2
    Arc 4 (4 -> 0), capacity=3, computed=0, expected=0
    Arc 5 (3 -> 1), capacity=4, computed=4, expected=4
    Arc 6 (4 -> 2), capacity=5, computed=4, expected=4
    Arc 7 (4 -> 3), capacity=6, computed=0, expected=0
    Arc 8 (5 -> 3), capacity=6, computed=6, expected=6
    Arc 9 (5 -> 4), capacity=4, computed=4, expected=4
*)

open Google.OrTools.Graph
open Google.OrTools.FSharp

printfn "Max Flow Problem"
let numNodes = 6;
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
| x when x = MaximumFlowResult.Optimal.Id ->
    let totalFlow = maxFlow.OptimalFlow();
    printfn "total computed flow %i, expected = %i" totalFlow expectedTotalFlow

    for i=1 to numArcs do
      printfn "Arc %i (%i -> %i), capacity=%i, computed=%i, expected=%i" i (maxFlow.Head(i-1)) (maxFlow.Tail(i-1)) (maxFlow.Capacity(i-1)) (maxFlow.Flow(i-1)) (expectedFlows.[i-1])
| _ ->
  printfn "Solving the max flow problem failed. Solver status: %s" (solveStatus.ToString("g"))
