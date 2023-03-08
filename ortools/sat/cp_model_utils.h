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

#ifndef OR_TOOLS_SAT_CP_MODEL_UTILS_H_
#define OR_TOOLS_SAT_CP_MODEL_UTILS_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "google/protobuf/text_format.h"
#include "ortools/base/helpers.h"
#endif  // !defined(__PORTABLE_PLATFORM__)
#include "absl/container/flat_hash_set.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "ortools/base/hash.h"
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
bool DomainInProtoContains(const ProtoWithDomain& proto, int64_t value) {
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
std::vector<int64_t> AllValuesInDomain(const ProtoWithDomain& proto) {
  std::vector<int64_t> result;
  for (int i = 0; i < proto.domain_size(); i += 2) {
    for (int64_t v = proto.domain(i); v <= proto.domain(i + 1); ++v) {
      CHECK_LE(result.size(), 1e6);
      result.push_back(v);
    }
  }
  return result;
}

// Scales back a objective value to a double value from the original model.
inline double ScaleObjectiveValue(const CpObjectiveProto& proto,
                                  int64_t value) {
  double result = static_cast<double>(value);
  if (value == std::numeric_limits<int64_t>::min())
    result = -std::numeric_limits<double>::infinity();
  if (value == std::numeric_limits<int64_t>::max())
    result = std::numeric_limits<double>::infinity();
  result += proto.offset();
  if (proto.scaling_factor() == 0) return result;
  return proto.scaling_factor() * result;
}

// Similar to ScaleObjectiveValue() but uses the integer version.
inline int64_t ScaleInnerObjectiveValue(const CpObjectiveProto& proto,
                                        int64_t value) {
  if (proto.integer_scaling_factor() == 0) {
    return value + proto.integer_before_offset();
  }
  return (value + proto.integer_before_offset()) *
             proto.integer_scaling_factor() +
         proto.integer_after_offset();
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
int64_t ComputeInnerObjective(const CpObjectiveProto& objective,
                              absl::Span<const int64_t> solution);

// Returns true if a linear expression can be reduced to a single ref.
bool ExpressionContainsSingleRef(const LinearExpressionProto& expr);

// Checks if the expression is affine or constant.
bool ExpressionIsAffine(const LinearExpressionProto& expr);

// Returns the reference the expression can be reduced to. It will DCHECK that
// ExpressionContainsSingleRef(expr) is true.
int GetSingleRefFromExpression(const LinearExpressionProto& expr);

// Adds a linear expression proto to a linear constraint in place.
//
// Important: The domain must already be set, otherwise the offset will be lost.
// We also do not do any duplicate detection, so the constraint might need
// presolving afterwards.
void AddLinearExpressionToLinearConstraint(const LinearExpressionProto& expr,
                                           int64_t coefficient,
                                           LinearConstraintProto* linear);

// Returns true iff a == b * b_scaling.
bool LinearExpressionProtosAreEqual(const LinearExpressionProto& a,
                                    const LinearExpressionProto& b,
                                    int64_t b_scaling = 1);

// Default seed for fingerprints.
constexpr uint64_t kDefaultFingerprintSeed = 0xa5b85c5e198ed849;

// T must be castable to uint64_t.
template <class T>
inline uint64_t FingerprintRepeatedField(
    const google::protobuf::RepeatedField<T>& sequence, uint64_t seed) {
  return fasthash64(reinterpret_cast<const char*>(sequence.data()),
                    sequence.size() * sizeof(T), seed);
}

// T must be castable to uint64_t.
template <class T>
inline uint64_t FingerprintSingleField(const T& field, uint64_t seed) {
  return fasthash64(reinterpret_cast<const char*>(&field), sizeof(T), seed);
}

// Returns a stable fingerprint of a linear expression.
uint64_t FingerprintExpression(const LinearExpressionProto& lin, uint64_t seed);

// Returns a stable fingerprint of a model.
uint64_t FingerprintModel(const CpModelProto& model,
                          uint64_t seed = kDefaultFingerprintSeed);

#if !defined(__PORTABLE_PLATFORM__)

// We register a few custom printers to display variables and linear
// expression on one line. This is especially nice for variables where it is
// easy to recover their indices from the line number now.
//
// ex:
//
// variables { domain: [0, 1] }
// variables { domain: [0, 1] }
// variables { domain: [0, 1] }
//
// constraints {
//   linear {
//     vars: [0, 1, 2]
//     coeffs: [2, 4, 5 ]
//     domain: [11, 11]
//   }
// }
void SetupTextFormatPrinter(google::protobuf::TextFormat::Printer* printer);
#endif  // !defined(__PORTABLE_PLATFORM__)

template <class M>
bool WriteModelProtoToFile(const M& proto, absl::string_view filename) {
#if defined(__PORTABLE_PLATFORM__)
  return false;
#else   // !defined(__PORTABLE_PLATFORM__)
  if (absl::EndsWith(filename, "txt")) {
    std::string proto_string;
    google::protobuf::TextFormat::Printer printer;
    SetupTextFormatPrinter(&printer);
    printer.PrintToString(proto, &proto_string);
    return file::SetContents(filename, proto_string, file::Defaults()).ok();
  } else {
    return file::SetBinaryProto(filename, proto, file::Defaults()).ok();
  }
#endif  // !defined(__PORTABLE_PLATFORM__)
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_UTILS_H_
