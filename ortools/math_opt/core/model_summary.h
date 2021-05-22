// Copyright 2010-2021 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CORE_MODEL_SUMMARY_H_
#define OR_TOOLS_MATH_OPT_CORE_MODEL_SUMMARY_H_

#include <cstdint>
#include <initializer_list>
#include <list>
#include <string>
#include <utility>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/map_util.h"

namespace operations_research {
namespace math_opt {

// Maintains a bidirectional mapping between names and ids, e.g. as used for
// variables and linear constraints.
//
// The following invariants are enforced:
//  * Ids must be unique and increasing (in insertion order).
//  * Names must be either empty or unique.
class IdNameBiMap {
 public:
  IdNameBiMap() = default;

  // This constructor CHECKs that the input ids are sorted in increasing
  // order. This constructor is expected to be used only for unit tests of
  // validation code.
  IdNameBiMap(std::initializer_list<std::pair<int64_t, absl::string_view>> ids);

  // Will CHECK fail if id is <= largest_id().
  // Will CHECK fail if id is present or if name is nonempty and present.
  inline void Insert(int64_t id, std::string name);

  // Will CHECK fail if id is not present.
  inline void Erase(int64_t id);

  inline bool HasId(int64_t id) const;
  inline bool HasName(absl::string_view name) const;
  inline bool Empty() const;
  inline int Size() const;
  inline int64_t LargestId() const;

  // Iteration order is in increasing id order.
  const gtl::linked_hash_map<int64_t, std::string>& id_to_name() const {
    return id_to_name_;
  }
  const absl::flat_hash_map<absl::string_view, int64_t>& nonempty_name_to_id()
      const {
    return nonempty_name_to_id_;
  }

 private:
  // Pointer stability for name strings and iterating in insertion order are
  // both needed (so we do not use flat_hash_map).
  gtl::linked_hash_map<int64_t, std::string> id_to_name_;
  absl::flat_hash_map<absl::string_view, int64_t> nonempty_name_to_id_;
};

struct ModelSummary {
  IdNameBiMap variables;
  IdNameBiMap linear_constraints;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

void IdNameBiMap::Insert(const int64_t id, std::string name) {
  if (!id_to_name_.empty()) {
    CHECK_GT(id, LargestId()) << name;
  }
  const auto [it, success] = id_to_name_.emplace(id, std::move(name));
  CHECK(success) << "id: " << id;
  const absl::string_view name_view(it->second);
  if (!name_view.empty()) {
    gtl::InsertOrDie(&nonempty_name_to_id_, name_view, id);
  }
}

void IdNameBiMap::Erase(const int64_t id) {
  const auto it = id_to_name_.find(id);
  CHECK(it != id_to_name_.end()) << id;
  const absl::string_view name_view(it->second);
  if (!name_view.empty()) {
    CHECK_EQ(1, nonempty_name_to_id_.erase(name_view))
        << "name: " << name_view << " id: " << id;
  }
  id_to_name_.erase(it);
}
bool IdNameBiMap::HasId(const int64_t id) const {
  return id_to_name_.contains(id);
}
bool IdNameBiMap::HasName(const absl::string_view name) const {
  CHECK(!name.empty());
  return nonempty_name_to_id_.contains(name);
}

bool IdNameBiMap::Empty() const { return id_to_name_.empty(); }

int IdNameBiMap::Size() const { return id_to_name_.size(); }

int64_t IdNameBiMap::LargestId() const {
  CHECK(!Empty());
  return id_to_name_.back().first;
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_MODEL_SUMMARY_H_
