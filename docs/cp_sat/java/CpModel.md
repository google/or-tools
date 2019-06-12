# [CpModel](..//CpModel.java#L36)

**Type:** `public` `final` `class`

Main modeling class. 

<p>Proposes a factory to create all modeling objects understood by the SAT solver. 












# [CpModelException](..//CpModel.java#L48)

**Type:** `public` `static` `class` `MismatchedArrayLengths` `extends`

Exception thrown when parallel arrays have mismatched lengths. 











# [CpModelException](..//CpModel.java#L55)

**Type:** `public` `static` `class` `WrongLength` `extends`

Exception thrown when an array has a wrong length. 











## [newIntVar](..//CpModel.java#L68)

**Type:** `public` `IntVar`

Creates an integer variable with domain [lb, ub]. 











## [newIntVarFromDomain](..//CpModel.java#L74)

**Type:** `public` `IntVar`

Creates an integer variable with given domain. 





|Parameter Name|Description|
|-----|-----|
|domain|an instance of the Domain class.|
|name|the name of the variable|


**Returned Value:** a variable with the given domain.  








## [newBoolVar](..//CpModel.java#L84)

**Type:** `public` `IntVar`

Creates a Boolean variable with the given name. 











## [newConstant](..//CpModel.java#L89)

**Type:** `public` `IntVar`

Creates a constant variable. 











## [addBoolOr](..//CpModel.java#L96)

**Type:** `public` `Constraint`

Adds == true.











## [addBoolAnd](..//CpModel.java#L106)

**Type:** `public` `Constraint`

Adds == true.











## [addBoolXor](..//CpModel.java#L116)

**Type:** `public` `Constraint`

Adds == true.











## [addImplication](..//CpModel.java#L126)

**Type:** `public` `Constraint`

Adds => b.











## [addLinearExpressionInDomain](..//CpModel.java#L133)

**Type:** `public` `Constraint`

Adds in domain.











## [addLinearConstraint](..//CpModel.java#L147)

**Type:** `public` `Constraint`

Adds <= expr <= ub.











## [addEquality](..//CpModel.java#L152)

**Type:** `public` `Constraint`

Adds == value.











## [addEquality](..//CpModel.java#L157)

**Type:** `public` `Constraint`

Adds == right.











## [addEqualityWithOffset](..//CpModel.java#L162)

**Type:** `public` `Constraint`

Adds + offset == right.











## [addLessOrEqual](..//CpModel.java#L167)

**Type:** `public` `Constraint`

Adds <= value.











## [addLessOrEqual](..//CpModel.java#L172)

**Type:** `public` `Constraint`

Adds <= right.











## [addLessThan](..//CpModel.java#L177)

**Type:** `public` `Constraint`

Adds < value.











## [addLessThan](..//CpModel.java#L182)

**Type:** `public` `Constraint`

Adds < right.











## [addLessOrEqualWithOffset](..//CpModel.java#L187)

**Type:** `public` `Constraint`

Adds + offset <= right.











## [addGreaterOrEqual](..//CpModel.java#L193)

**Type:** `public` `Constraint`

Adds >= value.











## [addGreaterOrEqual](..//CpModel.java#L198)

**Type:** `public` `Constraint`

Adds >= right.











## [addGreaterThan](..//CpModel.java#L203)

**Type:** `public` `Constraint`

Adds > value.











## [addGreaterThan](..//CpModel.java#L208)

**Type:** `public` `Constraint`

Adds > right.











## [addGreaterOrEqualWithOffset](..//CpModel.java#L213)

**Type:** `public` `Constraint`

Adds + offset >= right.











## [addDifferent](..//CpModel.java#L219)

**Type:** `public` `Constraint`

Adds != value.











## [addDifferent](..//CpModel.java#L226)

**Type:** `public` `Constraint`

Adds != right.











## [addDifferentWithOffset](..//CpModel.java#L232)

**Type:** `public` `Constraint`

Adds + offset != right.











## [addAllDifferent](..//CpModel.java#L242)

**Type:** `public` `Constraint`

Adds .

<p>This constraint forces all variables to have different values. 





|Parameter Name|Description|
|-----|-----|
|variables|a list of integer variables|


**Returned Value:** an instance of the Constraint class  








## [addElement](..//CpModel.java#L258)

**Type:** `public` `Constraint`

Adds the element constraint: == target.











## [addElement](..//CpModel.java#L270)

**Type:** `public` `Constraint`

Adds the element constraint: == target.











## [addElement](..//CpModel.java#L282)

**Type:** `public` `Constraint`

Adds the element constraint: == target.











## [addCircuit](..//CpModel.java#L295)

**Type:** `public` `Constraint`

Adds heads, literals).

<p>Adds a circuit constraint from a sparse list of arcs that encode the graph. 

<p>A circuit is a unique Hamiltonian path in a subgraph of the total graph. In case a node 'i' 
is not in the path, then there must be a loop arc -> i'associated with a true 
literal. Otherwise this constraint will fail. 





|Parameter Name|Description|
|-----|-----|
|tails|the tails of all arcs|
|heads|the heads of all arcs|
|literals|the literals that control whether an arc is selected or not|


**Returned Value:** an instance of the Constraint class




|Exception|Description|
|-----|-----|
|MismatchedArrayLengths|if the arrays have different sizes  |





## [addAllowedAssignments](..//CpModel.java#L333)

**Type:** `public` `Constraint`

Adds tuplesList).

<p>An AllowedAssignments constraint is a constraint on an array of variables that forces, when 
all variables are fixed to a single value, that the corresponding list of values is equal to 
one of the tuples of the tupleList. 





|Parameter Name|Description|
|-----|-----|
|variables|a list of variables|
|tuplesList|a list of admissible tuples. Each tuple must have the same length as the variables, and the ith value of a tuple corresponds to the ith variable.|


**Returned Value:** an instance of the Constraint class




|Exception|Description|
|-----|-----|
|WrongLength|if one tuple does not have the same length as the variables  |





## [addAllowedAssignments](..//CpModel.java#L366)

**Type:** `public` `Constraint`

Adds tuplesList).









**Reference:** #addAllowedAssignments(IntVar[], long[][]) addAllowedAssignments  





## [addForbiddenAssignments](..//CpModel.java#L391)

**Type:** `public` `Constraint`

Adds tuplesList).

<p>A ForbiddenAssignments constraint is a constraint on an array of variables where the list of 
impossible combinations is provided in the tuples list. 





|Parameter Name|Description|
|-----|-----|
|variables|a list of variables|
|tuplesList|a list of forbidden tuples. Each tuple must have the same length as the variables, and the ith value of a tuple corresponds to the ith variable.|


**Returned Value:** an instance of the Constraint class




|Exception|Description|
|-----|-----|
|WrongLength|if one tuple does not have the same length as the variables  |





## [addForbiddenAssignments](..//CpModel.java#L411)

**Type:** `public` `Constraint`

Adds tuplesList).









**Reference:** #addForbiddenAssignments(IntVar[], long[][]) addForbiddenAssignments  





## [addAutomaton](..//CpModel.java#L424)

**Type:** `public` `Constraint`

Adds an automaton constraint. 

<p>An automaton constraint takes a list of variables (of size n), an initial state, a set of 
final states, and a set of transitions. A transition is a triplet ('tail', 'transition', 
'head'), where 'tail' and 'head' are states, and 'transition' is the label of an arc from 
'head' to 'tail', corresponding to the value of one variable in the list of variables. 

<p>This automaton will be unrolled into a flow with n + 1 phases. Each phase contains the 
possible states of the automaton. The first state contains the initial state. The last phase 
contains the final states. 

<p>Between two consecutive phases i and i + 1, the automaton creates a set of arcs. For each 
transition (tail, label, head), it will add an arc from the state 'tail' of phase i and the 
state 'head' of phase i + 1. This arc labeled by the value 'label' of the variables 
'variables[i]'. That is, this arc can only be selected if 'variables[i]' is assigned the value 
'label'. 

<p>A feasible solution of this constraint is an assignment of variables such that, starting 
from the initial state in phase 0, there is a path labeled by the values of the variables that 
ends in one of the final states in the final phase. 





|Parameter Name|Description|
|-----|-----|
|transitionVariables|a non empty list of variables whose values correspond to the labels of the arcs traversed by the automaton|
|startingState|the initial state of the automaton|
|finalStates|a non empty list of admissible final states|
|transitions|a list of transition for the automaton, in the following format (currentState, variableValue, nextState)|


**Returned Value:** an instance of the Constraint class




|Exception|Description|
|-----|-----|
|WrongLength|if one transition does not have a length of 3  |





## [addInverse](..//CpModel.java#L477)

**Type:** `public` `Constraint`

Adds inverseVariables).

<p>An inverse constraint enforces that if 'variables[i]' is assigned a value 'j', then 
inverseVariables[j] is assigned a value 'i'. And vice versa. 





|Parameter Name|Description|
|-----|-----|
|variables|an array of integer variables|
|inverseVariables|an array of integer variables|


**Returned Value:** an instance of the Constraint class




|Exception|Description|
|-----|-----|
|MismatchedArrayLengths|if variables and inverseVariables have different length  |





## [addReservoirConstraint](..//CpModel.java#L504)

**Type:** `public` `Constraint`

Adds demands, minLevel, maxLevel).

<p>Maintains a reservoir level within bounds. The water level starts at 0, and at any non 
negative time , it must be between minLevel and maxLevel. Furthermore, this constraints expect 
all times variables to be non negative. If the variable times[i] is assigned a value t, then 
the current level changes by demands[i] (which is constant) at the time t. 

<p>Note that can be greater than 0, or can be less than 0. It 
just forces some demands to be executed at time 0 to make sure that we are within those bounds 
with the executed demands. Therefore, 
times[i] <= t) <= maxLevel}. 





|Parameter Name|Description|
|-----|-----|
|times|a list of positive integer variables which specify the time of the filling or emptying the reservoir|
|demands|a list of integer values that specifies the amount of the emptying or feeling|
|minLevel|at any non negative time, the level of the reservoir must be greater of equal than the min level|
|maxLevel|at any non negative time, the level of the reservoir must be less or equal than the max level|


**Returned Value:** an instance of the Constraint class




|Exception|Description|
|-----|-----|
|MismatchedArrayLengths|if times and demands have different length  |





## [addReservoirConstraint](..//CpModel.java#L545)

**Type:** `public` `Constraint`

Adds demands, minLevel, maxLevel).









**Reference:** #addReservoirConstraint(IntVar[], long[], long, long) Reservoir  





## [addReservoirConstraintWithActive](..//CpModel.java#L555)

**Type:** `public` `Constraint`

Adds demands, actives, minLevel, maxLevel).

<p>Maintains a reservoir level within bounds. The water level starts at 0, and at any non 
negative time , it must be between minLevel and maxLevel. Furthermore, this constraints expect 
all times variables to be non negative. If actives[i] is true, and if times[i] is assigned a 
value t, then the current level changes by demands[i] (which is constant) at the time t. 

<p>Note that can be greater than 0, or can be less than 0. It 
just forces some demands to be executed at time 0 to make sure that we are within those bounds 
with the executed demands. Therefore, 
actives[i] if times[i] <= t) <= maxLevel}. 





|Parameter Name|Description|
|-----|-----|
|times|a list of positive integer variables which specify the time of the filling or emptying the reservoir|
|demands|a list of integer values that specifies the amount of the emptying or feeling|
|actives|a list of integer variables that specifies if the operation actually takes place.|
|minLevel|at any non negative time, the level of the reservoir must be greater of equal than the min level|
|maxLevel|at any non negative time, the level of the reservoir must be less or equal than the max level|


**Returned Value:** an instance of the Constraint class




|Exception|Description|
|-----|-----|
|MismatchedArrayLengths|if times, demands, or actives have different length  |





## [addReservoirConstraintWithActive](..//CpModel.java#L605)

**Type:** `public` `Constraint`

Adds demands, actives, minLevel, maxLevel).









**Reference:** #addReservoirConstraintWithActive(IntVar[], long[], actives, long, long) Reservoir  





## [addMapDomain](..//CpModel.java#L615)

**Type:** `public` `void`

Adds == i + offset <=> booleans[i] == true for all i in [0, booleans.length).











## [addMinEquality](..//CpModel.java#L623)

**Type:** `public` `Constraint`

Adds == Min(vars).











## [addMaxEquality](..//CpModel.java#L634)

**Type:** `public` `Constraint`

Adds == Max(vars).











## [addDivisionEquality](..//CpModel.java#L645)

**Type:** `public` `Constraint`

Adds == num / denom,rounded towards 0. 











## [addAbsEquality](..//CpModel.java#L655)

**Type:** `public` `Constraint`

Adds == Abs(var).











## [addModuloEquality](..//CpModel.java#L665)

**Type:** `public` `Constraint`

Adds == var % mod.











## [addModuloEquality](..//CpModel.java#L675)

**Type:** `public` `Constraint`

Adds == var % mod.











## [addProductEquality](..//CpModel.java#L685)

**Type:** `public` `Constraint`

Adds == Product(vars).











## [newIntervalVar](..//CpModel.java#L699)

**Type:** `public` `IntervalVar`

Creates an interval variable from start, size, and end. 

<p>An interval variable is a constraint, that is itself used in other constraints like 
NoOverlap. 

<p>Internally, it ensures that + size == end.





|Parameter Name|Description|
|-----|-----|
|start|the start of the interval|
|size|the size of the interval|
|end|the end of the interval|
|name|the name of the interval variable|


**Returned Value:** An IntervalVar object  








## [newIntervalVar](..//CpModel.java#L717)

**Type:** `public` `IntervalVar`

Creates an interval variable with a fixed end. 









**Reference:** #newIntervalVar(IntVar, IntVar, IntVar, String) newIntervalVar  





## [newIntervalVar](..//CpModel.java#L727)

**Type:** `public` `IntervalVar`

Creates an interval variable with a fixed size. 









**Reference:** #newIntervalVar(IntVar, IntVar, IntVar, String) newIntervalVar  





## [newIntervalVar](..//CpModel.java#L737)

**Type:** `public` `IntervalVar`

Creates an interval variable with a fixed start. 









**Reference:** #newIntervalVar(IntVar, IntVar, IntVar, String) newIntervalVar  





## [newFixedInterval](..//CpModel.java#L746)

**Type:** `public` `IntervalVar`

Creates a fixed interval from its start and its size. 











## [newOptionalIntervalVar](..//CpModel.java#L753)

**Type:** `public` `IntervalVar`

Creates an optional interval variable from start, size, end, and isPresent. 

<p>An optional interval variable is a constraint, that is itself used in other constraints like 
NoOverlap. This constraint is protected by an literal that indicates if it is 
active or not. 

<p>Internally, it ensures that => start + size == end.





|Parameter Name|Description|
|-----|-----|
|start|the start of the interval. It can be an integer value, or an integer variable.|
|size|the size of the interval. It can be an integer value, or an integer variable.|
|end|the end of the interval. It can be an integer value, or an integer variable.|
|isPresent|a literal that indicates if the interval is active or not. A inactive interval is simply ignored by all constraints.|
|name|The name of the interval variable|


**Returned Value:** an IntervalVar object  








## [newOptionalIntervalVar](..//CpModel.java#L776)

**Type:** `public` `IntervalVar`

Creates an optional interval with a fixed end. 









**Reference:** #newOptionalIntervalVar(IntVar, IntVar, IntVar, Literal, String) newOptionalIntervalVar  





## [newOptionalIntervalVar](..//CpModel.java#L787)

**Type:** `public` `IntervalVar`

Creates an optional interval with a fixed size. 









**Reference:** #newOptionalIntervalVar(IntVar, IntVar, IntVar, Literal, String) newOptionalIntervalVar  





## [newOptionalIntervalVar](..//CpModel.java#L797)

**Type:** `public` `IntervalVar`

Creates an optional interval with a fixed start. 











## [newOptionalFixedInterval](..//CpModel.java#L805)

**Type:** `public` `IntervalVar`

Creates an optional fixed interval from start and size. 









**Reference:** #newOptionalIntervalVar(IntVar, IntVar, IntVar, Literal, String) newOptionalIntervalVar  





## [addNoOverlap](..//CpModel.java#L816)

**Type:** `public` `Constraint`

Adds .

<p>A NoOverlap constraint ensures that all present intervals do not overlap in time. 





|Parameter Name|Description|
|-----|-----|
|intervalVars|the list of interval variables to constrain|


**Returned Value:** an instance of the Constraint class  








## [addNoOverlap2D](..//CpModel.java#L833)

**Type:** `public` `Constraint`

Adds yIntervals).

<p>A NoOverlap2D constraint ensures that all present rectangles do not overlap on a plan. Each 
rectangle is aligned with the X and Y axis, and is defined by two intervals which represent its 
projection onto the X and Y axis. 





|Parameter Name|Description|
|-----|-----|
|xIntervals|the X coordinates of the rectangles|
|yIntervals|the Y coordinates of the rectangles|


**Returned Value:** an instance of the Constraint class  








## [addCumulative](..//CpModel.java#L856)

**Type:** `public` `Constraint`

Adds demands, capacity).

<p>This constraint enforces that: 

<p>{@code forall t: sum(demands[i] if (start(intervals[t]) <= t < end(intervals[t])) and (t is 
present)) <= capacity}. 





|Parameter Name|Description|
|-----|-----|
|intervals|the list of intervals|
|demands|the list of demands for each interval. Each demand must be a positive integer variable.|
|capacity|the maximum capacity of the cumulative constraint. It must be a positive integer variable.|


**Returned Value:** an instance of the Constraint class  








## [addCumulative](..//CpModel.java#L884)

**Type:** `public` `Constraint`

Adds demands, capacity)with fixed demands. 









**Reference:** #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative  





## [addCumulative](..//CpModel.java#L902)

**Type:** `public` `Constraint`

Adds demands, capacity)with fixed demands. 









**Reference:** #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative  





## [addCumulative](..//CpModel.java#L911)

**Type:** `public` `Constraint`

Adds demands, capacity)with fixed capacity. 









**Reference:** #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative  





## [addCumulative](..//CpModel.java#L929)

**Type:** `public` `Constraint`

Adds demands, capacity)with fixed demands and fixed capacity. 









**Reference:** #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative  





## [addCumulative](..//CpModel.java#L947)

**Type:** `public` `Constraint`

Adds demands, capacity)with fixed demands and fixed capacity. 









**Reference:** #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative  





## [minimize](..//CpModel.java#L957)

**Type:** `public` `void`

Adds a minimization objective of a linear expression. 











## [maximize](..//CpModel.java#L966)

**Type:** `public` `void`

Adds a maximization objective of a linear expression. 











## [addDecisionStrategy](..//CpModel.java#L978)

**Type:** `public` `void`

Adds varStr, domStr).











## [modelStats](..//CpModel.java#L990)

**Type:** `public` `String`

Returns some statistics on model as a string. 











## [validate](..//CpModel.java#L995)

**Type:** `public` `String`

Returns a non empty string explaining the issue if the model is invalid. 











## [getBuilder](..//CpModel.java#L1028)

**Type:** `public` `CpModelProto.Builder`

Returns the model builder. 











