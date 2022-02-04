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

// StrongIndex is a simple template class mechanism for defining "logical"
// index-like class types that support some of the same functionalities
// as int, but which prevent assignment, construction, and
// other operations from other similar integer-like types.  Essentially, the
// template class StrongIndex<StrongIndexName> has the additional
// property that it cannot be assigned to or constructed from other
// StrongIndexs.
//
// USAGE -----------------------------------------------------------------------
//
//    DEFINE_STRONG_INDEX_TYPE(StrongIndexName);
//
//  where:
//    StrongIndexName: is the desired (unique) name for the "logical" integer
//    type.
//
// SUPPORTED OPERATIONS --------------------------------------------------------
//
// The following operators are supported: unary: ++ (both prefix and postfix),
// comparison: ==, !=, <, <=, >, >=; assignment: =, +=, -=,; stream: <<. Each
// operator allows the same StrongIndexName and the int to be used on
// both left- and right-hand sides.
//
// It also supports an accessor value() returning the stored value as int.
//
// The class also defines a hash functor that allows the StrongIndex to be used
// as key to hashable containers such as hash_map and hash_set.

#ifndef OR_TOOLS_UTIL_STRONG_INDEX_H_
#define OR_TOOLS_UTIL_STRONG_INDEX_H_

#include <stddef.h>

#include <functional>
#include <iosfwd>
#include <ostream>  // NOLINT
#include <type_traits>

#include "absl/base/port.h"
#include "absl/strings/string_view.h"
#include "ortools/base/macros.h"

namespace operations_research {

template <typename StrongIndexName>
class StrongIndex;

// Defines the StrongIndex and typedefs it to index_type_name.
// The struct index_type_name ## _tag_ trickery is needed to ensure that a new
// type is created per index_type_name.
#define DEFINE_STRONG_INDEX_TYPE(index_type_name)                              \
  struct index_type_name##_tag_ {                                              \
    static constexpr absl::string_view TypeName() { return #index_type_name; } \
  };                                                                           \
  typedef ::operations_research::StrongIndex<index_type_name##_tag_>           \
      index_type_name;

// Holds an int value and behaves as an int by exposing assignment,
// unary, comparison, and arithmetic operators.
//
// The template parameter StrongIndexName defines the name for the int type and
// must be unique within a binary (the convenient DEFINE_INDEX_TYPE macro at the
// end of the file generates a unique StrongIndexName).
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

  template <typename H>
  friend H AbslHashValue(H h, const StrongIndex& i) {
    return H::combine(std::move(h), i.value_);
  }

  struct ABSL_DEPRECATED("Use absl::Hash instead") Hasher {
    size_t operator()(const StrongIndex& x) const {
      return static_cast<size_t>(x.value());
    }
  };

  // Default c'tor initializing value_ to 0.
  constexpr StrongIndex() : value_(0) {}
  // C'tor explicitly initializing from a int.
  constexpr explicit StrongIndex(int value) : value_(value) {}

  // StrongIndex uses the default copy constructor, destructor and assign
  // operator. The defaults are sufficient and omitting them allows the compiler
  // to add the move constructor/assignment.

  // -- ACCESSORS --------------------------------------------------------------
  // The class provides a value() accessor returning the stored int value_
  // as well as a templatized accessor that is just a syntactic sugar for
  // static_cast<T>(var.value());
  constexpr int value() const { return value_; }

  template <typename ValType>
  constexpr ValType value() const {
    return static_cast<ValType>(value_);
  }  

  // -- UNARY OPERATORS --------------------------------------------------------
  ThisType& operator++() {  // prefix ++
    ++value_;
    return *this;
  }
  const ThisType operator++(int v) {  // postfix ++
    ThisType temp(*this);
    ++value_;
    return temp;
  }
  ThisType& operator--() {  // prefix --
    --value_;
    return *this;
  }
  const ThisType operator--(int v) {  // postfix --
    ThisType temp(*this);
    --value_;
    return temp;
  }

  constexpr const ThisType operator+() const { return ThisType(value_); }
  constexpr const ThisType operator-() const { return ThisType(-value_); }

  // -- ASSIGNMENT OPERATORS ---------------------------------------------------
  // We support the following assignment operators: =, +=, -=, for both
  // ThisType and int.
#define INDEX_TYPE_ASSIGNMENT_OP(op)                 \
  ThisType& operator op(const ThisType& arg_value) { \
    value_ op arg_value.value();                     \
    return *this;                                    \
  }                                                  \
  ThisType& operator op(int arg_value) {             \
    value_ op arg_value;                             \
    return *this;                                    \
  }
  INDEX_TYPE_ASSIGNMENT_OP(+=);
  INDEX_TYPE_ASSIGNMENT_OP(-=);
#undef INDEX_TYPE_ASSIGNMENT_OP

  StrongIndex& operator=(int arg_value) {
    value_ = arg_value;
    return *this;
  }

 private:
  // The integer value of type int.
  int value_;
};

// -- NON-MEMBER STREAM OPERATORS ----------------------------------------------
// We provide the << operator, primarily for logging purposes.  Currently, there
// seems to be no need for an >> operator.
template <typename StrongIndexName>
std::ostream& operator<<(std::ostream& os,  // NOLINT
                         StrongIndex<StrongIndexName> arg) {
  return os << arg.value();
}

// -- NON-MEMBER ARITHMETIC OPERATORS ------------------------------------------
// We support only the +, -, and * operators with the same StrongIndex and
// int types.  The reason is to allow simple manipulation on strong indices
// when used as indices in vectors and arrays.
#define INDEX_TYPE_ARITHMETIC_OP(op)                                          \
  template <typename StrongIndexName>                                         \
  constexpr StrongIndex<StrongIndexName> operator op(                         \
      StrongIndex<StrongIndexName> id_1, StrongIndex<StrongIndexName> id_2) { \
    return StrongIndex<StrongIndexName>(id_1.value() op id_2.value());        \
  }                                                                           \
  template <typename StrongIndexName>                                         \
  constexpr StrongIndex<StrongIndexName> operator op(                         \
      StrongIndex<StrongIndexName> id, int arg_val) {                         \
    return StrongIndex<StrongIndexName>(id.value() op arg_val);               \
  }                                                                           \
  template <typename StrongIndexName>                                         \
  constexpr StrongIndex<StrongIndexName> operator op(                         \
      int arg_val, StrongIndex<StrongIndexName> id) {                         \
    return StrongIndex<StrongIndexName>(arg_val op id.value());               \
  }
INDEX_TYPE_ARITHMETIC_OP(+);
INDEX_TYPE_ARITHMETIC_OP(-);
INDEX_TYPE_ARITHMETIC_OP(*);
#undef INDEX_TYPE_ARITHMETIC_OP

// -- NON-MEMBER COMPARISON OPERATORS ------------------------------------------
// Static inline comparison operators.  We allow all comparison operators among
// the following types (OP \in [==, !=, <, <=, >, >=]:
//   StrongIndex<StrongIndexName> OP StrongIndex<StrongIndexName>
//   StrongIndex<StrongIndexName> OP int
//   int OP StrongIndex<StrongIndexName>
#define INDEX_TYPE_COMPARISON_OP(op)                                          \
  template <typename StrongIndexName>                                         \
  static inline constexpr bool operator op(                                   \
      StrongIndex<StrongIndexName> id_1, StrongIndex<StrongIndexName> id_2) { \
    return id_1.value() op id_2.value();                                      \
  }                                                                           \
  template <typename StrongIndexName>                                         \
  static inline constexpr bool operator op(StrongIndex<StrongIndexName> id,   \
                                           int val) {                         \
    return id.value() op val;                                                 \
  }                                                                           \
  template <typename StrongIndexName>                                         \
  static inline constexpr bool operator op(int val,                           \
                                           StrongIndex<StrongIndexName> id) { \
    return val op id.value();                                                 \
  }
INDEX_TYPE_COMPARISON_OP(==);  // NOLINT
INDEX_TYPE_COMPARISON_OP(!=);  // NOLINT
INDEX_TYPE_COMPARISON_OP(<);   // NOLINT
INDEX_TYPE_COMPARISON_OP(<=);  // NOLINT
INDEX_TYPE_COMPARISON_OP(>);   // NOLINT
INDEX_TYPE_COMPARISON_OP(>=);  // NOLINT
#undef INDEX_TYPE_COMPARISON_OP

}  // namespace operations_research

// Allows StrongIndex to be used as a key to hashable containers.
namespace std {
template <typename Tag>
struct hash<operations_research::StrongIndex<Tag> >
    : ::operations_research::StrongIndex<Tag>::Hasher {};
}  // namespace std

#endif  // OR_TOOLS_UTIL_STRONG_INDEX_H_
