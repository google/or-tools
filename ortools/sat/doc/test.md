# Channeling constraints

A *channeling constraint* links variables inside a model. They're used when you
want to express a complicated relationship between variables, such as "if this
variable satisfies a condition, force another variable to a particular value".

Channeling is usually implemented using *half-reified* linear constraints: one
constraint implies another (a &rarr; b), but not necessarily the other way
around (a &larr; b).

## If-Then-Else expressions

Let's say you want to implement the following: "If *x* is less than 5, set *y*
to 0. Otherwise, set *y* to 10-*x*". You can do this creating an intermediate
boolean variable *b* and two half-reified constraints, shown below:

<div class="ds-selector-tabs">
    <section>
    <h3>Python</h3>
```python
"""Link integer constraints together."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0

    def on_solution_callback(self):
        self.__solution_count += 1
        for v in self.__variables:
            print('%s=%i' % (v, self.Value(v)), end=' ')
        print()

    def solution_count(self):
        return self.__solution_count


def ChannelingSampleSat():
    """Demonstrates how to link integer constraints together."""

    # Model.
    model = cp_model.CpModel()

    # Variables.
    x = model.NewIntVar(0, 10, 'x')
    y = model.NewIntVar(0, 10, 'y')

    b = model.NewBoolVar('b')

    # Implement b == (x >= 5).
    model.Add(x >= 5).OnlyEnforceIf(b)
    model.Add(x < 5).OnlyEnforceIf(b.Not())

    # b implies (y == 10 - x).
    model.Add(y == 10 - x).OnlyEnforceIf(b)
    # not(b) implies y == 0.
    model.Add(y == 0).OnlyEnforceIf(b.Not())

    # Search for x values in increasing order.
    model.AddDecisionStrategy([x], cp_model.CHOOSE_FIRST,
                              cp_model.SELECT_MIN_VALUE)

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH

    # Searches and prints out all solutions.
    solution_printer = VarArraySolutionPrinter([x, y, b])
    solver.SearchForAllSolutions(model, solution_printer)


ChannelingSampleSat()
```
    </section>
    <section>
    <h3>C++</h3>
    ```[cpp]
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void ChannelingSampleSat() {
  // Model.
  CpModelBuilder cp_model;

  // Main variables.
  const IntVar x = cp_model.NewIntVar({0, 10});
  const IntVar y = cp_model.NewIntVar({0, 10});
  const BoolVar b = cp_model.NewBoolVar();

  // b == (x >= 5).
  cp_model.AddGreaterOrEqual(x, 5).OnlyEnforceIf(b);
  cp_model.AddLessThan(x, 5).OnlyEnforceIf(Not(b));

  // b implies (y == 10 - x).
  cp_model.AddEquality(LinearExpr::Sum({x, y}), 10).OnlyEnforceIf(b);
  // not(b) implies y == 0.
  cp_model.AddEquality(y, 0).OnlyEnforceIf(Not(b));

  // Search for x values in increasing order.
  cp_model.AddDecisionStrategy({x}, DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MIN_VALUE);

  Model model;
  SatParameters parameters;
  parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "x=" << SolutionIntegerValue(r, x)
              << " y=" << SolutionIntegerValue(r, y)
              << " b=" << SolutionBooleanValue(r, b);
  }));
  SolveWithModel(cp_model, &model);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ChannelingSampleSat();

  return EXIT_SUCCESS;
}
```
    </section>

This displays the following:

```
x=0 y=0 b=0
x=1 y=0 b=0
x=2 y=0 b=0
x=3 y=0 b=0
x=4 y=0 b=0
x=5 y=5 b=1
x=6 y=4 b=1
x=7 y=3 b=1
x=8 y=2 b=1
x=9 y=1 b=1
x=10 y=0 b=1
```
