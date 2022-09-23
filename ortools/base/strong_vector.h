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

// This file provides the StrongVector container that wraps around the STL
// std::vector.
// The wrapper restricts indexing to a pre-specified type-safe integer type or
// IntType (see int_type.h).  It prevents accidental indexing
// by different "logical" integer-like types (e.g.  another IntType) or native
// integer types.  The wrapper is useful as C++ and the standard template
// library allows the user to mix "logical" integral indices that might have a
// different role.
//
// The container can only be indexed by an instance of an IntType class, which
// can be declared as:
//
//     DEFINE_INT_TYPE(IntTypeName, IntTypeValueType);
//
// where IntTypeName is the desired name for the "logical" integer-like type
// and the ValueType is a supported native integer type such as int or
// uint64_t (see int_type.h for details).
//
// The wrapper exposes all public methods of STL vector and behaves mostly as
// pass-through.  The only method modified to ensure type-safety is the operator
// [] and the at() method.
//
// EXAMPLES --------------------------------------------------------------------
//
//    DEFINE_INT_TYPE(PhysicalChildIndex, int32_t);
//    absl::StrongVector<PhysicalChildIndex, ChildStats*> vec;
//
//    PhysicalChildIndex physical_index;
//    vec[physical_index] = ...;        <-- index type match: compiles properly.
//    vec.at(physical_index) = ...;     <-- index type match: compiles properly.
//
//    int32_t physical_index;
//    vec[physical_index] = ...;        <-- fails to compile.
//    vec.at(physical_index) = ...;     <-- fails to compile.
//
//    DEFINE_INT_TYPE(LogicalChildIndex, int32_t);
//    int32_t logical_index;
//    vec[logical_index] = ...;        <-- fails to compile.
//    vec.at(logical_index) = ...;     <-- fails to compile.
//
// NB: Iterator arithmetic is not allowed as the iterators are not wrapped
// themselves.  Therefore, the following caveat is possible:
//    *(vec.begin() + 0) = ...;

#ifndef OR_TOOLS_BASE_STRONG_VECTOR_H_
#define OR_TOOLS_BASE_STRONG_VECTOR_H_

#include <stddef.h>

#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/macros.h"

namespace absl {

// STL vector ------------------------------------------------------------------
template <typename IntType, typename T, typename Alloc = std::allocator<T> >
class StrongVector {
 public:
  typedef IntType IndexType;
  typedef std::vector<T, Alloc> ParentType;

  typedef typename ParentType::size_type size_type;
  typedef typename ParentType::allocator_type allocator_type;
  typedef typename ParentType::value_type value_type;
  typedef typename ParentType::difference_type difference_type;
  typedef typename ParentType::reference reference;
  typedef typename ParentType::const_reference const_reference;
  typedef typename ParentType::pointer pointer;
  typedef typename ParentType::const_pointer const_pointer;
  typedef typename ParentType::iterator iterator;
  typedef typename ParentType::const_iterator const_iterator;
  typedef typename ParentType::reverse_iterator reverse_iterator;
  typedef typename ParentType::const_reverse_iterator const_reverse_iterator;

 public:
  StrongVector() {}

  explicit StrongVector(const allocator_type& a) : v_(a) {}
  explicit StrongVector(size_type n) : v_(n) {}

  StrongVector(size_type n, const value_type& v,
               const allocator_type& a = allocator_type())
      : v_(n, v, a) {}

  StrongVector(
      std::initializer_list<value_type> il)  // NOLINT(runtime/explicit)
      : v_(il) {}

  template <typename InputIteratorType>
  StrongVector(InputIteratorType first, InputIteratorType last,
               const allocator_type& a = allocator_type())
      : v_(first, last, a) {}

  // -- Accessors --------------------------------------------------------------
  // This const accessor is useful in defining the comparison operators below.
  const ParentType& get() const { return v_; }
  // The mutable accessor is useful when using auxiliar methods relying on
  // vector parameters such as JoinUsing(), SplitStringUsing(), etc.  Methods
  // relying solely on iterators (e.g. STLDeleteElements) should work just fine
  // without the need for mutable_get().  NB: It should be used only in this
  // case and thus should not be abused to index the underlying vector without
  // the appropriate IntType.
  ParentType* mutable_get() { return &v_; }

  // -- Modified methods -------------------------------------------------------
  reference operator[](IndexType i) { return v_[Value(i)]; }
  const_reference operator[](IndexType i) const { return v_[Value(i)]; }
  reference at(IndexType i) { return v_.at(Value(i)); }
  const_reference at(IndexType i) const { return v_.at(Value(i)); }

  // -- Pass-through methods to STL vector -------------------------------------
  void assign(size_type n, const value_type& val) { v_.assign(n, val); }
  template <typename InputIt>
  void assign(InputIt f, InputIt l) {
    v_.assign(f, l);
  }
  void assign(std::initializer_list<value_type> ilist) { v_.assign(ilist); }

  iterator begin() { return v_.begin(); }
  const_iterator begin() const { return v_.begin(); }
  iterator end() { return v_.end(); }
  const_iterator end() const { return v_.end(); }
  reverse_iterator rbegin() { return v_.rbegin(); }
  const_reverse_iterator rbegin() const { return v_.rbegin(); }
  reverse_iterator rend() { return v_.rend(); }
  const_reverse_iterator rend() const { return v_.rend(); }

  size_type size() const { return v_.size(); }
  size_type max_size() const { return v_.max_size(); }

  void resize(size_type new_size) { v_.resize(new_size); }
  void resize(size_type new_size, const value_type& x) {
    v_.resize(new_size, x);
  }
  void resize(IntType new_size) { v_.resize(new_size.value()); }
  void resize(IntType new_size, const value_type& x) {
    v_.resize(new_size.value(), x);
  }

  size_type capacity() const { return v_.capacity(); }
  bool empty() const { return v_.empty(); }
  void reserve(size_type n) { v_.reserve(n); }
  void push_back(const value_type& x) { v_.push_back(x); }
  void push_back(value_type&& x) { v_.push_back(std::move(x)); }  // NOLINT
  template <typename... Args>
  void emplace_back(Args&&... args) {
    v_.emplace_back(std::forward<Args>(args)...);
  }
  template <typename... Args>
  iterator emplace(const_iterator pos, Args&&... args) {
    return v_.emplace(pos, std::forward<Args>(args)...);
  }
  void pop_back() { v_.pop_back(); }
  void swap(StrongVector& x) { v_.swap(x.v_); }
  void clear() { v_.clear(); }

  reference front() { return v_.front(); }
  const_reference front() const { return v_.front(); }
  reference back() { return v_.back(); }
  const_reference back() const { return v_.back(); }
  pointer data() { return v_.data(); }
  const_pointer data() const { return v_.data(); }

  iterator erase(const_iterator pos) { return v_.erase(pos); }
  iterator erase(const_iterator first, const_iterator last) {
    return v_.erase(first, last);
  }
  iterator insert(const_iterator pos, const value_type& x) {
    return v_.insert(pos, x);
  }
  iterator insert(const_iterator pos, value_type&& x) {  // NOLINT
    return v_.insert(pos, std::move(x));
  }
  iterator insert(const_iterator pos, size_type n, const value_type& x) {
    return v_.insert(pos, n, x);
  }
  template <typename IIt>
  iterator insert(const_iterator pos, IIt first, IIt last) {
    return v_.insert(pos, first, last);
  }
  iterator insert(const_iterator pos, std::initializer_list<value_type> ilist) {
    return v_.insert(pos, ilist);
  }

  friend bool operator==(const StrongVector& x, const StrongVector& y) {
    return x.get() == y.get();
  }
  friend bool operator!=(const StrongVector& x, const StrongVector& y) {
    return x.get() != y.get();
  }
  friend bool operator<(const StrongVector& x, const StrongVector& y) {
    return x.get() < y.get();
  }
  friend bool operator>(const StrongVector& x, const StrongVector& y) {
    return x.get() > y.get();
  }
  friend bool operator<=(const StrongVector& x, const StrongVector& y) {
    return x.get() <= y.get();
  }
  friend bool operator>=(const StrongVector& x, const StrongVector& y) {
    return x.get() >= y.get();
  }
  friend void swap(StrongVector& x, StrongVector& y) { x.swap(y); }

  template <typename H>
  friend H AbslHashValue(H h, const StrongVector& v) {
    return H::combine(std::move(h), v.v_);
  }

 private:
  static size_type Value(IndexType i) { return i.template value<size_type>(); }

  ParentType v_;

  COMPILE_ASSERT(std::is_integral<typename IndexType::ValueType>::value,
                 int_type_indexed_vector_must_have_integral_index);
};

}  // namespace absl

#endif  // OR_TOOLS_BASE_STRONG_VECTOR_H_
