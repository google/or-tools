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

#include "ortools/sat/sat_cnf_reader.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/bop/boolean_problem.h"
#include "ortools/bop/boolean_problem.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::operations_research::bop::LinearBooleanConstraint;
using ::operations_research::bop::LinearBooleanProblem;

// This implement the implicit contract needed by the SatCnfReader class.
class LinearBooleanProblemWrapper {
 public:
  explicit LinearBooleanProblemWrapper(LinearBooleanProblem* p) : problem_(p) {}

  // In the new 2022 .wcnf format, we don't know the number of variable before
  // hand (no header). So when this is called (after all the constraint have
  // been added), we need to re-index the slack so that they are after the
  // variable of the original problem.
  void SetSizeAndPostprocess(int num_variables, int num_slacks) {
    problem_->set_num_variables(num_variables + num_slacks);
    problem_->set_original_num_variables(num_variables);
    for (const int c : to_postprocess_) {
      auto* literals = problem_->mutable_constraints(c)->mutable_literals();
      const int last_index = literals->size() - 1;
      const int last = (*literals)[last_index];
      (*literals)[last_index] =
          last >= 0 ? last + num_variables : last - num_variables;
    }
  }

  // If last_is_slack is true, then the last literal is assumed to be a slack
  // with index in [-num_slacks, num_slacks]. We will re-index it at the end in
  // SetSizeAndPostprocess().
  void AddConstraint(absl::Span<const int> clause, bool last_is_slack = false) {
    if (last_is_slack)
      to_postprocess_.push_back(problem_->constraints().size());
    LinearBooleanConstraint* constraint = problem_->add_constraints();
    constraint->mutable_literals()->Reserve(clause.size());
    constraint->mutable_coefficients()->Reserve(clause.size());
    constraint->set_lower_bound(1);
    for (const int literal : clause) {
      constraint->add_literals(literal);
      constraint->add_coefficients(1);
    }
  }

  void AddObjectiveTerm(int literal, int64_t value) {
    CHECK_GE(literal, 0) << "Negative literal not supported.";
    problem_->mutable_objective()->add_literals(literal);
    problem_->mutable_objective()->add_coefficients(value);
  }

  void SetObjectiveOffset(int64_t offset) {
    problem_->mutable_objective()->set_offset(offset);
  }

 private:
  LinearBooleanProblem* problem_;
  std::vector<int> to_postprocess_;
};

std::string WriteTmpFileOrDie(absl::string_view content) {
  static int counter = 0;
  const std::string filename = file::JoinPath(
      ::testing::TempDir(), absl::StrCat("file_", counter++, ".cnf"));
  CHECK_OK(file::SetContents(filename, content, file::Defaults()));
  return filename;
}

LinearBooleanProblem LoadCnfFile(SatCnfReader& reader,
                                 absl::string_view file_content) {
  LinearBooleanProblem problem;
  LinearBooleanProblemWrapper wrapper(&problem);
  problem.Clear();
  const std::string filename = WriteTmpFileOrDie(file_content);
  problem.set_name(SatCnfReader::ExtractProblemName(filename));
  CHECK(reader.LoadInternal<LinearBooleanProblemWrapper>(filename, &wrapper));
  return problem;
}

TEST(SatCnfReader, CnfFormat) {
  std::string file_content =
      "p cnf 5 4\n"
      "+1 +2 +3 0\n"
      "-4 -5 0\n"
      "+1 0\n"
      "-1 0\n";
  SatCnfReader reader;
  LinearBooleanProblem problem = LoadCnfFile(reader, file_content);
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
  LinearBooleanProblem problem = LoadCnfFile(reader, file_content);

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
  LinearBooleanProblem problem = LoadCnfFile(reader, file_content);
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

  // Note that we changed that requirement since now we dynamically infer sizes.
  // We just log errors.
  LinearBooleanProblem problem = LoadCnfFile(reader, file_content);
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
  LinearBooleanProblem problem = LoadCnfFile(reader, file_content);
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
  LinearBooleanProblem problem = LoadCnfFile(reader, file_content);
  EXPECT_EQ(4, problem.constraints_size());
  EXPECT_EQ(file_content, LinearBooleanProblemToCnfString(problem));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
