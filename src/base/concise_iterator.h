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

#ifndef OR_TOOLS_BASE_CONCISE_ITERATOR_H_
#define OR_TOOLS_BASE_CONCISE_ITERATOR_H_

#include <deque>
#include <vector>

namespace operations_research {

// Concise STL iterator wrapper
//
// The ConstIter<> and MutableIter<> classes have similar syntax to
// STL iterators, but allow writing much more concise iteration loops.
//
// They internally keep track of the container, so there is additional
// memory cost of an extra pointer. As iterators are usually allocated
// on the stack, this is irrelevant most of the time.
//
// The container is required to provide begin/end() and an STL iterator
// that defines same operations that you'll be using on concise iterator.
//
// NOTE: ConstReverseIter<> and MutableReverseIter<> are provided for
// reverse iteration.
//
// EXAMPLES:
//
// std::vector<int> my_vector;
// my_vector.push_back(1);
// my_vector.push_back(2);
//
// for (ConstIter<std::vector<int> > it(my_vector); !it.at_end(); ++it)
//   LOG(INFO) << *it;
//
// std::map<int, std::string> my_map;
// my_map[1] = "a";
// my_map[2] = "b";
//
// for (ConstIter<std::map<int, std::string> > it(my_map); !it.at_end(); ++it)
//   LOG(INFO) << it->first << " " << it->second;
//
// Includes a quick and safe "erase" feature:
// (note the absence of "++it" in the for())
// for (MutableIter<std::map<int, std::string> > it(my_map); !it.at_end();) {
//   if ( ... it->first) {
//     it.erase();  // <- safely deletes that entry and moves the iterator one
//                  // step ahead.
//   } else {
//     ++it;
//   }
// }

template<class Container>
class Eraser;

template<class Container>
class ConstIter {
 public:
  typedef Container container_type;
  typedef typename Container::value_type value_type;
  typedef typename Container::const_iterator const_iterator_type;

  explicit ConstIter(const container_type& container)
      : container_(&container), iterator_(container.begin()) {}
  ConstIter(const ConstIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
  }
  ConstIter& operator= (const ConstIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
    return *this;
  }
  bool operator== (const ConstIter& iter) const {
    return iterator_ == iter.iterator_;
  }
  const value_type* operator->() const { return iterator_.operator->(); }
  const value_type& operator*() const { return iterator_.operator*(); }
  const container_type* const_container() const { return container_; }
  const_iterator_type const_iterator() const { return iterator_; }
  ConstIter& operator++() {
    ++iterator_;
    return *this;
  }
  ConstIter operator++(int unused) {
    ConstIter a = *this;
    ++(*this);
    return a;
  }
  bool at_end() const { return iterator_ == container_->end(); }

 private:
  const container_type* container_;
  const_iterator_type iterator_;
};

// Note: this class is not compatible with sets (operator* returns a non-const
// reference).
template<class Container>
class MutableIter {
 public:
  typedef Container container_type;
  typedef typename Container::value_type value_type;
  typedef typename Container::iterator iterator_type;
  typedef typename Container::const_iterator const_iterator_type;

  explicit MutableIter(container_type& container)  // NOLINT
      : container_(&container), iterator_(container.begin()) {}
  MutableIter(const MutableIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
  }
  MutableIter& operator= (const MutableIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
    return *this;
  }
  bool operator== (const MutableIter& iter) const {
    return iterator_ == iter.iterator_;
  }
  value_type* operator->() const { return iterator_.operator->(); }
  value_type& operator*() const { return iterator_.operator*(); }
  const container_type* const_container() const { return container_; }
  const_iterator_type const_iterator() const { return iterator_; }
  container_type* container() const { return container_; }
  iterator_type iterator() const { return iterator_; }
  MutableIter& operator++() {
    ++iterator_;
    return *this;
  }
  MutableIter operator++(int unused) {
    MutableIter a = *this;
    ++(*this);
    return a;
  }
  bool at_end() const { return iterator_ == container_->end(); }
  MutableIter& erase() {
    Eraser<Container>::erase(container_, &iterator_);
    return *this;
  }

 private:
  container_type* container_;
  iterator_type iterator_;
};

template<class Container>
class ConstReverseIter {
 public:
  typedef Container container_type;
  typedef typename Container::value_type value_type;
  typedef typename Container::const_reverse_iterator
      const_reverse_iterator_type;

  explicit ConstReverseIter(const container_type& container)
      : container_(&container), iterator_(container.rbegin()) {}
  ConstReverseIter(const ConstReverseIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
  }
  ConstReverseIter& operator= (const ConstReverseIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
    return *this;
  }
  bool operator== (const ConstReverseIter& iter) const {
    return iterator_ == iter.iterator_;
  }
  const value_type* operator->() const { return iterator_.operator->(); }
  const value_type& operator*() const { return iterator_.operator*(); }
  const container_type* const_container() const { return container_; }
  const_reverse_iterator_type const_reverse_iterator() const {
    return iterator_; }
  ConstReverseIter& operator++() {
    ++iterator_;
    return *this;
  }
  ConstReverseIter operator++(int unused) {
    ConstReverseIter a = *this;
    ++(*this);
    return a;
  }
  bool at_end() const { return iterator_ == container_->rend(); }

 private:
  const container_type* container_;
  const_reverse_iterator_type iterator_;
};

template<class Container>
class MutableReverseIter {
 public:
  typedef Container container_type;
  typedef typename Container::value_type value_type;
  typedef typename Container::reverse_iterator reverse_iterator_type;
  typedef typename Container::const_reverse_iterator
      const_reverse_iterator_type;

  explicit MutableReverseIter(container_type& container)  // NOLINT
      : container_(&container), iterator_(container.rbegin()) {}
  MutableReverseIter(const MutableReverseIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
  }
  MutableReverseIter& operator= (const MutableReverseIter& source) {
    container_ = source.container_;
    iterator_ = source.iterator_;
    return *this;
  }
  bool operator== (const MutableReverseIter& iter) const {
    return iterator_ == iter.iterator_;
  }
  value_type* operator->() const { return iterator_.operator->(); }
  value_type& operator*() const { return iterator_.operator*(); }
  const container_type* const_container() const { return container_; }
  const_reverse_iterator_type const_reverse_iterator() const {
    return iterator_; }
  container_type* container() const { return container_; }
  reverse_iterator_type reverse_iterator() const { return iterator_; }
  MutableReverseIter& operator++() {
    ++iterator_;
    return *this;
  }
  MutableReverseIter operator++(int unused) {
    MutableReverseIter a = *this;
    ++(*this);
    return a;
  }
  bool at_end() const { return iterator_ == container_->rend(); }
  // erase(reverse_iterator) is not supported by STL containers.

 private:
  container_type* container_;
  reverse_iterator_type iterator_;
};

// This class performs an iterator-friendly erasure: the element pointed at gets
// removed from the container and the iterator remains valid and points to the
// next element.
// This is only valid for set, multiset, map, multimap and list.
// Vectors and Deques are special cases that need specialized processing
// defined in the specific template classes below.
template<class Container>
class Eraser {
 public:
  typedef typename Container::iterator iterator_type;
  static iterator_type* erase(Container* container, iterator_type* iterator) {
    container->erase((*iterator)++);
    return iterator;
  }
};

// This version of the Eraser works for vectors
template<class T> class Eraser<std::vector<T> > {
 public:
  typedef typename std::vector<T>::iterator iterator_type;
  static iterator_type* erase(std::vector<T>* container,
                              iterator_type* iterator) {
    *iterator = container->erase(*iterator);
    return iterator;
  }
};

// This version of the Eraser works for deques
template<class T> class Eraser<std::deque<T> > {
 public:
  typedef typename std::deque<T>::iterator iterator_type;
  static iterator_type* erase(std::deque<T>* container,
                              iterator_type* iterator) {
    *iterator = container->erase(*iterator);
    return iterator;
  }
};
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_CONCISE_ITERATOR_H_
