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

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"

namespace operations_research {
namespace math_opt {

// Maintains a bidirectional mapping between names and ids, e.g. as used for
// variables and linear constraints.
//
// The following invariants are enforced:
//  * Ids must be unique and increasing (in insertion order).
//  * Ids are non-negative.
//  * Ids are not equal to std::numeric_limits<int64_t>::max()
//  * Ids removed are never reused.
//  * Names must be either empty or unique.
class IdNameBiMap {
 public:
  IdNameBiMap() = default;

  // This constructor CHECKs that the input ids are sorted in increasing
  // order. This constructor is expected to be used only for unit tests of
  // validation code.
  IdNameBiMap(std::initializer_list<std::pair<int64_t, absl::string_view>> ids);

  // Inserts the provided id and associate the provided name to it. CHECKs that
  // id >= next_free_id() and that when the name is nonempty it is not already
  // present. As a side effect it updates next_free_id to id + 1.
  inline void Insert(int64_t id, std::string name);

  // Removes the given id. CHECKs that it is present.
  inline void Erase(int64_t id);

  inline bool HasId(int64_t id) const;
  inline bool HasName(absl::string_view name) const;
  inline bool Empty() const;
  inline int Size() const;

  // The next id that has never been used (0 initially since ids are
  // non-negative).
  inline int64_t next_free_id() const;

  // Updates next_free_id(). CHECKs that the provided id is greater than any
  // exiting id and non negative.
  //
  // In practice this should only be used to increase the next_free_id() value
  // in cases where a ModelSummary is built with an existing model but we know
  // some ids of removed elements have already been used.
  inline void SetNextFreeId(int64_t new_next_free_id);

  // Iteration order is in increasing id order.
  const gtl::linked_hash_map<int64_t, std::string>& id_to_name() const {
    return id_to_name_;
  }
  const absl::flat_hash_map<absl::string_view, int64_t>& nonempty_name_to_id()
      const {
    return nonempty_name_to_id_;
  }

 private:
  // Next unused id.
  int64_t next_free_id_ = 0;

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
  CHECK_GE(id, next_free_id_);
  CHECK_LT(id, std::numeric_limits<int64_t>::max());
  next_free_id_ = id + 1;

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

int64_t IdNameBiMap::next_free_id() const { return next_free_id_; }

void IdNameBiMap::SetNextFreeId(const int64_t new_next_free_id) {
  if (!Empty()) {
    const int64_t largest_id = id_to_name_.back().first;
    CHECK_GT(new_next_free_id, largest_id);
  } else {
    CHECK_GE(new_next_free_id, 0);
  }
  next_free_id_ = new_next_free_id;
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_MODEL_SUMMARY_H_
