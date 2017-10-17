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

// This file provides the ITIVector container that wraps around the STL
// std::vector.
// The wrapper restricts indexing to a pre-specified type-safe integer type or
// IntType (see base/int_type.h).  It prevents accidental indexing
// by different "logical" integer-like types (e.g.  another IntType) or native
// integer types.  The wrapper is useful as C++ and the standard template
// library allow the user to mix "logical" integral indices that might have a
// different role.
//
// The container can only be indexed by an instance of an IntType class, which
// can be declared as:
//
//     DEFINE_INT_TYPE(IntTypeName, IntTypeValueType);
//
// where IntTypeName is the desired name for the "logical" integer-like type
// and the ValueType is a supported native integer type such as int or
// uint64 (see base/int_type.h for details).
//
// The wrapper exposes all public methods of STL std::vector and behaves mostly
// as
// a pass-through. The only method modified to ensure type-safety is the
// operator [] and the at() method.
//
// EXAMPLES --------------------------------------------------------------------
//
//    DEFINE_INT_TYPE(PhysicalChildIndex, int32);
//    ITIVector<PhysicalChildIndex, ChildStats*> vec;
//
//    PhysicalChildIndex physical_index;
//    vec[physical_index] = ...;        <-- index type match: compiles properly.
//    vec.at(physical_index) = ...;     <-- index type match: compiles properly.
//
//    int32 physical_index;
//    vec[physical_index] = ...;        <-- fails to compile.
//    vec.at(physical_index) = ...;     <-- fails to compile.
//
//    DEFINE_INT_TYPE(LogicalChildIndex, int32);
//    int32 logical_index;
//    vec[logical_index] = ...;        <-- fails to compile.
//    vec.at(logical_index) = ...;     <-- fails to compile.
//
// N.B.: Since the iterators are not wrapped themselves, it's possible
// (but certainly not recommended) to perform arithmetic on them:
//
//    *(vec.begin() + 0) = ...;

#ifndef OR_TOOLS_BASE_INT_TYPE_INDEXED_VECTOR_H_
#define OR_TOOLS_BASE_INT_TYPE_INDEXED_VECTOR_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"

// STL std::vector
// ------------------------------------------------------------------
template <typename IntType, typename T, typename Alloc = std::allocator<T> >
class ITIVector : protected std::vector<T, Alloc> {
 public:
  typedef std::vector<T, Alloc> ParentType;
  typedef typename ParentType::size_type size_type;
  typedef typename ParentType::allocator_type allocator_type;
  typedef typename ParentType::value_type value_type;
  typedef typename ParentType::reference reference;
  typedef typename ParentType::const_reference const_reference;
  typedef typename ParentType::pointer pointer;
  typedef typename ParentType::const_pointer const_pointer;
  typedef typename ParentType::iterator iterator;
  typedef typename ParentType::const_iterator const_iterator;
  typedef typename ParentType::reverse_iterator reverse_iterator;
  typedef typename ParentType::const_reverse_iterator const_reverse_iterator;

 public:
  ITIVector() {}
  explicit ITIVector(const allocator_type& a) : ParentType(a) {}
  ITIVector(size_type n, const value_type& v = value_type(),
            const allocator_type& a = allocator_type())
      : ParentType(n, v, a) {}
  ITIVector(const ITIVector& x) : ParentType(x.get()) {}
  template <typename InputIteratorType>
  ITIVector(InputIteratorType first, InputIteratorType last,
            const allocator_type& a = allocator_type())
      : ParentType(first, last, a) {}
  ~ITIVector() {}

  // -- Accessors --------------------------------------------------------------
  // This const accessor is useful in defining the comparison operators below.
  const ParentType& get() const { return *this; }
  // The mutable accessor is useful when using auxiliar methods relying on
  // std::vector parameters such as JoinUsing(), strings::Split(), etc.  Methods
  // relying solely on iterators (e.g. STLDeleteElements) should work just fine
  // without the need for mutable_get().  NB: It should be used only in this
  // case and thus should not be abused to index the underlying std::vector
  // without
  // the appropriate IntType.
  ParentType* mutable_get() { return this; }

  // -- Modified methods -------------------------------------------------------
  reference operator[](IntType i) {
    return ParentType::operator[](i.template value<size_t>());
  }
  const_reference operator[](IntType i) const {
    return ParentType::operator[](i.template value<size_t>());
  }
  reference at(IntType i) { return ParentType::at(i.template value<size_t>()); }
  const_reference at(IntType i) const {
    return ParentType::at(i.template value<size_t>());
  }

  // -- Pass-through methods to STL std::vector
  // -------------------------------------
  ITIVector& operator=(const ITIVector& x) {
    ParentType::operator=(x.get());
    return *this;
  }

  void assign(size_type n, const value_type& val) {
    ParentType::assign(n, val);
  }
  template <typename InputIt>
  void assign(InputIt f, InputIt l) {
    ParentType::assign(f, l);
  }

  iterator begin() { return ParentType::begin(); }
  const_iterator begin() const { return ParentType::begin(); }
  iterator end() { return ParentType::end(); }
  const_iterator end() const { return ParentType::end(); }
  reverse_iterator rbegin() { return ParentType::rbegin(); }
  const_reverse_iterator rbegin() const { return ParentType::rbegin(); }
  reverse_iterator rend() { return ParentType::rend(); }
  const_reverse_iterator rend() const { return ParentType::rend(); }

  size_type size() const { return ParentType::size(); }
  size_type max_size() const { return ParentType::max_size(); }

  void resize(size_type new_size, value_type x = value_type()) {
    ParentType::resize(new_size, x);
  }

  size_type capacity() const { return ParentType::capacity(); }
  bool empty() const { return ParentType::empty(); }
  void reserve(size_type n) { ParentType::reserve(n); }
  void push_back(const value_type& x) { ParentType::push_back(x); }
  void pop_back() { ParentType::pop_back(); }
  void swap(ITIVector& x) { ParentType::swap(*x.mutable_get()); }
  void clear() { return ParentType::clear(); }

  reference front() { return ParentType::front(); }
  const_reference front() const { return ParentType::front(); }
  reference back() { return ParentType::back(); }
  const_reference back() const { return ParentType::back(); }
  pointer data() { return ParentType::data(); }
  const_pointer data() const { return ParentType::data(); }

  iterator erase(iterator pos) { return ParentType::erase(pos); }
  iterator erase(iterator first, iterator last) {
    return ParentType::erase(first, last);
  }
  iterator insert(iterator pos, const value_type& x) {
    return ParentType::insert(pos, x);
  }
  void insert(iterator pos, size_type n, const value_type& x) {
    ParentType::insert(pos, n, x);
  }
  template <typename IIt>
  void insert(iterator pos, IIt first, IIt last) {
    ParentType::insert(pos, first, last);
  }
};

#define ITIVECTOR_COMPARISON_OP(op)                                \
  template <typename IntType, typename T, typename Alloc>          \
  inline bool operator op(const ITIVector<IntType, T, Alloc>& x,   \
                          const ITIVector<IntType, T, Alloc>& y) { \
    return x.get() op y.get();                                     \
  }
ITIVECTOR_COMPARISON_OP(== );  // NOLINT
ITIVECTOR_COMPARISON_OP(!= );  // NOLINT
ITIVECTOR_COMPARISON_OP(< );   // NOLINT
ITIVECTOR_COMPARISON_OP(<= );  // NOLINT
ITIVECTOR_COMPARISON_OP(> );   // NOLINT
ITIVECTOR_COMPARISON_OP(>= );  // NOLINT
#undef ITIVECTOR_COMPARISON_OP

template <typename IntType, typename T, typename Alloc>
inline void swap(ITIVector<IntType, T, Alloc>& x,
                 ITIVector<IntType, T, Alloc>& y) {
  x.swap(y);
}

#endif  // OR_TOOLS_BASE_INT_TYPE_INDEXED_VECTOR_H_
