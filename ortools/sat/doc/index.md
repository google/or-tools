# Using the CP-SAT solver



This document presents modeling recipes for the CP-SAT solver. These are grouped
by type:

-   [boolean logic](boolean_logic.md)
-   [integer arithmetic](integer_arithmetic.md)
-   [scheduling](scheduling.md)
-   [solving a CP-SAT model](solver.md)

Code samples are given in C++ and python.

## Python code samples

The Python interface to the CP-SAT solver is implemented using two classes. The
CpModel class proposes modeling methods that creates variables, or add
constraints. The CpSolver class encapsulates the solve API. and offers helpers
to access the solution found by the solve.

```python
from ortools.sat.python import cp_model

model = cp_model.CpModel()

x = model.NewBoolVar('x')
```

## C++ code samples

The interface to the C++ CP-SAT solver is through the cp_model.proto. To help
creating variables and constraints, we propose some inline helpers to improve
readability.

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"

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
```
