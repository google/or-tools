# Set covering

An instance of set covering is composed of two entities: elements and sets; sets
cover a series of elements. The problem of set covering is about finding the
cheapest combination of sets that cover all the elements.

[More information on Wikipedia](https://en.wikipedia.org/wiki/Set_cover_problem).

*   Solver: [`set_cover_heuristics.h`](set_cover_heuristics.h).
*   Instance representation: [`set_cover_model.h`](set_cover_model.h).
*   Instance parser: [`set_cover_reader.h`](set_cover_reader.h).

Create an instance:

```cpp
// If the elements are integers, a subset can be a std::vector<int> (in a pair
// along its cost).
std::vector<std::pair<std::vector<int>, int>> subsets = ...;

SetCoverModel model;
for (const auto& [subset, subset_cost] : subsets) {
  model.AddEmptySubset(subset_cost);
  for (const int element : subset) {
    model.AddElementToLastSubset(element);
  }
}
SetCoverInvariant inv(&model);
```

## Fast Large-Scale Approximation with Lazy Element Degree

It is highly recommended to use the **lazy element degree solution generator**
(`LazyElementDegreeSolutionGenerator`) in tandem with **lazy steepest search**
(`LazySteepestSearch`), instead of `GreedySolutionOptimizer` and
`SteepestSearch`.

To our knowledge and in our experience, it is the most efficient approximate
solver for set covering, making it essential for very large-scale problems.

### Why it is better:

*   **Lower Complexity:** Its complexity is bounded by $O(\text{\#nnz})$
    (number of non-zeros) and is roughly proportional to the size of the
    output.
*   **High Quality:** The quality of the solution, while not coming with the
    same guarantees as Chvatal's, is very similar.
*   **Extremely Fast:** Experimentally, it is *much* faster than all other
    solvers/generators. It can find a solution to a problem with 1 billion
    subsets and 400,000 elements in less than 10 seconds on a single x86-64
    core. The run time is largely dominated by the generation or the reading
    of the data.
*   **Minimizes Computation:** Using it with its twin `LazySteepestSearch`
    minimizes priority re-evaluations and overall computation.

### Example usage:

```cpp
LazyElementDegreeSolutionGenerator lazy_element_degree(&inv);
CHECK(lazy_element_degree.Optimize());

LazySteepestSearch lazy_steepest(&inv);
CHECK(lazy_steepest.Optimize());
```

### Further Refinement with Guided Local Search (GLS)

Once a quick initial solution is obtained via
`LazyElementDegreeSolutionGenerator` and `LazySteepestSearch`, you can
further refine the solution using **Guided Local Search**
(`GuidedLocalSearch`). GLS acts as a metaheuristic that penalizes
frequently used features to diversify the search and escape local minima.

```cpp
GuidedLocalSearch gls(&inv);
gls.SetMaxIterations(1000);
CHECK(gls.Optimize());
```
## Lower Bounds / Evaluating Solution Quality

For any minimization problem like set covering, it is valuable to compute a
mathematical **lower bound** of the optimal cost. This allows you to evaluate
how close a heuristic solution is to the global optimum (i.e., the optimality
gap).

The solver provides two fast lower-bound algorithms based on **dual ascent**:

*   **`ComputeDualAscentLB(inv, num_random_passes)`**: Greedily builds a lower
    bound using pseudorandom permutations of the elements, returning the largest
    lower bound found.
*   **`ComputeDualAscentLBFullRandom(inv, num_random_passes)`**: Greedily builds
    a lower bound using random permutations of the elements, returning the
    largest lower bound found.

### Example usage:

```cpp
// Solve using a heuristic.
LazyElementDegreeSolutionGenerator lazy_element_degree(&inv);
CHECK(lazy_element_degree.Optimize());

LazySteepestSearch lazy_steepest(&inv);
CHECK(lazy_steepest.Optimize());

const Cost solution_cost = inv.cost();

// Compute a lower bound to check quality.
// Shuffling elements with 10 random passes:
const Cost lower_bound = ComputeDualAscentLB(inv, 10);

LOG(INFO) << "Heuristic cost: " << solution_cost;
LOG(INFO) << "Lower bound: " << lower_bound;
LOG(INFO) << "Optimality gap is at most: "
          << (solution_cost - lower_bound) / lower_bound * 100 << "%";
```

A custom combination of heuristics (10,000 iterations of: clearing 10% of the
variables, running a Chvatal greedy descent, using steepest local search):

```cpp
Cost best_cost = std::numeric_limits<Cost>::max();
SubsetBoolVector best_choices = inv.is_selected();
for (int i = 0; i < 10000; ++i) {
  inv.LoadSolution(best_choices);
  ClearRandomSubsets(0.1 * model.num_subsets().value(), &inv);

  GreedySolutionOptimizer greedy(&inv);
  CHECK(greedy.Optimize());

  SteepestSearch steepest(&inv);
  CHECK(steepest.Optimize());

  EXPECT_EQ(inv.num_uncovered_elements(), 0);
  if (inv.cost() < best_cost) {
    best_cost = inv.cost();
    best_choices = inv.is_selected();
    LOG(INFO) << "Better cost: " << best_cost << " at iteration = " << i;
  }
}
inv.LoadSolution(best_choices);
LOG(INFO) << "Best cost: " << inv.cost();
```

## Multi-Format Input/Output

The library provides dedicated APIs in `set_cover_reader.h` to read and write
set covering models and solutions in multiple popular academic and
industrial formats:

*   **`ORLIB`**: Beasley's OR-Library format (`scp` benchmarks).
*   **`RAIL`**: Beasley's Rail rotation benchmarks.
*   **`FIMI`**: Frequent Itemset Mining Implementations (`.dat` format, where
    all costs default to 1).
*   **`PROTO` / `PROTO_BIN`**: Text and binary serialization formats using
    Protocol Buffers (`SetCoverProto`).

### Example usage:

```cpp
// Read a model from a Beasley SCP OR-Library file.
SetCoverModel model = ReadModel("scp41.txt", SetCoverFormat::ORLIB);

// Create the sparse row view required for the invariant and solvers.
model.CreateSparseRowView();

// Solve and save a solution in text format.
SubsetBoolVector solution = ...;
WriteSolution(model, solution, "solution.txt", SetCoverFormat::TXT);
```

## Generating Large Random Models

For testing and solver benchmarking, `SetCoverModel` includes a powerful
random model generator:

*   **`SetCoverModel::GenerateRandomModelFrom(...)`**: Generates a new random
    `SetCoverModel` using a small `seed_model` as a prototype. It scales the
    element degrees (rows), subset sizes (columns), and costs affinely based on
    user-defined scaling factors (`row_scale`, `column_scale`, `cost_scale`),
    making it easy to synthesize highly realistic, large-scale instances from
    simple real-world benchmarks.

### Example usage:

```cpp
// Populate or read a small seed model.
SetCoverModel seed_model = ...;
seed_model.CreateSparseRowView();

// Generate a large random model with 10,000 elements and 100,000 subsets.
// Scales element degrees by 1.0, subset cardinalities by 1.5, and costs
// by 2.0:
SetCoverModel large_model = SetCoverModel::GenerateRandomModelFrom(
    seed_model,
    /*num_elements=*/10000,
    /*num_subsets=*/100000,
    /*row_scale=*/1.0,
    /*column_scale=*/1.5,
    /*cost_scale=*/2.0);

if (!large_model.IsEmpty()) {
  // Construct sparse row view required to solve the generated model.
  large_model.CreateSparseRowView();
}
```

## Solving to Optimality with MIP

For problems that are **very small** compared to what
`LazyElementDegreeSolutionGenerator` can handle, you can solve the instance
exactly to global optimality using a Mixed-Integer Programming (MIP) solver.

> [!WARNING]
> Due to NP-hardness and exponential complexity, MIP solvers do not scale to
> large problems and should only be used on small instances.

### Example usage:

```cpp
// Solve it using a MIP solver (guarantees to yield the optimum solution
// if it has enough time to run).
SetCoverMip mip(&inv);
mip.SetTimeLimit(absl::Seconds(10));
mip.Optimize();

SubsetBoolVector best_choices = inv.is_selected();
LOG(INFO) << "Optimal Cost: " << inv.cost();
```
