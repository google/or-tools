# Using the CP solver

https://developers.google.com/optimization/cp/

## Documentation structure

OR-Tools comes with lots of constraint programming samples given in C++, Python,
Java and .Net. Each language have different requirements for the code samples.

## Basic example

### C++ code samples

```cpp
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

void SimpleCpProgram() {
  // Instantiate the solver.
  Solver solver("CpSimple");

  // Create the variables.
  const int64_t num_vals = 3;
  IntVar* const x = solver.MakeIntVar(0, num_vals - 1, "x");
  IntVar* const y = solver.MakeIntVar(0, num_vals - 1, "y");
  IntVar* const z = solver.MakeIntVar(0, num_vals - 1, "z");

  // Constraint 0: x != y..
  solver.AddConstraint(solver.MakeAllDifferent({x, y}));
  LOG(INFO) << "Number of constraints: "
            << std::to_string(solver.constraints());

  // Solve the problem.
  DecisionBuilder* const db = solver.MakePhase(
      {x, y, z}, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);

  // Print solution on console.
  int count = 0;
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    ++count;
    LOG(INFO) << "Solution " << count << ":" << std::endl
              << " x=" << x->Value() << " y=" << y->Value()
              << " z=" << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << "Number of solutions found: " << solver.solutions();

  LOG(INFO) << "Advanced usage:" << std::endl
            << "Problem solved in " << std::to_string(solver.wall_time())
            << "ms" << std::endl
            << "Memory usage: " << std::to_string(Solver::MemoryUsage())
            << "bytes";
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::SimpleCpProgram();
  return EXIT_SUCCESS;
}
```

### Python code samples

```python
"""Simple Constraint optimization example."""

from ortools.constraint_solver import pywrapcp


def main():
  """Entry point of the program."""
  # Instantiate the solver.
  solver = pywrapcp.Solver('CPSimple')

  # Create the variables.
  num_vals = 3
  x = solver.IntVar(0, num_vals - 1, 'x')
  y = solver.IntVar(0, num_vals - 1, 'y')
  z = solver.IntVar(0, num_vals - 1, 'z')

  # Constraint 0: x != y.
  solver.Add(x != y)
  print('Number of constraints: ', solver.Constraints())

  # Solve the problem.
  decision_builder = solver.Phase([x, y, z], solver.CHOOSE_FIRST_UNBOUND,
                                  solver.ASSIGN_MIN_VALUE)

  # Print solution on console.
  count = 0
  solver.NewSearch(decision_builder)
  while solver.NextSolution():
    count += 1
    solution = 'Solution {}:\n'.format(count)
    for var in [x, y, z]:
      solution += ' {} = {}'.format(var.Name(), var.Value())
    print(solution)
  solver.EndSearch()
  print('Number of solutions found: ', count)

  print('Advanced usage:')
  print('Problem solved in ', solver.WallTime(), 'ms')
  print('Memory usage: ', pywrapcp.Solver.MemoryUsage(), 'bytes')


if __name__ == '__main__':
  main()
```

### Java code samples

```java
package com.google.ortools.constraintsolver.samples;
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.Solver;
import java.util.logging.Logger;

/** Simple CP Program.*/
public class SimpleCpProgram {
  private SimpleCpProgram() {}

  private static final Logger logger = Logger.getLogger(SimpleCpProgram.class.getName());

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Instantiate the solver.
    Solver solver = new Solver("CpSimple");

    // Create the variables.
    final long numVals = 3;
    final IntVar x = solver.makeIntVar(0, numVals - 1, "x");
    final IntVar y = solver.makeIntVar(0, numVals - 1, "y");
    final IntVar z = solver.makeIntVar(0, numVals - 1, "z");

    // Constraint 0: x != y..
    solver.addConstraint(solver.makeAllDifferent(new IntVar[]{x, y}));
    logger.info("Number of constraints: " + solver.constraints());

    // Solve the problem.
    final DecisionBuilder db = solver.makePhase(
        new IntVar[]{x, y, z},
        Solver.CHOOSE_FIRST_UNBOUND,
        Solver.ASSIGN_MIN_VALUE);

    // Print solution on console.
    int count = 0;
    solver.newSearch(db);
    while (solver.nextSolution()) {
      ++count;
      logger.info(String.format("Solution: %d\n x=%d y=%d z=%d"
          , count
          , x.value()
          , y.value()
          , z.value()));
    }
    solver.endSearch();
    logger.info("Number of solutions found: " + solver.solutions());

    logger.info(String.format(
          "Advanced usage:\nProblem solved in %d ms\nMemory usage: %d bytes"
          , solver.wallTime(), Solver.memoryUsage()));
  }
}
```

### .Net code samples


```cs
using System;
using Google.OrTools.ConstraintSolver;

/// <summary>
///   This is a simple CP program.
/// </summary>
public class SimpleCpProgram
{
    public static void Main(String[] args)
    {
        // Instantiate the solver.
        Solver solver = new Solver("CpSimple");

        // Create the variables.
        const long numVals = 3;
        IntVar x = solver.MakeIntVar(0, numVals - 1, "x");
        IntVar y = solver.MakeIntVar(0, numVals - 1, "y");
        IntVar z = solver.MakeIntVar(0, numVals - 1, "z");

        // Constraint 0: x != y..
        solver.Add(solver.MakeAllDifferent(new IntVar[] { x, y }));
        Console.WriteLine($"Number of constraints: {solver.Constraints()}");

        // Solve the problem.
        DecisionBuilder db =
            solver.MakePhase(new IntVar[] { x, y, z }, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        // Print solution on console.
        int count = 0;
        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            ++count;
            Console.WriteLine($"Solution: {count}\n x={x.Value()} y={y.Value()} z={z.Value()}");
        }
        solver.EndSearch();
        Console.WriteLine($"Number of solutions found: {solver.Solutions()}");

        Console.WriteLine("Advanced usage:");
        Console.WriteLine($"Problem solved in {solver.WallTime()}ms");
        Console.WriteLine($"Memory usage: {Solver.MemoryUsage()}bytes");
    }
}
```
