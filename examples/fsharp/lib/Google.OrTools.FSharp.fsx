
#I "../../../bin"
#r "Google.OrTools.dll"

open System
open Google.OrTools.LinearSolver

type Goal =
  /// Maximize the Objective Function
  | Maximize
  /// Minimize the Objective Function
  | Minimize

/// Linear Programming Algorithm
type LinearProgramming =
  /// Coin-Or (Recommended default)
  | CLP
  /// GNU Linear Programming Kit
  | GLPK
  /// Google Linear Optimization
  | GLOP
  /// Gurobi Optimizer
  | GUROBI
  /// IBM CPLEX
  | CPLEX
  /// Solver Name
  override this.ToString() =
    match this with
    | CLP -> "CLP_LINEAR_PROGRAMMING"
    | GLPK -> "GLPK_LINEAR_PROGRAMMING"
    | GLOP -> "GLOP_LINEAR_PROGRAMMING"
    | GUROBI -> "GUROBI_LINEAR_PROGRAMMING"
    | CPLEX -> "CPLEX_LINEAR_PROGRAMMING"
  /// Solver Id
  member this.Id =
    match this with
    | CLP -> 0
    | GLPK -> 1
    | GLOP -> 2
    | GUROBI -> 6
    | CPLEX -> 10

/// Integer Programming Algoritm
type IntegerProgramming =
    /// Solving Constraint Integer Programs (Recommended default)
    | SCIP
    /// GNU Linear Programming Kit
    | GLPK
    /// Coin-Or Branch and Cut
    | CBC
    /// Gurobi Optimizer
    | GUROBI
    /// IBM CPLEX
    | CPLEX
    /// Binary Optimizer
    | BOP
    /// Solver Name
    override this.ToString() =
      match this with
      | SCIP -> "SCIP_MIXED_INTEGER_PROGRAMMING"
      | GLPK -> "GLPK_MIXED_INTEGER_PROGRAMMING"
      | CBC -> "CBC_MIXED_INTEGER_PROGRAMMING"
      | GUROBI -> "GUROBI_MIXED_INTEGER_PROGRAMMING"
      | CPLEX -> "CPLEX_MIXED_INTEGER_PROGRAMMING"
      | BOP -> "BOP_INTEGER_PROGRAMMING"
    /// Solver Id
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
  /// Name of the solver
  SolverName: string;
  /// Linear objective function vector
  ObjectiveFunction: float list;
  /// Matrix for linear inequality constraints
  ConstraintMatrix: float list list option;
  /// Upper Bound vector for linear inequality constraints
  ConstraintVectorUpperBound: float list option;
  /// Lower Bound vector for linear inequality constraints
  ConstraintVectorLowerBound: float list option;
  /// Vector of variable upper bounds
  VariableUpperBound: float list;
  /// Vector of variable lower bounds
  VariableLowerBound: float list;
  /// Matrix for linear equality constraints
  ConstraintMatrixEq: float list list option;
  /// Vector for linear equality constraints
  ConstraintVectorEq: float list option;
  /// Solver Algorithm to use
  SolverAlgorithm: LinearSolverAlgorithm;
  /// Solver Goal (Minimize/Maximize)
  SolverGoal: Goal
} with
  member this.Name(nm:string)=
    {this with SolverName = nm}
  member this.Objective(objVector:float list) =
    {this with ObjectiveFunction = objVector}
  member this.Matrix(mat:float list list) =
    {this with ConstraintMatrix = Some(mat)}
  member this.VectorUpperBound(vec:float list) =
    {this with ConstraintVectorUpperBound = Some(vec)}
  member this.VectorLowerBound(vec:float list) =
    {this with ConstraintVectorLowerBound = Some(vec)}
  member this.MatrixEq(mat:float list list) =
    {this with ConstraintMatrixEq = Some(mat)}
  member this.VectorEq(vec:float list) =
    {this with ConstraintVectorEq = Some(vec)}
  member this.VarUpperBound(ub:float list) =
    {this with VariableUpperBound = ub}
  member this.VarLowerBound(lb:float list) =
    {this with VariableLowerBound = lb}
  member this.Algorithm(algo:LinearSolverAlgorithm) =
    {this with SolverAlgorithm = algo}
  member this.Goal(goal:Goal) =
    {this with SolverGoal = goal}
  static member Default =
    {
      SolverName = "Solver";
      ObjectiveFunction = [];
      ConstraintMatrix = None;
      ConstraintVectorUpperBound = None;
      ConstraintVectorLowerBound = None;
      VariableUpperBound = [];
      VariableLowerBound = [];
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
  if (solverOptions.ConstraintMatrix.IsNone && solverOptions.ConstraintVectorUpperBound.IsNone && solverOptions.ConstraintVectorLowerBound.IsNone)
    && (solverOptions.ConstraintMatrixEq.IsNone && solverOptions.ConstraintVectorEq.IsNone) then
    failwith "Must provide at least one Matrix/Vector pair for inequality/equality contraints"

  if solverOptions.ObjectiveFunction.IsEmpty then
    failwith "Objective function cannot be empty"

  if solverOptions.VariableUpperBound.IsEmpty then
    failwith "Variable upper bound values cannot be empty"

  if solverOptions.VariableLowerBound.IsEmpty then
    failwith "Variable lower bound values cannot be empty"

  // check inequality matrix dimensions against bounds
  match (solverOptions.ConstraintMatrix, solverOptions.ConstraintVectorLowerBound, solverOptions.ConstraintVectorUpperBound) with
  | _, Some lb, Some ub when lb.Length <> ub.Length ->
    failwithf "Constraint vector dimensions should be equal.\nLower Bound Length: %i\nUpper Bound Length: %i" lb.Length ub.Length
  | _ -> ()

  // Check Objective function dimensions with variable bounds dimensions
  match (solverOptions.ObjectiveFunction.Length, solverOptions.VariableLowerBound.Length, solverOptions.VariableUpperBound.Length) with
  | _ , lb, ub when lb <> ub ->
    failwithf "Variable vector dimensions should be equal.\nLower Bound Length: %i\nUpper Bound Length: %i" lb ub
  | obj, _ , ub when obj <> ub ->
    failwithf "Variable vector dimensions should be equal.\nUpper Bound Length: %i\nObjective Function Length: %i" ub obj
  | obj, lb, _ when obj <> lb ->
    failwithf "Variable vector dimensions should be equal.\nLower Bound Length: %i\nObjective Function Length: %i" lb obj
  | _ -> ()

  // Variables
  let vars =
    match solverOptions.SolverAlgorithm with
      | LP lp ->
          [ for i in 0 .. (solverOptions.VariableLowerBound.Length-1) -> solver.MakeNumVar(solverOptions.VariableLowerBound.[i], solverOptions.VariableUpperBound.[i], (sprintf "var[%i]" i ) ) ]
      | IP ip ->
          [ for i in 0 .. (solverOptions.VariableLowerBound.Length-1) -> solver.MakeIntVar(solverOptions.VariableLowerBound.[i], solverOptions.VariableUpperBound.[i], (sprintf "var[%i]" i ) ) ]

  // Constraints
  let cols = [ for i in 0 .. (solverOptions.VariableLowerBound.Length-1) -> i ]   // generate column index selectors

  // Inequality Constraints
  match (solverOptions.ConstraintMatrix, solverOptions.ConstraintVectorLowerBound, solverOptions.ConstraintVectorUpperBound) with
  | (None, Some lb, Some ub) ->
      failwithf "Matrix for Equality Constraints undefined."
  | (Some mat, None, Some ub) ->
      for row = 0 to (ub.Length-1) do
          // generate constraint operands based on indices
          let constraintOperands = List.map (fun c ->  vars.[c] * mat.[c].[row]) cols
          let linearExp = List.reduce (+) constraintOperands

          // create the constraint
          let rangeConstraint = RangeConstraint(linearExp, Double.NegativeInfinity, ub.[row])
          solver.Add(rangeConstraint) |> ignore
  | (Some mat, Some lb, None) ->
      for row = 0 to (lb.Length-1) do
          // generate constraint operands based on indices
          let constraintOperands = List.map (fun c ->  vars.[c] * mat.[c].[row]) cols
          let linearExp = List.reduce (+) constraintOperands

          // create the constraint
          let rangeConstraint = RangeConstraint(linearExp, lb.[row], Double.PositiveInfinity)
          solver.Add(rangeConstraint) |> ignore
  | (Some mat, Some lb, Some ub) ->
      for row = 0 to (ub.Length-1) do
          // generate constraint operands based on indices
          let constraintOperands = List.map (fun c ->  vars.[c] * mat.[c].[row]) cols
          let linearExp = List.reduce (+) constraintOperands

          // create the constraint
          let rangeConstraint = RangeConstraint(linearExp, lb.[row], ub.[row])
          solver.Add(rangeConstraint) |> ignore
  | _ -> ()


  // Equality Constraints
  match (solverOptions.ConstraintMatrixEq, solverOptions.ConstraintVectorEq) with
  | (None, Some b) ->
      failwithf "Matrix for Equality Constraints undefined."
  | (Some mat, Some vec) ->
      for row = 0 to (vec.Length-1) do
          // generate constraint operands based on indices
          let constraintOperands = List.map (fun c ->  vars.[c] * mat.[c].[row]) cols
          let linearExp = List.reduce (+) constraintOperands

          // create the constraint
          let equalityConstraint = RangeConstraint(linearExp, vec.[row], vec.[row])
          solver.Add(equalityConstraint) |> ignore
  | _ -> ()

  // Objective
  let objectiveOperands = List.map (fun c ->  solverOptions.ObjectiveFunction.[c] * vars.[c]) cols
  let objectiveExp = List.reduce (+) objectiveOperands

  match solverOptions.SolverGoal with
  | Minimize ->
      solver.Minimize(objectiveExp)
  | Maximize ->
      solver.Maximize(objectiveExp)

  solver

/// Solves the optimization problem and prints the results to console
let SolverSummary (solver:Solver) =
  let resultStatus = solver.Solve();

  match resultStatus with
  | status when status <> Solver.OPTIMAL ->
      printfn "The problem does not have an optimal solution!"
      exit 0
  | _ ->
      printfn "\nProblem solved in %d milliseconds" (solver.WallTime())
      printfn "Iterations: %i\n" (solver.Iterations())

  printfn "Objective: %f" (solver.Objective().Value())
  for i in 0 .. (solver.NumVariables()-1) do
    printfn "%-10s: %f " (sprintf "var[%i]" i) ((solver.LookupVariableOrNull(sprintf "var[%i]" i)).SolutionValue())

  solver

