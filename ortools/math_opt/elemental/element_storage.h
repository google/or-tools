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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_STORAGE_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_STORAGE_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_builder.h"

namespace operations_research::math_opt {

namespace detail {

// A dense element storage, for use when no elements have been erased.
// Same API as `ElementStorage`, but no deletion.
// TODO(b/369972336): We should stay in dense mode if we have a small percentage
// of deletions.
class DenseElementStorage {
 public:
  int64_t Add(const absl::string_view name) {
    const int64_t id = elements_.size();
    elements_.emplace_back(name);
    return id;
  }

  bool exists(const int64_t id) const {
    return 0 <= id && id < elements_.size();
  }

  absl::StatusOr<absl::string_view> GetName(const int64_t id) const {
    if (exists(id)) {
      return elements_[id];
    }
    return util::InvalidArgumentErrorBuilder() << "no element with id " << id;
  }

  int64_t next_id() const { return size(); }

  std::vector<int64_t> AllIds() const;

  int64_t size() const { return elements_.size(); }

 private:
  friend class SparseElementStorage;
  std::vector<std::string> elements_;
};

// A sparse element storage, which supports deletion.
class SparseElementStorage {
 public:
  explicit SparseElementStorage(DenseElementStorage&& dense);

  int64_t Add(const absl::string_view name) {
    const int64_t id = next_id_;
    elements_.try_emplace(id, name);
    ++next_id_;
    return id;
  }

  bool Erase(int64_t id) { return elements_.erase(id) > 0; }

  bool exists(int64_t id) const { return elements_.contains(id); }

  absl::StatusOr<absl::string_view> GetName(int64_t id) const {
    if (const auto it = elements_.find(id); it != elements_.end()) {
      return it->second;
    }
    return util::InvalidArgumentErrorBuilder() << "no element with id " << id;
  }

  int64_t next_id() const { return next_id_; }

  std::vector<int64_t> AllIds() const;

  int64_t size() const { return elements_.size(); }

  void ensure_next_id_at_least(int64_t id) {
    next_id_ = std::max(next_id_, id);
  }

 private:
  absl::flat_hash_map<int64_t, std::string> elements_;
  int64_t next_id_ = 0;
};

}  // namespace detail

class ElementStorage {
  // Functions with deduced return must be defined before they are used.
 private:
  // std::visit is very slow, see yaqs/5253596299885805568.
  //
  // This function is static, taking Self as template argument, to avoid
  // having const and non-const versions. Post C++ 23, prefer:
  // https://en.cppreference.com/w/cpp/language/member_functions#Explicit_object_member_functions
  template <typename Self, typename Fn>
  static auto Visit(Self& self, Fn fn) {
    if (std::holds_alternative<detail::DenseElementStorage>(self.impl_)) {
      return fn(std::get<detail::DenseElementStorage>(self.impl_));
    } else {
      return fn(std::get<detail::SparseElementStorage>(self.impl_));
    }
  }

 public:
  // We start with a dense storage, which is more efficient, and switch to a
  // sparse storage when an element is erased.
  ElementStorage() : impl_(detail::DenseElementStorage()) {}

  // Creates a new element and returns its id.
  int64_t Add(const absl::string_view name) {
    return Visit(*this, [name](auto& impl) { return impl.Add(name); });
  }

  // Deletes an element by id, returning true on success and false if no element
  // was deleted (it was already deleted or the id was not from any existing
  // element).
  bool Erase(const int64_t id) { return AsSparse().Erase(id); }

  // Returns true an element with this id was created and not yet erased.
  bool Exists(const int64_t id) const {
    return Visit(*this, [id](auto& impl) { return impl.exists(id); });
  }

  // Returns the name of this element, or CHECK fails if no element with this id
  // exists.
  absl::StatusOr<absl::string_view> GetName(const int64_t id) const {
    return Visit(*this, [id](auto& impl) { return impl.GetName(id); });
  }

  // Returns the id that will be used for the next element added.
  //
  // NOTE: when no elements have been erased, this equals size().
  int64_t next_id() const {
    return Visit(*this, [](auto& impl) { return impl.next_id(); });
  }

  // Returns all ids of all elements in the model in an unsorted,
  // non-deterministic order.
  std::vector<int64_t> AllIds() const;

  // Returns the number of elements added and not erased.
  int64_t size() const {
    return Visit(*this, [](auto& impl) { return impl.size(); });
  }

  // Increases next_id() to `id` if it is currently less than `id`.
  //
  // Useful for reading a model back from proto, most users should not need to
  // call this directly.
  void EnsureNextIdAtLeast(const int64_t id) {
    if (id > next_id()) {
      AsSparse().ensure_next_id_at_least(id);
    }
  }

 private:
  detail::SparseElementStorage& AsSparse() {
    if (auto* sparse = std::get_if<detail::SparseElementStorage>(&impl_)) {
      return *sparse;
    }
    impl_ = detail::SparseElementStorage(
        std::move(std::get<detail::DenseElementStorage>(impl_)));
    return std::get<detail::SparseElementStorage>(impl_);
  }

  std::variant<detail::DenseElementStorage, detail::SparseElementStorage> impl_;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_STORAGE_H_
