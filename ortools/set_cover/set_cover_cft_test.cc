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

#include "ortools/set_cover/set_cover_cft.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/path.h"
#include "ortools/base/timer.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_reader.h"

namespace operations_research {

using CL = SetCoverInvariant::ConsistencyLevel;

void LogStats(const SetCoverModel& model) {
  const std::string sep = ", ";
  const std::string name = absl::StrCat(sep, model.name(), sep);
  LOG(INFO) << name << model.ToVerboseString(sep);
  LOG(INFO) << name << "cost, "
            << model.ComputeCostStats().ToVerboseString(sep);
  LOG(INFO) << name << "row size stats, "
            << model.ComputeRowStats().ToVerboseString(sep);
  LOG(INFO) << name << "row size deciles, "
            << absl::StrJoin(model.ComputeRowDeciles(), sep);
  LOG(INFO) << name << "column size stats, "
            << model.ComputeColumnStats().ToVerboseString(sep);
  LOG(INFO) << name << "column size deciles, "
            << absl::StrJoin(model.ComputeColumnDeciles(), sep);
  LOG(INFO) << name << "num_singleton_rows, " << model.ComputeNumSingletonRows()
            << ", num_singleton_columns, "
            << model.ComputeNumSingletonColumns();
}

void LogCostAndTiming(const absl::string_view problem_name,
                      absl::string_view alg_name, SetCoverInvariant& inv,
                      int64_t run_time_in_microseconds) {
  LOG(INFO) << ", " << problem_name << ", " << alg_name << ", cost, "
            << inv.CostOrLowerBound() << ", solution_cardinality, "
            << inv.ComputeCardinality() << ", " << run_time_in_microseconds
            << "e-6, s";
}

struct SolverResult {
  double cost;
  double lower_bound;
};

SolverResult RunSolver(const absl::string_view name, SetCoverModel& model) {
  LogStats(model);
  WallTimer timer;
  timer.Start();

  SetCoverInvariant inv(&model);
  SetCoverCftOptimizer cft(&inv);
  cft.params().time_limit =
      absl::Seconds(std::min(60.0, 0.02 * model.num_elements()));
  EXPECT_TRUE(cft.Optimize());

  timer.Stop();
  LogCostAndTiming(name, "CftHeuristic", inv, timer.Get());
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));
  EXPECT_EQ(inv.num_uncovered_elements(), 0);
  return SolverResult{.cost = inv.cost(), .lower_bound = inv.LowerBound()};
}

enum ProblemSize {
  SUBMILLI,       // < 1ms
  FEWMILLIS,      // < 3ms
  SUBHUNDREDTH,   // < 10ms
  FEWHUNDREDTHS,  // < 30ms
  SUBTENTH,       // < 100ms
  FEWTENTHS,      // < 300ms
  SUBSECOND,      // < 1s
  FEWSECONDS,     // < 3s
  MANYSECONDS,    // >= 3s
  UNKNOWN = 999,  //  Not known (i.e. not benchmarked).
};

#define APPEND(x, y) x##y
#define APPEND_AND_EVAL(x, y) APPEND(x, y)

const char data_dir[] =
    "operations_research_data/operations_research_data/"
    "SET_COVERING";

void RunOrlibTestHelper(absl::string_view name, double best_objective,
                        double expected_objective,
                        SetCoverModel (*read_function)(absl::string_view),
                        bool make_unicost) {
  const std::string filename = absl::StrCat(name, ".txt");
  const auto filespec =
      file::JoinPathRespectAbsolute(::testing::SrcDir(), data_dir, filename);
  LOG(INFO) << "Reading " << filename;
  SetCoverModel model = read_function(filespec);
  const std::string problem_name =
      make_unicost ? absl::StrCat(name, "_unicost") : std::string(name);
  model.SetName(problem_name);
  if (make_unicost) {
    for (const SubsetIndex i : model.SubsetRange()) {
      model.SetSubsetCost(i, 1.0);
    }
  }
  const SolverResult result = RunSolver(problem_name, model);
  if (!make_unicost) {
    if (expected_objective > 0.0) {
      EXPECT_LE(result.cost, expected_objective);
    }
    if (best_objective > 0.0) {
      EXPECT_GE(result.lower_bound, 0.5 * best_objective);
    }
  }
}

#define ORLIB_TEST(name, best_objective, expected_objective, size, function) \
  TEST(CftOrlibTest, Test_##name) {                                          \
    RunOrlibTestHelper(#name, best_objective, expected_objective, function,  \
                       /*make_unicost=*/false);                              \
  }

#define ORLIB_UNICOST_TEST(name, best_objective, expected_objective, size,  \
                           function)                                        \
  TEST(CftOrlibUnicostTest, Test_##name##_unicost) {                        \
    RunOrlibTestHelper(#name, -1.0, -1.0, function, /*make_unicost=*/true); \
  }

#define SCP_TEST(name, best_objective, expected_objective, size)     \
  ORLIB_TEST(name, best_objective, expected_objective, size,         \
             operations_research::ReadOrlibScp)                      \
  ORLIB_UNICOST_TEST(name, best_objective, expected_objective, size, \
                     operations_research::ReadOrlibScp)

#define RAIL_TEST(name, best_objective, expected_objective, size)    \
  ORLIB_TEST(name, best_objective, expected_objective, size,         \
             operations_research::ReadOrlibRail)                     \
  ORLIB_UNICOST_TEST(name, best_objective, expected_objective, size, \
                     operations_research::ReadOrlibRail)

#define BASIC_SCP
#define EXTRA_SCP
#define RAIL

#ifdef BASIC_SCP
SCP_TEST(scp41, 429, 442, UNKNOWN);
SCP_TEST(scp42, 512, 555, UNKNOWN);
SCP_TEST(scp43, 516, 557, UNKNOWN);
SCP_TEST(scp44, 494, 516, UNKNOWN);
SCP_TEST(scp45, 512, 530, UNKNOWN);
SCP_TEST(scp46, 560, 594, UNKNOWN);
SCP_TEST(scp47, 430, 451, UNKNOWN);
SCP_TEST(scp48, 492, 502, UNKNOWN);
SCP_TEST(scp49, 641, 693, UNKNOWN);
SCP_TEST(scp410, 514, 525, UNKNOWN);

SCP_TEST(scp51, 253, 274, UNKNOWN);
SCP_TEST(scp52, 302, 329, UNKNOWN);
SCP_TEST(scp53, 226, 233, UNKNOWN);
SCP_TEST(scp54, 242, 255, UNKNOWN);
SCP_TEST(scp55, 211, 222, UNKNOWN);
SCP_TEST(scp56, 213, 234, UNKNOWN);
SCP_TEST(scp57, 293, 313, UNKNOWN);
SCP_TEST(scp58, 288, 309, UNKNOWN);
SCP_TEST(scp59, 279, 292, UNKNOWN);
SCP_TEST(scp510, 265, 276, UNKNOWN);

SCP_TEST(scp61, 138, 151, UNKNOWN);
SCP_TEST(scp62, 146, 173, UNKNOWN);
SCP_TEST(scp63, 145, 154, UNKNOWN);
SCP_TEST(scp64, 131, 137, UNKNOWN);
SCP_TEST(scp65, 161, 181, UNKNOWN);

SCP_TEST(scpa1, 253, 275, UNKNOWN);
SCP_TEST(scpa2, 252, 268, UNKNOWN);
SCP_TEST(scpa3, 232, 244, UNKNOWN);
SCP_TEST(scpa4, 234, 253, UNKNOWN);
SCP_TEST(scpa5, 236, 249, UNKNOWN);

SCP_TEST(scpb1, 69, 74, UNKNOWN);
SCP_TEST(scpb2, 76, 78, UNKNOWN);
SCP_TEST(scpb3, 80, 85, UNKNOWN);
SCP_TEST(scpb4, 79, 85, UNKNOWN);
SCP_TEST(scpb5, 72, 77, UNKNOWN);

SCP_TEST(scpc1, 227, 251, UNKNOWN);
SCP_TEST(scpc2, 219, 238, UNKNOWN);
SCP_TEST(scpc3, 243, 259, UNKNOWN);
SCP_TEST(scpc4, 219, 246, UNKNOWN);
SCP_TEST(scpc5, 214, 228, UNKNOWN);

SCP_TEST(scpd1, 60, 68, UNKNOWN);
SCP_TEST(scpd2, 66, 70, UNKNOWN);
SCP_TEST(scpd3, 72, 78, UNKNOWN);
SCP_TEST(scpd4, 62, 67, UNKNOWN);
SCP_TEST(scpd5, 61, 72, UNKNOWN);

SCP_TEST(scpe1, 5, 5, UNKNOWN);
SCP_TEST(scpe2, 5, 6, UNKNOWN);
SCP_TEST(scpe3, 5, 5, UNKNOWN);
SCP_TEST(scpe4, 5, 6, UNKNOWN);
SCP_TEST(scpe5, 5, 5, UNKNOWN);

SCP_TEST(scpnre1, 29, 31, UNKNOWN);
SCP_TEST(scpnre2, 30, 34, UNKNOWN);
SCP_TEST(scpnre3, 27, 32, UNKNOWN);
SCP_TEST(scpnre4, 28, 32, UNKNOWN);
SCP_TEST(scpnre5, 28, 31, UNKNOWN);

SCP_TEST(scpnrf1, 14, 17, UNKNOWN);
SCP_TEST(scpnrf2, 15, 16, UNKNOWN);
SCP_TEST(scpnrf3, 14, 16, UNKNOWN);
SCP_TEST(scpnrf4, 14, 15, UNKNOWN);
SCP_TEST(scpnrf5, 13, 15, UNKNOWN);

SCP_TEST(scpnrg1, 176, 196, UNKNOWN);
SCP_TEST(scpnrg2, 154, 171, UNKNOWN);
SCP_TEST(scpnrg3, 166, 182, UNKNOWN);
SCP_TEST(scpnrg4, 168, 187, UNKNOWN);
SCP_TEST(scpnrg5, 168, 183, UNKNOWN);

SCP_TEST(scpnrh1, 63, 71, UNKNOWN);
SCP_TEST(scpnrh2, 63, 70, UNKNOWN);
SCP_TEST(scpnrh3, 59, 65, UNKNOWN);
SCP_TEST(scpnrh4, 58, 66, UNKNOWN);
SCP_TEST(scpnrh5, 55, 62, UNKNOWN);
#endif

#ifdef EXTRA_SCP
SCP_TEST(scpclr10, 0, 32, UNKNOWN);
SCP_TEST(scpclr11, 0, 30, UNKNOWN);
SCP_TEST(scpclr12, 0, 31, UNKNOWN);
SCP_TEST(scpclr13, 0, 33, UNKNOWN);
SCP_TEST(scpcyc06, 0, 60, UNKNOWN);
SCP_TEST(scpcyc07, 0, 144, UNKNOWN);
SCP_TEST(scpcyc08, 0, 360, UNKNOWN);
SCP_TEST(scpcyc09, 0, 816, UNKNOWN);
SCP_TEST(scpcyc10, 0, 1920, UNKNOWN);
SCP_TEST(scpcyc11, 0, 4284, UNKNOWN);
#endif

#ifdef RAIL
RAIL_TEST(rail507, 174, 218, UNKNOWN);
RAIL_TEST(rail516, 182, 204, UNKNOWN);
RAIL_TEST(rail582, 211, 250, UNKNOWN);
RAIL_TEST(rail2536, 691, 889, UNKNOWN);
RAIL_TEST(rail2586, 952, 1139, UNKNOWN);
RAIL_TEST(rail4284, 1065, 1362, UNKNOWN);
RAIL_TEST(rail4872, 1527, 1861, UNKNOWN);
#endif

#undef ORLIB_TEST
#undef ORLIB_UNICOST_TEST
#undef APPEND
#undef APPEND_AND_EVAL
#undef SCP_TEST
#undef RAIL_TEST

TEST(SetCoverCftSolutionGeneratorTest, SolveVerySmallModel) {
  SetCoverModel model;
  model.AddEmptySubset(1);  // Subset 0
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(1);  // Subset 1
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);
  model.AddEmptySubset(1);  // Subset 2
  model.AddElementToLastSubset(1);
  model.AddEmptySubset(1);  // Subset 3
  model.AddElementToLastSubset(2);
  EXPECT_TRUE(model.ComputeFeasibility());

  SetCoverInvariant inv(&model);
  SetCoverCftOptimizer generator(&inv);
  EXPECT_TRUE(generator.Optimize());
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));
  EXPECT_EQ(inv.num_uncovered_elements(), 0);
}

}  // namespace operations_research
