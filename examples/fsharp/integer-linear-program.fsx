(*
  Minimize:     6 * x[0] + 3 * x[1] + 1 * x[2] + 2 * x[3]

  Subject to:   1 * x[0] + 1 * x[1] + 1 * x[2] + 1 * x[3]  <= 8
                2 * x[0] + 1 * x[1] + 3 * x[2] + 0 * x[3]  <= 12
                0 * x[0] + 5 * x[1] + 1 * x[2] + 3 * x[3]  <= 6

  And           x[0] <= 1, x[1] <= 1, x[2] <= 4, x[3] <= 2


  Answer:
    Integer Program Solution

    Problem solved in 51 milliseconds
    Iterations: 1

    Objective: 11.000000
    var[0]    : 1.000000
    var[1]    : 0.000000
    var[2]    : 3.000000
    var[3]    : 1.000000
    Advanced usage:
    Problem solved in 0 branch-and-bound nodes


    Linear Program Solution

    Problem solved in 2 milliseconds
    Iterations: 2

    Objective: 11.111111
    var[0]    : 1.000000
    var[1]    : 0.000000
    var[2]    : 3.333333
    var[3]    : 0.888889
*)

#I "./lib"
#load "Google.OrTools.FSharp.fsx"

open System
open Google.OrTools.FSharp
open Google.OrTools.LinearSolver

let opts = SolverOpts.Default
            .Goal(Maximize)
            .Objective([6.;3.;1.;2.])
            .Matrix([[1.;2.;0.]; [1.;1.;5.]; [1.;3.;1.]; [1.;0.;3.]])
            .VectorUpperBound([8.; 12.; 6.])
            .VarLowerBound([0.; 0.; 0.; 0.])
            .VarUpperBound([1.; 1.; 4.; 2.])


printfn "\n\nInteger Program Solution"
let slvrIP = opts.Name("IP Solution").Algorithm(IP CBC) |> lpSolve |> SolverSummary
printfn "Advanced usage:"
printfn "Problem solved in %i branch-and-bound nodes" (slvrIP.Nodes())

printfn "\n\nLinear Program Solution"
let slvrLP = opts.Name("LP Solution").Algorithm(LP CLP) |> lpSolve |> SolverSummary

