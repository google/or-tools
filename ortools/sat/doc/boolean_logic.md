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


def LiteralSample():
  model = cp_model.CpModel()
  x = model.NewBoolVar('x')
  not_x = x.Not()
  print(x)
  print(not_x)


LiteralSample()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void LiteralSample() {
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
  LOG(INFO) << "x = " << x << ", not(x) = " << not_x;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::LiteralSample();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.*;

public class LiteralSample {

  static {
    System.loadLibrary("jniortools");
  }

  static void LiteralSample()
  {
    CpModel model = new CpModel();
    IntVar x = model.newBoolVar("x");
    ILiteral not_x = x.not();
    System.out.println(not_x.shortString());
  }

  public static void main(String[] args) throws Exception {
    LiteralSample();
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void LiteralSample()
  {
    CpModel model = new CpModel();
    IntVar x = model.NewBoolVar("x");
    ILiteral not_x = x.Not();
  }

  static void Main() {
    LiteralSample();
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


def BoolOrSample():
  model = cp_model.CpModel()

  x = model.NewBoolVar('x')
  y = model.NewBoolVar('y')

  model.AddBoolOr([x, y.Not()])


BoolOrSample()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void BoolOrSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
    BoolArgumentProto* const bool_or =
        cp_model.add_constraints()->mutable_bool_or();
    for (const int lit : literals) {
      bool_or->add_literals(lit);
    }
  };

  const int x = new_boolean_variable();
  const int y = new_boolean_variable();
  add_bool_or({x, NegatedRef(y)});
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::BoolOrSample();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.*;

public class BoolOrSample {

  static {
    System.loadLibrary("jniortools");
  }

  static void BoolOrSample()
  {
    CpModel model = new CpModel();
    IntVar x = model.newBoolVar("x");
    IntVar y = model.newBoolVar("y");
    model.addBoolOr(new ILiteral[] {x, y.not()});
  }

  public static void main(String[] args) throws Exception {
    BoolOrSample();
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void BoolOrSample()
  {
    CpModel model = new CpModel();
    IntVar x = model.NewBoolVar("x");
    IntVar y = model.newBoolVar("y");
    model.AddBoolOr(new ILiteral[] {x, y.Not()});
  }

  static void Main() {
    BoolOrSample();
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


def ReifiedSample():
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


ReifiedSample()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void ReifiedSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
    BoolArgumentProto* const bool_or =
        cp_model.add_constraints()->mutable_bool_or();
    for (const int lit : literals) {
      bool_or->add_literals(lit);
    }
  };

  auto add_reified_bool_and = [&cp_model](const std::vector<int>& literals,
                                          const int literal) {
    ConstraintProto* const ct = cp_model.add_constraints();
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

  // Second version using bool or.
  add_bool_or({NegatedRef(b), x});
  add_bool_or({NegatedRef(b), NegatedRef(y)});
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ReifiedSample();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.*;

public class ReifiedSample {

  static {
    System.loadLibrary("jniortools");
  }

  static void ReifiedSample()
  {
    CpModel model = new CpModel();

    IntVar x = model.newBoolVar("x");
    IntVar y = model.newBoolVar("y");
    IntVar b = model.newBoolVar("b");

    //  First version using a half-reified bool and.
    model.addBoolAnd(new ILiteral[] {x, y.not()}).onlyEnforceIf(b);

    // Second version using implications.
    model.addImplication(b, x);
    model.addImplication(b, y.not());

    // Third version using bool or.
    model.addBoolOr(new ILiteral[] {b.not(), x});
    model.addBoolOr(new ILiteral[] {b.not(), y.not()});
  }

  public static void main(String[] args) throws Exception {
    ReifiedSample();
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void ReifiedSample()
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

  static void Main() {
    ReifiedSample();
  }
}
```
