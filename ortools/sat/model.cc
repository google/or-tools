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

#include "ortools/sat/model.h"

#include <cstddef>

#include "absl/log/check.h"

namespace operations_research {
namespace sat {

void Model::AddNewSingleton(void* new_element, size_t type_id) {
  CHECK(singletons_.emplace(type_id, new_element).second)
      << "Duplicate type id: " << type_id;
}

void* Model::GetSingletonOrNullptr(size_t type_id) const {
  const auto it = singletons_.find(type_id);
  return it != singletons_.end() ? it->second : nullptr;
}

}  // namespace sat
}  // namespace operations_research
