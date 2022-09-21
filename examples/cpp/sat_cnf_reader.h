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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/flags/flag.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/filelineiter.h"

ABSL_FLAG(bool, wcnf_use_strong_slack, true,
          "If true, when we add a slack variable to reify a soft clause, we "
          "enforce the fact that when it is true, the clause must be false.");

namespace operations_research {
namespace sat {

struct LinearBooleanProblemWrapper {
  explicit LinearBooleanProblemWrapper(LinearBooleanProblem* p) : problem(p) {}

  void SetNumVariables(int num) { problem->set_num_variables(num); }
  void SetOriginalNumVariables(int num) {
    problem->set_original_num_variables(num);
  }

  void AddConstraint(absl::Span<const int> clause) {
    LinearBooleanConstraint* constraint = problem->add_constraints();
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
    problem->mutable_objective()->add_literals(literal);
    problem->mutable_objective()->add_coefficients(value);
  }

  void SetObjectiveOffset(int64_t offset) {
    problem->mutable_objective()->set_offset(offset);
  }

  LinearBooleanProblem* problem;
};

struct CpModelProtoWrapper {
  explicit CpModelProtoWrapper(CpModelProto* p) : problem(p) {}

  void SetNumVariables(int num) {
    for (int i = 0; i < num; ++i) {
      IntegerVariableProto* variable = problem->add_variables();
      variable->add_domain(0);
      variable->add_domain(1);
    }
  }

  // TODO(user): Not supported. This is only used for displaying a wcnf
  // solution in cnf format, so it is not useful internally. Instead of adding
  // another field, we could use the variables names or the search heuristics
  // to encode this info.
  void SetOriginalNumVariables(int num) {}

  int LiteralToRef(int signed_value) {
    return signed_value > 0 ? signed_value - 1 : signed_value;
  }

  void AddConstraint(absl::Span<const int> clause) {
    auto* constraint = problem->add_constraints()->mutable_bool_or();
    constraint->mutable_literals()->Reserve(clause.size());
    for (const int literal : clause) {
      constraint->add_literals(LiteralToRef(literal));
    }
  }

  void AddObjectiveTerm(int literal, int64_t value) {
    CHECK_GE(literal, 0) << "Negative literal not supported.";
    problem->mutable_objective()->add_vars(LiteralToRef(literal));
    problem->mutable_objective()->add_coeffs(value);
  }

  void SetObjectiveOffset(int64_t offset) {
    problem->mutable_objective()->set_offset(offset);
  }

  CpModelProto* problem;
};

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
    positive_literal_to_weight_.clear();
    objective_offset_ = 0;
    is_wcnf_ = false;
    end_marker_seen_ = false;
    hard_weight_ = 0;
    num_skipped_soft_clauses_ = 0;
    num_singleton_soft_clauses_ = 0;
    num_added_clauses_ = 0;
    num_slack_variables_ = 0;

    int num_lines = 0;
    for (const std::string& line : FileLines(filename)) {
      ++num_lines;
      ProcessNewLine(line, problem);
    }
    if (num_lines == 0) {
      LOG(FATAL) << "File '" << filename << "' is empty or can't be read.";
    }
    problem->SetOriginalNumVariables(num_variables_);
    problem->SetNumVariables(num_variables_ + num_slack_variables_);

    // Fill the objective.
    if (!positive_literal_to_weight_.empty()) {
      for (const std::pair<int, int64_t> p : positive_literal_to_weight_) {
        if (p.second != 0) {
          problem->AddObjectiveTerm(p.first, p.second);
        }
      }
      problem->SetObjectiveOffset(objective_offset_);
    }

    if (num_clauses_ != num_added_clauses_ + num_singleton_soft_clauses_ +
                            num_skipped_soft_clauses_) {
      LOG(ERROR) << "Wrong number of clauses. " << num_clauses_ << " "
                 << num_added_clauses_;
      return false;
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

  int64_t StringPieceAtoi(absl::string_view input) {
    int64_t value;
    // Hack: data() is not null terminated, but we do know that it points
    // inside a string where numbers are separated by " " and since SimpleAtoi
    // will stop at the first invalid char, this works.
    CHECK(absl::SimpleAtoi(input, &value));
    return value;
  }

  void ProcessHeader(const std::string& line) {
    static const char kWordDelimiters[] = " ";
    words_ = absl::StrSplit(line, kWordDelimiters, absl::SkipEmpty());

    CHECK_EQ(words_[0], "p");
    if (words_[1] == "cnf" || words_[1] == "wcnf") {
      num_variables_ = StringPieceAtoi(words_[2]);
      num_clauses_ = StringPieceAtoi(words_[3]);
      if (words_[1] == "wcnf") {
        is_wcnf_ = true;
        hard_weight_ = (words_.size() > 4) ? StringPieceAtoi(words_[4]) : 0;
      }
    } else {
      // TODO(user): The ToString() is only required for the open source. Fix.
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

    static const char kWordDelimiters[] = " ";
    auto splitter = absl::StrSplit(line, kWordDelimiters, absl::SkipEmpty());

    tmp_clause_.clear();
    int64_t weight =
        (!is_wcnf_ && interpret_cnf_as_max_sat_) ? 1 : hard_weight_;
    bool first = true;
    bool end_marker_seen = false;
    for (const absl::string_view word : splitter) {
      const int64_t signed_value = StringPieceAtoi(word);
      if (first && is_wcnf_) {
        // Mathematically, a soft clause of weight 0 can be removed.
        if (signed_value == 0) {
          ++num_skipped_soft_clauses_;
          return;
        }
        weight = signed_value;
      } else {
        if (signed_value == 0) {
          end_marker_seen = true;
          break;  // end of clause.
        }
        tmp_clause_.push_back(signed_value);
      }
      first = false;
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
        const int slack_literal = num_variables_ + num_slack_variables_ + 1;
        ++num_slack_variables_;

        tmp_clause_.push_back(slack_literal);

        ++num_added_clauses_;
        problem->AddConstraint(tmp_clause_);

        if (slack_literal > 0) {
          positive_literal_to_weight_[slack_literal] += weight;
        } else {
          positive_literal_to_weight_[-slack_literal] -= weight;
          objective_offset_ += weight;
        }

        if (absl::GetFlag(FLAGS_wcnf_use_strong_slack)) {
          // Add the binary implications slack_literal true => all the other
          // clause literals are false.
          for (int i = 0; i + 1 < tmp_clause_.size(); ++i) {
            problem->AddConstraint({-slack_literal, -tmp_clause_[i]});
          }
        }
      }
    }
  }

  bool interpret_cnf_as_max_sat_;

  int num_clauses_;
  int num_variables_;

  // Temporary storage for ProcessNewLine().
  std::vector<absl::string_view> words_;

  // We stores the objective in a map because we want the variables to appear
  // only once in the LinearObjective proto.
  absl::btree_map<int, int64_t> positive_literal_to_weight_;
  int64_t objective_offset_;

  // Used for the wcnf format.
  bool is_wcnf_;
  // Some files have text after %. This indicates if we have seen the '%'.
  bool end_marker_seen_;
  int64_t hard_weight_;

  int num_slack_variables_;
  int num_skipped_soft_clauses_;
  int num_singleton_soft_clauses_;
  int num_added_clauses_;

  std::vector<int> tmp_clause_;

  DISALLOW_COPY_AND_ASSIGN(SatCnfReader);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_CNF_READER_H_
