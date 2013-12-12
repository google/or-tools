// Copyright 2010-2013 Google
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
#ifndef OR_TOOLS_SAT_SAT_CNF_READER_H_
#define OR_TOOLS_SAT_SAT_CNF_READER_H_

#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/strtoint.h"
#include "base/split.h"
#include "sat/boolean_problem.pb.h"
#include "util/filelineiter.h"

using std::string;

namespace operations_research {
namespace sat {

// This class loads a file in cnf file format into a SatProblem.
// The format is described here:
//    http://people.sc.fsu.edu/~jburkardt/data/cnf/cnf.html
//
// It also support the wcnf input format for partial weighted max-sat problems.
class SatCnfReader {
 public:
  SatCnfReader() {}

  // Loads the given cnf filename into the given problem.
  bool Load(const string& filename, LinearBooleanProblem* problem) {
    problem->Clear();
    problem->set_name(ExtractProblemName(filename));
    is_wcnf_ = false;
    slack_variable_weights_.clear();

    int num_lines = 0;
    for (const string& line : FileLines(filename)) {
      ++num_lines;
      ProcessNewLine(problem, line);
    }
    if (num_lines == 0) {
      LOG(FATAL) << "File '" << filename << "' is empty or can't be read.";
    }
    problem->set_num_variables(
        num_variables_ + slack_variable_weights_.size());

    // Add the slack variables (to convert max-sat to an pseudo-Boolean
    // optimization problem).
    if (is_wcnf_) {
      LinearObjective* objective = problem->mutable_objective();
      int slack_literal = num_variables_ + 1;
      for (const int weight : slack_variable_weights_) {
        objective->add_literals(slack_literal);
        objective->add_coefficients(weight);
        ++slack_literal;
      }
    }

    if (problem->constraints_size() != num_clauses_) {
      LOG(ERROR) << "Wrong number of clauses.";
      return false;
    }
    return true;
  }

 private:
  // Since the problem name is not stored in the cnf format, we infer it from
  // the file name.
  static string ExtractProblemName(const string& filename) {
    const int found = filename.find_last_of("/");
    const string problem_name = found != string::npos ?
        filename.substr(found + 1) : filename;
    return problem_name;
  }

  void ProcessNewLine(LinearBooleanProblem* problem, const string& line) {
    static const char kWordDelimiters[] = " ";
    std::vector<string> words;
    SplitStringUsing(line, kWordDelimiters, &words);
    if (words.size() == 0 || words[0] == "c") return;

    // TODO(user): It seems our files contains 2 lines of junk at the end.
    // Hence this test. Clear them?
    if (words[0] == "%") return;

    if (words[0] == "p") {
      if (words[1] == "cnf" || words[1] == "wcnf") {
        num_variables_ = atoi32(words[2]);
        num_clauses_ = atoi32(words[3]);
        if (words[1] == "wcnf") {
          is_wcnf_ = true;
          hard_weight_ = atoi64(words[4]);
          problem->set_type(LinearBooleanProblem::MINIMIZATION);
        } else {
          problem->set_type(LinearBooleanProblem::SATISFIABILITY);
        }
      } else {
        LOG(FATAL) << "Unknow file type: " << words[1];
      }
    } else {
      LinearBooleanConstraint* constraint = problem->add_constraints();
      constraint->set_lower_bound(1);
      for (int i = 0; i < words.size(); ++i) {
        int64 signed_value = atoi64(words[i]);
        if (i == 0 && is_wcnf_) {
          // Mathematically, a soft clause of weight 0 can be removed.
          if (signed_value == 0) break;
          if (signed_value != hard_weight_) {
            const int slack_literal =
                num_variables_ + slack_variable_weights_.size() + 1;
            constraint->add_literals(slack_literal);
            constraint->add_coefficients(1);
            slack_variable_weights_.push_back(signed_value);
          }
          continue;
        }
        if (signed_value == 0) break;
        constraint->add_literals(signed_value);
        constraint->add_coefficients(1);
      }
    }
  }

  int num_clauses_;
  int num_variables_;

  // Used for the wcnf format.
  bool is_wcnf_;
  std::vector<int64> slack_variable_weights_;
  int64 hard_weight_;

  DISALLOW_COPY_AND_ASSIGN(SatCnfReader);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_CNF_READER_H_
