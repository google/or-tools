// Copyright 2010-2017 Google
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

#include "ortools/sat/drat.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/int_type.h"
#include "ortools/base/status.h"

namespace operations_research {
namespace sat {

DratWriter::~DratWriter() {
  if (output_ != nullptr) {
    CHECK_OK(file::WriteString(output_, buffer_, file::Defaults()));
    CHECK_OK(output_->Close(file::Defaults()));
  }
}

void DratWriter::ApplyMapping(
    const ITIVector<BooleanVariable, BooleanVariable>& mapping) {
  ITIVector<BooleanVariable, BooleanVariable> new_mapping;
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

void DratWriter::SetNumVariables(int num_variables) {
  CHECK_GE(num_variables, reverse_mapping_.size());
  while (reverse_mapping_.size() < num_variables) {
    reverse_mapping_.push_back(BooleanVariable(variable_index_++));
  }
}

void DratWriter::AddOneVariable() {
  reverse_mapping_.push_back(BooleanVariable(variable_index_++));
}

void DratWriter::AddClause(gtl::Span<Literal> clause) {
  WriteClause(clause);
}

void DratWriter::DeleteClause(gtl::Span<Literal> clause,
                              bool ignore_call) {
  if (ignore_call) return;
  buffer_ += "d ";
  WriteClause(clause);
}

void DratWriter::WriteClause(gtl::Span<Literal> clause) {
  values_.clear();
  for (const Literal l : clause) {
    CHECK_LT(l.Variable(), reverse_mapping_.size());
    const Literal original_literal =
        Literal(reverse_mapping_[l.Variable()], l.IsPositive());
    values_.push_back(original_literal.SignedValue());
  }

  // The sorting is such that new variables appear first. This is important for
  // BVA since DRAT-trim only check the RAT property with respect to the first
  // variable of the clause.
  std::sort(values_.begin(), values_.end(),
            [](int a, int b) { return std::abs(a) > std::abs(b); });

  for (const int v : values_) StringAppendF(&buffer_, "%d ", v);
  buffer_ += "0\n";
  if (buffer_.size() > 10000) {
    CHECK_OK(file::WriteString(output_, buffer_, file::Defaults()));
    buffer_.clear();
  }
}

}  // namespace sat
}  // namespace operations_research
