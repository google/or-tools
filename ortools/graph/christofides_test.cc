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

#include "ortools/graph/christofides.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"

namespace operations_research {

// Displays the path.
std::string PathToString(absl::Span<const int> path) {
  std::string path_string;
  const int size = path.size();
  for (int i = 0; i < size; i++) {
    absl::StrAppendFormat(&path_string, "%d ", path[i]);
  }
  return path_string;
}

// Prints the cost and the computed path.
template <typename C>
void ComputeAndShow(const std::string& name,
                    ChristofidesPathSolver<C>* chris_solver) {
  LOG(INFO) << name << " TSP cost = " << chris_solver->TravelingSalesmanCost();
  LOG(INFO) << name << " TSP path = "
            << PathToString(chris_solver->TravelingSalesmanPath());
}

void TestChristofides(const std::string& name, const int size,
                      absl::Span<const int> cost_data,
                      bool use_minimal_matching, bool use_mip,
                      const int expected_cost,
                      absl::string_view expected_solution) {
  using MatchingAlgorithm = ChristofidesPathSolver<int>::MatchingAlgorithm;
  std::vector<std::vector<int>> cost_mat(size);
  for (int i = 0; i < size; ++i) {
    cost_mat[i].resize(size);
  }
  int col = 0;
  int row = 0;
  for (int i = 0; i < cost_data.size(); ++i) {
    cost_mat[row][col] = cost_data[i];
    cost_mat[col][row] = cost_data[i];
    ++col;
    if (col > row) {
      col = 0;
      ++row;
    }
  }
  ChristofidesPathSolver<int> chris_solver(
      size, [&cost_mat](int i, int j) { return cost_mat[i][j]; });
  if (!use_minimal_matching) {
    if (use_mip) {
      chris_solver.SetMatchingAlgorithm(
          MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING_WITH_MIP);
    } else {
      chris_solver.SetMatchingAlgorithm(
          MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING);
    }
  } else {
    chris_solver.SetMatchingAlgorithm(
        MatchingAlgorithm::MINIMAL_WEIGHT_MATCHING);
  }
  ComputeAndShow(name, &chris_solver);
  EXPECT_EQ(expected_cost, chris_solver.TravelingSalesmanCost());
  EXPECT_EQ(expected_solution,
            PathToString(chris_solver.TravelingSalesmanPath()));
}

// Gr17 as taken from TSPLIB:
// http://elib.zib.de/pub/mp-testdata/tsp/tsplib/tsplib.html
// Only the lower half of the distance matrix is given. This explains the
// filling of the cost matrix, which is a bit more complicated than usual.

TEST(HamiltonianPathTest, Gr17) {
  const int kGr17Size = 17;
  const std::vector<int> gr17_data = {
      0,   633, 0,   257, 390, 0,   91,  661, 228, 0,   412, 227, 169, 383,
      0,   150, 488, 112, 120, 267, 0,   80,  572, 196, 77,  351, 63,  0,
      134, 530, 154, 105, 309, 34,  29,  0,   259, 555, 372, 175, 338, 264,
      232, 249, 0,   505, 289, 262, 476, 196, 360, 444, 402, 495, 0,   353,
      282, 110, 324, 61,  208, 292, 250, 352, 154, 0,   324, 638, 437, 240,
      421, 329, 297, 314, 95,  578, 435, 0,   70,  567, 191, 27,  346, 83,
      47,  68,  189, 439, 287, 254, 0,   211, 466, 74,  182, 243, 105, 150,
      108, 326, 336, 184, 391, 145, 0,   268, 420, 53,  239, 199, 123, 207,
      165, 383, 240, 140, 448, 202, 57,  0,   246, 745, 472, 237, 528, 364,
      332, 349, 202, 685, 542, 157, 289, 426, 483, 0,   121, 518, 142, 84,
      297, 35,  29,  36,  236, 390, 238, 301, 55,  96,  153, 336, 0};
  TestChristofides("Gr17", kGr17Size, gr17_data, false, true, 2190,
                   "0 12 6 7 5 10 4 1 9 2 14 13 16 3 8 11 15 0 ");
  TestChristofides("Gr17", kGr17Size, gr17_data, false, false, 2190,
                   "0 12 6 7 5 10 4 1 9 2 14 13 16 3 8 11 15 0 ");
  TestChristofides("Gr17", kGr17Size, gr17_data, true, false, 2421,
                   "0 12 3 8 11 15 1 4 10 9 2 14 13 16 6 7 5 0 ");
}

// Gr24 as taken from TSPLIB:
// http://elib.zib.de/pub/mp-testdata/tsp/tsplib/tsplib.html
// Only the lower half of the distance matrix is given. This explains the
// filling of the cost matrix, which is a bit more complicated than usual.

TEST(HamiltonianPathTest, Gr24) {
  const int kGr24Size = 24;
  const std::vector<int> gr24_data = {
      0,   257, 0,   187, 196, 0,   91,  228, 158, 0,   150, 112, 96,  120, 0,
      80,  196, 88,  77,  63,  0,   130, 167, 59,  101, 56,  25,  0,   134, 154,
      63,  105, 34,  29,  22,  0,   243, 209, 286, 159, 190, 216, 229, 225, 0,
      185, 86,  124, 156, 40,  124, 95,  82,  207, 0,   214, 223, 49,  185, 123,
      115, 86,  90,  313, 151, 0,   70,  191, 121, 27,  83,  47,  64,  68,  173,
      119, 148, 0,   272, 180, 315, 188, 193, 245, 258, 228, 29,  159, 342, 209,
      0,   219, 83,  172, 149, 79,  139, 134, 112, 126, 62,  199, 153, 97,  0,
      293, 50,  232, 264, 148, 232, 203, 190, 248, 122, 259, 227, 219, 134, 0,
      54,  219, 92,  82,  119, 31,  43,  58,  238, 147, 84,  53,  267, 170, 255,
      0,   211, 74,  81,  182, 105, 150, 121, 108, 310, 37,  160, 145, 196, 99,
      125, 173, 0,   290, 139, 98,  261, 144, 176, 164, 136, 389, 116, 147, 224,
      275, 178, 154, 190, 79,  0,   268, 53,  138, 239, 123, 207, 178, 165, 367,
      86,  187, 202, 227, 130, 68,  230, 57,  86,  0,   261, 43,  200, 232, 98,
      200, 171, 131, 166, 90,  227, 195, 137, 69,  82,  223, 90,  176, 90,  0,
      175, 128, 76,  146, 32,  76,  47,  30,  222, 56,  103, 109, 225, 104, 164,
      99,  57,  112, 114, 134, 0,   250, 99,  89,  221, 105, 189, 160, 147, 349,
      76,  138, 184, 235, 138, 114, 212, 39,  40,  46,  136, 96,  0,   192, 228,
      235, 108, 119, 165, 178, 154, 71,  136, 262, 110, 74,  96,  264, 187, 182,
      261, 239, 165, 151, 221, 0,   121, 142, 99,  84,  35,  29,  42,  36,  220,
      70,  126, 55,  249, 104, 178, 60,  96,  175, 153, 146, 47,  135, 169, 0};
  TestChristofides(
      "Gr24", kGr24Size, gr24_data, false, true, 1407,
      "0 15 5 6 2 10 7 20 4 9 16 21 17 18 1 14 19 12 8 22 13 23 11 3 0 ");
  TestChristofides(
      "Gr24", kGr24Size, gr24_data, false, false, 1407,
      "0 15 5 6 2 10 7 20 4 9 16 21 17 18 1 14 19 12 8 22 13 23 11 3 0 ");
  TestChristofides(
      "Gr24", kGr24Size, gr24_data, true, false, 1607,
      "0 15 5 6 7 20 4 9 16 21 17 18 1 19 14 13 22 8 12 10 2 23 11 3 0 ");
}

// This is the geographic distance as defined in TSPLIB. It is used here to
// obtain the right value for Ulysses22. ToRad is a helper function as defined
// in TSPLIB.

static double ToRad(double x) {
  const double kPi = 3.141592;
  const int64_t deg = static_cast<int64_t>(x);
  const double min = x - deg;
  return kPi * (deg + 5.0 * min / 3.0) / 180.0;
}

static int64_t GeoDistance(double from_lng, double from_lat, double to_lng,
                           double to_lat) {
  const double kTsplibRadius = 6378.388;
  const double q1 = cos(ToRad(from_lng) - ToRad(to_lng));
  const double q2 = cos(ToRad(from_lat) - ToRad(to_lat));
  const double q3 = cos(ToRad(from_lat) + ToRad(to_lat));
  return static_cast<int64_t>(
      kTsplibRadius * acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0);
}

// Ulysses22 data as taken from TSPLIB.

TEST(HamiltonianPathTest, Ulysses) {
  const int kUlyssesTourSize = 22;
  const double kLat[kUlyssesTourSize] = {
      38.24, 39.57, 40.56, 36.26, 33.48, 37.56, 38.42, 37.52,
      41.23, 41.17, 36.08, 38.47, 38.15, 37.51, 35.49, 39.36,
      38.09, 36.09, 40.44, 40.33, 40.37, 37.57};
  const double kLong[kUlyssesTourSize] = {
      20.42, 26.15, 25.32, 23.12, 10.54, 12.19, 13.11, 20.44,
      9.10,  13.05, -5.21, 15.13, 15.35, 15.17, 14.32, 19.56,
      24.36, 23.00, 13.57, 14.15, 14.23, 22.56};
  std::vector<std::vector<double>> cost(kUlyssesTourSize);
  for (int i = 0; i < kUlyssesTourSize; ++i) {
    cost[i].resize(kUlyssesTourSize);
    for (int j = 0; j < kUlyssesTourSize; ++j) {
      cost[i][j] = GeoDistance(kLong[i], kLat[i], kLong[j], kLat[j]);
    }
    // GeoDistance can return != 0 for i == j, we don't want that.
    cost[i][i] = 0;
  }
  ChristofidesPathSolver<double> chris_solver(
      kUlyssesTourSize, [&cost](int i, int j) { return cost[i][j]; });
  chris_solver.SetMatchingAlgorithm(
      ChristofidesPathSolver<
          double>::MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING);
  ComputeAndShow("Ulysses22", &chris_solver);
  EXPECT_EQ(7448, chris_solver.TravelingSalesmanCost());
  EXPECT_EQ("0 7 21 16 1 2 3 17 12 13 14 4 10 8 9 18 19 20 11 6 5 15 0 ",
            PathToString(chris_solver.TravelingSalesmanPath()));
}

TEST(ChristofidesTest, EmptyModel) {
  ChristofidesPathSolver<int> chris_solver(0, [](int, int) { return 0; });
  EXPECT_EQ(0, chris_solver.TravelingSalesmanCost());
  EXPECT_TRUE(chris_solver.TravelingSalesmanPath().empty());
}

TEST(ChristofidesTest, SingleNodeModel) {
  ChristofidesPathSolver<int> chris_solver(1, [](int, int) { return 0; });
  EXPECT_EQ(0, chris_solver.TravelingSalesmanCost());
  EXPECT_EQ("0 0 ", PathToString(chris_solver.TravelingSalesmanPath()));
}

TEST(ChristofidesTest, Int64Overflow) {
  ChristofidesPathSolver<int64_t> chris_solver(
      10, [](int, int) { return std::numeric_limits<int64_t>::max() / 2; });
  EXPECT_EQ(std::numeric_limits<int64_t>::max(),
            chris_solver.TravelingSalesmanCost());
}

TEST(ChristofidesTest, SaturatedDouble) {
  ChristofidesPathSolver<double> chris_solver(
      10, [](int, int) { return std::numeric_limits<double>::max() / 2.0; });
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            chris_solver.TravelingSalesmanCost());
}

TEST(ChristofidesTest, NoPerfectMatching) {
  ChristofidesPathSolver<int64_t> chris_solver(4, [](int i, int j) {
    // 1 and 3 cannot be connected,
    const int64_t cost[][4] = {
        {0, 0, 0, 0},
        {0, 0, 1, std::numeric_limits<int64_t>::max() / 2},
        {0, 1, 0, 1},
        {0, std::numeric_limits<int64_t>::max() / 2, 1, 0}};
    return cost[i][j];
  });
  // NOTE(user): Add a test for which MIP matching fails too.
  chris_solver.SetMatchingAlgorithm(
      ChristofidesPathSolver<
          int64_t>::MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING);
  EXPECT_FALSE(chris_solver.Solve());
}

// Benchmark for the Christofides algorithm on a 'size' by 'size' grid of nodes.
template <bool use_minimal_matching>
void BM_ChristofidesPathSolver(benchmark::State& state) {
  int size = state.range(0);
  const int num_nodes = size * size;
  std::vector<std::vector<int>> costs(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    const int x_i = i / size;
    const int y_i = i % size;
    costs[i].resize(num_nodes, 0);
    for (int j = 0; j < num_nodes; ++j) {
      const int x_j = j / size;
      const int y_j = j % size;
      costs[i][j] = std::abs(x_i - x_j) + std::abs(y_i - y_j);
    }
  }
  auto cost = [&costs](int i, int j) { return costs[i][j]; };
  using Cost = decltype(cost);
  using MatchingAlgorithm =
      typename ChristofidesPathSolver<int, int, int, Cost>::MatchingAlgorithm;
  for (auto _ : state) {
    ChristofidesPathSolver<int, int, int, Cost> chris_solver(num_nodes, cost);
    if (use_minimal_matching) {
      chris_solver.SetMatchingAlgorithm(
          MatchingAlgorithm::MINIMAL_WEIGHT_MATCHING);
    } else {
      chris_solver.SetMatchingAlgorithm(
          MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING);
    }
    EXPECT_NE(0, chris_solver.TravelingSalesmanCost());
  }
}

BENCHMARK_TEMPLATE(BM_ChristofidesPathSolver, true)->Range(2, 1 << 5);
BENCHMARK_TEMPLATE(BM_ChristofidesPathSolver, false)->Range(2, 1 << 4);

}  // namespace operations_research
