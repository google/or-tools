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

#include <algorithm>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/set_cover.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_mip.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/algorithms/set_cover_reader.h"
#include "ortools/base/logging.h"
#include "ortools/base/path.h"
#include "ortools/base/timer.h"

namespace operations_research {

double RunSolver(std::string name, SetCoverModel* model) {
  SetCoverInvariant inv(model);

  GreedySolutionGenerator greedy(&inv);
  WallTimer global_timer;
  WallTimer timer;
  global_timer.Start();
  timer.Start();
  CHECK(greedy.NextSolution());
  DCHECK(inv.CheckSolution());
  LOG(INFO) << name << "_GreedySolutionGenerator_cost, " << inv.cost() << ", "
            << absl::ToInt64Microseconds(timer.GetDuration()) << ", us";
  timer.Stop();
  timer.Reset();
  timer.Start();
  operations_research::SteepestSearch steepest(&inv);
  steepest.NextSolution(100000);
  LOG(INFO) << name << "_SteepestSearch_cost, " << inv.cost() << ", "
            << absl::ToInt64Microseconds(timer.GetDuration()) << ", us";
  double best_cost = inv.cost();
  DCHECK(inv.CheckSolution());
  SubsetBoolVector best_choices = inv.is_selected();
  std::vector<SubsetIndex> focus = model->all_subsets();
  timer.Stop();
  timer.Reset();
  timer.Start();
  for (int i = 0; i < 10; ++i) {
    std::vector<SubsetIndex> range =
        ClearMostCoveredElements(std::min(100UL, focus.size()), &inv);
    SetCoverMip mip(&inv);
    mip.NextSolution(range);
    if (inv.cost() < best_cost) {
      best_cost = inv.cost();
      best_choices = inv.is_selected();
    }
  }
  timer.Stop();
  LOG(INFO) << name << "_MIP_cost, " << best_cost << ", "
            << absl::ToInt64Microseconds(timer.GetDuration()) << ", us";
  global_timer.Stop();
  LOG(INFO) << name << "_total_running_time, " << best_cost << ", "
            << absl::ToInt64Microseconds(global_timer.GetDuration())
            << ", us, total_time";
  return best_cost;
  // TODO(user): add guided local search.
}

// We break down the ORLIB set covering problems by their expected runtime with
// our solver (as of July 2023).
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

// These two macros provide indirection which allows the __LINE__ macro
// to be pasted, giving the tests useful names.
#define APPEND(x, y) x##y
#define APPEND_AND_EVAL(x, y) APPEND(x, y)

const char data_dir[] =
    "operations_research_data/operations_research_data/"
    "SET_COVERING";

// In the following, the lower bounds are taken from:
// [1] Caprara, Alberto, Matteo Fischetti, and Paolo Toth. 1999. “A Heuristic
// Method for the Set Covering Problem.” Operations Research 47 (5): 730–43.
// https://www.jstor.org/stable/223097 , and
// [2] Yagiura, Mutsunori, Masahiro Kishida, and Toshihide Ibaraki. 2006.
// “A 3-Flip Neighborhood Local Search for the Set Covering Problem.” European
// Journal of Operational Research 172 (2): 472–99.
// https://www.sciencedirect.com/science/article/pii/S0377221704008264

// This macro makes it possible to declare each test below with a one-liner.
// 'best_objective' denotes the best objective costs found in literature.
// These are the proven optimal values. This can be achieved with MIP.
// For the rail instances, they are the best solution found in the literature
// [1] and [2]. They are not achievable though local search or MIP or a
// combination of the two.
// 'expected_objective' are the costs currently reached by the solver.
// TODO(user): find and add values for the unit cost (aka unicost) case.

#define ORLIB_TEST(name, best_objective, expected_objective, size, function) \
  TEST(OrlibTest, APPEND_AND_EVAL(TestOnLine, __LINE__)) {                   \
    auto filespec =                                                          \
        file::JoinPathRespectAbsolute(::testing::SrcDir(), data_dir, name);  \
    LOG(INFO) << "Reading " << name;                                         \
    operations_research::SetCoverModel model = function(filespec);           \
    double cost = RunSolver(name, &model);                                   \
    (void)cost;                                                              \
  }

#define ORLIB_UNICOST_TEST(name, best_objective, expected_objective, size,  \
                           function)                                        \
  TEST(OrlibUnicostTest, APPEND_AND_EVAL(TestOnLine, __LINE__)) {           \
    auto filespec =                                                         \
        file::JoinPathRespectAbsolute(::testing::SrcDir(), data_dir, name); \
    LOG(INFO) << "Reading " << name;                                        \
    operations_research::SetCoverModel model = function(filespec);          \
    for (int i = 0; i < model.num_subsets(); ++i) {                         \
      model.SetSubsetCost(i, 1.0);                                          \
    }                                                                       \
    double cost = RunSolver(absl::StrCat(name, "_unicost"), &model);        \
    (void)cost;                                                             \
  }

#define SCP_TEST(name, best_objective, expected_objective, size)     \
  ORLIB_TEST(name, best_objective, expected_objective, size,         \
             operations_research::ReadBeasleySetCoverProblem)        \
  ORLIB_UNICOST_TEST(name, best_objective, expected_objective, size, \
                     operations_research::ReadBeasleySetCoverProblem)

#define RAIL_TEST(name, best_objective, expected_objective, size)    \
  ORLIB_TEST(name, best_objective, expected_objective, size,         \
             operations_research::ReadRailSetCoverProblem)           \
  ORLIB_UNICOST_TEST(name, best_objective, expected_objective, size, \
                     operations_research::ReadRailSetCoverProblem)

SCP_TEST("scp41.txt", 429, 442, FEWMILLIS);
SCP_TEST("scp42.txt", 512, 555, FEWMILLIS);
SCP_TEST("scp43.txt", 516, 557, FEWMILLIS);
SCP_TEST("scp44.txt", 494, 516, FEWMILLIS);
SCP_TEST("scp45.txt", 512, 530, FEWMILLIS);
SCP_TEST("scp46.txt", 560, 594, FEWMILLIS);
SCP_TEST("scp47.txt", 430, 451, FEWMILLIS);
SCP_TEST("scp48.txt", 492, 502, FEWMILLIS);
SCP_TEST("scp49.txt", 641, 693, FEWMILLIS);
SCP_TEST("scp410.txt", 514, 525, FEWMILLIS);

SCP_TEST("scp51.txt", 253, 274, FEWMILLIS);
SCP_TEST("scp52.txt", 302, 329, FEWMILLIS);
SCP_TEST("scp53.txt", 226, 233, FEWMILLIS);
SCP_TEST("scp54.txt", 242, 255, FEWMILLIS);
SCP_TEST("scp55.txt", 211, 222, FEWMILLIS);
SCP_TEST("scp56.txt", 213, 234, FEWMILLIS);
SCP_TEST("scp57.txt", 293, 313, FEWMILLIS);
SCP_TEST("scp58.txt", 288, 309, FEWMILLIS);
SCP_TEST("scp59.txt", 279, 292, FEWMILLIS);
SCP_TEST("scp510.txt", 265, 276, FEWMILLIS);

SCP_TEST("scp61.txt", 138, 151, FEWMILLIS);
SCP_TEST("scp62.txt", 146, 173, FEWMILLIS);
SCP_TEST("scp63.txt", 145, 154, FEWMILLIS);
SCP_TEST("scp64.txt", 131, 137, FEWMILLIS);
SCP_TEST("scp65.txt", 161, 181, FEWMILLIS);

SCP_TEST("scpa1.txt", 253, 275, FEWHUNDREDTHS);
SCP_TEST("scpa2.txt", 252, 268, FEWHUNDREDTHS);
SCP_TEST("scpa3.txt", 232, 244, FEWHUNDREDTHS);
SCP_TEST("scpa4.txt", 234, 253, FEWHUNDREDTHS);
SCP_TEST("scpa5.txt", 236, 249, FEWHUNDREDTHS);

SCP_TEST("scpb1.txt", 69, 74, FEWTENTHS);
SCP_TEST("scpb2.txt", 76, 78, FEWTENTHS);
SCP_TEST("scpb3.txt", 80, 85, FEWTENTHS);
SCP_TEST("scpb4.txt", 79, 85, FEWTENTHS);
SCP_TEST("scpb5.txt", 72, 77, FEWTENTHS);

SCP_TEST("scpc1.txt", 227, 251, FEWHUNDREDTHS);
SCP_TEST("scpc2.txt", 219, 238, FEWHUNDREDTHS);
SCP_TEST("scpc3.txt", 243, 259, FEWHUNDREDTHS);
SCP_TEST("scpc4.txt", 219, 246, FEWHUNDREDTHS);
SCP_TEST("scpc5.txt", 214, 228, FEWHUNDREDTHS);

SCP_TEST("scpd1.txt", 60, 68, FEWHUNDREDTHS);
SCP_TEST("scpd2.txt", 66, 70, FEWHUNDREDTHS);
SCP_TEST("scpd3.txt", 72, 78, FEWHUNDREDTHS);
SCP_TEST("scpd4.txt", 62, 67, FEWHUNDREDTHS);
SCP_TEST("scpd5.txt", 61, 72, FEWHUNDREDTHS);

SCP_TEST("scpe1.txt", 5, 5, FEWMILLIS);
SCP_TEST("scpe2.txt", 5, 6, FEWMILLIS);
SCP_TEST("scpe3.txt", 5, 5, FEWMILLIS);
SCP_TEST("scpe4.txt", 5, 6, FEWMILLIS);
SCP_TEST("scpe5.txt", 5, 5, FEWMILLIS);

SCP_TEST("scpnre1.txt", 29, 31, SUBTENTH);
SCP_TEST("scpnre2.txt", 30, 34, SUBTENTH);
SCP_TEST("scpnre3.txt", 27, 32, SUBTENTH);
SCP_TEST("scpnre4.txt", 28, 32, SUBTENTH);
SCP_TEST("scpnre5.txt", 28, 31, SUBTENTH);

SCP_TEST("scpnrf1.txt", 14, 17, SUBTENTH);
SCP_TEST("scpnrf2.txt", 15, 16, SUBTENTH);
SCP_TEST("scpnrf3.txt", 14, 16, SUBTENTH);
SCP_TEST("scpnrf4.txt", 14, 15, SUBTENTH);
SCP_TEST("scpnrf5.txt", 13, 15, SUBTENTH);

SCP_TEST("scpnrg1.txt", 176, 196, SUBTENTH);
SCP_TEST("scpnrg2.txt", 154, 171, SUBTENTH);
SCP_TEST("scpnrg3.txt", 166, 182, SUBTENTH);
SCP_TEST("scpnrg4.txt", 168, 187, SUBTENTH);
SCP_TEST("scpnrg5.txt", 168, 183, SUBTENTH);

SCP_TEST("scpnrh1.txt", 63, 71, FEWTENTHS);
SCP_TEST("scpnrh2.txt", 63, 70, FEWTENTHS);
SCP_TEST("scpnrh3.txt", 59, 65, FEWTENTHS);
SCP_TEST("scpnrh4.txt", 58, 66, FEWTENTHS);
SCP_TEST("scpnrh5.txt", 55, 62, FEWTENTHS);

RAIL_TEST("rail507.txt", 174, 218, FEWTENTHS);
RAIL_TEST("rail516.txt", 182, 204, FEWTENTHS);
RAIL_TEST("rail582.txt", 211, 250, FEWTENTHS);
RAIL_TEST("rail2536.txt", 691, 889, MANYSECONDS);
RAIL_TEST("rail2586.txt", 952, 1139, MANYSECONDS);
RAIL_TEST("rail4284.txt", 1065, 1362, MANYSECONDS);
RAIL_TEST("rail4872.txt", 1527, 1861, MANYSECONDS);  // [2]

SCP_TEST("scpclr10.txt", 0, 32, FEWMILLIS);
SCP_TEST("scpclr11.txt", 0, 30, FEWMILLIS);
SCP_TEST("scpclr12.txt", 0, 31, FEWMILLIS);
SCP_TEST("scpclr13.txt", 0, 33, FEWMILLIS);

SCP_TEST("scpcyc06.txt", 0, 60, FEWMILLIS);
SCP_TEST("scpcyc07.txt", 0, 144, FEWMILLIS);
SCP_TEST("scpcyc08.txt", 0, 360, FEWMILLIS);
SCP_TEST("scpcyc09.txt", 0, 816, SUBHUNDREDTH);
SCP_TEST("scpcyc10.txt", 0, 1920, FEWHUNDREDTHS);
SCP_TEST("scpcyc11.txt", 0, 4284, SUBTENTH);

#undef ORLIB_TEST
#undef ORLIB_UNICOST_TEST
#undef APPEND
#undef APPEND_AND_EVAL
#undef SCP_TEST
#undef RAIL_TEST

}  // namespace operations_research
