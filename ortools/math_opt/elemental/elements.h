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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"
// IWYU pragma: friend "ortools/math_opt/elemental/.*"

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTS_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTS_H_

#include <array>
#include <ostream>
#include <type_traits>

#include "absl/strings/string_view.h"
#include "ortools/math_opt/elemental/tagged_id.h"

namespace operations_research::math_opt {

enum class ElementType {
  kVariable,
  kLinearConstraint,
  kAuxiliaryObjective,
  kQuadraticConstraint,
  kIndicatorConstraint,
};
constexpr auto kElements = std::to_array(
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

template <ElementType element_type>
using ElementId = TaggedId<element_type>;

template <ElementType element_type, typename Container>
using ElementIdsConstView = TaggedIdsConstView<element_type, Container>;

template <ElementType element_type>
using ElementIdsVector = TaggedIdsVector<element_type>;

template <ElementType element_type>
using ElementIdsSpan = TaggedIdsSpan<element_type>;

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

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTS_H_
