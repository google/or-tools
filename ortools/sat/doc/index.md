# Using the CP-SAT solver



This document presents modeling recipes for the CP-SAT solver. These are grouped
by type:

-   [boolean logic](boolean_logic.md)
-   [integer arithmetic](integer_arithmetic.md)
-   [channeling constraints](channeling.md)
-   [scheduling](scheduling.md)
-   [Using the CP-SAT solver](solver.md)
-   [Reference manual for the CpModel and CpSolver classes](reference.md)

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
**CpModelBuilder** class described in *ortools/sat/cp_model.h*.
This class is just a helper to fill in the cp_model protobuf.

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void CodeSample() {
  CpModelBuilder cp_model;

  const IntVar x = cp_model.NewBoolVar().WithName("x");
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
**CpModel** and a **CpSolver** class.

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
**CpModel** and a **CpSolver** class.


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
