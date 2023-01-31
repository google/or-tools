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

#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_STORAGE_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_STORAGE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sorted.h"

namespace operations_research::math_opt {
namespace internal {

// Internal storage representation for a single SOS constraint.
//
// Implements the interface specified for the `ConstraintData` parameter of
// `AtomicConstraintStorage`.
template <typename ConstraintId>
class SosConstraintData {
 public:
  using IdType = ConstraintId;
  using ProtoType = SosConstraintProto;
  using UpdatesProtoType = SosConstraintUpdatesProto;

  static_assert(
      std::disjunction_v<std::is_same<ConstraintId, Sos1ConstraintId>,
                         std::is_same<ConstraintId, Sos2ConstraintId>>,
      "ID type may only be Sos1ConstraintId or Sos2ConstraintId");

  struct LinearExpression {
    absl::flat_hash_map<VariableId, double> terms;
    double offset = 0.0;
  };

  // `weights` must either be empty or the same length as `expressions`. If it
  // is empty, default weights of 1, 2, ... will be used.
  SosConstraintData(std::vector<LinearExpression> expressions,
                    std::vector<double> weights, std::string name)
      : expressions_(std::move(expressions)), name_(std::move(name)) {
    if (!weights.empty()) {
      CHECK_EQ(weights.size(), expressions_.size());
      weights_ = std::move(weights);
    }
  }

  // The `in_proto` must be in a valid state; see the inline comments on
  // `SosConstraintProto` for details.
  static SosConstraintData FromProto(const ProtoType& in_proto);
  ProtoType Proto() const;
  std::vector<VariableId> RelatedVariables() const;
  void DeleteVariable(VariableId var);

  bool has_weights() const { return weights_.has_value(); }

  double weight(const int index) const {
    AssertInbounds(index);
    return weights_.has_value() ? (*weights_)[index] : index + 1;
  }
  const LinearExpression& expression(const int index) const {
    AssertInbounds(index);
    return expressions_[index];
  }
  int64_t num_expressions() const { return expressions_.size(); }
  const std::string& name() const { return name_; }

 private:
  SosConstraintData() = default;
  void AssertInbounds(const int index) const {
    CHECK_GE(index, 0);
    CHECK_LT(index, expressions_.size());
  }
  // If present, length must be the same as that of `expressions_`.
  // If absent, default weights of 1, 2, ... are used.
  std::optional<std::vector<double>> weights_;
  std::vector<LinearExpression> expressions_;
  std::string name_;
};

}  // namespace internal

using Sos1ConstraintData = internal::SosConstraintData<Sos1ConstraintId>;
using Sos2ConstraintData = internal::SosConstraintData<Sos2ConstraintId>;

template <>
struct AtomicConstraintTraits<Sos1ConstraintId> {
  using ConstraintData = Sos1ConstraintData;
};

template <>
struct AtomicConstraintTraits<Sos2ConstraintId> {
  using ConstraintData = Sos2ConstraintData;
};

////////////////////////////////////////////////////////////////////////////////
// Inline implementations
////////////////////////////////////////////////////////////////////////////////

namespace internal {

template <typename ConstraintId>
SosConstraintData<ConstraintId> SosConstraintData<ConstraintId>::FromProto(
    const ProtoType& in_proto) {
  const int num_expressions = in_proto.expressions_size();
  SosConstraintData data;
  data.name_ = in_proto.name();
  for (int i = 0; i < num_expressions; ++i) {
    LinearExpression& expression = data.expressions_.emplace_back();
    const LinearExpressionProto& proto_expression = in_proto.expressions(i);
    expression.offset = proto_expression.offset();
    for (int j = 0; j < proto_expression.ids_size(); ++j) {
      expression.terms.insert({VariableId(proto_expression.ids(j)),
                               proto_expression.coefficients(j)});
    }
  }
  // Otherwise proto has default weights, so leave data.weights_ as unset.
  if (!in_proto.weights().empty()) {
    data.weights_.emplace().reserve(num_expressions);
    for (int i = 0; i < num_expressions; ++i) {
      data.weights_->push_back(in_proto.weights(i));
    }
  }
  return data;
}

template <typename ConstraintId>
typename SosConstraintData<ConstraintId>::ProtoType
SosConstraintData<ConstraintId>::Proto() const {
  ProtoType constraint;
  constraint.set_name(name());
  for (int i = 0; i < num_expressions(); ++i) {
    const LinearExpression& expr = expression(i);
    LinearExpressionProto& proto_expr = *constraint.add_expressions();
    proto_expr.set_offset(expr.offset);
    for (const VariableId id : SortedMapKeys(expr.terms)) {
      proto_expr.add_ids(id.value());
      proto_expr.add_coefficients(expr.terms.at(id));
    }
  }
  if (weights_.has_value()) {
    for (int i = 0; i < num_expressions(); ++i) {
      constraint.add_weights(weight(i));
    }
  }
  return constraint;
}

template <typename ConstraintId>
std::vector<VariableId> SosConstraintData<ConstraintId>::RelatedVariables()
    const {
  absl::flat_hash_set<VariableId> vars;
  for (const LinearExpression& expression : expressions_) {
    for (const auto [var, _] : expression.terms) {
      vars.insert(var);
    }
  }
  return std::vector<VariableId>(vars.begin(), vars.end());
}

template <typename ConstraintId>
void SosConstraintData<ConstraintId>::DeleteVariable(const VariableId var) {
  for (LinearExpression& expression : expressions_) {
    expression.terms.erase(var);
  }
}

}  // namespace internal
}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_STORAGE_H_
