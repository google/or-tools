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

// Utility classes for documenting and checking access (in debug mode) to  class
// variables that are recycled.
// In release mode, this only does compile-time checks and does nothing at
// runtime.
//
// A common pattern to avoid allocations is:
//
// class A {
//  public:
//   void foo() {
//     recycled_.clear();
//     // Use `recycled_` ...
//     //
//   }
//
//  private:
//   // A class variable that is only used by `foo`.
//   std::vector<T> recycled_;
// };
//
// Using a `RecycledVarGuard` forces the user to document that `foo` uses
// `recycled_`, and allows checking that there are never two function calls
// overlapping in time that use `recycled_`.
//
// class A {
//  public:
//   void foo() {
//     RecycledVarLock guard(recycled_guard_);
//     recycled_.clear();
//     // Use `recycled_` ...
//     //
//   }
//
//  private:
//   // A class variable that is only used by `foo`.
//   [[no_unique_address]] RecycledVarGuard recycled_guard_;
//   std::vector<T> recycled_ ABSL_GUARDED_BY(recycled_guard_);
// };
//
// `[[no_unique_address]]` is optional and allows `guard_` to take no space in
// debug mode.

#ifndef ORTOOLS_UTIL_RECYCLED_VAR_GUARD_H_
#define ORTOOLS_UTIL_RECYCLED_VAR_GUARD_H_

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"

namespace operations_research {

// A guard for recycled variables.
class ABSL_LOCKABLE ABSL_ATTRIBUTE_WARN_UNUSED RecycledVarGuard {
 public:
  RecycledVarGuard() = default;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() {
#ifndef NDEBUG
    DCHECK(!used_) << "recycled var is already being used";
    used_ = true;
#endif
  }

  void Unlock() ABSL_UNLOCK_FUNCTION() {
#ifndef NDEBUG
    used_ = false;
#endif
  }

 private:
#ifndef NDEBUG
  // TODO(user): Grab a stack trace to show where the first use was ?
  bool used_ = false;
#endif
};

// An RAII lock for recycled variable guards.
class ABSL_SCOPED_LOCKABLE RecycledVarLock {
 public:
  explicit RecycledVarLock(RecycledVarGuard& guard)
      ABSL_EXCLUSIVE_LOCK_FUNCTION(guard)
      : guard_(guard) {
    guard_.Lock();
  }
  ~RecycledVarLock() ABSL_UNLOCK_FUNCTION() { guard_.Unlock(); };

  RecycledVarLock(const RecycledVarLock&) = delete;
  RecycledVarLock& operator=(const RecycledVarLock&) = delete;
  RecycledVarLock(RecycledVarLock&&) = delete;
  RecycledVarLock& operator=(RecycledVarLock&&) = delete;

 private:
  RecycledVarGuard& guard_;
};
}  // namespace operations_research

#endif  // ORTOOLS_UTIL_RECYCLED_VAR_GUARD_H_
