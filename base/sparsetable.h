// Copyright 2010 Google
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

#ifndef BASE_SPARSETABLE_H
#define BASE_SPARSETABLE_H

#include <vector>
#include "base/logging.h"

namespace operations_research {
// This class implement a simple block based sparse vector.
template <class T> class sparsetable {
 public:
  sparsetable() : size_(0) {}

  void resize(int new_size) {
    CHECK_GE(new_size, 0);
    size_ = new_size_;
    elements_.resize((size_ + kBlockSize - 1) / kBlockSize);
  }

  const T& get(int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, size_);
    return elements_[index / kBlockSize][index % kBlockSize];
  }

  void set(int index, const T& elem) {
    elements_[index / kBlockSize][index % kBlockSize] = elem;
  }

  int size() const { return size_; }
 private:
  static const int kBlockSize = 16;

  int size_;
  vector<vector<T> > elements_;
};
}

#endir // BASE_SPARSETABLE_H
