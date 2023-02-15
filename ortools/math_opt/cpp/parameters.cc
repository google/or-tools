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

#include "ortools/math_opt/cpp/parameters.h"

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/status_macros.h"

namespace operations_research {
namespace math_opt {
namespace {

template <typename EnumType>
bool ParseEnumFlag(const absl::string_view text, EnumType* const value,
                   std::string* const error) {
  const std::optional<EnumType> enum_value = EnumFromString<EnumType>(text);
  if (!enum_value.has_value()) {
    *error = "unknown value for enumeration";
    return false;
  }

  *value = *enum_value;
  return true;
}

template <typename EnumType>
std::string UnparseEnumFlag(const EnumType value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

}  // namespace

std::optional<absl::string_view> Enum<SolverType>::ToOptString(
    SolverType value) {
  switch (value) {
    case SolverType::kGscip:
      return "gscip";
    case SolverType::kGurobi:
      return "gurobi";
    case SolverType::kGlop:
      return "glop";
    case SolverType::kCpSat:
      return "cp_sat";
    case SolverType::kGlpk:
      return "glpk";
  }
  return std::nullopt;
}

absl::Span<const SolverType> Enum<SolverType>::AllValues() {
  static constexpr SolverType kSolverTypeValues[] = {
      SolverType::kGscip, SolverType::kGurobi, SolverType::kGlop,
      SolverType::kCpSat, SolverType::kGlpk,
  };
  return absl::MakeConstSpan(kSolverTypeValues);
}

bool AbslParseFlag(const absl::string_view text, SolverType* const value,
                   std::string* const error) {
  return ParseEnumFlag(text, value, error);
}

std::string AbslUnparseFlag(const SolverType value) {
  return UnparseEnumFlag(value);
}

std::optional<absl::string_view> Enum<LPAlgorithm>::ToOptString(
    LPAlgorithm value) {
  switch (value) {
    case LPAlgorithm::kPrimalSimplex:
      return "primal_simplex";
    case LPAlgorithm::kDualSimplex:
      return "dual_simplex";
    case LPAlgorithm::kBarrier:
      return "barrier";
  }
  return std::nullopt;
}

absl::Span<const LPAlgorithm> Enum<LPAlgorithm>::AllValues() {
  static constexpr LPAlgorithm kLPAlgorithmValues[] = {
      LPAlgorithm::kPrimalSimplex,
      LPAlgorithm::kDualSimplex,
      LPAlgorithm::kBarrier,
  };
  return absl::MakeConstSpan(kLPAlgorithmValues);
}

bool AbslParseFlag(absl::string_view text, LPAlgorithm* const value,
                   std::string* const error) {
  return ParseEnumFlag(text, value, error);
}

std::string AbslUnparseFlag(const LPAlgorithm value) {
  return UnparseEnumFlag(value);
}

std::optional<absl::string_view> Enum<Emphasis>::ToOptString(Emphasis value) {
  switch (value) {
    case Emphasis::kOff:
      return "off";
    case Emphasis::kLow:
      return "low";
    case Emphasis::kMedium:
      return "medium";
    case Emphasis::kHigh:
      return "high";
    case Emphasis::kVeryHigh:
      return "very_high";
  }
  return std::nullopt;
}

absl::Span<const Emphasis> Enum<Emphasis>::AllValues() {
  static constexpr Emphasis kEmphasisValues[] = {
      Emphasis::kOff,  Emphasis::kLow,      Emphasis::kMedium,
      Emphasis::kHigh, Emphasis::kVeryHigh,
  };
  return absl::MakeConstSpan(kEmphasisValues);
}

bool AbslParseFlag(absl::string_view text, Emphasis* const value,
                   std::string* const error) {
  return ParseEnumFlag(text, value, error);
}

std::string AbslUnparseFlag(const Emphasis value) {
  return UnparseEnumFlag(value);
}

GurobiParametersProto GurobiParameters::Proto() const {
  GurobiParametersProto result;
  for (const auto& [key, val] : param_values) {
    GurobiParametersProto::Parameter& p = *result.add_parameters();
    p.set_name(key);
    p.set_value(val);
  }
  return result;
}

GurobiParameters GurobiParameters::FromProto(
    const GurobiParametersProto& proto) {
  GurobiParameters result;
  for (const GurobiParametersProto::Parameter& p : proto.parameters()) {
    result.param_values[p.name()] = p.value();
  }
  return result;
}

SolveParametersProto SolveParameters::Proto() const {
  SolveParametersProto result;
  result.set_enable_output(enable_output);
  if (time_limit < absl::InfiniteDuration()) {
    CHECK_OK(util_time::EncodeGoogleApiProto(time_limit,
                                             result.mutable_time_limit()));
  }
  if (iteration_limit.has_value()) {
    result.set_iteration_limit(*iteration_limit);
  }
  if (node_limit.has_value()) {
    result.set_node_limit(*node_limit);
  }
  if (cutoff_limit.has_value()) {
    result.set_cutoff_limit(*cutoff_limit);
  }
  if (objective_limit.has_value()) {
    result.set_objective_limit(*objective_limit);
  }
  if (best_bound_limit.has_value()) {
    result.set_best_bound_limit(*best_bound_limit);
  }
  if (solution_limit.has_value()) {
    result.set_solution_limit(*solution_limit);
  }
  if (threads.has_value()) {
    result.set_threads(*threads);
  }
  if (random_seed.has_value()) {
    result.set_random_seed(*random_seed);
  }
  if (relative_gap_tolerance.has_value()) {
    result.set_relative_gap_tolerance(*relative_gap_tolerance);
  }
  if (absolute_gap_tolerance.has_value()) {
    result.set_absolute_gap_tolerance(*absolute_gap_tolerance);
  }
  result.set_lp_algorithm(EnumToProto(lp_algorithm));
  result.set_presolve(EnumToProto(presolve));
  result.set_cuts(EnumToProto(cuts));
  result.set_heuristics(EnumToProto(heuristics));
  result.set_scaling(EnumToProto(scaling));
  *result.mutable_gscip() = gscip;
  *result.mutable_gurobi() = gurobi.Proto();
  *result.mutable_glop() = glop;
  *result.mutable_cp_sat() = cp_sat;
  return result;
}

absl::StatusOr<SolveParameters> SolveParameters::FromProto(
    const SolveParametersProto& proto) {
  SolveParameters result;
  result.enable_output = proto.enable_output();
  if (proto.has_time_limit()) {
    OR_ASSIGN_OR_RETURN3(result.time_limit,
                         util_time::DecodeGoogleApiProto(proto.time_limit()),
                         _ << "invalid time_limit");
  } else {
    result.time_limit = absl::InfiniteDuration();
  }
  if (proto.has_iteration_limit()) {
    result.iteration_limit = proto.iteration_limit();
  }
  if (proto.has_node_limit()) {
    result.node_limit = proto.node_limit();
  }
  if (proto.has_cutoff_limit()) {
    result.cutoff_limit = proto.cutoff_limit();
  }
  if (proto.has_objective_limit()) {
    result.objective_limit = proto.objective_limit();
  }
  if (proto.has_best_bound_limit()) {
    result.best_bound_limit = proto.best_bound_limit();
  }
  if (proto.has_solution_limit()) {
    result.solution_limit = proto.solution_limit();
  }
  if (proto.has_threads()) {
    result.threads = proto.threads();
  }
  if (proto.has_random_seed()) {
    result.random_seed = proto.random_seed();
  }
  if (proto.has_absolute_gap_tolerance()) {
    result.absolute_gap_tolerance = proto.absolute_gap_tolerance();
  }
  if (proto.has_relative_gap_tolerance()) {
    result.relative_gap_tolerance = proto.relative_gap_tolerance();
  }
  result.lp_algorithm = EnumFromProto(proto.lp_algorithm());
  result.presolve = EnumFromProto(proto.presolve());
  result.cuts = EnumFromProto(proto.cuts());
  result.heuristics = EnumFromProto(proto.heuristics());
  result.scaling = EnumFromProto(proto.scaling());
  result.gscip = proto.gscip();
  result.gurobi = GurobiParameters::FromProto(proto.gurobi());
  result.glop = proto.glop();
  result.cp_sat = proto.cp_sat();
  return result;
}

bool AbslParseFlag(absl::string_view text, SolveParameters* solve_parameters,
                   std::string* error) {
  SolveParametersProto proto;
  if (!ProtobufParseTextProtoForFlag(text, &proto, error)) {
    return false;
  }
  absl::StatusOr<SolveParameters> params = SolveParameters::FromProto(proto);
  if (!params.ok()) {
    *error = absl::StrCat(
        "SolveParametersProto was invalid and could not convert to "
        "SolveParameters: ",
        params.status().ToString());
    return false;
  }
  *solve_parameters = *std::move(params);
  return true;
}

std::string AbslUnparseFlag(SolveParameters solve_parameters) {
  return ProtobufTextFormatPrintToString(solve_parameters.Proto());
}

}  // namespace math_opt
}  // namespace operations_research
