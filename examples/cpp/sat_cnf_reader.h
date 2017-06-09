// Copyright 2010-2014 Google
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

#include <map>
#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/split.h"
#include "ortools/base/string_view.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/util/filelineiter.h"

DEFINE_bool(wcnf_use_strong_slack, true,
            "If true, when we add a slack variable to reify a soft clause, we "
            "enforce the fact that when it is true, the clause must be false.");

namespace operations_research {
namespace sat {

// This class loads a file in cnf file format into a SatProblem.
// The format is described here:
//    http://people.sc.fsu.edu/~jburkardt/data/cnf/cnf.html
//
// It also support the wcnf input format for partial weighted max-sat problems.
class SatCnfReader {
 public:
  SatCnfReader() : interpret_cnf_as_max_sat_(false) {}

  // If called with true, then a cnf file will be converted to the max-sat
  // problem: Try to minimize the number of unsatisfiable clauses.
  void InterpretCnfAsMaxSat(bool v) { interpret_cnf_as_max_sat_ = v; }

  // Loads the given cnf filename into the given problem.
  bool Load(const std::string& filename, LinearBooleanProblem* problem) {
    positive_literal_to_weight_.clear();
    objective_offset_ = 0;
    problem->Clear();
    problem->set_name(ExtractProblemName(filename));
    is_wcnf_ = false;
    end_marker_seen_ = false;
    hard_weight_ = 0;
    num_skipped_soft_clauses_ = 0;
    num_singleton_soft_clauses_ = 0;
    num_slack_variables_ = 0;
    num_slack_binary_clauses_ = 0;

    int num_lines = 0;
    for (const std::string& line : FileLines(filename)) {
      ++num_lines;
      ProcessNewLine(line, problem);
    }
    if (num_lines == 0) {
      LOG(FATAL) << "File '" << filename << "' is empty or can't be read.";
    }
    problem->set_original_num_variables(num_variables_);
    problem->set_num_variables(num_variables_ + num_slack_variables_);

    // Fill the LinearBooleanProblem objective.
    if (!positive_literal_to_weight_.empty()) {
      LinearObjective* objective = problem->mutable_objective();
      for (const std::pair<int, int64> p : positive_literal_to_weight_) {
        if (p.second != 0) {
          objective->add_literals(p.first);
          objective->add_coefficients(p.second);
        }
      }
      objective->set_offset(objective_offset_);
    }

    if (num_clauses_ + num_slack_binary_clauses_ !=
        problem->constraints_size() + num_singleton_soft_clauses_ +
            num_skipped_soft_clauses_) {
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

  int64 StringViewAtoi(const string_view& input) {
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
    if (words_.empty() || words_[0] == "c" || end_marker_seen_) return;
    if (words_[0] == "%") {
      end_marker_seen_ = true;
      return;
    }

    if (words_[0] == "p") {
      if (words_[1] == "cnf" || words_[1] == "wcnf") {
        num_variables_ = StringViewAtoi(words_[2]);
        num_clauses_ = StringViewAtoi(words_[3]);
        if (words_[1] == "wcnf") {
          is_wcnf_ = true;
          hard_weight_ = (words_.size() > 4) ? StringViewAtoi(words_[4]) : 0;
        }
      } else {
        // TODO(user): The ToString() is only required for the open source. Fix.
        LOG(FATAL) << "Unknown file type: " << words_[1];
      }
    } else {
      // In the cnf file format, the last words should always be 0.
      DCHECK_EQ("0", words_.back());
      const int size = words_.size() - 1;
      const int reserved_size =
          (!is_wcnf_ && interpret_cnf_as_max_sat_) ? size + 1 : size;

      LinearBooleanConstraint* constraint = problem->add_constraints();
      constraint->mutable_literals()->Reserve(reserved_size);
      constraint->mutable_coefficients()->Reserve(reserved_size);
      constraint->set_lower_bound(1);

      int64 weight =
          (!is_wcnf_ && interpret_cnf_as_max_sat_) ? 1 : hard_weight_;
      for (int i = 0; i < size; ++i) {
        const int64 signed_value = StringViewAtoi(words_[i]);
        if (i == 0 && is_wcnf_) {
          // Mathematically, a soft clause of weight 0 can be removed.
          if (signed_value == 0) {
            ++num_skipped_soft_clauses_;
            problem->mutable_constraints()->RemoveLast();
            break;
          }
          weight = signed_value;
        } else {
          DCHECK_NE(signed_value, 0);
          constraint->add_literals(signed_value);
          constraint->add_coefficients(1);
        }
      }
      if (weight != hard_weight_) {
        if (constraint->literals_size() == 1) {
          // The max-sat formulation of an optimization sat problem with a
          // linear objective introduces many singleton soft clauses. Because we
          // natively work with a linear objective, we can just put the cost on
          // the unique variable of such clause and remove the clause.
          ++num_singleton_soft_clauses_;
          const int literal = -constraint->literals(0);
          if (literal > 0) {
            positive_literal_to_weight_[literal] += weight;
          } else {
            positive_literal_to_weight_[-literal] -= weight;
            objective_offset_ += weight;
          }
          problem->mutable_constraints()->RemoveLast();
        } else {
          // The +1 is because a positive literal is the same as the 1-based
          // variable index.
          const int slack_literal = num_variables_ + num_slack_variables_ + 1;
          ++num_slack_variables_;
          constraint->add_literals(slack_literal);
          constraint->add_coefficients(1);
          DCHECK_EQ(constraint->literals_size(), reserved_size);

          if (slack_literal > 0) {
            positive_literal_to_weight_[slack_literal] += weight;
          } else {
            positive_literal_to_weight_[-slack_literal] -= weight;
            objective_offset_ += weight;
          }

          if (FLAGS_wcnf_use_strong_slack) {
            // Add the binary implications slack_literal true => all the other
            // clause literals are false.
            LinearBooleanConstraint base_constraint;
            base_constraint.set_lower_bound(1);
            base_constraint.add_coefficients(1);
            base_constraint.add_coefficients(1);
            base_constraint.add_literals(-slack_literal);
            base_constraint.add_literals(-slack_literal);
            for (int i = 0; i + 1 < constraint->literals_size(); ++i) {
              LinearBooleanConstraint* bc = problem->add_constraints();
              *bc = base_constraint;
              bc->mutable_literals()->Set(1, -constraint->literals(i));
              ++num_slack_binary_clauses_;
            }
          }
        }
      } else {
        // If wcnf is true, we currently reserve one more literals than needed
        // for the hard clauses.
        DCHECK_EQ(constraint->literals_size(), is_wcnf_ ? size - 1 : size);
      }
    }
  }

  bool interpret_cnf_as_max_sat_;

  int num_clauses_;
  int num_variables_;

  // Temporary storage for ProcessNewLine().
  std::vector<string_view> words_;

  // We stores the objective in a map because we want the variables to appear
  // only once in the LinearObjective proto.
  std::map<int, int64> positive_literal_to_weight_;
  int64 objective_offset_;

  // Used for the wcnf format.
  bool is_wcnf_;
  // Some files have text after %. This indicates if we have seen the '%'.
  bool end_marker_seen_;
  int64 hard_weight_;

  int num_slack_variables_;
  int num_skipped_soft_clauses_;
  int num_singleton_soft_clauses_;
  int num_slack_binary_clauses_;

  DISALLOW_COPY_AND_ASSIGN(SatCnfReader);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_CNF_READER_H_
