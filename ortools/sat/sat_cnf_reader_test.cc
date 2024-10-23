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

#include "ortools/sat/sat_cnf_reader.h"

#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/boolean_problem.pb.h"

namespace operations_research {
namespace sat {
namespace {

std::string WriteTmpFileOrDie(absl::string_view content) {
  static int counter = 0;
  const std::string filename = file::JoinPath(
      ::testing::TempDir(), absl::StrCat("file_", counter++, ".cnf"));
  CHECK_OK(file::SetContents(filename, content, file::Defaults()));
  return filename;
}

TEST(SatCnfReader, CnfFormat) {
  std::string file_content =
      "p cnf 5 4\n"
      "+1 +2 +3 0\n"
      "-4 -5 0\n"
      "+1 0\n"
      "-1 0\n";
  SatCnfReader reader;
  LinearBooleanProblem problem;
  EXPECT_TRUE(reader.Load(WriteTmpFileOrDie(file_content), &problem));
  EXPECT_EQ(file_content, LinearBooleanProblemToCnfString(problem));
}

TEST(SatCnfReader, CnfFormatAsMaxSat) {
  const std::string file_content =
      "p cnf 5 4\n"
      "+1 +2 +3 0\n"
      "-4 -5 0\n"
      "+1 0\n"
      "-1 0\n";
  SatCnfReader reader(/*wcnf_use_strong_slack=*/false);
  reader.InterpretCnfAsMaxSat(true);
  LinearBooleanProblem problem;
  EXPECT_TRUE(reader.Load(WriteTmpFileOrDie(file_content), &problem));

  // Note that we currently loose the objective offset of 1 due to the two
  // clauses +1 and -1 in the original problem.
  const std::string wcnf_output =
      "p wcnf 5 2 3\n"
      "1 +1 +2 +3 0\n"
      "1 -4 -5 0\n";
  EXPECT_EQ(wcnf_output, LinearBooleanProblemToCnfString(problem));
}

TEST(SatCnfReader, CnfFormatCornerCases) {
  std::string file_content =
      "p cnf 5 4\n"
      "+1 +2 +3 0\n"
      "-4 -5 0\n"
      "+1 0\n"
      "-1 0\n";
  std::string file_content_with_comments =
      "c comments are ignored\n"
      "p cnf 5 4\n"
      "c + are not mandatory: \n"
      "+1 2 +3 0\n"
      "c and can be anywhere, with 0 0 0\n"
      "-4 -5 0\n"
      "c empty line are ignored\n"
      "\n\n\n    \n"
      "+1 0\n"
      "c same for spaces:\n"
      "    -1   0\n";
  SatCnfReader reader;
  LinearBooleanProblem problem;
  EXPECT_TRUE(
      reader.Load(WriteTmpFileOrDie(file_content_with_comments), &problem));
  EXPECT_EQ(file_content, LinearBooleanProblemToCnfString(problem));
}

TEST(SatCnfReader, ClausesNumberMustMatch) {
  std::string file_content =
      "p cnf 5 4\n"
      "+1 +2 +3 0\n"
      "-4 -5 0\n"
      "+1 0\n"
      "0\n"
      "-1 0\n";
  SatCnfReader reader;
  LinearBooleanProblem problem;

  // Note that we changed that requirement since now we dynamically infer sizes.
  // We just log errors.
  EXPECT_TRUE(reader.Load(WriteTmpFileOrDie(file_content), &problem));
}

TEST(SatCnfReader, WcnfFormat) {
  // Note that the input format is such that it is the same as the one produced
  // by LinearBooleanProblemToCnfString().
  //
  // The special hard weight "109" is by convention the sum of all the soft
  // weight + 1. It means that not satisfying an hard clause is worse than
  // satisfying none of the soft clause. Note that this is just a "convention",
  // in the way we interpret it, it doesn't really matter and must just be a
  // different number than any of the soft weight.
  std::string file_content =
      "p wcnf 5 7 109\n"
      "1 +1 +2 +3 0\n"
      "2 -4 -5 0\n"
      "109 -1 0\n"
      "109 +1 0\n"
      "99 +1 0\n"
      "3 +4 0\n"
      "3 +5 0\n";
  SatCnfReader reader(/*wcnf_use_strong_slack=*/false);
  LinearBooleanProblem problem;
  EXPECT_TRUE(reader.Load(WriteTmpFileOrDie(file_content), &problem));
  EXPECT_EQ(4, problem.constraints_size());
  EXPECT_EQ(file_content, LinearBooleanProblemToCnfString(problem));
}

TEST(SatCnfReader, WcnfNewFormat) {
  const std::string new_format =
      "1 +1 +2 +3 0\n"
      "2 -4 -5 0\n"
      "h -1 0\n"
      "h +1 0\n"
      "99 +1 0\n"
      "3 +4 0\n"
      "3 +5 0\n";
  const std::string file_content =
      "p wcnf 5 7 109\n"
      "1 +1 +2 +3 0\n"
      "2 -4 -5 0\n"
      "109 -1 0\n"
      "109 +1 0\n"
      "99 +1 0\n"
      "3 +4 0\n"
      "3 +5 0\n";
  SatCnfReader reader(/*wcnf_use_strong_slack=*/false);
  LinearBooleanProblem problem;
  EXPECT_TRUE(reader.Load(WriteTmpFileOrDie(new_format), &problem));
  EXPECT_EQ(4, problem.constraints_size());
  EXPECT_EQ(file_content, LinearBooleanProblemToCnfString(problem));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
