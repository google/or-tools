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

#ifndef OR_TOOLS_BASE_SPARSETABLE_H_
#define OR_TOOLS_BASE_SPARSETABLE_H_

#include <vector>
#include "ortools/base/logging.h"

namespace operations_research {
// This class implements a simple block-based sparse std::vector.
template <class T>
class sparsetable {
 public:
  sparsetable() : size_(0) {}

  void resize(int new_size) {
    CHECK_GE(new_size, 0);
    size_ = new_size;
    const int reduced_size = (size_ + kBlockSize - 1) / kBlockSize;
    if (reduced_size != elements_.size()) {
      elements_.resize(reduced_size);
      masks_.resize(reduced_size, 0U);
    }
  }

  const T& get(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, size_);
    DCHECK(test(index));
    return elements_[index / kBlockSize][index % kBlockSize];
  }

  void set(int index, const T& elem) {
    const int offset = index / kBlockSize;
    const int pos = index % kBlockSize;
    if (elements_[offset].size() == 0) {
      elements_[offset].resize(kBlockSize);
    }
    elements_[offset][index % kBlockSize] = elem;
    masks_[offset] |= (1U << pos);
  }

  bool test(int index) const {
    const int offset = index / kBlockSize;
    const int pos = index % kBlockSize;
    return ((masks_[offset] & (1U << pos)) != 0);
  }

  int size() const { return size_; }

 private:
  static const int kBlockSize = 32;

  int size_;
  std::vector<std::vector<T> > elements_;
  std::vector<uint32> masks_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_SPARSETABLE_H_
