(*
  Minimize:     2 * x[0] + 1 * x[1]

  Subject to:   3 * x[0] + 1 * x[1] - 1 * x[2]                        = 3
                4 * x[0] + 3 * x[1]            - 1 * x[3]             = 6
                1 * x[0] + 2 * x[1]                       - 1 * x[4]  = 2

  And x[j] >= 0 for j in [0..4]


  Answer:
    Objective: 2.400000
    x[0]    : 0.600000
    x[1]    : 1.200000
    x[2]    : 0.000000
    x[3]    : 0.000000
    x[4]    : 1.000000

*)

#I "./lib"
#load "Google.OrTools.FSharp.fsx"

open System
open Google.OrTools.FSharp
open Google.OrTools.LinearSolver

let opts = SolverOpts.Default
            .Name("Equality Constraints")
            .Goal(Minimize)
            .Objective([2.0;1.0;0.0;0.0;0.0])
            .MatrixEq([[3.0;4.0;1.0]; [1.0;3.0;2.0]; [-1.0;0.0;0.0]; [0.0;-1.0;0.0]; [0.0;0.0;-1.0]])
            .VectorEq([3.0; 6.0; 2.0])
            .VarLowerBound([0.0; 0.0; 0.0; 0.0; 0.0])
            .VarUpperBound([Double.PositiveInfinity; Double.PositiveInfinity; Double.PositiveInfinity; Double.PositiveInfinity; Double.PositiveInfinity])
            .Algorithm(LP CLP)

let slvr = opts |> lpSolve |> SolverSummary
