// Copyright 2010-2025 Google LLC
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

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover.pb.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_mip.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {
using CL = SetCoverInvariant::ConsistencyLevel;

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

  void DisplaySolutionAscii(const SubsetBoolVector& choices) const {
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

  void DisplaySolutionUnicode(const SubsetBoolVector& choices) const {
    for (int row = 0; row < num_rows_; ++row) {
      std::string line;
      for (int col = 0; col < num_cols_; ++col) {
        const SubsetIndex subset(SubsetNumber(row, col));

        absl::StrAppend(&line, choices[subset] ? "♞" : ".");
      }
      LOG(INFO) << line;
    }
  }

  void DisplaySolution(const SubsetBoolVector& choices) const {
    DisplaySolutionUnicode(choices);
  }

  std::vector<SubsetIndex> ClearSubsetWithinRadius(
      const SetCoverInvariant::ConsistencyLevel consistency, int row, int col,
      int radius, SetCoverInvariant* inv) const {
    std::vector<SubsetIndex> cleared_subsets;
    cleared_subsets.reserve((2 * radius + 1) * (2 * radius + 1));
    for (int r = row - radius; r <= row + radius; ++r) {
      for (int c = col - radius; c <= col + radius; ++c) {
        if (!IsOnBoard(r, c)) continue;
        const SubsetIndex subset(SubsetNumber(r, c));
        if (inv->is_selected()[subset]) {
          inv->Deselect(subset, consistency);
          cleared_subsets.push_back(subset);
        }
      }
    }
    return cleared_subsets;
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
  model.SortElementsInSubsets();
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
  CHECK(steepest.SetMaxIterations(500).NextSolution());
  EXPECT_TRUE(inv.CheckConsistency(CL::kRedundancy));
  SetCoverSolutionResponse steepest_proto = inv.ExportSolutionAsProto();
  inv.ImportSolutionFromProto(greedy_proto);
  CHECK(steepest.SetMaxIterations(500).NextSolution());
  EXPECT_TRUE(inv.CheckConsistency(CL::kRedundancy));
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
  CHECK(steepest.SetMaxIterations(100'000).NextSolution());
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
  CHECK(steepest.SetMaxIterations(100).NextSolution());
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverDegree) {
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
  SetCoverInvariant inv(&model);

  ElementDegreeSolutionGenerator degree(&inv);
  CHECK(degree.NextSolution());
  LOG(INFO) << "ElementDegreeSolutionGenerator cost: " << inv.cost();

  SteepestSearch steepest(&inv);
  CHECK(steepest.SetMaxIterations(100).NextSolution());
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
}

TEST(SetCoverTest, KnightsCoverGLS) {
  SetCoverModel model = KnightsCover(SIZE, SIZE).model();
  SetCoverInvariant inv(&model);
  GreedySolutionGenerator greedy(&inv);
  CHECK(greedy.NextSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << inv.cost();
  GuidedLocalSearch gls(&inv);
  CHECK(gls.SetMaxIterations(100).NextSolution());
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
  CHECK(steepest.SetMaxIterations(100).NextSolution());
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
  CHECK(steepest.SetMaxIterations(100).NextSolution());
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
  CHECK(steepest.SetMaxIterations(100).NextSolution());
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));

  GuidedTabuSearch gts(&inv);
  CHECK(gts.SetMaxIterations(1'000).NextSolution());
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
    CHECK(steepest.SetMaxIterations(10'000).NextSolution());

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
    EXPECT_GE(inv.cost(), 350);
  }
}

TEST(SetCoverTest, KnightsCoverCliqueGuidedLNS) {
  // Best known values for the Knight Covering Problem.
  // 0-2: trivial/undefined (0).
  // 3-10: https://www.contestcen.com/kn3.htm
  // 11+: https://www.contestcen.com/knight.htm
  const std::vector<int> best_known_cost = {
      0,  0,  0,                               // 0-2
      4,  4,  5,  8,  10, 12, 14, 16,          // 3-10
      21, 24, 28, 32, 36, 40, 46, 52, 57, 62,  // 11-20
      68, 75, 82, 88, 95,                      // 21-25
      // 102, 109, 116, 124, 131,                 // 26-30
      // 140, 148, 157, 166, 176, 186, 196, 206, 217, 228,  // 31-40
      // 239, 250, 261, 273, 285, 297, 309, 322, 335, 350   // 41-50
  };

  for (int BoardSize = 3; BoardSize < 21; ++BoardSize) {
    KnightsCover knights(BoardSize, BoardSize);
    SetCoverModel model = knights.model();
    SetCoverInvariant inv(&model);
    Cost lower_bound = ComputeDualAscentLB(inv, 1000);
    LOG(INFO) << "Dual ascent Lower bound: " << lower_bound;
    Cost lower_bound_degree = ComputeDegreeBasedDualAscentLB(inv, 1000);
    LOG(INFO) << "Degree based dual ascent Lower bound: " << lower_bound_degree;
    LOG(INFO) << "Lower bound: " << std::max(lower_bound, lower_bound_degree);
    LazyElementDegreeSolutionGenerator degree(&inv);
    degree.SetNumRandomPasses(1000);
    CHECK(degree.NextSolution());
    LOG(INFO) << "LazyElementDegreeSolutionGenerator cost: " << inv.cost();
    SteepestSearch steepest(&inv);
    CHECK(steepest.SetMaxIterations(100).NextSolution());
    LOG(INFO) << "LazyElementDegreeSolutionGenerator + SteepestSearch cost: "
              << inv.cost();
    CliqueGuidedLNS clique_guided_lns(&inv);
    clique_guided_lns.SetMaxCliqueSize(1000).SetMaxNumCliques(400);
    clique_guided_lns.SetTimeLimit(absl::Milliseconds(500));
    CHECK(clique_guided_lns.NextSolution());
    LOG(INFO) << "CliqueGuidedLNS cost (" << BoardSize << " * " << BoardSize
              << "): " << inv.cost() << " / " << best_known_cost[BoardSize]
              << " Time:" << ToInt64Milliseconds(clique_guided_lns.run_time());
    EXPECT_GE(inv.cost(), best_known_cost[BoardSize]);
    knights.DisplaySolution(inv.is_selected());
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

  LazyElementDegreeSolutionGenerator degree(&inv);
  LazySteepestSearch steepest(&inv);
  std::vector<SetCoverDecision> best_trace;
  ElementToIntVector best_coverage;
  for (int iteration = 0; iteration < 10000; ++iteration) {
    CHECK(degree.NextSolution());
    CHECK(steepest.SetMaxIterations(100).NextSolution());

    if (inv.cost() < best_cost) {
      best_cost = inv.cost();
      inv.CompressTrace();
      best_trace = inv.trace();
      best_coverage = inv.coverage();
      LOG(INFO) << "Best cost: " << best_cost
                << " at iteration = " << iteration;
    } else {
      inv.LoadTraceAndCoverage(best_trace, best_coverage);
    }
    ClearRandomSubsets(0.1 * inv.trace().size(), &inv);
  }
  inv.LoadTraceAndCoverage(best_trace, best_coverage);
  knights.DisplaySolution(inv.is_selected());
  LOG(INFO) << "RandomClear cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    EXPECT_GE(inv.cost(), 350);
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
  std::vector<SetCoverDecision> best_trace;
  ElementToIntVector best_coverage;
  int iteration = 0;
  LazyElementDegreeSolutionGenerator degree(&inv);
  LazySteepestSearch steepest(&inv);
  for (int radius = 8; radius >= 1; --radius) {
    for (int row = 0; row < BoardSize; ++row) {
      for (int col = 0; col < BoardSize; ++col) {
        CHECK(degree.NextSolution());
        DCHECK(inv.CheckConsistency(CL::kCostAndCoverage));

        LazySteepestSearch steepest(&inv);
        CHECK(steepest.SetMaxIterations(100).NextSolution());

        if (inv.cost() < best_cost) {
          best_cost = inv.cost();
          inv.CompressTrace();
          best_trace = inv.trace();
          best_coverage = inv.coverage();
          LOG(INFO) << "Best cost: " << best_cost
                    << " at iteration = " << iteration;
        } else {
          inv.LoadTraceAndCoverage(best_trace, best_coverage);
        }
        knights.ClearSubsetWithinRadius(CL::kCostAndCoverage, row, col, radius,
                                        &inv);
        ++iteration;
      }
    }
  }
  inv.LoadTraceAndCoverage(best_trace, best_coverage);
  knights.DisplaySolution(inv.is_selected());
  LOG(INFO) << "RadiusClear cost: " << best_cost;
  // The best solution found until 2023-08 has a cost of 350.
  // http://www.contestcen.com/kn50.htm
  if (BoardSize == 50) {
    EXPECT_GE(inv.cost(), 350);
  }
}

TEST(SetCoverTest, DISABLED_KnightsCoverRandomClearMip) {
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
  CHECK(steepest.SetMaxIterations(100).NextSolution());
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();

  Cost best_cost = inv.cost();
  SubsetBoolVector best_choices = inv.is_selected();
  for (int i = 0; i < 1'000; ++i) {
    auto focus = ClearRandomSubsets(0.1 * inv.trace().size(), &inv);
    SetCoverMip mip(&inv);
    mip.UseIntegers(true).SetTimeLimit(absl::Seconds(1));
    mip.NextSolution(focus);
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
  mip.UseIntegers(true).SetTimeLimit(absl::Milliseconds(500));
  mip.NextSolution();
  LOG(INFO) << "Mip cost: " << inv.cost();
  knights.DisplaySolution(inv.is_selected());
  if (BoardSize == 50) {
    EXPECT_GE(inv.cost(), 350);
  }
}

}  // namespace
}  // namespace operations_research
