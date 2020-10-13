// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_CLEANUP_H_
#define OR_TOOLS_BASE_CLEANUP_H_

#include <utility>

#include "absl/base/macros.h"
#include "ortools/base/logging.h"

namespace absl {

namespace cleanup_internal {

template <typename Callback>
class Storage {
  using InvokeT = absl::base_internal::invoke_result_t<Callback>;
  static_assert(std::is_same<InvokeT, void>::value, "");
  static_assert(!std::is_reference<Callback>::value, "");

 public:
  Storage() : contains_callback_(false), callback_() {}

  Storage(Storage&& other_storage)
      : contains_callback_(other_storage.ContainsCallback()),
        callback_(other_storage.ReleaseCallback()) {}

  template <typename TheCallback>
  explicit Storage(TheCallback&& the_callback)
      : contains_callback_(true),
        callback_(std::forward<TheCallback>(the_callback)) {}

  template <typename OtherCallback>
  Storage(Storage<OtherCallback>&& other_storage)  // NOLINT
      : contains_callback_(other_storage.ContainsCallback()),
        callback_(other_storage.ReleaseCallback()) {}

  Storage& operator=(Storage&& other_storage) {
    if (ContainsCallback()) std::move(callback_)();
    contains_callback_ = other_storage.ContainsCallback();
    callback_ = other_storage.ReleaseCallback();
    return *this;
  }

  bool ContainsCallback() const { return contains_callback_; }

  Callback ReleaseCallback() {
    contains_callback_ = false;
    return std::move(callback_);
  }

  void CancelCallback() { contains_callback_ = false; }

  void InvokeCallback() {
    CancelCallback();

    std::move(callback_)();
  }

 private:
  bool contains_callback_;
  Callback callback_;
};

struct AccessStorage {
  template <template <typename> class Cleanup, typename Callback>
  static Storage<Callback>& From(Cleanup<Callback>& cleanup) {
    return cleanup.storage_;
  }
};

}  // namespace cleanup_internal

template <typename Callback>
class ABSL_MUST_USE_RESULT Cleanup {
  using Storage = cleanup_internal::Storage<Callback>;
  using AccessStorage = cleanup_internal::AccessStorage;

 public:
  Cleanup() = default;

  Cleanup(Cleanup&&) = default;

  template <typename TheCallback>
  explicit Cleanup(TheCallback&& the_callback)
      : storage_(std::forward<TheCallback>(the_callback)) {}

  template <typename OtherCallback>
  Cleanup(Cleanup<OtherCallback>&& other_cleanup)  // NOLINT
      : storage_(std::move(AccessStorage::From(other_cleanup))) {}

  ~Cleanup() {
    if (storage_.ContainsCallback()) storage_.InvokeCallback();
  }

  // Assignment to a cleanup object behaves like destroying it and making a new
  // one in its place (analogous to `std::unique_ptr<T>` semantics).
  Cleanup& operator=(Cleanup&&) = default;

  bool is_released() const { return !storage_.ContainsCallback(); }

 private:
  friend AccessStorage;

  Storage storage_;
};

template <int&... PreventExplicitTemplateArguments, typename Callback>
absl::Cleanup<absl::decay_t<Callback>> MakeCleanup(Callback&& callback) {
  return absl::Cleanup<absl::decay_t<Callback>>(
      std::forward<Callback>(callback));
}

}  // namespace absl

#endif  // OR_TOOLS_BASE_CLEANUP_H_
