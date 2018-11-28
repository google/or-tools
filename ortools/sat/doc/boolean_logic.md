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
"""Code sample to demonstrate Boolean variable and literals."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def LiteralSampleSat():
  model = cp_model.CpModel()
  x = model.NewBoolVar('x')
  not_x = x.Not()
  print(x)
  print(not_x)


LiteralSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void LiteralSampleSat() {
  CpModelBuilder cp_model;

  const BoolVar x = cp_model.NewBoolVar().WithName("x");
  const BoolVar not_x = Not(x);
  LOG(INFO) << "x = " << x << ", not(x) = " << not_x;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::LiteralSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.Literal;

/** Code sample to demonstrate Boolean variable and literals. */
public class LiteralSampleSat {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    CpModel model = new CpModel();
    IntVar x = model.newBoolVar("x");
    Literal notX = x.not();
    System.out.println(notX.getShortString());
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class LiteralSampleSat
{
  static void Main()
  {
    CpModel model = new CpModel();
    IntVar x = model.NewBoolVar("x");
    ILiteral not_x = x.Not();
  }
}
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
"""Code sample to demonstrates a simple Boolean constraint."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def BoolOrSampleSat():
  model = cp_model.CpModel()

  x = model.NewBoolVar('x')
  y = model.NewBoolVar('y')

  model.AddBoolOr([x, y.Not()])


BoolOrSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void BoolOrSampleSat() {
  CpModelBuilder cp_model;

  const BoolVar x = cp_model.NewBoolVar();
  const BoolVar y = cp_model.NewBoolVar();
  cp_model.AddBoolOr({x, Not(y)});
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::BoolOrSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.Literal;

/** Code sample to demonstrates a simple Boolean constraint. */
public class BoolOrSampleSat {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    CpModel model = new CpModel();
    IntVar x = model.newBoolVar("x");
    IntVar y = model.newBoolVar("y");
    model.addBoolOr(new Literal[] {x, y.not()});
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class BoolOrSampleSat
{
  static void Main()
  {
    CpModel model = new CpModel();

    IntVar x = model.NewBoolVar("x");
    IntVar y = model.NewBoolVar("y");

    model.AddBoolOr(new ILiteral[] { x, y.Not() });
  }
}
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
"""Simple model with a reified constraint."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def ReifiedSampleSat():
  """Showcase creating a reified constraint."""
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
  model.AddBoolOr([b.Not(), y.Not()])


ReifiedSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void ReifiedSampleSat() {
  CpModelBuilder cp_model;

  const BoolVar x = cp_model.NewBoolVar();
  const BoolVar y = cp_model.NewBoolVar();
  const BoolVar b = cp_model.NewBoolVar();

  // First version using a half-reified bool and.
  cp_model.AddBoolAnd({x, Not(y)}).OnlyEnforceIf(b);

  // Second version using implications.
  cp_model.AddImplication(b, x);
  cp_model.AddImplication(b, Not(y));

  // Third version using bool or.
  cp_model.AddBoolOr({Not(b), x});
  cp_model.AddBoolOr({Not(b), Not(y)});
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ReifiedSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.Literal;

/**
 * Reification is the action of associating a Boolean variable to a constraint. This boolean
 * enforces or prohibits the constraint according to the value the Boolean variable is fixed to.
 *
 * <p>Half-reification is defined as a simple implication: If the Boolean variable is true, then the
 * constraint holds, instead of an complete equivalence.
 *
 * <p>The SAT solver offers half-reification. To implement full reification, two half-reified
 * constraints must be used.
 */
public class ReifiedSampleSat {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    CpModel model = new CpModel();

    IntVar x = model.newBoolVar("x");
    IntVar y = model.newBoolVar("y");
    IntVar b = model.newBoolVar("b");

    // Version 1: a half-reified boolean and.
    model.addBoolAnd(new Literal[] {x, y.not()}).onlyEnforceIf(b);

    // Version 2: implications.
    model.addImplication(b, x);
    model.addImplication(b, y.not());

    // Version 3: boolean or.
    model.addBoolOr(new Literal[] {b.not(), x});
    model.addBoolOr(new Literal[] {b.not(), y.not()});
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class ReifiedSampleSat
{
  static void Main()
  {
    CpModel model = new CpModel();

    IntVar x = model.NewBoolVar("x");
    IntVar y = model.NewBoolVar("y");
    IntVar b = model.NewBoolVar("b");

    //  First version using a half-reified bool and.
    model.AddBoolAnd(new ILiteral[] {x, y.Not()}).OnlyEnforceIf(b);

    // Second version using implications.
    model.AddImplication(b, x);
    model.AddImplication(b, y.Not());

    // Third version using bool or.
    model.AddBoolOr(new ILiteral[] {b.Not(), x});
    model.AddBoolOr(new ILiteral[] {b.Not(), y.Not()});
  }
}
```
