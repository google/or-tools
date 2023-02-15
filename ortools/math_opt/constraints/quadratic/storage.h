// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_STORAGE_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_STORAGE_H_

#include <limits>
#include <string>
#include <vector>

#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

// Internal storage representation for a single quadratic constraint.
//
// Implements the interface specified for the `ConstraintData` parameter of
// `AtomicConstraintStorage`.
struct QuadraticConstraintData {
  using IdType = QuadraticConstraintId;
  using ProtoType = QuadraticConstraintProto;
  using UpdatesProtoType = QuadraticConstraintUpdatesProto;

  // The `in_proto` must be in a valid state; see the inline comments on
  // `QuadraticConstraintProto` for details.
  static QuadraticConstraintData FromProto(const ProtoType& in_proto);
  ProtoType Proto() const;
  std::vector<VariableId> RelatedVariables() const;
  void DeleteVariable(VariableId var);

  double lower_bound = -std::numeric_limits<double>::infinity();
  double upper_bound = std::numeric_limits<double>::infinity();
  SparseCoefficientMap linear_terms;
  SparseSymmetricMatrix quadratic_terms;
  std::string name;
};

template <>
struct AtomicConstraintTraits<QuadraticConstraintId> {
  using ConstraintData = QuadraticConstraintData;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_STORAGE_H_
