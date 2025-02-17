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

#ifndef OR_TOOLS_SAT_CP_MODEL_COPY_H_
#define OR_TOOLS_SAT_CP_MODEL_COPY_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// This helper class perform copy with simplification from a model and a
// partial assignment to another model. The purpose is to minimize the size of
// the copied model, as well as to reduce the pressure on the memory sub-system.
//
// It is currently used by the LNS part, but could be used with any other scheme
// that generates partial assignments.
class ModelCopy {
 public:
  explicit ModelCopy(PresolveContext* context);

  // Copies all constraints from in_model to working model of the context.
  //
  // During the process, it will read variable domains from the context, and
  // simplify constraints to minimize the size of the copied model.
  // Thus it is important that the context->working_model already have the
  // variables part copied.
  //
  // It returns false iff the model is proven infeasible.
  //
  // It does not clear the constraints part of the working model of the context.
  //
  // Note(user): If first_copy is true, we will reorder the scheduling
  // constraint so that they only use reference to previously defined intervals.
  // This allow to be more efficient later in a few preprocessing steps.
  bool ImportAndSimplifyConstraints(
      const CpModelProto& in_model, bool first_copy = false,
      std::function<bool(int)> active_constraints = nullptr);

  // Copy variables from the in_model to the working model.
  // It reads the 'ignore_names' parameters from the context, and keeps or
  // deletes names accordingly.
  void ImportVariablesAndMaybeIgnoreNames(const CpModelProto& in_model);

  // Setup new variables from a vector of domains.
  // Inactive variables will be fixed to their lower bound.
  void CreateVariablesFromDomains(absl::Span<const Domain> domains);

 private:
  // Overwrites the out_model to be unsat. Returns false.
  // The arguments are used to log which constraint caused unsat.
  bool CreateUnsatModel(int c, const ConstraintProto& ct);

  // Returns false if the constraint is never enforced and can be skipped.
  bool PrepareEnforcementCopy(const ConstraintProto& ct);
  bool PrepareEnforcementCopyWithDup(const ConstraintProto& ct);
  void FinishEnforcementCopy(ConstraintProto* ct);

  // All these functions return false if the constraint is found infeasible.
  bool CopyBoolOr(const ConstraintProto& ct);
  bool CopyBoolOrWithDupSupport(const ConstraintProto& ct);
  bool FinishBoolOrCopy();

  bool CopyBoolAnd(const ConstraintProto& ct);
  bool CopyBoolAndWithDupSupport(const ConstraintProto& ct);

  bool CopyAtMostOne(const ConstraintProto& ct);
  bool CopyExactlyOne(const ConstraintProto& ct);

  bool CopyElement(const ConstraintProto& ct);
  bool CopyIntProd(const ConstraintProto& ct, bool ignore_names);
  bool CopyIntDiv(const ConstraintProto& ct, bool ignore_names);
  bool CopyIntMod(const ConstraintProto& ct, bool ignore_names);
  bool CopyLinear(const ConstraintProto& ct, bool canonicalize);
  bool CopyLinearExpression(const LinearExpressionProto& expr,
                            LinearExpressionProto* dst,
                            absl::Span<const int> enforcement_literals = {});
  bool CopyAutomaton(const ConstraintProto& ct);
  bool CopyTable(const ConstraintProto& ct);
  bool CopyAllDiff(const ConstraintProto& ct);
  bool CopyLinMax(const ConstraintProto& ct);

  // If we "copy" an interval for a first time, we make sure to create the
  // linear constraint between the start, size and end. This allow to simplify
  // the input proto and client side code.
  bool CopyInterval(const ConstraintProto& ct, int c, bool ignore_names);
  bool AddLinearConstraintForInterval(const ConstraintProto& ct);

  // These function remove unperformed intervals. Note that they requires
  // interval to appear before (validated) as they test unperformed by testing
  // if interval_mapping_ is empty.
  void CopyAndMapNoOverlap(const ConstraintProto& ct);
  void CopyAndMapNoOverlap2D(const ConstraintProto& ct);
  bool CopyAndMapCumulative(const ConstraintProto& ct);

  PresolveContext* context_;

  // Temp vectors.
  std::vector<int> non_fixed_variables_;
  std::vector<int64_t> non_fixed_coefficients_;
  std::vector<int64_t> interval_mapping_;
  int starting_constraint_index_ = 0;

  std::vector<int> temp_enforcement_literals_;
  absl::flat_hash_set<int> temp_enforcement_literals_set_;

  std::vector<int> temp_literals_;
  absl::flat_hash_set<int> temp_literals_set_;

  ConstraintProto tmp_constraint_;
};

// Copy in_model to the model in the presolve context.
// It performs on the fly simplification, and returns false if the
// model is proved infeasible. If reads the parameters 'ignore_names' and keeps
// or deletes variables and constraints names accordingly.
//
// This should only be called on the first copy of the user given model.
// Note that this reorder all constraints that use intervals last. We loose the
// user-defined order, but hopefully that should not matter too much.
bool ImportModelWithBasicPresolveIntoContext(const CpModelProto& in_model,
                                             PresolveContext* context);

// Same as ImportModelWithBasicPresolveIntoContext() except that variable
// domains are read from domains.
bool ImportModelAndDomainsWithBasicPresolveIntoContext(
    const CpModelProto& in_model, absl::Span<const Domain> domains,
    std::function<bool(int)> active_constraints, PresolveContext* context);

// Copies the non constraint, non variables part of the model.
void CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(
    const CpModelProto& in_model, PresolveContext* context);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_COPY_H_
