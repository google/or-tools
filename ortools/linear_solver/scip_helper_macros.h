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

#ifndef ORTOOLS_LINEAR_SOLVER_SCIP_HELPER_MACROS_H_
#define ORTOOLS_LINEAR_SOLVER_SCIP_HELPER_MACROS_H_

#include <cstdio>
#include <string>
#include <vector>

#include "absl/base/call_once.h"
#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_macros.h"
#include "scip/pub_message.h"
#include "scip/type_retcode.h"

namespace operations_research {
namespace internal {

// Returns the name of the SCIP enum, E.g., "SCIP_OKAY", or "SCIP_READERROR".
//
// Unknown values return "SCIP_???".
absl::string_view SCIPRetcodeName(SCIP_RETCODE retcode);

// RAII object that captures message from SCIPerrorMessage() macro in the
// calling thread.
//
// The first instantiation of this objects installs an error printer with
// SCIPmessageSetErrorPrinting(). It prints to stderr when no
// ScopedCaptureScipErrorPrinting instance exists in the calling thread.
//
// It is valid to create a new instance in a thread where other instances
// already exist. This typically happens in callbacks that adds lazy
// constraints.
//
// Only the most recent instance captures errors, previous instances are
// temporarily disabled.
//
// The instances MUST be destroyed in reverse order of their creation (which
// SCIP_TO_STATUS() will do by construction).
class ScopedCaptureScipErrorPrinting {
 public:
  // Starts the capture of error prints, keeping track of the previous active
  // instance.
  //
  // See class documentation for details having multiple instances in the same
  // thread.
  ScopedCaptureScipErrorPrinting()
      : current_thread_previous_capture_(current_thread_capture_) {
    current_thread_capture_ = this;
    RedirectsScipErrorPrinting();
  }

  ScopedCaptureScipErrorPrinting(const ScopedCaptureScipErrorPrinting&) =
      delete;
  ScopedCaptureScipErrorPrinting& operator=(
      const ScopedCaptureScipErrorPrinting&) = delete;

  // Stops the capture of error prints, restoring a previous active instance if
  // there was one.
  //
  // See class documentation for details having multiple instances in the same
  // thread.
  ~ScopedCaptureScipErrorPrinting() {
    // We validate that we destroy instances in proper order.
    CHECK_EQ(current_thread_capture_, this);
    current_thread_capture_ = current_thread_previous_capture_;
  }

  // Returns the captured messages joined by lines, without the terminating '\n'
  // at the end of each line.
  std::vector<std::string> MessagesGroupedByLines() const;

  // Returns the raw messages.
  //
  // SCIP calls the printer multiple times per-line. See
  // MessagesGroupedByLines() to get the lines.
  const std::vector<std::string>& messages() const { return messages_; }
  std::vector<std::string>& messages() { return messages_; }

 private:
  // Permanently redirects all SCIP error printing. Each time SCIP print errors,
  // it is redirected:
  // * either to stderr (default SCIP behavior) when current_thread_capture_ is
  //   nullptr,
  // * or to the messages in current_thread_capture_.
  static void RedirectsScipErrorPrinting() {
    absl::call_once(redirect_scip_error_printing_once_, []() {
      SCIPmessageSetErrorPrinting(&ErrorPrinter, /*data=*/nullptr);
    });
  }

  // Callback function installed by RedirectsScipErrorPrinting() to redirect
  // SCIP error messages.
  static void ErrorPrinter(void*, FILE*, const char* msg);

  // Flag used in RedirectsScipErrorPrinting() to make sure only one thread will
  // setup SCIP to redirect outputs and that other threads will wait for this
  // thread to be done.
  //
  // We must do so since SCIP has a unique global variable that points to the
  // printer.
  inline static constinit absl::once_flag redirect_scip_error_printing_once_;

  // Set by ScopedCaptureScipErrorPrinting constructor to point to the thread's
  // current capture.
  //
  // Reset by its destructor.
  //
  // As of 2026-02-25, clang-format will put the entire type of this variable on
  // a single line and the linter then complains that it is more than 80
  // characters long. We thus disable linting.
  //
  // NOLINTNEXTLINE
  inline static constinit thread_local ScopedCaptureScipErrorPrinting* absl_nullable
      current_thread_capture_ = nullptr;

  // The value of current_thread_capture_ when this instance was built.
  ScopedCaptureScipErrorPrinting* absl_nullable const
      current_thread_previous_capture_;

  std::vector<std::string> messages_;
};

// Our own version of SCIP_CALL to do error management.
inline absl::Status ScipCodeToUtilStatus(
    const SCIP_RETCODE retcode, const char* const source_file,
    const int source_line, const char* const scip_statement,
    const ScopedCaptureScipErrorPrinting& captured_errors) {
  if (retcode == SCIP_OKAY) {
    // Test with inline messages() before calling MessagesGroupedByLines() as we
    // almost never expect any message in the SCIP_OKAY case.
    if (!captured_errors.messages().empty()) {
      const std::vector<std::string> lines =
          captured_errors.MessagesGroupedByLines();
      LOG(ERROR) << "Scip returned SCIP_OKAY but it has printed some errors:";
      for (const std::string& line : lines) {
        LOG(ERROR) << "  " << line;
      }
    }
    return absl::OkStatus();
  }

  auto status_builder =
      util::InvalidArgumentErrorBuilder()
      << "SCIP error code " << retcode << " (" << SCIPRetcodeName(retcode)
      << ") (file '" << absl::CEscape(source_file) << "', line " << source_line
      << ") on '" << absl::CEscape(scip_statement) << "'";
  const std::vector<std::string> lines =
      captured_errors.MessagesGroupedByLines();
  if (!lines.empty()) {
    status_builder << " (SCIP messages: [";
    bool first = true;
    for (const std::string& message : lines) {
      if (first) {
        first = false;
      } else {
        status_builder << ", ";
      }
      status_builder << '\'' << absl::CEscape(message) << '\'';
    }
    status_builder << "])";
  }
  return status_builder;
}

}  // namespace internal

#define SCIP_TO_STATUS(x)                                           \
  [&]() {                                                           \
    ::operations_research::internal::ScopedCaptureScipErrorPrinting \
        captured_errors;                                            \
    const SCIP_RETCODE retcode = (x);                               \
    return ::operations_research::internal::ScipCodeToUtilStatus(   \
        retcode, __FILE__, __LINE__, #x, captured_errors);          \
  }()

#define RETURN_IF_SCIP_ERROR(x) RETURN_IF_ERROR(SCIP_TO_STATUS(x));

}  // namespace operations_research

#endif  // ORTOOLS_LINEAR_SOLVER_SCIP_HELPER_MACROS_H_
