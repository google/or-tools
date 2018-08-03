# Using the CP-SAT solver



This document presents modeling recipes for the CP-SAT solver. These are grouped
by type:

-   [boolean logic](boolean_logic.md)
-   [integer arithmetic](integer_arithmetic.md)
-   [channeling constraints](channeling.md)
-   [scheduling](scheduling.md)
-   [Using the CP-SAT solver](solver.md)

Code samples are given in C++ and python. Each language have different
requirements for the code samples.

## Python code samples

The Python interface to the CP-SAT solver is implemented using two classes.

*   The **CpModel** class proposes modeling methods that creates variables, or
    add constraints. This class completely hides the underlying *CpModelProto*
    used to store the model.
*   The **CpSolver** class encapsulates the solve API. and offers helpers to
    access the solution found by the solve.

```python
"""Creates a single Boolean variable."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def CodeSample():
  model = cp_model.CpModel()
  x = model.NewBoolVar('x')
  print(x)


CodeSample()
```

## C++ code samples

The interface to the C++ CP-SAT solver is implemented through the
**CpModelProto** class described in
*ortools/sat/cp_model.proto*.

We provide some inline helper methods to simplify variable and constraint
creation.

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

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
  LOG(INFO) << x;
}
}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::CodeSample();

  return EXIT_SUCCESS;
}
```

## Java code samples

The Java code implements the same interface as the Python code, with a
**CpModel**, and a **CpSolver** class.

```java
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;

public class CodeSample {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the Boolean variable.
    IntVar x = model.newBoolVar("x");
    System.out.println(x);
  }
}
```

## C\# code samples

The C\# code implements the same interface as the Python code, with a
**CpModel**, and a **CpSolver** class.


```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void CodeSample()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the Boolean variable.
    IntVar x = model.NewBoolVar("x");
  }

  static void Main()
  {
    CodeSample();
  }
}
```
