[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Python API](https://or-tools.github.io/docs/pdoc/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | -----------------------------------------------------------------------------------
# Boolean logic recipes for the CP-SAT solver.

https://developers.google.com/optimization/

## Introduction

The CP-SAT solver can express Boolean variables and constraints. A **Boolean
variable** is an integer variable constrained to be either 0 or 1. A **literal**
is either a Boolean variable or its negation: 0 negated is 1, and 1 negated is
0. See
https://en.wikipedia.org/wiki/Boolean_satisfiability_problem#Basic_definitions_and_terminology.

## Boolean variables and literals

We can create a Boolean variable `x` and a literal `not_x` equal to the logical
negation of `x`.

### Python code

```python
# Snippet from ortools/sat/samples/literal_sample_sat.py
"""Code sample to demonstrate Boolean variable and literals."""


from ortools.sat.python import cp_model


def literal_sample_sat():
  model = cp_model.CpModel()
  x = model.new_bool_var('x')
  not_x = ~x
  print(x)
  print(not_x)


literal_sample_sat()
```

### C++ code

```cpp
// Snippet from ortools/sat/samples/literal_sample_sat.cc
#include <stdlib.h>

#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void LiteralSampleSat() {
  CpModelBuilder cp_model;

  const BoolVar x = cp_model.NewBoolVar().WithName("x");
  const BoolVar not_x = ~x;
  LOG(INFO) << "x = " << x << ", not(x) = " << not_x;
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::LiteralSampleSat();
  return EXIT_SUCCESS;
}
```

### Java code

```java
// Snippet from ortools/sat/samples/LiteralSampleSat.java
package com.google.ortools.sat.samples;

import com.google.ortools.sat.BoolVar;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.Literal;
import com.google.ortools.Loader;

/** Code sample to demonstrate Boolean variable and literals. */
public class LiteralSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    BoolVar x = model.newBoolVar("x");
    Literal notX = x.not();
    System.out.println(notX);
  }
}
```

### C\# code

```csharp
// Snippet from ortools/sat/samples/LiteralSampleSat.cs
using System;
using Google.OrTools.Sat;

public class LiteralSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();
        BoolVar x = model.NewBoolVar("x");
        ILiteral not_x = x.Not();
    }
}
```

### Go code

```go
// Snippet from ortools/sat/samples/literal_sample_sat.go
// The literal_sample_sat command is a simple example of literals.
package main

import (
	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

func literalSampleSat() {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar().WithName("x")
	notX := x.Not()

	log.Infof("x = %d, x.Not() = %d", x.Index(), notX.Index())
}

func main() {
	literalSampleSat()
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
# Snippet from ortools/sat/samples/bool_or_sample_sat.py
"""Code sample to demonstrates a simple Boolean constraint."""


from ortools.sat.python import cp_model


def bool_or_sample_sat():
  model = cp_model.CpModel()

  x = model.new_bool_var('x')
  y = model.new_bool_var('y')

  model.add_bool_or([x, y.negated()])
  # The [] is not mandatory.
  # ~y is equivalent to y.negated()
  model.add_bool_or(x, ~y)


bool_or_sample_sat()
```

### C++ code

```cpp
// Snippet from ortools/sat/samples/bool_or_sample_sat.cc
#include <stdlib.h>

#include "ortools/base/init_google.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void BoolOrSampleSat() {
  CpModelBuilder cp_model;

  const BoolVar x = cp_model.NewBoolVar();
  const BoolVar y = cp_model.NewBoolVar();
  cp_model.AddBoolOr({x, ~y});
  // You can also use the ~ operator.
  cp_model.AddBoolOr({x, ~y});
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::BoolOrSampleSat();
  return EXIT_SUCCESS;
}
```

### Java code

```java
// Snippet from ortools/sat/samples/BoolOrSampleSat.java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.BoolVar;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.Literal;

/** Code sample to demonstrates a simple Boolean constraint. */
public class BoolOrSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    BoolVar x = model.newBoolVar("x");
    BoolVar y = model.newBoolVar("y");
    model.addBoolOr(new Literal[] {x, y.not()});
  }
}
```

### C\# code

```csharp
// Snippet from ortools/sat/samples/BoolOrSampleSat.cs
using System;
using Google.OrTools.Sat;

public class BoolOrSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();

        BoolVar x = model.NewBoolVar("x");
        BoolVar y = model.NewBoolVar("y");

        model.AddBoolOr(new ILiteral[] { x, y.Not() });
    }
}
```

### Go code

```go
// Snippet from ortools/sat/samples/bool_or_sample_sat.go
// The bool_or_sample_sat command is simple example of the BoolOr constraint.
package main

import (
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

func boolOrSampleSat() {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar()
	y := model.NewBoolVar()

	model.AddBoolOr(x, y.Not())
}

func main() {
	boolOrSampleSat()
}

```

## Reified constraints

The CP-SAT solver supports *half-reified* constraints, also called
*implications*, which are of the form:

```
x implies constraint
```

where the constraint must hold if `x` is true.

Note that this is not an equivalence relation. The constraint can still be true
if `x` is false.

So we can write b => And(x, not y). That is, if b is true, then x is true and y
is false. Note that in this particular example, there are multiple ways to
express this constraint: you can also write (b => x) and (b => not y), which
then is written as Or(not b, x) and Or(not b, not y).

### Python code

```python
# Snippet from ortools/sat/samples/reified_sample_sat.py
"""Simple model with a reified constraint."""

from ortools.sat.python import cp_model


def reified_sample_sat():
  """Showcase creating a reified constraint."""
  model = cp_model.CpModel()

  x = model.new_bool_var('x')
  y = model.new_bool_var('y')
  b = model.new_bool_var('b')

  # First version using a half-reified bool and.
  model.add_bool_and(x, ~y).only_enforce_if(b)

  # Second version using implications.
  model.add_implication(b, x)
  model.add_implication(b, ~y)

  # Third version using bool or.
  model.add_bool_or(~b, x)
  model.add_bool_or(~b, ~y)


reified_sample_sat()
```

### C++ code

```cpp
// Snippet from ortools/sat/samples/reified_sample_sat.cc
#include <stdlib.h>

#include "ortools/base/init_google.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void ReifiedSampleSat() {
  CpModelBuilder cp_model;

  const BoolVar x = cp_model.NewBoolVar();
  const BoolVar y = cp_model.NewBoolVar();
  const BoolVar b = cp_model.NewBoolVar();

  // First version using a half-reified bool and.
  cp_model.AddBoolAnd({x, ~y}).OnlyEnforceIf(b);

  // Second version using implications.
  cp_model.AddImplication(b, x);
  cp_model.AddImplication(b, ~y);

  // Third version using bool or.
  cp_model.AddBoolOr({~b, x});
  cp_model.AddBoolOr({~b, ~y});
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::ReifiedSampleSat();
  return EXIT_SUCCESS;
}
```

### Java code

```java
// Snippet from ortools/sat/samples/ReifiedSampleSat.java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.BoolVar;
import com.google.ortools.sat.CpModel;
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
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();

    BoolVar x = model.newBoolVar("x");
    BoolVar y = model.newBoolVar("y");
    BoolVar b = model.newBoolVar("b");

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

```csharp
// Snippet from ortools/sat/samples/ReifiedSampleSat.cs
using System;
using Google.OrTools.Sat;

public class ReifiedSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();

        BoolVar x = model.NewBoolVar("x");
        BoolVar y = model.NewBoolVar("y");
        BoolVar b = model.NewBoolVar("b");

        //  First version using a half-reified bool and.
        model.AddBoolAnd(new ILiteral[] { x, y.Not() }).OnlyEnforceIf(b);

        // Second version using implications.
        model.AddImplication(b, x);
        model.AddImplication(b, y.Not());

        // Third version using bool or.
        model.AddBoolOr(new ILiteral[] { b.Not(), x });
        model.AddBoolOr(new ILiteral[] { b.Not(), y.Not() });
    }
}
```

### Go code

```go
// Snippet from ortools/sat/samples/reified_sample_sat.go
// The reified_sample_sat command is a simple example of implication constraints.
package main

import (
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

func reifiedSampleSat() {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar()
	y := model.NewBoolVar()
	b := model.NewBoolVar()

	// First version using a half-reified bool and.
	model.AddBoolAnd(x, y.Not()).OnlyEnforceIf(b)

	// Second version using implications.
	model.AddImplication(b, x)
	model.AddImplication(b, y.Not())

	// Third version using bool or.
	model.AddBoolOr(b.Not(), x)
	model.AddBoolOr(b.Not(), y.Not())
}

func main() {
	reifiedSampleSat()
}

```

## Product of two Boolean Variables

A useful construct is the product `p` of two Boolean variables `x` and `y`.

```
p == x * y
```

This is equivalent to the logical relation

```
p <=> x and y
```

This is encoded using one bool_or constraint and two implications. The following
code samples output this truth table:

```
x = 0   y = 0   p = 0
x = 1   y = 0   p = 0
x = 0   y = 1   p = 0
x = 1   y = 1   p = 1
```

### Python code

```python
# Snippet from ortools/sat/samples/boolean_product_sample_sat.py
"""Code sample that encodes the product of two Boolean variables."""


from ortools.sat.python import cp_model


def boolean_product_sample_sat():
  """Encoding of the product of two Boolean variables.

  p == x * y, which is the same as p <=> x and y
  """
  model = cp_model.CpModel()
  x = model.new_bool_var('x')
  y = model.new_bool_var('y')
  p = model.new_bool_var('p')

  # x and y implies p, rewrite as not(x and y) or p.
  model.add_bool_or(~x, ~y, p)

  # p implies x and y, expanded into two implications.
  model.add_implication(p, x)
  model.add_implication(p, y)

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = cp_model.VarArraySolutionPrinter([x, y, p])
  solver.parameters.enumerate_all_solutions = True
  solver.solve(model, solution_printer)


boolean_product_sample_sat()
```

### Go code

```go
// Snippet from ortools/sat/samples/boolean_product_sample_sat.go
// The boolean_product_sample_sat command is a simple example of the product of two literals.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"google.golang.org/protobuf/proto"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
)

func booleanProductSample() error {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar().WithName("x")
	y := model.NewBoolVar().WithName("y")
	p := model.NewBoolVar().WithName("p")

	// x and y implies p, rewrite as not(x and y) or p.
	model.AddBoolOr(x.Not(), y.Not(), p)

	// p implies x and y, expanded into two implications.
	model.AddImplication(p, x)
	model.AddImplication(p, y)

	// Solve.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	// Set `fill_additional_solutions_in_response` and `enumerate_all_solutions` to true so
	// the solver returns all solutions found.
		params := &sppb.SatParameters{
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		EnumerateAllSolutions:             proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(4),
	}
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())

	for _, additionalSolution := range response.GetAdditionalSolutions() {
		fmt.Printf("x: %v", additionalSolution.GetValues()[x.Index()])
		fmt.Printf(" y: %v", additionalSolution.GetValues()[y.Index()])
		fmt.Printf(" p: %v\n", additionalSolution.GetValues()[p.Index()])
	}

	return nil
}

func main() {
	err := booleanProductSample()
	if err != nil {
		log.Exitf("booleanProductSample returned with error: %v", err)
	}
}

```
