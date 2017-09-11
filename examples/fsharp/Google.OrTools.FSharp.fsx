namespace Google.OrTools.FSharp


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



