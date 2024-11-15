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

#include "ortools/algorithms/set_cover_reader.h"

#include <cctype>
#include <cstddef>
#include <cstdint>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {

class SetCoverReader {
 public:
  explicit SetCoverReader(File* file);
  absl::string_view GetNextToken();
  double ParseNextDouble();
  int64_t ParseNextInteger();

 private:
  size_t SkipBlanks(size_t pos) const;
  size_t SkipNonBlanks(size_t pos) const;
  FileLineIterator line_iter_;
  absl::string_view line_;
  int start_pos_;
  int end_pos_;
};

SetCoverReader::SetCoverReader(File* file)
    : line_iter_(file, FileLineIterator::REMOVE_INLINE_CR |
                           FileLineIterator::REMOVE_BLANK_LINES),
      start_pos_(0),
      end_pos_(0) {
  line_ = *line_iter_;
}

size_t SetCoverReader::SkipBlanks(size_t pos) const {
  const size_t size = line_.size();
  for (; pos < size && std::isspace(line_[pos]); ++pos) {
  }
  return pos;
}

size_t SetCoverReader::SkipNonBlanks(size_t pos) const {
  const size_t size = line_.size();
  for (; pos < size && !std::isspace(line_[pos]); ++pos) {
  }
  return pos;
}

absl::string_view SetCoverReader::GetNextToken() {
  const size_t size = line_.size();
  start_pos_ = SkipBlanks(end_pos_);
  if (start_pos_ >= size) {
    ++line_iter_;
    line_ = *line_iter_;
    start_pos_ = SkipBlanks(0);
  }
  end_pos_ = SkipNonBlanks(start_pos_);
  return line_.substr(start_pos_, end_pos_ - start_pos_);
}

double SetCoverReader::ParseNextDouble() {
  double value = 0;
  CHECK(absl::SimpleAtod(GetNextToken(), &value));
  return value;
}

int64_t SetCoverReader::ParseNextInteger() {
  int64_t value = 0;
  CHECK(absl::SimpleAtoi(GetNextToken(), &value));
  return value;
}

SetCoverModel ReadBeasleySetCoverProblem(absl::string_view filename) {
  SetCoverModel model;
  File* file(file::OpenOrDie(filename, "r", file::Defaults()));
  SetCoverReader reader(file);
  const ElementIndex num_rows(reader.ParseNextInteger());
  const SubsetIndex num_cols(reader.ParseNextInteger());
  model.ReserveNumSubsets(num_cols.value());
  for (SubsetIndex subset : SubsetRange(num_cols)) {
    const double cost(reader.ParseNextDouble());
    model.SetSubsetCost(subset.value(), cost);
  }
  for (ElementIndex element : ElementRange(num_rows)) {
    const RowEntryIndex row_size(reader.ParseNextInteger());
    for (RowEntryIndex entry(0); entry < row_size; ++entry) {
      const int subset(reader.ParseNextInteger() - 1);
      model.AddElementToSubset(element.value(), subset);
    }
  }
  file->Close(file::Defaults()).IgnoreError();
  model.CreateSparseRowView();
  return model;
}

SetCoverModel ReadRailSetCoverProblem(absl::string_view filename) {
  SetCoverModel model;
  File* file(file::OpenOrDie(filename, "r", file::Defaults()));
  SetCoverReader reader(file);
  const ElementIndex num_rows(reader.ParseNextInteger());
  const BaseInt num_cols(reader.ParseNextInteger());
  model.ReserveNumSubsets(num_cols);
  for (int i(0); i < num_cols; ++i) {
    const double cost(reader.ParseNextDouble());
    model.SetSubsetCost(i, cost);
    const ColumnEntryIndex column_size(reader.ParseNextInteger());
    model.ReserveNumElementsInSubset(i, column_size.value());
    for (const ColumnEntryIndex _ : ColumnEntryRange(column_size)) {
      const ElementIndex element(reader.ParseNextInteger() - 1);
      model.AddElementToSubset(element.value(), i);
    }
  }
  file->Close(file::Defaults()).IgnoreError();
  model.CreateSparseRowView();
  return model;
}

}  // namespace operations_research
