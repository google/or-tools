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

#include "ortools/math_opt/elemental/element_storage.h"

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"

namespace operations_research::math_opt {

namespace detail {

std::vector<int64_t> SparseElementStorage::AllIds() const {
  std::vector<int64_t> result;
  result.reserve(elements_.size());
  for (const auto& [id, unused] : elements_) {
    result.push_back(id);
  }
  return result;
}

std::vector<int64_t> DenseElementStorage::AllIds() const {
  std::vector<int64_t> result(elements_.size());
  absl::c_iota(result, 0);
  return result;
}

SparseElementStorage::SparseElementStorage(DenseElementStorage&& dense)
    : next_id_(dense.size()) {
  elements_.reserve(next_id_);
  for (int i = 0; i < next_id_; ++i) {
    elements_.emplace(i, std::move(dense.elements_[i]));
  }
}

}  // namespace detail

std::vector<int64_t> ElementStorage::AllIds() const {
  return std::visit([](const auto& impl) { return impl.AllIds(); }, impl_);
}

}  // namespace operations_research::math_opt
