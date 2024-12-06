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
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {

class SetCoverReader {
 public:
  explicit SetCoverReader(File* file);
  absl::string_view GetLine() { return line_; }
  void Advance() { ++line_iter_; }
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

// This is a row-based format where the elements are 1-indexed.
SetCoverModel ReadOrlibScp(absl::string_view filename) {
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
      // Correct the 1-indexing.
      const int subset(reader.ParseNextInteger() - 1);
      model.AddElementToSubset(element.value(), subset);
    }
  }
  file->Close(file::Defaults()).IgnoreError();
  model.CreateSparseRowView();
  return model;
}

// This is a column-based format where the elements are 1-indexed.
SetCoverModel ReadOrlibRail(absl::string_view filename) {
  SetCoverModel model;
  File* file(file::OpenOrDie(filename, "r", file::Defaults()));
  SetCoverReader reader(file);
  const ElementIndex num_rows(reader.ParseNextInteger());
  const BaseInt num_cols(reader.ParseNextInteger());
  model.ReserveNumSubsets(num_cols);
  for (int subset(0); subset < num_cols; ++subset) {
    const double cost(reader.ParseNextDouble());
    model.SetSubsetCost(subset, cost);
    const ColumnEntryIndex column_size(reader.ParseNextInteger());
    model.ReserveNumElementsInSubset(column_size.value(), subset);
    for (const ColumnEntryIndex _ : ColumnEntryRange(column_size)) {
      // Correct the 1-indexing.
      const ElementIndex element(reader.ParseNextInteger() - 1);
      model.AddElementToSubset(element.value(), subset);
    }
  }
  file->Close(file::Defaults()).IgnoreError();
  model.CreateSparseRowView();
  return model;
}

SetCoverModel ReadFimiDat(absl::string_view filename) {
  SetCoverModel model;
  for (const std::string& line : FileLines(filename)) {
    SubsetIndex subset(0);
    std::vector<std::string> elements = absl::StrSplit(line, ' ');
    if (elements.back().empty() || elements.back()[0] == '\0') {
      elements.pop_back();
    }
    model.AddEmptySubset(1);
    for (const std::string& number : elements) {
      BaseInt element;
      CHECK(absl::SimpleAtoi(number, &element));
      CHECK_GT(element, 0);
      // Correct the 1-indexing.
      model.AddElementToLastSubset(ElementIndex(element - 1));
    }
    ++subset;
  }
  model.CreateSparseRowView();
  return model;
}

SetCoverModel ReadSetCoverProto(absl::string_view filename, bool binary) {
  SetCoverModel model;
  SetCoverProto message;
  if (binary) {
    CHECK_OK(file::GetBinaryProto(filename, &message, file::Defaults()));
  } else {
    CHECK_OK(file::GetTextProto(filename, &message, file::Defaults()));
  }
  model.ImportModelFromProto(message);
  return model;
}

namespace {
// A class to write a line of text to a file.
// The line is written in chunks of at most max_cols characters.
class LineWriter {
 public:
  LineWriter(File* file, int max_cols)
      : num_cols_(0), max_cols_(max_cols), line_(), file_(file) {}
  ~LineWriter() { Close(); }
  void Write(BaseInt value) {
    const std::string text = absl::StrCat(value, " ");
    const int text_size = text.size();
    if (num_cols_ + text_size > max_cols_) {
      CHECK_OK(file::WriteString(file_, absl::StrCat(line_, "\n"),
                                 file::Defaults()));
      line_.clear();
    } else {
      absl::StrAppend(&line_, text);
      num_cols_ += text_size;
    }
  }
  void Close() { CHECK_OK(file::WriteString(file_, line_, file::Defaults())); }

 private:
  int num_cols_;
  int max_cols_;
  std::string line_;
  File* file_;
};
}  // namespace

void WriteOrlibScp(const SetCoverModel& model, absl::string_view filename) {
  const int kMaxCols = 80;
  File* file(file::OpenOrDie(filename, "w", file::Defaults()));
  CHECK_OK(file::WriteString(
      file, absl::StrCat(model.num_elements(), " ", model.num_subsets(), "\n"),
      file::Defaults()));
  LineWriter cost_writer(file, kMaxCols);
  for (const SubsetIndex subset : model.SubsetRange()) {
    cost_writer.Write(model.subset_costs()[subset]);
  }
  for (const ElementIndex element : model.ElementRange()) {
    CHECK_OK(file::WriteString(file,
                               absl::StrCat(model.rows()[element].size(), "\n"),
                               file::Defaults()));
    LineWriter row_writer(file, kMaxCols);
    for (const SubsetIndex subset : model.rows()[element]) {
      row_writer.Write(subset.value() + 1);
    }
  }
  file->Close(file::Defaults()).IgnoreError();
}

// Beware the fact that elements written are converted to 1-indexed.
void WriteOrlibRail(const SetCoverModel& model, absl::string_view filename) {
  const int kMaxCols = 80;
  File* file(file::OpenOrDie(filename, "w", file::Defaults()));
  CHECK_OK(file::WriteString(
      file, absl::StrCat(model.num_elements(), " ", model.num_subsets(), "\n"),
      file::Defaults()));
  for (const SubsetIndex subset : model.SubsetRange()) {
    CHECK_OK(
        file::WriteString(file,
                          absl::StrCat(model.subset_costs()[subset], " ",
                                       model.columns()[subset].size(), "\n"),
                          file::Defaults()));
    LineWriter writer(file, kMaxCols);
    for (const ElementIndex element : model.columns()[subset]) {
      writer.Write(element.value() + 1);
    }
  }
  file->Close(file::Defaults()).IgnoreError();
}

void WriteSetCoverProto(const SetCoverModel& model, absl::string_view filename,
                        bool binary) {
  const SetCoverProto message = model.ExportModelAsProto();
  if (binary) {
    CHECK_OK(file::SetBinaryProto(filename, message, file::Defaults()));
  } else {
    CHECK_OK(file::SetTextProto(filename, message, file::Defaults()));
  }
}

SubsetBoolVector ReadSetCoverSolutionText(absl::string_view filename) {
  SubsetBoolVector solution;
  File* file(file::OpenOrDie(filename, "r", file::Defaults()));
  SetCoverReader reader(file);
  const BaseInt num_cols(reader.ParseNextInteger());
  solution.resize(num_cols, false);
  const BaseInt cardinality(reader.ParseNextInteger());
  for (int i = 0; i < cardinality; ++i) {
    // NOTE(user): The solution is 0-indexed.
    const SubsetIndex subset(reader.ParseNextInteger());
    solution[subset] = true;
  }
  file->Close(file::Defaults()).IgnoreError();
  return solution;
}

SubsetBoolVector ReadSetCoverSolutionProto(absl::string_view filename,
                                           bool binary) {
  SubsetBoolVector solution;
  SetCoverSolutionResponse message;
  if (binary) {
    CHECK_OK(file::GetBinaryProto(filename, &message, file::Defaults()));
  } else {
    CHECK_OK(file::GetTextProto(filename, &message, file::Defaults()));
  }
  solution.resize(message.num_subsets(), false);
  // NOTE(user): The solution is 0-indexed.
  for (const BaseInt subset : message.subset()) {
    solution[SubsetIndex(subset)] = true;
  }
  return solution;
}

void WriteSetCoverSolutionText(const SetCoverModel& model,
                               const SubsetBoolVector& solution,
                               absl::string_view filename) {
  File* file(file::OpenOrDie(filename, "w", file::Defaults()));
  BaseInt cardinality(0);
  Cost cost(0);
  for (SubsetIndex subset(0); subset.value() < solution.size(); ++subset) {
    if (solution[subset]) {
      ++cardinality;
      cost += model.subset_costs()[subset];
    }
  }
  CHECK_OK(file::WriteString(
      file, absl::StrCat(solution.size(), " ", cardinality, " ", cost, "\n"),
      file::Defaults()));
  const int kMaxCols = 80;
  LineWriter writer(file, kMaxCols);
  for (BaseInt subset(0); subset < solution.size(); ++subset) {
    if (solution[SubsetIndex(subset)]) {
      writer.Write(subset);
    }
  }
}

void WriteSetCoverSolutionProto(const SetCoverModel& model,
                                const SubsetBoolVector& solution,
                                absl::string_view filename, bool binary) {
  SetCoverSolutionResponse message;
  message.set_num_subsets(solution.size());
  Cost cost(0);
  for (SubsetIndex subset(0); subset.value() < solution.size(); ++subset) {
    if (solution[subset]) {
      message.add_subset(subset.value());
      cost += model.subset_costs()[subset];
    }
  }
  message.set_cost(cost);
  if (binary) {
    CHECK_OK(file::SetBinaryProto(filename, message, file::Defaults()));
  } else {
    CHECK_OK(file::SetTextProto(filename, message, file::Defaults()));
  }
}
}  // namespace operations_research
