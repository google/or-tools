// Copyright 2010-2017 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Costas Array Problem
//
// Finds an NxN matrix of 0s and 1s, with only one 1 per row,
// and one 1 per column, such that all displacement vectors
// between each pair of 1s are distinct.
//
// This example contains two separate implementations. CostasHard()
// uses hard constraints, whereas CostasSoft() uses a minimizer to
// minimize the number of duplicates.
#include <ctime>
#include <set>
#include <utility>
#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/base/random.h"

DEFINE_int32(minsize, 0, "Minimum degree of Costas matrix.");
DEFINE_int32(maxsize, 0, "Maximum degree of Costas matrix.");
DEFINE_int32(freevar, 5, "Number of free variables.");
DEFINE_int32(freeorderedvar, 4, "Number of variables in ordered subset.");
DEFINE_int32(sublimit, 20, "Number of attempts per subtree.");
DEFINE_int32(timelimit, 120000, "Time limit for local search.");
DEFINE_bool(soft_constraints, false, "Use soft constraints.");
DEFINE_string(export_profile, "", "filename to save the profile overview");

namespace operations_research {

// Checks that all pairwise distances are unique and returns all violators
void CheckConstraintViolators(const std::vector<int64>& vars,
                              std::vector<int>* const violators) {
  int dim = vars.size();

  // Check that all indices are unique
  for (int i = 0; i < dim; ++i) {
    for (int k = i + 1; k < dim; ++k) {
      if (vars[i] == vars[k]) {
        violators->push_back(i);
        violators->push_back(k);
      }
    }
  }

  // Check that all differences are unique for each level
  for (int level = 1; level < dim; ++level) {
    for (int i = 0; i < dim - level; ++i) {
      const int difference = vars[i + level] - vars[i];

      for (int k = i + 1; k < dim - level; ++k) {
        if (difference == vars[k + level] - vars[k]) {
          violators->push_back(k + level);
          violators->push_back(k);
          violators->push_back(i + level);
          violators->push_back(i);
        }
      }
    }
  }
}

// Check that all pairwise differences are unique
bool CheckCostas(const std::vector<int64>& vars) {
  std::vector<int> violators;

  CheckConstraintViolators(vars, &violators);

  return violators.empty();
}

// Cycles all possible permutations
class OrderedLns : public BaseLns {
 public:
  OrderedLns(const std::vector<IntVar*>& vars, int free_elements)
      : BaseLns(vars), free_elements_(free_elements) {
    index_ = 0;

    // Start of with the first free_elements_ as a permutations, eg. 0,1,2,3,...
    for (int i = 0; i < free_elements; ++i) {
      index_ +=
          i * pow(static_cast<double>(vars.size()), free_elements - i - 1);
    }
  }

  bool NextFragment() override {
    int dim = Size();
    std::set<int> fragment_set;

    do {
      int work_index = index_;

      fragment_set.clear();

      for (int i = 0; i < free_elements_; ++i) {
        int current_index = work_index % dim;
        work_index = work_index / dim;

        std::pair<std::set<int>::iterator, bool> ret =
            fragment_set.insert(current_index);

        // Check if element has been used before
        if (ret.second) {
          AppendToFragment(current_index);
        } else {
          break;
        }
      }

      // Go to next possible permutation
      ++index_;
      // Try again if a duplicate index is used
    } while (fragment_set.size() < free_elements_);

    return true;
  }

 private:
  int index_;
  const int free_elements_;
};

// RandomLns is used for the local search and frees the
// number of elements specified in 'free_elements' randomly.
class RandomLns : public BaseLns {
 public:
  RandomLns(const std::vector<IntVar*>& vars, int free_elements)
      : BaseLns(vars),
        free_elements_(free_elements),
        rand_(ACMRandom::HostnamePidTimeSeed()) {}

  bool NextFragment() override {
    std::vector<int> weighted_elements;
    std::vector<int64> values;

    // Create weighted vector for randomizer. Add one of each possible elements
    // to the weighted vector and then add more elements depending on the
    // number of conflicts
    for (int i = 0; i < Size(); ++i) {
      values.push_back(Value(i));

      // Insert each variable at least once.
      weighted_elements.push_back(i);
    }

    CheckConstraintViolators(values, &weighted_elements);
    int size = weighted_elements.size();

    // Randomly insert elements in vector until no more options remain
    while (FragmentSize() < std::min(free_elements_, size)) {
      const int index = weighted_elements[rand_.Next() % size];
      AppendToFragment(index);

      // Remove all elements with this index from weighted_elements
      for (std::vector<int>::iterator pos = weighted_elements.begin();
           pos != weighted_elements.end();) {
        if (*pos == index) {
          // Try to erase as many elements as possible at the same time
          std::vector<int>::iterator end = pos;

          while ((end + 1) != weighted_elements.end() && *(end + 1) == *pos) {
            ++end;
          }

          pos = weighted_elements.erase(pos, end);
        } else {
          ++pos;
        }
      }

      size = weighted_elements.size();
    }

    return true;
  }

 private:
  const int free_elements_;
  ACMRandom rand_;
};

class Evaluator {
 public:
  explicit Evaluator(const std::vector<IntVar*>& vars) : vars_(vars) {}

  // Prefer the value with the smallest domain
  int64 VarEvaluator(int64 index) const { return vars_[index]->Size(); }

  // Penalize for each time the value appears in a different domain,
  // as values have to be unique
  int64 ValueEvaluator(int64 id, int64 value) const {
    int appearance = 0;

    for (int i = 0; i < vars_.size(); ++i) {
      if (i != id && vars_[i]->Contains(value)) {
        ++appearance;
      }
    }

    return appearance;
  }

 private:
  std::vector<IntVar*> vars_;
};

// Computes a Costas Array using soft constraints.
// Instead of enforcing that all distance vectors are unique, we
// minimize the number of duplicate distance vectors.
void CostasSoft(const int dim) {
  Solver solver("Costas");

  // For the matrix and for the count of occurrences of each possible distance
  // for each stage
  const int num_elements = dim + (2 * dim + 1) * (dim);

  // create the variables
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(num_elements, -dim, dim, "var_", &vars);

  // the costas matrix
  std::vector<IntVar*> matrix(dim);
  // number of occurrences per stage
  std::vector<IntVar*> occurences;

  // All possible values of the distance vector
  // used to count the occurrence of all these values
  std::vector<int64> possible_values(2 * dim + 1);

  for (int64 i = -dim; i <= dim; ++i) {
    possible_values[i + dim] = i;
  }

  int index = 0;

  // Initialize the matrix that contains the coordinates of all '1' per row
  for (; index < dim; ++index) {
    matrix[index] = vars[index];
    vars[index]->SetMin(1);
  }

  // First constraint for the elements in the Costas Matrix. We want
  // them to be unique.
  std::vector<IntVar*> matrix_count;
  solver.MakeIntVarArray(2 * dim + 1, 0, dim, "matrix_count_", &matrix_count);
  solver.AddConstraint(
      solver.MakeDistribute(matrix, possible_values, matrix_count));

  // Here we only consider the elements from 1 to dim.
  for (int64 j = dim + 1; j <= 2 * dim; ++j) {
    // Penalize if an element occurs more than once.
    vars[index] =
        solver.MakeSemiContinuousExpr(solver.MakeSum(matrix_count[j], -1), 0, 1)
            ->Var();

    occurences.push_back(vars[index++]);
  }

  // Count the number of duplicates for each stage
  for (int i = 1; i < dim; ++i) {
    std::vector<IntVar*> subset(dim - i);

    // Initialize each stage
    for (int j = 0; j < dim - i; ++j) {
      IntVar* const diff = solver.MakeDifference(vars[j + i], vars[j])->Var();
      subset[j] = diff;
    }

    // Count the number of occurrences for all possible values
    std::vector<IntVar*> domain_count;
    solver.MakeIntVarArray(2 * dim + 1, 0, dim, "domain_count_", &domain_count);
    solver.AddConstraint(
        solver.MakeDistribute(subset, possible_values, domain_count));

    // Penalize occurrences of more than one
    for (int64 j = 0; j <= 2 * dim; ++j) {
      vars[index] = solver.MakeSemiContinuousExpr(
                              solver.MakeSum(domain_count[j], -1), 0, dim - i)
                        ->Var();

      occurences.push_back(vars[index++]);
    }
  }

  // We would like to minimize the penalties that we just computed
  IntVar* const objective_var = solver.MakeSum(occurences)->Var();
  OptimizeVar* const total_duplicates = solver.MakeMinimize(objective_var, 1);

  SearchMonitor* const log = solver.MakeSearchLog(1000, objective_var);

  // Out of all solutions, we just want to store the last one.
  SolutionCollector* const collector = solver.MakeLastSolutionCollector();
  collector->Add(vars);

  // The first solution that the local optimization is based on
  Evaluator evaluator(matrix);
  DecisionBuilder* const first_solution = solver.MakePhase(
      matrix,
      [&evaluator](int64 index) { return evaluator.VarEvaluator(index); },
      [&evaluator](int64 var, int64 value) {
        return evaluator.ValueEvaluator(var, value);
      });

  SearchLimit* const search_time_limit =
      solver.MakeLimit(FLAGS_timelimit, kint64max, kint64max, kint64max);

  // Locally optimize solutions for Lns
  SearchLimit* const fail_limit =
      solver.MakeLimit(kint64max, kint64max, FLAGS_sublimit, kint64max);

  DecisionBuilder* const subdecision_builder =
      solver.MakeSolveOnce(first_solution, fail_limit);

  std::vector<LocalSearchOperator*> localSearchOperators;

  // Apply RandomLns to free FLAGS_freevar variables at each stage
  localSearchOperators.push_back(
      solver.RevAlloc(new OrderedLns(matrix, FLAGS_freevar)));

  // Go through all possible permutations one by one
  localSearchOperators.push_back(
      solver.RevAlloc(new OrderedLns(matrix, FLAGS_freeorderedvar)));

  LocalSearchPhaseParameters* const ls_params =
      solver.MakeLocalSearchPhaseParameters(
          solver.ConcatenateOperators(localSearchOperators),
          subdecision_builder);

  DecisionBuilder* const second_phase =
      solver.MakeLocalSearchPhase(matrix, first_solution, ls_params);

  // Try to find a solution
  solver.Solve(second_phase, collector, log, total_duplicates,
               search_time_limit);

  if (collector->solution_count() > 0) {
    std::vector<int64> costas_matrix;
    std::string output;

    for (int n = 0; n < dim; ++n) {
      const int64 v = collector->Value(0, vars[n]);
      costas_matrix.push_back(v);
      StringAppendF(&output, "%3lld", v);
    }

    if (!CheckCostas(costas_matrix)) {
      LOG(INFO) << "No Costas Matrix found, closest solution displayed.";
    }

    LOG(INFO) << output;
  } else {
    LOG(INFO) << "No solution found";
  }
}

// Computes a Costas Array.
void CostasHard(const int dim) {
  Solver solver("costas");

  // create the variables
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(dim, -dim, dim, "var", &vars);

  // Initialize the matrix that contains the coordinates of all '1' per row
  for (int m = 0; m < dim; ++m) {
    vars[m]->SetMin(1);
  }

  solver.AddConstraint(solver.MakeAllDifferent(vars));

  // Check that the pairwise difference is unique
  for (int i = 1; i < dim; ++i) {
    std::vector<IntVar*> subset(dim - i);

    for (int j = 0; j < dim - i; ++j) {
      subset[j] = solver.MakeDifference(vars[j + i], vars[j])->Var();
    }

    solver.AddConstraint(solver.MakeAllDifferent(subset));
  }

  DecisionBuilder* const db = solver.MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);

  if (solver.NextSolution()) {
    std::vector<int64> costas_matrix;
    std::string output;

    for (int n = 0; n < dim; ++n) {
      const int64 v = vars[n]->Value();
      costas_matrix.push_back(v);
      StringAppendF(&output, "%3lld", v);
    }

    LOG(INFO) << output << " (" << solver.wall_time() << "ms)";

    CHECK(CheckCostas(costas_matrix))
        << ": Solution is not a valid Costas Matrix.";
  } else {
    LOG(INFO) << "No solution found.";
  }
  if (!FLAGS_export_profile.empty()) {
    solver.ExportProfilingOverview(FLAGS_export_profile);
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  int min = 1;
  int max = 10;

  if (FLAGS_minsize != 0) {
    min = FLAGS_minsize;

    if (FLAGS_maxsize != 0) {
      max = FLAGS_maxsize;
    } else {
      max = min;
    }
  }

  for (int i = min; i <= max; ++i) {
    LOG(INFO) << "Computing Costas Array for dim = " << i;
    if (FLAGS_soft_constraints) {
      operations_research::CostasSoft(i);
    } else {
      operations_research::CostasHard(i);
    }
  }

  return 0;
}
