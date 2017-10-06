(*
  Minimize:     -3 * x[0] + 1 * x[1] + 1 * x[2]

  Subject to:   1 * x[0] - 2 * x[1] + 1 * x[2]   <= 11
               -4 * x[0] + 1 * x[1] + 2 * x[2]   >= 3
               -2 * x[0] + 0 * x[1] - 1 * x[2]   = -1

  And           x[0] >= 0, x[1] >= 0, x[2] >= 0


  Answer:
    Objective: -2.000000
    var[0]    : 4.000000
    var[1]    : 1.000000
    var[2]    : 9.000000
    var[3]    : 0.000000
    var[4]    : 0.000000

  Note: When entering the matrix, it is reduced to standard form with appropriate slack variables.

*)

#I "./lib"
#load "Google.OrTools.FSharp.fsx"

open System
open Google.OrTools.FSharp
open Google.OrTools.LinearSolver

let opts = SolverOpts.Default
            .Name("Equality/Inequality Constraints")
            .Goal(Minimize)
            .Objective([-3.0;1.0;1.0;0.0;0.0])
            .MatrixEq([[1.0;-4.0;-2.0]; [-2.0;1.0;0.0]; [1.0;2.0;1.0]; [1.0;0.0;0.0]; [0.0;-1.0;0.0]])
            .VectorEq([11.0; 3.0; 1.0])
            .VarLowerBound(0. |> List.replicate 5)
            .VarUpperBound(Double.PositiveInfinity |> List.replicate 5)

let slvrCLP = opts.Algorithm(LP CLP) |> lpSolve |> SolverSummary
let slvrGLOP = opts.Algorithm(LP GLOP) |> lpSolve |> SolverSummary

