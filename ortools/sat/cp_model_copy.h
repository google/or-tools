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

#ifndef ORTOOLS_SAT_CP_MODEL_COPY_H_
#define ORTOOLS_SAT_CP_MODEL_COPY_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// A variable which is removed from the model during copy.
// This can only be done if the variable is fixed.
constexpr int kNoVariableMapping = std::numeric_limits<int>::min();

// This helper class perform copy with simplification from a model and a
// partial assignment to another model. The purpose is to minimize the size of
// the copied model, as well as to reduce the pressure on the memory sub-system.
//
// It is currently used by the LNS part, but could be used with any other scheme
// that generates partial assignments.
class ModelCopy {
 public:
  // If `variable_mapping` is not empty, it is applied to all variable
  // references in all the copied constraints. In this case, `context` must
  // describe the variables before mapping. A fixed variable can be removed by
  // setting its mapped value to `kNoVariableMapping`. If some Boolean variables
  // are fixed, at least one of them must not be removed. A variable appearing
  // in an InverseConstraintProto must not be removed. Several variables can be
  // mapped to the same variable. As of 2025-03-25, non Boolean variables
  // remapped to a negative variable reference are not supported.
  // `variable_mapping` and `reverse_mapping` must remain valid and unchanged
  // during the lifetime of the constructed instance.
  explicit ModelCopy(PresolveContext* context,
                     absl::Span<const int> variable_mapping = {},
                     absl::Span<const int> reverse_mapping = {});

  // Copy variables from the in_model to the working model. It reads the
  // 'ignore_names' parameters from the context, and keeps or deletes names
  // accordingly. This must be done before importing constraints. The imported
  // variables must be the ones before variable mapping, if any. They are not
  // remapped in the context's working model (this must be done at the end, with
  // RemapVariables()).
  void ImportVariablesAndMaybeIgnoreNames(const CpModelProto& in_model);

  // Setup new variables from a vector of domains. This must be done before
  // importing constraints. The imported variables must be the ones before
  // variable mapping, if any. They are not remapped in the context's working
  // model (this must be done at the end, with RemapVariables()).
  void CreateVariablesFromDomains(absl::Span<const Domain> domains);

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

  bool ImportObjective(const CpModelProto& in_model);
  void ImportSolutionHint(const CpModelProto& in_model);

  // Copies the non constraint, non variables part of the model. `copy_symmetry`
  // is only supported if there is no variable mapping.
  bool ImportEverythingExceptVariablesConstraintsAndHint(
      const CpModelProto& in_model, bool copy_symmetry = true);

  // Remaps all variables in the context's working model using the variable
  // mapping passed at construction time. This must be done after all
  // constraints have been imported.
  // Returns false iff the model is proven infeasible.
  bool RemapVariablesInProtoAndContext();

  // Advanced usage. When a model was copied, interval_mapping[i] will
  // contain for a copied interval with original index i, its new index.
  absl::Span<const int64_t> InternalIntervalMapping() const {
    return interval_mapping_;
  }

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
  bool CopyBoolOrWithDupSupport(const ConstraintProto& ct,
                                int one_based_cnf_index);
  bool FinishBoolOrCopy();

  bool CopyBoolAnd(const ConstraintProto& ct);
  bool CopyBoolAndWithDupSupport(const ConstraintProto& ct);

  bool CopyAtMostOne(const ConstraintProto& ct);
  bool CopyExactlyOne(const ConstraintProto& ct);
  bool CopyBoolXor(const ConstraintProto& ct);

  bool CopyElement(const ConstraintProto& ct);
  bool CopyIntProd(const ConstraintProto& ct, bool ignore_names);
  bool CopyIntDiv(const ConstraintProto& ct, bool ignore_names);
  bool CopyIntMod(const ConstraintProto& ct, bool ignore_names);
  bool CopyLinear(const ConstraintProto& ct, bool canonicalize);
  bool CopyLinearExpression(
      const LinearExpressionProto& expr, LinearExpressionProto* dst,
      const absl::flat_hash_set<int>* mapped_enforcement_literals = nullptr);
  template <typename T>
  void CanonicalizeLinearExpression(
      const absl::flat_hash_set<int>* enforcement_literals,
      std::vector<std::pair<int, T>>& terms, T& offset) const;
  bool CopyAutomaton(const ConstraintProto& ct);
  bool CopyTable(const ConstraintProto& ct);
  bool CopyAllDiff(const ConstraintProto& ct);
  bool CopyLinMax(const ConstraintProto& ct);
  bool CopyCircuit(const ConstraintProto& ct);
  bool CopyRoutes(const ConstraintProto& ct);
  bool CopyInverse(const ConstraintProto& ct);
  bool CopyReservoir(const ConstraintProto& ct);

  // If we "copy" an interval for a first time, we make sure to create the
  // linear constraint between the start, size and end. This allow to simplify
  // the input proto and client side code. If there are more than one
  // enforcement literals, we replace them with a new one, made equal to their
  // conjunction with two new constraints.
  bool CopyInterval(const ConstraintProto& ct, int c, bool ignore_names);
  bool AddLinearConstraintForInterval(const ConstraintProto& ct);
  int GetOrCreateVariableForConjunction(std::vector<int>* literals);

  // These function remove unperformed intervals. Note that they requires
  // interval to appear before (validated) as they test unperformed by testing
  // if interval_mapping_ is empty.
  void CopyAndMapNoOverlap(const ConstraintProto& ct);
  void CopyAndMapNoOverlap2D(const ConstraintProto& ct);
  bool CopyAndMapCumulative(const ConstraintProto& ct);

  bool CopyObjective(const CpObjectiveProto& objective);
  void CopyFloatingPointObjective(const FloatObjectiveProto& objective);
  void CopySolutionHint(const PartialVariableAssignment& hint);

  // Expands linear expressions with more than one variable in constraints which
  // internally only support affine expressions (such as all_diff, element,
  // interval, reservoir, table, etc). This creates new variables for each such
  // expression, and replaces the original expressions with the new variables in
  // the constraints.
  void ExpandNonAffineExpressions();
  // Replaces the expression sum a_i * x_i + c with gcd * y + c, where y is a
  // new variable defined with an additional constraint y = sum a_i / gcd * x_i.
  void MaybeExpandNonAffineExpression(LinearExpressionProto* expr);
  void MaybeExpandNonAffineExpressions(LinearArgumentProto* linear_argument);

  int MapLiteral(int lit) {
    if (variable_mapping_.empty()) return lit;
    if (context_->IsFixed(lit)) {
      const int true_mapped_lit = GetTrueMappedLiteral();
      return context_->LiteralIsTrue(lit) ? true_mapped_lit
                                          : NegatedRef(true_mapped_lit);
    }
    return MapRef(lit);
  }

  // `ref` must not have a fixed value, otherwise it might have no mapping.
  int MapRef(int ref) const {
    if (variable_mapping_.empty()) return ref;
    const int mapped_ref = variable_mapping_[PositiveRef(ref)];
    DCHECK_NE(mapped_ref, kNoVariableMapping);
    return RefIsPositive(ref) ? mapped_ref : NegatedRef(mapped_ref);
  }

  int ReverseMapRef(int mapped_ref) const {
    if (variable_mapping_.empty()) return mapped_ref;
    const int ref = reverse_mapping_[PositiveRef(mapped_ref)];
    return RefIsPositive(mapped_ref) ? ref : NegatedRef(ref);
  }

  // Normalizes `ref` to a positive reference, replaces fixed terms with an
  // updated `offset` and a 0 `coeff`, and remaps the variable if a variable
  // mapping is present.
  template <typename T>
  void MapTerm(int& ref, T& coeff, T& offset) const;

  // Returns the domain of `mapped_var`, computed from the domain of the
  // original variable mapped to `mapped_var` (as stored in the context).
  const Domain& MappedVarDomain(int mapped_var) const;

  int GetTrueMappedLiteral();

  PresolveContext* context_;
  absl::Span<const int> variable_mapping_;
  absl::Span<const int> reverse_mapping_;
  // If some original Boolean variables are fixed at least one of them must not
  // be removed by the variable mapping, from which we compute this always true
  // mapped literal.
  std::optional<int> true_mapped_literal_;
  LratProofHandler* lrat_proof_handler_;

  // Temp vectors.
  std::vector<std::pair<int, int64_t>> non_fixed_terms_;
  std::vector<int64_t> interval_mapping_;
  int starting_constraint_index_ = 0;

  // These contain mapped literals.
  std::vector<int> temp_enforcement_literals_;
  absl::flat_hash_set<int> temp_enforcement_literals_set_;
  std::vector<int> temp_literals_;
  absl::flat_hash_set<int> temp_literals_set_;

  ConstraintProto tmp_constraint_;

  // Temp vectors used for LRAT.
  std::vector<Literal> temp_clause_;
  std::vector<Literal> temp_simplified_clause_;
  std::vector<ClausePtr> temp_proof_;

  // Map used in GetOrCreateVariableForConjunction() to avoid creating duplicate
  // variables for identical sets of literals.
  absl::flat_hash_map<std::vector<int>, int> boolean_product_encoding_;
  // Map used in ExpandNonAffineExpressions() to avoid creating duplicate
  // variables for the identical non affine expressions.
  absl::flat_hash_map<std::vector<std::pair<int, int64_t>>, int>
      non_affine_expression_to_new_var_;

  const Domain domain0_ = Domain(0);
  const Domain domain1_ = Domain(1);
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
// domains are read from domains and constraint might be filtered.
bool ImportModelAndDomainsWithBasicPresolveIntoContext(
    const CpModelProto& in_model, absl::Span<const Domain> domains,
    std::function<bool(int)> active_constraints, PresolveContext* context,
    std::vector<int>* interval_mapping);

// Accessing Domain can be expensive, so we maintain vector of bool for the
// hot spots.
class VariableDomains {
 public:
  VariableDomains(absl::string_view name, SharedBoundsManager* shared_bounds)
      : shared_bounds_id_(
            shared_bounds == nullptr ? 0 : shared_bounds->RegisterNewId(name)),
        shared_bounds_(shared_bounds) {}

  void Reset(int num_vars);

  absl::Span<const Domain> AsSpan() const { return domains_; }
  Domain operator[](int var) const { return domains_[var]; }
  size_t size() const { return domains_.size(); }

  bool HasTwoValues(int var) const { return has_two_values_[var]; }
  bool IsFixed(int var) const { return is_fixed_[var]; }

  absl::Span<const int> FixedVariables() const { return fixed_vars_; }

  void Set(int var, Domain d);

  // Return false if one of the domain becomes empty (UNSAT). This might happen
  // while we are cleaning up all workers at the end of a search.
  bool UpdateFromSharedBounds(absl::Span<const int> variable_mapping,
                              int64_t& timestamp);

 private:
  const int shared_bounds_id_;
  SharedBoundsManager* shared_bounds_;

  // Depends on domain updates.
  std::vector<Domain> domains_;
  std::vector<bool> has_two_values_;
  std::vector<bool> is_fixed_;
  std::vector<int> fixed_vars_;

  // Temporary data for UpdateFromSharedBounds()
  std::vector<int> tmp_variables_;
  std::vector<int64_t> tmp_new_lower_bounds_;
  std::vector<int64_t> tmp_new_upper_bounds_;
};

// A CpModelProto copy where fixed and non-representative variables are removed,
// which can be dynamically updated when new bounds or equivalences are found.
class DenseModelCopy {
 public:
  DenseModelCopy(absl::string_view name, const CpModelProto& input_model_proto,
                 SharedBoundsManager* shared_bounds,
                 SharedClausesManager* shared_clauses);

  // The timestamp of the shared bounds and equivalences used to compute this
  // dense model copy.
  int64_t bounds_timestamp() const { return bounds_timestamp_; }
  int64_t equivalences_timestamp() const { return equivalences_timestamp_; }

  // The dense model proto and the domains of its variables.
  const CpModelProto& proto() const { return model_proto_; }
  const VariableDomains& var_domains() const { return var_domains_; }

  // Updates the model with the latest bounds and equivalences from the
  // SharedBoundsManager and SharedClausesManager. Returns false if the model is
  // proven infeasible. Sets `updated` to true if the model was updated.
  bool MaybeUpdate(bool& updated);

  // Maps a solution from the input model to the dense model, and vice versa.
  std::vector<int64_t> MapSolution(absl::Span<const int64_t> input_solution);
  std::vector<int64_t> ReverseMapSolution(absl::Span<const int64_t> solution);

 private:
  // Computes and applies a dense mapping of the variables which removes fixed
  // and non-representative variables. Returns false if UNSAT.
  bool ComputeVariableMapping(absl::Span<const int> input_var_representatives,
                              std::vector<int>& new_input_var_mapping);
  bool ApplyVariableMapping(absl::Span<const int> input_var_mapping);
  void ResetVarDomains();

  const CpModelProto& input_model_proto_;
  SharedClausesManager* shared_clauses_;

  // Timestamps of the data used to compute the fields below.
  int64_t bounds_timestamp_ = -1;
  int64_t equivalences_timestamp_ = -1;

  // A dense `input_model_proto_` copy, with fixed and non-representative
  // variables removed. Unless stated otherwise with an explicit mention to the
  // 'input' model, the fields, methods, and method parameters in this class
  // operate on the variables and constraints of this model.
  CpModelProto model_proto_;

  // A mapping from the `input_model_proto_` to the `model_proto_` variables,
  // which removes the fixed and non-representative variables. Removed variables
  // are "mapped" to `kNoVariableMapping`.
  std::vector<int> input_var_mapping_;
  // The reverse mapping from the dense to the input variables.
  std::vector<int> reverse_mapping_;

  // The domains of the `model_proto_` variables.
  VariableDomains var_domains_;

  // For each `input_model_proto_` variable, its fixed value according to the
  // SharedBoundsManager, or int64_t::min() if there is none.
  std::vector<int64_t> fixed_input_var_values_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_MODEL_COPY_H_
