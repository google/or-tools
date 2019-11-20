// Copyright 2010-2018 Google LLC
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

#include <iterator>
#include <vector>

namespace util {

// This is useful for wrapping iterators of a class that support many different
// iterations. For instance, on a Graph class, one can write:
//
// BeginEndWrapper<OutgoingArcIterator> Graph::OutgoingArcs(NodeInde node)
//      const {
//   return BeginEndRange(
//       OutgoingArcIterator(*this, node, /*at_end=*/false),
//       OutgoingArcIterator(*this, node, /*at_end=*/true));
// }
//
// And a client will use it like this:
//
// for (const ArcIndex arc : graph.OutgoingArcs(node)) { ... }
template <typename Iterator>
class BeginEndWrapper {
 public:
  using const_iterator = Iterator;
  using value_type = typename std::iterator_traits<Iterator>::value_type;

  BeginEndWrapper(Iterator begin, Iterator end) : begin_(begin), end_(end) {}
  Iterator begin() const { return begin_; }
  Iterator end() const { return end_; }

  bool empty() const { return begin() == end(); }

 private:
  const Iterator begin_;
  const Iterator end_;
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

// Simple iterator on an integer range, see IntegerRange below.
template <typename IntegerType>
class IntegerRangeIterator
    : public std::iterator<std::input_iterator_tag, IntegerType> {
 public:
  explicit IntegerRangeIterator(IntegerType value) : index_(value) {}
  IntegerRangeIterator(const IntegerRangeIterator& other)
      : index_(other.index_) {}
  IntegerRangeIterator& operator=(const IntegerRangeIterator& other) {
    index_ = other.index_;
  }
  bool operator!=(const IntegerRangeIterator& other) const {
    // This may seems weird, but using < instead of != avoid almost-infinite
    // loop if one use IntegerRange<int>(1, 0) below for instance.
    return index_ < other.index_;
  }
  bool operator==(const IntegerRangeIterator& other) const {
    return index_ == other.index_;
  }
  IntegerType operator*() const { return index_; }
  IntegerRangeIterator& operator++() {
    ++index_;
    return *this;
  }
  IntegerRangeIterator operator++(int) {
    IntegerRangeIterator previous_position(*this);
    ++index_;
    return previous_position;
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
  IntegerRange(IntegerType begin, IntegerType end)
      : BeginEndWrapper<IntegerRangeIterator<IntegerType>>(
            IntegerRangeIterator<IntegerType>(begin),
            IntegerRangeIterator<IntegerType>(end)) {}
};

// Allow iterating over a vector<T> as a mutable vector<T*>.
template <class T>
struct MutableVectorIteration {
  explicit MutableVectorIteration(std::vector<T>* v) : v_(v) {}
  struct Iterator {
    explicit Iterator(typename std::vector<T>::iterator it) : it_(it) {}
    T* operator*() { return &*it_; }
    Iterator& operator++() {
      it_++;
      return *this;
    }
    bool operator!=(const Iterator& other) const { return other.it_ != it_; }

   private:
    typename std::vector<T>::iterator it_;
  };
  Iterator begin() { return Iterator(v_->begin()); }
  Iterator end() { return Iterator(v_->end()); }

 private:
  std::vector<T>* const v_;
};
}  // namespace util

#endif  // UTIL_GRAPH_ITERATORS_H_
