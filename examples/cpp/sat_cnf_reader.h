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
  bool Load(const std::string& filename, LinearBooleanProblem* problem) {
    problem->Clear();
    problem->set_name(ExtractProblemName(filename));
    is_wcnf_ = false;
    end_marker_seen_ = false;
    slack_variable_weights_.clear();

    int num_lines = 0;
    for (const std::string& line : FileLines(filename)) {
      ++num_lines;
      ProcessNewLine(line, problem);
    }
    if (num_lines == 0) {
      LOG(FATAL) << "File '" << filename << "' is empty or can't be read.";
    }
    problem->set_original_num_variables(num_variables_);
    problem->set_num_variables(num_variables_ + slack_variable_weights_.size());

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
  static std::string ExtractProblemName(const std::string& filename) {
    const int found = filename.find_last_of("/");
    const std::string problem_name =
        found != std::string::npos ? filename.substr(found + 1) : filename;
    return problem_name;
  }

  int64 StringPieceAtoi(StringPiece input) {
    // Hack: data() is not null terminated, but we do know that it points
    // inside a std::string where numbers are separated by " " and since atoi64 will
    // stop at the first invalid char, this works.
    return atoi64(input.data());
  }

  void ProcessNewLine(const std::string& line, LinearBooleanProblem* problem) {
    static const char kWordDelimiters[] = " ";
    words_ = strings::Split(
        line, kWordDelimiters,
        static_cast<int64>(strings::SkipEmpty()));
    if (words_.size() == 0 || words_[0] == "c" || end_marker_seen_) return;
    if (words_[0] == "%") {
      end_marker_seen_ = true;
      return;
    }

    if (words_[0] == "p") {
      if (words_[1] == "cnf" || words_[1] == "wcnf") {
        num_variables_ = StringPieceAtoi(words_[2]);
        num_clauses_ = StringPieceAtoi(words_[3]);
        if (words_[1] == "wcnf") {
          is_wcnf_ = true;
          hard_weight_ = (words_.size() > 4) ? StringPieceAtoi(words_[4]) : 0;
          problem->set_type(LinearBooleanProblem::MINIMIZATION);
        } else {
          problem->set_type(LinearBooleanProblem::SATISFIABILITY);
        }
      } else {
        LOG(FATAL) << "Unknow file type: " << words_[1];
      }
    } else {
      // In the cnf file format, the last words should always be 0.
      DCHECK_EQ("0", words_.back());
      const int size = words_.size() - 1;

      LinearBooleanConstraint* constraint = problem->add_constraints();
      constraint->mutable_literals()->Reserve(size);
      constraint->mutable_coefficients()->Reserve(size);
      constraint->set_lower_bound(1);

      for (int i = 0; i < size; ++i) {
        const int64 signed_value = StringPieceAtoi(words_[i]);
        if (i == 0 && is_wcnf_) {
          // Mathematically, a soft clause of weight 0 can be removed.
          if (signed_value == 0) {
            problem->mutable_constraints()->RemoveLast();
            break;
          }
          if (signed_value != hard_weight_) {
            const int slack_literal =
                num_variables_ + slack_variable_weights_.size() + 1;
            constraint->add_literals(slack_literal);
            constraint->add_coefficients(1);
            slack_variable_weights_.push_back(signed_value);
          }
          continue;
        }
        DCHECK_NE(signed_value, 0);
        constraint->add_literals(signed_value);
        constraint->add_coefficients(1);
      }
      if (DEBUG_MODE && !is_wcnf_) {
        // If wcnf is true, we currently reserve one more literals than needed
        // for the hard clauses.
        DCHECK_EQ(constraint->literals_size(), size);
      }
    }
  }

  int num_clauses_;
  int num_variables_;

  // Temporary storage for ProcessNewLine().
  std::vector<StringPiece> words_;

  // Used for the wcnf format.
  bool is_wcnf_;
  // Some files have text after %. This indicates if we have seen the '%'.
  bool end_marker_seen_;
  std::vector<int64> slack_variable_weights_;
  int64 hard_weight_;

  DISALLOW_COPY_AND_ASSIGN(SatCnfReader);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_CNF_READER_H_
