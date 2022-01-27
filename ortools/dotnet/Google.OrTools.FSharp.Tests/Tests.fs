namespace Google.OrTools.FSharp


module Tests =
  open System
  open Xunit

  open Google.OrTools.LinearSolver
  open Google.OrTools.FSharp


  [<Fact>]
  let ``Equality Matrix cannot be empty`` () =
      try
        let opts = SolverOpts.Default.Name("Equality Constraints").Goal(Minimize).Objective([2.0;1.0;0.0;0.0;0.0]).Algorithm(LP CLP)
        opts |> lpSolve |> SolverSummary |> ignore
      with
      | Failure msg ->
          Assert.Equal("Must provide at least one Matrix/Vector pair for inequality/equality contraints", msg)


  [<Fact>]
  let ``Linear Solver with GLOP Solver`` () =
      let svr = Solver.CreateSolver("GLOP")

      // x1, x2 are continuous non-negative variables.
      let x1 = svr.MakeNumVar(Double.NegativeInfinity, Double.PositiveInfinity, "x1")
      let x2 = svr.MakeNumVar(Double.NegativeInfinity, Double.PositiveInfinity, "x2")

      // Maximize 8 * x1 + 4 * x2
      let objective = svr.Objective()
      objective.SetCoefficient(x1, 8.0)
      objective.SetCoefficient(x2, 4.0)
      objective.SetMaximization()

      // x1 + x2 <= 10.
      let c0 = svr.MakeConstraint(Double.NegativeInfinity, 10.0)
      c0.SetCoefficient(x1, 1.0)
      c0.SetCoefficient(x2, 1.0)

      // 5 * x1 + x2 <= 15.
      let c1 = svr.MakeConstraint(Double.NegativeInfinity, 15.0)
      c1.SetCoefficient(x1, 5.0)
      c1.SetCoefficient(x2, 1.0)

      Assert.Equal(svr.NumVariables(), 2)
      Assert.Equal(svr.NumConstraints(), 2)

      let resultStatus = svr.Solve()

      Assert.Equal(resultStatus, Solver.ResultStatus.OPTIMAL)
      Assert.Equal(svr.Objective().Value(), 45.0)
      Assert.Equal(x1.SolutionValue(), 5.0/4.0)
      Assert.Equal(x2.SolutionValue(), 35.0/4.0)
