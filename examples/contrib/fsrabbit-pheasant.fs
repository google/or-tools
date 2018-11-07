(* Here is a very simple Constraint Programming problem:

   Knowing that we see 56 legs and 20 heads, how many pheasants and rabbits
   are we looking at ?
*)

open Google.OrTools.ConstraintSolver

let solver = new Solver("pheasant")

// Variables
let p = solver.MakeIntVar(0L, 20L, "pheasant")
let r = solver.MakeIntVar(0L, 20L, "rabbit")

// Constraints
let legs = solver.MakeSum(solver.MakeProd(p, 2L), solver.MakeProd(r, 4L))
let heads = solver.MakeSum(p, r)

let constraintLegs = solver.MakeEquality(legs, 56)
let constraintHeads = solver.MakeEquality(heads, 20)

solver.Add(constraintLegs)
solver.Add(constraintHeads)

let vars = new IntVarVector([|p; r|])
let db = solver.MakePhase(vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE)

// Solve
solver.NewSearch(db)

while (solver.NextSolution()) do
  printfn "rabbits: %d, pheasants: %d" (r.Value()) (p.Value())

solver.EndSearch()
