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

#ifndef OR_TOOLS_BASE_PROTO_ENUM_UTILS_H_
#define OR_TOOLS_BASE_PROTO_ENUM_UTILS_H_

// Provides utility functions that help with handling Protocol Buffer enums.
//
// Examples:
//
// A function to easily iterate over all defined values of an enum known at
// compile-time:
//
// for (Proto::Enum e : EnumerateEnumValues<Proto::Enum>()) {
//    ...
// }
//

#include <iterator>

#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/repeated_field.h"
namespace google::protobuf::contrib::utils {

using google::protobuf::GetEnumDescriptor;
using google::protobuf::RepeatedField;

template <typename E>
class ProtoEnumIterator;

template <typename E>
class EnumeratedProtoEnumView;

template <typename E>
bool operator==(const ProtoEnumIterator<E>& a, const ProtoEnumIterator<E>& b);

template <typename E>
bool operator!=(const ProtoEnumIterator<E>& a, const ProtoEnumIterator<E>& b);

// Generic Proto enum iterator.
template <typename E>
class ProtoEnumIterator {
 public:
  typedef E value_type;
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef E* pointer;
  typedef E& reference;

  ProtoEnumIterator() : current_(0) {}

  ProtoEnumIterator(const ProtoEnumIterator& other)
      : current_(other.current_) {}

  ProtoEnumIterator& operator=(const ProtoEnumIterator& other) {
    current_ = other.current_;
    return *this;
  }

  ProtoEnumIterator operator++(int) {
    ProtoEnumIterator other(*this);
    ++(*this);
    return other;
  }

  ProtoEnumIterator& operator++() {
    ++current_;
    return *this;
  }

  E operator*() const {
    return static_cast<E>(GetEnumDescriptor<E>()->value(current_)->number());
  }

 private:
  explicit ProtoEnumIterator(int current) : current_(current) {}

  int current_;

  // Only EnumeratedProtoEnumView can instantiate ProtoEnumIterator.
  friend class EnumeratedProtoEnumView<E>;
  friend bool operator==
      <>(const ProtoEnumIterator<E>& a, const ProtoEnumIterator<E>& b);
  friend bool operator!=
      <>(const ProtoEnumIterator<E>& a, const ProtoEnumIterator<E>& b);
};

template <typename E>
bool operator==(const ProtoEnumIterator<E>& a, const ProtoEnumIterator<E>& b) {
  return a.current_ == b.current_;
}

template <typename E>
bool operator!=(const ProtoEnumIterator<E>& a, const ProtoEnumIterator<E>& b) {
  return a.current_ != b.current_;
}

template <typename E>
class EnumeratedProtoEnumView {
 public:
  typedef E value_type;
  typedef ProtoEnumIterator<E> iterator;
  iterator begin() const { return iterator(0); }
  iterator end() const {
    return iterator(GetEnumDescriptor<E>()->value_count());
  }
};

// Returns an EnumeratedProtoEnumView that can be iterated over:
// for (Proto::Enum e : EnumerateEnumValues<Proto::Enum>()) {
//    ...
// }
template <typename E>
EnumeratedProtoEnumView<E> EnumerateEnumValues() {
  return EnumeratedProtoEnumView<E>();
}

// Returns a view that allows to iterate directly over the enum values
// in an enum repeated field, wrapping the repeated field with a type-safe
// iterator that provides access to the enum values.
//
// for (Enum enum :
//          REPEATED_ENUM_ADAPTER(message, repeated_enum_field)) {
//   ...
// }
//
// It provides greater safety than iterating over the enum directly, as the
// following will fail to type-check:
//
// .proto
// RightEnum enum = 5;
//
// client .cc
// for (WrongEnum e : REPEATED_ENUM_ADAPTER(proto, enum)) { <- Error: Cannot
//                                                             cast from
//                                                             RightEnum to
//                                                             WrongEnum
// }
//
// NOTE: As per http://shortn/_CYfjpruK6N, unrecognized enum values are treated
// differently between proto2 and proto3.
//
// For proto2, they are stripped out from the message when read, so all
// unrecognized enum values from the wire format will be skipped when iterating
// over the wrapper (this is the same behavior as iterating over the
// RepeatedField<int> directly).
//
// For proto3, they are left as-is, so unrecognized enum values from the wire
// format will still be returned when iterating over the wrapper (this is the
// same behavior as iterating over the RepeatedField<int> directly).
//
#define REPEATED_ENUM_ADAPTER(var, field)                       \
  google::protobuf::contrib::utils::internal::RepeatedEnumView< \
      decltype(var.field(0))>(var.field())

// ==== WARNING TO USERS ====
// Below are internal implementations, not public API, and may change without
// notice. Do NOT use directly.

namespace internal {

// Implementation for REPEATED_ENUM_ADAPTER. This does not provide type safety
// thus should be used through REPEATED_ENUM_ADAPTER only. See cr/246914845 for
// context.
template <typename E>
class RepeatedEnumView {
 public:
  class Iterator
#if __cplusplus < 201703L
      : public std::iterator<std::input_iterator_tag, E>
#endif
  {
   public:
    using difference_type = ptrdiff_t;
    using value_type = E;
#if __cplusplus >= 201703L
    using iterator_category = std::input_iterator_tag;
    using pointer = E*;
    using reference = E&;
#endif
    explicit Iterator(RepeatedField<int>::const_iterator ptr) : ptr_(ptr) {}
    bool operator==(const Iterator& it) const { return ptr_ == it.ptr_; }
    bool operator!=(const Iterator& it) const { return ptr_ != it.ptr_; }
    Iterator& operator++() {
      ++ptr_;
      return *this;
    }
    E operator*() const { return static_cast<E>(*ptr_); }

   private:
    RepeatedField<int>::const_iterator ptr_;
  };

  explicit RepeatedEnumView(const RepeatedField<int>& repeated_field)
      : repeated_field_(repeated_field) {}

  Iterator begin() const { return Iterator(repeated_field_.begin()); }
  Iterator end() const { return Iterator(repeated_field_.end()); }

 private:
  const RepeatedField<int>& repeated_field_;
};

}  // namespace internal

}  // namespace google::protobuf::contrib::utils

#endif  // OR_TOOLS_BASE_PROTO_ENUM_UTILS_H_
