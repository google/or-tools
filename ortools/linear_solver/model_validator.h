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

#ifndef OR_TOOLS_LINEAR_SOLVER_MODEL_VALIDATOR_H_
#define OR_TOOLS_LINEAR_SOLVER_MODEL_VALIDATOR_H_

#include <optional>
#include <string>

#include "absl/types/optional.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/lazy_mutable_copy.h"

namespace operations_research {
/**
 * Returns an empty string iff the model is valid and not trivially infeasible.
 * Otherwise, returns a description of the first error or trivial infeasibility
 * variable or constraint bound encountered.
 *
 * abs_value_threshold is the (exclusive) limit for the abs value of constraint
 * coefficients, objective coefficients, etc. If unspecified, or 0, it defaults
 * to FLAGS_model_validator_infinity.
 *
 * NOTE(user): the code of this method (and the client code too!) is
 * considerably simplified by this string-based, simple API. If clients
 * require it, we could add a formal error status enum.
 */
std::string FindErrorInMPModelProto(
    const MPModelProto& model, double abs_value_threshold = 0.0,
    // If false, variable and constraint bounds like [lb=1.2, ub=0.7] yield an
    // error, and also integer variable bounds like [0.7, 0.8].
    bool accept_trivially_infeasible_bounds = false);

/**
 * Like FindErrorInMPModelProto, but for a MPModelDeltaProto applied to a given
 * baseline model (assumed valid, eg. FindErrorInMPModelProto(model)="").
 * Works in O(|model_delta|) + O(num_vars in model), but the latter term has a
 * very small constant factor.
 */
std::string FindErrorInMPModelDeltaProto(const MPModelDeltaProto& delta,
                                         const MPModelProto& model);

/**
 * If the model is valid and non-empty, returns it (possibly after extracting
 * the model_delta). If invalid or empty, updates `response` and returns null.
 */
std::optional<LazyMutableCopy<MPModelProto>>
ExtractValidMPModelOrPopulateResponseStatus(const MPModelRequest& request,
                                            MPSolutionResponse* response);

/**
 * Like ExtractValidMPModelOrPopulateResponseStatus(), but works in-place:
 * if the MPModel needed extraction, it will be populated in the request, and
 * it returns the success boolean.
 */
bool ExtractValidMPModelInPlaceOrPopulateResponseStatus(
    MPModelRequest* request, MPSolutionResponse* response);

/**
 * Returns an empty string if the solution hint given in the model is a feasible
 * solution. Otherwise, returns a description of the first reason for
 * infeasibility.
 *
 * This function can be useful for debugging/checking that the given solution
 * hint is feasible when it is expected to be the case. The feasibility is
 * checked up to the given tolerance using the
 * ::operations_research::IsLowerWithinTolerance() function.
 */
std::string FindFeasibilityErrorInSolutionHint(const MPModelProto& model,
                                               double tolerance);

// Partially merges a MPConstraintProto onto another, skipping only the
// repeated fields "var_index" and "coefficients". This is used within
// FindErrorInMPModelDeltaProto.
// See the unit test MergeMPConstraintProtoExceptTermsTest that explains why we
// need this.
void MergeMPConstraintProtoExceptTerms(const MPConstraintProto& from,
                                       MPConstraintProto* to);

// Applies the given model_delta to "model". Assumes that
// FindErrorInMPModelDeltaProto() found no error. For full error checking,
// please use ExtractValidMPModelOrPopulateResponseStatus() instead.
void ApplyVerifiedMPModelDelta(const MPModelDeltaProto& delta,
                               MPModelProto* model);

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_MODEL_VALIDATOR_H_
