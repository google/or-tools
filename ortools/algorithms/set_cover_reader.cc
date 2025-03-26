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

#include "ortools/algorithms/set_cover_reader.h"

#include <sys/types.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
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
  // As it is expected that the blanks will be spaces, we can skip them faster
  // by checking for spaces only.
  for (; pos < size && line_[pos] == ' '; ++pos) {
  }
  // We skip all forms of blanks to be on the safe side.
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
  model.ResizeNumSubsets(num_cols.value());
  for (SubsetIndex subset : SubsetRange(num_cols)) {
    const double cost(reader.ParseNextDouble());
    model.SetSubsetCost(subset.value(), cost);
  }
  for (ElementIndex element : ElementRange(num_rows)) {
    LOG_EVERY_N_SEC(INFO, 5)
        << absl::StrFormat("Reading element %d (%.1f%%)", element.value(),
                           100.0 * element.value() / model.num_elements());
    const RowEntryIndex row_size(reader.ParseNextInteger());
    for (RowEntryIndex entry(0); entry < row_size; ++entry) {
      // Correct the 1-indexing.
      const int subset(reader.ParseNextInteger() - 1);
      model.AddElementToSubset(element.value(), subset);
    }
  }
  LOG(INFO) << "Finished reading the model.";
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
  model.ResizeNumSubsets(num_cols);
  for (BaseInt subset(0); subset < num_cols; ++subset) {
    LOG_EVERY_N_SEC(INFO, 5)
        << absl::StrFormat("Reading subset %d (%.1f%%)", subset,
                           100.0 * subset / model.num_subsets());
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
  LOG(INFO) << "Finished reading the model.";
  file->Close(file::Defaults()).IgnoreError();
  model.CreateSparseRowView();
  return model;
}

SetCoverModel ReadFimiDat(absl::string_view filename) {
  SetCoverModel model;
  BaseInt subset(0);
  for (const std::string& line : FileLines(filename)) {
    LOG_EVERY_N_SEC(INFO, 5)
        << absl::StrFormat("Reading subset %d (%.1f%%)", subset,
                           100.0 * subset / model.num_subsets());
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
  LOG(INFO) << "Finished reading the model.";
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
// A class to format data and write it to a file.
// Text is formatted in chunks of at most max_cols characters.
// Text is actually written to the file when the current chunk is full or when
// FlushLine() is called.
// FlushLine() must be called before closing the file.
class LineFormatter {
 public:
  explicit LineFormatter(File* file)
      : LineFormatter(file, std::numeric_limits<int64_t>::max()) {}
  LineFormatter(File* file, int64_t max_cols)
      : num_cols_(0), max_cols_(max_cols), line_(), file_(file) {}
  ~LineFormatter() { CHECK(line_.empty()); }

  void Append(absl::string_view text) {
    const int text_size = text.size();
    if (!text.empty() && text_size + num_cols_ > max_cols_) {
      FlushLine();
    }
    absl::StrAppend(&line_, text);
    num_cols_ += text_size;
  }

  void Append(BaseInt value) { Append(absl::StrCat(value, " ")); }

  void Append(double value) { Append(absl::StrFormat("%.17g ", value)); }

  void FlushLine() {
    CHECK_OK(
        file::WriteString(file_, absl::StrCat(line_, "\n"), file::Defaults()));
    line_.clear();
    num_cols_ = 0;
  }

 private:
  int64_t num_cols_;
  int64_t max_cols_;
  std::string line_;
  File* file_;
};
}  // namespace

void WriteOrlibScp(const SetCoverModel& model, absl::string_view filename) {
  File* file(file::OpenOrDie(filename, "w", file::Defaults()));
  LineFormatter formatter(file);
  formatter.Append(model.num_elements());
  formatter.Append(model.num_subsets());
  formatter.FlushLine();
  for (const SubsetIndex subset : model.SubsetRange()) {
    formatter.Append(model.subset_costs()[subset]);
  }
  formatter.FlushLine();
  for (const ElementIndex element : model.ElementRange()) {
    LOG_EVERY_N_SEC(INFO, 5)
        << absl::StrFormat("Writing element %d (%.1f%%)", element.value(),
                           100.0 * element.value() / model.num_elements());
    formatter.Append(absl::StrCat(model.rows()[element].size(), "\n"));
    for (const SubsetIndex subset : model.rows()[element]) {
      formatter.Append(subset.value() + 1);
    }
    formatter.FlushLine();
  }
  LOG(INFO) << "Finished writing the model.";
  file->Close(file::Defaults()).IgnoreError();
}

// Beware the fact that elements written are converted to 1-indexed.
void WriteOrlibRail(const SetCoverModel& model, absl::string_view filename) {
  File* file(file::OpenOrDie(filename, "w", file::Defaults()));
  CHECK_OK(file::WriteString(
      file, absl::StrCat(model.num_elements(), " ", model.num_subsets(), "\n"),
      file::Defaults()));
  LineFormatter formatter(file);
  for (const SubsetIndex subset : model.SubsetRange()) {
    LOG_EVERY_N_SEC(INFO, 5)
        << absl::StrFormat("Writing subset %d (%.1f%%)", subset.value(),
                           100.0 * subset.value() / model.num_subsets());
    formatter.Append(model.subset_costs()[subset]);
    formatter.Append(static_cast<BaseInt>(model.columns()[subset].size()));
    for (const ElementIndex element : model.columns()[subset]) {
      formatter.Append(element.value() + 1);
    }
    formatter.FlushLine();
  }
  LOG(INFO) << "Finished writing the model.";
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
  LineFormatter formatter(file);
  for (BaseInt subset(0); subset < solution.size(); ++subset) {
    if (solution[SubsetIndex(subset)]) {
      formatter.Append(subset);
    }
  }
  formatter.FlushLine();
  file->Close(file::Defaults()).IgnoreError();
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
