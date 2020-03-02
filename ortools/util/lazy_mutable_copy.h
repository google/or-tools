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

#ifndef OR_TOOLS_UTIL_LAZY_MUTABLE_COPY_H_
#define OR_TOOLS_UTIL_LAZY_MUTABLE_COPY_H_

#include <memory>

#include "absl/memory/memory.h"

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
// At the call site: ProcessProto({const_ref_to_my_proto});
//
// In basic usage, a LazyMutableCopy is in one of two states:
// - original: points to the const original. No memory allocated.
// - copy: points to a mutable copy of the original and owns it. Owning the
//   copy means that the destructor will delete it, like std::unique_ptr<>.
//   This is what you get by calling get_mutable().
template <class T>
class LazyMutableCopy {
 public:
  // You always construct a LazyMutableCopy with a const reference to an object,
  // which must outlive this class (unless get_mutable() was called).
  LazyMutableCopy(const T& obj)  // NOLINT(google-explicit-constructor)
      : original_(&obj) {}

  // You can move a LazyMutableCopy, much like a std::unique_ptr<> or a const*.
  // We simply rely on the default move constructors being available.

  const T& get() const { return copy_ != nullptr ? *copy_ : *original_; }
  T* get_mutable() {
    if (copy_ == nullptr) {
      copy_ = absl::make_unique<T>(*original_);
      original_ = nullptr;
    }
    return copy_.get();
  }

  // True iff get_mutable() was called at least once (in which case the object
  // was copied).
  bool was_copied() const { return copy_ != nullptr; }

 private:
  const T* original_;
  std::unique_ptr<T> copy_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_LAZY_MUTABLE_COPY_H_
