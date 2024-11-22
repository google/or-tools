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
#include <vector>

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
#include "ortools/base/parse_text_proto.h"

namespace operations_research {
namespace {
using google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using CL = SetCoverInvariant::ConsistencyLevel;

TEST(SetCoverTest, GuidedLocalSearchVerySmall) {
  SetCoverProto proto = ParseTextProtoOrDie(R"pb(
    subset { cost: 1 element: 1 element: 2 }
    subset { cost: 1 element: 0 })pb");

  SetCoverModel model;
  model.ImportModelFromProto(proto);
  CHECK(model.ComputeFeasibility());
  SetCoverInvariant inv(&model);
  GreedySolutionGenerator greedy_search(&inv);
  CHECK(greedy_search.NextSolution());
  CHECK(inv.CheckConsistency(CL::kFreeAndUncovered));
  GuidedLocalSearch search(&inv);
  CHECK(search.NextSolution(100));
  CHECK(inv.CheckConsistency(CL::kRedundancy));
}

class KnightsCover {
 public:
  KnightsCover(int num_rows, int num_cols)
      : num_rows_(num_rows), num_cols_(num_cols), model_() {
    constexpr int knight_row_move[] = {2, 1, -1, -2, -2, -1, 1, 2};
    constexpr int knight_col_move[] = {1, 2, 2, 1, -1, -2, -2, -1};
    for (int row = 0; row < num_rows_; ++row) {
      for (int col = 0; col < num_cols_; ++col) {
        model_.AddEmptySubset(1);
        model_.AddElementToLastSubset(ElementNumber(row, col));
        for (int i = 0; i < 8; ++i) {
          const int new_row = row + knight_row_move[i];
          const int new_col = col + knight_col_move[i];
          if (IsOnBoard(new_row, new_col)) {
            model_.AddElementToLastSubset(ElementNumber(new_row, new_col));
          }
        }
      }
    }
  }

  SetCoverModel model() const { return model_; }

  void DisplaySolution(const SubsetBoolVector& choices) const {
    std::string line;
    std::string separator = "+";
    for (int col = 0; col < num_cols_; ++col) {
      absl::StrAppend(&separator, "-+");
    }
    LOG(INFO) << separator;
    for (int row = 0; row < num_rows_; ++row) {
      line = "|";
      for (int col = 0; col < num_cols_; ++col) {
        const SubsetIndex subset(SubsetNumber(row, col));
        absl::StrAppend(&line, choices[subset] ? "X|" : " |");
      }
      LOG(INFO) << line;
      LOG(INFO) << separator;
    }
  }

  void ClearSubsetWithinRadius(int row, int col, int radius,
                               SetCoverInvariant* inv) const {
    for (int r = row - radius; r <= row + radius; ++r) {
      for (int c = col - radius; c <= col + radius; ++c) {
        if (!IsOnBoard(r, c)) continue;
        const SubsetIndex subset(SubsetNumber(r, c));
        if (inv->is_selected()[subset]) {
          inv->Deselect(subset, CL::kFreeAndUncovered);
        }
      }
    }
  }

 private:
  bool IsOnBoard(int row, int col) const {
    return row >= 0 && row < num_rows_ && col >= 0 && col < num_cols_;
  }
  // There is a 1:1 mapping between elements and subsets because the
  // subset i corresponds to placing the element i at the position (r, c).
  ElementIndex ElementNumber(int row, int col) const {
    return ElementIndex(row * num_cols_ + col);
  }
  SubsetIndex SubsetNumber(int row, int col) const {
    return SubsetIndex(row * num_cols_ + col);
  }
  int num_rows_;
  int num_cols_;
  SetCoverModel model_;
};

TEST(SetCoverProtoTest, SaveReload) {
  SetCoverModel model = KnightsCover(10, 10).model();
  SetCoverProto proto = model.ExportModelAsProto();
  SetCoverModel reloaded;
  reloaded.ImportModelFromProto(proto);
  EXPECT_EQ(model.num_subsets(), reloaded.num_subsets());
  EXPECT_EQ(model.num_elements(), reloaded.num_elements());
  EXPECT_EQ(model.subset_costs(), reloaded.subset_costs());
  EXPECT_EQ(model.columns(), reloaded.columns());
}

TEST(SolutionProtoTest, SaveReloadTwice) {
  SetCoverModel model = KnightsCover(3, 3).model();
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
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
  EXPECT_TRUE(model.ComputeFeasibility());
}

TEST(SetCoverTest, KnightsCoverTrivalAndGreedy) {
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
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
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
  SetCoverInvariant inv(&model);

  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverDegree) {
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
  SetCoverInvariant inv(&model);

  ElementDegreeSolutionGenerator degree(&inv);
  CHECK(degree.NextSolution());
  LOG(INFO) << "ElementDegreeSolutionGenerator cost: " << inv.cost();

  SteepestSearch steepest(&inv);
  CHECK(steepest.NextSolution(100));
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverGLS) {
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
  SetCoverInvariant inv(&model);
  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();
  GuidedLocalSearch gls(&inv);
  CHECK(gls.NextSolution(100));
  LOG(INFO) << "GuidedLocalSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverRandom) {
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
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
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
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
  KnightsCover knights(BoardSize, BoardSize);
  SetCoverModel model = knights.model();
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
  knights.DisplaySolution(inv.is_selected());
}

TEST(SetCoverTest, KnightsCoverGreedyRandomClear) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  KnightsCover knights(BoardSize, BoardSize);
  SetCoverModel model = knights.model();
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
  knights.DisplaySolution(best_choices);
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
  KnightsCover knights(BoardSize, BoardSize);
  SetCoverModel model = knights.model();
  SetCoverInvariant inv(&model);
  Cost best_cost = std::numeric_limits<Cost>::max();
  SubsetBoolVector best_choices;
  for (int i = 0; i < 10000; ++i) {
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
  knights.DisplaySolution(best_choices);
  LOG(INFO) << "RandomClear cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    CHECK_GE(inv.cost(), 350);
  }
}

TEST(SetCoverTest, KnightsCoverElementDegreeRadiusClear) {
#ifdef NDEBUG
  constexpr int BoardSize = 50;
#else
  constexpr int BoardSize = 15;
#endif
  KnightsCover knights(BoardSize, BoardSize);
  SetCoverModel model = knights.model();
  SetCoverInvariant inv(&model);
  Cost best_cost = std::numeric_limits<Cost>::max();
  SubsetBoolVector best_choices;
  int iteration = 0;
  for (int radius = 7; radius >= 1; --radius) {
    for (int row = 0; row < BoardSize; ++row) {
      for (int col = 0; col < BoardSize; ++col) {
        LazyElementDegreeSolutionGenerator degree(&inv);
        CHECK(degree.NextSolution());
        CHECK(inv.CheckConsistency(CL::kCostAndCoverage));

        SteepestSearch steepest(&inv);
        CHECK(steepest.NextSolution(100));

        if (inv.cost() < best_cost) {
          best_cost = inv.cost();
          best_choices = inv.is_selected();
          LOG(INFO) << "Best cost: " << best_cost
                    << " at iteration = " << iteration;
        }
        inv.LoadSolution(best_choices);
        knights.ClearSubsetWithinRadius(row, col, radius, &inv);
        ++iteration;
      }
    }
  }
  inv.LoadSolution(best_choices);
  knights.DisplaySolution(best_choices);
  LOG(INFO) << "RadiusClear cost: " << best_cost;
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
  KnightsCover knights(BoardSize, BoardSize);
  SetCoverModel model = knights.model();
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
  knights.DisplaySolution(best_choices);
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
  KnightsCover knights(BoardSize, BoardSize);
  SetCoverModel model = knights.model();
  SetCoverInvariant inv(&model);
  SetCoverMip mip(&inv);
  mip.NextSolution(true, .5);
  LOG(INFO) << "Mip cost: " << inv.cost();
  knights.DisplaySolution(inv.is_selected());
  if (BoardSize == 50) {
    CHECK_GE(inv.cost(), 350);
  }
}

void BM_Steepest(benchmark::State& state) {
  for (auto s : state) {
    SetCoverModel model = KnightsCover(SIZE, SIZE).model();
    SetCoverInvariant inv(&model);
    GreedySolutionGenerator greedy(&inv);
    SteepestSearch steepest(&inv);
  }
}

BENCHMARK(BM_Steepest)->Arg(1 << 5);

}  // namespace
}  // namespace operations_research
