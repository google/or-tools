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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENTS_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENTS_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/array.h"

namespace operations_research::math_opt {

enum class ElementType {
  kVariable,
  kLinearConstraint,
  kAuxiliaryObjective,
  kQuadraticConstraint,
  kIndicatorConstraint,
};
constexpr auto kElements = gtl::to_array(
    {ElementType::kVariable, ElementType::kLinearConstraint,
     ElementType::kAuxiliaryObjective, ElementType::kQuadraticConstraint,
     ElementType::kIndicatorConstraint});
constexpr int kNumElements = static_cast<int>(kElements.size());
constexpr absl::string_view kElementNames[kNumElements] = {
    "variable", "linear_constraint", "auxiliary_objective",
    "quadratic_constraint", "indicator_constraint"};
// Short names, typically to fit more things on a screen when debugging. Not
// part of the API, can be changed if needed.
constexpr absl::string_view kShortElementNames[kNumElements] = {
    "var", "lin_con", "aux_obj", "quad_con", "indic_con"};

absl::string_view ToString(ElementType element_type);
std::ostream& operator<<(std::ostream& ostr, ElementType element_type);

template <typename Sink>
void AbslStringify(Sink& sink, ElementType element_type) {
  sink.Append(ToString(element_type));
}

// A strongly typed element id. Value type.
template <ElementType element_type>
class ElementId final {
 public:
  // Constructs an invalid element id.
  constexpr ElementId() : id_(-1) {}

  // Constructs a valid element id. `DCHECK`s that id is non-negative.
  constexpr explicit ElementId(int64_t id) : id_(id) {
    CHECK_GE(id, 0) << "negative " << element_type << " id: " << id;
  }

  constexpr bool IsValid() const { return id_ >= 0; }

  // Returns the raw id value.
  int64_t value() const { return id_; }

  static constexpr ElementType type() { return element_type; }

  friend bool operator==(const ElementId& lhs, const ElementId& rhs) {
    return lhs.id_ == rhs.id_;
  }

  friend bool operator!=(const ElementId& lhs, const ElementId& rhs) {
    return lhs.id_ != rhs.id_;
  }

  friend bool operator<(const ElementId& lhs, const ElementId& rhs) {
    return lhs.id_ < rhs.id_;
  }

  friend bool operator<=(const ElementId& lhs, const ElementId& rhs) {
    return lhs.id_ <= rhs.id_;
  }

  friend bool operator>(const ElementId& lhs, const ElementId& rhs) {
    return lhs.id_ > rhs.id_;
  }

  friend bool operator>=(const ElementId& lhs, const ElementId& rhs) {
    return lhs.id_ >= rhs.id_;
  }

  // We don't support addition between `ElementId`s: what does it mean to add
  // indices ? We do support getting the next element id though.
  ElementId Next() const {
    DCHECK(IsValid());
    return ElementId(id_ + 1);
  }

  // `MakeStrongIntRange()` relies on `operator++` being present. Prefer the
  // more explicit `Next()` in general.
  ElementId operator++() {
    DCHECK(IsValid());
    ++id_;
    return *this;
  }

  template <typename H>
  friend H AbslHashValue(H h, const ElementId& a) {
    return H::combine(std::move(h), a.id_);
  }

  using ValueType = int64_t;  // Support for `StrongVector<ElementId>`.

 private:
  int64_t id_;
};

template <ElementType element_type>
std::string ToString(const ElementId<element_type>& id) {
  if (id.IsValid()) {
    return absl::StrFormat("%s{%i}", ToString(id.type()), id.value());
  } else {
    return absl::StrFormat("%s{invalid}", ToString(id.type()));
  }
}

template <ElementType element_type>
std::ostream& operator<<(std::ostream& ostr,
                         const ElementId<element_type>& id) {
  ostr << ToString(id);
  return ostr;
}

template <typename Sink, ElementType element_type>
void AbslStringify(Sink& sink, const ElementId<element_type>& id) {
  sink.Append(ToString(id));
}

// An adaptor to expose a sequential container of `int64_t` as strongly typed
// element ids.
// Does not own the container.
template <ElementType element_type, typename Container>
class ElementIdsConstView final {
 public:
  using value_type = ElementId<element_type>;

  explicit ElementIdsConstView(
      const Container* container ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : container_(*container) {}

  size_t size() const { return container_.size(); }
  bool empty() const { return container_.empty(); }

  ElementId<element_type> operator[](size_t i) const {
    return ElementId<element_type>(container_[i]);
  }

  auto begin() const { return Iterator(container_.begin()); }
  auto end() const { return Iterator(container_.end()); }

 private:
  class Iterator final {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = ElementId<element_type>;

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

// A container that exposes a container of `int64_t` as strongly typed element
// ids.
template <ElementType element_type>
class ElementIdsVector final {
 public:
  using Container = std::vector<int64_t>;
  using View = ElementIdsConstView<element_type, Container>;
  using value_type = typename View::value_type;

  explicit ElementIdsVector(Container&& container)
      : container_(std::move(container)) {}

  // This is move-constructible only.
  ElementIdsVector(ElementIdsVector&&) noexcept = default;

  View view() const { return View(&container_); }

  size_t size() const { return container_.size(); }
  bool empty() const { return container_.empty(); }

  ElementId<element_type> operator[](size_t i) const {
    return ElementId<element_type>(container_[i]);
  }

  auto begin() const { return view().begin(); }
  auto end() const { return view().end(); }

  ElementIdsVector(const ElementIdsVector& container) = delete;
  ElementIdsVector& operator=(const ElementIdsVector& container) = delete;
  ElementIdsVector& operator=(ElementIdsVector&& container) = delete;

  // Provides access to the untyped container.
  Container& container() { return container_; }

 private:
  Container container_;
};

template <ElementType element_type>
using ElementIdsSpan = typename ElementIdsVector<element_type>::View;

// Traits to detect whether `T` is an `ElementId`.
template <typename T>
struct is_element_id : public std::false_type {};

template <ElementType element_type>
struct is_element_id<ElementId<element_type>> : public std::true_type {};

template <typename T>
static constexpr inline bool is_element_id_v = is_element_id<T>::value;

using VariableId = ElementId<ElementType::kVariable>;
using LinearConstraintId = ElementId<ElementType::kLinearConstraint>;
using AuxiliaryObjectiveId = ElementId<ElementType::kAuxiliaryObjective>;
using QuadraticConstraintId = ElementId<ElementType::kQuadraticConstraint>;
using IndicatorConstraintId = ElementId<ElementType::kIndicatorConstraint>;

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENTS_H_
