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

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip_testing.h"

namespace operations_research {
namespace {

using ::testing::HasSubstr;

// minimize 3.0 * x
// s.t. x in [-2.0, 4.0]
TEST(GScipIOTest, SearchLogsToFile) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->AddVariable(-2.0, 4.0, 3.0, GScipVarType::kContinuous, "x"));
  GScipParameters parameters;
  std::string log_file =
      file::JoinPath(::testing::TempDir(), "scip_search_log.txt");
  parameters.set_search_logs_filename(log_file);
  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  ASSERT_OK_AND_ASSIGN(const std::string logs,
                       file::GetContents(log_file, file::Defaults()));
  EXPECT_THAT(logs, HasSubstr("problem is solved [optimal solution found]"));
  EXPECT_THAT(logs, HasSubstr("-6.0"));
}

TEST(GScipIOTest, SCIPModelToFile) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->AddVariable(-2.0, 4.0, 3.0, GScipVarType::kContinuous, "x"));
  GScipParameters parameters = TestGScipParameters();
  std::string dump_model_file =
      file::JoinPath(::testing::TempDir(), "scip_model.txt");
  parameters.set_scip_model_filename(dump_model_file);
  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  ASSERT_OK_AND_ASSIGN(const std::string cip_file,
                       file::GetContents(dump_model_file, file::Defaults()));
  EXPECT_EQ(cip_file,
            R"(STATISTICS
  Problem name     : scip_test
  Variables        : 1 (0 binary, 0 integer, 0 implicit integer, 1 continuous)
  Constraints      : 0 initial, 0 maximal
OBJECTIVE
  Sense            : minimize
VARIABLES
  [continuous] <x>: obj=3, original bounds=[-2,4]
END
)");
}

// minimize 3.0 * x
// s.t. x in [-2.0, 4.0]
TEST(GScipIOTest, DetailedSolveStatsToFile) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->AddVariable(-2.0, 4.0, 3.0, GScipVarType::kContinuous, "x"));
  GScipParameters parameters = TestGScipParameters();
  std::string solve_stats_file =
      file::JoinPath(::testing::TempDir(), "scip_solve_stats.txt");
  parameters.set_detailed_solving_stats_filename(solve_stats_file);
  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  ASSERT_OK_AND_ASSIGN(const std::string logs,
                       file::GetContents(solve_stats_file, file::Defaults()));
  EXPECT_THAT(logs, HasSubstr("Presolvers"));
  EXPECT_THAT(logs, HasSubstr("boundshift"));
}

}  // namespace
}  // namespace operations_research
