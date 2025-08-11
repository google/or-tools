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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_TAGGED_ID_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_TAGGED_ID_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"

namespace operations_research::math_opt {

// A strongly typed int64_t where the tag is an a non-type template parameter
// (e.g. an enum value or an int).
//
// An id of -1 corresponds to an invalid id and is the result of the default
// constructor, otherwise, negative values are not allowed.
//
// The << operator must be defined for `tag`.
template <auto tag>
class TaggedId {
 public:
  // Constructs an invalid element id.
  constexpr TaggedId() : id_(-1) {}

  // Constructs a valid element id. `DCHECK`s that id is non-negative.
  constexpr explicit TaggedId(int64_t id) : id_(id) {
    CHECK_GE(id, 0) << "negative " << tag << " id: " << id;
  }

  constexpr bool IsValid() const { return id_ >= 0; }

  // Returns the raw id value.
  int64_t value() const { return id_; }

  static constexpr auto tag_value() { return tag; }

  friend bool operator==(const TaggedId& lhs, const TaggedId& rhs) {
    return lhs.id_ == rhs.id_;
  }

  friend bool operator!=(const TaggedId& lhs, const TaggedId& rhs) {
    return lhs.id_ != rhs.id_;
  }

  friend bool operator<(const TaggedId& lhs, const TaggedId& rhs) {
    return lhs.id_ < rhs.id_;
  }

  friend bool operator<=(const TaggedId& lhs, const TaggedId& rhs) {
    return lhs.id_ <= rhs.id_;
  }

  friend bool operator>(const TaggedId& lhs, const TaggedId& rhs) {
    return lhs.id_ > rhs.id_;
  }

  friend bool operator>=(const TaggedId& lhs, const TaggedId& rhs) {
    return lhs.id_ >= rhs.id_;
  }

  // We don't support addition between `TaggedId`s: what does it mean to add
  // indices? We do support getting the next element id though.
  TaggedId Next() const {
    DCHECK(IsValid());
    return TaggedId(id_ + 1);
  }

  // `MakeStrongIntRange()` relies on `operator++` being present. Prefer the
  // more explicit `Next()` in general.
  TaggedId operator++() {
    DCHECK(IsValid());
    ++id_;
    return *this;
  }

  template <typename H>
  friend H AbslHashValue(H h, const TaggedId& a) {
    return H::combine(std::move(h), a.id_);
  }

  using ValueType = int64_t;  // Support for `StrongVector<TaggedId<tag>>`.

 private:
  int64_t id_;
};

template <auto tag>
std::string ToString(const TaggedId<tag>& id) {
  if (id.IsValid()) {
    return absl::StrFormat("%s{%i}", absl::FormatStreamed(id.tag_value()),
                           id.value());
  } else {
    return absl::StrFormat("%s{invalid}", absl::FormatStreamed(id.tag_value()));
  }
}

template <auto tag>
std::ostream& operator<<(std::ostream& ostr, const TaggedId<tag>& id) {
  ostr << ToString(id);
  return ostr;
}

template <typename Sink, auto tag>
void AbslStringify(Sink& sink, const TaggedId<tag>& id) {
  sink.Append(ToString(id));
}

// An adaptor to expose a sequential container of `int64_t` as strongly typed
// ids.
//
// Does not own the container.
template <auto tag, typename Container>
class TaggedIdsConstView final {
 public:
  using value_type = TaggedId<tag>;

  explicit TaggedIdsConstView(
      const Container* container ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : container_(*container) {}

  size_t size() const { return container_.size(); }
  bool empty() const { return container_.empty(); }

  TaggedId<tag> operator[](size_t i) const {
    return TaggedId<tag>(container_[i]);
  }

  auto begin() const { return Iterator(container_.begin()); }
  auto end() const { return Iterator(container_.end()); }

 private:
  class Iterator final {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = TaggedId<tag>;

    Iterator() = default;
    explicit Iterator(typename Container::const_iterator it) : it_(it) {}

    value_type operator*() const { return value_type(*it_); }

    Iterator& operator++() {
      ++it_;
      return *this;
    }

    Iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.it_ == rhs.it_;
    }
    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
      return lhs.it_ != rhs.it_;
    }

   private:
    typename Container::const_iterator it_ = {};
  };
#if __cplusplus >= 202002L
  static_assert(std::forward_iterator<Iterator>);
#endif

  const Container& container_;
};

// A container that exposes a container of `int64_t` as strongly typed ids.
template <auto tag>
class TaggedIdsVector final {
 public:
  using Container = std::vector<int64_t>;
  using View = TaggedIdsConstView<tag, Container>;
  using value_type = typename View::value_type;

  explicit TaggedIdsVector(Container&& container)
      : container_(std::move(container)) {}

  // This is move-constructible only.
  TaggedIdsVector(TaggedIdsVector&&) noexcept = default;

  View view() const { return View(&container_); }

  size_t size() const { return container_.size(); }
  bool empty() const { return container_.empty(); }

  TaggedId<tag> operator[](size_t i) const {
    return TaggedId<tag>(container_[i]);
  }

  auto begin() const { return view().begin(); }
  auto end() const { return view().end(); }

  TaggedIdsVector(const TaggedIdsVector& container) = delete;
  TaggedIdsVector& operator=(const TaggedIdsVector& container) = delete;
  TaggedIdsVector& operator=(TaggedIdsVector&& container) = delete;

  // Provides access to the untyped container.
  Container& container() { return container_; }

 private:
  Container container_;
};

template <auto tag>
using TaggedIdsSpan = typename TaggedIdsVector<tag>::View;

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_TAGGED_ID_H_
