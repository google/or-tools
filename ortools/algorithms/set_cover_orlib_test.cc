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

#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/set_cover.h"
#include "ortools/algorithms/set_cover_reader.h"
#include "ortools/base/logging.h"
#include "ortools/base/path.h"

namespace operations_research {

void RunSolver(operations_research::SetCoverModel* model,
               double expected_cost) {
  operations_research::SetCoverLedger ledger(model);

  operations_research::GreedySolutionGenerator greedy(&ledger);
  CHECK(greedy.NextSolution());
  CHECK(ledger.CheckSolution());
  LOG(INFO) << "GreedySolutionGenerator cost: " << ledger.cost();

  operations_research::SteepestSearch steepest(&ledger);
  steepest.NextSolution(100000);
  LOG(INFO) << "SteepestSearch cost: " << ledger.cost();
  CHECK(ledger.CheckSolution());

  CHECK_EQ(ledger.cost(), expected_cost);

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

// This macro makes it possible to declare each test below with a one liner.
#define ORLIB_TEST(name, objective, size, function)                            \
  TEST(SetCoverTest, APPEND_AND_EVAL(TestOnLine, __LINE__)) {                  \
    auto filespec =                                                            \
        file::JoinPathRespectAbsolute(absl::GetFlag(FLAGS_test_srcdir),        \
                                      "operations_research_data/"              \
                                      "operations_research_data/SET_COVERING", \
                                      name);                                   \
    LOG(INFO) << "Reading " << name;                                           \
    operations_research::SetCoverModel model = function(filespec);             \
    RunSolver(&model, objective);                                              \
  }

#define RAIL_TEST(name, objective, size) \
  ORLIB_TEST(name, objective, size,      \
             operations_research::ReadRailSetCoverProblem)

#define SCP_TEST(name, objective, size) \
  ORLIB_TEST(name, objective, size,     \
             operations_research::ReadBeasleySetCoverProblem)

// Costs mentioned are the cost currently reached by the solver.
// TODO(user): add the best costs from the literature and compare.

RAIL_TEST("rail2536.txt", 889, MANYSECONDS);
RAIL_TEST("rail2586.txt", 1139, MANYSECONDS);
RAIL_TEST("rail4284.txt", 1362, MANYSECONDS);
RAIL_TEST("rail4872.txt", 1861, MANYSECONDS);
RAIL_TEST("rail507.txt", 218, FEWTENTHS);
RAIL_TEST("rail516.txt", 204, FEWTENTHS);
RAIL_TEST("rail582.txt", 250, FEWTENTHS);

SCP_TEST("scp41.txt", 442, FEWMILLIS);
SCP_TEST("scp42.txt", 555, FEWMILLIS);
SCP_TEST("scp43.txt", 557, FEWMILLIS);
SCP_TEST("scp44.txt", 516, FEWMILLIS);
SCP_TEST("scp45.txt", 530, FEWMILLIS);
SCP_TEST("scp46.txt", 594, FEWMILLIS);
SCP_TEST("scp47.txt", 451, FEWMILLIS);
SCP_TEST("scp48.txt", 502, FEWMILLIS);
SCP_TEST("scp49.txt", 693, FEWMILLIS);
SCP_TEST("scp410.txt", 525, FEWMILLIS);

SCP_TEST("scp51.txt", 274, FEWMILLIS);
SCP_TEST("scp52.txt", 329, FEWMILLIS);
SCP_TEST("scp53.txt", 233, FEWMILLIS);
SCP_TEST("scp54.txt", 255, FEWMILLIS);
SCP_TEST("scp55.txt", 222, FEWMILLIS);
SCP_TEST("scp56.txt", 234, FEWMILLIS);
SCP_TEST("scp57.txt", 313, FEWMILLIS);
SCP_TEST("scp58.txt", 309, FEWMILLIS);
SCP_TEST("scp59.txt", 292, FEWMILLIS);
SCP_TEST("scp510.txt", 276, FEWMILLIS);

SCP_TEST("scp61.txt", 151, FEWMILLIS);
SCP_TEST("scp62.txt", 173, FEWMILLIS);
SCP_TEST("scp63.txt", 154, FEWMILLIS);
SCP_TEST("scp64.txt", 137, FEWMILLIS);
SCP_TEST("scp65.txt", 181, FEWMILLIS);

SCP_TEST("scpa1.txt", 275, FEWHUNDREDTHS);
SCP_TEST("scpa2.txt", 268, FEWHUNDREDTHS);
SCP_TEST("scpa3.txt", 244, FEWHUNDREDTHS);
SCP_TEST("scpa4.txt", 253, FEWHUNDREDTHS);
SCP_TEST("scpa5.txt", 249, FEWHUNDREDTHS);

SCP_TEST("scpb1.txt", 74, FEWTENTHS);
SCP_TEST("scpb2.txt", 78, FEWTENTHS);
SCP_TEST("scpb3.txt", 85, FEWTENTHS);
SCP_TEST("scpb4.txt", 85, FEWTENTHS);
SCP_TEST("scpb5.txt", 77, FEWTENTHS);

SCP_TEST("scpc1.txt", 251, FEWHUNDREDTHS);
SCP_TEST("scpc2.txt", 238, FEWHUNDREDTHS);
SCP_TEST("scpc3.txt", 259, FEWHUNDREDTHS);
SCP_TEST("scpc4.txt", 246, FEWHUNDREDTHS);
SCP_TEST("scpc5.txt", 228, FEWHUNDREDTHS);

SCP_TEST("scpclr10.txt", 32, FEWMILLIS);
SCP_TEST("scpclr11.txt", 30, FEWMILLIS);
SCP_TEST("scpclr12.txt", 31, FEWMILLIS);
SCP_TEST("scpclr13.txt", 33, FEWMILLIS);

SCP_TEST("scpcyc06.txt", 60, FEWMILLIS);
SCP_TEST("scpcyc07.txt", 144, FEWMILLIS);
SCP_TEST("scpcyc08.txt", 360, FEWMILLIS);
SCP_TEST("scpcyc09.txt", 816, SUBHUNDREDTH);
SCP_TEST("scpcyc10.txt", 1920, FEWHUNDREDTHS);
SCP_TEST("scpcyc11.txt", 4284, SUBTENTH);

SCP_TEST("scpd1.txt", 68, FEWHUNDREDTHS);
SCP_TEST("scpd2.txt", 70, FEWHUNDREDTHS);
SCP_TEST("scpd3.txt", 78, FEWHUNDREDTHS);
SCP_TEST("scpd4.txt", 67, FEWHUNDREDTHS);
SCP_TEST("scpd5.txt", 72, FEWHUNDREDTHS);

SCP_TEST("scpe1.txt", 5, FEWMILLIS);
SCP_TEST("scpe2.txt", 6, FEWMILLIS);
SCP_TEST("scpe3.txt", 5, FEWMILLIS);
SCP_TEST("scpe4.txt", 6, FEWMILLIS);
SCP_TEST("scpe5.txt", 5, FEWMILLIS);

SCP_TEST("scpnre1.txt", 31, SUBTENTH);
SCP_TEST("scpnre2.txt", 34, SUBTENTH);
SCP_TEST("scpnre3.txt", 32, SUBTENTH);
SCP_TEST("scpnre4.txt", 32, SUBTENTH);
SCP_TEST("scpnre5.txt", 31, SUBTENTH);

SCP_TEST("scpnrf1.txt", 17, SUBTENTH);
SCP_TEST("scpnrf2.txt", 16, SUBTENTH);
SCP_TEST("scpnrf3.txt", 16, SUBTENTH);
SCP_TEST("scpnrf4.txt", 15, SUBTENTH);
SCP_TEST("scpnrf5.txt", 15, SUBTENTH);

SCP_TEST("scpnrg1.txt", 196, SUBTENTH);
SCP_TEST("scpnrg2.txt", 171, SUBTENTH);
SCP_TEST("scpnrg3.txt", 182, SUBTENTH);
SCP_TEST("scpnrg4.txt", 187, SUBTENTH);
SCP_TEST("scpnrg5.txt", 183, SUBTENTH);

SCP_TEST("scpnrh1.txt", 71, FEWTENTHS);
SCP_TEST("scpnrh2.txt", 70, FEWTENTHS);
SCP_TEST("scpnrh3.txt", 65, FEWTENTHS);
SCP_TEST("scpnrh4.txt", 66, FEWTENTHS);
SCP_TEST("scpnrh5.txt", 62, FEWTENTHS);

#undef APPEND
#undef APPEND_AND_EVAL
#undef SCP_TEST
#undef RAIL_TEST

}  // namespace operations_research
