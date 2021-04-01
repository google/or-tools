// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/drat_proof_handler.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {
namespace sat {

DratProofHandler::DratProofHandler()
    : variable_index_(0), drat_checker_(new DratChecker()) {}

DratProofHandler::DratProofHandler(bool in_binary_format, File* output,
                                   bool check)
    : variable_index_(0),
      drat_writer_(new DratWriter(in_binary_format, output)) {
  if (check) {
    drat_checker_ = absl::make_unique<DratChecker>();
  }
}

void DratProofHandler::ApplyMapping(
    const absl::StrongVector<BooleanVariable, BooleanVariable>& mapping) {
  absl::StrongVector<BooleanVariable, BooleanVariable> new_mapping;
  for (BooleanVariable v(0); v < mapping.size(); ++v) {
    const BooleanVariable image = mapping[v];
    if (image != kNoBooleanVariable) {
      if (image >= new_mapping.size())
        new_mapping.resize(image.value() + 1, kNoBooleanVariable);
      CHECK_EQ(new_mapping[image], kNoBooleanVariable);
      new_mapping[image] =
          v < reverse_mapping_.size() ? reverse_mapping_[v] : v;
      CHECK_NE(new_mapping[image], kNoBooleanVariable);
    }
  }
  std::swap(new_mapping, reverse_mapping_);
}

void DratProofHandler::SetNumVariables(int num_variables) {
  CHECK_GE(num_variables, reverse_mapping_.size());
  while (reverse_mapping_.size() < num_variables) {
    reverse_mapping_.push_back(BooleanVariable(variable_index_++));
  }
}

void DratProofHandler::AddOneVariable() {
  reverse_mapping_.push_back(BooleanVariable(variable_index_++));
}

void DratProofHandler::AddProblemClause(absl::Span<const Literal> clause) {
  if (drat_checker_ != nullptr) {
    drat_checker_->AddProblemClause(clause);
  }
}

void DratProofHandler::AddClause(absl::Span<const Literal> clause) {
  MapClause(clause);
  if (drat_checker_ != nullptr) {
    drat_checker_->AddInferedClause(values_);
  }
  if (drat_writer_ != nullptr) {
    drat_writer_->AddClause(values_);
  }
}

void DratProofHandler::DeleteClause(absl::Span<const Literal> clause) {
  MapClause(clause);
  if (drat_checker_ != nullptr) {
    drat_checker_->DeleteClause(values_);
  }
  if (drat_writer_ != nullptr) {
    drat_writer_->DeleteClause(values_);
  }
}

DratChecker::Status DratProofHandler::Check(double max_time_in_seconds) {
  if (drat_checker_ != nullptr) {
    // The empty clause is not explicitly added by the solver.
    drat_checker_->AddInferedClause({});
    return drat_checker_->Check(max_time_in_seconds);
  }
  return DratChecker::Status::UNKNOWN;
}

void DratProofHandler::MapClause(absl::Span<const Literal> clause) {
  values_.clear();
  for (const Literal l : clause) {
    CHECK_LT(l.Variable(), reverse_mapping_.size());
    const Literal original_literal =
        Literal(reverse_mapping_[l.Variable()], l.IsPositive());
    values_.push_back(original_literal);
  }

  // The sorting is such that new variables appear first. This is important for
  // BVA since DRAT-trim only check the RAT property with respect to the first
  // variable of the clause.
  std::sort(values_.begin(), values_.end(), [](Literal a, Literal b) {
    return std::abs(a.SignedValue()) > std::abs(b.SignedValue());
  });
}

}  // namespace sat
}  // namespace operations_research
