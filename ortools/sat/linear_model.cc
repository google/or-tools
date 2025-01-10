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

#include "ortools/sat/linear_model.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

namespace {

// This struct stores constraints of the form literal => var ==/!= value.
// It is meant to be in a sorted vector to detect complimentary equations.
struct EqualityDetectionHelper {
  int constraint_index;
  int literal;
  int64_t value;
  bool is_equality;  // false if != instead.

  bool operator<(const EqualityDetectionHelper& o) const {
    if (PositiveRef(literal) == PositiveRef(o.literal)) {
      if (value == o.value) return is_equality && !o.is_equality;
      return value < o.value;
    }
    return PositiveRef(literal) < PositiveRef(o.literal);
  }
};

// For a given variable. This struct stores the literal that encodes a value, as
// well as the indices of the two constraints in the model that implement
//     literal <=> var == value.
struct LitVarEncodingInfo {
  int lit;
  int positive_ct_index;
  int negative_ct_index;
};

// Struct use to store literal/value attached to a var. It is meant to be sorted
// by ascending value order.
struct ValueLiteralCtIndex {
  int64_t value;
  int literal;
  int ct_index;

  bool operator<(const ValueLiteralCtIndex& o) const {
    return (value < o.value) || (value == o.value && literal < o.literal);
  }
};

// All var == value (stored in model_proto_.constraints(ct_index)) for a given
// literal.
struct VarValueCtIndex {
  int var;
  int64_t value;
  int ct_index;
};

}  // namespace

LinearModel::LinearModel(const CpModelProto& model_proto)
    : model_proto_(model_proto),
      ignored_constraints_(model_proto.constraints_size(), false) {
  // TODO(user): Do we use the loader code to detect full encodings and
  // element encodings.
  absl::flat_hash_set<BoolArgumentProto> exactly_ones_cache;
  absl::flat_hash_set<LinearConstraintProto> encoding_cache;
  std::vector<std::vector<EqualityDetectionHelper>> var_to_equalities(
      model_proto_.variables_size());
  absl::flat_hash_map<int, std::vector<VarValueCtIndex>>
      literal_to_var_value_ct_indices;

  // Loop over all constraints and fill var_to_equalities.
  for (int c = 0; c < model_proto_.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto_.constraints(c);
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kExactlyOne) {
      BoolArgumentProto bool_arg = ct.exactly_one();
      // Sort literals to get a canonical constraint.
      std::sort(bool_arg.mutable_literals()->begin(),
                bool_arg.mutable_literals()->end());
      if (!exactly_ones_cache.insert(bool_arg).second) {
        ignored_constraints_[c] = true;
        ++num_ignored_constraints_;
      }
    }
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
      continue;
    }
    if (ct.enforcement_literal().size() != 1) continue;
    if (ct.linear().vars_size() != 1) continue;

    // ct is a linear constraint with one term and one enforcement literal.
    const int enforcement_literal = ct.enforcement_literal(0);
    const int ref = ct.linear().vars(0);
    const int var = PositiveRef(ref);

    const Domain domain = ReadDomainFromProto(model_proto_.variables(var));
    const Domain domain_if_enforced =
        ReadDomainFromProto(ct.linear())
            .InverseMultiplicationBy(ct.linear().coeffs(0) *
                                     (RefIsPositive(ref) ? 1 : -1));

    // Detect enforcement_literal => (var == value or var != value).
    //
    // Note that for domain with 2 values like [0, 1], we will detect both ==
    // 0 and != 1. Similarly, for a domain in [min, max], we should both
    // detect (== min) and (<= min), and both detect (== max) and (>= max).
    {
      const Domain inter = domain.IntersectionWith(domain_if_enforced);
      if (!inter.IsEmpty() && inter.IsFixed()) {
        var_to_equalities[var].push_back(
            {c, enforcement_literal, inter.FixedValue(), true});
        literal_to_var_value_ct_indices[enforcement_literal].push_back(
            {var, inter.FixedValue(), c});
      }
    }
    {
      const Domain inter =
          domain.IntersectionWith(domain_if_enforced.Complement());
      if (!inter.IsEmpty() && inter.Min() == inter.Max()) {
        var_to_equalities[var].push_back(
            {c, enforcement_literal, inter.FixedValue(), false});
      }
    }
  }

  // Detect Literal <=> X == value and rebuild full encodings.
  absl::flat_hash_map<int64_t, LitVarEncodingInfo> value_encodings;
  std::vector<std::pair<int64_t, int>> value_literal_pairs;

  for (int var = 0; var < var_to_equalities.size(); ++var) {
    std::vector<EqualityDetectionHelper>& encoding = var_to_equalities[var];
    if (encoding.empty()) continue;

    std::sort(encoding.begin(), encoding.end());
    const Domain domain = ReadDomainFromProto(model_proto_.variables(var));
    value_encodings.clear();

    for (int j = 0; j + 1 < encoding.size(); j++) {
      if ((encoding[j].value != encoding[j + 1].value) ||
          (encoding[j].literal != NegatedRef(encoding[j + 1].literal)) ||
          (encoding[j].is_equality != true) ||
          (encoding[j + 1].is_equality != false)) {
        continue;
      }

      // TODO(user): Deal with/Check double insertion.
      value_encodings[encoding[j].value] = {encoding[j].literal,
                                            encoding[j].constraint_index,
                                            encoding[j + 1].constraint_index};
    }
    if (value_encodings.size() == domain.Size()) {
      value_literal_pairs.clear();
      for (const auto& [value, value_encoding] : value_encodings) {
        ignored_constraints_[value_encoding.positive_ct_index] = true;
        ignored_constraints_[value_encoding.negative_ct_index] = true;
        num_ignored_constraints_ += 2;
        value_literal_pairs.push_back({value, value_encoding.lit});
      }
      // We sort to have a canonical representation with increasing values
      // and sorted literals.
      std::sort(value_literal_pairs.begin(), value_literal_pairs.end());

      ConstraintProto var_encoding;
      ConstraintProto exactly_one;
      int64_t offset = 0;
      int64_t min_value = 0;
      var_encoding.mutable_linear()->add_vars(var);
      var_encoding.mutable_linear()->add_coeffs(-1);
      bool first_iteration = true;
      for (const auto [value, lit] : value_literal_pairs) {
        // Fill exactly one.
        exactly_one.mutable_exactly_one()->add_literals(lit);

        // Fill linear encoding.
        if (first_iteration) {  // First iteration.
          // We shift the var = sum(lit * value) by the min value.
          offset = value;
          min_value = value;
          first_iteration = false;
        } else {
          const int64_t delta = value - min_value;
          DCHECK_GT(delta, 0);  // Full encoding: all variables are different.
          if (RefIsPositive(lit)) {
            var_encoding.mutable_linear()->add_vars(lit);
            var_encoding.mutable_linear()->add_coeffs(delta);
          } else {
            var_encoding.mutable_linear()->add_vars(PositiveRef(lit));
            var_encoding.mutable_linear()->add_coeffs(-delta);
            offset += delta;
          }
        }
      }
      var_encoding.mutable_linear()->add_domain(-offset);
      var_encoding.mutable_linear()->add_domain(-offset);
      if (encoding_cache.insert(var_encoding.linear()).second) {
        additional_constraints_.push_back(var_encoding);
        num_full_encodings_++;
      }

      // Add exactly_one constraint if new.
      std::sort(exactly_one.mutable_exactly_one()->mutable_literals()->begin(),
                exactly_one.mutable_exactly_one()->mutable_literals()->end());
      if (exactly_ones_cache.insert(exactly_one.exactly_one()).second) {
        additional_constraints_.push_back(exactly_one);
        num_exactly_ones_++;
      }
    }
  }

  VLOG(1) << "Linear model created:";
  VLOG(1) << "  #model constraints: " << model_proto_.constraints_size();
  VLOG(1) << "  #full encodings detected: " << num_full_encodings_;
  VLOG(1) << "  #exactly_ones added: " << num_exactly_ones_;
  VLOG(1) << "  #constraints ignored: " << num_ignored_constraints_;
}

}  // namespace sat
}  // namespace operations_research
