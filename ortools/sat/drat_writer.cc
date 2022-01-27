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

#include "ortools/sat/drat_writer.h"

#include <cstdlib>

#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/file.h"
#endif  // !__PORTABLE_PLATFORM__
#include "absl/status/status.h"

namespace operations_research {
namespace sat {

DratWriter::~DratWriter() {
  if (output_ != nullptr) {
#if !defined(__PORTABLE_PLATFORM__)
    CHECK_OK(file::WriteString(output_, buffer_, file::Defaults()));
    CHECK_OK(output_->Close(file::Defaults()));
#endif  // !__PORTABLE_PLATFORM__
  }
}

void DratWriter::AddClause(absl::Span<const Literal> clause) {
  WriteClause(clause);
}

void DratWriter::DeleteClause(absl::Span<const Literal> clause) {
  buffer_ += "d ";
  WriteClause(clause);
}

void DratWriter::WriteClause(absl::Span<const Literal> clause) {
  for (const Literal literal : clause) {
    absl::StrAppendFormat(&buffer_, "%d ", literal.SignedValue());
  }
  buffer_ += "0\n";
  if (buffer_.size() > 10000) {
#if !defined(__PORTABLE_PLATFORM__)
    CHECK_OK(file::WriteString(output_, buffer_, file::Defaults()));
#endif  // !__PORTABLE_PLATFORM__
    buffer_.clear();
  }
}

}  // namespace sat
}  // namespace operations_research
