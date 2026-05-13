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

#ifndef OR_TOOLS_UTIL_LAZY_MUTABLE_COPY_H_
#define OR_TOOLS_UTIL_LAZY_MUTABLE_COPY_H_

#include <memory>

namespace operations_research {

// LazyMutableCopy<T> is a helper class for making an on-demand copy of an
// object of arbitrary type T. Type T must have a copy constructor.
//
// Sample usage:
//   const Proto& original_input = ...;
//   LazyMutableCopy<Proto> input(original_input);
//   if (input.get().foo() == BAD_VALUE) {
//     input.get_mutable()->set_foo(GOOD_VALUE);  // Copies the object.
//   }
//   // Process "input" here without worrying about BAD_VALUE.
// A good pattern is to have function taking LazyMutableCopy<> as argument:
// void ProcessProto(LazyMutableCopy<Proto> input) {  // pass by copy
//   ...
// }
// At the call site: ProcessProto(const_ref_to_my_proto);
//
// In basic usage, a LazyMutableCopy is in one of two states:
// - original: points to the const original. No memory allocated.
// - copy: points to a mutable copy of the original and owns it. Owning the
//   copy means that the destructor will delete it, like std::unique_ptr<>.
//   This is what you get by calling get_mutable() or constructing it with
//   a move.
template <class T>
class LazyMutableCopy {
 public:
  // You can construct a LazyMutableCopy with a const reference to an object,
  // which must outlive this class (unless get_mutable() was called).
  LazyMutableCopy(const T& obj)  // NOLINT(google-explicit-constructor)
      : ptr_(&obj) {}

  // The other option is to construct a LazyMutableCopy with a std::move(T).
  // In this case you transfer ownership and you can mutate it for free.
  LazyMutableCopy(T&& obj)  // NOLINT(google-explicit-constructor)
      : copy_(std::make_unique<T>(std::move(obj))), ptr_(copy_.get()) {}

  // You can move a LazyMutableCopy but not copy it, much like a
  // std::unique_ptr<>.
  LazyMutableCopy(LazyMutableCopy&&) = default;
  LazyMutableCopy(const LazyMutableCopy&) = delete;
  class LazyMutableCopy<T>& operator=(LazyMutableCopy<T>&&) = default;
  class LazyMutableCopy<T>& operator=(const LazyMutableCopy<T>&) = delete;

  // This will copy the object if we don't already have ownership.
  T* get_mutable() {
    if (copy_ == nullptr && ptr_ != nullptr) {
      copy_ = std::make_unique<T>(*ptr_);
      ptr_ = copy_.get();
    }
    return copy_.get();
  }

  // Lazily make a copy if not already done and transfer ownership from this
  // class to the returned std::unique_ptr<T>. Calling this function leaves the
  // class in a state where the only valid operations is to assign it a new
  // value.
  //
  // We force a call via
  // std::move(lazy_mutable_copy).copy_or_move_as_unique_ptr() to make it
  // clearer that lazy_mutable_copy shouldn't really be used after this.
  std::unique_ptr<T> copy_or_move_as_unique_ptr() && {
    if (copy_ == nullptr && ptr_ != nullptr) {
      std::unique_ptr<T> result = std::make_unique<T>(*ptr_);
      ptr_ = nullptr;
      return result;
    }
    ptr_ = nullptr;
    return std::move(copy_);
  }

  // True iff get_mutable() was called at least once (in which case the object
  // was copied) or if we constructed this via std::move().
  bool has_ownership() const { return copy_ != nullptr; }

  // Standard smart pointer accessor, but only for const purpose.
  // Undefined if the class contains no object.
  const T* get() const { return ptr_; }
  const T& operator*() const { return *ptr_; }
  const T* operator->() const { return ptr_; }

  // Destroys any owned value. Calling this function leaves the class in a state
  // where the only valid operations is to assign it a new value.
  //
  // We force a call via std::move(lazy_mutable_copy).dispose() to make it
  // clearer that lazy_mutable_copy shouldn't really be used after this.
  void dispose() && {
    ptr_ = nullptr;
    copy_ = nullptr;
  }

 private:
  std::unique_ptr<T> copy_;
  const T* ptr_ = nullptr;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_LAZY_MUTABLE_COPY_H_
