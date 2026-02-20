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

#ifndef ORTOOLS_SAT_CP_MODEL_MAPPING_H_
#define ORTOOLS_SAT_CP_MODEL_MAPPING_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// For an optimization problem, this contains the internal integer objective
// to minimize and information on how to display it correctly in the logs.
struct ObjectiveDefinition {
  double scaling_factor = 1.0;
  double offset = 0.0;
  IntegerVariable objective_var = kNoIntegerVariable;

  // The objective linear expression that should be equal to objective_var.
  // If not all proto variable have an IntegerVariable view, then some vars
  // will be set to kNoIntegerVariable. In practice, when this is used, we make
  // sure there is a view though.
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs;

  // List of variable that when set to their lower bound should help getting a
  // better objective. This is used by some search heuristic to preferably
  // assign any of the variable here to their lower bound first.
  absl::flat_hash_set<IntegerVariable> objective_impacting_variables;

  double ScaleIntegerObjective(IntegerValue value) const {
    return (ToDouble(value) + offset) * scaling_factor;
  }

  double ScaleObjective(double value) const {
    return (value + offset) * scaling_factor;
  }
};

// Holds the mapping between CpModel proto indices and the sat::model ones.
//
// This also holds some information used when loading a CpModel proto.
class CpModelMapping {
 public:
  // Returns true if the given CpModelProto variable reference refers to a
  // Boolean variable. Such variable will always have an associated Literal(),
  // but not always an associated Integer().
  bool IsBoolean(int ref) const {
    DCHECK_LT(PositiveRef(ref), booleans_.size());
    return booleans_[PositiveRef(ref)] != kNoBooleanVariable;
  }

  bool IsInteger(int ref) const {
    DCHECK_LT(PositiveRef(ref), integers_.size());
    return integers_[PositiveRef(ref)] != kNoIntegerVariable;
  }

  sat::Literal Literal(int ref) const {
    DCHECK(IsBoolean(ref));
    return sat::Literal(booleans_[PositiveRef(ref)], RefIsPositive(ref));
  }

  IntegerVariable Integer(int ref) const {
    DCHECK(IsInteger(ref));
    const IntegerVariable var = integers_[PositiveRef(ref)];
    return RefIsPositive(ref) ? var : NegationOf(var);
  }

  // TODO(user): We could "easily" create an intermediate variable for more
  // complex linear expression. We could also identify duplicate expressions to
  // not create two identical integer variable.
  AffineExpression Affine(const LinearExpressionProto& exp) const {
    CHECK_LE(exp.vars().size(), 1);
    if (exp.vars().empty()) {
      return AffineExpression(IntegerValue(exp.offset()));
    }
    return AffineExpression(Integer(exp.vars(0)), IntegerValue(exp.coeffs(0)),
                            IntegerValue(exp.offset()));
  }

  IntervalVariable Interval(int i) const {
    CHECK_GE(i, 0);
    CHECK_LT(i, intervals_.size());
    CHECK_NE(intervals_[i], kNoIntervalVariable);
    return intervals_[i];
  }

  template <typename List>
  std::vector<IntegerVariable> Integers(const List& list) const {
    std::vector<IntegerVariable> result;
    result.reserve(list.size());
    for (const auto i : list) result.push_back(Integer(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<sat::Literal> Literals(const ProtoIndices& indices) const {
    std::vector<sat::Literal> result;
    result.reserve(indices.size());
    for (const int i : indices) result.push_back(CpModelMapping::Literal(i));
    return result;
  }

  template <typename List>
  std::vector<AffineExpression> Affines(const List& list) const {
    std::vector<AffineExpression> result;
    result.reserve(list.size());
    for (const auto& i : list) result.push_back(Affine(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<IntervalVariable> Intervals(const ProtoIndices& indices) const {
    std::vector<IntervalVariable> result;
    result.reserve(indices.size());
    for (const int i : indices) result.push_back(Interval(i));
    return result;
  }

  // Depending on the option, we will load constraints in stages. This is used
  // to detect constraints that are already loaded. For instance the interval
  // constraints and the linear constraint of size 1 (encodings) are usually
  // loaded first.
  bool ConstraintIsAlreadyLoaded(const ConstraintProto* ct) const {
    return already_loaded_ct_.contains(ct);
  }

  // Returns true if the given constraint is a fully reified var == value or
  // var >= value. We will deal with them differently during linearization.
  bool IsLinear1EncodingConstraint(const ConstraintProto* ct) const {
    return encoding_ct_.contains(ct);
  }

  // Note that both these functions returns positive reference or -1.
  int GetProtoVariableFromBooleanVariable(BooleanVariable var) const {
    if (var.value() >= reverse_boolean_map_.size()) return -1;
    return reverse_boolean_map_[var];
  }
  int GetProtoVariableFromIntegerVariable(IntegerVariable var) const {
    DCHECK(VariableIsPositive(var));
    const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
    if (index >= reverse_integer_map_.end_index()) return -1;
    return reverse_integer_map_[index];
  }

  // This one should only be used when we have a mapping.
  int GetProtoLiteralFromLiteral(sat::Literal lit) const {
    const int proto_var = GetProtoVariableFromBooleanVariable(lit.Variable());
    DCHECK_NE(proto_var, -1);
    return lit.IsPositive() ? proto_var : NegatedRef(proto_var);
  }

  const std::vector<IntegerVariable>& GetVariableMapping() const {
    return integers_;
  }

  LinearExpression GetExprFromProto(
      const LinearExpressionProto& expr_proto) const {
    LinearExpression expr;
    expr.vars = Integers(expr_proto.vars());
    for (int j = 0; j < expr_proto.coeffs_size(); ++j) {
      expr.coeffs.push_back(IntegerValue(expr_proto.coeffs(j)));
    }
    expr.offset = IntegerValue(expr_proto.offset());
    return CanonicalizeExpr(expr);
  }

  // Returns the min/max activity of the linear constraint under the current
  // integer_trail bounds.
  std::pair<int64_t, int64_t> ComputeMinMaxActivity(
      const LinearConstraintProto& proto, IntegerTrail* integer_trail) {
    int64_t sum_min = 0;
    int64_t sum_max = 0;

    for (int i = 0; i < proto.vars_size(); ++i) {
      const int64_t coeff = proto.coeffs(i);
      const IntegerVariable var = this->Integer(proto.vars(i));
      const int64_t lb = integer_trail->LowerBound(var).value();
      const int64_t ub = integer_trail->UpperBound(var).value();
      if (coeff >= 0) {
        sum_min += coeff * lb;
        sum_max += coeff * ub;
      } else {
        sum_min += coeff * ub;
        sum_max += coeff * lb;
      }
    }
    return {sum_min, sum_max};
  }

  // For logging only, these are not super efficient.
  int NumIntegerVariables() const {
    int result = 0;
    for (const IntegerVariable var : integers_) {
      if (var != kNoIntegerVariable) result++;
    }
    return result;
  }
  int NumBooleanVariables() const {
    int result = 0;
    for (const BooleanVariable var : booleans_) {
      if (var != kNoBooleanVariable) result++;
    }
    return result;
  }

  // This one do not count [0, 1] integers.
  int NumNonBooleanIntegers() const { return num_non_boolean_integers_; }

  // Returns the number of variables in the loaded proto.
  int NumProtoVariables() const { return integers_.size(); }

 private:
  friend void LoadVariables(const CpModelProto& model_proto,
                            bool view_all_booleans_as_integers, Model* m);
  friend void ExtractEncoding(const CpModelProto& model_proto, Model* m);

  // Note that only the variables used by at least one constraint will be
  // created, the other will have a kNo[Integer,Interval,Boolean]VariableValue.
  std::vector<IntegerVariable> integers_;
  std::vector<IntervalVariable> intervals_;
  std::vector<BooleanVariable> booleans_;

  // Recover from a IntervalVariable/BooleanVariable its associated CpModelProto
  // index. The value of -1 is used to indicate that there is no correspondence
  // (i.e. this variable is only used internally).
  util_intops::StrongVector<BooleanVariable, int> reverse_boolean_map_;
  util_intops::StrongVector<PositiveOnlyIndex, int> reverse_integer_map_;

  // Set of constraints to ignore because they were already dealt with by
  // ExtractEncoding().
  absl::flat_hash_set<const ConstraintProto*> already_loaded_ct_;

  // These are the linear1 that are either part of lit <=> X==value or lit <=> X
  // >= value. Because they are fully reified, they will be processed
  // differently during linearization.
  absl::flat_hash_set<const ConstraintProto*> encoding_ct_;

  int64_t num_non_boolean_integers_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_MODEL_MAPPING_H_
