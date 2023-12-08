// Copyright 2010-2022 Google LLC
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

#include "ortools/algorithms/set_cover.h"

#include <limits>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/algorithms/set_cover_ledger.h"
#include "ortools/algorithms/set_cover_mip.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"

namespace operations_research {
namespace {

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
  SetCoverModel model = CreateKnightsCoverModel(10, 10);
  SetCoverLedger ledger(&model);
  GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());
  EXPECT_TRUE(ledger.CheckSolution());
  SetCoverSolutionResponse greedy_proto = ledger.ExportSolutionAsProto();
  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(500));
  EXPECT_TRUE(ledger.CheckSolution());
  SetCoverSolutionResponse steepest_proto = ledger.ExportSolutionAsProto();
  ledger.ImportSolutionFromProto(greedy_proto);
  CHECK(steepest.NextSolution(500));
  EXPECT_TRUE(ledger.CheckSolution());
  SetCoverSolutionResponse reloaded_proto = ledger.ExportSolutionAsProto();
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

  SetCoverLedger ledger(&model);
  TrivialSolutionGenerator trivial(&ledger);
  CHECK(trivial.NextSolution());
  LOG(INFO) << "TrivialSolutionGenerator cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());

  GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(500));
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());
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
  SetCoverLedger ledger(&model);

  TrivialSolutionGenerator trivial(&ledger);
  CHECK(trivial.NextSolution());
  LOG(INFO) << "TrivialSolutionGenerator cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());

  // Reinitialize before using Greedy, to start from scratch.
  ledger.Initialize();
  GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(100000));
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());
}

TEST(SetCoverTest, KnightsCoverGreedy) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  SetCoverLedger ledger(&model);

  GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << ledger.cost();

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(100000));
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();
}

TEST(SetCoverTest, KnightsCoverRandom) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  EXPECT_TRUE(model.ComputeFeasibility());
  SetCoverLedger ledger(&model);

  RandomSolutionGenerator random(&ledger);
  CHECK(random.NextSolution());
  LOG(INFO) << "RandomSolutionGenerator cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(100000));
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());
}

TEST(SetCoverTest, KnightsCoverTrivial) {
  SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
  EXPECT_TRUE(model.ComputeFeasibility());
  SetCoverLedger ledger(&model);

  TrivialSolutionGenerator trivial(&ledger);
  CHECK(trivial.NextSolution());
  LOG(INFO) << "TrivialSolutionGenerator cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(100000));
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());
}

TEST(SetCoverTest, KnightsCoverGreedyAndTabu) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverLedger ledger(&model);

  GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << ledger.cost();

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(10000));
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());

  GuidedTabuSearch gts(&ledger);
  CHECK(gts.NextSolution(10000));
  LOG(INFO) << "GuidedTabuSearch cost: " << ledger.cost();
  EXPECT_TRUE(ledger.CheckSolution());
  DisplayKnightsCoverSolution(ledger.is_selected(), BoardSize, BoardSize);
}

TEST(SetCoverTest, KnightsCoverRandomClear) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverLedger ledger(&model);
  Cost best_cost = std::numeric_limits<Cost>::max();
  SubsetBoolVector best_choices = ledger.is_selected();
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
      best_choices = ledger.is_selected();
      LOG(INFO) << "Best cost: " << best_cost << " at iteration = " << i;
    }
  }
  ledger.LoadSolution(best_choices);
  DisplayKnightsCoverSolution(best_choices, BoardSize, BoardSize);
  LOG(INFO) << "RandomClear cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    CHECK_GE(ledger.cost(), 350);
  }
}

TEST(SetCoverTest, KnightsCoverRandomClearMip) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverLedger ledger(&model);
  Cost best_cost = std::numeric_limits<Cost>::max();
  SubsetBoolVector best_choices = ledger.is_selected();
  GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << ledger.cost();

  SteepestSearch steepest(&ledger);
  CHECK(steepest.NextSolution(10000));
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();

  best_cost = ledger.cost();
  best_choices = ledger.is_selected();
  for (int i = 0; i < 100; ++i) {
    ledger.LoadSolution(best_choices);
    auto focus = ClearRandomSubsets(0.1 * model.num_subsets().value(), &ledger);
    SetCoverMip mip(&ledger);
    mip.SetTimeLimitInSeconds(1);
    mip.NextSolution(focus);
    EXPECT_TRUE(ledger.CheckSolution());
    if (ledger.cost() < best_cost) {
      best_cost = ledger.cost();
      best_choices = ledger.is_selected();
      LOG(INFO) << "Best cost: " << best_cost << " at iteration = " << i;
    }
  }
  ledger.LoadSolution(best_choices);
  DisplayKnightsCoverSolution(best_choices, BoardSize, BoardSize);
  LOG(INFO) << "RandomClearMip cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    CHECK_GE(ledger.cost(), 350);
  }
}

TEST(SetCoverTest, KnightsCoverMip) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  SetCoverModel model = CreateKnightsCoverModel(BoardSize, BoardSize);
  SetCoverLedger ledger(&model);
  SetCoverMip mip(&ledger);
  mip.SetTimeLimitInSeconds(10);
  mip.NextSolution();
  SubsetBoolVector best_choices = ledger.is_selected();
  DisplayKnightsCoverSolution(best_choices, BoardSize, BoardSize);
  LOG(INFO) << "Mip cost: " << ledger.cost();
}

void BM_Steepest(benchmark::State& state) {
  for (auto s : state) {
    SetCoverModel model = CreateKnightsCoverModel(SIZE, SIZE);
    SetCoverLedger ledger(&model);
    GreedySolutionGenerator greedy(&ledger);
    SteepestSearch steepest(&ledger);
  }
}

BENCHMARK(BM_Steepest)->Arg(1 << 5);

}  // namespace
}  // namespace operations_research
