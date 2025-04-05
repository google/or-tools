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

#ifndef OR_TOOLS_SAT_OPB_READER_H_
#define OR_TOOLS_SAT_OPB_READER_H_

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {
namespace sat {

// This class loads a file in pbo file format into a LinearBooleanProblem.
// The format is described here:
//   http://www.cril.univ-artois.fr/PB24/format.pdf
class OpbReader {
 public:
  OpbReader() = default;
  // This type is neither copyable nor movable.
  OpbReader(const OpbReader&) = delete;
  OpbReader& operator=(const OpbReader&) = delete;

  // Returns the number of variables in the problem.
  int num_variables() const { return num_variables_; }

  // Returns true if the model is supported. A model is not supported if it
  // contains an integer that does not fit in int64_t.
  bool model_is_supported() const { return model_is_supported_; }

  // Loads the given opb filename into the given problem.
  // Returns true on success.
  bool LoadAndValidate(const std::string& filename, CpModelProto* model) {
    model->Clear();
    model->set_name(ExtractProblemName(filename));

    num_variables_ = 0;
    int num_lines = 0;
    model_is_supported_ = true;

    // Read constraints line by line (1 constraint per line).
    // We process into a temporary structure to support non linear constraints
    // and weighted constraints.
    for (const std::string& line : FileLines(filename)) {
      ++num_lines;
      ProcessNewLine(line);
      if (!model_is_supported_) {
        LOG(ERROR) << "Unsupported model: '" << filename << "'";
        return false;
      }
    }
    if (num_lines == 0) {
      LOG(ERROR) << "File '" << filename << "' is empty or can't be read.";
      return false;
    }

    LOG(INFO) << "Read " << num_lines << " lines from " << filename;
    LOG(INFO) << "#variables: " << num_variables_;
    LOG(INFO) << "#constraints: " << constraints_.size();
    LOG(INFO) << "#objective: " << objective_.size();

    const std::string error_message = ValidateModel();
    if (!error_message.empty()) {
      LOG(ERROR) << "Error while trying to parse '" << filename
                 << "': " << error_message;
      return false;
    }

    BuildModel(model);
    return true;
  }

 private:
  // A term is coeff * Product(literals).
  // Note that it is okay to have duplicate literals here, we will just merge
  // them. Having a literal and its negation will always result in a product of
  // zero.
  struct PbTerm {
    int64_t coeff;
    std::vector<int> literals;  // CpModelProto literals
  };

  enum PbConstraintType {
    UNDEFINED_OPERATION,
    GE_OPERATION,
    EQ_OPERATION,
  };

  struct PbConstraint {
    std::vector<PbTerm> terms;
    PbConstraintType type = UNDEFINED_OPERATION;
    int64_t rhs = std::numeric_limits<int64_t>::min();
    int64_t soft_cost = std::numeric_limits<int64_t>::max();
  };

  // Since the problem name is not stored in the opb format, we infer it from
  // the file name.
  static std::string ExtractProblemName(const std::string& filename) {
    const int found = filename.find_last_of('/');
    const std::string problem_name =
        found != std::string::npos ? filename.substr(found + 1) : filename;
    return problem_name;
  }

  void ProcessNewLine(const std::string& line) {
    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" ;"), absl::SkipEmpty());
    if (words.empty() || words[0].empty() || words[0][0] == '*') {
      // TODO(user): Parse comments.
      return;
    }

    if (words[0] == "min:") {
      for (int i = 1; i < words.size(); ++i) {
        const std::string& word = words[i];
        if (word.empty() || word[0] == ';') continue;
        if (word[0] == 'x') {
          const int index = ParseIndex(word.substr(1));
          num_variables_ = std::max(num_variables_, index);
          objective_.back().literals.push_back(
              PbLiteralToCpModelLiteral(index));
        } else if (word[0] == '~' && word[1] == 'x') {
          const int index = ParseIndex(word.substr(2));
          num_variables_ = std::max(num_variables_, index);
          objective_.back().literals.push_back(
              NegatedRef(PbLiteralToCpModelLiteral(index)));
        } else {
          // Note that coefficient always appear before the variable/variables.
          PbTerm term;
          if (!ParseInt64Into(word, &term.coeff)) return;
          objective_.emplace_back(std::move(term));
        }
      }

      // Normalize objective literals.
      for (PbTerm& term : objective_) {
        if (term.literals.size() <= 1) continue;
        gtl::STLSortAndRemoveDuplicates(&term.literals);
        CHECK_GT(term.literals.size(), 1);
      }

      return;
    }

    PbConstraint constraint;
    for (int i = 0; i < words.size(); ++i) {
      const std::string& word = words[i];
      CHECK(!word.empty());
      if (word[0] == '[') {  // Soft constraint.
        if (!ParseInt64Into(word.substr(1, word.size() - 2),
                            &constraint.soft_cost)) {
          return;
        }
      } else if (word == ">=") {
        CHECK_LT(i + 1, words.size());
        constraint.type = GE_OPERATION;
        if (!ParseInt64Into(words[i + 1], &constraint.rhs)) return;
        break;
      } else if (word == "=") {
        CHECK_LT(i + 1, words.size());
        constraint.type = EQ_OPERATION;
        if (!ParseInt64Into(words[i + 1], &constraint.rhs)) return;
        break;
      } else if (word[0] == 'x') {
        const int index = ParseIndex(word.substr(1));
        num_variables_ = std::max(num_variables_, index);
        constraint.terms.back().literals.push_back(
            PbLiteralToCpModelLiteral(index));
      } else if (word[0] == '~' && word[1] == 'x') {
        const int index = ParseIndex(word.substr(2));
        num_variables_ = std::max(num_variables_, index);
        constraint.terms.back().literals.push_back(
            NegatedRef(PbLiteralToCpModelLiteral(index)));
      } else {
        // Note that coefficient always appear before the variable/variables.
        PbTerm term;
        if (!ParseInt64Into(word, &term.coeff)) return;
        constraint.terms.emplace_back(std::move(term));
      }
    }

    // Normalize literals.
    for (PbTerm& term : constraint.terms) {
      if (term.literals.size() <= 1) continue;
      gtl::STLSortAndRemoveDuplicates(&term.literals);
      CHECK_GT(term.literals.size(), 1);
    }

    constraints_.push_back(std::move(constraint));
  }

  std::string ValidateModel() {
    // Normalize and validate constraints.
    for (const PbConstraint& constraint : constraints_) {
      if (constraint.rhs == std::numeric_limits<int64_t>::min()) {
        return "constraint error: undefined rhs";
      }

      if (constraint.type == UNDEFINED_OPERATION) {
        return "constraint error: undefined operation";
      }

      for (const PbTerm& term : constraint.terms) {
        if (term.coeff == 0) {
          return "constraint error: coefficient cannot be zero";
        }
        if (term.literals.empty()) return "constraint error: empty literals";
        if (term.literals.size() == 1) {
          if (!RefIsPositive(term.literals[0])) {
            return "constraint error: linear terms must use positive literals";
          }
        }
      }
    }

    // Normalize and validate objective.
    if (objective_.empty()) return "";  // No objective.

    for (const PbTerm& term : objective_) {
      if (term.coeff == 0) return "objective error: coefficient cannot be zero";
      if (term.literals.empty()) return "objective error: empty literals";
      if (term.literals.size() == 1) {
        if (!RefIsPositive(term.literals[0])) {
          return "objective error: linear terms must use positive literals";
        }
        return "";
      }
    }

    return "";
  }

  static int PbLiteralToCpModelLiteral(int pb_literal) {
    return pb_literal > 0 ? pb_literal - 1 : -pb_literal;
  }

  bool ParseInt64Into(const std::string& word, int64_t* value) {
    if (!absl::SimpleAtoi(word, value)) {
      model_is_supported_ = false;
      return false;
    }
    return true;
  }

  static int ParseIndex(const std::string& word) {
    int index;
    CHECK(absl::SimpleAtoi(word, &index));
    return index;
  }

  int GetVariable(const PbTerm& term, CpModelProto* model) {
    CHECK(!term.literals.empty());
    if (term.literals.size() == 1) {
      CHECK(RefIsPositive(term.literals[0]));
      return term.literals[0];
    }

    const auto it = product_to_var_.find(term.literals);
    if (it != product_to_var_.end()) {
      return it->second;
    }

    const int var_index = model->variables_size();
    IntegerVariableProto* var_proto = model->add_variables();
    var_proto->add_domain(0);
    var_proto->add_domain(1);

    product_to_var_[term.literals] = var_index;

    // Link the new variable to the terms.
    // var_index => and(literals).
    ConstraintProto* var_to_literals = model->add_constraints();
    var_to_literals->add_enforcement_literal(var_index);

    // and(literals) => var_index.
    ConstraintProto* literals_to_var = model->add_constraints();
    literals_to_var->mutable_bool_and()->add_literals(var_index);

    for (const int proto_literal : term.literals) {
      var_to_literals->mutable_bool_and()->add_literals(proto_literal);
      literals_to_var->add_enforcement_literal(proto_literal);
    }

    return var_index;
  }

  void BuildModel(CpModelProto* model) {
    // We know how many variables we have, so we can add them all.
    for (int i = 0; i < num_variables_; ++i) {
      IntegerVariableProto* var = model->add_variables();
      var->add_domain(0);
      var->add_domain(1);
    }

    for (const PbConstraint& constraint : constraints_) {
      ConstraintProto* ct = model->add_constraints();
      LinearConstraintProto* lin = ct->mutable_linear();
      for (const PbTerm& term : constraint.terms) {
        lin->add_vars(GetVariable(term, model));
        lin->add_coeffs(term.coeff);
      }
      if (constraint.type == GE_OPERATION) {
        lin->add_domain(constraint.rhs);
        lin->add_domain(std::numeric_limits<int64_t>::max());
      } else if (constraint.type == EQ_OPERATION) {
        lin->add_domain(constraint.rhs);
        lin->add_domain(constraint.rhs);
      } else {
        LOG(FATAL) << "Unsupported operation: " << constraint.type;
      }

      if (constraint.soft_cost != std::numeric_limits<int64_t>::max()) {
        const int violation_var_index = model->variables_size();
        IntegerVariableProto* violation_var = model->add_variables();
        violation_var->add_domain(0);
        violation_var->add_domain(1);

        // Update the objective.
        model->mutable_objective()->add_vars(violation_var_index);
        model->mutable_objective()->add_coeffs(constraint.soft_cost);

        // Add the enforcement literal to ct.
        ct->add_enforcement_literal(NegatedRef(violation_var_index));
      }
    }

    if (!objective_.empty()) {
      CpObjectiveProto* obj = model->mutable_objective();
      for (const PbTerm& term : objective_) {
        obj->add_vars(GetVariable(term, model));
        obj->add_coeffs(term.coeff);
      }
    }
  }

  int num_variables_;
  std::vector<PbTerm> objective_;
  std::vector<PbConstraint> constraints_;
  absl::flat_hash_map<absl::Span<const int>, int> product_to_var_;
  bool model_is_supported_ = true;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_OPB_READER_H_
