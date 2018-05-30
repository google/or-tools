# Using the CP-SAT solver



This document presents modeling recipes for the CP-SAT solver. These are grouped
by type:

-   [boolean logic](boolean_logic.md)
-   [integer arithmetic](integer_arithmetic.md)
-   [scheduling](scheduling.md)
-   [solving a CP-SAT model](solver.md)

Code samples are given in C++ and python. Each language have different
requirements for the code samples.

## Python code samples

The Python interface to the CP-SAT solver is implemented using two classes.

*   The **CpModel** class proposes modeling methods that creates variables, or
    add constraints. This class completely hides the underlying *CpModelProto*
    used to store the model.
*   The **CpSolver** class encapsulates the solve API. and offers
    helpers to access the solution found by the solve.

```python
from ortools.sat.python import cp_model

model = cp_model.CpModel()

x = model.NewBoolVar('x')
```

## C++ code samples

The interface to the C++ CP-SAT solver is implemented through the
**CpModelProto** class described in *ortools/sat/cp_model.proto*.

To help creating variables and constraints, we propose some inline helpers to
improve readability.

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

void CodeSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  const int x = new_boolean_variable();
}

}  // namespace sat
}  // namespace operations_research
```

## C\# code

The C\# code implements the same interface as the python code, with a
**CpModel**, and a **CpSolver** class.

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void CodeSample() {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the Boolean variable.
    IntVar x = model.NewBoolVar("x");
  }

  static void Main() {
    CodeSample();
  }
}
```
