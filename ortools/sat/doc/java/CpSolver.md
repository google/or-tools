# [CpSolver](..//CpSolver.java#L21)

**Type:** `public` `final` `class`

Wrapper around the SAT solver. 

<p>This class proposes different solve() methods, as well as accessors to get the values of 
variables in the best solution, as well as general statistics of the search. 












## [CpSolver](..//CpSolver.java#L27)

**Type:** `public`

Main construction of the CpSolver class. 











## [solve](..//CpSolver.java#L32)

**Type:** `public` `CpSolverStatus`

Solves the given module, and returns the solve status. 











## [solveWithSolutionCallback](..//CpSolver.java#L38)

**Type:** `public` `CpSolverStatus`

Solves a problem and passes each solution found to the callback. 











## [searchAllSolutions](..//CpSolver.java#L46)

**Type:** `public` `CpSolverStatus`

Searches for all solutions of a satisfiability problem. 

<p>This method searches for all feasible solutions of a given model. Then it feeds the 
solutions to the callback. 

<p>Note that the model cannot have an objective. 





|Parameter Name|Description|
|-----|-----|
|model|the model to solve|
|cb|the callback that will be called at each solution|


**Returned Value:** the status of the solve (FEASIBLE, INFEASIBLE...)  








## [objectiveValue](..//CpSolver.java#L65)

**Type:** `public` `double`

Returns the best objective value found during search. 











## [bestObjectiveBound](..//CpSolver.java#L71)

**Type:** `public` `double`

Returns the best lower bound found when minimizing, of the best upper bound found when 
maximizing. 












## [value](..//CpSolver.java#L78)

**Type:** `public` `long`

Returns the value of a variable in the last solution found. 











## [booleanValue](..//CpSolver.java#L83)

**Type:** `public` `Boolean`

Returns the Boolean value of a literal in the last solution found. 











## [response](..//CpSolver.java#L93)

**Type:** `public` `CpSolverResponse`

Returns the internal response protobuf that is returned internally by the SAT solver. 











## [numBranches](..//CpSolver.java#L98)

**Type:** `public` `long`

Returns the number of branches explored during search. 











## [numConflicts](..//CpSolver.java#L103)

**Type:** `public` `long`

Returns the number of conflicts created during search. 











## [wallTime](..//CpSolver.java#L108)

**Type:** `public` `double`

Returns the wall time of the search. 











## [userTime](..//CpSolver.java#L113)

**Type:** `public` `double`

Returns the user time of the search. 











## [getParameters](..//CpSolver.java#L118)

**Type:** `public` `SatParameters.Builder`

Returns the builder of the parameters of the SAT solver for modification. 











## [responseStats](..//CpSolver.java#L123)

**Type:** `public` `String`

Returns some statistics on the solution found as a string. 











