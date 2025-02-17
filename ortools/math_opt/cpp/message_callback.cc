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

#include "ortools/math_opt/cpp/message_callback.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/repeated_ptr_field.h"
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

void PushBack(absl::Span<const std::string> messages,
              std::vector<std::string>* const sink) {
  sink->insert(sink->end(), messages.begin(), messages.end());
}

void PushBack(const std::vector<std::string>& messages,
              google::protobuf::RepeatedPtrField<std::string>* const sink) {
  std::copy(messages.begin(), messages.end(),
            google::protobuf::RepeatedFieldBackInserter(sink));
}

template <typename Sink>
class VectorLikeMessageCallbackImpl {
 public:
  explicit VectorLikeMessageCallbackImpl(Sink* const sink)
      : sink_(ABSL_DIE_IF_NULL(sink)) {}

  void Call(const std::vector<std::string>& messages) {
    const absl::MutexLock lock(&mutex_);
    PushBack(messages, sink_);
  }

 private:
  absl::Mutex mutex_;
  Sink* const sink_;
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

MessageCallback InfoLoggerMessageCallback(const absl::string_view prefix,
                                          const absl::SourceLocation loc) {
  return [=](const std::vector<std::string>& messages) {
    for (const std::string& message : messages) {
      LOG(INFO).AtLocation(loc.file_name(), loc.line()) << prefix << message;
    }
  };
}

MessageCallback VLoggerMessageCallback(int level, absl::string_view prefix,
                                       absl::SourceLocation loc) {
  return [=](const std::vector<std::string>& messages) {
    for (const std::string& message : messages) {
      VLOG(level).AtLocation(loc.file_name(), loc.line()) << prefix << message;
    }
  };
}

MessageCallback VectorMessageCallback(std::vector<std::string>* sink) {
  CHECK(sink != nullptr);
  // Here we must use an std::shared_ptr since std::function requires that its
  // input is copyable. And VectorMessageCallbackImpl can't be copyable since it
  // uses an absl::Mutex that is not.
  const auto impl =
      std::make_shared<VectorLikeMessageCallbackImpl<std::vector<std::string>>>(
          sink);
  return
      [=](const std::vector<std::string>& messages) { impl->Call(messages); };
}

MessageCallback RepeatedPtrFieldMessageCallback(
    google::protobuf::RepeatedPtrField<std::string>* sink) {
  CHECK(sink != nullptr);
  // Here we must use an std::shared_ptr since std::function requires that its
  // input is copyable. And VectorMessageCallbackImpl can't be copyable since
  // it uses an absl::Mutex that is not.
  const auto impl = std::make_shared<VectorLikeMessageCallbackImpl<
      google::protobuf::RepeatedPtrField<std::string>>>(sink);
  return
      [=](const std::vector<std::string>& messages) { impl->Call(messages); };
}

}  // namespace operations_research::math_opt
