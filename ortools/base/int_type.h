// Copyright 2010-2017 Google
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

// IntType is a simple template class mechanism for defining "logical"
// integer-like class types that support many of the same functionalities
// as native integer types, but which prevent assignment, construction, and
// other operations from other similar integer-like types.  Essentially, the
// template class IntType<IntTypeName, ValueType> (where ValueType assumes
// valid scalar types such as int, uint, int32, etc) has the additional
// property that it cannot be assigned to or constructed from other IntTypes
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
//    DEFINE_INT_TYPE(IntTypeName, ValueType);
//
//  where:
//    IntTypeName: is the desired (unique) name for the "logical" integer type.
//    ValueType: is one of the integral types:
//       (int, int64, short, long, char, unsigned version).
//
// DISALLOWED OPERATIONS / TYPE-SAFETY ENFORCEMENT -----------------------------
//
//  Consider these definitions and variable declarations:
//    DEFINE_INT_TYPE(GlobalDocID, int64);
//    DEFINE_INT_TYPE(LocalDocID, int64);
//    GlobalDocID global;
//    LocalDocID local;
//
//  The class IntType prevents:
//
//  1) Assignments of other IntTypes with different IntTypeNames.
//
//    global = local;                  <-- Fails to compile!
//    local = global;                  <-- Fails to compile!
//
//  2) Explicit/implicit conversion from an IntType to another IntType.
//
//    LocalDocID l(global);            <-- Fails to compile!
//    LocalDocID l = global;           <-- Fails to compile!
//
//    void GetGlobalDoc(GlobalDocID global) { }
//    GetGlobalDoc(global);            <-- Compiles fine, types match!
//    GetGlobalDoc(local);             <-- Fails to compile!
//
//  3) Implicit conversion from an IntType to a native integer type.
//
//    void GetGlobalDoc(int64 global) { ...
//    GetGlobalDoc(global);            <-- Fails to compile!
//    GetGlobalDoc(local);             <-- Fails to compile!
//
//    void GetLocalDoc(int32 local) { ...
//    GetLocalDoc(global);            <-- Fails to compile!
//    GetLocalDoc(local);             <-- Fails to compile!
//
//
// SUPPORTED OPERATIONS --------------------------------------------------------
//
// The following operators are supported: unary: ++ (both prefix and postfix),
// +, -, ! (logical not), ~ (one's complement); comparison: ==, !=, <, <=, >,
// >=; numerical: +, -, *, /; assignment: =, +=, -=, /=, *=; stream: <<. Each
// operator allows the same IntTypeName and the ValueType to be used on
// both left- and right-hand sides.
//
// It also supports an accessor value() returning the stored value as ValueType,
// and a templatized accessor value<T>() method that serves as syntactic sugar
// for static_cast<T>(var.value()).  These accessors are useful when assigning
// the stored value into protocol buffer fields and using it as printf args.
//
// The class also defines a hash functor that allows the IntType to be used
// as key to hashable containers such as std::unordered_{map|set}.
//
// We suggest using the IntTypeIndexedContainer wrapper around STL
// std::vector (see int_type_indexed_std::vector.h) if an IntType is intended
// to be used as an index into these containers.  These wrappers are
// indexed in a type-safe manner using IntTypes to ensure type-safety.
//
// NB: this implementation does not attempt to abide by or enforce dimensional
// analysis on these scalar types.
//
// EXAMPLES --------------------------------------------------------------------
//
//    DEFINE_INT_TYPE(GlobalDocID, int64);
//    GlobalDocID global = 3;
//    cout << global;                      <-- Prints 3 to stdout.
//
//    for (GlobalDocID i(0); i < global; ++i) {
//      cout << i;
//    }                                    <-- Print(ln)s 0 1 2 to stdout
//
//    DEFINE_INT_TYPE(LocalDocID, int64);
//    LocalDocID local;
//    cout << local;                       <-- Prints 0 to stdout it default
//                                             initializes the value to 0.
//
//    local = 5;
//    local *= 2;
//    LocalDocID l(local);
//    cout << l + local;                   <-- Prints 20 to stdout.
//
//    GenericSearchRequest request;
//    request.set_doc_id(global.value());  <-- Uses value() to extract the value
//                                             from the IntType class.
//
// REMARKS ---------------------------------------------------------------------
//
// The following bad usage is permissible although discouraged.  Essentially, it
// involves using the value*() accessors to extract the native integer type out
// of the IntType class.  Keep in mind that the primary reason for the IntType
// class is to prevent *accidental* mingling of similar logical integer types --
// and not type casting from one type to another.
//
//  DEFINE_INT_TYPE(GlobalDocID, int64);
//  DEFINE_INT_TYPE(LocalDocID, int64);
//  GlobalDocID global;
//  LocalDocID local;
//
//  global = local.value();                       <-- Compiles fine.
//
//  void GetGlobalDoc(GlobalDocID global) { ...
//  GetGlobalDoc(local.value());                  <-- Compiles fine.
//
//  void GetGlobalDoc(int64 global) { ...
//  GetGlobalDoc(local.value());                  <-- Compiles fine.

#ifndef OR_TOOLS_BASE_INT_TYPE_H_
#define OR_TOOLS_BASE_INT_TYPE_H_

#include <stddef.h>
#include <unordered_map>
#include <iosfwd>
#include <ostream>  // NOLINT

#include "ortools/base/macros.h"

template <typename IntTypeName, typename _ValueType>
class IntType;

// Defines the IntType using value_type and typedefs it to int_type_name.
// The struct int_type_name ## _tag_ trickery is needed to ensure that a new
// type is created per int_type_name.
#define DEFINE_INT_TYPE(int_type_name, value_type) \
  struct int_type_name##_tag_ {};                  \
  typedef IntType<int_type_name##_tag_, value_type> int_type_name;

// Holds a integral value (of type ValueType) and behaves as a
// ValueType by exposing assignment, unary, comparison, and arithmetic
// operators.
//
// The template parameter IntTypeName defines the name for the int type and must
// be unique within a binary (the convenient DEFINE_INT_TYPE macro at the end of
// the file generates a unique IntTypeName).  The parameter ValueType defines
// the integer type value (see supported list above).
//
// This class is NOT thread-safe.
template <typename IntTypeName, typename _ValueType>
class IntType {
 public:
  typedef _ValueType ValueType;                      // for non-member operators
  typedef IntType<IntTypeName, ValueType> ThisType;  // Syntactic sugar.

 public:
  // Default c'tor initializing value_ to 0.
  IntType() : value_(0) {}
  // C'tor explicitly initializing from a ValueType.
  explicit IntType(ValueType value) : value_(value) {}
  // IntType uses the default copy constructor and destructor.

  // -- ACCESSORS --------------------------------------------------------------
  // The class provides a value() accessor returning the stored ValueType value_
  // as well as a templatized accessor that is just a syntactic sugar for
  // static_cast<T>(var.value());
  ValueType value() const { return value_; }

  template <typename ValType>
  ValType value() const {
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

  bool operator!() const { return value_ == 0; }
  const ThisType operator+() const { return ThisType(value_); }
  const ThisType operator-() const { return ThisType(-value_); }
  const ThisType operator~() const { return ThisType(~value_); }

// -- ASSIGNMENT OPERATORS ---------------------------------------------------
// We support the following assignment operators: =, +=, -=, *=, /=, <<=, >>=
// and %= for both ThisType and ValueType.
#define INT_TYPE_ASSIGNMENT_OP(op)                   \
  ThisType& operator op(const ThisType& arg_value) { \
    value_ op arg_value.value();                     \
    return *this;                                    \
  }                                                  \
  ThisType& operator op(ValueType arg_value) {       \
    value_ op arg_value;                             \
    return *this;                                    \
  }
  INT_TYPE_ASSIGNMENT_OP(=);
  INT_TYPE_ASSIGNMENT_OP(+=);
  INT_TYPE_ASSIGNMENT_OP(-=);
  INT_TYPE_ASSIGNMENT_OP(*=);
  INT_TYPE_ASSIGNMENT_OP(/=);
  INT_TYPE_ASSIGNMENT_OP(<<=);  // NOLINT
  INT_TYPE_ASSIGNMENT_OP(>>=);  // NOLINT
  INT_TYPE_ASSIGNMENT_OP(%=);
#undef INT_TYPE_ASSIGNMENT_OP

 private:
  // The integer value of type ValueType.
  ValueType value_;
};

// -- NON-MEMBER STREAM OPERATORS ----------------------------------------------
// We provide the << operator, primarily for logging purposes.  Currently, there
// seems to be no need for an >> operator.
template <typename IntTypeName, typename ValueType>
std::ostream& operator<<(std::ostream& os,  // NOLINT
                         IntType<IntTypeName, ValueType> arg) {
  return os << arg.value();
}

// -- NON-MEMBER ARITHMETIC OPERATORS ------------------------------------------
// We support only the +, -, *, and / operators with the same IntType and
// ValueType types.  The reason is to allow simple manipulation on these IDs
// when used as indices in std::vectors and arrays.
//
// NB: Although it is possible to do IntType * IntType and IntType / IntType,
// it is probably non-sensical from a dimensionality analysis perspective.
#define INT_TYPE_ARITHMETIC_OP(op)                                   \
  template <typename IntTypeName, typename ValueType>                \
  static inline IntType<IntTypeName, ValueType> operator op(         \
      IntType<IntTypeName, ValueType> id_1,                          \
      IntType<IntTypeName, ValueType> id_2) {                        \
    id_1 op## = id_2.value();                                        \
    return id_1;                                                     \
  }                                                                  \
  template <typename IntTypeName, typename ValueType>                \
  static inline IntType<IntTypeName, ValueType> operator op(         \
      IntType<IntTypeName, ValueType> id,                            \
      typename IntType<IntTypeName, ValueType>::ValueType arg_val) { \
    id op## = arg_val;                                               \
    return id;                                                       \
  }                                                                  \
  template <typename IntTypeName, typename ValueType>                \
  static inline IntType<IntTypeName, ValueType> operator op(         \
      typename IntType<IntTypeName, ValueType>::ValueType arg_val,   \
      IntType<IntTypeName, ValueType> id) {                          \
    IntType<IntTypeName, ValueType> obj(arg_val);                    \
    obj op## = id.value();                                           \
    return obj;                                                      \
  }
INT_TYPE_ARITHMETIC_OP(+);
INT_TYPE_ARITHMETIC_OP(-);
INT_TYPE_ARITHMETIC_OP(*);
INT_TYPE_ARITHMETIC_OP(/);
INT_TYPE_ARITHMETIC_OP(<<);  // NOLINT
INT_TYPE_ARITHMETIC_OP(>>);  // NOLINT
INT_TYPE_ARITHMETIC_OP(%);
#undef INT_TYPE_ARITHMETIC_OP

// -- NON-MEMBER COMPARISON OPERATORS ------------------------------------------
// Static inline comparison operators.  We allow all comparison operators among
// the following types (OP \in [==, !=, <, <=, >, >=]:
//   IntType<IntTypeName, ValueType> OP IntType<IntTypeName, ValueType>
//   IntType<IntTypeName, ValueType> OP ValueType
//   ValueType OP IntType<IntTypeName, ValueType>
#define INT_TYPE_COMPARISON_OP(op)                                       \
  template <typename IntTypeName, typename ValueType>                    \
  static inline bool operator op(IntType<IntTypeName, ValueType> id_1,   \
                                 IntType<IntTypeName, ValueType> id_2) { \
    return id_1.value() op id_2.value();                                 \
  }                                                                      \
  template <typename IntTypeName, typename ValueType>                    \
  static inline bool operator op(                                        \
      IntType<IntTypeName, ValueType> id,                                \
      typename IntType<IntTypeName, ValueType>::ValueType val) {         \
    return id.value() op val;                                            \
  }                                                                      \
  template <typename IntTypeName, typename ValueType>                    \
  static inline bool operator op(                                        \
      typename IntType<IntTypeName, ValueType>::ValueType val,           \
      IntType<IntTypeName, ValueType> id) {                              \
    return val op id.value();                                            \
  }
INT_TYPE_COMPARISON_OP(==);  // NOLINT
INT_TYPE_COMPARISON_OP(!=);  // NOLINT
INT_TYPE_COMPARISON_OP(<);   // NOLINT
INT_TYPE_COMPARISON_OP(<=);  // NOLINT
INT_TYPE_COMPARISON_OP(>);   // NOLINT
INT_TYPE_COMPARISON_OP(>=);  // NOLINT
#undef INT_TYPE_COMPARISON_OP

namespace std {
template <typename IntTypeName, typename ValueType>
struct hash<IntType<IntTypeName, ValueType> > {
  size_t operator()(const IntType<IntTypeName, ValueType>& idx) const {
    return hash<ValueType>()(idx.value());
  }
};
}  // namespace std

#endif  // OR_TOOLS_BASE_INT_TYPE_H_
