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

#ifndef ORTOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINTS_V2_H_
#define ORTOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINTS_V2_H_

#include <string>

#include "absl/log/check.h"
#include "ortools/math_opt/constraints/indicator/storage.h"
#include "ortools/math_opt/constraints/quadratic/storage.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/atomic_constraint_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt::internal {

template <typename ConstraintData>
typename ConstraintData::IdType AddAtomicConstraint(const ConstraintData& data,
                                                    Elemental& elemental);

template <ElementType e>
typename AtomicConstraintTraits<ElementId<e>>::ConstraintData
GetAtomicConstraint(ElementId<e> id, const Elemental& elemental);

////////////////////////////////////////////////////////////////////////////////
// Quadratic constraints
////////////////////////////////////////////////////////////////////////////////

template <>
inline QuadraticConstraintId AddAtomicConstraint(
    const QuadraticConstraintData& data, Elemental& elemental) {
  const QuadraticConstraintId con =
      elemental.AddElement<ElementType::kQuadraticConstraint>(data.name);
  elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kQuadConLb, AttrKey(con),
                                         data.lower_bound);
  elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kQuadConUb, AttrKey(con),
                                         data.upper_bound);
  for (const auto [var, coef] : data.linear_terms.terms()) {
    elemental.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(con, var), coef);
  }
  for (const auto [v1, v2, coef] : data.quadratic_terms.Terms()) {
    using Key = AttrKeyFor<SymmetricDoubleAttr3>;
    elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, Key(con, v1, v2),
                      coef);
  }
  return con;
}

template <>
inline QuadraticConstraintData GetAtomicConstraint(QuadraticConstraintId id,
                                                   const Elemental& elemental) {
  QuadraticConstraintData result;
  const auto name = elemental.GetElementName(id);
  CHECK_OK(name) << "quadratic constraint with id: " << id
                 << " is not in the model";
  result.name = std::string(*name);
  result.lower_bound = elemental.GetAttr(DoubleAttr1::kQuadConLb, AttrKey(id));
  result.upper_bound = elemental.GetAttr(DoubleAttr1::kQuadConUb, AttrKey(id));
  for (const AttrKey<2> key :
       elemental.Slice<0>(DoubleAttr2::kQuadConLinCoef, id.value())) {
    const VariableId var{key[1]};
    const double coef = elemental.GetAttr(DoubleAttr2::kQuadConLinCoef, key);
    result.linear_terms.set(var, coef);
  }
  using QuadKey = AttrKeyFor<SymmetricDoubleAttr3>;
  for (const QuadKey key :
       elemental.Slice<0>(SymmetricDoubleAttr3::kQuadConQuadCoef, id.value())) {
    const VariableId var1{key[1]};
    const VariableId var2{key[2]};
    const double coef =
        elemental.GetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, key);
    result.quadratic_terms.set(var1, var2, coef);
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Indicator constraints
////////////////////////////////////////////////////////////////////////////////

template <>
inline IndicatorConstraintId AddAtomicConstraint(
    const IndicatorConstraintData& data, Elemental& elemental) {
  const IndicatorConstraintId con =
      elemental.AddElement<ElementType::kIndicatorConstraint>(data.name);
  elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kIndConLb, AttrKey(con),
                                         data.lower_bound);
  elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kIndConUb, AttrKey(con),
                                         data.upper_bound);
  for (const auto [var, coef] : data.linear_terms.terms()) {
    elemental.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(con, var), coef);
  }
  elemental.SetAttr<Elemental::UBPolicy>(BoolAttr1::kIndConActivateOnZero,
                                         AttrKey(con), data.activate_on_zero);
  if (data.indicator.has_value()) {
    elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(con),
                      *data.indicator);
  }
  return con;
}

template <>
inline IndicatorConstraintData GetAtomicConstraint(IndicatorConstraintId id,
                                                   const Elemental& elemental) {
  IndicatorConstraintData result;
  const auto name = elemental.GetElementName(id);
  CHECK_OK(name) << "indicator constraint with id: " << id
                 << " is not in the model";
  result.name = std::string(*name);
  const AttrKey<1> con_key(id);
  result.lower_bound = elemental.GetAttr(DoubleAttr1::kIndConLb, con_key);
  result.upper_bound = elemental.GetAttr(DoubleAttr1::kIndConUb, con_key);
  for (const AttrKey<2> key :
       elemental.Slice<0>(DoubleAttr2::kIndConLinCoef, id.value())) {
    const VariableId var{key[1]};
    const double coef = elemental.GetAttr(DoubleAttr2::kIndConLinCoef, key);
    result.linear_terms.set(var, coef);
  }
  result.activate_on_zero =
      elemental.GetAttr(BoolAttr1::kIndConActivateOnZero, con_key);
  const VariableId ind =
      elemental.GetAttr(VariableAttr1::kIndConIndicator, con_key);
  if (ind.IsValid()) {
    result.indicator = ind;
  }
  return result;
}

}  // namespace operations_research::math_opt::internal

#endif  // ORTOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINTS_V2_H_
