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

// Helper classes to make it easy to implement range-based for loops.

#ifndef UTIL_GRAPH_ITERATORS_H_
#define UTIL_GRAPH_ITERATORS_H_

#include <cstddef>
#include <iterator>
#include <utility>

#include "absl/log/check.h"

namespace util {

// This is useful for wrapping iterators of a class that support many different
// iterations. For instance, on a Graph class, one can write:
//
// BeginEndWrapper<OutgoingArcIterator> Graph::OutgoingArcs(NodeInde node)
//      const {
//   return BeginEndWrapper(
//       OutgoingArcIterator(*this, node, /*at_end=*/false),
//       OutgoingArcIterator(*this, node, /*at_end=*/true));
// }
//
// And a client will use it like this:
//
// for (const ArcIndex arc : graph.OutgoingArcs(node)) { ... }
//
// Note that `BeginEndWrapper` is conceptually a borrowed range as per the C++
// standard (`std::ranges::borrowed_range`):
// "The concept borrowed_range defines the requirements of a range such that a
// function can take it by value and return iterators obtained from it without
// danger of dangling". We cannot `static_assert` this property though as
// `std::ranges` is prohibited in google3.
template <typename Iterator>
class BeginEndWrapper {
 public:
  using const_iterator = Iterator;
  using value_type = typename std::iterator_traits<Iterator>::value_type;

  // If `Iterator` is default-constructible, an empty range.
  BeginEndWrapper() = default;

  BeginEndWrapper(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

  Iterator begin() const { return begin_; }
  Iterator end() const { return end_; }

  // Available only if `Iterator` is a random access iterator.
  size_t size() const { return end_ - begin_; }

  bool empty() const { return begin() == end(); }

 private:
  Iterator begin_;
  Iterator end_;
};

// Inline wrapper methods, to make the client code even simpler.
// The harm of overloading is probably less than the benefit of the nice,
// compact name, in this special case.
template <typename Iterator>
inline BeginEndWrapper<Iterator> BeginEndRange(Iterator begin, Iterator end) {
  return BeginEndWrapper<Iterator>(begin, end);
}
template <typename Iterator>
inline BeginEndWrapper<Iterator> BeginEndRange(
    std::pair<Iterator, Iterator> begin_end) {
  return BeginEndWrapper<Iterator>(begin_end.first, begin_end.second);
}

// Shortcut for BeginEndRange(multimap::equal_range(key)).
// TODO(user): go further and expose only the values, not the pairs (key,
// values) since the caller already knows the key.
template <typename MultiMap>
inline BeginEndWrapper<typename MultiMap::iterator> EqualRange(
    MultiMap& multi_map, const typename MultiMap::key_type& key) {
  return BeginEndRange(multi_map.equal_range(key));
}
template <typename MultiMap>
inline BeginEndWrapper<typename MultiMap::const_iterator> EqualRange(
    const MultiMap& multi_map, const typename MultiMap::key_type& key) {
  return BeginEndRange(multi_map.equal_range(key));
}

// The Reverse() function allows to reverse the iteration order of a range-based
// for loop over a container that support STL reverse iterators.
// The syntax is:
//   for (const type& t : Reverse(container_of_t)) { ... }
template <typename Container>
class BeginEndReverseIteratorWrapper {
 public:
  explicit BeginEndReverseIteratorWrapper(const Container& c) : c_(c) {}
  typename Container::const_reverse_iterator begin() const {
    return c_.rbegin();
  }
  typename Container::const_reverse_iterator end() const { return c_.rend(); }

 private:
  const Container& c_;
};
template <typename Container>
BeginEndReverseIteratorWrapper<Container> Reverse(const Container& c) {
  return BeginEndReverseIteratorWrapper<Container>(c);
}

// Simple iterator on an integer range, see `IntegerRange` below.
// `IntegerType` can be any signed integer type, or strong integer type that
// defines usual operations (e.g. `gtl::IntType<T>`).
template <typename IntegerType>
class IntegerRangeIterator
// TODO(b/385094969): In C++17, `std::iterator_traits<Iterator>` required
// explicitly specifying the iterator category. Remove this when backwards
// compatibility with C++17 is no longer needed.
#if __cplusplus < 201703L
    : public std::iterator<std::input_iterator_tag, IntegerType>
#endif
{
 public:
  using difference_type = ptrdiff_t;
  using value_type = IntegerType;
#if __cplusplus >= 201703L && __cplusplus < 202002L
  using iterator_category = std::input_iterator_tag;
  using pointer = IntegerType*;
  using reference = IntegerType&;
#endif

  IntegerRangeIterator() : index_{} {}

  explicit IntegerRangeIterator(IntegerType value) : index_(value) {}

  IntegerType operator*() const { return index_; }

  // TODO(b/385094969): Use `=default` when backwards compatibility with C++17
  // is no longer needed.
  bool operator==(const IntegerRangeIterator& other) const {
    return index_ == other.index_;
  }
  bool operator!=(const IntegerRangeIterator& other) const {
    return index_ != other.index_;
  }
  bool operator<(const IntegerRangeIterator& other) const {
    return index_ < other.index_;
  }
  bool operator>(const IntegerRangeIterator& other) const {
    return index_ > other.index_;
  }
  bool operator<=(const IntegerRangeIterator& other) const {
    return index_ <= other.index_;
  }
  bool operator>=(const IntegerRangeIterator& other) const {
    return index_ >= other.index_;
  }

  IntegerRangeIterator& operator++() {
    ++index_;
    return *this;
  }

  IntegerRangeIterator operator++(int) {
    auto tmp = *this;
    ++*this;
    return tmp;
  }

  IntegerRangeIterator& operator+=(difference_type n) {
    index_ += n;
    return *this;
  }

  IntegerRangeIterator& operator--() {
    --index_;
    return *this;
  }

  IntegerRangeIterator operator--(int) {
    auto tmp = *this;
    --*this;
    return tmp;
  }

  IntegerRangeIterator& operator-=(difference_type n) {
    index_ -= n;
    return *this;
  }

  IntegerType operator[](difference_type n) const { return index_ + n; }

  friend IntegerRangeIterator operator+(IntegerRangeIterator it,
                                        difference_type n) {
    return IntegerRangeIterator(it.index_ + n);
  }

  friend IntegerRangeIterator operator+(difference_type n,
                                        IntegerRangeIterator it) {
    return IntegerRangeIterator(it.index_ + n);
  }

  friend IntegerRangeIterator operator-(IntegerRangeIterator it,
                                        difference_type n) {
    return IntegerRangeIterator(it.index_ - n);
  }

  friend difference_type operator-(const IntegerRangeIterator l,
                                   const IntegerRangeIterator r) {
    return static_cast<difference_type>(l.index_ - r.index_);
  }

 private:
  IntegerType index_;
};

// Allows to easily construct nice functions for range-based for loop.
// This can be used like this:
//
// for (const int i : IntegerRange<int>(0, 10)) { ... }
//
// But it main purpose is to be used as return value for more complex classes:
//
// for (const ArcIndex arc : graph.AllOutgoingArcs());
// for (const NodeIndex node : graph.AllNodes());
template <typename IntegerType>
class IntegerRange : public BeginEndWrapper<IntegerRangeIterator<IntegerType>> {
 public:
  // Requires `begin <= end`.
  IntegerRange(IntegerType begin, IntegerType end)
      : BeginEndWrapper<IntegerRangeIterator<IntegerType>>(
            IntegerRangeIterator<IntegerType>(begin),
            IntegerRangeIterator<IntegerType>(end)) {
    DCHECK_LE(begin, end);
  }
};

// A helper class for implementing list graph iterators: This does pointer
// chasing on `next` until `sentinel` is found. `Tag` allows distinguishing
// different iterators with the same index type and sentinel.
template <typename IndexT, const IndexT& sentinel, typename Tag>
class ChasingIterator
#if __cplusplus < 201703L
    : public std::iterator<std::input_iterator_tag, IndexT>
#endif
{
 public:
  using difference_type = ptrdiff_t;
  using value_type = IndexT;
#if __cplusplus >= 201703L && __cplusplus < 202002L
  using iterator_category = std::input_iterator_tag;
  using pointer = IndexT*;
  using reference = IndexT&;
#endif

  ChasingIterator() : index_(sentinel), next_(nullptr) {}

  ChasingIterator(IndexT index, const IndexT* next)
      : index_(index), next_(next) {}

  IndexT operator*() const { return index_; }

  ChasingIterator& operator++() {
    index_ = next_[static_cast<ptrdiff_t>(index_)];
    return *this;
  }
  ChasingIterator operator++(int) {
    auto tmp = *this;
    index_ = next_[static_cast<ptrdiff_t>(index_)];
    return tmp;
  }

  friend bool operator==(const ChasingIterator& l, const ChasingIterator& r) {
    return l.index_ == r.index_;
  }
  friend bool operator!=(const ChasingIterator& l, const ChasingIterator& r) {
    return l.index_ != r.index_;
  }

 private:
  IndexT index_;
  const IndexT* next_;
};

}  // namespace util

#endif  // UTIL_GRAPH_ITERATORS_H_
