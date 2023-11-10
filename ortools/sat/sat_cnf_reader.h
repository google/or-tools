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

#ifndef OR_TOOLS_SAT_SAT_CNF_READER_H_
#define OR_TOOLS_SAT_SAT_CNF_READER_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {
namespace sat {

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

// This implement the implicit contract needed by the SatCnfReader class.
class CpModelProtoWrapper {
 public:
  explicit CpModelProtoWrapper(CpModelProto* p) : problem_(p) {}

  void SetSizeAndPostprocess(int num_variables, int num_slacks) {
    for (int i = 0; i < num_variables + num_slacks; ++i) {
      IntegerVariableProto* variable = problem_->add_variables();
      variable->add_domain(0);
      variable->add_domain(1);
    }
    for (const int c : to_postprocess_) {
      auto* literals = problem_->mutable_constraints(c)
                           ->mutable_bool_or()
                           ->mutable_literals();
      const int last_index = literals->size() - 1;
      const int last = (*literals)[last_index];
      (*literals)[last_index] =
          last >= 0 ? last + num_variables : last - num_variables;
    }
  }

  int LiteralToRef(int signed_value) {
    return signed_value > 0 ? signed_value - 1 : signed_value;
  }

  void AddConstraint(absl::Span<const int> clause, bool last_is_slack = false) {
    if (last_is_slack)
      to_postprocess_.push_back(problem_->constraints().size());
    auto* constraint = problem_->add_constraints()->mutable_bool_or();
    constraint->mutable_literals()->Reserve(clause.size());
    for (const int literal : clause) {
      constraint->add_literals(LiteralToRef(literal));
    }
  }

  void AddObjectiveTerm(int literal, int64_t value) {
    CHECK_GE(literal, 0) << "Negative literal not supported.";
    problem_->mutable_objective()->add_vars(LiteralToRef(literal));
    problem_->mutable_objective()->add_coeffs(value);
  }

  void SetObjectiveOffset(int64_t offset) {
    problem_->mutable_objective()->set_offset(offset);
  }

 private:
  CpModelProto* problem_;
  std::vector<int> to_postprocess_;
};

// This class loads a file in cnf file format into a SatProblem.
// The format is described here:
//    http://people.sc.fsu.edu/~jburkardt/data/cnf/cnf.html
//
// It also support the wcnf input format for partial weighted max-sat problems.
class SatCnfReader {
 public:
  explicit SatCnfReader(bool wcnf_use_strong_slack = true)
      : interpret_cnf_as_max_sat_(false),
        wcnf_use_strong_slack_(wcnf_use_strong_slack) {}

  // This type is neither copyable nor movable.
  SatCnfReader(const SatCnfReader&) = delete;
  SatCnfReader& operator=(const SatCnfReader&) = delete;

  // If called with true, then a cnf file will be converted to the max-sat
  // problem: Try to minimize the number of unsatisfiable clauses.
  void InterpretCnfAsMaxSat(bool v) { interpret_cnf_as_max_sat_ = v; }

  // Loads the given cnf filename into the given proto.
  bool Load(const std::string& filename, LinearBooleanProblem* problem) {
    problem->Clear();
    problem->set_name(ExtractProblemName(filename));
    LinearBooleanProblemWrapper wrapper(problem);
    return LoadInternal(filename, &wrapper);
  }
  bool Load(const std::string& filename, CpModelProto* problem) {
    problem->Clear();
    problem->set_name(ExtractProblemName(filename));
    CpModelProtoWrapper wrapper(problem);
    return LoadInternal(filename, &wrapper);
  }

 private:
  template <class Problem>
  bool LoadInternal(const std::string& filename, Problem* problem) {
    is_wcnf_ = false;
    objective_offset_ = 0;
    positive_literal_to_weight_.clear();

    end_marker_seen_ = false;
    hard_weight_ = 0;
    num_skipped_soft_clauses_ = 0;
    num_singleton_soft_clauses_ = 0;
    num_added_clauses_ = 0;
    num_slack_variables_ = 0;

    num_variables_ = 0;
    num_clauses_ = 0;
    actual_num_variables_ = 0;

    int num_lines = 0;
    for (const std::string& line : FileLines(filename)) {
      ++num_lines;
      ProcessNewLine(line, problem);
    }
    if (num_lines == 0) {
      LOG(FATAL) << "File '" << filename << "' is empty or can't be read.";
    }

    if (num_variables_ > 0 && num_variables_ != actual_num_variables_) {
      LOG(ERROR) << "Wrong number of variables ! Expected:" << num_variables_
                 << " Seen:" << actual_num_variables_;
    }

    problem->SetSizeAndPostprocess(actual_num_variables_, num_slack_variables_);

    // Fill the objective.
    if (!positive_literal_to_weight_.empty() ||
        !slack_literal_to_weight_.empty()) {
      for (const std::pair<int, int64_t> p : positive_literal_to_weight_) {
        if (p.second != 0) {
          problem->AddObjectiveTerm(p.first, p.second);
        }
      }
      for (const std::pair<int, int64_t> p : slack_literal_to_weight_) {
        if (p.second != 0) {
          problem->AddObjectiveTerm(actual_num_variables_ + p.first, p.second);
        }
      }
      problem->SetObjectiveOffset(objective_offset_);
    }

    // Some file from the max-sat competition seems to have the wrong number of
    // clause !? I checked manually, so still parse them with best effort.
    const int total_seen = num_added_clauses_ + num_singleton_soft_clauses_ +
                           num_skipped_soft_clauses_;
    if (num_clauses_ > 0 && num_clauses_ != total_seen) {
      LOG(ERROR) << "Wrong number of clauses ! Expected:" << num_clauses_
                 << " Seen:" << total_seen;
    }
    return true;
  }

  // Since the problem name is not stored in the cnf format, we infer it from
  // the file name.
  static std::string ExtractProblemName(const std::string& filename) {
    const int found = filename.find_last_of('/');
    const std::string problem_name =
        found != std::string::npos ? filename.substr(found + 1) : filename;
    return problem_name;
  }

  void ProcessHeader(const std::string& line) {
    static const char kWordDelimiters[] = " ";
    words_ = absl::StrSplit(line, kWordDelimiters, absl::SkipEmpty());

    CHECK_EQ(words_[0], "p");
    if (words_[1] == "cnf" || words_[1] == "wcnf") {
      CHECK(absl::SimpleAtoi(words_[2], &num_variables_));
      CHECK(absl::SimpleAtoi(words_[3], &num_clauses_));
      if (words_[1] == "wcnf") {
        is_wcnf_ = true;
        hard_weight_ = 0;
        if (words_.size() > 4) {
          CHECK(absl::SimpleAtoi(words_[4], &hard_weight_));
        }
      }
    } else {
      LOG(FATAL) << "Unknown file type: " << words_[1];
    }
  }

  template <class Problem>
  void ProcessNewLine(const std::string& line, Problem* problem) {
    if (line.empty() || end_marker_seen_) return;
    if (line[0] == 'c') return;
    if (line[0] == '%') {
      end_marker_seen_ = true;
      return;
    }
    if (line[0] == 'p') {
      ProcessHeader(line);
      return;
    }

    // The new wcnf format do not have header p line anymore.
    if (num_variables_ == 0) {
      is_wcnf_ = true;
    }

    static const char kWordDelimiters[] = " ";
    auto splitter = absl::StrSplit(line, kWordDelimiters, absl::SkipEmpty());

    tmp_clause_.clear();
    int64_t weight =
        (!is_wcnf_ && interpret_cnf_as_max_sat_) ? 1 : hard_weight_;
    bool first = true;
    bool end_marker_seen = false;
    for (const absl::string_view word : splitter) {
      if (first && is_wcnf_) {
        first = false;
        if (word == "h") {
          // Hard clause in the new 2022 format.
          // Note that hard_weight_ == 0 here.
          weight = hard_weight_;
        } else {
          CHECK(absl::SimpleAtoi(word, &weight));
          CHECK_GE(weight, 0);

          // A soft clause of weight 0 can be removed.
          if (weight == 0) {
            ++num_skipped_soft_clauses_;
            return;
          }
        }
        continue;
      }

      int signed_value;
      CHECK(absl::SimpleAtoi(word, &signed_value));
      if (signed_value == 0) {
        end_marker_seen = true;
        break;  // end of clause.
      }

      actual_num_variables_ = std::max(actual_num_variables_,
                                       std::max(signed_value, -signed_value));
      tmp_clause_.push_back(signed_value);
    }
    if (!end_marker_seen) return;

    if (weight == hard_weight_) {
      ++num_added_clauses_;
      problem->AddConstraint(tmp_clause_);
    } else {
      if (tmp_clause_.size() == 1) {
        // The max-sat formulation of an optimization sat problem with a
        // linear objective introduces many singleton soft clauses. Because we
        // natively work with a linear objective, we can just add the cost to
        // the unique variable of such clause and remove the clause.
        ++num_singleton_soft_clauses_;
        const int literal = -tmp_clause_[0];
        if (literal > 0) {
          positive_literal_to_weight_[literal] += weight;
        } else {
          positive_literal_to_weight_[-literal] -= weight;
          objective_offset_ += weight;
        }
      } else {
        // The +1 is because a positive literal is the same as the 1-based
        // variable index.
        const int slack_literal = ++num_slack_variables_;

        slack_literal_to_weight_[slack_literal] += weight;
        tmp_clause_.push_back(slack_literal);

        ++num_added_clauses_;
        problem->AddConstraint(tmp_clause_, /*last_is_slack=*/true);

        if (wcnf_use_strong_slack_) {
          // Add the binary implications slack_literal true => all the other
          // clause literals are false.
          for (int i = 0; i + 1 < tmp_clause_.size(); ++i) {
            problem->AddConstraint({-tmp_clause_[i], -slack_literal},
                                   /*last_is_slack=*/true);
          }
        }
      }
    }
  }

  bool interpret_cnf_as_max_sat_;
  const bool wcnf_use_strong_slack_;

  int num_clauses_ = 0;
  int num_variables_ = 0;
  int actual_num_variables_ = 0;

  // Temporary storage for ProcessNewLine().
  std::vector<absl::string_view> words_;

  // We stores the objective in a map because we want the variables to appear
  // only once in the LinearObjective proto.
  int64_t objective_offset_;
  absl::btree_map<int, int64_t> positive_literal_to_weight_;
  absl::btree_map<int, int64_t> slack_literal_to_weight_;

  // Used for the wcnf format.
  bool is_wcnf_;
  // Some files have text after %. This indicates if we have seen the '%'.
  bool end_marker_seen_;
  int64_t hard_weight_ = 0;

  int num_slack_variables_;
  int num_skipped_soft_clauses_;
  int num_singleton_soft_clauses_;
  int num_added_clauses_;

  std::vector<int> tmp_clause_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_CNF_READER_H_
