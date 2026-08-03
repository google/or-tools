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

#include "ortools/linear_solver/scip_helper_macros.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "scip/type_retcode.h"

namespace operations_research {
namespace internal {

absl::string_view SCIPRetcodeName(SCIP_RETCODE retcode) {
  switch (retcode) {
    case SCIP_OKAY:
      return "SCIP_OKAY";
    case SCIP_ERROR:
      return "SCIP_ERROR";
    case SCIP_NOMEMORY:
      return "SCIP_NOMEMORY";
    case SCIP_READERROR:
      return "SCIP_READERROR";
    case SCIP_WRITEERROR:
      return "SCIP_WRITEERROR";
    case SCIP_NOFILE:
      return "SCIP_NOFILE";
    case SCIP_FILECREATEERROR:
      return "SCIP_FILECREATEERROR";
    case SCIP_LPERROR:
      return "SCIP_LPERROR";
    case SCIP_NOPROBLEM:
      return "SCIP_NOPROBLEM";
    case SCIP_INVALIDCALL:
      return "SCIP_INVALIDCALL";
    case SCIP_INVALIDDATA:
      return "SCIP_INVALIDDATA";
    case SCIP_INVALIDRESULT:
      return "SCIP_INVALIDRESULT";
    case SCIP_PLUGINNOTFOUND:
      return "SCIP_PLUGINNOTFOUND";
    case SCIP_PARAMETERUNKNOWN:
      return "SCIP_PARAMETERUNKNOWN";
    case SCIP_PARAMETERWRONGTYPE:
      return "SCIP_PARAMETERWRONGTYPE";
    case SCIP_PARAMETERWRONGVAL:
      return "SCIP_PARAMETERWRONGVAL";
    case SCIP_KEYALREADYEXISTING:
      return "SCIP_KEYALREADYEXISTING";
    case SCIP_MAXDEPTHLEVEL:
      return "SCIP_MAXDEPTHLEVEL";
    case SCIP_BRANCHERROR:
      return "SCIP_BRANCHERROR";
    case SCIP_NOTIMPLEMENTED:
      return "SCIP_NOTIMPLEMENTED";
  }
  // We don't use `default:` above so that we get a compilation error due to
  // non-exhaustive `switch` when SCIP adds new enum values.
  return "SCIP_???";
}

void ScopedCaptureScipErrorPrinting::ErrorPrinter(void*, FILE*,
                                                  const char* msg) {
  if (current_thread_capture_ != nullptr) {
    current_thread_capture_->messages().push_back(msg);
    return;
  }

  std::cerr << msg;
}

std::vector<std::string>
ScopedCaptureScipErrorPrinting::MessagesGroupedByLines() const {
  std::vector<std::string> lines;
  std::string pending_partial_line;
  for (absl::string_view message : messages_) {
    if (!message.ends_with('\n')) {
      absl::StrAppend(&pending_partial_line, message);
      continue;
    }
    message.remove_suffix(1);
    lines.push_back(absl::StrCat(pending_partial_line, message));
    pending_partial_line.clear();
  }
  // We expect SCIP to terminate all lines, but better be safe here.
  if (!pending_partial_line.empty()) {
    lines.push_back(std::move(pending_partial_line));
  }
  return lines;
}

}  // namespace internal
}  // namespace operations_research
