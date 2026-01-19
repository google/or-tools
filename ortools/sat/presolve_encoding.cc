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

#include "ortools/sat/presolve_encoding.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "google/protobuf/repeated_field.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/util/bitset.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

namespace {
bool ConstraintIsEncodingBound(const ConstraintProto& ct) {
  if (ct.constraint_case() != ConstraintProto::kLinear) return false;
  if (ct.linear().vars_size() != 1) return false;
  if (ct.linear().coeffs(0) != 1) return false;
  if (ct.enforcement_literal_size() != 1) return false;
  if (PositiveRef(ct.enforcement_literal(0)) == ct.linear().vars(0)) {
    return false;
  }
  return true;
}
}  // namespace

std::vector<VariableEncodingLocalModel> CreateVariableEncodingLocalModels(
    PresolveContext* context) {
  // In this function we want to make sure we don't waste too much time on
  // problems that do not have many linear1. Thus, the first thing we do is to
  // filter out as soon and cheaply as possible the bare minimum of constraints
  // that could be relevant to the final output.

  // Constraints taking a list of literals that can, under some conditions,
  // accept the following substitution:
  //   constraint(a, b, ...) => constraint(a | b, ...)
  // one obvious case is bool_or. But if we can know that a and b cannot be
  // both true, we can also apply this to at_most_one and exactly_one.
  //
  // Note that in the implementation we might for simplicity refer to the
  // constraints we are interested in as "bool_or" but this is just to avoid
  // mentioning all the three types over and over.
  // TODO(user): this should also work for linear constraints with the two
  // booleans having the same coefficient?
  std::vector<int> constraint_encoding_or;  // bool_or, exactly_one, at_most_one

  // Do a pass to gather all linear1 constraints.
  absl::flat_hash_map<int, absl::InlinedVector<int, 1>> var_to_linear1;
  for (int i = 0; i < context->working_model->constraints_size(); ++i) {
    const ConstraintProto& ct = context->working_model->constraints(i);
    if (ct.constraint_case() == ConstraintProto::kBoolOr ||
        ct.constraint_case() == ConstraintProto::kAtMostOne ||
        ct.constraint_case() == ConstraintProto::kExactlyOne) {
      constraint_encoding_or.push_back(i);
      continue;
    }
    if (!ConstraintIsEncodingBound(ct)) {
      continue;
    }
    var_to_linear1[ct.linear().vars(0)].push_back(i);
  }

  // Filter out the variables that do not have an interesting encoding.
  absl::erase_if(var_to_linear1, [context](const auto& p) {
    if (p.second.size() > 1) return false;
    return context->VarToConstraints(p.first).size() > 2;
  });

  if (var_to_linear1.empty()) return {};

  absl::flat_hash_map<int, absl::InlinedVector<int, 2>> bool_to_var_encodings;

  // Now we use the linear1 we found to see which bool_or/amo/exactly_one are
  // linking two encodings of the same variable. But first, since some models
  // have a lot of bool_or, we use a simple heuristic to filter out all that are
  // not related to the encodings. We use a bitset to keep track of all boolean
  // potentially encoding a domain for any variable and we filter out all
  // bool_or that are not linked to at least two of these booleans.
  Bitset64<int> booleans_potentially_encoding_domain(
      context->working_model->variables_size());

  for (const auto& [var, linear1_cts] : var_to_linear1) {
    for (const int c : linear1_cts) {
      const ConstraintProto& ct = context->working_model->constraints(c);
      const int bool_var = PositiveRef(ct.enforcement_literal(0));
      booleans_potentially_encoding_domain.Set(bool_var);
      bool_to_var_encodings[bool_var].push_back(var);
    }
  }
  for (auto& [bool_var, var_encodings] : bool_to_var_encodings) {
    // Remove the potential duplicate for the negation.
    gtl::STLSortAndRemoveDuplicates(&var_encodings);
  }
  int new_encoding_or_count = 0;
  for (int i = 0; i < constraint_encoding_or.size(); ++i) {
    const int c = constraint_encoding_or[i];
    const ConstraintProto& ct = context->working_model->constraints(c);
    const BoolArgumentProto& bool_ct =
        ct.constraint_case() == ConstraintProto::kAtMostOne
            ? ct.at_most_one()
            : (ct.constraint_case() == ConstraintProto::kExactlyOne
                   ? ct.exactly_one()
                   : ct.bool_or());
    if (absl::c_count_if(
            bool_ct.literals(),
            [booleans_potentially_encoding_domain](int ref) {
              return booleans_potentially_encoding_domain[PositiveRef(ref)];
            }) < 2) {
      continue;
    }
    constraint_encoding_or[new_encoding_or_count++] = c;
  }
  constraint_encoding_or.resize(new_encoding_or_count);

  // Track the number of times a given boolean appears in the local model for a
  // given variable.
  struct VariableAndBoolInfo {
    // Can only be 1 or 2 (for negation) if properly presolved.
    int linear1_count = 0;
    // Number of times the boolean will appear in
    // `constraints_linking_two_encoding_booleans`.
    int bool_or_count = 0;
  };
  absl::flat_hash_map<std::pair<int, int>, VariableAndBoolInfo> var_bool_counts;

  // Now that we have a potentially smaller set of bool_or, we actually check
  // which of them are linking two encodings of the same variable.
  absl::flat_hash_map<int, std::vector<int>> var_to_constraints_encoding_or;

  // Map from variable to the bools that appear in a given bool_or.
  absl::flat_hash_map<int, std::vector<int>> var_to_bools;

  for (const int c : constraint_encoding_or) {
    var_to_bools.clear();
    const ConstraintProto& ct = context->working_model->constraints(c);
    const BoolArgumentProto& bool_ct =
        ct.constraint_case() == ConstraintProto::kAtMostOne
            ? ct.at_most_one()
            : (ct.constraint_case() == ConstraintProto::kExactlyOne
                   ? ct.exactly_one()
                   : ct.bool_or());
    for (const int ref : bool_ct.literals()) {
      const int bool_var = PositiveRef(ref);
      if (!booleans_potentially_encoding_domain[bool_var]) continue;
      for (const int var : bool_to_var_encodings[bool_var]) {
        var_to_bools[var].push_back(bool_var);
      }
    }
    for (const auto& [var, bools] : var_to_bools) {
      if (bools.size() >= 2) {
        // We have two encodings of `var` in the same constraint `c`. Thus `c`
        // should be part of the local model for `var`.
        var_to_constraints_encoding_or[var].push_back(c);
        for (const int bool_var : bools) {
          var_bool_counts[{var, bool_var}].bool_or_count++;
        }
      }
    }
  }

  std::vector<VariableEncodingLocalModel> local_models;
  // Now that we have all the information, we can create the local models.
  for (const auto& [var, linear1_cts] : var_to_linear1) {
    VariableEncodingLocalModel& encoding_model = local_models.emplace_back();
    encoding_model.var = var;
    encoding_model.linear1_constraints.assign(linear1_cts.begin(),
                                              linear1_cts.end());
    encoding_model.constraints_linking_two_encoding_booleans =
        var_to_constraints_encoding_or[var];
    absl::c_sort(encoding_model.constraints_linking_two_encoding_booleans);
    encoding_model.var_in_more_than_one_constraint_outside_the_local_model =
        (context->VarToConstraints(var).size() - linear1_cts.size() > 1);
    for (const int ct : linear1_cts) {
      const int bool_var = PositiveRef(
          context->working_model->constraints(ct).enforcement_literal(0));
      encoding_model.bools_only_used_inside_the_local_model.insert(bool_var);
      var_bool_counts[{var, bool_var}].linear1_count++;
    }
    absl::erase_if(encoding_model.bools_only_used_inside_the_local_model,
                   [context, v = var, &var_bool_counts](int bool_var) {
                     const auto& counts = var_bool_counts[{v, bool_var}];
                     return context->VarToConstraints(bool_var).size() !=
                            counts.linear1_count + counts.bool_or_count;
                   });
    auto it = context->ObjectiveMap().find(var);
    if (it != context->ObjectiveMap().end()) {
      encoding_model.variable_coeff_in_objective = it->second;
    }
  }
  absl::c_sort(local_models, [](const VariableEncodingLocalModel& a,
                                const VariableEncodingLocalModel& b) {
    return a.var < b.var;
  });
  return local_models;
}

bool BasicPresolveAndGetFullyEncodedDomains(
    PresolveContext* context, VariableEncodingLocalModel& local_model,
    absl::flat_hash_map<int, Domain>* result, bool* changed) {
  *changed = false;
  absl::flat_hash_map<int, int> ref_to_linear1;

  // Fill ref_to_linear1 and do some basic presolving.
  const Domain var_domain = context->DomainOf(local_model.var);
  for (const int ct : local_model.linear1_constraints) {
    ConstraintProto* ct_proto = context->working_model->mutable_constraints(ct);
    DCHECK(ConstraintIsEncodingBound(*ct_proto));
    const int ref = ct_proto->enforcement_literal(0);
    const Domain domain = ReadDomainFromProto(ct_proto->linear());
    if (!domain.OverlapsWith(var_domain)) {
      *changed = true;
      context->UpdateRuleStats(
          "variables: linear1 with domain not included in variable domain");
      if (!context->SetLiteralToFalse(ref)) {
        return false;
      }
      ct_proto->Clear();
      context->UpdateConstraintVariableUsage(ct);
      continue;
    }
    auto [it, inserted] = ref_to_linear1.insert({ref, ct});
    if (!inserted) {
      *changed = true;
      ConstraintProto* old_ct_proto =
          context->working_model->mutable_constraints(it->second);
      const Domain old_ct_domain = ReadDomainFromProto(old_ct_proto->linear());
      const Domain new_domain = domain.IntersectionWith(old_ct_domain);
      ct_proto->Clear();
      context->UpdateConstraintVariableUsage(ct);
      if (new_domain.IsEmpty()) {
        context->UpdateRuleStats(
            "variables: linear1 with same variable and enforcement and "
            "non-overlapping domain, setting enforcement to false");
        if (!context->SetLiteralToFalse(ref)) {
          return false;
        }
        old_ct_proto->Clear();
        context->UpdateConstraintVariableUsage(it->second);
        ref_to_linear1.erase(ref);
      } else {
        FillDomainInProto(new_domain, old_ct_proto->mutable_linear());
        context->UpdateRuleStats(
            "variables: merged two linear1 with same variable and enforcement");
      }
    }
  }

  // Remove from the local model anything that was removed in the loop above.
  int new_linear1_size = 0;
  for (int i = 0; i < local_model.linear1_constraints.size(); ++i) {
    const int ct = local_model.linear1_constraints[i];
    const ConstraintProto& ct_proto = context->working_model->constraints(ct);
    if (ct_proto.constraint_case() != ConstraintProto::kLinear) continue;
    if (context->IsFixed(ct_proto.enforcement_literal(0))) {
      continue;
    }
    DCHECK(ConstraintIsEncodingBound(ct_proto));
    local_model.linear1_constraints[new_linear1_size++] = ct;
  }
  if (new_linear1_size != local_model.linear1_constraints.size()) {
    *changed = true;
    local_model.linear1_constraints.resize(new_linear1_size);
    // Rerun the presolve loop to recompute ref_to_linear1.
    return true;
  }

  for (const auto& [ref, ct] : ref_to_linear1) {
    auto it = ref_to_linear1.find(NegatedRef(ref));
    if (it == ref_to_linear1.end()) continue;
    const ConstraintProto& positive_ct =
        context->working_model->constraints(ct);
    const ConstraintProto& negative_ct =
        context->working_model->constraints(it->second);
    const Domain positive_domain = ReadDomainFromProto(positive_ct.linear());
    const Domain negative_domain = ReadDomainFromProto(negative_ct.linear());
    if (!positive_domain.IntersectionWith(negative_domain).IsEmpty()) {
      // This is not a fully encoded domain. For example, it could be
      //    l => x in {-inf,inf}
      //   ~l => x in {-inf,inf}
      // which actually means that `l` doesn't really encode anything.
      continue;
    }
    bool domain_modified = false;
    if (!context->IntersectDomainWith(
            local_model.var, positive_domain.UnionWith(negative_domain),
            &domain_modified)) {
      return false;
    }
    *changed = *changed || domain_modified;
    result->insert({ref, positive_domain});
    result->insert({NegatedRef(ref), negative_domain});
  }

  // Now detect a different way of fully encoding a domain:
  //   l1 => x in D1
  //   l2 => x in D2
  //   l3 => x in D3
  //   ...
  //   l_n => x in D_n
  //   bool_or(l1, l2, l3, ..., l_n)
  //
  // where D1, D2, ..., D_n are non overlapping. This works too for exactly_one.
  for (const int ct : local_model.constraints_linking_two_encoding_booleans) {
    const ConstraintProto& ct_proto = context->working_model->constraints(ct);
    if (ct_proto.constraint_case() != ConstraintProto::kBoolOr &&
        ct_proto.constraint_case() != ConstraintProto::kExactlyOne) {
      continue;
    }
    if (!ct_proto.enforcement_literal().empty()) continue;
    const BoolArgumentProto& bool_or =
        ct_proto.constraint_case() == ConstraintProto::kExactlyOne
            ? ct_proto.exactly_one()
            : ct_proto.bool_or();
    if (bool_or.literals().size() < 2) continue;
    bool encoding_detected = true;
    Domain non_overlapping_domain;
    std::vector<std::pair<int, Domain>> ref_and_domains;
    for (const int ref : bool_or.literals()) {
      auto it = ref_to_linear1.find(ref);
      if (it == ref_to_linear1.end()) {
        encoding_detected = false;
        break;
      }
      const Domain domain = ReadDomainFromProto(
          context->working_model->constraints(it->second).linear());
      ref_and_domains.push_back({ref, domain});
      if (!non_overlapping_domain.IntersectionWith(domain).IsEmpty()) {
        encoding_detected = false;
        break;
      }
      non_overlapping_domain = non_overlapping_domain.UnionWith(domain);
    }
    if (encoding_detected) {
      context->UpdateRuleStats("variables: detected fully encoded domain");
      bool domain_modified = false;
      if (!context->IntersectDomainWith(local_model.var, non_overlapping_domain,
                                        &domain_modified)) {
        return false;
      }
      if (domain_modified) {
        context->UpdateRuleStats(
            "variables: restricted domain to fully encoded domain");
      }
      *changed = *changed || domain_modified;
      for (const auto& [ref, domain] : ref_and_domains) {
        result->insert({ref, domain});
        result->insert({NegatedRef(ref),
                        var_domain.IntersectionWith(domain.Complement())});
      }
      // Promote a bool_or to an exactly_one.
      if (ct_proto.constraint_case() == ConstraintProto::kBoolOr) {
        context->UpdateRuleStats(
            "variables: promoted bool_or to exactly_one for fully encoded "
            "domain");
        std::vector<int> new_enforcement_literals(bool_or.literals().begin(),
                                                  bool_or.literals().end());
        context->working_model->mutable_constraints(ct)->clear_bool_or();
        context->working_model->mutable_constraints(ct)
            ->mutable_exactly_one()
            ->mutable_literals()
            ->Add(new_enforcement_literals.begin(),
                  new_enforcement_literals.end());
        *changed = true;
      }
    }
  }
  return true;
}

// Return false on unsat
bool DetectEncodedComplexDomain(
    PresolveContext* context, int ct_index,
    VariableEncodingLocalModel& local_model,
    absl::flat_hash_map<int, Domain>* fully_encoded_domains, bool* changed) {
  ConstraintProto* ct = context->working_model->mutable_constraints(ct_index);
  *changed = false;

  if (context->ModelIsUnsat()) return false;
  DCHECK(ct->constraint_case() == ConstraintProto::kAtMostOne ||
         ct->constraint_case() == ConstraintProto::kExactlyOne ||
         ct->constraint_case() == ConstraintProto::kBoolOr);

  // Handling exaclty_one, at_most_one and bool_or is pretty similar. If we have
  // l1 <=> v \in D1
  // l2 <=> v \in D2
  //
  // We built
  //   l <=> v \in (D1 U D2).
  //
  // Moreover, if we have exactly_one(l1, l2, ...) or at_most_one(l1, l2, ...),
  // we know that v cannot be in the intersection of D1 and D2. Thus, we first
  // unconditionally remove (D1 ∩ D2) from the domain of v, making
  // (l1=true and l2=true) impossible and allowing us to write our clauses as
  // exactly_one(l1 or l2, ...) or at_most_one(l1 or l2, ...).
  //
  // Thus, other than the domain reduction that should not be done for the
  // bool_or, all we need is to create a variable
  // (l1 or l2) == l <=> (v \in (D1 U D2)).
  google::protobuf::RepeatedField<int32_t>& literals =
      ct->constraint_case() == ConstraintProto::kAtMostOne
          ? *ct->mutable_at_most_one()->mutable_literals()
          : (ct->constraint_case() == ConstraintProto::kExactlyOne
                 ? *ct->mutable_exactly_one()->mutable_literals()
                 : *ct->mutable_bool_or()->mutable_literals());
  if (literals.size() <= 1) return true;

  if (!ct->enforcement_literal().empty()) {
    // TODO(user): support this case if it any problem needs it.
    return true;
  }

  // When we have
  //     lit => var in D1
  //    ~lit => var in D2
  // we can represent this on a line:
  //
  //             ----------------D1----------------
  //  ----------------D2---------------
  // |+++++++++++|*********************|++++++++++|
  //   lit=false    lit unconstrained     lit=true
  //
  // Handling the case where the variable is unconstrained by the lit is a
  // bit of a pain: we want to replace two literals in a exactly_one by a
  // single one, and if they are both unconstrained we might be forced to pick
  // one arbitrarily to set to true. In any case, this is not a proper
  // encoding of a complex domain, so we just ignore it.
  // TODO(user): This can be implemented if it turns out to be common.

  // The solver does not handle very well linear1 with complex domains. So, when
  // we look at two encodings to merge, we will only consider the pair if the
  // new domain that will replace both is not more complex than any of the
  // original domains.
  // TODO(user): if those linear1 are that bad, we should consider expanding
  // them in the end of the presolve instead of avoiding them.
  std::vector<std::pair<int, Domain>> candidates;
  candidates.reserve(literals.size());
  for (int i = 0; i < literals.size(); ++i) {
    const int lit_var = literals.Get(i);
    if (!local_model.bools_only_used_inside_the_local_model.contains(
            PositiveRef(lit_var))) {
      continue;
    }
    auto it = fully_encoded_domains->find(lit_var);
    if (it == fully_encoded_domains->end()) {
      continue;
    }
    candidates.push_back({lit_var, it->second});
  }

  const Domain var_domain = context->DomainOf(local_model.var);
  Domain domain_new_var_false;
  Domain domain_new_var_true;
  int lit1;
  int lit2;
  Domain domain_lit1;
  Domain domain_lit2;
  bool found_pair = false;
  for (int i = 0; i < candidates.size(); ++i) {
    for (int j = i + 1; j < candidates.size(); ++j) {
      const auto& [lit_var1, domain_var1] = candidates[i];
      const auto& [lit_var2, domain_var2] = candidates[j];
      domain_new_var_true = domain_var1.UnionWith(domain_var2)
                                .SimplifyUsingImpliedDomain(var_domain);
      domain_new_var_false =
          domain_new_var_true.Complement().SimplifyUsingImpliedDomain(
              var_domain);
      const int initial_complexity =
          std::max(domain_var1.NumIntervals(), domain_var2.NumIntervals());
      if (domain_new_var_true.NumIntervals() <= initial_complexity &&
          domain_new_var_false.NumIntervals() <= initial_complexity) {
        lit1 = lit_var1;
        domain_lit1 = domain_var1;
        lit2 = lit_var2;
        domain_lit2 = domain_var2;
        found_pair = true;
        break;
      }
    }
    if (found_pair) break;
  }

  if (!found_pair) return true;

  // We found two literals that each fully encodes an interval and are both only
  // used in the encoding and in the bool_or/exactly_one/at_most_one. We can
  // thus replace the two literals by their OR. Since this code is already
  // rather complex, so we will just simplify a pair of literals at a time, and
  // leave for the presolve fixpoint to do the rest.
  *changed = true;

  context->UpdateRuleStats(
      "variables: detected encoding of a complex domain with multiple "
      "linear1");

  if (ct->constraint_case() != ConstraintProto::kBoolOr) {
    // In virtue of the AMO, var must not be in the intersection of the two
    // domains where both literals are true.
    if (!context->IntersectDomainWith(
            local_model.var,
            domain_lit2.IntersectionWith(domain_lit1).Complement())) {
      return false;
    }
  }

  // Now we want to build a lit3 = (lit1 or lit2) to use in the AMO/bool_or.
  const int new_var = context->NewBoolVarWithClause({lit1, lit2});

  if (domain_new_var_true.IsEmpty()) {
    CHECK(context->SetLiteralToFalse(new_var));
  } else if (domain_new_var_false.IsEmpty()) {
    CHECK(context->SetLiteralToTrue(new_var));
  } else {
    local_model.linear1_constraints.push_back(
        context->working_model->constraints_size());
    ConstraintProto* new_ct = context->working_model->add_constraints();
    new_ct->add_enforcement_literal(new_var);
    new_ct->mutable_linear()->add_vars(local_model.var);
    new_ct->mutable_linear()->add_coeffs(1);
    FillDomainInProto(domain_new_var_true, new_ct->mutable_linear());
    local_model.linear1_constraints.push_back(
        context->working_model->constraints_size());
    local_model.bools_only_used_inside_the_local_model.insert(
        PositiveRef(new_var));
    new_ct = context->working_model->add_constraints();
    new_ct->add_enforcement_literal(NegatedRef(new_var));
    new_ct->mutable_linear()->add_vars(local_model.var);
    new_ct->mutable_linear()->add_coeffs(1);
    FillDomainInProto(domain_new_var_false, new_ct->mutable_linear());
    context->UpdateNewConstraintsVariableUsage();
  }

  // Remove the two literals from the AMO.
  int new_size = 0;
  for (int i = 0; i < literals.size(); ++i) {
    if (literals.Get(i) != lit1 && literals.Get(i) != lit2) {
      literals.Set(new_size++, literals.Get(i));
    }
  }
  literals.Truncate(new_size);
  literals.Add(new_var);
  context->UpdateConstraintVariableUsage(ct_index);

  // Finally, move the four linear1 to the mapping model.
  fully_encoded_domains->insert({new_var, domain_new_var_true});
  fully_encoded_domains->insert({NegatedRef(new_var), domain_new_var_false});
  fully_encoded_domains->erase(lit1);
  fully_encoded_domains->erase(lit2);
  fully_encoded_domains->erase(NegatedRef(lit1));
  fully_encoded_domains->erase(NegatedRef(lit2));
  context->MarkVariableAsRemoved(PositiveRef(lit1));
  context->MarkVariableAsRemoved(PositiveRef(lit2));
  int new_linear1_size = 0;
  for (int i = 0; i < local_model.linear1_constraints.size(); ++i) {
    const int ct = local_model.linear1_constraints[i];
    ConstraintProto* ct_proto = context->working_model->mutable_constraints(ct);
    if (PositiveRef(ct_proto->enforcement_literal(0)) == PositiveRef(lit1) ||
        PositiveRef(ct_proto->enforcement_literal(0)) == PositiveRef(lit2)) {
      context->NewMappingConstraint(*ct_proto, __FILE__, __LINE__);
      ct_proto->Clear();
      context->UpdateConstraintVariableUsage(ct);
      continue;
    }
    local_model.linear1_constraints[new_linear1_size++] = ct;
  }
  local_model.linear1_constraints.resize(new_linear1_size);

  return true;
}

bool DetectAllEncodedComplexDomain(PresolveContext* context,
                                   VariableEncodingLocalModel& local_model) {
  absl::flat_hash_map<int, Domain> fully_encoded_domains;
  bool changed_on_basic_presolve = false;
  if (!BasicPresolveAndGetFullyEncodedDomains(context, local_model,
                                              &fully_encoded_domains,
                                              &changed_on_basic_presolve)) {
    return false;
  }
  if (local_model.constraints_linking_two_encoding_booleans.size() != 1) {
    return true;
  }
  const int ct = local_model.constraints_linking_two_encoding_booleans[0];
  bool changed = true;
  while (changed) {
    if (!DetectEncodedComplexDomain(context, ct, local_model,
                                    &fully_encoded_domains, &changed)) {
      return false;
    }
  }
  return true;
}

bool MaybeTransferLinear1ToAnotherVariable(
    VariableEncodingLocalModel& local_model, PresolveContext* context) {
  if (local_model.var == -1) return true;
  if (local_model.variable_coeff_in_objective != 0) return true;
  if (local_model.single_constraint_using_the_var_outside_the_local_model ==
      -1) {
    return true;
  }
  const int other_c =
      local_model.single_constraint_using_the_var_outside_the_local_model;

  const std::vector<int>& to_rewrite = local_model.linear1_constraints;

  // In general constraint with more than two variable can't be removed.
  // Similarly for linear2 with non-fixed rhs as we would need to check the form
  // of all implied domain.
  const auto& other_ct = context->working_model->constraints(other_c);
  if (context->ConstraintToVars(other_c).size() != 2 ||
      !other_ct.enforcement_literal().empty() ||
      other_ct.constraint_case() == ConstraintProto::kLinear) {
    return true;
  }

  // This will be the rewriting function. It takes the implied domain of var
  // from linear1, and return a pair {new_var, new_var_implied_domain}.
  std::function<std::pair<int, Domain>(const Domain& implied)> transfer_f =
      nullptr;

  const int var = local_model.var;
  // We only support a few cases.
  //
  // TODO(user): implement more! Note that the linear2 case was tempting, but if
  // we don't have an equality, we can't transfer, and if we do, we actually
  // have affine equivalence already.
  if (other_ct.constraint_case() == ConstraintProto::kLinMax &&
      other_ct.lin_max().target().vars().size() == 1 &&
      other_ct.lin_max().target().vars(0) == var &&
      std::abs(other_ct.lin_max().target().coeffs(0)) == 1 &&
      IsAffineIntAbs(other_ct)) {
    context->UpdateRuleStats("linear1: transferred from abs(X) to X");
    const LinearExpressionProto& target = other_ct.lin_max().target();
    const LinearExpressionProto& expr = other_ct.lin_max().exprs(0);
    transfer_f = [target = target, expr = expr](const Domain& implied) {
      Domain target_domain =
          implied.ContinuousMultiplicationBy(target.coeffs(0))
              .AdditionWith(Domain(target.offset()));
      target_domain = target_domain.IntersectionWith(
          Domain(0, std::numeric_limits<int64_t>::max()));

      // We have target = abs(expr).
      const Domain expr_domain =
          target_domain.UnionWith(target_domain.Negation());
      const Domain new_domain = expr_domain.AdditionWith(Domain(-expr.offset()))
                                    .InverseMultiplicationBy(expr.coeffs(0));
      return std::make_pair(expr.vars(0), new_domain);
    };
  }

  if (transfer_f == nullptr) {
    context->UpdateRuleStats(
        "TODO linear1: appear in only one extra 2-var constraint");
    return true;
  }

  // Applies transfer_f to all linear1.
  const Domain var_domain = context->DomainOf(var);
  for (const int c : to_rewrite) {
    ConstraintProto* ct = context->working_model->mutable_constraints(c);
    if (ct->linear().vars(0) != var || ct->linear().coeffs(0) != 1) {
      // This shouldn't happen.
      LOG(INFO) << "Aborted in MaybeTransferLinear1ToAnotherVariable()";
      return true;
    }

    const Domain implied =
        var_domain.IntersectionWith(ReadDomainFromProto(ct->linear()));
    auto [new_var, new_domain] = transfer_f(implied);
    const Domain current = context->DomainOf(new_var);
    new_domain = new_domain.IntersectionWith(current);
    if (new_domain.IsEmpty()) {
      if (!context->MarkConstraintAsFalse(ct, "linear1: unsat transfer")) {
        return false;
      }
    } else if (new_domain == current) {
      // Note that we don't need to remove this constraint from
      // local_model.linear1_constraints since we will set
      // local_model.var = -1 below.
      ct->Clear();
    } else {
      ct->mutable_linear()->set_vars(0, new_var);
      FillDomainInProto(new_domain, ct->mutable_linear());
    }
    context->UpdateConstraintVariableUsage(c);
  }

  // Copy other_ct to the mapping model and delete var!
  context->NewMappingConstraint(other_ct, __FILE__, __LINE__);
  context->working_model->mutable_constraints(other_c)->Clear();
  context->UpdateConstraintVariableUsage(other_c);
  context->MarkVariableAsRemoved(var);
  local_model.var = -1;
  return true;
}
}  // namespace sat
}  // namespace operations_research
