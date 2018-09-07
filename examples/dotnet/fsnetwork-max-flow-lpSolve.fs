(*
  The matrix in the problem represent the arcs versus the number of nodes
  (not including the source and sink nodes). In this problem we have 6 nodes
  and nine arcs. For maximal flow, we want to maximize the flow in the arcs
  connected to the source (in this case node 6). The answer represent the arcs
  flows.

  The objective function vector identify the arcs we want to maximize and the lower
  and upper bounds are the arc capacities.

  Answer:
    Objective: 12.000000
    var[0]    : 1.000000
    var[1]    : 4.000000
    var[2]    : 5.000000
    var[3]    : 2.000000
    var[4]    : 4.000000
    var[5]    : 0.000000
    var[6]    : 6.000000
    var[7]    : 6.000000
    var[8]    : 1.000000

*)

open Google.OrTools.FSharp

let opts = SolverOpts.Default
            .Name("Max Flow with Linear Programming")
            .Goal(Maximize)
            .Objective([0.; 0.; 0.; 0.; 0.; 0.; 1.; 1.; 0.])
            .MatrixEq([[1.; 0.; 0. ;0.]; [0.; 1.; 0.; 0.]; [0.; 0.; 1.; 0.]; [0.; 0.; 0.; 1.]; [0.; -1.; 0.; 1.]; [0.; 0.; -1.; 1.]; [0.; 0.; 0.; -1.]; [0.; 0.; -1.; 0.]; [-1.; 0.; 1.; 0.]])
            .VectorEq([0.; 0.; 0.; 0.])
            .VarLowerBound([0.; 0.; 0.; 0.; 0.; 0.; 0.; 0.; 0.])
            .VarUpperBound([5.; 8.; 5.; 3.; 4.; 5.; 6.; 6.; 4.])
            .Algorithm(LP CLP)

let slvr = opts |> lpSolve
slvr |> SolverSummary
