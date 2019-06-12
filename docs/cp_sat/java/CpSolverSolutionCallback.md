# [SolutionCallback](..//CpSolverSolutionCallback.java#L17)

**Type:** `public` `class` `CpSolverSolutionCallback` `extends`

Parent class to create a callback called at each solution. 

<p>From the parent class, it inherits the methods: 

<p>{@code long numBooleans()} to query the number of boolean variables created. 

<p>{@code long numBranches()} to query the number of branches explored so far. 

<p>{@code long numConflicts()} to query the number of conflicts created so far. 

<p>{@code long numBinaryPropagations()} to query the number of boolean propagations in the SAT 
solver so far. 

<p>{@code long numIntegerPropagations()} to query the number of integer propagations in the SAT 
solver so far. 

<p>{@code double wallTime()} to query wall time passed in the search so far. 

<p>{@code double userTime()} to query the user time passed in the search so far. 

<p>{@code long objectiveValue()} to get the best objective value found so far. 












## [value](..//CpSolverSolutionCallback.java#L40)

**Type:** `public` `long`

Returns the value of the variable in the current solution. 











## [booleanValue](..//CpSolverSolutionCallback.java#L46)

**Type:** `public` `Boolean`

Returns the Boolean value of the literal in the current solution. 











## [onSolutionCallback](..//CpSolverSolutionCallback.java#L51)

**Type:** `@Override` `public` `void`

Callback method to override. It will be called at each new solution. 











