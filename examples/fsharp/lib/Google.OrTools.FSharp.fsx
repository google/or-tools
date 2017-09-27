namespace Google.OrTools

#I "../../../bin"
#r "Google.OrTools.dll"

open System
open Google.OrTools.LinearSolver

module FSharp =
  type Goal =
    | Maximize
    | Minimize

  type LinearProgramming =
    // Linear Programming
    | CLP  // Recommended default value.
    | GLPK
    | GLOP
    | GUROBI
    | CPLEX
    override this.ToString() =
      match this with
      | CLP -> "CLP_LINEAR_PROGRAMMING"
      | GLPK -> "GLPK_LINEAR_PROGRAMMING"
      | GLOP -> "GLOP_LINEAR_PROGRAMMING"
      | GUROBI -> "GUROBI_LINEAR_PROGRAMMING"
      | CPLEX -> "CPLEX_LINEAR_PROGRAMMING"
    member this.Id =
      match this with
      | CLP -> 0
      | GLPK -> 1
      | GLOP -> 2
      | GUROBI -> 6
      | CPLEX -> 10


  type IntegerProgramming =
      // Integer programming problems.
      | SCIP  // Recommended default value.
      | GLPK
      | CBC
      | GUROBI
      | CPLEX
      | BOP
      override this.ToString() =
        match this with
        | SCIP -> "SCIP_MIXED_INTEGER_PROGRAMMING"  // Recommended default value.
        | GLPK -> "GLPK_MIXED_INTEGER_PROGRAMMING"
        | CBC -> "CBC_MIXED_INTEGER_PROGRAMMING"
        | GUROBI -> "GUROBI_MIXED_INTEGER_PROGRAMMING"
        | CPLEX -> "CPLEX_MIXED_INTEGER_PROGRAMMING"
        | BOP -> "BOP_INTEGER_PROGRAMMING"
      member this.Id =
        match this with
        | SCIP -> 3
        | GLPK -> 4
        | CBC -> 5
        | GUROBI -> 7
        | CPLEX -> 11
        | BOP -> 12

  type LinearSolverAlgorithm =
    | LP of LinearProgramming
    | IP of IntegerProgramming


  type SolverOpts = {
    SolverName: string;                               // Name of the solver
    ObjectiveFunction: list<float>;                   // Linear objective function vector
    ConstraintMatrixIneq: list<list<float>>;          // Matrix for linear inequality constraints
    ConstraintVectorIneq: list<float>;                // Vector for linear inequality constraints
    UpperBound: list<float>;                          // Vector of variable upper bounds
    LowerBound: list<float> ;                         // Vector of variable lower bounds
    ConstraintMatrixEq: list<list<float>> option;     // Matrix for linear equality constraints
    ConstraintVectorEq: list<float> option;           // Vector for linear equality constraints
    SolverAlgorithm: LinearSolverAlgorithm            // Solver Algorithm to use
    SolverGoal: Goal                                  // Solver Goal
  } with
    member this.Name(nm:string)=
      {this with SolverName = nm}
    member this.Objective(objVector:list<float>) =
      {this with ObjectiveFunction = objVector}
    member this.MatrixIneq(mat:list<list<float>>) =
      {this with ConstraintMatrixIneq = mat}
    member this.VectorIneq(vec:list<float>) =
      {this with ConstraintVectorIneq = vec}
    member this.MatrixEq(mat:list<list<float>>) =
      {this with ConstraintMatrixEq = Some(mat)}
    member this.VectorEq(vec:list<float>) =
      {this with ConstraintVectorEq = Some(vec)}
    member this.Upper(ub:list<float>) =
      {this with UpperBound = ub}
    member this.Lower(lb:list<float>) =
      {this with LowerBound = lb}
    member this.Algorithm(algo:LinearSolverAlgorithm) =
      {this with SolverAlgorithm = algo}
    member this.Goal(goal:Goal) =
      {this with SolverGoal = goal}

  let defaultSolverOpts = {
    SolverName = "Solver";
    ObjectiveFunction = [];
    ConstraintMatrixIneq = [[]];
    ConstraintVectorIneq = [];
    UpperBound = [];
    LowerBound = [];
    ConstraintMatrixEq = None;
    ConstraintVectorEq = None;
    SolverAlgorithm = LP CLP
    SolverGoal = Maximize
  }

  let lpSolve (solverOptions:SolverOpts) =
    // extract the specific algorithm so its Id can be used to create solver
    let algorithm =
      match solverOptions.SolverAlgorithm with
      | LP lp -> lp.Id
      | IP ip -> ip.Id

    let solver = new Solver(solverOptions.SolverName, algorithm)

    // Detect errors on required parameters
    if solverOptions.ObjectiveFunction.IsEmpty then
      failwith "Objective function cannot be empty"

    if solverOptions.ConstraintMatrixIneq.IsEmpty || solverOptions.ConstraintMatrixIneq.[0].IsEmpty then
      failwith "Matrix of inequality contraints cannot be empty"

    if solverOptions.ConstraintVectorIneq.IsEmpty then
      failwith "Vector inequality contraints cannot be empty"

    if solverOptions.UpperBound.IsEmpty then
      failwith "Variable upper bound values cannot be empty"

    if solverOptions.LowerBound.IsEmpty then
      failwith "Variable lower bound values cannot be empty"

    if solverOptions.LowerBound.Length <> solverOptions.UpperBound.Length then
      failwithf "Variable bounds' dimensions should be equal.\nLower Bound Length: %i\nUpper Bound Length: %i" solverOptions.LowerBound.Length solverOptions.UpperBound.Length

    // Variables
    let vars = [ for i in 0 .. (solverOptions.LowerBound.Length-1) -> solver.MakeNumVar(solverOptions.LowerBound.[i], solverOptions.UpperBound.[i], (sprintf "var[%i]" i ) ) ]

    // Constraints
    let cols = [ for i in 0 .. (solverOptions.LowerBound.Length-1) -> i ]   // generate column index selectors

    // Inequality Constraints
    for row = 0 to (solverOptions.ConstraintMatrixIneq.Length-1) do
        // generate constraint operands based on indices
        let constraintOperands = List.map (fun c ->  vars.[c] * solverOptions.ConstraintMatrixIneq.[c].[row]) cols
        let linearExp = List.reduce (+) constraintOperands

        // create the constraint
        let rangeConstraint = RangeConstraint(linearExp, Double.NegativeInfinity, solverOptions.ConstraintVectorIneq.[row])
        solver.Add(rangeConstraint) |> ignore

    // Equality Constraints (if they exist)
    match (solverOptions.ConstraintMatrixEq, solverOptions.ConstraintVectorEq) with
    | (Some mat, Some vec) ->
        for row = 0 to (mat.Length-1) do
            // generate constraint operands based on indices
            let constraintOperands = List.map (fun c ->  vars.[c] * mat.[c].[row]) cols
            let linearExp = List.reduce (+) constraintOperands

            // create the constraint
            let equalityConstraint = RangeConstraint(linearExp, vec.[row], vec.[row])
            solver.Add(equalityConstraint) |> ignore
    | (None, Some vec) ->
        failwithf "Matrix for Equality Constraints undefined."
    | (Some mat, None) ->
        failwithf "Vector for Equality Constraints undefined."
    | (None, None) ->
        printfn "No Equality Constraints present in formulation" |> ignore

    // Objective
    let objectiveOperands = List.map (fun c ->  solverOptions.ObjectiveFunction.[c] * vars.[c]) cols
    let objectiveExp = List.reduce (+) objectiveOperands

    match solverOptions.SolverGoal with
    | Minimize ->
        solver.Minimize(objectiveExp)
    | Maximize ->
        solver.Maximize(objectiveExp)

    solver

