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

// Vector with map from element to index in the vector.

#ifndef OR_TOOLS_UTIL_VECTOR_MAP_H_
#define OR_TOOLS_UTIL_VECTOR_MAP_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/map_util.h"

namespace operations_research {

// This class stores a vector of distinct elements, as well as a map
// from elements to index to find the index in the vector.
// This is useful to store mapping between objects and indices.
template <class T>
class VectorMap {
 public:
  // Adds an element if not already present, and returns its index in
  // the vector-map.
  int Add(const T& element) {
    int current_index = Index(element);
    if (current_index != -1) {
      return current_index;
    }
    const int index = list_.size();
    CHECK_EQ(index, map_.size());
    list_.push_back(element);
    map_[element] = index;
    return index;
  }
  // TODO(user): Use ArraySlice.

  // Adds all elements of the vector.
  void Add(const std::vector<T>& elements) {
    for (int i = 0; i < elements.size(); ++i) {
      Add(elements[i]);
    }
  }

  // Will return the index of the element if present, or die otherwise.
  int IndexOrDie(const T& element) const {
    return gtl::FindOrDie(map_, element);
  }

  // Returns -1 if the element is not in the vector, or its unique
  // index if it is.
  int Index(const T& element) const {
    return gtl::FindWithDefault(map_, element, -1);
  }
  // TODO(user): explore a int-type version.

  // Returns whether the element has already been added to the vector-map.
  bool Contains(const T& element) const {
    return gtl::ContainsKey(map_, element);
  }

  // Returns the element at position index.
  const T& Element(int index) const {
    CHECK_GE(index, 0);
    CHECK_LT(index, list_.size());
    return list_[index];
  }

  const T& operator[](int index) const { return Element(index); }

  // Returns the number of distinct elements added to the vector-map.
  int size() const { return list_.size(); }

  // Clears all the elements added to the vector-map.
  void clear() {
    list_.clear();
    map_.clear();
  }

  // Returns a read-only access to the vector of elements.
  const std::vector<T>& list() const { return list_; }

  // Standard STL container boilerplate.
  typedef T value_type;
  typedef const T* pointer;
  typedef const T& reference;
  typedef const T& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  static const size_type npos;
  typedef const T* const_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  const_iterator begin() const { return list_.data(); }
  const_iterator end() const { return list_.data() + list_.size(); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(list_.data() + list_.size());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(list_.data());
  }

 private:
  std::vector<T> list_;
  absl::flat_hash_map<T, int> map_;
};

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_VECTOR_MAP_H_
