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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_LINEAR_EXPRESSION_DATA_H_
#define OR_TOOLS_MATH_OPT_STORAGE_LINEAR_EXPRESSION_DATA_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/sorted.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

// Represents a linear expression in "raw ID" form.
//
// The data storage is not interesting, this struct exists to provide helpers
// that go to/from the proto representation (via member functions) and the C++
// model representations (via raw functions in `constraints/util/model_util.h`).
struct LinearExpressionData {
  inline LinearExpressionProto Proto() const;
  // This method assumes that `expr_proto` is in a valid state; see the inline
  // comments for `LinearExpressionProto` for details.
  inline static LinearExpressionData FromProto(
      const LinearExpressionProto& expr_proto);

  SparseCoefficientMap coeffs;
  double offset = 0.0;
};

// Inline implementations.
LinearExpressionProto LinearExpressionData::Proto() const {
  LinearExpressionProto proto_expr;
  proto_expr.set_offset(offset);
  {
    const int num_terms = static_cast<int>(coeffs.terms().size());
    proto_expr.mutable_ids()->Reserve(num_terms);
    proto_expr.mutable_coefficients()->Reserve(num_terms);
  }
  for (const VariableId id : SortedMapKeys(coeffs.terms())) {
    proto_expr.add_ids(id.value());
    proto_expr.add_coefficients(coeffs.get(id));
  }
  return proto_expr;
}

LinearExpressionData LinearExpressionData::FromProto(
    const LinearExpressionProto& expr_proto) {
  LinearExpressionData expr_data{.offset = expr_proto.offset()};
  for (int i = 0; i < expr_proto.ids_size(); ++i) {
    expr_data.coeffs.set(VariableId(expr_proto.ids(i)),
                         expr_proto.coefficients(i));
  }
  return expr_data;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_LINEAR_EXPRESSION_DATA_H_
