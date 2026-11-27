[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Migration](migration.md) | [Python API](https://or-tools.github.io/docs/pdoc/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | ------------------------- | -----------------------------------------------------------------------------------
# Troubleshooting

## Enable logging

CP-SAT supports extensive logging of its internals.

To enable it, just set the parameter `log_search_progress` to true.

A good explanation of the log can be found in the
[cpsat-primer](https://d-krupke.github.io/cpsat-primer/understanding_the_log.html).

## Exporting the model

As seen in the [model section](model.md), the model is stored as a protocol
buffer object. This protocol buffer can be exported using the command
`model.ExportToFile(filename)` and `model.exportToFile(filename)` in java.

if filename ends with `.txt` or `.textproto`, the model will be written using
the protocol buffer text format. Otherwise, the binary format will be used.

Note that the binary format is not human readable, but takes less space.

## Improving performance with multiple workers

CP-SAT is built with parallelism in mind. While you can achieve a good solution
with a single worker, you'll get the best results when using multiple workers.

There are several tiers of behavior:

-   **[8 workers]** This is the minimum number of workers needed to trigger
    parallel search. It blends workers with different linear relaxations (none,
    default, maximal), core-based search if applicable, a quick_restart
    subsolver, a violation_ls subsolver, and dedicated Large Neighborhood Search
    subsolvers to find improved solutions.
-   **[16 workers]** Bumping to 16 workers adds a continuous probing subsolver,
    more first solution subsolvers (random search and violation_ls), two dual
    subsolvers dedicated to improving the lower bound of the objective (when
    minimizing), and more LNS subsolvers.
-   **[24 workers]** Adds more dual subsolvers, no_lp and max_lp variations of
    previous subsolvers, more violation_ls subsolvers, and more LNS subsolvers.
-   **[32 workers and more]** Adds more LNS workers, and more first solution
    subsolvers.

## Getting deterministic results

By default, CP-SAT is not deterministic when using multiple workers. Two solves
of the same model can explore the search space differently, and return
different solutions. This is a problem if you want to compare a solve against a
known result, or to replay it.

To get reproducible results, use the following parameters:

-   **[num_workers]** With `num_workers` set to 1, the search is deterministic.
-   **[interleave_search]** To use multiple workers, set `interleave_search` to
    true. The log will display `Starting deterministic search`. Be aware that
    it is much slower than the default parallel search, and that some
    subsolvers are disabled in this mode.
-   **[max_deterministic_time]** With a time limit in seconds
    (`max_time_in_seconds`), the amount of work done by the solver depends on
    the speed and the load of the machine. Use `max_deterministic_time`
    instead. Its unit is as close as possible to a second.
-   **[random_seed]** The default seed is fixed. If you change it, the solver
    may make different choices during the search.

These parameters are described in
[sat_parameters.proto](/ortools/sat/sat_parameters.proto).

Note that the model must be the same too. The code that builds a model is not
always deterministic, as it can depend on the order of a hash table. To check
that two models are the same, compare the `model_fingerprint` displayed at the
beginning of the log.

Finally, be aware that results are only reproducible with the same version of
OR-Tools on the same platform. Floating point operations and hashing do not
behave the same on different platforms. Therefore, the same model can yield
different solutions on two different operating systems, CPU architectures, or
compilers, even with the parameters above. Do not write tests that expect one
specific solution: it can change with any new version of OR-Tools.

## Solve status

Solving a problem yields the following possible status (CpSolverStatus):

-   **[UNKNOWN]** The status of the model is still unknown. A search limit has
    been reached before any of the statuses below could be determined.
-   **[MODEL_INVALID]** The given CpModelProto didn't pass the validation step.
    You can get a detailed error by calling `ValidateCpModel(model_proto)`in
    C++, or `model.validate()` in other languages.
-   **[FEASIBLE]** A feasible solution has been found. But the search was
    stopped before we could prove optimality or before we enumerated all
    solutions of a feasibility problem (if asked).
-   **[INFEASIBLE]** The problem has been proven infeasible.
-   **[OPTIMAL]** An optimal feasible solution has been found. More generally,
    this status represents a success. So we also return OPTIMAL if we find a
    solution for a pure feasibility problem or if a gap limit has been specified
    and we return a solution within this limit. In the case where we need to
    return all the feasible solution, this status will only be returned if
    solve() enumerated all of them; If we stopped earlier, we will return
    FEASIBLE.

## Debugging infeasible models

Oftentimes, solving a model yields an infeasibility status. While this can be a
valid answer, it can happen because of either a data issue, or a model issue.

Here are some ways to diagnose the source of the infeasibility.

-   reduce the model size, if possible
-   remove all constraints, which making sure that the resulting model is still
    infeasible
-   enlarge the domains of variables
-   inject a known feasible solution and try to find where it breaks
-   check the soundness of your data (e.g., do you have negative capacities, or
    unreasonable ranges?)

## Using assumptions to explain infeasibility

CP-SAT implements a mechanism of assumptions to find the root cause of an
infeasible problem. To use it, one must add enforcement literals to constraints
and add these literals to the set of assumptions.

Note that this set is minimized but not guaranteed to be minimal. To find a
minimal unsatisfiable set (MUS), you must minimize the (weighted) sum of these
assumption literals instead of using the assumptions mechanism.

Finally, be aware that solving with assumptions is not compatible with
parallelism. Therefore, the number of workers must be set to 1.

### Python code sample

```python
# Snippet from ortools/sat/samples/assumptions_sample_sat.py
from ortools.sat.python import cp_model



def main() -> None:
  """Showcases assumptions."""
  # Creates the model.
  model = cp_model.CpModel()

  # Creates the variables.
  x = model.new_int_var(0, 10, 'x')
  y = model.new_int_var(0, 10, 'y')
  z = model.new_int_var(0, 10, 'z')
  a = model.new_bool_var('a')
  b = model.new_bool_var('b')
  c = model.new_bool_var('c')

  # Creates the constraints.
  model.add(x > y).only_enforce_if(a)
  model.add(y > z).only_enforce_if(b)
  model.add(z > x).only_enforce_if(c)

  # Add assumptions
  model.add_assumptions([a, b, c])

  # Creates a solver and solves.
  solver = cp_model.CpSolver()
  status = solver.solve(model)

  # Print solution.
  print(f'Status = {solver.status_name(status)}')
  if status == cp_model.INFEASIBLE:
    print(
        'sufficient_assumptions_for_infeasibility = '
        f'{solver.sufficient_assumptions_for_infeasibility()}'
    )


if __name__ == '__main__':
  main()
```

### C++ code samples

```cpp
// Snippet from ortools/sat/samples/assumptions_sample_sat.cc
#include <stdlib.h>

#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void AssumptionsSampleSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 10);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");
  const BoolVar a = cp_model.NewBoolVar().WithName("a");
  const BoolVar b = cp_model.NewBoolVar().WithName("b");
  const BoolVar c = cp_model.NewBoolVar().WithName("c");

  cp_model.AddGreaterThan(x, y).OnlyEnforceIf(a);
  cp_model.AddGreaterThan(y, z).OnlyEnforceIf(b);
  cp_model.AddGreaterThan(z, x).OnlyEnforceIf(c);

  // Add assumptions
  cp_model.AddAssumptions({a, b, c});

  // Solving part.
  const CpSolverResponse response = Solve(cp_model.Build());

  // Print solution.
  LOG(INFO) << CpSolverResponseStats(response);
  if (response.status() == CpSolverStatus::INFEASIBLE) {
    for (const int index :
         response.sufficient_assumptions_for_infeasibility()) {
      LOG(INFO) << index;
    }
  }
}
}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::AssumptionsSampleSat();
  return EXIT_SUCCESS;
}
```

### Java code samples

```java
// Snippet from ortools/sat/samples/java/AssumptionsSampleSat.java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.Literal;


/** Minimal CP-SAT example to showcase assumptions. */
public class AssumptionsSampleSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Create the model.
    CpModel model = new CpModel();

    // Create the variables.
    IntVar x = model.newIntVar(0, 10, "x");
    IntVar y = model.newIntVar(0, 10, "y");
    IntVar z = model.newIntVar(0, 10, "z");
    Literal a = model.newBoolVar("a");
    Literal b = model.newBoolVar("b");
    Literal c = model.newBoolVar("c");

    // Creates the constraints.
    model.addGreaterThan(x, y).onlyEnforceIf(a);
    model.addGreaterThan(y, z).onlyEnforceIf(b);
    model.addGreaterThan(z, x).onlyEnforceIf(c);

    // Add assumptions
    model.addAssumptions(new Literal[] {a, b, c});

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    // Print solution.
    // Check that the problem is infeasible.
    if (status == CpSolverStatus.INFEASIBLE) {
      System.out.println(solver.sufficientAssumptionsForInfeasibility());
    }
  }

  private AssumptionsSampleSat() {}
}
```

### C\# code samples

```csharp
// Snippet from ortools/sat/samples/AssumptionsSampleSat.cs
using System;
using Google.OrTools.Sat;

public class AssumptionsSampleSat {
  static void Main() {
    // Creates the model.
    CpModel model = new CpModel();

    // Creates the variables.
    IntVar x = model.NewIntVar(0, 10, "x");
    IntVar y = model.NewIntVar(0, 10, "y");
    IntVar z = model.NewIntVar(0, 10, "z");
    ILiteral a = model.NewBoolVar("a");
    ILiteral b = model.NewBoolVar("b");
    ILiteral c = model.NewBoolVar("c");

    // Creates the constraints.
    model.Add(x > y).OnlyEnforceIf(a);
    model.Add(y > z).OnlyEnforceIf(b);
    model.Add(z > x).OnlyEnforceIf(c);

    // Add assumptions
    model.AddAssumptions(new ILiteral[] { a, b, c });

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);
    Console.WriteLine(solver.SufficientAssumptionsForInfeasibility());
  }
}
```
