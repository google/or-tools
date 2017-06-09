// Copyright 2010-2014 Google
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


#ifndef OR_TOOLS_BASE_SPAN_H_
#define OR_TOOLS_BASE_SPAN_H_

// An Span<T> represents an immutable array of elements of type
// T.  It has a length "length", and a base pointer "ptr", and the
// array it represents contains the elements "ptr[0] .. ptr[len-1]".
// The backing store for the array is *not* owned by the Span
// object, and clients must arrange for the backing store to remain
// live while the Span object is in use.
//
// An Span<T> is somewhat analogous to a StringPiece, but for
// array elements of type T.
//
// Implicit conversion operations are provided from types such as
// std::vector<T> and util::gtl::InlinedVector<T, N>.  Note that Span
// objects constructed from types in this way may be invalidated by
// any operations that mutate the underlying vector.
//
// One common use for Span is when passing arguments to a
// routine where you want to be able to accept a variety of array
// types (e.g. a vector, a util::gtl::InlinedVector, a C-style array,
// etc.).  The usual approach here is to have the client explicitly
// pass in a pointer and a length, as in:
//
//   void MyRoutine(const int* elems, int N) {
//     for (int i = 0; i < N; i++) { .. do something with elems[i] .. }
//   }
//
// Unfortunately, this leads to ugly and error-prone code at the call site:
//
//   std::vector<int> my_vector;
//   MyRoutine(vector_as_array(&my_vector), my_vector.size());
//
//   util::gtl::InlinedVector<int, 4> my_inline_vector;
//   MyRoutine(my_inline_vector.array(), my_inline_vector.size());
//
//   int my_array[10];
//   MyRoutine(my_array, 10);
//
// Instead, you can use an Span as the argument to the routine:
//
//   void MyRoutine(Span<int> a) {
//     for (int i = 0; i < a.size(); i++) { .. do something with a[i] .. }
//   }
//
// This makes the call sites cleaner, for the most part:
//
//   std::vector<int> my_vector;
//   MyRoutine(my_vector);
//
//   util::gtl::InlinedVector<int, 4> my_inline_vector;
//   MyRoutine(my_inline_vector);
//
//   int my_array[10];
//   MyRoutine(my_array);
//
//   int* my_array = new int[10];
//   MyRoutine(gtl::Span<int>(my_array, 10));
//
// MutableSpan<T> represents a mutable array of elements, and, like
// Span, does not own the backing store. The implicit constructors it
// provides allow functions not to worry about whether their mutable arguments
// refer to vectors, arrays, google::protobuf::RepeatedFields, etc.:
//
//   void MyMutatingRoutine(MutableSpan<int> a) {
//     for (int i = 0; i < a.size(); i++) { .. mutate a[i] .. }
//   }
//
//   std::vector<int> my_vector;
//   MyMutatingRoutine(&my_vector);
//
//   int my_array[10];
//   MyMutatingRoutine(my_array);
//
//   int* my_array = new int[10];
//   MyMutatingRoutine(gtl::MutableSpan<int>(my_array, 10));
//
//   MyProto my_proto;
//   for (int i = 0; i < 10; ++i) { my_proto.add_value(i); }
//   MyMutatingRoutine(my_proto.mutable_value());

#include <initializer_list>
#include <type_traits>
#include <vector>

#include "ortools/base/inlined_vector.h"

namespace gtl {
namespace internal {

// Template logic for generic constructors.

// Wrappers whose Get() delegates to the appropriate method of a container, and
// is defined when this method exists. Delegates to the const method if C is a
// const type.
struct Data {
  template <typename C>
  static decltype(std::declval<C>().data()) Get(C* v) {
    return v->data();
  }
};

struct MutableData {
  template <typename C>
  static decltype(std::declval<C>().mutable_data()) Get(C* v) {
    return v->mutable_data();
  }
};

struct Size {
  template <typename C>
  static decltype(std::declval<C>().size()) Get(C* v) {
    return v->size();
  }
};

struct MutableStringData {
  // Defined only for std::string.
  static char* Get(std::string* v) { return v->empty() ? nullptr : &*v->begin(); }
};

// Checks whether M::Get(C*) is defined and has a return type R such that
// Checker::valid<R>()==true.
template <typename M, typename Checker, typename C>
struct HasGetHelper : public M {
 private:
  struct None {};
  // M::Get is selected when it is viable. Get(...) is selected otherwise.
  using M::Get;
  static None Get(...);

 public:
  static constexpr bool HasGet() {
    using Result = decltype(Get(std::declval<C*>()));
    return !std::is_same<Result, None>() && Checker::template valid<Result>();
  }
};

// Defines HasGet() for a particular method, container, and checker. If
// HasGet()==true, provides Get() that delegates to the method.
template <typename M, typename Checker, typename C,
          bool /*has_get*/ = HasGetHelper<M, Checker, C>::HasGet()>
struct Wrapper {
  static constexpr bool HasGet() { return false; }
};

template <typename M, typename Checker, typename C>
struct Wrapper<M, Checker, C, true> {
  static constexpr bool HasGet() { return true; }
  static decltype(M::Get(std::declval<C*>())) Get(C* v) { return M::Get(v); }
};

// Type checker for a method returning an integral value.
struct SizeChecker {
  template <typename R>
  static constexpr bool valid() {
    return std::is_integral<R>::value;
  }
};

// Type checker for a method returning either a pointer to T or a less const
// version of that.
template <typename T>
struct DataChecker {
  // We want to enable conversion from std::vector<T*> to Span<const T*>
  // but
  // disable conversion from std::vector<Derived> to Span<Base>. Here we
  // use
  // the fact that U** is convertible to Q* const* if and only if Q is the same
  // type or a more cv-qualified version of U.
  template <typename R>
  static constexpr bool valid() {
    return std::is_convertible<R*, T* const*>::value;
  }
};

// Aliases to A if A::HasGet()==true, or to B otherwise.
template <typename A, typename B>
using FirstWithGet = typename std::conditional<A::HasGet(), A, B>::type;

// Wraps C::data() const, returning a pointer to const data.
template <typename T, typename C>
using ContainerData = Wrapper<Data, DataChecker<const T>, const C>;

// Wraps a method returning a pointer to mutable data. Prefers data() over
// mutable_data(), and handles strings when T==char. If data() returns a pointer
// to mutable data, it is most likely overloaded, but may also be a single
// method 'T* C::data() const' in a non-STL-compliant container.
template <typename T, typename C>
using ContainerMutableData =
    FirstWithGet<Wrapper<Data, DataChecker<T>, C>,
                 FirstWithGet<Wrapper<MutableData, DataChecker<T>, C>,
                              Wrapper<MutableStringData, DataChecker<T>, C>>>;

// Wraps C::size() const.
template <typename C>
using ContainerSize = Wrapper<Size, SizeChecker, const C>;

// Implementation class for Span and MutableSpan. In the case of
// Span, T will be a const type; for MutableSpan, T will be a
// mutable type.
template <typename T>
class SpanImplBase {
 public:
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef pointer iterator;
  typedef const_pointer const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  static const size_type npos = static_cast<size_type>(-1);

  SpanImplBase(pointer array, size_type length)
      : ptr_(array), length_(length) {}

  // Substring of another Span.
  // pos must be non-negative and <= x.length().
  // len must be non-negative and will be pinned to at most x.length() - pos.
  SpanImplBase(const SpanImplBase& x, size_type pos, size_type len)
      : ptr_(x.ptr_ + pos), length_(std::min(x.length_ - pos, len)) {}

  // Some of the const methods below return pointers and references to mutable
  // data. This is only the case in this internal class; Span and
  // MutableSpan provide deep-constness.

  pointer data() const { return ptr_; }
  size_type size() const { return length_; }

  void clear() {
    ptr_ = nullptr;
    length_ = 0;
  }

  reference operator[](size_type i) const { return ptr_[i]; }
  reference at(size_type i) const {
    DCHECK_LT(i, length_);
    return ptr_[i];
  }
  reference front() const {
    DCHECK_GT(length_, 0);
    return ptr_[0];
  }
  reference back() const {
    DCHECK_GT(length_, 0);
    return ptr_[length_ - 1];
  }

  void remove_prefix(size_type n) {
    DCHECK_GE(length_, n);
    ptr_ += n;
    length_ -= n;
  }
  void remove_suffix(size_type n) {
    DCHECK_GE(length_, n);
    length_ -= n;
  }

  iterator begin() const { return ptr_; }
  iterator end() const { return ptr_ + length_; }
  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }

  bool operator==(const SpanImplBase& other) const {
    if (size() != other.size()) return false;
    if (data() == other.data()) return true;
    return std::equal(data(), data() + size(), other.data());
  }
  bool operator!=(const SpanImplBase& other) const { return !(*this == other); }

 private:
  pointer ptr_;
  size_type length_;
};

template <typename T>
class SpanImpl : public SpanImplBase<const T> {
 public:
  using SpanImplBase<const T>::SpanImplBase;

  // Defined iff the data and size accessors for the container C have been
  // defined.
  template <typename C>
  using EnableIfConvertibleFrom =
      typename std::enable_if<ContainerData<T, C>::HasGet() &&
                              ContainerSize<C>::HasGet()>::type;

  // Constructs from a container when EnableIfConvertibleFrom is
  // defined. std::addressof handles types with overloaded operator&.
  template <typename C>
  explicit SpanImpl(const C& v)
      : SpanImplBase<const T>(ContainerData<T, C>::Get(std::addressof(v)),
                              ContainerSize<C>::Get(std::addressof(v))) {}
};

template <typename T>
class MutableSpanImpl : public SpanImplBase<T> {
 public:
  using SpanImplBase<T>::SpanImplBase;

  template <typename C>
  using EnableIfConvertibleFrom =
      typename std::enable_if<ContainerMutableData<T, C>::HasGet() &&
                              ContainerSize<C>::HasGet()>::type;

  template <typename C>
  explicit MutableSpanImpl(C* v)
      : SpanImplBase<T>(ContainerMutableData<T, C>::Get(v),
                        ContainerSize<C>::Get(v)) {}
};
}  // namespace internal

template <typename T>
class Span {
 private:
  typedef internal::SpanImpl<T> Impl;

 public:
  typedef T value_type;
  typedef typename Impl::pointer pointer;
  typedef typename Impl::const_pointer const_pointer;
  typedef typename Impl::reference reference;
  typedef typename Impl::const_reference const_reference;
  typedef typename Impl::iterator iterator;
  typedef typename Impl::const_iterator const_iterator;
  typedef typename Impl::reverse_iterator reverse_iterator;
  typedef typename Impl::const_reverse_iterator const_reverse_iterator;
  typedef typename Impl::size_type size_type;
  typedef typename Impl::difference_type difference_type;

  static const size_type npos = Impl::npos;

  Span() : impl_(nullptr, 0) {}
  Span(const_pointer array, size_type length) : impl_(array, length) {}

  // Implicit conversion constructors
  Span(const std::vector<value_type>& v)  // NOLINT(runtime/explicit)
      : impl_(v.data(), v.size()) {}

  template <size_t N>
  Span(const value_type (&a)[N])  // NOLINT(runtime/explicit)
      : impl_(a, N) {}

  template <int N>
  Span(const InlinedVector<value_type, N>& v)  // NOLINT(runtime/explicit)
      : impl_(v.data(), v.size()) {}

  // The constructor for any class supplying 'data() const' that returns either
  // const T* or a less const-qualified version of it, and 'some_integral_type
  // size() const'. google::protobuf::RepeatedField<T>, std::string and (since C++11)
  // std::vector<T,A> and std::array<T, N> are examples of this. See
  // span_internal.h for details.
  template <typename V,
            typename = typename Impl::template EnableIfConvertibleFrom<V>>
  Span(const V& v)  // NOLINT(runtime/explicit)
      : impl_(v) {}

  // Implicitly constructs an Span from an initializer list. This makes it
  // possible to pass a brace-enclosed initializer list to a function expecting
  // an Span:
  //   void Process(Span<int> x);
  //   Process({1, 2, 3});
  // The data referenced by the initializer_list must outlive this
  // Span. For example, "Span<int> s={1,2};" and "return
  // Span<int>({3,4});" are errors, as the resulting Span may
  // reference data that is no longer valid.
  Span(std::initializer_list<value_type> v)  // NOLINT(runtime/explicit)
      : impl_(v.begin(), v.size()) {}

  // Substring of another Span.
  // pos must be non-negative and <= x.length().
  // len must be non-negative and will be pinned to at most x.length() - pos.
  // If len==npos, the substring continues till the end of x.
  Span(const Span& x, size_type pos, size_type len)
      : impl_(x.impl_, pos, len) {}

  const_pointer data() const { return impl_.data(); }
  size_type size() const { return impl_.size(); }
  size_type length() const { return size(); }
  bool empty() const { return size() == 0; }

  void clear() { impl_.clear(); }

  const_reference operator[](size_type i) const { return impl_[i]; }
  const_reference at(size_type i) const { return impl_.at(i); }
  const_reference front() const { return impl_.front(); }
  const_reference back() const { return impl_.back(); }

  const_iterator begin() const { return impl_.begin(); }
  const_iterator end() const { return impl_.end(); }
  const_reverse_iterator rbegin() const { return impl_.rbegin(); }
  const_reverse_iterator rend() const { return impl_.rend(); }

  void remove_prefix(size_type n) { impl_.remove_prefix(n); }
  void remove_suffix(size_type n) { impl_.remove_suffix(n); }
  void pop_back() { remove_suffix(1); }
  void pop_front() { remove_prefix(1); }

  // These relational operators have the same semantics as the
  // std::vector<T> relational operators: they do deep (elementwise)
  // comparisons.  Array slices are equal iff their size is the same
  // and all their elements are equal.
  bool operator==(Span<T> other) const { return impl_ == other.impl_; }
  bool operator!=(Span<T> other) const { return impl_ != other.impl_; }

 private:
  Impl impl_;
};

// Mutable version of Span, which allows the clients to mutate the
// underlying data. It is implicitly convertible to Span since it provides
// the data() and size() methods with correct signatures. When a
// MutableSpan is created from a pointer to a container (as opposed to raw
// memory pointer), the pointer must not be null.
//
// A note on const-ness: "mutable" here refers to the mutability of the
// underlying data, not of the slice itself. It is perfectly reasonable to have
// a variable of type "const MutableSpan<T>"; this means that the bounds
// of the view on the array cannot be changed, but the underlying data in the
// array still may be modified. This is akin to a "T* const" pointer, as opposed
// to a "const T*" pointer (corresponding to a non-const Span<T>).
template <typename T>
class MutableSpan {
 private:
  typedef internal::MutableSpanImpl<T> Impl;

 public:
  typedef T value_type;
  typedef typename Impl::pointer pointer;
  typedef typename Impl::const_pointer const_pointer;
  typedef typename Impl::reference reference;
  typedef typename Impl::const_reference const_reference;
  typedef typename Impl::iterator iterator;
  typedef typename Impl::const_iterator const_iterator;
  typedef typename Impl::reverse_iterator reverse_iterator;
  typedef typename Impl::const_reverse_iterator const_reverse_iterator;
  typedef typename Impl::size_type size_type;
  typedef typename Impl::difference_type difference_type;

  static const size_type npos = Impl::npos;

  MutableSpan() : impl_(nullptr, 0) {}
  MutableSpan(pointer array, size_type length) : impl_(array, length) {}

  // Implicit conversion constructors
  MutableSpan(std::vector<value_type>* v)  // NOLINT(runtime/explicit)
      : impl_(v->data(), v->size()) {}

  template <size_t N>
  MutableSpan(value_type (&a)[N])  // NOLINT(runtime/explicit)
      : impl_(a, N) {}

  template <int N>
  MutableSpan(InlinedVector<value_type, N>* v)  // NOLINT(runtime/explicit)
      : impl_(v->data(), v->size()) {}

  // The constructor for any class supplying 'T* data()' or 'T* mutable_data()'
  // (the former is called if both exist), and 'some_integral_type size()
  // const'. google::protobuf::RepeatedField is an example of this. Also supports std::string
  // arguments, when T==char. The appropriate ctor is selected using SFINAE. See
  // span_internal.h for details.
  template <typename V,
            typename = typename Impl::template EnableIfConvertibleFrom<V>>
  MutableSpan(V* v)  // NOLINT(runtime/explicit)
      : impl_(v) {}

  // Substring of another MutableSpan.
  // pos must be non-negative and <= x.length().
  // len must be non-negative and will be pinned to at most x.length() - pos.
  // If len==npos, the substring continues till the end of x.
  MutableSpan(const MutableSpan& x, size_type pos, size_type len)
      : impl_(x.impl_, pos, len) {}

  // Accessors.
  pointer data() const { return impl_.data(); }
  size_type size() const { return impl_.size(); }
  size_type length() const { return size(); }
  bool empty() const { return size() == 0; }

  void clear() { impl_.clear(); }

  reference operator[](size_type i) const { return impl_[i]; }
  reference at(size_type i) const { return impl_.at(i); }
  reference front() const { return impl_.front(); }
  reference back() const { return impl_.back(); }

  iterator begin() const { return impl_.begin(); }
  iterator end() const { return impl_.end(); }
  reverse_iterator rbegin() const { return impl_.rbegin(); }
  reverse_iterator rend() const { return impl_.rend(); }

  void remove_prefix(size_type n) { impl_.remove_prefix(n); }
  void remove_suffix(size_type n) { impl_.remove_suffix(n); }
  void pop_back() { remove_suffix(1); }
  void pop_front() { remove_prefix(1); }

  bool operator==(Span<T> other) const { return Span<T>(*this) == other; }
  bool operator!=(Span<T> other) const { return Span<T>(*this) != other; }

  // DEPRECATED(jacobsa): Please use data() instead.
  pointer mutable_data() const { return impl_.data(); }

 private:
  Impl impl_;
};

template <typename T>
const typename Span<T>::size_type Span<T>::npos;
template <typename T>
const typename MutableSpan<T>::size_type MutableSpan<T>::npos;

}  // namespace gtl

#endif  // OR_TOOLS_BASE_SPAN_H_
