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

#include "ortools/util/parse_proto.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/text_format.h"

namespace operations_research {
namespace {

class TextFormatErrorCollector : public google::protobuf::io::ErrorCollector {
 public:
  struct CollectedError {
    bool warning;
    int line;
    int column;
    std::string message;
  };

  TextFormatErrorCollector() = default;
  ~TextFormatErrorCollector() override = default;

  void RecordError(const int line, const int column,
                   absl::string_view message) override {
    collected_errors_.push_back(
        {/*warning=*/false, line, column, std::string(message)});
  }

  void RecordWarning(const int line, const int column,
                     absl::string_view message) override {
    collected_errors_.push_back(
        {/*warning=*/true, line, column, std::string(message)});
  }

  // Returns a string listing each collected error.  When an error is associated
  // with a line number and column number that can be found in `value`, that
  // error message is shown in context using a caret (^), like:
  //  { good_field: 1 error_field: "bad" }
  //                             ^
  std::string RenderErrorMessage(const absl::string_view value) {
    std::string message;
    std::vector<std::string> value_lines = absl::StrSplit(value, '\n');
    for (const auto& error : collected_errors_) {
      if (error.warning) {
        absl::StrAppend(&message, "warning: ");
      }
      absl::StrAppend(&message, error.message, "\n");
      if (error.line >= 0 && error.line < value_lines.size()) {
        const std::string& error_line = value_lines[error.line];
        if (error.column >= 0 && error.column < error_line.size()) {
          // If possible, use ^ to point at error.column in error_line.
          absl::StrAppend(&message, error_line, "\n");
          absl::StrAppend(&message, std::string(error.column, ' '), "^\n");
        }
      }
    }
    return message;
  }

 private:
  std::vector<CollectedError> collected_errors_;
};

}  // namespace

bool ParseTextProtoForFlag(const absl::string_view text,
                           google::protobuf::Message* const message_out,
                           std::string* const error_out) {
  TextFormatErrorCollector errors;
  google::protobuf::TextFormat::Parser parser;
  parser.RecordErrorsTo(&errors);
  const bool success = parser.ParseFromString(std::string(text), message_out);
  *error_out = errors.RenderErrorMessage(text);
  return success;
}

}  // namespace operations_research
