// Copyright 2010-2024 Google LLC
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

#include <limits>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/algorithms/set_cover_heuristics.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_mip.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"

namespace operations_research {
namespace {

using CL = SetCoverInvariant::ConsistencyLevel;

SetCoverModel CreateKnightsCoverModel(int num_rows, int num_cols) {
  SetCoverModel model;
  constexpr int knight_row_move[] = {2, 1, -1, -2, -2, -1, 1, 2};
  constexpr int knight_col_move[] = {1, 2, 2, 1, -1, -2, -2, -1};
  for (int row = 0; row < num_rows; ++row) {
    for (int col = 0; col < num_cols; ++col) {
      model.AddEmptySubset(1);
      model.AddElementToLastSubset(row * num_cols + col);
      for (int i = 0; i < 8; ++i) {
        const int new_row = row + knight_row_move[i];
        const int new_col = col + knight_col_move[i];
        if (new_row >= 0 && new_row < num_rows && new_col >= 0 &&
            new_col < num_cols) {
          model.AddElementToLastSubset(new_row * num_cols + new_col);
        }
      }
    }
  }
  return model;
}

void DisplayKnightsCoverSolution(const SubsetBoolVector& choices, int num_rows,
                                 int num_cols) {
  std::string line;
  std::string separator = "+";
  for (int col = 0; col < num_cols; ++col) {
    absl::StrAppend(&separator, "-+");
  }
  LOG(INFO) << separator;
  for (int row = 0; row < num_rows; ++row) {
    line = "|";
    for (int col = 0; col < num_cols; ++col) {
      const SubsetIndex subset(row * num_cols + col);
      absl::StrAppend(&line, choices[subset] ? "X|" : " |");
    }
    LOG(INFO) << line;
    LOG(INFO) << separator;
  }
}

TEST(SetCoverProtoTest, SaveReload) {
  SetCoverModel model = CreateKnightsCoverModel(10, 10);
  SetCoverProto proto = model.ExportModelAsProto();
  SetCoverModel reloaded;
  reloaded.ImportModelFromProto(proto);
  EXPECT_EQ(model.num_subsets(), reloaded.num_subsets());
  EXPECT_EQ(model.num_elements(), reloaded.num_elements());
  EXPECT_EQ(model.subset_costs(), reloaded.subset_costs());
  EXPECT_EQ(model.columns(), reloaded.columns());
}

TEST(SolutionProtoTest, SaveReloadTwice) {
  SetCoverModel model = CreateKnightsCoverModel(3, 3);
  SetCoverInvariant inv(&model);
  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
  SetCoverSolutionResponse greedy_proto = inv.ExportSolutionAsProto();
  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(500));
  EXPECT_TRUE(inv.CheckConsistency(CL::kRedundancy));
  SetCoverSolutionResponse steepest_proto = inv.ExportSolutionAsProto();
  inv.ImportSolutionFromProto(greedy_proto);
  CHECK(steepest.NextSolution(500));
  EXPECT_TRUE(inv.CheckConsistency(CL::kRedundancy));
}

TEST(SetCoverTest, InitialValues) {
  SetCoverModel model;
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(1);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(2);
  EXPECT_TRUE(model.ComputeFeasibility());

  SetCoverInvariant inv(&model);
  TrivialSolutionGenerator trivial(&inv);
  CHECK(trivial.NextSolution());
  LOG(INFO) << "TrivialSolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));

  GreedySolutionGenerator greedy(&inv);
  EXPECT_TRUE(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));

  EXPECT_EQ(inv.num_uncovered_elements(), 0);
  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(500));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
}

TEST(SetCoverTest, Preprocessor) {
  SetCoverModel model;
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(1);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(2);
  EXPECT_TRUE(model.ComputeFeasibility());

  SetCoverInvariant inv(&model);
  Preprocessor preprocessor(&inv);
  preprocessor.NextSolution();
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));
  GreedySolutionGenerator greedy(&inv);
  EXPECT_TRUE(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
}

TEST(SetCoverTest, Infeasible) {
  SetCoverModel model;
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(3);
  EXPECT_FALSE(model.ComputeFeasibility());
}

#ifdef NDEBUG
static constexpr int SIZE = 128;
#else
static constexpr int SIZE = 16;
#endif

TEST(SetCoverTest, KnightsCoverCreation) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  EXPECT_TRUE(model.ComputeFeasibility());
}

TEST(SetCoverTest, KnightsCoverTrivalAndGreedy) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  EXPECT_TRUE(model.ComputeFeasibility());
  SetCoverInvariant inv(&model);

  TrivialSolutionGenerator trivial(&inv);
  CHECK(trivial.NextSolution());
  LOG(INFO) << "TrivialSolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));

  // Reinitialize before using Greedy, to start from scratch.
  inv.Initialize();
  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100'000));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
}

TEST(SetCoverTest, KnightsCoverGreedy) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  SetCoverInvariant inv(&model);

  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverDegree) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  SetCoverInvariant inv(&model);

  ElementDegreeSolutionGenerator degree(&inv);
  CHECK(degree.NextSolution());
  LOG(INFO) << "ElementDegreeSolutionGenerator cost: " << inv.cost();

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverGLS) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  SetCoverInvariant inv(&model);
  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();
  GuidedLocalSearch gls(&inv);
  CHECK(gls.NextSolution(100));
  LOG(INFO) << "GuidedLocalSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverRandom) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  EXPECT_TRUE(model.ComputeFeasibility());
  SetCoverInvariant inv(&model);

  RandomSolutionGenerator random(&inv);
  CHECK(random.NextSolution());
  LOG(INFO) << "RandomSolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
}

TEST(SetCoverTest, KnightsCoverTrivial) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  EXPECT_TRUE(model.ComputeFeasibility());
  SetCoverInvariant inv(&model);

  TrivialSolutionGenerator trivial(&inv);
  CHECK(trivial.NextSolution());
  LOG(INFO) << "TrivialSolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
}

TEST(SetCoverTest, KnightsCoverGreedyAndTabu) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverInvariant inv(&model);

  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));

  GuidedTabuSearch gts(&inv);
  CHECK(gts.NextSolution(1'000));
  LOG(INFO) << "GuidedTabuSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
  DisplayKnightsCoverSolution(inv.is_selected(), BoardSize, BoardSize);
}

TEST(SetCoverTest, KnightsCoverGreedyRandomClear) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverInvariant inv(&model);
  Cost best_cost = std::numeric_limits<Cost>::max();
  SubsetBoolVector best_choices = inv.is_selected();
  for (int i = 0; i < 100; ++i) {
    inv.LoadSolution(best_choices);
    ClearRandomSubsets(0.1 * inv.trace().size(), &inv);

    GreedySolutionGenerator greedy(&inv);
    CHECK(greedy.NextSolution());

    SteepestSearch steepest(&inv);
    CHECK(steepest.NextSolution(10'000));

    if (inv.cost() < best_cost) {
      best_cost = inv.cost();
      best_choices = inv.is_selected();
      LOG(INFO) << "Best cost: " << best_cost << " at iteration = " << i;
    }
  }
  inv.LoadSolution(best_choices);
  DisplayKnightsCoverSolution(best_choices, BoardSize, BoardSize);
  LOG(INFO) << "RandomClear cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    CHECK_GE(inv.cost(), 350);
  }
}

TEST(SetCoverTest, KnightsCoverElementDegreeRandomClear) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverInvariant inv(&model);
  Cost best_cost = std::numeric_limits<Cost>::max();
  SubsetBoolVector best_choices;
  for (int i = 0; i < 1000; ++i) {
    LazyElementDegreeSolutionGenerator degree(&inv);
    CHECK(degree.NextSolution());
    CHECK(inv.CheckConsistency(CL::kCostAndCoverage));

    SteepestSearch steepest(&inv);
    CHECK(steepest.NextSolution(100));

    if (inv.cost() < best_cost) {
      best_cost = inv.cost();
      best_choices = inv.is_selected();
      LOG(INFO) << "Best cost: " << best_cost << " at iteration = " << i;
    }
    inv.LoadSolution(best_choices);
    ClearRandomSubsets(0.1 * inv.trace().size(), &inv);
  }
  inv.LoadSolution(best_choices);
  DisplayKnightsCoverSolution(best_choices, BoardSize, BoardSize);
  LOG(INFO) << "RandomClear cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    CHECK_GE(inv.cost(), 350);
  }
}

TEST(SetCoverTest, KnightsCoverRandomClearMip) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverInvariant inv(&model);
  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();

  Cost best_cost = inv.cost();
  SubsetBoolVector best_choices = inv.is_selected();
  for (int i = 0; i < 1'000; ++i) {
    auto focus = ClearRandomSubsets(0.1 * inv.trace().size(), &inv);
    SetCoverMip mip(&inv);
    mip.NextSolution(focus, true, 1);
    EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));
    if (inv.cost() < best_cost) {
      best_cost = inv.cost();
      best_choices = inv.is_selected();
      LOG(INFO) << "Best cost: " << best_cost << " at iteration = " << i;
    }
    inv.LoadSolution(best_choices);
  }
  DisplayKnightsCoverSolution(best_choices, BoardSize, BoardSize);
  LOG(INFO) << "RandomClearMip cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    CHECK_GE(inv.cost(), 350);
  }
}

TEST(SetCoverTest, KnightsCoverMip) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverInvariant inv(&model);
  SetCoverMip mip(&inv);
  mip.NextSolution(true, .5);
  LOG(INFO) << "Mip cost: " << inv.cost();
  DisplayKnightsCoverSolution(inv.is_selected(), BoardSize, BoardSize);
  if (BoardSize == 50) {
    CHECK_GE(inv.cost(), 350);
  }
}

void BM_Steepest(benchmark::State& state) {
  for (auto s : state) {
    SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
    SetCoverInvariant inv(&model);
    GreedySolutionGenerator greedy(&inv);
    SteepestSearch steepest(&inv);
  }
}

BENCHMARK(BM_Steepest)->Arg(1 << 5);

}  // namespace
}  // namespace operations_research
