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

#include <algorithm>
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
#include "ortools/base/types.h"
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
constexpr int kNoVariableMapping = kint32min;

// A simpler version of PresolveContext with just a few helper to query and
// manipulate variable domains during the copy and handle remapping.
//
// Only visible for testing.
class ModelCopyHelper {
 public:
  ModelCopyHelper() = default;

  // IMPORTANT: Must be called before any other functions in this class.
  //
  // Takes list of num_input_vars domains of the "input" cp_model proto before
  // the mapping. The mapping can be empty or must be of the same same and will
  // remap [0, num_input_vars) into a potential smaller [0, num_output_vars)
  // dense space.
  //
  // More than one variable can be mapped to the same index, in which case these
  // are assumed to be equivalent. And input variables with kNoVariableMapping
  // must be fixed, that is InputIsFixed() should return true.
  bool InitializeDomains(std::vector<Domain> domains,
                         absl::Span<const int> mapping);

  // All Input*() functions accept variable indices in the input space. Note
  // however that the "fixed" status might reflect changes we did in the mapped
  // space as we perform the copy.
  bool InputIsFixed(int ref) const;
  bool InputFixedLiteralIsTrue(int ref) const;
  int64_t InputFixedValue(int var) const;
  std::optional<int64_t> InputFixedValueOrNullopt(
      const LinearExpressionProto& expr) const;

  struct FixedLinearArgument {
    int64_t target;
    std::vector<int64_t> exprs;
  };
  std::optional<FixedLinearArgument> InputFixedLinearArgumentOrNullopt(
      const LinearArgumentProto& linear_argument) const;

  // All the *Mapped*() functions work in the mapped space, after the mapping
  // has been applied. All the mutable function are in this category.
  ABSL_MUST_USE_RESULT bool IntersectMappedDomainWith(int var,
                                                      const Domain& domain);

  ABSL_MUST_USE_RESULT bool SetMappedLiteralToTrue(int ref) {
    const int value = RefIsPositive(ref) ? 1 : 0;
    return IntersectMappedDomainWith(PositiveRef(ref), Domain(value));
  }

  ABSL_MUST_USE_RESULT bool SetMappedLiteralToFalse(int ref) {
    const int value = RefIsPositive(ref) ? 0 : 1;
    return IntersectMappedDomainWith(PositiveRef(ref), Domain(value));
  }

  // The current variables domains in the mapped space.
  absl::Span<const Domain> MappedDomains() const { return mapped_domains_; }
  const Domain& MappedDomain(int var) const { return mapped_domains_[var]; }
  int64_t MappedMinOf(const LinearExpressionProto& expr) const;

  // Create a new mapped variable. This is only allowed if there is no mapping.
  int NewIntVar(const Domain& domain) {
    UpdateRuleStats("new variable during copy");
    CHECK(mapping_.empty());
    const int index = mapped_domains_.size();
    mapped_domains_.push_back(domain);
    solution_crush_.Resize(mapped_domains_.size());
    return index;
  }

  // Used to track what happened during the copy.
  // DisplaySummary() will list these statistics.
  void UpdateRuleStats(std::string_view name, int num_times = 1) {
    stats_by_rule_name_[name] += num_times;
  }
  void DisplaySummary(SolverLogger* logger) {
    absl::btree_map<std::string, int> sorted_rules(stats_by_rule_name_.begin(),
                                                   stats_by_rule_name_.end());
    for (const auto& entry : sorted_rules) {
      if (entry.second == 1) {
        SOLVER_LOG(logger, "  - rule '", entry.first, "' was applied 1 time.");
      } else {
        SOLVER_LOG(logger, "  - rule '", entry.first, "' was applied ",
                   FormatCounter(entry.second), " times.");
      }
    }
  }

  // This is used to udate the solution hint as we create new variables.
  SolutionCrush* solution_crush() { return &solution_crush_; }

 private:
  // We track which of the variable in the input proto was or is now fixed.
  // And for such fixed variable, its fixed value.
  //
  // This is mutable for speed as we can update that when a variable becomes
  // fixed lazily.
  mutable std::vector<bool> input_variable_is_fixed_;
  mutable std::vector<int64_t> input_variable_fixed_values_;

  // Mapping from the input proto indexing to the new one. Fixed variable do not
  // need to be mapped and can have a kNoVariableMapping entry. Note that
  // Boolean might be mapped to negative reference.
  std::vector<int> mapping_;

  // The domain in the output variable index space.
  std::vector<Domain> mapped_domains_;

  // This is used temporarily to transfrom the hint during copy.
  SolutionCrush solution_crush_;

  // Summary of the performed operations.
  absl::flat_hash_map<std::string, int> stats_by_rule_name_;
};

// This helper class performs copy with simplification from a CpModelProto and a
// partial assignment to another CpModelProto. The purpose is to minimize the
// size of the copied model, as well as to reduce the pressure on the memory
// sub-system.
//
// When first_copy is true, this is also responsible for canonicalizing the
// user-given model so that we don't have to handle all corner cases after this.
class ModelCopy {
 public:
  // If `variable_mapping` is not empty, it is applied to all variable
  // references in all the copied constraints.
  //
  // Fixed variables can be removed by setting their mapped value to
  // `kNoVariableMapping`. If some Boolean variables are fixed, at least one of
  // them must not be removed.
  //
  // Several variables can be mapped to the same variable. Moreover, for Boolean
  // only, we can map a Boolean to the negation of another by using a negated
  // reference.
  explicit ModelCopy(CpModelProto* out_proto, Model* model,
                     absl::Span<const int> variable_mapping = {});

  // Setup new variables from the one in the given model.
  // The imported variables must be the ones before variable mapping, if any.
  // This must be done first before any other call here. Note that the actual
  // IntegerVariableProto will only be written to the out_model in FinishCopy().
  //
  // Returns false iff the model was infeasible. This can happen if we map
  // two incompatible variables to the same one, or if some domain were empty.
  ABSL_MUST_USE_RESULT bool ImportVariables(const CpModelProto& in_model);

  // Same as ImportVariables() but from a vector of Domain instead.
  ABSL_MUST_USE_RESULT bool CreateVariablesFromDomains(
      absl::Span<const Domain> domains);

  // This must be done before we import the constraints. The hint will be
  // adapted if we ever create new variable as we canonicalize constraints.
  // Note that the hint will only be written in FinishCopy().
  void ImportSolutionHint(const CpModelProto& in_model);

  // Copies all constraints from in_model to working model of the context.
  //
  // During the process, it will read variable domains from the context, and
  // simplify constraints to minimize the size of the copied model.
  // Thus it is important that the context->working_model already have the
  // variables part copied. The hint must also be imported first so that it can
  // be updated during the simplification.
  //
  // It returns false iff the model is proven infeasible.
  //
  // It does not clear the constraints part of the working model of the context.
  //
  // Note(user): If first_copy is true, we will reorder the scheduling
  // constraint so that they only use reference to previously defined intervals.
  // This allow to be more efficient later in a few preprocessing steps.
  ABSL_MUST_USE_RESULT bool ImportAndSimplifyConstraints(
      const CpModelProto& in_model, bool first_copy = false,
      std::function<bool(int)> active_constraints = nullptr);

  // Imports and write the objective.
  ABSL_MUST_USE_RESULT bool ImportObjective(const CpModelProto& in_model);

  // Copies the non constraint, non variables part of the model. `copy_symmetry`
  // is only supported if there is no variable mapping.
  ABSL_MUST_USE_RESULT bool ImportEverythingExceptVariablesConstraintsAndHint(
      const CpModelProto& in_model, bool copy_symmetry = true);

  // This must be called to finish the copy. Note that variables proto will only
  // be filled at this stage in the out_model.
  //
  // Returns false iff the model was infeasible.
  ABSL_MUST_USE_RESULT bool FinishCopy(const CpModelProto& in_model);

  // Advanced usage. When a model was copied, interval_mapping[i] will
  // contain for a copied interval with original index i, its new index.
  absl::Span<const int64_t> InternalIntervalMapping() const {
    return interval_mapping_;
  }

 private:
  // Write a summary of what happen to the logger_.
  void DisplaySummary();

  // Overwrites the out_model to be unsat. Returns false.
  // The arguments are used to log which constraint caused unsat.
  bool CreateUnsatModel(int c, const ConstraintProto& ct);

  // Returns false if the constraint is never enforced and can be skipped.
  bool PrepareEnforcementCopy(const ConstraintProto& ct);
  bool PrepareEnforcementCopyWithDup(const ConstraintProto& ct);
  void FinishEnforcementCopy(ConstraintProto* ct);

  // All these functions return false if the constraint is found infeasible.

  // Copy a constraint that is always false, returning false if the enforcement
  // literals are empty or creating a constraint forcing at least one of them to
  // be false.
  bool CopyFalseConstraint();

  // Copy a constraint that is either always or never satisfied.
  bool CopyTrivialConstraint(bool is_always_satisfied) {
    if (is_always_satisfied) return true;
    return CopyFalseConstraint();
  }

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
      std::vector<std::pair<int, T>>& terms, T& offset);

  // This fills the non_fixed_terms_ field.
  template <typename T>
  int64_t FillNonFixedTermsAndReturnOffset(const T& proto_with_vars_and_coeffs,
                                           int64_t offset = 0);

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

  int MapLiteralEvenIfFixed(int lit) {
    if (variable_mapping_.empty()) return lit;
    if (helper_.InputIsFixed(lit)) {
      const int true_mapped_lit = GetTrueMappedLiteral();
      return helper_.InputFixedLiteralIsTrue(lit) ? true_mapped_lit
                                                  : NegatedRef(true_mapped_lit);
    }
    return MapLiteral(lit);
  }

  // `lit` must not have a fixed value, otherwise it might have no mapping.
  // Use MapLiteralEvenIfFixed() if unsure.
  int MapLiteral(int lit) const {
    DCHECK(!helper_.InputIsFixed(lit));
    if (variable_mapping_.empty()) return lit;
    const int mapped_ref = variable_mapping_[PositiveRef(lit)];
    DCHECK_NE(mapped_ref, kNoVariableMapping);
    return RefIsPositive(lit) ? mapped_ref : NegatedRef(mapped_ref);
  }

  // Normalizes `ref` to a positive reference, replaces fixed terms with an
  // updated `offset` and a 0 `coeff`, and remaps the variable if a variable
  // mapping is present.
  template <typename T>
  void MapTerm(int& ref, T& coeff, T& offset) const;

  // Helper to convert old proto format (single var) int new
  // LinearExpressionProto format.
  void ConvertSingleVarFormatToExpr(int var, LinearExpressionProto* expr);

  int GetTrueMappedLiteral();

  ModelCopyHelper helper_;
  const SatParameters& params_;
  SolverLogger* logger_;
  CpModelProto* working_model_;

  absl::Span<const int> variable_mapping_;

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
};

// Copy in_proto to out_proto.
// It performs on the fly simplification, and returns false if the
// model is proved infeasible. If reads the parameters 'ignore_names' and keeps
// or deletes variables and constraints names accordingly.
//
// This should only be called on the first copy of the user given model.
// Note that this reorder all constraints that use intervals last. We loose the
// user-defined order, but hopefully that should not matter too much.
bool CopyModel(const CpModelProto& in_proto, CpModelProto* out_proto,
               Model* model);

// Same as CopyModel() except that variable domains are read from domains and
// constraint might be filtered.
bool CopyModelAdvanced(const CpModelProto& in_proto,
                       absl::Span<const Domain> domains,
                       std::function<bool(int)> active_constraints,
                       std::vector<int>* interval_mapping,
                       CpModelProto* out_proto, Model* model);

// Accessing Domain can be expensive, so we maintain vector of bool for the
// hot spots.
class VariableDomains {
 public:
  void Reset(int num_vars);

  absl::Span<const Domain> AsSpan() const { return domains_; }
  Domain operator[](int var) const { return domains_[var]; }
  size_t size() const { return domains_.size(); }

  bool HasTwoValues(int var) const { return has_two_values_[var]; }
  bool IsFixed(int var) const { return is_fixed_[var]; }

  absl::Span<const int> FixedVariables() const { return fixed_vars_; }

  void Set(int var, Domain d);

 private:
  // Depends on domain updates.
  std::vector<Domain> domains_;
  std::vector<bool> has_two_values_;
  std::vector<bool> is_fixed_;
  std::vector<int> fixed_vars_;
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
  std::vector<int64_t> MapSolution(
      absl::Span<const int64_t> input_solution) const;
  std::vector<int64_t> ReverseMapSolution(
      absl::Span<const int64_t> solution) const;

  // Maps an inner objective value from the input model to the dense model.
  int64_t MapInnerObjectiveValue(int64_t input_inner_objective_value) const;

 private:
  // Return false if one of the domain becomes empty (UNSAT). This might happen
  // while we are cleaning up all workers at the end of a search.
  bool UpdateFromSharedBounds(int64_t& timestamp);

  // Computes and applies a dense mapping of the variables which removes fixed
  // and non-representative variables. Returns false if UNSAT.
  bool ComputeVariableMapping(absl::Span<const int> input_var_representatives);
  bool ApplyVariableMapping();
  void ResetVarDomains();

  const CpModelProto& input_model_proto_;
  SharedClausesManager* shared_clauses_;
  SharedBoundsManager* shared_bounds_;
  const int shared_bounds_id_;

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

  // The domains of the `input_model_proto_` variables.
  std::vector<Domain> input_var_domains_;
  // The domains of the `model_proto_` variables.
  VariableDomains var_domains_;

  // Temporary data for UpdateFromSharedBounds().
  std::vector<int> tmp_variables_;
  std::vector<int64_t> tmp_new_lower_bounds_;
  std::vector<int64_t> tmp_new_upper_bounds_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_MODEL_COPY_H_
