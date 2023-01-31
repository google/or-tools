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

// StrongInt is a simple template class mechanism for defining "logical"
// integer-like class types that support many of the same functionalities
// as native integer types, but which prevent assignment, construction, and
// other operations from other similar integer-like types.  Essentially, the
// template class StrongInt<StrongIntName, ValueType> (where ValueType assumes
// valid scalar types such as int, uint, int32_t, etc) has the additional
// property that it cannot be assigned to or constructed from other StrongInts
// or native integer types of equal or implicitly convertible type.
//
// The class is useful for preventing mingling of integer variables with
// different logical roles or units.  Unfortunately, C++ provides relatively
// good type-safety for user-defined classes but not for integer types.  It is
// essentially up to the user to use nice variable names and comments to prevent
// accidental mismatches, such as confusing a user-index with a group-index or a
// time-in-milliseconds with a time-in-seconds.  The use of typedefs are limited
// in that regard as they do not enforce type-safety.
//
// USAGE -----------------------------------------------------------------------
//
//    DEFINE_STRONG_INT_TYPE(StrongIntName, ValueType);
//
//  where:
//    StrongIntName: is the desired (unique) name for the "logical" integer type
//    ValueType: is one of the integral types as defined by std::is_integral
//               (see <type_traits>).
//
// DISALLOWED OPERATIONS / TYPE-SAFETY ENFORCEMENT -----------------------------
//
//  Consider these definitions and variable declarations:
//    DEFINE_STRONG_INT_TYPE(GlobalDocID, int64_t);
//    DEFINE_STRONG_INT_TYPE(LocalDocID, int64_t);
//    GlobalDocID global;
//    LocalDocID local;
//
//  The class StrongInt prevents:
//
//  1) Assignments of other StrongInts with different StrongIntNames.
//
//    global = local;                  <-- Fails to compile!
//    local = global;                  <-- Fails to compile!
//
//  2) Explicit/implicit conversion from an StrongInt to another StrongInt.
//
//    LocalDocID l(global);            <-- Fails to compile!
//    LocalDocID l = global;           <-- Fails to compile!
//
//    void GetGlobalDoc(GlobalDocID global) { }
//    GetGlobalDoc(global);            <-- Compiles fine, types match!
//    GetGlobalDoc(local);             <-- Fails to compile!
//
//  3) Implicit conversion from an StrongInt to a native integer type.
//
//    void GetGlobalDoc(int64_t global) { ...
//    GetGlobalDoc(global);            <-- Fails to compile!
//    GetGlobalDoc(local);             <-- Fails to compile!
//
//    void GetLocalDoc(int32_t local) { ...
//    GetLocalDoc(global);             <-- Fails to compile!
//    GetLocalDoc(local);              <-- Fails to compile!
//
//
// SUPPORTED OPERATIONS --------------------------------------------------------
//
// The following operators are supported: unary: ++ (both prefix and postfix),
// +, -, ! (logical not), ~ (one's complement); comparison: ==, !=, <, <=, >,
// >=; numerical: +, -, *, /; assignment: =, +=, -=, /=, *=; stream: <<. Each
// operator allows the same StrongIntName and the ValueType to be used on
// both left- and right-hand sides.
//
// It also supports an accessor value() returning the stored value as ValueType,
// and a templatized accessor value<T>() method that serves as syntactic sugar
// for static_cast<T>(var.value()).  These accessors are useful when assigning
// the stored value into protocol buffer fields and using it as printf args.
//
// The class also defines a hash functor that allows the StrongInt to be used
// as key to hashable containers such as hash_map and hash_set.
//
// We suggest using the StrongIntIndexedContainer wrapper around google3's
// FixedArray and STL vector (see int-type-indexed-container.h) if an StrongInt
// is intended to be used as an index into these containers.  These wrappers are
// indexed in a type-safe manner using StrongInts to ensure type-safety.
//
// NB: this implementation does not attempt to abide by or enforce dimensional
// analysis on these scalar types.
//
// EXAMPLES --------------------------------------------------------------------
//
//    DEFINE_STRONG_INT_TYPE(GlobalDocID, int64_t);
//    GlobalDocID global = 3;
//    std::cout << global;                      <-- Prints 3 to stdout.
//
//    for (GlobalDocID i(0); i < global; ++i) {
//      std::cout << i;
//    }                                    <-- Print(ln)s 0 1 2 to stdout
//
//    DEFINE_STRONG_INT_TYPE(LocalDocID, int64_t);
//    LocalDocID local;
//    std::cout << local;                       <-- Prints 0 to stdout it
//    default
//                                             initializes the value to 0.
//
//    local = 5;
//    local *= 2;
//    LocalDocID l(local);
//    std::cout << l + local;                   <-- Prints 20 to stdout.
//
//    GenericSearchRequest request;
//    request.set_doc_id(global.value());  <-- Uses value() to extract the value
//                                             from the StrongInt class.
//
// REMARKS ---------------------------------------------------------------------
//
// The following bad usage is permissible although discouraged.  Essentially, it
// involves using the value*() accessors to extract the native integer type out
// of the StrongInt class.  Keep in mind that the primary reason for the
// StrongInt class is to prevent *accidental* mingling of similar logical
// integer types -- and not type casting from one type to another.
//
//  DEFINE_STRONG_INT_TYPE(GlobalDocID, int64_t);
//  DEFINE_STRONG_INT_TYPE(LocalDocID, int64_t);
//  GlobalDocID global;
//  LocalDocID local;
//
//  global = local.value();                       <-- Compiles fine.
//
//  void GetGlobalDoc(GlobalDocID global) { ...
//  GetGlobalDoc(local.value());                  <-- Compiles fine.
//
//  void GetGlobalDoc(int64_t global) { ...
//  GetGlobalDoc(local.value());                  <-- Compiles fine.

#ifndef OR_TOOLS_BASE_STRONG_INT_H_
#define OR_TOOLS_BASE_STRONG_INT_H_

#include <stddef.h>

#include <functional>
#include <iosfwd>
#include <ostream>  // NOLINT
#include <type_traits>

#include "absl/base/port.h"
#include "absl/strings/string_view.h"
#include "ortools/base/macros.h"

namespace util_intops {

template <typename StrongIntName, typename _ValueType>
class StrongInt;

// Defines the StrongInt using value_type and typedefs it to int_type_name.
// The struct int_type_name ## _tag_ trickery is needed to ensure that a new
// type is created per int_type_name.
#define DEFINE_STRONG_INT_TYPE(int_type_name, value_type)                    \
  struct int_type_name##_tag_ {                                              \
    static constexpr absl::string_view TypeName() { return #int_type_name; } \
  };                                                                         \
  typedef ::util_intops::StrongInt<int_type_name##_tag_, value_type>         \
      int_type_name;

// Holds a integral value (of type ValueType) and behaves as a
// ValueType by exposing assignment, unary, comparison, and arithmetic
// operators.
//
// The template parameter StrongIntName defines the name for the int type and
// must be unique within a binary (the convenient DEFINE_STRONG_INT macro at the
// end of the file generates a unique StrongIntName).  The parameter ValueType
// defines the integer type value (see supported list above).
//
// This class is NOT thread-safe.
template <typename StrongIntName, typename _ValueType>
class StrongInt {
 public:
  typedef _ValueType ValueType;  // for non-member operators
  typedef StrongInt<StrongIntName, ValueType> ThisType;  // Syntactic sugar.

  static constexpr absl::string_view TypeName() {
    return StrongIntName::TypeName();
  }

  // Note that this may change from time to time without notice.
  // See .
  struct Hasher {
    size_t operator()(const StrongInt& arg) const {
      return static_cast<size_t>(arg.value());
    }
  };

 public:
  // Default c'tor initializing value_ to 0.
  constexpr StrongInt() : value_(0) {}
  // C'tor explicitly initializing from a ValueType.
  constexpr explicit StrongInt(ValueType value) : value_(value) {}

  // StrongInt uses the default copy constructor, destructor and assign
  // operator. The defaults are sufficient and omitting them allows the compiler
  // to add the move constructor/assignment.

  // -- ACCESSORS --------------------------------------------------------------
  // The class provides a value() accessor returning the stored ValueType value_
  // as well as a templatized accessor that is just a syntactic sugar for
  // static_cast<T>(var.value());
  constexpr ValueType value() const { return value_; }

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

  constexpr bool operator!() const { return value_ == 0; }
  constexpr const ThisType operator+() const { return ThisType(value_); }
  constexpr const ThisType operator-() const { return ThisType(-value_); }
  constexpr const ThisType operator~() const { return ThisType(~value_); }

  // -- ASSIGNMENT OPERATORS ---------------------------------------------------
  // We support the following assignment operators: =, +=, -=, *=, /=, <<=, >>=
  // and %= for both ThisType and ValueType.
#define STRONG_INT_TYPE_ASSIGNMENT_OP(op)            \
  ThisType& operator op(const ThisType& arg_value) { \
    value_ op arg_value.value();                     \
    return *this;                                    \
  }                                                  \
  ThisType& operator op(ValueType arg_value) {       \
    value_ op arg_value;                             \
    return *this;                                    \
  }
  STRONG_INT_TYPE_ASSIGNMENT_OP(+=);
  STRONG_INT_TYPE_ASSIGNMENT_OP(-=);
  STRONG_INT_TYPE_ASSIGNMENT_OP(*=);
  STRONG_INT_TYPE_ASSIGNMENT_OP(/=);
  STRONG_INT_TYPE_ASSIGNMENT_OP(<<=);  // NOLINT
  STRONG_INT_TYPE_ASSIGNMENT_OP(>>=);  // NOLINT
  STRONG_INT_TYPE_ASSIGNMENT_OP(%=);
#undef STRONG_INT_TYPE_ASSIGNMENT_OP

  ThisType& operator=(ValueType arg_value) {
    value_ = arg_value;
    return *this;
  }

 private:
  // The integer value of type ValueType.
  ValueType value_;

  COMPILE_ASSERT(std::is_integral<ValueType>::value,
                 invalid_integer_type_for_id_type_);
} ABSL_ATTRIBUTE_PACKED;

// -- NON-MEMBER STREAM OPERATORS ----------------------------------------------
// We provide the << operator, primarily for logging purposes.  Currently, there
// seems to be no need for an >> operator.
template <typename StrongIntName, typename ValueType>
std::ostream& operator<<(std::ostream& os,  // NOLINT
                         StrongInt<StrongIntName, ValueType> arg) {
  return os << arg.value();
}

// -- NON-MEMBER ARITHMETIC OPERATORS ------------------------------------------
// We support only the +, -, *, and / operators with the same StrongInt and
// ValueType types.  The reason is to allow simple manipulation on these IDs
// when used as indices in vectors and arrays.
//
// NB: Although it is possible to do StrongInt * StrongInt and StrongInt /
// StrongInt, it is probably non-sensical from a dimensionality analysis
// perspective.
#define STRONG_INT_TYPE_ARITHMETIC_OP(op)                                     \
  template <typename StrongIntName, typename ValueType>                       \
  constexpr StrongInt<StrongIntName, ValueType> operator op(                  \
      StrongInt<StrongIntName, ValueType> id_1,                               \
      StrongInt<StrongIntName, ValueType> id_2) {                             \
    return StrongInt<StrongIntName, ValueType>(id_1.value() op id_2.value()); \
  }                                                                           \
  template <typename StrongIntName, typename ValueType>                       \
  constexpr StrongInt<StrongIntName, ValueType> operator op(                  \
      StrongInt<StrongIntName, ValueType> id,                                 \
      typename StrongInt<StrongIntName, ValueType>::ValueType arg_val) {      \
    return StrongInt<StrongIntName, ValueType>(id.value() op arg_val);        \
  }                                                                           \
  template <typename StrongIntName, typename ValueType>                       \
  constexpr StrongInt<StrongIntName, ValueType> operator op(                  \
      typename StrongInt<StrongIntName, ValueType>::ValueType arg_val,        \
      StrongInt<StrongIntName, ValueType> id) {                               \
    return StrongInt<StrongIntName, ValueType>(arg_val op id.value());        \
  }
STRONG_INT_TYPE_ARITHMETIC_OP(+);
STRONG_INT_TYPE_ARITHMETIC_OP(-);
STRONG_INT_TYPE_ARITHMETIC_OP(*);
STRONG_INT_TYPE_ARITHMETIC_OP(/);
STRONG_INT_TYPE_ARITHMETIC_OP(<<);  // NOLINT
STRONG_INT_TYPE_ARITHMETIC_OP(>>);  // NOLINT
STRONG_INT_TYPE_ARITHMETIC_OP(%);
#undef STRONG_INT_TYPE_ARITHMETIC_OP

// -- NON-MEMBER COMPARISON OPERATORS ------------------------------------------
// Static inline comparison operators.  We allow all comparison operators among
// the following types (OP \in [==, !=, <, <=, >, >=]:
//   StrongInt<StrongIntName, ValueType> OP StrongInt<StrongIntName, ValueType>
//   StrongInt<StrongIntName, ValueType> OP ValueType
//   ValueType OP StrongInt<StrongIntName, ValueType>
#define STRONG_INT_TYPE_COMPARISON_OP(op)                            \
  template <typename StrongIntName, typename ValueType>              \
  static inline constexpr bool operator op(                          \
      StrongInt<StrongIntName, ValueType> id_1,                      \
      StrongInt<StrongIntName, ValueType> id_2) {                    \
    return id_1.value() op id_2.value();                             \
  }                                                                  \
  template <typename StrongIntName, typename ValueType>              \
  static inline constexpr bool operator op(                          \
      StrongInt<StrongIntName, ValueType> id,                        \
      typename StrongInt<StrongIntName, ValueType>::ValueType val) { \
    return id.value() op val;                                        \
  }                                                                  \
  template <typename StrongIntName, typename ValueType>              \
  static inline constexpr bool operator op(                          \
      typename StrongInt<StrongIntName, ValueType>::ValueType val,   \
      StrongInt<StrongIntName, ValueType> id) {                      \
    return val op id.value();                                        \
  }
STRONG_INT_TYPE_COMPARISON_OP(==);  // NOLINT
STRONG_INT_TYPE_COMPARISON_OP(!=);  // NOLINT
STRONG_INT_TYPE_COMPARISON_OP(<);   // NOLINT
STRONG_INT_TYPE_COMPARISON_OP(<=);  // NOLINT
STRONG_INT_TYPE_COMPARISON_OP(>);   // NOLINT
STRONG_INT_TYPE_COMPARISON_OP(>=);  // NOLINT
#undef STRONG_INT_TYPE_COMPARISON_OP

// Support for-range loops. Enables easier looping over ranges of StrongInts,
// especially looping over sub-ranges of StrongVectors.
template <typename IntType>
class StrongIntRange {
 public:
  // Iterator over the indices.
  class StrongIntRangeIterator {
   public:
    using value_type = IntType;
    using difference_type = IntType;
    using reference = const IntType&;
    using pointer = const IntType*;
    using iterator_category = std::input_iterator_tag;

    explicit StrongIntRangeIterator(IntType initial) : current_(initial) {}
    bool operator!=(const StrongIntRangeIterator& other) const {
      return current_ != other.current_;
    }
    bool operator==(const StrongIntRangeIterator& other) const {
      return current_ == other.current_;
    }
    value_type operator*() const { return current_; }
    pointer operator->() const { return &current_; }
    StrongIntRangeIterator& operator++() {
      ++current_;
      return *this;
    }
    StrongIntRangeIterator operator++(int) {
      StrongIntRangeIterator old_iter = *this;
      ++current_;
      return old_iter;
    }

   private:
    IntType current_;
  };

  // Loops from IntType(0) up to (but not including) end.
  explicit StrongIntRange(IntType end) : begin_(IntType(0)), end_(end) {}
  // Loops from begin up to (but not including) end.
  StrongIntRange(IntType begin, IntType end) : begin_(begin), end_(end) {}
  StrongIntRangeIterator begin() const { return begin_; }
  StrongIntRangeIterator end() const { return end_; }

 private:
  const StrongIntRangeIterator begin_;
  const StrongIntRangeIterator end_;
};

template <typename IntType>
StrongIntRange<IntType> MakeStrongIntRange(IntType end) {
  return StrongIntRange<IntType>(end);
}

template <typename IntType>
StrongIntRange<IntType> MakeStrongIntRange(IntType begin, IntType end) {
  return StrongIntRange<IntType>(begin, end);
}
}  // namespace util_intops

// Allows it to be used as a key to hashable containers.
namespace std {
template <typename StrongIntName, typename ValueType>
struct hash<util_intops::StrongInt<StrongIntName, ValueType> >
    : util_intops::StrongInt<StrongIntName, ValueType>::Hasher {};
}  // namespace std

#endif  // OR_TOOLS_BASE_STRONG_INT_H_
