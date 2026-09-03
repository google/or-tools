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

#include "ortools/math_opt/core/message_callback_testing.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/synchronization/mutex.h"
#include "ortools/math_opt/core/base_solver.h"

namespace operations_research::math_opt {
namespace {

// Shared state of the MessageCallback returned by TestingMessageCallback().
class TestingMessageCallbackImpl {
 public:
  explicit TestingMessageCallbackImpl(
      std::vector<std::vector<std::string>>* absl_nonnull const calls)
      : calls_(calls) {}
  TestingMessageCallbackImpl(const TestingMessageCallbackImpl&) = delete;
  TestingMessageCallbackImpl& operator=(const TestingMessageCallbackImpl&) =
      delete;

  // Appends a call to the MessageCallback.
  void AppendCall(const std::vector<std::string>& messages) {
    const absl::MutexLock lock(mutex_);
    calls_->push_back(messages);
  }

 private:
  mutable absl::Mutex mutex_;
  std::vector<std::vector<std::string>>* absl_nonnull const calls_;
};

}  // namespace

BaseSolver::MessageCallback TestingMessageCallback(
    std::vector<std::vector<std::string>>* absl_nonnull const calls) {
  std::shared_ptr<TestingMessageCallbackImpl> impl =
      std::make_shared<TestingMessageCallbackImpl>(calls);
  return [impl = std::move(impl)](const std::vector<std::string>& messages) {
    impl->AppendCall(messages);
  };
};

}  // namespace operations_research::math_opt
