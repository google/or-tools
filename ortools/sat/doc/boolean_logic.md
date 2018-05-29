# Boolean logic recipes for the CP-SAT solver.



## Introduction

The CP-SAT solver can express Boolean variables and constraints. A **Boolean
variable** is an integer variable constrained to be either 0 or 1. A **literal**
is either a Boolean variable or its negation: 0 negated is 1, and vice versa.
See
https://en.wikipedia.org/wiki/Boolean_satisfiability_problem#Basic_definitions_and_terminology.

## Boolean variables and literals

We can create a Boolean variable 'x' and a literal 'not_x' equal to the logical
negation of 'x'.

### Python code

```python
from google3.util.operations_research.sat.python import cp_model

model = cp_model.CpModel()

x = model.NewBoolVar('x')

not_x = x.Not()
```

### C++ code

```cpp
#include "util/operations_research/sat/cp_model.pb.h"
#include "util/operations_research/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

CpModelProto cp_model;

auto new_boolean_variable = [&cp_model]() {
  const int index = cp_model.variables_size();
  IntegerVariableProto* const var = cp_model.add_variables();
  var->add_domain(0);
  var->add_domain(1);
  return index;
};

const int x = new_boolean_variable();
const int not_x = NegatedRef(x);

}  // namespace sat
}  // namespace operations_research
```

## Boolean constraints

For Boolean variables x and y, the following are elementary Boolean constraints:

-   or(x_1, .., x_n)
-   and(x_1, .., x_n)
-   xor(x_1, .., x_n)
-   not(x)

More complicated constraints can be built up by combining these elementary
constraints. For instance, we can add a constraint Or(x, not(y)).

### Python code

```python
from google3.util.operations_research.sat.python import cp_model

model = cp_model.CpModel()

x = model.NewBoolVar('x')
y = model.NewBoolVar('y')

model.AddBoolOr([x, y.Not()])
```

### C++ code

```cpp
#include "util/operations_research/sat/cp_model.pb.h"
#include "util/operations_research/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

CpModelProto cp_model;

auto new_boolean_variable = [&cp_model]() {
  const int index = cp_model.variables_size();
  IntegerVariableProto* const var = cp_model.add_variables();
  var->add_domain(0);
  var->add_domain(1);
  return index;
};

auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
  BooleanArgumentProto* const bool_or =
      model.add_constraints()->mutable_bool_or();
  for (const int lit : literals) {
    mutable_bool_or->add_literals(lit);
  }
};

const int x = new_boolean_variable();
const int y = new_boolean_variable();
add_bool_or({x, NegatedRef(y)});

}  // namespace sat
}  // namespace operations_research
```

## Reified constraints

The CP-SAT solver supports *half-reified* constraints, also called
*implications*, which are of the form:

    x implies constraint

where the constraint must hold if *x* is true.

Please note that this is not an equivalence relation. The constraint can still
be true if *x* is false.

So we can write b => And(x, not y). That is, if b is true, then x is true and y
is false. Note that in this particular example, there are multiple ways to
express this constraint: you can also write (b => x) and (b => not y), which
then is written as Or(not b, x) and Or(not b, not y).

### Python code

```python
from google3.util.operations_research.sat.python import cp_model

model = cp_model.CpModel()

x = model.NewBoolVar('x')
y = model.NewBoolVar('y')
b = model.NewBoolVar('b')

# First version using a half-reified bool and.
model.AddBoolAnd([x, y.Not()]).OnlyEnforceIf(b)

# Second version using implications.
model.AddImplication(b, x)
model.AddImplication(b, y.Not())

# Third version using bool or.
model.AddBoolOr([b.Not(), x])
model.AddBoolOt([b.Not(), y.Not()])
```

### C++ code

```cpp
#include "util/operations_research/sat/cp_model.pb.h"
#include "util/operations_research/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

CpModelProto cp_model;

auto new_boolean_variable = [&cp_model]() {
  const int index = cp_model.variables_size();
  IntegerVariableProto* const var = cp_model.add_variables();
  var->add_domain(0);
  var->add_domain(1);
  return index;
};

auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
  BooleanArgumentProto* const bool_or =
      model.add_constraints()->mutable_bool_or();
  for (const int lit : literals) {
    mutable_bool_or->add_literals(lit);
  }
};

auto add_reified_bool_and = [&cp_model](const std::vector<int>& literals,
                                        const int literal) {
  Constraint* const ct = model.add_constraints();
  ct->add_enforcement_literal(literal);
  for (const int lit : literals) {
    ct->mutable_bool_and()->add_literals(lit);
  }
};

const int x = new_boolean_variable();
const int y = new_boolean_variable();
const int b = new_boolean_variable();

// First version using a half-reified bool and.
add_reified_bool_and({x, NegatedRef(y)}, b);

// Second version using implications.
add_bool_or({NegatedRef(b), x});
add_bool_or({NegatedRef(b), NegatedRef(y)});

}  // namespace sat
}  // namespace operations_research
```
