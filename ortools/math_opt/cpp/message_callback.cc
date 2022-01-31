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

#include "ortools/math_opt/cpp/message_callback.h"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/logging.h"
#include "ortools/base/source_location.h"

namespace operations_research::math_opt {
namespace {

class PrinterMessageCallbackImpl {
 public:
  PrinterMessageCallbackImpl(std::ostream& output_stream,
                             const absl::string_view prefix)
      : output_stream_(output_stream), prefix_(prefix) {}

  void Call(const std::vector<std::string>& messages) {
    const absl::MutexLock lock(&mutex_);
    for (const std::string& message : messages) {
      output_stream_ << prefix_ << message << '\n';
    }
    output_stream_.flush();
  }

 private:
  absl::Mutex mutex_;
  std::ostream& output_stream_ ABSL_GUARDED_BY(mutex_);
  const std::string prefix_;
};

}  // namespace

MessageCallback PrinterMessageCallback(std::ostream& output_stream,
                                       const absl::string_view prefix) {
  // Here we must use an std::shared_ptr since std::function requires that its
  // input is copyable. And PrinterMessageCallbackImpl can't be copyable since
  // it uses an absl::Mutex that is not.
  const auto impl =
      std::make_shared<PrinterMessageCallbackImpl>(output_stream, prefix);
  return
      [=](const std::vector<std::string>& messages) { impl->Call(messages); };
}

}  // namespace operations_research::math_opt
