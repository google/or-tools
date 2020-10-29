// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_CP_MODEL_UTILS_H_
#define OR_TOOLS_SAT_CP_MODEL_UTILS_H_

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// Small utility functions to deal with negative variable/literal references.
inline int NegatedRef(int ref) { return -ref - 1; }
inline int PositiveRef(int ref) { return std::max(ref, NegatedRef(ref)); }
inline bool RefIsPositive(int ref) { return ref >= 0; }

// Small utility functions to deal with half-reified constraints.
inline bool HasEnforcementLiteral(const ConstraintProto& ct) {
  return !ct.enforcement_literal().empty();
}
inline int EnforcementLiteral(const ConstraintProto& ct) {
  return ct.enforcement_literal(0);
}

// Fills the target as negated ref.
void SetToNegatedLinearExpression(const LinearExpressionProto& input_expr,
                                  LinearExpressionProto* output_negated_expr);

// Collects all the references used by a constraint. This function is used in a
// few places to have a "generic" code dealing with constraints. Note that the
// enforcement_literal is NOT counted here and that the vectors can have
// duplicates.
struct IndexReferences {
  std::vector<int> variables;
  std::vector<int> literals;
};
IndexReferences GetReferencesUsedByConstraint(const ConstraintProto& ct);

// Applies the given function to all variables/literals/intervals indices of the
// constraint. This function is used in a few places to have a "generic" code
// dealing with constraints.
void ApplyToAllVariableIndices(const std::function<void(int*)>& function,
                               ConstraintProto* ct);
void ApplyToAllLiteralIndices(const std::function<void(int*)>& function,
                              ConstraintProto* ct);
void ApplyToAllIntervalIndices(const std::function<void(int*)>& function,
                               ConstraintProto* ct);

// Returns the name of the ConstraintProto::ConstraintCase oneof enum.
// Note(user): There is no such function in the proto API as of 16/01/2017.
std::string ConstraintCaseName(ConstraintProto::ConstraintCase constraint_case);

// Returns the sorted list of variables used by a constraint.
// Note that this include variable used as a literal.
std::vector<int> UsedVariables(const ConstraintProto& ct);

// Returns the sorted list of interval used by a constraint.
std::vector<int> UsedIntervals(const ConstraintProto& ct);

// Returns true if a proto.domain() contain the given value.
// The domain is expected to be encoded as a sorted disjoint interval list.
template <typename ProtoWithDomain>
bool DomainInProtoContains(const ProtoWithDomain& proto, int64 value) {
  for (int i = 0; i < proto.domain_size(); i += 2) {
    if (value >= proto.domain(i) && value <= proto.domain(i + 1)) return true;
  }
  return false;
}

// Serializes a Domain into the domain field of a proto.
template <typename ProtoWithDomain>
void FillDomainInProto(const Domain& domain, ProtoWithDomain* proto) {
  proto->clear_domain();
  proto->mutable_domain()->Reserve(domain.NumIntervals());
  for (const ClosedInterval& interval : domain) {
    proto->add_domain(interval.start);
    proto->add_domain(interval.end);
  }
}

// Reads a Domain from the domain field of a proto.
template <typename ProtoWithDomain>
Domain ReadDomainFromProto(const ProtoWithDomain& proto) {
#if defined(__PORTABLE_PLATFORM__)
  return Domain::FromFlatIntervals(
      {proto.domain().begin(), proto.domain().end()});
#else
  return Domain::FromFlatSpanOfIntervals(proto.domain());
#endif
}

// Returns the list of values in a given domain.
// This will fail if the domain contains more than one millions values.
//
// TODO(user): work directly on the Domain class instead.
template <typename ProtoWithDomain>
std::vector<int64> AllValuesInDomain(const ProtoWithDomain& proto) {
  std::vector<int64> result;
  for (int i = 0; i < proto.domain_size(); i += 2) {
    for (int64 v = proto.domain(i); v <= proto.domain(i + 1); ++v) {
      CHECK_LE(result.size(), 1e6);
      result.push_back(v);
    }
  }
  return result;
}

// Scales back a objective value to a double value from the original model.
inline double ScaleObjectiveValue(const CpObjectiveProto& proto, int64 value) {
  double result = static_cast<double>(value);
  if (value == kint64min) result = -std::numeric_limits<double>::infinity();
  if (value == kint64max) result = std::numeric_limits<double>::infinity();
  result += proto.offset();
  if (proto.scaling_factor() == 0) return result;
  return proto.scaling_factor() * result;
}

// Removes the objective scaling and offset from the given value.
inline double UnscaleObjectiveValue(const CpObjectiveProto& proto,
                                    double value) {
  double result = value;
  if (proto.scaling_factor() != 0) {
    result /= proto.scaling_factor();
  }
  return result - proto.offset();
}

// Computes the "inner" objective of a response that contains a solution.
// This is the objective without offset and scaling. Call ScaleObjectiveValue()
// to get the user facing objective.
int64 ComputeInnerObjective(const CpObjectiveProto& objective,
                            const CpSolverResponse& response);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_UTILS_H_
