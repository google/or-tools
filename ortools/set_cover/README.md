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
for (const auto [subset, subset_cost] : subsets) {
  model.AddEmptySubset(subset_cost)
  for (const int element : subset) {
    model.AddElementToLastSubset(element);
  }
}
SetCoverLedger ledger(&model);
```

Solve it using a MIP solver (guarantees to yield the optimum solution if it has
enough time to run):

```cpp
SetCoverMip mip(&ledger);
mip.SetTimeLimitInSeconds(10);
mip.NextSolution();
SubsetBoolVector best_choices = ledger.GetSolution();
LOG(INFO) << "Cost: " << ledger.cost();
```

A custom combination of heuristics (10,000 iterations of: clearing 10% of the
variables, running a Chvatal greedy descent, using steepest local search):

```cpp
Cost best_cost = std::numeric_limits<Cost>::max();
SubsetBoolVector best_choices = ledger.GetSolution();
for (int i = 0; i < 10000; ++i) {
  ledger.LoadSolution(best_choices);
  ClearRandomSubsets(0.1 * model.num_subsets().value(), &ledger);

  GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(10000));

  EXPECT_TRUE(ledger.CheckSolution());
  if (ledger.cost() < best_cost) {
    best_cost = ledger.cost();
    best_choices = ledger.GetSolution();
    LOG(INFO) << "Better cost: " << best_cost << " at iteration = " << i;
  }
}
ledger.LoadSolution(best_choices);
LOG(INFO) << "Best cost: " << ledger.cost();
```
