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

#include "ortools/sat/cp_model_copy.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/macros/os_support.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/base/types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

bool ModelCopyHelper::InitializeDomains(std::vector<Domain> domains,
                                        absl::Span<const int> mapping) {
  mapping_.assign(mapping.begin(), mapping.end());

  input_variable_is_fixed_.assign(domains.size(), false);
  input_variable_fixed_values_.resize(domains.size());
  for (int var = 0; var < domains.size(); ++var) {
    if (domains[var].IsEmpty()) return false;
    if (domains[var].IsFixed()) {
      input_variable_is_fixed_[var] = true;
      input_variable_fixed_values_[var] = domains[var].FixedValue();
    }
  }

  if (mapping.empty()) {
    mapped_domains_ = std::move(domains);
    return true;
  }
  CHECK_EQ(mapping.size(), domains.size());

  // Compute the range [0, mapped_size) of the mapped variables
  int mapped_size = 0;
  for (const int image : mapping) {
    if (image == kNoVariableMapping) continue;
    mapped_size = std::max(mapped_size, PositiveRef(image) + 1);
  }

  // If many variables are mapped to the same one, the domain will be the
  // intersection of all initial domains. Note, that we start with empty
  // domains to mean unset. This works since after each operation if we are
  // empty, we return false right away (UNSAT).
  mapped_domains_.resize(mapped_size);
  const auto update_domain = [this](int image, const Domain& domain) {
    DCHECK(!domain.IsEmpty());
    Domain& mapped_domain = mapped_domains_[image];
    if (mapped_domain.IsEmpty()) {
      mapped_domain = domain;
      return true;
    } else {
      mapped_domain = mapped_domain.IntersectionWith(domain);
      return !mapped_domain.IsEmpty();
    }
  };

  for (int i = 0; i < mapping.size(); ++i) {
    if (mapping[i] == kNoVariableMapping) continue;
    const Domain& domain = domains[i];
    const int image = PositiveRef(mapping[i]);

    // Special negative mapping for boolean.
    if (!RefIsPositive(mapping[i])) {
      DCHECK_GE(domain.Min(), 0);
      DCHECK_LE(domain.Max(), 1);
      if (domain.IsFixed()) {
        const Domain fixed_value = Domain(1 - domain.FixedValue());
        if (!update_domain(image, fixed_value)) return false;
        continue;
      }
    }

    if (!update_domain(image, domain)) return false;
  }

  return true;
}

bool ModelCopyHelper::InputIsFixed(int ref) const {
  const int var = PositiveRef(ref);
  if (input_variable_is_fixed_[var]) return true;

  // If the mapped variable is fixed, mark the input as such.
  // This make sure we use the latest state as we copy.
  //
  // TODO(user): This reflect more the state during copy, but it might be
  // slower. So maybe we should leave that for presolve? On another hand, more
  // than speed, it is the saving in memory that is interesting during the
  // first copy.
  const int image = mapping_.empty() ? var : mapping_[var];
  DCHECK_NE(image, kNoVariableMapping);
  const Domain& mapped_domain = mapped_domains_[PositiveRef(image)];
  if (mapped_domain.IsFixed()) {
    input_variable_is_fixed_[var] = true;
    input_variable_fixed_values_[var] = RefIsPositive(image)
                                            ? mapped_domain.FixedValue()
                                            : 1 - mapped_domain.FixedValue();
    return true;
  }

  return false;
}

bool ModelCopyHelper::InputFixedLiteralIsTrue(int ref) const {
  DCHECK(InputIsFixed(ref));
  const int value = RefIsPositive(ref) ? 1 : 0;
  const int var = PositiveRef(ref);
  return input_variable_fixed_values_[var] == value;
}

int64_t ModelCopyHelper::InputFixedValue(int var) const {
  DCHECK(InputIsFixed(var));
  DCHECK(RefIsPositive(var));
  return input_variable_fixed_values_[var];
}

std::optional<int64_t> ModelCopyHelper::InputFixedValueOrNullopt(
    const LinearExpressionProto& expr) const {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    if (expr.coeffs(i) == 0) continue;
    const int var = expr.vars(i);
    if (!InputIsFixed(var)) return std::nullopt;
    result += expr.coeffs(i) * InputFixedValue(var);
  }
  return result;
}

int64_t ModelCopyHelper::MappedMinOf(const LinearExpressionProto& expr) const {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int64_t coeff = expr.coeffs(i);
    if (coeff > 0) {
      result += coeff * mapped_domains_[expr.vars(i)].Min();
    } else {
      result += coeff * mapped_domains_[expr.vars(i)].Max();
    }
  }
  return result;
}

bool ModelCopyHelper::IntersectMappedDomainWith(int var, const Domain& domain) {
  if (mapped_domains_[var].IsIncludedIn(domain)) return true;
  UpdateRuleStats("domain: changed");

  const Domain intersection = mapped_domains_[var].IntersectionWith(domain);
  mapped_domains_[var] = intersection;
  if (intersection.IsEmpty()) return false;
  if (mapping_.empty()) {
    solution_crush_.SetOrUpdateVarToDomain(var, intersection);
  }
  return true;
}

ModelCopy::ModelCopy(CpModelProto* out_proto, Model* model,
                     absl::Span<const int> variable_mapping)
    : params_(*model->GetOrCreate<SatParameters>()),
      logger_(model->GetOrCreate<SolverLogger>()),
      working_model_(out_proto),
      variable_mapping_(variable_mapping),
      lrat_proof_handler_(model->Mutable<LratProofHandler>()) {}

bool ModelCopy::ImportVariables(const CpModelProto& in_model) {
  std::vector<Domain> domains;
  domains.reserve(in_model.variables().size());
  for (const IntegerVariableProto& var_proto : in_model.variables()) {
    domains.push_back(ReadDomainFromProto(var_proto));
  }
  return helper_.InitializeDomains(std::move(domains), variable_mapping_);
}

bool ModelCopy::CreateVariablesFromDomains(absl::Span<const Domain> domains) {
  std::vector<Domain> copy(domains.begin(), domains.end());
  return helper_.InitializeDomains(std::move(copy), variable_mapping_);
}

// TODO(user): Merge with the phase 1 of the presolve code.
//
// TODO(user): It seems easy to forget to update this if any new constraint
// contains an interval or if we add a field to an existing constraint. Find a
// way to remind contributor to not forget this.
bool ModelCopy::ImportAndSimplifyConstraints(
    const CpModelProto& in_model, bool first_copy,
    std::function<bool(int)> active_constraints) {
  const bool ignore_names = params_.ignore_names();

  // If first_copy is true, we reorder the scheduling constraint to be sure they
  // refer to interval before them.
  std::vector<int> constraints_using_intervals;

  interval_mapping_.assign(in_model.constraints().size(), -1);
  boolean_product_encoding_.clear();

  if (first_copy && lrat_proof_handler_ != nullptr) {
    lrat_proof_handler_->proof_status()->SetMaxOneBasedCnfIndex(
        in_model.constraints_size());
  }
  if (first_copy && params_.cp_model_pure_sat_presolve()) {
    // In this case, just copy all the constraints as is (just with duplicate
    // literals removed). If LRAT is enabled we cannot simply drop a constraint,
    // otherwise the occurrence counts in the LRAT checker would be incorrect.
    // To handle this, the easiest is to rely on the SatSolver used later on in
    // the presolve. And to get the same behavior when LRAT is disabled, we
    // always use this direct copy when pure SAT presolve is enabled.
    const std::string error_msg =
        "cp_model_pure_sat_presolve can only be used with pure SAT problems.";
    if (working_model_->has_objective()) {
      LOG(FATAL) << error_msg;
    }
    for (int i = 0; i < in_model.variables_size(); ++i) {
      const auto& domain = in_model.variables(i).domain();
      if (domain.size() != 2 || domain[0] < 0 || domain[1] > 1) {
        // This is not a Boolean.
        LOG(FATAL) << error_msg;
      }
    }
    for (int c = 0; c < in_model.constraints_size(); ++c) {
      const ConstraintProto& ct = in_model.constraints(c);
      if (ct.constraint_case() != ConstraintProto::kBoolOr) {
        LOG(FATAL) << error_msg;
      }
      temp_literals_.clear();
      temp_literals_set_.clear();
      for (const int enforcement_lit : ct.enforcement_literal()) {
        const int lit = NegatedRef(enforcement_lit);
        const auto [it, inserted] = temp_literals_set_.insert(lit);
        if (inserted) temp_literals_.push_back(lit);
      }
      for (const int lit : ct.bool_or().literals()) {
        const auto [it, inserted] = temp_literals_set_.insert(lit);
        if (inserted) temp_literals_.push_back(lit);
      }
      working_model_->add_constraints()
          ->mutable_bool_or()
          ->mutable_literals()
          ->Add(temp_literals_.begin(), temp_literals_.end());
    }
    return true;
  }

  starting_constraint_index_ = working_model_->constraints_size();
  for (int c = 0; c < in_model.constraints_size(); ++c) {
    if (active_constraints != nullptr && !active_constraints(c)) {
      continue;
    }
    const ConstraintProto& ct = in_model.constraints(c);
    if (first_copy) {
      if (!PrepareEnforcementCopyWithDup(ct)) continue;
    } else {
      if (!PrepareEnforcementCopy(ct)) continue;
    }

    // TODO(user): if ignore_names is false, we should make sure the
    // name are properly copied by all these functions. Or we should never copy
    // name and have a separate if (!ignore_name) copy the name...
    switch (ct.constraint_case()) {
      case ConstraintProto::CONSTRAINT_NOT_SET:
        break;
      case ConstraintProto::kBoolOr:
        if (first_copy) {
          if (!CopyBoolOrWithDupSupport(ct, c + 1)) {
            return CreateUnsatModel(c, ct);
          }
        } else {
          if (!CopyBoolOr(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kBoolAnd:
        if (temp_enforcement_literals_.empty()) {
          helper_.UpdateRuleStats("bool_and: non-reified");
          for (const int lit : ct.bool_and().literals()) {
            if (helper_.InputIsFixed(lit)) {
              if (helper_.InputFixedLiteralIsTrue(lit)) continue;
              return CreateUnsatModel(c, ct);
            }
            if (!helper_.SetMappedLiteralToTrue(MapLiteral(lit))) {
              return CreateUnsatModel(c, ct);
            }
          }
        } else if (first_copy) {
          if (!CopyBoolAndWithDupSupport(ct)) return CreateUnsatModel(c, ct);
        } else {
          if (!CopyBoolAnd(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kLinear:
        if (!CopyLinear(ct, /*canonicalize=*/first_copy ||
                                !variable_mapping_.empty())) {
          return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kIntProd:
        if (!CopyIntProd(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kIntDiv:
        if (!CopyIntDiv(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kIntMod:
        if (!CopyIntMod(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kElement:
        if (!CopyElement(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kTable:
        if (!CopyTable(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAutomaton:
        if (!CopyAutomaton(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAllDiff:
        if (!CopyAllDiff(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kLinMax:
        if (!CopyLinMax(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kCircuit:
        if (!CopyCircuit(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kRoutes:
        if (!CopyRoutes(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kInverse:
        if (!CopyInverse(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kReservoir:
        if (!CopyReservoir(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAtMostOne:
        if (!CopyAtMostOne(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kExactlyOne:
        if (!CopyExactlyOne(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kBoolXor:
        if (!CopyBoolXor(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kInterval:
        if (!CopyInterval(ct, c, ignore_names)) return CreateUnsatModel(c, ct);
        if (first_copy) {
          if (!AddLinearConstraintForInterval(ct))
            return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kNoOverlap:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          CopyAndMapNoOverlap(ct);
        }
        break;
      case ConstraintProto::kNoOverlap2D:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          CopyAndMapNoOverlap2D(ct);
        }
        break;
      case ConstraintProto::kCumulative:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          if (!CopyAndMapCumulative(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      default:
        LOG(FATAL) << "Unsupported constraint: " << ct.constraint_case();
        break;
    }
  }

  // This should be empty if first_copy is false.
  DCHECK(first_copy || constraints_using_intervals.empty());
  for (const int c : constraints_using_intervals) {
    const ConstraintProto& ct = in_model.constraints(c);
    if (!PrepareEnforcementCopyWithDup(ct)) continue;
    switch (ct.constraint_case()) {
      case ConstraintProto::kNoOverlap:
        CopyAndMapNoOverlap(ct);
        break;
      case ConstraintProto::kNoOverlap2D:
        CopyAndMapNoOverlap2D(ct);
        break;
      case ConstraintProto::kCumulative:
        if (!CopyAndMapCumulative(ct)) return CreateUnsatModel(c, ct);
        break;
      default:
        LOG(DFATAL) << "Shouldn't be here.";
    }
  }

  if (first_copy) {
    ExpandNonAffineExpressions();
  }

  return true;
}

bool ModelCopy::ImportObjective(const CpModelProto& in_model) {
  if (in_model.has_objective()) {
    return CopyObjective(in_model.objective());
  }
  return true;
}

// We have two modes:
// - If variable_mapping is empty, we  import the hint and load it
//   in the solution_crush to be updated as we copy.
// - Otherwise, we assume solution_crush is not needed, and we copy right
//   away the hint in the remapped format.
void ModelCopy::ImportSolutionHint(const CpModelProto& in_model) {
  if (!in_model.has_solution_hint()) return;
  const PartialVariableAssignment& hint = in_model.solution_hint();
  PartialVariableAssignment& new_hint =
      *working_model_->mutable_solution_hint();

  // Copy and remap.
  //
  // Note that we will clamp the hint in FinishCopy()
  // since the domain might be more precise then.
  int num_clamped = 0;
  std::vector<bool> hint_added(helper_.MappedDomains().size(), false);
  for (int i = 0; i < hint.vars_size(); ++i) {
    int mapped_ref = hint.vars(i);
    if (!variable_mapping_.empty()) {
      mapped_ref = variable_mapping_[mapped_ref];
      if (mapped_ref == kNoVariableMapping) continue;
    }

    const int mapped_var = PositiveRef(mapped_ref);
    if (hint_added[mapped_var]) continue;
    hint_added[mapped_var] = true;

    int64_t hint_value = hint.values(i);
    if (!RefIsPositive(mapped_ref)) {
      // We alread checked that this must have been a literal.
      // Note however that the hint value is from outside the solver, so
      // we distinguish true / false with hint_value > 0 and we inverse it.
      if (hint_value > 0) {
        hint_value = 0;
      } else {
        hint_value = 1;
      }
    }

    // We also clamp it.
    const int64_t closest_domain_value =
        helper_.MappedDomain(mapped_var).ClosestValue(hint_value);
    if (closest_domain_value != hint_value) {
      ++num_clamped;
      hint_value = closest_domain_value;
    }

    new_hint.add_vars(mapped_var);
    new_hint.add_values(hint_value);
  }
  if (num_clamped > 0) {
    helper_.UpdateRuleStats("hint: moved var hint within its domain",
                            num_clamped);
  }

  if (variable_mapping_.empty()) {
    // Load the solution hint in the solution_crush so that it can be updated
    // when new variables are added, for instance in
    // ExpandNonAffineExpressions(). This is not needed if there is a variable
    // mapping, because such mappings are only used on models where non affine
    // expressions have already been expanded.
    absl::flat_hash_map<int, int64_t> hint_values;
    const int num_vars = helper_.MappedDomains().size();
    for (int i = 0; i < new_hint.vars().size(); ++i) {
      hint_values[new_hint.vars(i)] = new_hint.values(i);
    }
    for (int i = 0; i < num_vars; ++i) {
      if (helper_.InputIsFixed(i)) {
        hint_values.insert({i, helper_.InputFixedValue(i)});
      }
    }
    helper_.solution_crush()->LoadSolution(num_vars, hint_values);
  }
}

bool ModelCopy::ImportEverythingExceptVariablesConstraintsAndHint(
    const CpModelProto& in_model, bool copy_symmetry) {
  if (!in_model.name().empty()) {
    working_model_->set_name(in_model.name());
  }
  if (in_model.has_objective()) {
    if (!CopyObjective(in_model.objective())) return false;
  }
  if (in_model.has_floating_point_objective()) {
    CopyFloatingPointObjective(in_model.floating_point_objective());
  }
  if (!in_model.search_strategy().empty()) {
    *working_model_->mutable_search_strategy() = in_model.search_strategy();
    for (DecisionStrategyProto& strategy :
         *working_model_->mutable_search_strategy()) {
      google::protobuf::util::RemoveIf(strategy.mutable_exprs(),
                                       [](const LinearExpressionProto* expr) {
                                         return expr->vars().empty();
                                       });
      // We make sure we do not use the old variables field.
      if (!strategy.variables().empty()) {
        CHECK(strategy.exprs().empty());
        for (const int ref : strategy.variables()) {
          LinearExpressionProto* expr = strategy.add_exprs();
          expr->add_vars(PositiveRef(ref));
          expr->add_coeffs(RefIsPositive(ref) ? 1 : -1);
        }
        strategy.clear_variables();
      }
      if (!variable_mapping_.empty()) {
        google::protobuf::RepeatedPtrField<LinearExpressionProto> mapped_exprs;
        for (LinearExpressionProto& expr : *strategy.mutable_exprs()) {
          CopyLinearExpression(expr, mapped_exprs.Add());
          // The expression can be canonicalized to a constant.
          if (mapped_exprs.rbegin()->vars().empty()) {
            mapped_exprs.RemoveLast();
          }
        }
        strategy.mutable_exprs()->Swap(&mapped_exprs);
      }
    }
  }
  if (!in_model.assumptions().empty()) {
    for (const int lit : in_model.assumptions()) {
      working_model_->add_assumptions(MapLiteralEvenIfFixed(lit));
    }
  }
  if (in_model.has_symmetry() && copy_symmetry) {
    CHECK(variable_mapping_.empty());
    *working_model_->mutable_symmetry() = in_model.symmetry();
  }
  return true;
}

bool ModelCopy::FinishCopy(const CpModelProto& in_model) {
  // Copy the mapped domains to the proto.
  absl::Span<const Domain> domains = helper_.MappedDomains();
  working_model_->mutable_variables()->Clear();
  working_model_->mutable_variables()->Reserve(domains.size());
  for (const Domain& domain : domains) {
    FillDomainInProto(domain, working_model_->add_variables());
  }

  // Copy the names if requested.
  if (!params_.ignore_names()) {
    const int num_original_vars = in_model.variables().size();
    if (variable_mapping_.empty()) {
      for (int i = 0; i < num_original_vars; ++i) {
        working_model_->mutable_variables(i)->set_name(
            in_model.variables(i).name());
      }
    } else {
      for (int i = 0; i < num_original_vars; ++i) {
        const auto& name = in_model.variables(i).name();
        if (name.empty()) continue;
        if (variable_mapping_[i] == kNoVariableMapping) continue;

        // We keep the name of the variable with smallest index if non-empty.
        const int image = PositiveRef(variable_mapping_[i]);
        if (working_model_->variables(image).name().empty()) {
          working_model_->mutable_variables(image)->set_name(name);
        }
      }
    }
  }

  // When there is no mapping, we might have created new variable and updated
  // the hint, so write it back. Note that we DCHECK() that we don't create
  // new variable when the mapping is non-empty.
  if (variable_mapping_.empty()) {
    helper_.solution_crush()->StoreSolutionAsHint(*working_model_);
  }

  helper_.DisplaySummary(logger_);
  return true;
}

bool ModelCopy::PrepareEnforcementCopy(const ConstraintProto& ct) {
  temp_enforcement_literals_.clear();
  for (const int lit : ct.enforcement_literal()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) continue;
      helper_.UpdateRuleStats("enforcement: always false");
      return false;
    }
    temp_enforcement_literals_.push_back(MapLiteral(lit));
  }
  return true;  // Continue processing.
}

bool ModelCopy::PrepareEnforcementCopyWithDup(const ConstraintProto& ct) {
  temp_enforcement_literals_.clear();
  temp_enforcement_literals_set_.clear();
  for (const int lit : ct.enforcement_literal()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) continue;
      helper_.UpdateRuleStats("enforcement: always false");
      return false;
    }
    const int mapped_lit = MapLiteral(lit);
    if (temp_enforcement_literals_set_.contains(mapped_lit)) {
      helper_.UpdateRuleStats("enforcement: removed duplicate literal");
      continue;
    }
    if (temp_enforcement_literals_set_.contains(NegatedRef(mapped_lit))) {
      helper_.UpdateRuleStats("enforcement: contains x and not(x)");
      return false;
    }

    temp_enforcement_literals_.push_back(mapped_lit);
    temp_enforcement_literals_set_.insert(mapped_lit);
  }
  return true;  // Continue processing.
}

void ModelCopy::FinishEnforcementCopy(ConstraintProto* ct) {
  ct->mutable_enforcement_literal()->Add(temp_enforcement_literals_.begin(),
                                         temp_enforcement_literals_.end());
}

bool ModelCopy::FinishBoolOrCopy() {
  if (temp_literals_.empty()) return false;

  if (temp_literals_.size() == 1) {
    helper_.UpdateRuleStats("bool_or: only one literal");
    return helper_.SetMappedLiteralToTrue(temp_literals_[0]);
  }

  working_model_->add_constraints()->mutable_bool_or()->mutable_literals()->Add(
      temp_literals_.begin(), temp_literals_.end());
  return true;
}

bool ModelCopy::CopyBoolOr(const ConstraintProto& ct) {
  temp_literals_.clear();
  for (const int lit : temp_enforcement_literals_) {
    temp_literals_.push_back(NegatedRef(lit));
  }
  for (const int lit : ct.bool_or().literals()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) return true;
      continue;
    }
    temp_literals_.push_back(MapLiteral(lit));
  }
  return FinishBoolOrCopy();
}

namespace {

// This is only used for LRAT proof support for pure-sat problem.
//
// This assumes an identity mapping between positive proto refs and Boolean
// variables (this might not be the case if the input proto contains non Boolean
// variables between Boolean ones).
Literal RefToLiteral(int ref) {
  return Literal(BooleanVariable(PositiveRef(ref)), RefIsPositive(ref));
}
int LiteralToRef(Literal lit) {
  const int var = lit.Variable().value();
  return lit.IsPositive() ? var : NegatedRef(var);
}

}  // namespace

bool ModelCopy::CopyBoolOrWithDupSupport(const ConstraintProto& ct,
                                         int one_based_cnf_index) {
  temp_literals_.clear();
  temp_literals_set_.clear();
  for (const int enforcement_lit : temp_enforcement_literals_) {
    // Having an enforcement literal is the same as having its negation on
    // the clause.
    const int lit = NegatedRef(enforcement_lit);

    // Note that we already dealt with duplicate since we should have called
    // PrepareEnforcementCopyWithDup() in this case.
    temp_literals_set_.insert(lit);
    temp_literals_.push_back(lit);
  }
  for (const int lit : ct.bool_or().literals()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) {
        helper_.UpdateRuleStats("bool_or: always true");
        return true;
      }
      continue;
    }

    const int mapped_lit = MapLiteral(lit);
    if (temp_literals_set_.contains(NegatedRef(mapped_lit))) {
      helper_.UpdateRuleStats("bool_or: always true");
      return true;
    }
    const auto [it, inserted] = temp_literals_set_.insert(mapped_lit);
    if (inserted) temp_literals_.push_back(mapped_lit);
  }
  DCHECK_EQ(temp_literals_.size(), temp_literals_set_.size());
  if (lrat_proof_handler_ != nullptr) {
    CHECK(variable_mapping_.empty());
    // Add the original clause as a problem clause, and its simplified version
    // as an inferred clause (only if it is different), with proof.
    temp_clause_.clear();
    for (const int lit : ct.enforcement_literal()) {
      temp_clause_.push_back(RefToLiteral(lit).Negated());
    }
    for (const int lit : ct.bool_or().literals()) {
      temp_clause_.push_back(RefToLiteral(lit));
    }
    ClausePtr tmp_clause = NewClausePtr(temp_clause_);
    lrat_proof_handler_->AddProblemClause(tmp_clause, one_based_cnf_index);

    if (temp_literals_.size() != temp_clause_.size()) {
      temp_proof_.clear();
      for (const Literal lit : temp_clause_) {
        if (!temp_literals_set_.contains(LiteralToRef(lit))) {
          temp_proof_.push_back(ClausePtr(lit.Negated()));
        }
      }
      temp_proof_.push_back(tmp_clause);
      temp_simplified_clause_.clear();
      for (const int lit : temp_literals_) {
        temp_simplified_clause_.push_back(RefToLiteral(lit));
      }
      const ClausePtr new_clause = NewClausePtr(temp_simplified_clause_);
      lrat_proof_handler_->AddInferredClause(new_clause, temp_proof_);
      DCHECK_GT(temp_clause_.size(), 1);
      lrat_proof_handler_->DeleteClause(tmp_clause);
      tmp_clause = new_clause;
    }
    if (tmp_clause.IsSatClausePtr()) {
      delete tmp_clause.GetSatClause();
    }
  }
  return FinishBoolOrCopy();
}

bool ModelCopy::CopyBoolAnd(const ConstraintProto& ct) {
  bool at_least_one_false = false;
  int num_non_fixed_literals = 0;
  for (const int lit : ct.bool_and().literals()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) continue;
      at_least_one_false = true;
      break;
    }
    num_non_fixed_literals++;
  }

  if (at_least_one_false) {
    // One enforcement literal must be false.
    BoolArgumentProto* bool_or =
        working_model_->add_constraints()->mutable_bool_or();
    for (const int lit : temp_enforcement_literals_) {
      bool_or->add_literals(NegatedRef(lit));
    }
    return !bool_or->literals().empty();
  } else if (num_non_fixed_literals > 0) {
    ConstraintProto* new_ct = working_model_->add_constraints();
    FinishEnforcementCopy(new_ct);
    BoolArgumentProto* bool_and = new_ct->mutable_bool_and();
    bool_and->mutable_literals()->Reserve(num_non_fixed_literals);
    for (const int lit : ct.bool_and().literals()) {
      if (helper_.InputIsFixed(lit)) continue;  // we tested above for false.
      bool_and->add_literals(MapLiteral(lit));
    }
  }
  return true;
}

bool ModelCopy::CopyBoolAndWithDupSupport(const ConstraintProto& ct) {
  DCHECK(!temp_enforcement_literals_.empty());

  bool at_least_one_false = false;
  temp_literals_.clear();
  temp_literals_set_.clear();
  for (const int lit : ct.bool_and().literals()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) continue;
      helper_.UpdateRuleStats("bool and: always false");
      at_least_one_false = true;
      break;
    }

    const int mapped_lit = MapLiteral(lit);
    if (temp_literals_set_.contains(NegatedRef(mapped_lit))) {
      helper_.UpdateRuleStats("bool and: => x and not(x) ");
      at_least_one_false = true;
      break;
    }
    if (temp_enforcement_literals_set_.contains(NegatedRef(mapped_lit))) {
      helper_.UpdateRuleStats("bool and: not(x) => x");
      at_least_one_false = true;
      break;
    }

    if (temp_enforcement_literals_set_.contains(mapped_lit)) {
      helper_.UpdateRuleStats("bool and: x => x");
      continue;
    }
    const auto [it, inserted] = temp_literals_set_.insert(mapped_lit);
    if (inserted) temp_literals_.push_back(mapped_lit);
  }

  if (at_least_one_false) {
    // One enforcement literal must be false.
    BoolArgumentProto* bool_or =
        working_model_->add_constraints()->mutable_bool_or();
    for (const int lit : temp_enforcement_literals_) {
      bool_or->add_literals(NegatedRef(lit));
    }
    return !bool_or->literals().empty();
  }

  if (temp_literals_.empty()) {
    helper_.UpdateRuleStats("bool and: empty");
    return true;
  }

  // Copy.
  ConstraintProto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  new_ct->mutable_bool_and()->mutable_literals()->Add(temp_literals_.begin(),
                                                      temp_literals_.end());
  return true;
}

template <typename T>
int64_t ModelCopy::FillNonFixedTermsAndReturnOffset(
    const T& proto_with_vars_and_coeffs, int64_t offset) {
  non_fixed_terms_.clear();
  for (int i = 0; i < proto_with_vars_and_coeffs.vars_size(); ++i) {
    int ref = proto_with_vars_and_coeffs.vars(i);
    int64_t coeff = proto_with_vars_and_coeffs.coeffs(i);
    MapTerm(ref, coeff, offset);
    if (coeff == 0) continue;
    DCHECK(RefIsPositive(ref));
    non_fixed_terms_.push_back({ref, coeff});
  }
  return offset;
}

bool ModelCopy::CopyLinearExpression(
    const LinearExpressionProto& expr, LinearExpressionProto* dst,
    const absl::flat_hash_set<int>* mapped_enforcement_literals) {
  int64_t offset = FillNonFixedTermsAndReturnOffset(expr, expr.offset());
  CanonicalizeLinearExpression(mapped_enforcement_literals, non_fixed_terms_,
                               offset);

  dst->set_offset(offset);
  for (const auto& [var, coeff] : non_fixed_terms_) {
    dst->add_vars(var);
    dst->add_coeffs(coeff);
  }
  return true;
}

bool ModelCopy::CopyLinear(const ConstraintProto& ct, bool canonicalize) {
  int64_t offset = FillNonFixedTermsAndReturnOffset(ct.linear());
  if (canonicalize) {
    // TODO(user): In practice we always do because this is either user-given
    // or we use a variable_mapping_ that might merge variables.
    CanonicalizeLinearExpression(&temp_enforcement_literals_set_,
                                 non_fixed_terms_, offset);
  }

  int64_t min_activity = 0;
  int64_t max_activity = 0;
  for (const auto& [var, coeff] : non_fixed_terms_) {
    DCHECK_NE(coeff, 0);
    const Domain& domain = helper_.MappedDomain(var);
    if (coeff > 0) {
      min_activity += coeff * domain.Min();
      max_activity += coeff * domain.Max();
    } else {
      min_activity += coeff * domain.Max();
      max_activity += coeff * domain.Min();
    }
  }
  const Domain implied(min_activity, max_activity);
  const Domain new_rhs =
      ReadDomainFromProto(ct.linear()).AdditionWith(Domain(-offset));

  // Trivial constraint?
  if (implied.IsIncludedIn(new_rhs)) {
    helper_.UpdateRuleStats("linear: always true");
    return true;
  }

  // Constraint is false?
  const Domain tight_domain = implied.IntersectionWith(new_rhs);
  if (tight_domain.IsEmpty()) {
    if (temp_enforcement_literals_.empty()) return false;
    temp_literals_.clear();
    for (const int literal : temp_enforcement_literals_) {
      temp_literals_.push_back(NegatedRef(literal));
    }
    working_model_->add_constraints()
        ->mutable_bool_or()
        ->mutable_literals()
        ->Add(temp_literals_.begin(), temp_literals_.end());
    return true;
  }

  DCHECK(!non_fixed_terms_.empty());

  if (non_fixed_terms_.size() == 1 && temp_enforcement_literals_.empty()) {
    helper_.UpdateRuleStats("linear1: x in domain");
    const auto [single_var, coeff] = non_fixed_terms_[0];
    const Domain new_var_domain = new_rhs.InverseMultiplicationBy(coeff);
    return helper_.IntersectMappedDomainWith(single_var, new_var_domain);
  }

  ConstraintProto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  LinearConstraintProto* linear = new_ct->mutable_linear();
  linear->mutable_vars()->Reserve(non_fixed_terms_.size());
  linear->mutable_coeffs()->Reserve(non_fixed_terms_.size());
  for (const auto& [var, coeff] : non_fixed_terms_) {
    linear->add_vars(var);
    linear->add_coeffs(coeff);
  }
  FillDomainInProto(tight_domain, linear);
  return true;
}

template <typename T>
void ModelCopy::CanonicalizeLinearExpression(
    const absl::flat_hash_set<int>* enforcement_literals,
    std::vector<std::pair<int, T>>& terms, T& offset) {
  // Merge terms with the same variable, remove terms with a zero coefficient
  // and replace the enforcement literals with their value.
  int current_var = 0;
  T current_coeff = 0;
  int new_size = 0;
  auto maybe_add_current_term = [&]() {
    if (enforcement_literals != nullptr) {
      if (enforcement_literals->contains(current_var)) {
        // If the constraint is enforced, we can assume the variable is at 1.
        offset += current_coeff;
        helper_.UpdateRuleStats("linear: enforcement literal in expression");
        return;
      } else if (enforcement_literals->contains(NegatedRef(current_var))) {
        // We can assume the variable is at 0.
        helper_.UpdateRuleStats("linear: enforcement literal in expression");
        return;
      }
    }
    terms[new_size++] = {current_var, current_coeff};
  };
  std::sort(terms.begin(), terms.end());
  for (const auto [var, coeff] : terms) {
    if (var == current_var) {
      current_coeff += coeff;
    } else {
      if (current_coeff != 0) {
        maybe_add_current_term();
      }
      current_var = var;
      current_coeff = coeff;
    }
  }
  if (current_coeff != 0) {
    maybe_add_current_term();
  }
  if (new_size < terms.size()) {
    helper_.UpdateRuleStats("linear: fixed or dup variables");
  }
  terms.resize(new_size);
}

void ModelCopy::ConvertSingleVarFormatToExpr(int var,
                                             LinearExpressionProto* expr) {
  if (helper_.InputIsFixed(var)) {
    expr->set_offset(helper_.InputFixedValue(var));
  } else {
    DCHECK(RefIsPositive(var));
    expr->mutable_vars()->Reserve(1);
    expr->mutable_coeffs()->Reserve(1);
    expr->add_coeffs(1);
    if (!variable_mapping_.empty()) {
      // We actually only encounter the old format with a mapping in tests.
      // And in this case, we just have positive maps.
      var = variable_mapping_[var];
      DCHECK_NE(var, kNoVariableMapping);
      DCHECK(RefIsPositive(var));
    }
    expr->add_vars(var);
  }
}

bool ModelCopy::CopyElement(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  if (ct.element().vars().empty() && !ct.element().exprs().empty()) {
    // New format, just copy and remap variables.
    FinishEnforcementCopy(new_ct);
    CopyLinearExpression(ct.element().linear_index(),
                         new_ct->mutable_element()->mutable_linear_index());
    CopyLinearExpression(ct.element().linear_target(),
                         new_ct->mutable_element()->mutable_linear_target());
    for (const LinearExpressionProto& expr : ct.element().exprs()) {
      CopyLinearExpression(expr, new_ct->mutable_element()->add_exprs());
    }
    return true;
  }

  FinishEnforcementCopy(new_ct);
  ConvertSingleVarFormatToExpr(
      ct.element().index(), new_ct->mutable_element()->mutable_linear_index());
  ConvertSingleVarFormatToExpr(
      ct.element().target(),
      new_ct->mutable_element()->mutable_linear_target());
  for (const int var : ct.element().vars()) {
    ConvertSingleVarFormatToExpr(var, new_ct->mutable_element()->add_exprs());
  }
  return true;
}

bool ModelCopy::CopyAutomaton(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  new_ct->mutable_automaton()->set_starting_state(
      ct.automaton().starting_state());
  *new_ct->mutable_automaton()->mutable_final_states() =
      ct.automaton().final_states();
  *new_ct->mutable_automaton()->mutable_transition_tail() =
      ct.automaton().transition_tail();
  *new_ct->mutable_automaton()->mutable_transition_head() =
      ct.automaton().transition_head();
  *new_ct->mutable_automaton()->mutable_transition_label() =
      ct.automaton().transition_label();
  for (const LinearExpressionProto& expr : ct.automaton().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_automaton()->add_exprs());
  }
  FinishEnforcementCopy(new_ct);

  for (const int var : ct.automaton().vars()) {
    ConvertSingleVarFormatToExpr(var, new_ct->mutable_automaton()->add_exprs());
  }

  return true;
}

bool ModelCopy::CopyTable(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  if (ct.table().vars().empty() && !ct.table().exprs().empty()) {
    // New format, just copy and remap variables.
    FinishEnforcementCopy(new_ct);
    *new_ct->mutable_table()->mutable_values() = ct.table().values();
    for (const LinearExpressionProto& expr : ct.table().exprs()) {
      CopyLinearExpression(expr, new_ct->mutable_table()->add_exprs());
    }
    new_ct->mutable_table()->set_negated(ct.table().negated());
    return true;
  }

  FinishEnforcementCopy(new_ct);
  for (const int var : ct.table().vars()) {
    ConvertSingleVarFormatToExpr(var, new_ct->mutable_table()->add_exprs());
  }
  *new_ct->mutable_table()->mutable_values() = ct.table().values();
  new_ct->mutable_table()->set_negated(ct.table().negated());

  return true;
}

bool ModelCopy::CopyAllDiff(const ConstraintProto& ct) {
  if (ct.all_diff().exprs().size() <= 1) return true;
  ConstraintProto* new_ct = working_model_->add_constraints();
  for (const LinearExpressionProto& expr : ct.all_diff().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_all_diff()->add_exprs());
  }
  FinishEnforcementCopy(new_ct);
  return true;
}

bool ModelCopy::CopyLinMax(const ConstraintProto& ct) {
  // We will create it lazily if we end up copying something.
  ConstraintProto* new_ct = nullptr;

  // Regroup all constant terms and copy the other.
  int64_t max_of_fixed_terms = kint64min;
  for (const auto& expr : ct.lin_max().exprs()) {
    const std::optional<int64_t> fixed = helper_.InputFixedValueOrNullopt(expr);
    if (fixed != std::nullopt) {
      max_of_fixed_terms = std::max(max_of_fixed_terms, fixed.value());
    } else {
      // copy.
      if (new_ct == nullptr) {
        new_ct = working_model_->add_constraints();
      }
      CopyLinearExpression(expr, new_ct->mutable_lin_max()->add_exprs());
    }
  }

  // If we have no non-fixed expression, we can just fix the target when it
  // involves at most one variable.
  const LinearExpressionProto& target = ct.lin_max().target();
  if (new_ct == nullptr && temp_enforcement_literals_.empty() &&
      target.vars().size() <= 1) {
    helper_.UpdateRuleStats("lin_max: all exprs fixed during copy");

    // coeff * var + offset == max_of_fixed_terms.
    absl::int128 rhs = absl::int128(max_of_fixed_terms) - target.offset();
    if (target.vars().empty()) {
      return rhs == 0;  // Unsat otherwise.
    }

    int var = target.vars(0);
    int64_t coeff = target.coeffs(0);
    int64_t offset = 0;
    MapTerm(var, coeff, offset);
    rhs -= offset;

    if (coeff == 0) {
      return rhs == 0;  // Unsat otherwise.
    }

    if (rhs % coeff != 0) return false;
    rhs /= coeff;
    if (rhs > absl::int128(kint64max) || rhs < absl::int128(kint64min)) {
      helper_.UpdateRuleStats(
          "lin_max: target must take value outside int64_t range");
      return false;
    }

    return helper_.IntersectMappedDomainWith(var,
                                             Domain(static_cast<int64_t>(rhs)));
  }

  // Otherwise, add a constant term if needed.
  if (max_of_fixed_terms > kint64min) {
    if (new_ct == nullptr) {
      new_ct = working_model_->add_constraints();
    }
    new_ct->mutable_lin_max()->add_exprs()->set_offset(max_of_fixed_terms);
  }

  // Finish by copying the target.
  if (new_ct == nullptr) return false;  // No expr == unsat.
  CopyLinearExpression(ct.lin_max().target(),
                       new_ct->mutable_lin_max()->mutable_target());
  FinishEnforcementCopy(new_ct);
  return true;
}

bool ModelCopy::CopyCircuit(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  *new_ct->mutable_circuit()->mutable_tails() = ct.circuit().tails();
  *new_ct->mutable_circuit()->mutable_heads() = ct.circuit().heads();
  new_ct->mutable_circuit()->mutable_literals()->Reserve(
      ct.circuit().literals_size());
  for (const int lit : ct.circuit().literals()) {
    new_ct->mutable_circuit()->add_literals(MapLiteralEvenIfFixed(lit));
  }
  return true;
}

bool ModelCopy::CopyRoutes(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  *new_ct->mutable_routes()->mutable_tails() = ct.routes().tails();
  *new_ct->mutable_routes()->mutable_heads() = ct.routes().heads();
  new_ct->mutable_routes()->mutable_literals()->Reserve(
      ct.routes().literals_size());
  for (const int lit : ct.routes().literals()) {
    new_ct->mutable_routes()->add_literals(MapLiteralEvenIfFixed(lit));
  }
  new_ct->mutable_routes()->mutable_dimensions()->Reserve(
      ct.routes().dimensions_size());
  for (const RoutesConstraintProto::NodeExpressions& exprs :
       ct.routes().dimensions()) {
    auto* new_exprs =
        new_ct->mutable_routes()->add_dimensions()->mutable_exprs();
    new_exprs->Reserve(exprs.exprs_size());
    for (const LinearExpressionProto& expr : exprs.exprs()) {
      CopyLinearExpression(expr, new_exprs->Add());
    }
  }
  return true;
}

bool ModelCopy::CopyInverse(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  if (ct.inverse().f_direct().empty() &&
      !ct.inverse().f_expr_direct().empty()) {
    // New format, just copy and remap variables.
    FinishEnforcementCopy(new_ct);
    for (const LinearExpressionProto& expr : ct.inverse().f_expr_direct()) {
      CopyLinearExpression(expr,
                           new_ct->mutable_inverse()->add_f_expr_direct());
    }
    for (const LinearExpressionProto& expr : ct.inverse().f_expr_inverse()) {
      CopyLinearExpression(expr,
                           new_ct->mutable_inverse()->add_f_expr_inverse());
    }
    return true;
  }

  FinishEnforcementCopy(new_ct);
  for (const int f : ct.inverse().f_direct()) {
    ConvertSingleVarFormatToExpr(
        f, new_ct->mutable_inverse()->add_f_expr_direct());
  }
  for (const int f : ct.inverse().f_inverse()) {
    ConvertSingleVarFormatToExpr(
        f, new_ct->mutable_inverse()->add_f_expr_inverse());
  }
  return true;
}

bool ModelCopy::CopyReservoir(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  new_ct->mutable_reservoir()->set_min_level(ct.reservoir().min_level());
  new_ct->mutable_reservoir()->set_max_level(ct.reservoir().max_level());
  new_ct->mutable_reservoir()->mutable_time_exprs()->Reserve(
      ct.reservoir().time_exprs_size());
  for (const LinearExpressionProto& expr : ct.reservoir().time_exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_reservoir()->add_time_exprs());
  }
  new_ct->mutable_reservoir()->mutable_level_changes()->Reserve(
      ct.reservoir().level_changes_size());
  for (const LinearExpressionProto& expr : ct.reservoir().level_changes()) {
    CopyLinearExpression(expr,
                         new_ct->mutable_reservoir()->add_level_changes());
  }
  new_ct->mutable_reservoir()->mutable_active_literals()->Reserve(
      ct.reservoir().active_literals_size());
  for (const int lit : ct.reservoir().active_literals()) {
    new_ct->mutable_reservoir()->add_active_literals(
        MapLiteralEvenIfFixed(lit));
  }
  return true;
}

bool ModelCopy::CopyAtMostOne(const ConstraintProto& ct) {
  if (!ct.enforcement_literal().empty()) {
    ConstraintProto new_ct;
    FinishEnforcementCopy(&new_ct);
    LiteralsToLinear(ct.at_most_one().literals(), /*lb=*/0, /*ub=*/1,
                     new_ct.mutable_linear());
    return CopyLinear(new_ct, true);
  }
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.at_most_one().literals()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) num_true++;
      continue;
    }
    temp_literals_.push_back(MapLiteral(lit));
  }

  if (num_true > 1) return false;
  if (num_true == 1) {
    for (int lit : temp_literals_) {
      if (!helper_.SetMappedLiteralToFalse(lit)) return false;
    }
    return true;
  }
  if (temp_literals_.size() <= 1) return true;

  ConstraintProto* new_ct = working_model_->add_constraints();
  new_ct->mutable_at_most_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyExactlyOne(const ConstraintProto& ct) {
  if (!ct.enforcement_literal().empty()) {
    ConstraintProto new_ct;
    FinishEnforcementCopy(&new_ct);
    LiteralsToLinear(ct.exactly_one().literals(), /*lb=*/1, /*ub=*/1,
                     new_ct.mutable_linear());
    return CopyLinear(new_ct, true);
  }
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.exactly_one().literals()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) num_true++;
      continue;
    }
    temp_literals_.push_back(MapLiteral(lit));
  }

  if (num_true > 1) return false;
  if (num_true == 1) {
    for (int lit : temp_literals_) {
      if (!helper_.SetMappedLiteralToFalse(lit)) return false;
    }
    return true;
  }
  if (temp_literals_.empty()) return false;
  if (temp_literals_.size() == 1) {
    return helper_.SetMappedLiteralToTrue(temp_literals_[0]);
  }

  ConstraintProto* new_ct = working_model_->add_constraints();
  new_ct->mutable_exactly_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyBoolXor(const ConstraintProto& ct) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);

  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.bool_xor().literals()) {
    if (helper_.InputIsFixed(lit)) {
      if (helper_.InputFixedLiteralIsTrue(lit)) num_true++;
      continue;
    }
    temp_literals_.push_back(MapLiteral(lit));
  }
  if (num_true % 2 == 1) {
    temp_literals_.push_back(GetTrueMappedLiteral());
  }
  new_ct->mutable_bool_xor()->mutable_literals()->Add(temp_literals_.begin(),
                                                      temp_literals_.end());
  return true;
}

bool ModelCopy::CopyInterval(const ConstraintProto& ct, int c,
                             bool ignore_names) {
  CHECK_EQ(starting_constraint_index_, 0)
      << "Adding new interval constraints to partially filled model is not "
         "supported.";
  interval_mapping_[c] = working_model_->constraints_size();
  ConstraintProto* new_ct = working_model_->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  if (temp_enforcement_literals_.size() > 1) {
    temp_enforcement_literals_ = {
        GetOrCreateVariableForConjunction(&temp_enforcement_literals_)};
    temp_enforcement_literals_set_ = {temp_enforcement_literals_.front()};
  }
  FinishEnforcementCopy(new_ct);
  CopyLinearExpression(ct.interval().start(),
                       new_ct->mutable_interval()->mutable_start(),
                       &temp_enforcement_literals_set_);
  CopyLinearExpression(ct.interval().size(),
                       new_ct->mutable_interval()->mutable_size(),
                       &temp_enforcement_literals_set_);
  CopyLinearExpression(ct.interval().end(),
                       new_ct->mutable_interval()->mutable_end(),
                       &temp_enforcement_literals_set_);
  return true;
}

bool ModelCopy::CopyIntProd(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  FinishEnforcementCopy(new_ct);
  for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_prod()->add_exprs());
  }
  CopyLinearExpression(ct.int_prod().target(),
                       new_ct->mutable_int_prod()->mutable_target());
  return true;
}

bool ModelCopy::CopyIntDiv(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  FinishEnforcementCopy(new_ct);
  for (const LinearExpressionProto& expr : ct.int_div().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_div()->add_exprs());
  }
  CopyLinearExpression(ct.int_div().target(),
                       new_ct->mutable_int_div()->mutable_target());
  return true;
}

bool ModelCopy::CopyIntMod(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = working_model_->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  FinishEnforcementCopy(new_ct);
  for (const LinearExpressionProto& expr : ct.int_mod().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_mod()->add_exprs());
  }
  CopyLinearExpression(ct.int_mod().target(),
                       new_ct->mutable_int_mod()->mutable_target());
  return true;
}

bool ModelCopy::AddLinearConstraintForInterval(const ConstraintProto& ct) {
  // Add the linear constraint enforcement => (start + size == end).
  //
  // We rely on the presolve for simplification, but deal with the trivial
  // case of (start, offset, start + offset) here.
  const IntervalConstraintProto& itv = ct.interval();
  if (itv.size().vars().empty() &&
      itv.start().offset() + itv.size().offset() == itv.end().offset() &&
      absl::Span<const int>(itv.start().vars()) ==
          absl::Span<const int>(itv.end().vars()) &&
      absl::Span<const int64_t>(itv.start().coeffs()) ==
          absl::Span<const int64_t>(itv.end().coeffs())) {
    // Trivial constraint, nothing to do.
  } else {
    tmp_constraint_.Clear();
    *tmp_constraint_.mutable_enforcement_literal() = ct.enforcement_literal();
    LinearConstraintProto* mutable_linear = tmp_constraint_.mutable_linear();

    mutable_linear->add_domain(0);
    mutable_linear->add_domain(0);
    AddLinearExpressionToLinearConstraint(itv.start(), 1, mutable_linear);
    AddLinearExpressionToLinearConstraint(itv.size(), 1, mutable_linear);
    AddLinearExpressionToLinearConstraint(itv.end(), -1, mutable_linear);
    if (!CopyLinear(tmp_constraint_, true)) return false;
  }

  // An enforced interval must have its size non-negative.
  //
  // Tricky: This is only called during first copy, so there is no mapping and
  // we can use the MappedMinOf(). Alternatively we could look at the domain of
  // the input cp_model_proto directly.
  CHECK(variable_mapping_.empty());
  const LinearExpressionProto& size_expr = itv.size();
  if (helper_.MappedMinOf(size_expr) < 0) {
    tmp_constraint_.Clear();
    *tmp_constraint_.mutable_enforcement_literal() = ct.enforcement_literal();
    *tmp_constraint_.mutable_linear()->mutable_vars() = size_expr.vars();
    *tmp_constraint_.mutable_linear()->mutable_coeffs() = size_expr.coeffs();
    tmp_constraint_.mutable_linear()->add_domain(-size_expr.offset());
    tmp_constraint_.mutable_linear()->add_domain(kint64max);
    if (!CopyLinear(tmp_constraint_, true)) return false;
  }

  return true;
}

int ModelCopy::GetOrCreateVariableForConjunction(std::vector<int>* literals) {
  // Variable mapping is only used to copy models where multiple enforcement
  // literals in constraints which do not support them have already been
  // replaced with their conjunction. Otherwise we would need to extend the
  // variable mapping (if there is one) and to make sure the solution crush
  // works with a variable mapping.
  CHECK(variable_mapping_.empty());
  std::sort(literals->begin(), literals->end());
  auto it = boolean_product_encoding_.find(*literals);
  if (it != boolean_product_encoding_.end()) return it->second;

  const int new_var = helper_.NewIntVar(Domain(0, 1));
  helper_.solution_crush()->SetVarToConjunction(new_var, *literals);

  boolean_product_encoding_[*literals] = new_var;
  // Add the constraint 'literals => new_var'
  auto* ct1 = working_model_->add_constraints();
  ct1->mutable_bool_or()->mutable_literals()->Reserve(literals->size() + 1);
  for (const int literal : *literals) {
    ct1->mutable_bool_or()->add_literals(NegatedRef(literal));
  }
  ct1->mutable_bool_or()->add_literals(new_var);
  // Add the constraint 'new_var => literals'
  auto* ct2 = working_model_->add_constraints();
  ct2->add_enforcement_literal(new_var);
  *ct2->mutable_bool_and()->mutable_literals() = {literals->begin(),
                                                  literals->end()};
  return new_var;
}

void ModelCopy::CopyAndMapNoOverlap(const ConstraintProto& ct) {
  // Note that we don't copy names here.
  auto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  NoOverlapConstraintProto* no_overlap = new_ct->mutable_no_overlap();
  no_overlap->mutable_intervals()->Reserve(ct.no_overlap().intervals().size());
  for (const int index : ct.no_overlap().intervals()) {
    const int new_index = interval_mapping_[index];
    if (new_index != -1) {
      no_overlap->add_intervals(new_index);
    }
  }
}

void ModelCopy::CopyAndMapNoOverlap2D(const ConstraintProto& ct) {
  // Note that we don't copy names here.
  auto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  NoOverlap2DConstraintProto* no_overlap_2d = new_ct->mutable_no_overlap_2d();
  const int num_intervals = ct.no_overlap_2d().x_intervals().size();
  no_overlap_2d->mutable_x_intervals()->Reserve(num_intervals);
  no_overlap_2d->mutable_y_intervals()->Reserve(num_intervals);
  for (int i = 0; i < num_intervals; ++i) {
    const int new_x = interval_mapping_[ct.no_overlap_2d().x_intervals(i)];
    if (new_x == -1) continue;
    const int new_y = interval_mapping_[ct.no_overlap_2d().y_intervals(i)];
    if (new_y == -1) continue;
    no_overlap_2d->add_x_intervals(new_x);
    no_overlap_2d->add_y_intervals(new_y);
  }
}

bool ModelCopy::CopyAndMapCumulative(const ConstraintProto& ct) {
  const std::optional<int64_t> fixed_capa =
      helper_.InputFixedValueOrNullopt(ct.cumulative().capacity());
  if (ct.cumulative().intervals().empty() && fixed_capa != std::nullopt) {
    // Trivial constraint, either obviously SAT or UNSAT if enforced.
    const int64_t capacity = fixed_capa.value();
    if (temp_enforcement_literals_.empty()) {
      return capacity >= 0;
    }
    if (capacity < 0) {
      // At least one enforcement literal must be false.
      auto* new_ct = working_model_->add_constraints();
      for (const int literal : temp_enforcement_literals_) {
        new_ct->mutable_bool_or()->add_literals(NegatedRef(literal));
      }
    }
    return true;
  }
  // Note that we don't copy names here.
  auto* new_ct = working_model_->add_constraints();
  FinishEnforcementCopy(new_ct);
  CumulativeConstraintProto* cumulative = new_ct->mutable_cumulative();
  CopyLinearExpression(ct.cumulative().capacity(),
                       cumulative->mutable_capacity());

  const int num_intervals = ct.cumulative().intervals().size();
  cumulative->mutable_intervals()->Reserve(num_intervals);
  cumulative->mutable_demands()->Reserve(num_intervals);
  for (int i = 0; i < num_intervals; ++i) {
    const int new_index = interval_mapping_[ct.cumulative().intervals(i)];
    if (new_index != -1) {
      cumulative->add_intervals(new_index);
      CopyLinearExpression(ct.cumulative().demands(i),
                           cumulative->add_demands());
    }
  }

  return true;
}

bool ModelCopy::CopyObjective(const CpObjectiveProto& objective) {
  int64_t offset = FillNonFixedTermsAndReturnOffset(objective);
  CanonicalizeLinearExpression(/*enforcement_literals=*/nullptr,
                               non_fixed_terms_, offset);

  CpObjectiveProto& new_objective = *working_model_->mutable_objective();
  new_objective = objective;
  new_objective.clear_vars();
  new_objective.clear_coeffs();
  int64_t min_activity = 0;
  int64_t max_activity = 0;
  for (const auto [var, coeff] : non_fixed_terms_) {
    new_objective.add_vars(var);
    new_objective.add_coeffs(coeff);
    const Domain& domain = helper_.MappedDomain(var);
    if (coeff > 0) {
      min_activity += coeff * domain.Min();
      max_activity += coeff * domain.Max();
    } else {
      min_activity += coeff * domain.Max();
      max_activity += coeff * domain.Min();
    }
  }
  new_objective.set_offset(new_objective.offset() +
                           static_cast<double>(offset));
  if (objective.domain_size() > 0) {
    Domain domain =
        ReadDomainFromProto(objective).AdditionWith(Domain(-offset));
    domain = domain.IntersectionWith(Domain(min_activity, max_activity));
    if (domain.IsEmpty()) return false;
    FillDomainInProto(domain, &new_objective);
  }
  new_objective.set_integer_before_offset(
      new_objective.integer_before_offset() + offset);
  return true;
}

void ModelCopy::CopyFloatingPointObjective(
    const FloatObjectiveProto& objective) {
  std::vector<std::pair<int, double>> non_fixed_terms;
  double offset = 0;
  for (int i = 0; i < objective.vars_size(); ++i) {
    int ref = objective.vars(i);
    double coeff = objective.coeffs(i);
    MapTerm(ref, coeff, offset);
    if (coeff == 0) continue;
    DCHECK(RefIsPositive(ref));
    non_fixed_terms.push_back({ref, coeff});
  }
  CanonicalizeLinearExpression(/*enforcement_literals=*/nullptr,
                               non_fixed_terms, offset);

  FloatObjectiveProto& new_objective =
      *working_model_->mutable_floating_point_objective();
  new_objective = objective;
  new_objective.clear_vars();
  new_objective.clear_coeffs();
  for (const auto [ref, coeff] : non_fixed_terms) {
    new_objective.add_vars(ref);
    new_objective.add_coeffs(coeff);
  }
  new_objective.set_offset(new_objective.offset() + offset);
}

bool ModelCopy::CreateUnsatModel(int c, const ConstraintProto& ct) {
  working_model_->mutable_constraints()->Clear();
  working_model_->add_constraints()->mutable_bool_or();

  std::string proto_string;
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
  static_assert(kTargetOsSupportsProtoDescriptor);
  google::protobuf::TextFormat::Printer printer;
  SetupTextFormatPrinter(&printer);
  printer.PrintToString(ct, &proto_string);
#else
  static_assert(!kTargetOsSupportsProtoDescriptor);
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
  std::string message = absl::StrCat(
      "proven during initial copy of constraint #", c, ":\n", proto_string);
  std::vector<int> vars = UsedVariables(ct);
  if (vars.size() < 10 && variable_mapping_.empty()) {
    absl::StrAppend(&message, "With current variable domains:\n");
    for (const int var : vars) {
      absl::StrAppend(&message, "var:", var,
                      " domain:", helper_.MappedDomain(var).ToString(), "\n");
    }
  }

  helper_.DisplaySummary(logger_);
  SOLVER_LOG(logger_, "INFEASIBLE: '", message, "'");
  return false;
}

void ModelCopy::ExpandNonAffineExpressions() {
  non_affine_expression_to_new_var_.clear();
  for (int c = 0; c < working_model_->constraints_size(); ++c) {
    ConstraintProto* const ct = working_model_->mutable_constraints(c);
    switch (ct->constraint_case()) {
      case ConstraintProto::kIntDiv:
        MaybeExpandNonAffineExpressions(ct->mutable_int_div());
        break;
      case ConstraintProto::kIntMod:
        MaybeExpandNonAffineExpressions(ct->mutable_int_mod());
        break;
      case ConstraintProto::kIntProd:
        MaybeExpandNonAffineExpressions(ct->mutable_int_prod());
        break;
      case ConstraintProto::kAllDiff:
        for (LinearExpressionProto& expr :
             *ct->mutable_all_diff()->mutable_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      case ConstraintProto::kElement:
        if (!ct->element().exprs().empty()) {
          MaybeExpandNonAffineExpression(
              ct->mutable_element()->mutable_linear_index());
          MaybeExpandNonAffineExpression(
              ct->mutable_element()->mutable_linear_target());
          for (LinearExpressionProto& expr :
               *ct->mutable_element()->mutable_exprs()) {
            MaybeExpandNonAffineExpression(&expr);
          }
        }
        break;
      case ConstraintProto::kInterval:
        MaybeExpandNonAffineExpression(ct->mutable_interval()->mutable_start());
        MaybeExpandNonAffineExpression(ct->mutable_interval()->mutable_end());
        MaybeExpandNonAffineExpression(ct->mutable_interval()->mutable_size());
        break;
      case ConstraintProto::kReservoir:
        for (LinearExpressionProto& expr :
             *ct->mutable_reservoir()->mutable_time_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      case ConstraintProto::kRoutes:
        for (RoutesConstraintProto::NodeExpressions& node_exprs :
             *ct->mutable_routes()->mutable_dimensions()) {
          for (LinearExpressionProto& expr : *node_exprs.mutable_exprs()) {
            MaybeExpandNonAffineExpression(&expr);
          }
        }
        break;
      case ConstraintProto::kTable:
        for (LinearExpressionProto& expr :
             *ct->mutable_table()->mutable_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      case ConstraintProto::kInverse:
        for (LinearExpressionProto& expr :
             *ct->mutable_inverse()->mutable_f_expr_direct()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        for (LinearExpressionProto& expr :
             *ct->mutable_inverse()->mutable_f_expr_inverse()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      case ConstraintProto::kAutomaton:
        for (LinearExpressionProto& expr :
             *ct->mutable_automaton()->mutable_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      default:
        break;
    }
  }
}

// Replaces the expression sum a_i * x_i + c with gcd * y + c, where y is a new
// variable defined with an additional constraint y = sum a_i / gcd * x_i.
void ModelCopy::MaybeExpandNonAffineExpression(LinearExpressionProto* expr) {
  int new_size = 0;
  for (int i = 0; i < expr->vars().size(); ++i) {
    if (expr->coeffs(i) != 0) {
      expr->set_vars(new_size, expr->vars(i));
      expr->set_coeffs(new_size, expr->coeffs(i));
      ++new_size;
    }
  }
  expr->mutable_vars()->Truncate(new_size);
  expr->mutable_coeffs()->Truncate(new_size);
  if (expr->vars_size() < 2) return;

  // Variable mapping is only used to copy models where non affine expressions
  // in constraints which do not support them have already been expanded.
  // Otherwise we would need to extend the variable mapping (if there is one)
  // and to make sure the solution crush works with a variable mapping.
  CHECK(variable_mapping_.empty());
  int64_t gcd = std::abs(expr->coeffs(0));
  for (int i = 1; i < expr->coeffs().size(); ++i) {
    gcd = std::gcd(gcd, std::abs(expr->coeffs(i)));
  }
  Domain domain(0);
  int64_t domain_min = 0;
  int64_t domain_max = 0;
  std::vector<std::pair<int, int64_t>> definition;
  definition.reserve(expr->vars().size());
  for (int i = 0; i < expr->vars().size(); ++i) {
    const int var = expr->vars(i);
    const int64_t coeff = expr->coeffs(i) / gcd;
    const Domain& domain = helper_.MappedDomain(var);  // no mapping.
    if (coeff > 0) {
      domain_min += coeff * domain.Min();
      domain_max += coeff * domain.Max();
    } else {
      domain_min += coeff * domain.Max();
      domain_max += coeff * domain.Min();
    }
    definition.push_back({var, coeff});
  }
  std::sort(definition.begin(), definition.end());
  int new_var;
  auto it = non_affine_expression_to_new_var_.find(definition);
  if (it != non_affine_expression_to_new_var_.end()) {
    new_var = it->second;
  } else {
    std::vector<std::pair<int, int64_t>> negated_definition;
    negated_definition.reserve(definition.size());
    for (const auto [var, coeff] : definition) {
      negated_definition.push_back({var, -coeff});
    }
    std::sort(negated_definition.begin(), negated_definition.end());
    it = non_affine_expression_to_new_var_.find(negated_definition);
    if (it != non_affine_expression_to_new_var_.end()) {
      new_var = it->second;
      gcd = -gcd;
    } else {
      new_var = helper_.NewIntVar(Domain(domain_min, domain_max));
      non_affine_expression_to_new_var_[definition] = new_var;
      auto* new_linear = working_model_->add_constraints()->mutable_linear();
      new_linear->add_vars(new_var);
      new_linear->add_coeffs(-1);
      for (const auto [var, coeff] : definition) {
        new_linear->add_vars(var);
        new_linear->add_coeffs(coeff);
      }
      new_linear->add_domain(0);
      new_linear->add_domain(0);
      helper_.solution_crush()->SetVarToLinearExpression(new_var, definition);
    }
  }
  expr->clear_vars();
  expr->clear_coeffs();
  expr->add_vars(new_var);
  expr->add_coeffs(gcd);
}

void ModelCopy::MaybeExpandNonAffineExpressions(
    LinearArgumentProto* linear_argument) {
  MaybeExpandNonAffineExpression(linear_argument->mutable_target());
  for (LinearExpressionProto& expr : *linear_argument->mutable_exprs()) {
    MaybeExpandNonAffineExpression(&expr);
  }
}

template <typename T>
void ModelCopy::MapTerm(int& var, T& coeff, T& offset) const {
  DCHECK(RefIsPositive(var));
  if (helper_.InputIsFixed(var)) {
    offset += coeff * helper_.InputFixedValue(var);
    coeff = 0;
    return;
  }
  if (variable_mapping_.empty()) return;
  const int mapped_ref = variable_mapping_[var];
  if (RefIsPositive(mapped_ref)) {
    var = mapped_ref;
  } else {
    // Only Boolean variables can be mapped to a negated var. If x is mapped to
    // NegatedRef(y), then coeff * x = coeff * (1 - y).
    offset += coeff;
    coeff = -coeff;
    var = NegatedRef(mapped_ref);
  }
  const Domain mapped_domain = helper_.MappedDomain(var);
  if (mapped_domain.IsFixed()) {
    offset += coeff * mapped_domain.Min();
    coeff = 0;
  }
}

int ModelCopy::GetTrueMappedLiteral() {
  if (!true_mapped_literal_.has_value()) {
    true_mapped_literal_ = kNoVariableMapping;
    const int num_vars = helper_.MappedDomains().size();
    for (int i = 0; i < num_vars; ++i) {
      const Domain& domain = helper_.MappedDomain(i);
      if (domain.IsEmpty() || !domain.IsFixed()) continue;
      if (domain.Min() == 1) {
        true_mapped_literal_ = i;
        break;
      } else if (domain.Min() == 0) {
        true_mapped_literal_ = NegatedRef(i);
        break;
      }
    }
    DCHECK_NE(*true_mapped_literal_, kNoVariableMapping);
  }
  return *true_mapped_literal_;
}

bool CopyModel(const CpModelProto& in_proto, CpModelProto* out_proto,
               Model* model) {
  out_proto->Clear();
  ModelCopy copier(out_proto, model);
  if (!copier.ImportVariables(in_proto)) return false;
  copier.ImportSolutionHint(in_proto);
  if (!copier.ImportAndSimplifyConstraints(in_proto, /*first_copy=*/true)) {
    return false;
  }
  if (!copier.ImportEverythingExceptVariablesConstraintsAndHint(in_proto)) {
    return false;
  }
  if (!copier.FinishCopy(in_proto)) {
    return false;
  }

  return true;
}

bool CopyModelAdvanced(const CpModelProto& in_proto,
                       absl::Span<const Domain> domains,
                       std::function<bool(int)> active_constraints,
                       std::vector<int>* interval_mapping,
                       CpModelProto* out_proto, Model* model) {
  out_proto->Clear();
  CHECK_EQ(domains.size(), in_proto.variables_size());

  ModelCopy copier(out_proto, model);
  if (!copier.CreateVariablesFromDomains(domains)) return false;
  copier.ImportSolutionHint(in_proto);
  if (!copier.ImportAndSimplifyConstraints(in_proto, /*first_copy=*/false,
                                           active_constraints)) {
    return false;
  }
  if (!copier.ImportEverythingExceptVariablesConstraintsAndHint(in_proto)) {
    return false;
  }
  if (!copier.FinishCopy(in_proto)) {
    return false;
  }

  interval_mapping->assign(copier.InternalIntervalMapping().begin(),
                           copier.InternalIntervalMapping().end());
  return true;
}

void VariableDomains::Reset(int num_vars) {
  domains_.assign(num_vars, Domain());
  has_two_values_.assign(num_vars, false);
  is_fixed_.assign(num_vars, false);
  fixed_vars_.clear();
}

void VariableDomains::Set(int var, Domain d) {
  has_two_values_[var] = d.HasTwoValues();
  if (is_fixed_[var]) {
    // The code here assume that once fixed, a variable stays that way.
    CHECK(d.IsFixed());
  } else if (d.IsFixed()) {
    is_fixed_[var] = true;
    fixed_vars_.push_back(var);
  }
  domains_[var] = std::move(d);
}

// Return false if one of the domain becomes empty (UNSAT). This might happen
// while we are cleaning up all workers at the end of a search.
bool VariableDomains::UpdateFromSharedBounds(
    absl::Span<const int> variable_mapping, int64_t& timestamp) {
  if (shared_bounds_ == nullptr) return true;
  shared_bounds_->GetChangedBounds(shared_bounds_id_, &tmp_variables_,
                                   &tmp_new_lower_bounds_,
                                   &tmp_new_upper_bounds_, &timestamp);
  for (int i = 0; i < tmp_variables_.size(); ++i) {
    const int input_var = tmp_variables_[i];
    const int ref = variable_mapping[input_var];
    if (ref == kNoVariableMapping) continue;
    Domain domain;
    if (RefIsPositive(ref)) {
      domain = domains_[ref].IntersectionWith(
          Domain(tmp_new_lower_bounds_[i], tmp_new_upper_bounds_[i]));
    } else {
      // Only Boolean variables can be mapped to a negative reference.
      domain = domains_[PositiveRef(ref)].IntersectionWith(
          Domain(1 - tmp_new_upper_bounds_[i], 1 - tmp_new_lower_bounds_[i]));
    }
    if (domain.IsEmpty()) return false;
    Set(PositiveRef(ref), domain);
  }
  return true;
}

DenseModelCopy::DenseModelCopy(absl::string_view name,
                               const CpModelProto& input_model_proto,
                               SharedBoundsManager* shared_bounds,
                               SharedClausesManager* shared_clauses)
    : input_model_proto_(input_model_proto),
      shared_clauses_(shared_clauses),
      model_proto_(input_model_proto),
      var_domains_(name, shared_bounds) {
  const int num_vars = input_model_proto_.variables_size();
  input_var_mapping_.resize(num_vars);
  reverse_mapping_.resize(num_vars);
  for (int i = 0; i < num_vars; ++i) {
    input_var_mapping_[i] = i;
    reverse_mapping_[i] = i;
  }
  ResetVarDomains();
}

bool DenseModelCopy::MaybeUpdate(bool& updated) {
  updated = false;
  int64_t new_bounds_timestamp = bounds_timestamp_;
  int64_t new_equivalences_timestamp = equivalences_timestamp_;
  if (!var_domains_.UpdateFromSharedBounds(input_var_mapping_,
                                           new_bounds_timestamp)) {
    return false;
  }
  std::vector<int> input_var_representatives;
  if (shared_clauses_ != nullptr) {
    input_var_representatives =
        shared_clauses_->GetRepresentatives(&new_equivalences_timestamp);
  }

  // TODO(user): throttle this to avoid recomputing too often?
  if (new_bounds_timestamp != bounds_timestamp_ ||
      new_equivalences_timestamp != equivalences_timestamp_) {
    updated = true;
    bounds_timestamp_ = new_bounds_timestamp;
    equivalences_timestamp_ = new_equivalences_timestamp;
    std::vector<int> new_input_var_mapping;
    if (!ComputeVariableMapping(input_var_representatives,
                                new_input_var_mapping) ||
        !ApplyVariableMapping(new_input_var_mapping)) {
      return false;
    }
    std::swap(input_var_mapping_, new_input_var_mapping);
  }
  return true;
}

bool DenseModelCopy::ComputeVariableMapping(
    absl::Span<const int> input_var_representatives,
    std::vector<int>& new_input_var_mapping) {
  const int num_input_vars = input_var_mapping_.size();
  const int num_vars = reverse_mapping_.size();

  if (!input_var_representatives.empty()) {
    // Convert the representative relations between the input variables into
    // representative relations between the mapped variables (using the current
    // input_var_mapping_).
    std::vector<int> var_representatives;
    var_representatives.reserve(num_vars);
    for (int i = 0; i < num_vars; ++i) {
      var_representatives.push_back(i);
    }
    for (int i = 0; i < input_var_representatives.size(); ++i) {
      const int input_rep = input_var_representatives[i];
      if (input_rep == i) continue;
      const int mapped_i = input_var_mapping_[i];
      const int mapped_rep = input_var_mapping_[PositiveRef(input_rep)];
      if (mapped_i == kNoVariableMapping) continue;
      if (mapped_rep == kNoVariableMapping) continue;
      if (RefIsPositive(mapped_i) == RefIsPositive(mapped_rep)) {
        var_representatives[PositiveRef(mapped_i)] =
            RefIsPositive(input_rep) ? mapped_rep : NegatedRef(mapped_rep);
      } else {
        var_representatives[PositiveRef(mapped_i)] =
            RefIsPositive(input_rep) ? NegatedRef(mapped_rep) : mapped_rep;
      }
    }

    // If a variable is fixed then the equivalent variables can be fixed too.
    auto fixed_value = [&](int ref) {
      if (RefIsPositive(ref)) {
        return var_domains_[ref].FixedValue();
      } else {
        // Only Boolean variables can be accessed with a negative reference.
        return 1 - var_domains_[PositiveRef(ref)].FixedValue();
      }
    };
    auto fix_literal = [&](int literal, bool value) {
      if (!RefIsPositive(literal)) {
        literal = PositiveRef(literal);
        value = !value;
      }
      const Domain new_domain =
          var_domains_[literal].IntersectionWith(Domain(value ? 1 : 0));
      if (new_domain.IsEmpty()) return false;
      var_domains_.Set(literal, new_domain);
      return true;
    };
    for (int i = 0; i < num_vars; ++i) {
      if (var_domains_.IsFixed(i)) {
        const int rep = var_representatives[i];
        if (rep != i) {
          if (!fix_literal(rep, fixed_value(i))) return false;
        }
      }
    }
    for (int i = 0; i < num_vars; ++i) {
      const int rep = var_representatives[i];
      if (rep != i && var_domains_.IsFixed(PositiveRef(rep))) {
        if (!fix_literal(i, fixed_value(rep))) return false;
      }
    }
  }

  new_input_var_mapping.assign(num_input_vars, kNoVariableMapping);
  fixed_input_var_values_.resize(num_input_vars, kint64min);
  reverse_mapping_.clear();
  int first_fixed_literal = -1;
  for (int input_var = 0; input_var < num_input_vars; ++input_var) {
    const int ref = input_var_mapping_[input_var];
    if (ref == kNoVariableMapping) continue;
    if (var_domains_[PositiveRef(ref)].IsFixed()) {
      if (RefIsPositive(ref)) {
        fixed_input_var_values_[input_var] = var_domains_[ref].FixedValue();
      } else {
        fixed_input_var_values_[input_var] =
            1 - var_domains_[PositiveRef(ref)].FixedValue();
      }
      // ModelCopy requires that if some literals are fixed, then one of them
      // must not be removed by the mapping.
      if (first_fixed_literal == -1 &&
          (fixed_input_var_values_[input_var] == 0 ||
           fixed_input_var_values_[input_var] == 1)) {
        first_fixed_literal = input_var;
      } else {
        continue;
      }
    }
    if (new_input_var_mapping[input_var] == kNoVariableMapping) {
      const int input_rep = input_var < input_var_representatives.size()
                                ? input_var_representatives[input_var]
                                : input_var;
      const int input_rep_var = PositiveRef(input_rep);
      if (new_input_var_mapping[input_rep_var] == kNoVariableMapping) {
        new_input_var_mapping[input_var] = reverse_mapping_.size();
        new_input_var_mapping[input_rep_var] =
            RefIsPositive(input_rep)
                ? new_input_var_mapping[input_var]
                : NegatedRef(new_input_var_mapping[input_var]);
        reverse_mapping_.push_back(input_var);
      } else {
        new_input_var_mapping[input_var] =
            RefIsPositive(input_rep)
                ? new_input_var_mapping[input_rep_var]
                : NegatedRef(new_input_var_mapping[input_rep_var]);
      }
    }
  }
  // Make sure that if a variable is removed in the current mapping, it stays
  // removed in the new one.
  for (int input_var = 0; input_var < num_input_vars; ++input_var) {
    if (input_var_mapping_[input_var] == kNoVariableMapping) {
      new_input_var_mapping[input_var] = kNoVariableMapping;
    }
  }
  return true;
}

bool DenseModelCopy::ApplyVariableMapping(
    absl::Span<const int> input_var_mapping) {
  model_proto_.Clear();

  const int num_input_vars = input_model_proto_.variables_size();
  std::vector<Domain> input_var_domains;
  input_var_domains.reserve(num_input_vars);
  for (int input_var = 0; input_var < num_input_vars; ++input_var) {
    const int current_ref = input_var_mapping_[input_var];
    if (input_var_mapping[input_var] == kNoVariableMapping) {
      DCHECK_NE(fixed_input_var_values_[input_var], kint64min);
      input_var_domains.push_back(Domain(fixed_input_var_values_[input_var]));
      continue;
    }
    DCHECK_NE(current_ref, kNoVariableMapping);
    if (RefIsPositive(current_ref)) {
      input_var_domains.push_back(var_domains_[current_ref]);
    } else {
      input_var_domains.push_back(
          var_domains_[PositiveRef(current_ref)].Negation().AdditionWith(
              Domain(1)));
    }
  }

  Model local_model;
  ModelCopy copier(&model_proto_, &local_model, input_var_mapping);
  if (!copier.CreateVariablesFromDomains(input_var_domains)) {
    return false;
  }
  if (!copier.ImportAndSimplifyConstraints(input_model_proto_)) {
    return false;
  }
  if (!copier.ImportEverythingExceptVariablesConstraintsAndHint(
          input_model_proto_, /*copy_symmetry=*/false)) {
    return false;
  }
  if (!copier.FinishCopy(input_model_proto_)) {
    return false;
  }
  ResetVarDomains();
  return true;
}

void DenseModelCopy::ResetVarDomains() {
  const int num_vars = model_proto_.variables_size();
  var_domains_.Reset(num_vars);
  for (int v = 0; v < num_vars; ++v) {
    var_domains_.Set(v, ReadDomainFromProto(model_proto_.variables(v)));
  }
}

std::vector<int64_t> DenseModelCopy::MapSolution(
    absl::Span<const int64_t> input_solution) const {
  std::vector<int64_t> solution;
  solution.reserve(reverse_mapping_.size());
  for (int var = 0; var < reverse_mapping_.size(); ++var) {
    const int input_ref = reverse_mapping_[var];
    if (RefIsPositive(input_ref)) {
      solution.push_back(input_solution[input_ref]);
    } else {
      // Only Boolean variables can be accessed with a negative reference.
      solution.push_back(1 - input_solution[PositiveRef(input_ref)]);
    }
  }
  return solution;
}

std::vector<int64_t> DenseModelCopy::ReverseMapSolution(
    absl::Span<const int64_t> solution) const {
  const int num_input_vars = input_var_mapping_.size();
  std::vector<int64_t> input_solution;
  input_solution.reserve(num_input_vars);
  for (int input_var = 0; input_var < num_input_vars; ++input_var) {
    const int ref = input_var_mapping_[input_var];
    if (ref == kNoVariableMapping) {
      input_solution.push_back(fixed_input_var_values_[input_var]);
      DCHECK_NE(input_solution.back(), kint64min);
    } else {
      const int64_t value = solution[PositiveRef(ref)];
      input_solution.push_back(RefIsPositive(ref) ? value : 1 - value);
    }
  }
  return input_solution;
}

}  // namespace sat
}  // namespace operations_research
