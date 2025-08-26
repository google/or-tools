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

// This file provides the StrongVector container that wraps around the STL
// vector. The wrapper restrict indexing to a pre-specified type-safe integer
// type or StrongInt (see ortools/base/strong_int.h).  It prevents accidental
// indexing by different "logical" integer-like types (e.g. another StrongInt)
// or native integer types.  The wrapper is useful as C++ and the standard
// template library allows the user to mix "logical" integral indices that might
// have a different role.
//
// The container can only be indexed by an instance of an StrongInt class, which
// can be declared as:
//
//    DEFINE_STRONG_INT_TYPE(type_name, value_type);
//
// where type_name is the desired name for the "logical" integer-like type
// and the value_type is a supported native integer type such as int or
// uint64_t (see ortools/base/strong_int.h for details).
//
// The wrapper exposes all public methods of STL vector and behaves mostly as
// pass-through.  The only methods modified to ensure type-safety are the
// operator [] and the at() methods.
//
// EXAMPLES --------------------------------------------------------------------
//
//    DEFINE_STRONG_INT_TYPE(PhysicalChildIndex, int32_t);
//    StrongVector<PhysicalChildIndex, ChildStats*> vec;
//
//    PhysicalChildIndex physical_index;
//    vec[physical_index] = ...;        <-- index type match: compiles properly.
//    vec.at(physical_index) = ...;     <-- index type match: compiles properly.
//
//    int32_t physical_index;
//    vec[physical_index] = ...;        <-- fails to compile.
//    vec.at(physical_index) = ...;     <-- fails to compile.
//
//    DEFINE_STRONG_INT_TYPE(LogicalChildIndex, int32_t);
//    LogicalChildIndex logical_index;
//    vec[logical_index] = ...;        <-- fails to compile.
//    vec.at(logical_index) = ...;     <-- fails to compile.
//
// NB: Iterator arithmetic bypasses strong typing for the index.
//
// OVERFLOW BEHAVIOR
//
// This class ONLY guards against growing the size beyond the range
// indexable by the index type in debug mode. In optimized mode the
// user can CHECK IsValidSize() when deemed important.

#ifndef OR_TOOLS_BASE_STRONG_VECTOR_H_
#define OR_TOOLS_BASE_STRONG_VECTOR_H_

#include <limits>
#include <type_traits>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/strong_int.h"

namespace util_intops {

template <typename IntType, typename NativeType,
          typename Alloc = std::allocator<NativeType>>
class StrongVector : protected std::vector<NativeType, Alloc> {
 public:
  typedef std::vector<NativeType, Alloc> ParentType;
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
  StrongVector() {}
  explicit StrongVector(const allocator_type& a) : ParentType(a) {}
  explicit StrongVector(size_type n) : ParentType(n) { DCHECK(IsValidSize()); }
  explicit StrongVector(IntType n)
      : StrongVector(static_cast<size_type>(n.value())) {}
  explicit StrongVector(size_type n, const value_type& v,
                        const allocator_type& a = allocator_type())
      : ParentType(n, v, a) {
    DCHECK(IsValidSize());
  }
  explicit StrongVector(IntType n, const value_type& v,
                        const allocator_type& a = allocator_type())
      : StrongVector(static_cast<size_type>(n.value()), v, a) {}
  StrongVector(const StrongVector& x) : ParentType(x.get()) {
    DCHECK(IsValidSize());
  }
  StrongVector(StrongVector&& x) = default;
  StrongVector(std::initializer_list<value_type> l,
               const allocator_type& a = allocator_type())
      : ParentType(l, a) {
    DCHECK(IsValidSize());
  }
  template <typename InputIteratorType>
  StrongVector(InputIteratorType first, InputIteratorType last,
               const allocator_type& a = allocator_type())
      : ParentType(first, last, a) {
    DCHECK(IsValidSize());
  }
  ~StrongVector() {}

  // -- Accessors --------------------------------------------------------------
  // This const accessor is useful in defining the comparison operators below.
  const ParentType& get() const { return *this; }
  // The mutable accessor is useful when using auxiliary methods relying on
  // vector parameters such as JoinUsing(), SplitStringUsing(), etc.  Methods
  // relying solely on iterators (e.g. STLDeleteElements) should work just fine
  // without the need for mutable_get().  NB: It should be used only in this
  // case and thus should not be abused to index the underlying vector without
  // the appropriate IntType.
  ParentType* mutable_get() { return this; }

  // -- Modified methods -------------------------------------------------------
  reference operator[](IntType i) {
    return ParentType::operator[](static_cast<size_type>(i.value()));
  }
  const_reference operator[](IntType i) const {
    return ParentType::operator[](static_cast<size_type>(i.value()));
  }
  reference at(IntType i) {
    return ParentType::at(static_cast<size_type>(i.value()));
  }
  const_reference at(IntType i) const {
    return ParentType::at(static_cast<size_type>(i.value()));
  }

  // -- Extension methods ------------------------------------------------------

  // Iteration related methods. Useful for parallel iteration and
  // non-trivial access patterns. Typical loop will be:
  // for (auto i = v.start_index(); i < v.end_index(); ++i) ...
  IntType start_index() const { return IntType(0); }
  // Index following the last valid index into the vector. In case
  // size() has grown beyond values representable by IntType, this
  // function will truncate the result. There is a debugging check for
  // such behavior, but it is unlikely to be triggered in testing.
  IntType end_index() const {
    DCHECK(IsValidSize());
    return IntType(size());
  }

  // Returns true if the vector is fully addressable by the index type.
  bool IsValidSize() const { return ValidSize(size()); }

  // Most methods from vector can be reused without any changes.
  using ParentType::back;
  using ParentType::begin;
  using ParentType::capacity;
  using ParentType::cbegin;
  using ParentType::cend;
  using ParentType::clear;
  using ParentType::empty;
  using ParentType::end;
  using ParentType::erase;
  using ParentType::front;
  using ParentType::max_size;
  using ParentType::pop_back;
  using ParentType::rbegin;
  using ParentType::rend;
  using ParentType::shrink_to_fit;

  // Returns an iterator of valid indices into this vector. Goes from
  // start_index() to end_index(). This is useful for cases of
  // parallel iteration over several vectors indexed by the same type, e.g.
  //   StrongVector<MyInt, foo> v1;
  //   StrongVector<MyInt, foo> v2;
  //   CHECK_EQ(v1.size(), v2.size());
  //   for (const auto i : v1.index_range()) {
  //     do_stuff(v1[i], v2[i]);
  //   }
  StrongIntRange<IntType> index_range() const {
    return StrongIntRange<IntType>(start_index(), end_index());
  }

  // -- Pass-through methods to STL vector -------------------------------------

  // Note that vector<bool>::data() does not exist. By wrapping data()
  // below, this allows StrongVector<T, bool> to still compile, as long as
  // StrongVector<T, bool>::data() is never called.
  value_type* data() { return ParentType::data(); }
  const value_type* data() const { return ParentType::data(); }

  StrongVector& operator=(const StrongVector& x) {
    ParentType::operator=(x.get());
    return *this;
  }
  StrongVector& operator=(StrongVector&& x) = default;
  StrongVector& operator=(std::initializer_list<value_type> l) {
    ParentType::operator=(l);
    DCHECK(IsValidSize());
    return *this;
  }

  void swap(StrongVector& x) noexcept { ParentType::swap(*x.mutable_get()); }

  void assign(size_type n, const value_type& val) {
    DCHECK(ValidSize(n));
    ParentType::assign(n, val);
  }
  template <typename InputIt>
  void assign(InputIt f, InputIt l) {
    ParentType::assign(f, l);
    DCHECK(IsValidSize());
  }
  void assign(std::initializer_list<value_type> l) {
    ParentType::assign(l);
    DCHECK(IsValidSize());
  }

  template <typename... Args>
  iterator emplace(const_iterator pos, Args&&... args) {
    iterator result = ParentType::emplace(pos, std::forward<Args>(args)...);
    DCHECK(IsValidSize());
    return result;
  }

  template <typename... Args>
  reference emplace_back(Args&&... args) {
    reference value = ParentType::emplace_back(std::forward<Args>(args)...);
    DCHECK(IsValidSize());
    return value;
  }

  iterator insert(const_iterator pos, const value_type& x) {
    iterator result = ParentType::insert(pos, x);
    DCHECK(IsValidSize());
    return result;
  }
  iterator insert(const_iterator pos, value_type&& x) {
    iterator result = ParentType::insert(pos, std::move(x));
    DCHECK(IsValidSize());
    return result;
  }
  void insert(const_iterator pos, size_type n, const value_type& x) {
    ParentType::insert(pos, n, x);
    DCHECK(IsValidSize());
  }
  template <typename SIT>
  void insert(const_iterator pos, SIT first, SIT last) {
    ParentType::insert(pos, first, last);
    DCHECK(IsValidSize());
  }

  void push_back(const value_type& val) {
    ParentType::push_back(val);
    DCHECK(IsValidSize());
  }
  void push_back(value_type&& val) {
    ParentType::push_back(std::move(val));
    DCHECK(IsValidSize());
  }

  void reserve(size_type n) {
    DCHECK(ValidSize(n));
    ParentType::reserve(n);
  }

  void reserve(IntType n) { reserve(static_cast<size_type>(n.value())); }

  void resize(size_type new_size) {
    DCHECK(ValidSize(new_size));
    ParentType::resize(new_size);
  }

  void resize(IntType new_size) {
    resize(static_cast<size_type>(new_size.value()));
  }

  void resize(size_type new_size, const value_type& x) {
    DCHECK(ValidSize(new_size));
    ParentType::resize(new_size, x);
  }

  void resize(IntType new_size, const value_type& x) {
    resize(static_cast<size_type>(new_size.value()), x);
  }

  using ParentType::size;

  static_assert(std::is_integral<typename IntType::ValueType>::value,
                "int type indexed vector must have integral index");

  template <typename H>
  friend H AbslHashValue(H h, const StrongVector& v) {
    return H::combine(std::move(h), v.get());
  }

 private:
  // Checks that the given value n is in range of the index type.
  static bool ValidSize(size_type n) {
    return n <= std::numeric_limits<typename IntType::ValueType>::max();
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
  friend void swap(StrongVector& x, StrongVector& y) noexcept { x.swap(y); }
};

}  // namespace util_intops

#endif  // OR_TOOLS_BASE_STRONG_VECTOR_H_
