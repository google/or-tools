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

#include "ortools/graph/hamiltonian_path.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"

namespace operations_research {

TEST(SetTest, Enumerate) {
  typedef Set<uint64_t> Set64;
  for (int card = 0; card <= 64; ++card) {
    Set64 set = Set64::FullSet(card);
    ASSERT_EQ(card, set.Cardinality());
    if (set.value() != 0) {
      ASSERT_EQ(0, set.SmallestElement());
    }
    int pos = 0;
    for (int i : set) {
      EXPECT_EQ(pos, i);
      ++pos;
    }
    EXPECT_EQ(card, pos);
  }
}

int64_t Choose(int64_t n, int64_t k) {
  double result = 1.0;
  for (int i = 1; i <= k; ++i) {
    result *= static_cast<double>(n - k + i) / static_cast<double>(i);
  }
  return static_cast<int64_t>(std::round(result));
}

TEST(SetRangeWithCardinalityTest, Enumerate) {
  typedef Set<uint32_t> Set32;
  for (int max_card = 1; max_card <= 16; ++max_card) {
    for (int card = 1; card <= max_card; ++card) {
      int64_t num_subsets = 0;
      Set32 previous_s(0);
      for (const Set32 s :
           SetRangeWithCardinality<Set<uint32_t>>(card, max_card)) {
        ++num_subsets;
        EXPECT_EQ(card, s.Cardinality());
        EXPECT_LT(previous_s.value(), s.value());
        previous_s = s;
      }
      EXPECT_EQ(Choose(max_card, card), num_subsets);
    }
  }
}

TEST(LatticeMemoryManagerTest, Offset) {
  typedef Set<uint32_t> Set32;
  for (int max_card = 1; max_card < 16; ++max_card) {
    LatticeMemoryManager<Set<uint32_t>, double> memory;
    memory.Init(max_card);
    int previous_pos = -1;
    for (int card = 1; card <= max_card; ++card) {
      for (Set32 set : SetRangeWithCardinality<Set32>(card, max_card)) {
        for (int node : set) {
          const int pos = memory.Offset(set, node);
          EXPECT_EQ(previous_pos + 1, pos);
          EXPECT_EQ(pos, memory.BaseOffset(card, set) + set.ElementRank(node));
          previous_pos = pos;
        }
      }
    }
  }
}

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
template <typename T, typename C>
void ComputeAndShow(const std::string& name,
                    HamiltonianPathSolver<T, C>* ham_solver) {
  int best_end_node = ham_solver->BestHamiltonianPathEndNode();
  LOG(INFO) << name << " End node = " << best_end_node;
  LOG(INFO) << name << " Robustness = " << ham_solver->IsRobust();
  LOG(INFO) << name << " TSP cost = " << ham_solver->TravelingSalesmanCost();
  LOG(INFO) << name << " TSP path = "
            << PathToString(ham_solver->TravelingSalesmanPath());
  LOG(INFO) << name << " Hamiltonian path cost = "
            << ham_solver->HamiltonianCost(best_end_node);
  LOG(INFO) << name << " Hamiltonian path = "
            << PathToString(ham_solver->HamiltonianPath(best_end_node));
}

// Gr17 as taken from TSPLIB:
// http://elib.zib.de/pub/mp-testdata/tsp/tsplib/tsplib.html
// Only the lower half of the distance matrix is given. This explains the
// filling of the cost matrix, which is a bit more complicated than usual.

TEST(HamiltonianPathTest, Gr17) {
  const int kGr17Data[] = {
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
  const int kGr17Size = 17;  // this is the size of the cost matrix for gr17
  std::vector<std::vector<int>> cost_mat(kGr17Size);
  for (int i = 0; i < kGr17Size; ++i) {
    cost_mat[i].resize(kGr17Size);
  }
  int col = 0;
  int row = 0;
  for (int i = 0; i < ABSL_ARRAYSIZE(kGr17Data); ++i) {
    cost_mat[row][col] = kGr17Data[i];
    cost_mat[col][row] = kGr17Data[i];
    ++col;
    if (col > row) {
      col = 0;
      ++row;
    }
  }
  HamiltonianPathSolver<int, std::vector<std::vector<int>>> ham_solver(
      cost_mat);
  EXPECT_TRUE(ham_solver.IsRobust());
  ComputeAndShow("Gr17", &ham_solver);
  EXPECT_EQ(2085, ham_solver.TravelingSalesmanCost());
  EXPECT_EQ("0 15 11 8 4 1 9 10 2 14 13 16 5 7 6 12 3 0 ",
            PathToString(ham_solver.TravelingSalesmanPath()));
  int best_end_node = ham_solver.BestHamiltonianPathEndNode();
  EXPECT_EQ(1707, ham_solver.HamiltonianCost(best_end_node));
  EXPECT_EQ("0 15 11 8 3 12 6 7 5 16 13 14 2 10 4 9 1 ",
            PathToString(ham_solver.HamiltonianPath(best_end_node)));

  PruningHamiltonianSolver<int, std::vector<std::vector<int>>> prune_solver(
      cost_mat);
  EXPECT_EQ(1707, prune_solver.HamiltonianCost(best_end_node));
}

// Gr24 as taken from TSPLIB:
// http://elib.zib.de/pub/mp-testdata/tsp/tsplib/tsplib.html
// Only the lower half of the distance matrix is given. This explains the
// filling of the cost matrix, which is a bit more complicated than usual.
// Currently disabled to speed up test. Can be run with
// --gunit_also_run_disabled_tests.

TEST(HamiltonianPathTest, DISABLED_Gr24) {
  const int kGr24Data[] = {
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

  const int kGr24Size = 24;  // this is the size of the cost matrix for gr24
  std::vector<std::vector<int>> cost_mat(kGr24Size);
  for (int i = 0; i < kGr24Size; ++i) {
    cost_mat[i].resize(kGr24Size);
  }
  int col = 0;
  int row = 0;
  for (int i = 0; i < ABSL_ARRAYSIZE(kGr24Data); ++i) {
    cost_mat[row][col] = kGr24Data[i];
    cost_mat[col][row] = kGr24Data[i];
    ++col;
    if (col > row) {
      col = 0;
      ++row;
    }
  }
  HamiltonianPathSolver<int, std::vector<std::vector<int>>> ham_solver(
      cost_mat);
  EXPECT_TRUE(ham_solver.IsRobust());
  ComputeAndShow("Gr24", &ham_solver);
  EXPECT_EQ(1272, ham_solver.TravelingSalesmanCost());
  EXPECT_EQ("0 15 10 2 6 5 23 7 20 4 9 16 21 17 18 14 1 19 13 12 8 22 3 11 0 ",
            PathToString(ham_solver.TravelingSalesmanPath()));
  int best_end_node = ham_solver.BestHamiltonianPathEndNode();
  EXPECT_EQ(1165, ham_solver.HamiltonianCost(best_end_node));
  EXPECT_EQ("0 15 5 23 11 3 22 8 12 13 19 1 14 18 21 17 16 9 4 20 7 6 2 10 ",
            PathToString(ham_solver.HamiltonianPath(best_end_node)));
}

// This is the geographic distance as defined in TSPLIB.
// It is used here so as to obtain the right value for Ulysses22.
// ToRad is a helper function as defined in TSPLIB.

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
  HamiltonianPathSolver<double, std::vector<std::vector<double>>> ham_solver(
      cost);
  EXPECT_TRUE(ham_solver.IsRobust());
  EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
  ComputeAndShow("Ulysses22", &ham_solver);
  EXPECT_EQ(7013, ham_solver.TravelingSalesmanCost());
  EXPECT_EQ("0 13 12 11 6 5 14 4 10 8 9 18 19 20 15 2 1 16 21 3 17 7 0 ",
            PathToString(ham_solver.TravelingSalesmanPath()));
  int best_end_node = ham_solver.BestHamiltonianPathEndNode();
  EXPECT_EQ(5423, ham_solver.HamiltonianCost(best_end_node));
  EXPECT_EQ("0 7 17 3 21 16 1 2 15 11 12 13 14 4 5 6 19 20 18 9 8 10 ",
            PathToString(ham_solver.HamiltonianPath(best_end_node)));
}

static double Euclidean(double x1, double y1, double x2, double y2) {
  const double dx = x1 - x2;
  const double dy = y1 - y2;
  return sqrt(dx * dx + dy * dy);
}

// Helper function for setting up a cost matrix with perturbation in the case of
// tests on problems with random coordinates. The idea is to increase the cost
// row and column for a given index so as to perturbate the matrix. If the
// increase fits with the precision of computations, the same resulting paths
// must be expected.

void InitEuclideanCosts(int size, std::vector<double> x, std::vector<double> y,
                        double perturbation,
                        std::vector<std::vector<double>>* cost) {
  cost->resize(size);
  for (int i = 0; i < size; ++i) {
    (*cost)[i].resize(size);
    for (int j = 0; j < size; ++j) {
      (*cost)[i][j] = Euclidean(x[i], y[i], x[j], y[j]);
    }
  }
  const int kPerturbationIndex = 5;
  if (perturbation != 0.0 && size > kPerturbationIndex) {
    for (int j = 0; j < size; ++j) {
      (*cost)[kPerturbationIndex][j] += perturbation;
      (*cost)[j][kPerturbationIndex] += perturbation;
    }
    (*cost)[kPerturbationIndex][kPerturbationIndex] = 0.0;
  }
}

bool ComparePaths(absl::Span<const int> path1, absl::Span<const int> path2) {
  // Returns true if TSP paths are equal or one is the reverse of the other.
  // TSP paths always start and end with 0 (the start node). For example, paths
  // (0, 1, 2, 3, 0) and (0, 3, 2, 1, 0) are equivalent, but (0, 1, 2, 3, 0) and
  // (0, 2, 3, 1, 0) are not.
  if (path1.size() != path2.size()) return false;
  int size = path1.size();
  bool same_ordering = true;
  for (int i = 0; i < size; ++i) {
    if (path1[i] != path2[i]) {
      same_ordering = false;
      break;
    }
  }
  if (same_ordering) return true;
  bool reverse_ordering = true;
  for (int i = 0; i < size; ++i) {
    if (path1[i] != path2[size - i - 1]) {
      reverse_ordering = false;
      break;
    }
  }
  return reverse_ordering;
}

TEST(HamiltonianPathTest, RandomPaths) {
  const int kMinSize = 6;
  const int kMaxSize = 20;
  std::vector<double> x(kMaxSize);
  std::vector<double> y(kMaxSize);

  std::mt19937 randomizer(0);
  for (int i = 0; i < kMaxSize; ++i) {
    x[i] = absl::Uniform(randomizer, 0, 100'000);
    y[i] = absl::Uniform(randomizer, 0, 100'000);
  }
  for (int size = kMinSize; size <= kMaxSize; ++size) {
    std::vector<std::vector<double>> cost;
    InitEuclideanCosts(size, x, y, 0, &cost);
    HamiltonianPathSolver<double, std::vector<std::vector<double>>> ham_solver(
        cost);
    EXPECT_TRUE(ham_solver.IsRobust());
    EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
    ComputeAndShow("RandomPath", &ham_solver);
    std::vector<int> good_path = ham_solver.TravelingSalesmanPath();
    InitEuclideanCosts(size, x, y, 1e15, &cost);
    ham_solver.ChangeCostMatrix(cost);
    EXPECT_TRUE(ham_solver.IsRobust());
    EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
    ComputeAndShow("RandomPath with manageable perturbation", &ham_solver);
    EXPECT_TRUE(ComparePaths(good_path, ham_solver.TravelingSalesmanPath()));
    InitEuclideanCosts(size, x, y, 1e25, &cost);
    ham_solver.ChangeCostMatrix(cost);
    EXPECT_FALSE(ham_solver.IsRobust());
    EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
    ComputeAndShow("RandomPath with unmanageable perturbation", &ham_solver);
    EXPECT_FALSE(ComparePaths(good_path, ham_solver.TravelingSalesmanPath()));
  }
}

TEST(HamiltonianPathTest, EmptyCosts) {
  std::vector<std::vector<int>> cost;
  HamiltonianPathSolver<int, std::vector<std::vector<int>>> ham_solver(cost);
  int best_end_node = ham_solver.BestHamiltonianPathEndNode();
  EXPECT_EQ(0, ham_solver.HamiltonianCost(best_end_node));
  EXPECT_EQ(0, ham_solver.TravelingSalesmanCost());
  std::vector<int> path = ham_solver.HamiltonianPath(best_end_node);
  EXPECT_EQ(0, path[0]);
  path = ham_solver.TravelingSalesmanPath();
  EXPECT_EQ(0, path[0]);
  ham_solver.ChangeCostMatrix(cost);
  path = ham_solver.HamiltonianPath(best_end_node);
  EXPECT_EQ(0, path[0]);
  path = ham_solver.TravelingSalesmanPath();
  EXPECT_EQ(0, path[0]);
  const int kSize = 10;
  cost.resize(kSize);
  for (int i = 0; i < kSize; ++i) {
    cost[i].resize(kSize);
  }
  ham_solver.ChangeCostMatrix(cost);
  path = ham_solver.TravelingSalesmanPath();
  EXPECT_EQ(kSize + 1, path.size());
}

TEST(HamiltonianPathTest, RectangleCosts) {
  using Solver = HamiltonianPathSolver<int, std::vector<std::vector<int>>>;
  const int kSize = 10;
  std::vector<std::vector<int>> cost(kSize);
  EXPECT_DEATH(Solver ham_solver(cost), "Matrix must be square.");
}

TEST(HamiltonianPathTest, SmallAsymmetricMatrix) {
  const int kAsymmetricMatrixSize = 3;
  int AsymmetricMatrix[kAsymmetricMatrixSize][kAsymmetricMatrixSize] = {
      {0, 511, 439}, {1067, 0, 1506}, {449, 960, 0}};
  std::vector<std::vector<int>> cost(kAsymmetricMatrixSize);
  for (int row = 0; row < kAsymmetricMatrixSize; ++row) {
    cost[row].resize(kAsymmetricMatrixSize);
    for (int col = 0; col < kAsymmetricMatrixSize; ++col) {
      cost[row][col] = AsymmetricMatrix[row][col];
    }
  }
  HamiltonianPathSolver<int, std::vector<std::vector<int>>> ham_solver(
      kAsymmetricMatrixSize, cost);
  EXPECT_TRUE(ham_solver.IsRobust());
  EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
  ComputeAndShow("Small asymmetric matrix", &ham_solver);
}

int Card(int set) {
  int c = 0;
  while (set != 0) {
    c += (set & 1);
    set >>= 1;
  }
  return c;
}

bool Contains(int set, int i) { return set & (1 << i); }

TEST(HamiltonianPathTest, AsymmetricMatrix) {
  typedef double TestType;
  const int kAsymmetricMatrixSize = 13;
  TestType AsymmetricMatrix[kAsymmetricMatrixSize][kAsymmetricMatrixSize] = {
      {0, 357, 511, 611, 667, 819, 1204, 1689, 1842, 2191, 940, 439, 895},
      {678, 0, 164, 264, 320, 472, 857, 1342, 1495, 1844, 730, 229, 685},
      {1067, 1424, 0, 100, 156, 308, 693, 1178, 1331, 1680, 1096, 1506, 857},
      {1263, 1620, 1774, 0, 56, 208, 593, 1078, 1231, 1580, 1531, 1702, 1272},
      {1207, 1564, 1718, 505, 0, 152, 537, 1022, 1175, 1524, 1475, 1646, 1216},
      {1728, 2085, 2239, 2339, 2395, 0, 385, 870, 1023, 1372, 1572, 2167, 1819},
      {1343, 1700, 1854, 1954, 2010, 2162, 0, 485, 638, 987, 1187, 1782, 1434},
      {858, 1215, 1369, 1469, 1525, 1677, 2062, 0, 153, 502, 702, 1297, 949},
      {705, 1062, 1216, 1316, 1372, 1524, 1909, 2394, 0, 349, 549, 1144, 796},
      {356, 713, 867, 967, 1023, 1175, 1560, 2045, 2198, 0, 200, 795, 447},
      {156, 513, 667, 767, 823, 975, 1360, 1845, 1998, 2347, 0, 595, 710},
      {449, 806, 960, 1060, 1116, 1268, 1653, 2138, 2291, 2452, 501, 0, 456},
      {210, 567, 721, 821, 877, 1029, 1414, 1899, 2052, 2401, 719, 649, 0}};

  // Iterate on all the subsets of the matrix and check that everything is
  // working OK.

  for (int subset = 0; subset < (1 << kAsymmetricMatrixSize); ++subset) {
    int sub_problem_size = Card(subset);
    if (sub_problem_size < 3) continue;
    std::vector<std::vector<TestType>> cost(sub_problem_size);
    int row = 0;
    for (int i = 0; i < kAsymmetricMatrixSize; ++i) {
      if (!Contains(subset, i)) continue;
      cost[row].resize(sub_problem_size);
      int col = 0;
      for (int j = 0; j < kAsymmetricMatrixSize; ++j) {
        if (!Contains(subset, j)) continue;
        cost[row][col] = AsymmetricMatrix[i][j];
        col++;
      }
      row++;
    }

    HamiltonianPathSolver<TestType, std::vector<std::vector<TestType>>>
        ham_solver(cost);
    EXPECT_TRUE(ham_solver.IsRobust());
    EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
    const int best_end_node = ham_solver.BestHamiltonianPathEndNode();
    std::vector<int> hamiltonian_path =
        ham_solver.HamiltonianPath(best_end_node);
    if (hamiltonian_path[0] != 0) {
      LOG(INFO) << "Sub-problem size : " << sub_problem_size
                << " subset : " << subset;
      ComputeAndShow("Asymmetric matrix", &ham_solver);
      for (int row = 0; row < cost.size(); ++row) {
        for (int col = 0; col < cost[row].size(); ++col) {
          absl::PrintF("%g ", cost[row][col]);
        }
        absl::PrintF("\n");
      }
    }
    CHECK_EQ(0, hamiltonian_path[0]);
  }
}

template <typename T>
class HamiltonianPathOverflowTest : public testing::Test {};

typedef testing::Types<int32_t, int64_t> HamiltonianPathOverflowTypes;
TYPED_TEST_SUITE(HamiltonianPathOverflowTest, HamiltonianPathOverflowTypes);

TYPED_TEST(HamiltonianPathOverflowTest, CostsWithOverflow) {
  const int kSize = 10;
  std::vector<std::vector<TypeParam>> cost(kSize);
  for (int i = 0; i < kSize; ++i) {
    cost[i].resize(kSize, i == 0 ? std::numeric_limits<TypeParam>::max() : 1);
  }
  HamiltonianPathSolver<TypeParam, std::vector<std::vector<TypeParam>>>
      ham_solver(kSize, cost);
  EXPECT_TRUE(ham_solver.IsRobust());
  EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
  ComputeAndShow("Overflow matrix", &ham_solver);
  EXPECT_EQ(std::numeric_limits<TypeParam>::max(),
            ham_solver.TravelingSalesmanCost());
  const int best_end_node = ham_solver.BestHamiltonianPathEndNode();
  EXPECT_EQ(std::numeric_limits<TypeParam>::max(),
            ham_solver.HamiltonianCost(best_end_node));
}

TYPED_TEST(HamiltonianPathOverflowTest, AllMaxCosts) {
  const int kSize = 10;
  std::vector<std::vector<TypeParam>> cost(kSize);
  for (int i = 0; i < kSize; ++i) {
    cost[i].resize(kSize, std::numeric_limits<TypeParam>::max());
  }
  HamiltonianPathSolver<TypeParam, std::vector<std::vector<TypeParam>>>
      ham_solver(kSize, cost);
  EXPECT_TRUE(ham_solver.IsRobust());
  EXPECT_TRUE(ham_solver.VerifiesTriangleInequality());
  ComputeAndShow("Overflow matrix", &ham_solver);
  EXPECT_EQ(std::numeric_limits<TypeParam>::max(),
            ham_solver.TravelingSalesmanCost());
  const int best_end_node = ham_solver.BestHamiltonianPathEndNode();
  EXPECT_EQ(std::numeric_limits<TypeParam>::max(),
            ham_solver.HamiltonianCost(best_end_node));
}
}  // namespace operations_research
