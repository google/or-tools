// Copyright 2010-2022 Google LLC
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

// Generates strongly typed integer types.
//
// StrongIndex is a simple template class mechanism for defining "logical"
// index-like class types that support some of the same functionalities
// as int, but which prevent assignment, construction, and
// other operations from other similar integer-like types.  Essentially, the
// template class StrongIndex<StrongIndexName> has the additional
// property that it cannot be assigned to or constructed from another
// StrongIndex with a different StrongIndexName.
//
// Usage
//    DEFINE_STRONG_INDEX_TYPE(name);
//  where name is the desired (unique) name for the "logical" index type.
//
// StrongInt64 is a more general strong integer class based on int64_t.
// It has the same general type safeness, and supports more integer operators.
//
// Usage
//    DEFINE_STRONG_INT64_TYPE(name);
//  where name is the desired (unique) name for the "logical" int64_t type.
//
// SUPPORTED OPERATIONS --------------------------------------------------------
//
// The StrongIndex type is limited and only supports following operators are
// supported: unary: ++ (both prefix and postfix), comparison: ==, !=, <, <=, >,
// >=; assignment: =, +=, -=,; stream: <<. Each operator allows the same
// StrongIndexName and the int to be used on both left- and right-hand sides.
//
// The StrongInt64 type supports all integer operators across StrongInt64
// with the same StrongIntegerName and int64_t.
//
// Both support an accessor value() returning the stored value.
//
// The classes also define hash functors that allows the strong types to be used
// as key to hashable containers.

#ifndef OR_TOOLS_UTIL_STRONG_INTEGERS_H_
#define OR_TOOLS_UTIL_STRONG_INTEGERS_H_

#include <stddef.h>

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <ostream>  // NOLINT
#include <type_traits>

#include "absl/base/port.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

// Defines the StrongIndex and typedefs it to index_type_name.
//
// Note: The struct index_type_name ## _index_tag_ trickery is needed to ensure
// that a new type is created per index_type_name.
#define DEFINE_STRONG_INDEX_TYPE(index_type_name)                              \
  struct index_type_name##_index_tag_ {                                        \
    static constexpr absl::string_view TypeName() { return #index_type_name; } \
  };                                                                           \
  typedef ::operations_research::StrongIndex<index_type_name##_index_tag_>     \
      index_type_name;

// Defines the StrongInt64 and typedefs it to integer_type_name.
//
// Note: The struct integer_type_name ## _integer_tag_ trickery is needed to
// ensure that a new type is created per integer_type_name.
#define DEFINE_STRONG_INT64_TYPE(integer_type_name)                            \
  struct integer_type_name##_integer_tag_ {                                    \
    static constexpr absl::string_view TypeName() {                            \
      return #integer_type_name;                                               \
    }                                                                          \
  };                                                                           \
  typedef ::operations_research::StrongInt64<integer_type_name##_integer_tag_> \
      integer_type_name;

// ----------- Implementation ------------

// Note: we need two classes as it is the only way to have different set of
// operators on each class.
// Note: We use to class to easily define a different set of operators for the
// index and int64_t type.

#define STRONG_ASSIGNMENT_OP(StrongClass, IntType, op) \
  ThisType& operator op(const ThisType& arg_value) {   \
    value_ op arg_value.value();                       \
    return *this;                                      \
  }                                                    \
  ThisType& operator op(IntType arg_value) {           \
    value_ op arg_value;                               \
    return *this;                                      \
  }

#define INCREMENT_AND_DECREMENT_OPERATORS \
  ThisType& operator++() {                \
    ++value_;                             \
    return *this;                         \
  }                                       \
  const ThisType operator++(int) {        \
    ThisType temp(*this);                 \
    ++value_;                             \
    return temp;                          \
  }                                       \
  ThisType& operator--() {                \
    --value_;                             \
    return *this;                         \
  }                                       \
  const ThisType operator--(int) {        \
    ThisType temp(*this);                 \
    --value_;                             \
    return temp;                          \
  }

// Holds an int value and behaves as an int by exposing assignment,
// unary, comparison, and arithmetic operators.
//
// The template parameter StrongIndexName defines the name for the int type and
// must be unique within a binary (the convenient DEFINE_STRONG_INDEX_TYPE macro
// at the start of the file generates a unique StrongIndexName).
//
// This class is NOT thread-safe.
template <typename StrongIndexName>
class StrongIndex {
 public:
  typedef int ValueType;                          // Needed for StrongVector.
  typedef StrongIndex<StrongIndexName> ThisType;  // Syntactic sugar.

  static constexpr absl::string_view TypeName() {
    return StrongIndexName::TypeName();
  }

  struct ABSL_DEPRECATED("Use absl::Hash instead") Hasher {
    size_t operator()(const StrongIndex& x) const {
      return static_cast<size_t>(x.value());
    }
  };

  constexpr StrongIndex() : value_(0) {}

  explicit constexpr StrongIndex(int value) : value_(value) {}

  StrongIndex& operator=(int arg_value) {
    value_ = arg_value;
    return *this;
  }

  // The class provides a value() accessor returning the stored int value_
  // as well as a templatized accessor that is just a syntactic sugar for
  // static_cast<T>(var.value());
  constexpr int value() const { return value_; }

  template <typename ValType>  // Needed for StrongVector.
  constexpr ValType value() const {
    return static_cast<ValType>(value_);
  }

  constexpr const ThisType operator+() const { return ThisType(value_); }
  constexpr const ThisType operator-() const { return ThisType(-value_); }

  INCREMENT_AND_DECREMENT_OPERATORS;

  STRONG_ASSIGNMENT_OP(StrongIndex, int, +=);
  STRONG_ASSIGNMENT_OP(StrongIndex, int, -=);

 private:
  int value_;
};

// Holds an int64_t value and behaves as an int64_t by exposing assignment,
// unary, comparison, and arithmetic operators.
//
// The template parameter StrongIntegerName defines the name for the int type
// and must be unique within a binary (the convenient DEFINE_STRONG_INTEGER_TYPE
// macro at the start of the file generates a unique StrongIntegerName).
//
// This class is NOT thread-safe.
template <typename StrongIntegerName>
class StrongInt64 {
 public:
  typedef int64_t ValueType;                        // Needed for StrongVector.
  typedef StrongInt64<StrongIntegerName> ThisType;  // Syntactic sugar.

  static constexpr absl::string_view TypeName() {
    return StrongIntegerName::TypeName();
  }

  struct ABSL_DEPRECATED("Use absl::Hash instead") Hasher {
    size_t operator()(const StrongInt64& x) const {
      return static_cast<size_t>(x.value());
    }
  };

  constexpr StrongInt64() : value_(0) {}

  // NOLINTBEGIN(google-explicit-constructor)
  constexpr StrongInt64(int64_t value) : value_(value) {}
  // NOLINTEND(google-explicit-constructor)

  StrongInt64& operator=(int64_t arg_value) {
    value_ = arg_value;
    return *this;
  }

  constexpr int64_t value() const { return value_; }

  template <typename ValType>  // Needed for StrongVector.
  constexpr ValType value() const {
    return static_cast<ValType>(value_);
  }

  INCREMENT_AND_DECREMENT_OPERATORS;

  constexpr const ThisType operator+() const { return ThisType(value_); }
  constexpr const ThisType operator-() const { return ThisType(-value_); }
  constexpr const ThisType operator~() const { return ThisType(~value_); }

  STRONG_ASSIGNMENT_OP(StrongInt64, int64_t, +=);
  STRONG_ASSIGNMENT_OP(StrongInt64, int64_t, -=);
  STRONG_ASSIGNMENT_OP(StrongInt64, int64_t, *=);
  STRONG_ASSIGNMENT_OP(StrongInt64, int64_t, /=);
  STRONG_ASSIGNMENT_OP(StrongInt64, int64_t, <<=);
  STRONG_ASSIGNMENT_OP(StrongInt64, int64_t, >>=);
  STRONG_ASSIGNMENT_OP(StrongInt64, int64_t, %=);

 private:
  int64_t value_;
};

#undef STRONG_ASSIGNMENT_OP
#undef INCREMENT_AND_DECREMENT_OPERATORS

// -- NON-MEMBER STREAM OPERATORS ----------------------------------------------
// We provide the << operator, primarily for logging purposes.  Currently, there
// seems to be no need for an >> operator.
template <typename StrongIndexName>
std::ostream& operator<<(std::ostream& os,  // NOLINT
                         StrongIndex<StrongIndexName> arg) {
  return os << arg.value();
}

template <typename StrongIntegerName>
std::ostream& operator<<(std::ostream& os,  // NOLINT
                         StrongInt64<StrongIntegerName> arg) {
  return os << arg.value();
}

// -- NON-MEMBER ARITHMETIC OPERATORS ------------------------------------------
#define STRONG_TYPE_ARITHMETIC_OP(StrongType, IntType, op)                    \
  template <typename StrongName>                                              \
  constexpr StrongType<StrongName> operator op(StrongType<StrongName> id_1,   \
                                               StrongType<StrongName> id_2) { \
    return StrongType<StrongName>(id_1.value() op id_2.value());              \
  }                                                                           \
  template <typename StrongName>                                              \
  constexpr StrongType<StrongName> operator op(StrongType<StrongName> id,     \
                                               IntType arg_val) {             \
    return StrongType<StrongName>(id.value() op arg_val);                     \
  }                                                                           \
  template <typename StrongName>                                              \
  constexpr StrongType<StrongName> operator op(IntType arg_val,               \
                                               StrongType<StrongName> id) {   \
    return StrongType<StrongName>(arg_val op id.value());                     \
  }

STRONG_TYPE_ARITHMETIC_OP(StrongIndex, int, +);
STRONG_TYPE_ARITHMETIC_OP(StrongIndex, int, -);
STRONG_TYPE_ARITHMETIC_OP(StrongIndex, int, *);
STRONG_TYPE_ARITHMETIC_OP(StrongIndex, int, %);

STRONG_TYPE_ARITHMETIC_OP(StrongInt64, int64_t, +);
STRONG_TYPE_ARITHMETIC_OP(StrongInt64, int64_t, -);
STRONG_TYPE_ARITHMETIC_OP(StrongInt64, int64_t, *);
STRONG_TYPE_ARITHMETIC_OP(StrongInt64, int64_t, /);
STRONG_TYPE_ARITHMETIC_OP(StrongInt64, int64_t, <<);
STRONG_TYPE_ARITHMETIC_OP(StrongInt64, int64_t, >>);
STRONG_TYPE_ARITHMETIC_OP(StrongInt64, int64_t, %);
#undef STRONG_TYPE_ARITHMETIC_OP

// -- NON-MEMBER COMPARISON OPERATORS ------------------------------------------
#define STRONG_TYPE_COMPARISON_OP(StrongType, IntType, op)                \
  template <typename StrongName>                                          \
  static inline constexpr bool operator op(StrongType<StrongName> id_1,   \
                                           StrongType<StrongName> id_2) { \
    return id_1.value() op id_2.value();                                  \
  }                                                                       \
  template <typename StrongName>                                          \
  static inline constexpr bool operator op(StrongType<StrongName> id,     \
                                           IntType val) {                 \
    return id.value() op val;                                             \
  }                                                                       \
  template <typename StrongName>                                          \
  static inline constexpr bool operator op(IntType val,                   \
                                           StrongType<StrongName> id) {   \
    return val op id.value();                                             \
  }

STRONG_TYPE_COMPARISON_OP(StrongIndex, int, ==);  // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongIndex, int, !=);  // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongIndex, int, <);   // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongIndex, int, <=);  // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongIndex, int, >);   // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongIndex, int, >=);  // NOLINT

STRONG_TYPE_COMPARISON_OP(StrongInt64, int64_t, ==);  // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongInt64, int64_t, !=);  // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongInt64, int64_t, <);   // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongInt64, int64_t, <=);  // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongInt64, int64_t, >);   // NOLINT
STRONG_TYPE_COMPARISON_OP(StrongInt64, int64_t, >=);  // NOLINT
#undef STRONG_TYPE_COMPARISON_OP

// -- ABSL HASHING SUPPORT -----------------------------------------------------
template <typename StrongIndexName, typename H>
H AbslHashValue(H h, const StrongIndex<StrongIndexName>& i) {
  return H::combine(std::move(h), i.value());
}

template <typename StrongIntegerName, typename H>
H AbslHashValue(H h, const StrongInt64<StrongIntegerName>& i) {
  return H::combine(std::move(h), i.value());
}

}  // namespace operations_research

// -- STD HASHING SUPPORT -----------------------------------------------------
namespace std {
template <typename Tag>
struct hash<operations_research::StrongIndex<Tag> >
    : ::operations_research::StrongIndex<Tag>::Hasher {};

template <typename Tag>
struct hash<operations_research::StrongInt64<Tag> >
    : ::operations_research::StrongInt64<Tag>::Hasher {};
}  // namespace std

#endif  // OR_TOOLS_UTIL_STRONG_INTEGERS_H_
