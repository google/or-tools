// Copyright 2010-2013 Google
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
// Helper classes to make it easy to implement range-based for loop.

#ifndef OR_TOOLS_UTIL_ITERATORS_H_
#define OR_TOOLS_UTIL_ITERATORS_H_

namespace operations_research {

// This is useful for wrapping iterators of a class that support many different
// iterations. For instance, on a Graph class, one can write:
//
// BeginEndWrapper<OutgoingArcIterator> Graph::OutgoingArcs(NodeInde node)
//      const {
//   return BeginEndWrapper<OutgoingArcIterator>(
//       OutgoingArcIterator(*this, node, /*at_end=*/false),
//       OutgoingArcIterator(*this, node, /*at_end=*/true));
// }
//
// And a client will use it like this:
// for (const ArcIndex arc : graph.OutgoingArcs(node)) { ... }
template<typename Iterator>
class BeginEndWrapper {
 public:
  BeginEndWrapper(Iterator begin, Iterator end) : begin_(begin), end_(end) {}
  Iterator begin() const { return begin_; }
  Iterator end() const { return end_; }
 private:
  const Iterator begin_;
  const Iterator end_;
};

// The Reverse() function allows to reverse the iteration order of a range-based
// for loop over a container that support STL reverse iterators.
// The syntax is:
//   for (const type& t : Reverse(container_of_t)) { ... }
template<typename Container>
class BeginEndReverseIteratorWrapper {
 public:
  explicit BeginEndReverseIteratorWrapper(const Container& c) : c_(c) {}
  typename Container::const_reverse_iterator begin() const {
      return c_.rbegin();
  }
  typename Container::const_reverse_iterator end() const {
      return c_.rend();
  }
 private:
  const Container& c_;
};
template<typename Container>
BeginEndReverseIteratorWrapper<Container> Reverse(const Container& c) {
  return BeginEndReverseIteratorWrapper<Container>(c);
}

// Simple iterator on an integer range, see IntegerRange below.
template<typename IntegerType>
class IntegerRangeIterator {
 public:
  explicit IntegerRangeIterator(IntegerType value) : index_(value) {}
  bool operator!=(const IntegerRangeIterator& other) const {
    // This may seems weird, but using < instead of != avoid almost-infinite
    // loop if one use IntegerRange<int>(1, 0) below for instance.
    return index_ < other.index_;
  }
  IntegerType operator*() const {
    return index_;
  }
  void operator++() {
    ++index_;
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
// for (const EntryIndex i : sparse_column.AllEntryIndex());
template<typename IntegerType>
class IntegerRange : public BeginEndWrapper<IntegerRangeIterator<IntegerType>> {
 public:
  IntegerRange(IntegerType begin, IntegerType end)
      : BeginEndWrapper<IntegerRangeIterator<IntegerType>> (
          IntegerRangeIterator<IntegerType>(begin),
          IntegerRangeIterator<IntegerType>(end)) {}
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_ITERATORS_H_
