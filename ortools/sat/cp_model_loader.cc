// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/cp_model_loader.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/vlog_is_on.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/diffn.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/symmetry.h"
#include "ortools/sat/table.h"
#include "ortools/sat/timetable.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

namespace {

template <typename Values>
std::vector<int64_t> ValuesFromProto(const Values& values) {
  return std::vector<int64_t>(values.begin(), values.end());
}

void ComputeLinearBounds(const LinearConstraintProto& proto,
                         CpModelMapping* mapping, IntegerTrail* integer_trail,
                         int64_t* sum_min, int64_t* sum_max) {
  *sum_min = 0;
  *sum_max = 0;

  for (int i = 0; i < proto.vars_size(); ++i) {
    const int64_t coeff = proto.coeffs(i);
    const IntegerVariable var = mapping->Integer(proto.vars(i));
    const int64_t lb = integer_trail->LowerBound(var).value();
    const int64_t ub = integer_trail->UpperBound(var).value();
    if (coeff >= 0) {
      (*sum_min) += coeff * lb;
      (*sum_max) += coeff * ub;
    } else {
      (*sum_min) += coeff * ub;
      (*sum_max) += coeff * lb;
    }
  }
}

// We check if the constraint is a sum(ax * xi) == value.
bool ConstraintIsEq(const LinearConstraintProto& proto) {
  return proto.domain_size() == 2 && proto.domain(0) == proto.domain(1);
}

// We check if the constraint is a sum(ax * xi) != value.
bool ConstraintIsNEq(const LinearConstraintProto& proto,
                     CpModelMapping* mapping, IntegerTrail* integer_trail,
                     int64_t* single_value) {
  int64_t sum_min = 0;
  int64_t sum_max = 0;
  ComputeLinearBounds(proto, mapping, integer_trail, &sum_min, &sum_max);

  const Domain complement =
      Domain(sum_min, sum_max)
          .IntersectionWith(ReadDomainFromProto(proto).Complement());
  if (complement.IsEmpty()) return false;
  const int64_t value = complement.Min();

  if (complement.Size() == 1) {
    if (single_value != nullptr) {
      *single_value = value;
    }
    return true;
  }
  return false;
}

}  // namespace

void LoadVariables(const CpModelProto& model_proto,
                   bool view_all_booleans_as_integers, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const int num_proto_variables = model_proto.variables_size();

  // All [0, 1] variables always have a corresponding Boolean, even if it is
  // fixed to 0 (domain == [0,0]) or fixed to 1 (domain == [1,1]).
  {
    auto* sat_solver = m->GetOrCreate<SatSolver>();
    CHECK_EQ(sat_solver->NumVariables(), 0);

    BooleanVariable new_var(0);
    std::vector<BooleanVariable> false_variables;
    std::vector<BooleanVariable> true_variables;

    mapping->booleans_.resize(num_proto_variables, kNoBooleanVariable);
    mapping->reverse_boolean_map_.resize(num_proto_variables, -1);
    for (int i = 0; i < num_proto_variables; ++i) {
      const auto& domain = model_proto.variables(i).domain();
      if (domain.size() != 2) continue;
      if (domain[0] >= 0 && domain[1] <= 1) {
        mapping->booleans_[i] = new_var;
        mapping->reverse_boolean_map_[new_var] = i;
        if (domain[1] == 0) {
          false_variables.push_back(new_var);
        } else if (domain[0] == 1) {
          true_variables.push_back(new_var);
        }
        ++new_var;
      }
    }

    sat_solver->SetNumVariables(new_var.value());
    for (const BooleanVariable var : true_variables) {
      m->Add(ClauseConstraint({sat::Literal(var, true)}));
    }
    for (const BooleanVariable var : false_variables) {
      m->Add(ClauseConstraint({sat::Literal(var, false)}));
    }
  }

  // Compute the list of positive variable reference for which we need to
  // create an IntegerVariable.
  std::vector<int> var_to_instantiate_as_integer;
  if (view_all_booleans_as_integers) {
    var_to_instantiate_as_integer.resize(num_proto_variables);
    for (int i = 0; i < num_proto_variables; ++i) {
      var_to_instantiate_as_integer[i] = i;
    }
  } else {
    // Compute the integer variable references used by the model.
    absl::flat_hash_set<int> used_variables;

    IndexReferences refs;
    for (int c = 0; c < model_proto.constraints_size(); ++c) {
      const ConstraintProto& ct = model_proto.constraints(c);
      refs = GetReferencesUsedByConstraint(ct);
      for (const int ref : refs.variables) {
        used_variables.insert(PositiveRef(ref));
      }
    }

    // Add the objectives variables that needs to be referenceable as integer
    // even if they are only used as Booleans.
    if (model_proto.has_objective()) {
      for (const int obj_var : model_proto.objective().vars()) {
        used_variables.insert(PositiveRef(obj_var));
      }
    }

    // Make sure any unused variable, that is not already a Boolean is
    // considered "used".
    for (int i = 0; i < num_proto_variables; ++i) {
      if (mapping->booleans_[i] == kNoBooleanVariable) {
        used_variables.insert(i);
      }
    }

    // We want the variable in the problem order.
    var_to_instantiate_as_integer.assign(used_variables.begin(),
                                         used_variables.end());
    gtl::STLSortAndRemoveDuplicates(&var_to_instantiate_as_integer);
  }
  mapping->integers_.resize(num_proto_variables, kNoIntegerVariable);

  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  integer_trail->ReserveSpaceForNumVariables(
      var_to_instantiate_as_integer.size());
  mapping->reverse_integer_map_.resize(2 * var_to_instantiate_as_integer.size(),
                                       -1);
  for (const int i : var_to_instantiate_as_integer) {
    const auto& var_proto = model_proto.variables(i);
    mapping->integers_[i] =
        integer_trail->AddIntegerVariable(ReadDomainFromProto(var_proto));
    DCHECK_LT(mapping->integers_[i], mapping->reverse_integer_map_.size());
    mapping->reverse_integer_map_[mapping->integers_[i]] = i;
  }

  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  auto* intervals_repository = m->GetOrCreate<IntervalsRepository>();

  // Link any variable that has both views.
  for (int i = 0; i < num_proto_variables; ++i) {
    if (mapping->integers_[i] == kNoIntegerVariable) continue;
    if (mapping->booleans_[i] == kNoBooleanVariable) continue;

    // Associate with corresponding integer variable.
    encoder->AssociateToIntegerEqualValue(
        sat::Literal(mapping->booleans_[i], true), mapping->integers_[i],
        IntegerValue(1));
  }

  // Create the interval variables.
  mapping->intervals_.resize(model_proto.constraints_size(),
                             kNoIntervalVariable);
  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kInterval) {
      continue;
    }
    if (HasEnforcementLiteral(ct)) {
      const sat::Literal enforcement_literal =
          mapping->Literal(ct.enforcement_literal(0));
      // TODO(user): Fix the constant variable situation. An optional interval
      // with constant start/end or size cannot share the same constant
      // variable if it is used in non-optional situation.
      mapping->intervals_[c] = intervals_repository->CreateInterval(
          mapping->Affine(ct.interval().start()),
          mapping->Affine(ct.interval().end()),
          mapping->Affine(ct.interval().size()), enforcement_literal.Index(),
          /*add_linear_relation=*/false);
    } else {
      mapping->intervals_[c] = intervals_repository->CreateInterval(
          mapping->Affine(ct.interval().start()),
          mapping->Affine(ct.interval().end()),
          mapping->Affine(ct.interval().size()), kNoLiteralIndex,
          /*add_linear_relation=*/false);
    }
    mapping->already_loaded_ct_.insert(&ct);
  }
}

void LoadBooleanSymmetries(const CpModelProto& model_proto, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const SymmetryProto& symmetry = model_proto.symmetry();
  if (symmetry.permutations().empty()) return;

  // We currently can only use symmetry that touch a subset of variables.
  const int num_vars = model_proto.variables().size();
  std::vector<bool> can_be_used_in_symmetry(num_vars, true);

  // First, we currently only support loading symmetry between Booleans.
  for (int v = 0; v < num_vars; ++v) {
    if (!mapping->IsBoolean(v)) can_be_used_in_symmetry[v] = false;
  }

  // Tricky: Moreover, some constraint will causes extra Boolean to be created
  // and linked with the Boolean in the constraints. We can't use any of the
  // symmetry that touch these since we potentially miss the component that will
  // map these extra Booleans between each other.
  //
  // TODO(user): We could add these extra Boolean during expansion/presolve so
  // that we have the symmetry involing them. Or maybe comes up with a different
  // solution.
  const int num_constraints = model_proto.constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    if (ct.linear().domain().size() <= 2) continue;

    // A linear with a complex domain might need extra Booleans to be loaded.
    // Note that it should be fine for the Boolean(s) in enforcement_literal
    // though.
    for (const int ref : ct.linear().vars()) {
      can_be_used_in_symmetry[PositiveRef(ref)] = false;
    }
  }

  auto* sat_solver = m->GetOrCreate<SatSolver>();
  auto* symmetry_handler = m->GetOrCreate<SymmetryPropagator>();
  sat_solver->AddPropagator(symmetry_handler);
  const int num_literals = 2 * sat_solver->NumVariables();

  for (const SparsePermutationProto& perm : symmetry.permutations()) {
    bool can_be_used = true;
    for (const int var : perm.support()) {
      if (!can_be_used_in_symmetry[var]) {
        can_be_used = false;
        break;
      }
    }
    if (!can_be_used) continue;

    // Convert the variable symmetry to a "literal" one.
    auto literal_permutation =
        absl::make_unique<SparsePermutation>(num_literals);
    int support_index = 0;
    const int num_cycle = perm.cycle_sizes().size();
    for (int i = 0; i < num_cycle; ++i) {
      const int size = perm.cycle_sizes(i);
      const int saved_support_index = support_index;
      for (int j = 0; j < size; ++j) {
        const int var = perm.support(support_index++);
        literal_permutation->AddToCurrentCycle(
            mapping->Literal(var).Index().value());
      }
      literal_permutation->CloseCurrentCycle();

      // Note that we also need to add the corresponding cycle for the negated
      // literals.
      support_index = saved_support_index;
      for (int j = 0; j < size; ++j) {
        const int var = perm.support(support_index++);
        literal_permutation->AddToCurrentCycle(
            mapping->Literal(var).NegatedIndex().value());
      }
      literal_permutation->CloseCurrentCycle();
    }
    symmetry_handler->AddSymmetry(std::move(literal_permutation));
  }

  SOLVER_LOG(m->GetOrCreate<SolverLogger>(), "Added ",
             symmetry_handler->num_permutations(),
             " symmetry to the SAT solver.");
}

// The logic assumes that the linear constraints have been presolved, so that
// equality with a domain bound have been converted to <= or >= and so that we
// never have any trivial inequalities.
//
// TODO(user): Regroup/presolve two encoding like b => x > 2 and the same
// Boolean b => x > 5. These shouldn't happen if we merge linear constraints.
void ExtractEncoding(const CpModelProto& model_proto, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  auto* sat_solver = m->GetOrCreate<SatSolver>();

  // TODO(user): Debug what makes it unsat at this point.
  if (sat_solver->IsModelUnsat()) return;

  // Detection of literal equivalent to (i_var == value). We collect all the
  // half-reified constraint lit => equality or lit => inequality for a given
  // variable, and we will later sort them to detect equivalence.
  struct EqualityDetectionHelper {
    const ConstraintProto* ct;
    sat::Literal literal;
    int64_t value;
    bool is_equality;  // false if != instead.

    bool operator<(const EqualityDetectionHelper& o) const {
      if (literal.Variable() == o.literal.Variable()) {
        if (value == o.value) return is_equality && !o.is_equality;
        return value < o.value;
      }
      return literal.Variable() < o.literal.Variable();
    }
  };
  std::vector<std::vector<EqualityDetectionHelper>> var_to_equalities(
      model_proto.variables_size());

  // TODO(user): We will re-add the same implied bounds during probing, so
  // it might not be necessary to do that here. Also, it might be too early
  // if some of the literal view used in the LP are created later, but that
  // should be fixable via calls to implied_bounds->NotifyNewIntegerView().
  auto* implied_bounds = m->GetOrCreate<ImpliedBounds>();

  // Detection of literal equivalent to (i_var >= bound). We also collect
  // all the half-refied part and we will sort the vector for detection of the
  // equivalence.
  struct InequalityDetectionHelper {
    const ConstraintProto* ct;
    sat::Literal literal;
    IntegerLiteral i_lit;

    bool operator<(const InequalityDetectionHelper& o) const {
      if (literal.Variable() == o.literal.Variable()) {
        return i_lit.var < o.i_lit.var;
      }
      return literal.Variable() < o.literal.Variable();
    }
  };
  std::vector<InequalityDetectionHelper> inequalities;

  // Loop over all constraints and fill var_to_equalities and inequalities.
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
      continue;
    }
    if (ct.enforcement_literal().size() != 1) continue;
    if (ct.linear().vars_size() != 1) continue;

    // ct is a linear constraint with one term and one enforcement literal.
    const sat::Literal enforcement_literal =
        mapping->Literal(ct.enforcement_literal(0));
    if (sat_solver->Assignment().LiteralIsFalse(enforcement_literal)) continue;

    const int ref = ct.linear().vars(0);
    const int var = PositiveRef(ref);

    const Domain domain = ReadDomainFromProto(model_proto.variables(var));
    const Domain domain_if_enforced =
        ReadDomainFromProto(ct.linear())
            .InverseMultiplicationBy(ct.linear().coeffs(0) *
                                     (RefIsPositive(ref) ? 1 : -1));

    if (domain_if_enforced.IsEmpty()) {
      if (!sat_solver->AddUnitClause(enforcement_literal.Negated())) return;
      continue;
    }

    // Detect enforcement_literal => (var >= value or var <= value).
    if (domain_if_enforced.NumIntervals() == 1) {
      if (domain_if_enforced.Max() >= domain.Max() &&
          domain_if_enforced.Min() > domain.Min()) {
        inequalities.push_back({&ct, enforcement_literal,
                                IntegerLiteral::GreaterOrEqual(
                                    mapping->Integer(var),
                                    IntegerValue(domain_if_enforced.Min()))});
      } else if (domain_if_enforced.Min() <= domain.Min() &&
                 domain_if_enforced.Max() < domain.Max()) {
        inequalities.push_back({&ct, enforcement_literal,
                                IntegerLiteral::LowerOrEqual(
                                    mapping->Integer(var),
                                    IntegerValue(domain_if_enforced.Max()))});
      }
    }

    // Detect implied bounds. The test is less strict than the above
    // test.
    if (domain_if_enforced.Min() > domain.Min()) {
      implied_bounds->Add(
          enforcement_literal,
          IntegerLiteral::GreaterOrEqual(
              mapping->Integer(var), IntegerValue(domain_if_enforced.Min())));
    }
    if (domain_if_enforced.Max() < domain.Max()) {
      implied_bounds->Add(
          enforcement_literal,
          IntegerLiteral::LowerOrEqual(mapping->Integer(var),
                                       IntegerValue(domain_if_enforced.Max())));
    }

    // Detect enforcement_literal => (var == value or var != value).
    //
    // Note that for domain with 2 values like [0, 1], we will detect both ==
    // 0 and != 1. Similarly, for a domain in [min, max], we should both
    // detect (== min) and (<= min), and both detect (== max) and (>= max).
    {
      const Domain inter = domain.IntersectionWith(domain_if_enforced);
      if (!inter.IsEmpty() && inter.Min() == inter.Max()) {
        var_to_equalities[var].push_back(
            {&ct, enforcement_literal, inter.Min(), true});
        if (domain.Contains(inter.Min())) {
          mapping->variables_to_encoded_values_[var].insert(inter.Min());
        }
      }
    }
    {
      const Domain inter =
          domain.IntersectionWith(domain_if_enforced.Complement());
      if (!inter.IsEmpty() && inter.Min() == inter.Max()) {
        var_to_equalities[var].push_back(
            {&ct, enforcement_literal, inter.Min(), false});
        if (domain.Contains(inter.Min())) {
          mapping->variables_to_encoded_values_[var].insert(inter.Min());
        }
      }
    }
  }

  // Detect Literal <=> X >= value
  int num_inequalities = 0;
  std::sort(inequalities.begin(), inequalities.end());
  for (int i = 0; i + 1 < inequalities.size(); i++) {
    if (inequalities[i].literal != inequalities[i + 1].literal.Negated()) {
      continue;
    }

    // TODO(user): In these cases, we could fix the enforcement literal right
    // away or ignore the constraint. Note that it will be done later anyway
    // though.
    if (integer_trail->IntegerLiteralIsTrue(inequalities[i].i_lit) ||
        integer_trail->IntegerLiteralIsFalse(inequalities[i].i_lit)) {
      continue;
    }
    if (integer_trail->IntegerLiteralIsTrue(inequalities[i + 1].i_lit) ||
        integer_trail->IntegerLiteralIsFalse(inequalities[i + 1].i_lit)) {
      continue;
    }

    const auto pair_a = encoder->Canonicalize(inequalities[i].i_lit);
    const auto pair_b = encoder->Canonicalize(inequalities[i + 1].i_lit);
    if (pair_a.first == pair_b.second) {
      ++num_inequalities;
      encoder->AssociateToIntegerLiteral(inequalities[i].literal,
                                         inequalities[i].i_lit);
      mapping->already_loaded_ct_.insert(inequalities[i].ct);
      mapping->already_loaded_ct_.insert(inequalities[i + 1].ct);
    }
  }

  // Encode the half-inequalities.
  int num_half_inequalities = 0;
  for (const auto inequality : inequalities) {
    if (mapping->ConstraintIsAlreadyLoaded(inequality.ct)) continue;
    m->Add(
        Implication(inequality.literal,
                    encoder->GetOrCreateAssociatedLiteral(inequality.i_lit)));
    if (sat_solver->IsModelUnsat()) return;

    ++num_half_inequalities;
    mapping->already_loaded_ct_.insert(inequality.ct);
    mapping->is_half_encoding_ct_.insert(inequality.ct);
  }

  if (!inequalities.empty()) {
    VLOG(1) << num_inequalities << " literals associated to VAR >= value, and "
            << num_half_inequalities << " half-associations.";
  }

  // Detect Literal <=> X == value and associate them in the IntegerEncoder.
  //
  // TODO(user): Fully encode variable that are almost fully encoded?
  int num_constraints = 0;
  int num_equalities = 0;
  int num_half_equalities = 0;
  int num_fully_encoded = 0;
  int num_partially_encoded = 0;
  for (int i = 0; i < var_to_equalities.size(); ++i) {
    std::vector<EqualityDetectionHelper>& encoding = var_to_equalities[i];
    std::sort(encoding.begin(), encoding.end());
    if (encoding.empty()) continue;
    num_constraints += encoding.size();

    absl::flat_hash_set<int64_t> values;
    for (int j = 0; j + 1 < encoding.size(); j++) {
      if ((encoding[j].value != encoding[j + 1].value) ||
          (encoding[j].literal != encoding[j + 1].literal.Negated()) ||
          (encoding[j].is_equality != true) ||
          (encoding[j + 1].is_equality != false)) {
        continue;
      }

      ++num_equalities;
      encoder->AssociateToIntegerEqualValue(encoding[j].literal,
                                            mapping->integers_[i],
                                            IntegerValue(encoding[j].value));
      mapping->already_loaded_ct_.insert(encoding[j].ct);
      mapping->already_loaded_ct_.insert(encoding[j + 1].ct);
      values.insert(encoding[j].value);
    }

    // TODO(user): Try to remove it. Normally we caught UNSAT above, but
    // tests are very flaky (it only happens in parallel). Keeping it there for
    // the time being.
    if (sat_solver->IsModelUnsat()) return;

    // Encode the half-equalities.
    //
    // TODO(user): delay this after PropagateEncodingFromEquivalenceRelations()?
    // Otherwise we might create new Boolean variables for no reason. Note
    // however, that in the presolve, we should only use the "representative" in
    // linear constraints, so we should be fine.
    for (const auto equality : encoding) {
      if (mapping->ConstraintIsAlreadyLoaded(equality.ct)) continue;
      const class Literal eq = encoder->GetOrCreateLiteralAssociatedToEquality(
          mapping->integers_[i], IntegerValue(equality.value));
      if (equality.is_equality) {
        m->Add(Implication(equality.literal, eq));
      } else {
        m->Add(Implication(equality.literal, eq.Negated()));
      }

      ++num_half_equalities;
      mapping->already_loaded_ct_.insert(equality.ct);
      mapping->is_half_encoding_ct_.insert(equality.ct);
    }

    // Update stats.
    if (VLOG_IS_ON(1)) {
      if (encoder->VariableIsFullyEncoded(mapping->integers_[i])) {
        ++num_fully_encoded;
      } else {
        ++num_partially_encoded;
      }
    }
  }

  if (num_constraints > 0) {
    VLOG(1) << num_equalities << " literals associated to VAR == value, and "
            << num_half_equalities << " half-associations.";
  }
  if (num_fully_encoded > 0) {
    VLOG(1) << "num_fully_encoded_variables: " << num_fully_encoded;
  }
  if (num_partially_encoded > 0) {
    VLOG(1) << "num_partially_encoded_variables: " << num_partially_encoded;
  }
}

void ExtractElementEncoding(const CpModelProto& model_proto, Model* m) {
  int num_element_encoded = 0;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* implied_bounds = m->GetOrCreate<ImpliedBounds>();

  // Scan all exactly_one constraints and look for literal => var == value to
  // detect element encodings.
  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);

    if (ct.constraint_case() != ConstraintProto::kExactlyOne) continue;

    // Project the implied values onto each integer variable.
    absl::flat_hash_map<IntegerVariable, std::vector<ValueLiteralPair>>
        var_to_value_literal_list;
    for (const int l : ct.exactly_one().literals()) {
      const Literal literal = mapping->Literal(l);
      for (const auto& var_value : implied_bounds->GetImpliedValues(literal)) {
        var_to_value_literal_list[var_value.first].push_back(
            {var_value.second, literal});
      }
    }

    // VLOG info.
    std::vector<IntegerVariable> encoded_variables;
    std::string encoded_variables_str;

    // Search for variable fully covered by the literals of the exactly_one.
    for (const auto& [var, literal_value_list] : var_to_value_literal_list) {
      if (literal_value_list.size() < ct.exactly_one().literals_size()) {
        VLOG(2) << "X" << var.value() << " has " << literal_value_list.size()
                << " implied values, and a domain of size "
                << m->GetOrCreate<IntegerTrail>()
                       ->InitialVariableDomain(var)
                       .Size();
        continue;
      }

      // We use the order of literals of the exactly_one.
      implied_bounds->AddElementEncoding(var, literal_value_list, c);
      if (VLOG_IS_ON(1)) {
        encoded_variables.push_back(var);
        absl::StrAppend(&encoded_variables_str, " X", var.value());
        num_element_encoded++;
      }
    }
    if (encoded_variables.size() > 1 && VLOG_IS_ON(1)) {
      VLOG(1) << "exactly_one(" << c << ") encodes " << encoded_variables.size()
              << " variables at the same time: " << encoded_variables_str;
    }
  }

  if (num_element_encoded > 0) {
    VLOG(1) << "num_element_encoded: " << num_element_encoded;
  }
}

void PropagateEncodingFromEquivalenceRelations(const CpModelProto& model_proto,
                                               Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  auto* sat_solver = m->GetOrCreate<SatSolver>();

  // Loop over all constraints and find affine ones.
  int64_t num_associations = 0;
  int64_t num_set_to_false = 0;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (!ct.enforcement_literal().empty()) continue;
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    if (ct.linear().vars_size() != 2) continue;
    if (!ConstraintIsEq(ct.linear())) continue;

    const IntegerValue rhs(ct.linear().domain(0));

    // Make sure the coefficient are positive.
    IntegerVariable var1 = mapping->Integer(ct.linear().vars(0));
    IntegerVariable var2 = mapping->Integer(ct.linear().vars(1));
    IntegerValue coeff1(ct.linear().coeffs(0));
    IntegerValue coeff2(ct.linear().coeffs(1));
    if (coeff1 < 0) {
      var1 = NegationOf(var1);
      coeff1 = -coeff1;
    }
    if (coeff2 < 0) {
      var2 = NegationOf(var2);
      coeff2 = -coeff2;
    }

    // TODO(user): This is not supposed to happen, but apparently it did on
    // once on routing_GCM_0001_sat.fzn. Investigate and fix.
    if (coeff1 == 0 || coeff2 == 0) continue;

    // We first map the >= literals.
    // It is important to do that first, since otherwise mapping a == literal
    // might creates the underlying >= and <= literals.
    for (int i = 0; i < 2; ++i) {
      for (const auto value_literal :
           encoder->PartialGreaterThanEncoding(var1)) {
        const IntegerValue value1 = value_literal.first;
        const IntegerValue bound2 = FloorRatio(rhs - value1 * coeff1, coeff2);
        ++num_associations;
        encoder->AssociateToIntegerLiteral(
            value_literal.second, IntegerLiteral::LowerOrEqual(var2, bound2));
      }
      std::swap(var1, var2);
      std::swap(coeff1, coeff2);
    }

    // Same for the == literals.
    //
    // TODO(user): This is similar to LoadEquivalenceAC() for unreified
    // constraints, but when the later is called, more encoding might have taken
    // place.
    for (int i = 0; i < 2; ++i) {
      for (const auto value_literal : encoder->PartialDomainEncoding(var1)) {
        const IntegerValue value1 = value_literal.value;
        const IntegerValue intermediate = rhs - value1 * coeff1;
        if (intermediate % coeff2 != 0) {
          // Using this function deals properly with UNSAT.
          ++num_set_to_false;
          sat_solver->AddUnitClause(value_literal.literal.Negated());
          continue;
        }
        ++num_associations;
        encoder->AssociateToIntegerEqualValue(value_literal.literal, var2,
                                              intermediate / coeff2);
      }
      std::swap(var1, var2);
      std::swap(coeff1, coeff2);
    }
  }

  if (num_associations > 0) {
    VLOG(1) << "Num associations from equivalences = " << num_associations;
  }
  if (num_set_to_false > 0) {
    VLOG(1) << "Num literals set to false from equivalences = "
            << num_set_to_false;
  }
}

void DetectOptionalVariables(const CpModelProto& model_proto, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(m->GetOrCreate<SatParameters>());
  if (!parameters.use_optional_variables()) return;
  if (parameters.enumerate_all_solutions()) return;

  // The variables from the objective cannot be marked as optional!
  const int num_proto_variables = model_proto.variables_size();
  std::vector<bool> already_seen(num_proto_variables, false);
  if (model_proto.has_objective()) {
    for (const int ref : model_proto.objective().vars()) {
      already_seen[PositiveRef(ref)] = true;
    }
  }

  // Compute for each variables the intersection of the enforcement literals
  // of the constraints in which they appear.
  //
  // TODO(user): This deals with the simplest cases, but we could try to
  // detect literals that implies all the constraints in which a variable
  // appear to false. This can be done with a LCA computation in the tree of
  // Boolean implication (once the presolve remove cycles). Not sure if we can
  // properly exploit that afterwards though. Do some research!
  std::vector<std::vector<int>> enforcement_intersection(num_proto_variables);
  std::set<int> literals_set;
  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (ct.enforcement_literal().empty()) {
      for (const int var : UsedVariables(ct)) {
        already_seen[var] = true;
        enforcement_intersection[var].clear();
      }
    } else {
      literals_set.clear();
      literals_set.insert(ct.enforcement_literal().begin(),
                          ct.enforcement_literal().end());
      for (const int var : UsedVariables(ct)) {
        if (!already_seen[var]) {
          enforcement_intersection[var].assign(ct.enforcement_literal().begin(),
                                               ct.enforcement_literal().end());
        } else {
          // Take the intersection.
          std::vector<int>& vector_ref = enforcement_intersection[var];
          int new_size = 0;
          for (const int literal : vector_ref) {
            if (gtl::ContainsKey(literals_set, literal)) {
              vector_ref[new_size++] = literal;
            }
          }
          vector_ref.resize(new_size);
        }
        already_seen[var] = true;
      }
    }
  }

  // Auto-detect optional variables.
  int num_optionals = 0;
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  for (int var = 0; var < num_proto_variables; ++var) {
    const IntegerVariableProto& var_proto = model_proto.variables(var);
    const int64_t min = var_proto.domain(0);
    const int64_t max = var_proto.domain(var_proto.domain().size() - 1);
    if (min == max) continue;
    if (min == 0 && max == 1) continue;
    if (enforcement_intersection[var].empty()) continue;

    ++num_optionals;
    integer_trail->MarkIntegerVariableAsOptional(
        mapping->Integer(var),
        mapping->Literal(enforcement_intersection[var].front()));
  }
  VLOG(2) << "Auto-detected " << num_optionals << " optional variables.";
}

void AddFullEncodingFromSearchBranching(const CpModelProto& model_proto,
                                        Model* m) {
  if (model_proto.search_strategy().empty()) return;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    if (strategy.domain_reduction_strategy() ==
        DecisionStrategyProto::SELECT_MEDIAN_VALUE) {
      for (const int ref : strategy.variables()) {
        if (!mapping->IsInteger(ref)) return;
        const IntegerVariable variable = mapping->Integer(PositiveRef(ref));
        if (!integer_trail->IsFixed(variable)) {
          m->Add(FullyEncodeVariable(variable));
        }
      }
    }
  }
}

// ============================================================================
// Constraint loading functions.
// ============================================================================

void LoadBoolOrConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<Literal> literals = mapping->Literals(ct.bool_or().literals());
  for (const int ref : ct.enforcement_literal()) {
    literals.push_back(mapping->Literal(ref).Negated());
  }
  m->Add(ClauseConstraint(literals));
}

void LoadBoolAndConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<Literal> literals;
  for (const int ref : ct.enforcement_literal()) {
    literals.push_back(mapping->Literal(ref).Negated());
  }
  auto* sat_solver = m->GetOrCreate<SatSolver>();
  for (const Literal literal : mapping->Literals(ct.bool_and().literals())) {
    literals.push_back(literal);
    sat_solver->AddProblemClause(literals);
    literals.pop_back();
  }
}

void LoadAtMostOneConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  CHECK(!HasEnforcementLiteral(ct)) << "Not supported.";
  m->Add(AtMostOneConstraint(mapping->Literals(ct.at_most_one().literals())));
}

void LoadExactlyOneConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  CHECK(!HasEnforcementLiteral(ct)) << "Not supported.";
  m->Add(ExactlyOneConstraint(mapping->Literals(ct.exactly_one().literals())));
}

void LoadBoolXorConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  CHECK(!HasEnforcementLiteral(ct)) << "Not supported.";
  m->Add(LiteralXorIs(mapping->Literals(ct.bool_xor().literals()), true));
}

namespace {

// Boolean encoding of:
// enforcement_literal => coeff1 * var1 + coeff2 * var2 == rhs;
void LoadEquivalenceAC(const std::vector<Literal> enforcement_literal,
                       IntegerValue coeff1, IntegerVariable var1,
                       IntegerValue coeff2, IntegerVariable var2,
                       const IntegerValue rhs, Model* m) {
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  CHECK(encoder->VariableIsFullyEncoded(var1));
  CHECK(encoder->VariableIsFullyEncoded(var2));
  absl::flat_hash_map<IntegerValue, Literal> term1_value_to_literal;
  for (const auto value_literal : encoder->FullDomainEncoding(var1)) {
    term1_value_to_literal[coeff1 * value_literal.value] =
        value_literal.literal;
  }
  for (const auto value_literal : encoder->FullDomainEncoding(var2)) {
    const IntegerValue target = rhs - value_literal.value * coeff2;
    if (!term1_value_to_literal.contains(target)) {
      m->Add(EnforcedClause(enforcement_literal,
                            {value_literal.literal.Negated()}));
    } else {
      const Literal target_literal = term1_value_to_literal[target];
      m->Add(EnforcedClause(enforcement_literal,
                            {value_literal.literal.Negated(), target_literal}));
      m->Add(EnforcedClause(enforcement_literal,
                            {value_literal.literal, target_literal.Negated()}));

      // This "target" can never be reached again, so it is safe to remove it.
      // We do that so we know the term1 values that are never reached.
      term1_value_to_literal.erase(target);
    }
  }

  // Exclude the values that can never be "matched" by coeff2 * var2.
  // We need the std::sort() to be deterministic!
  std::vector<Literal> implied_false;
  for (const auto entry : term1_value_to_literal) {
    implied_false.push_back(entry.second);
  }
  std::sort(implied_false.begin(), implied_false.end());
  for (const Literal l : implied_false) {
    m->Add(EnforcedClause(enforcement_literal, {l.Negated()}));
  }
}

// Boolean encoding of:
// enforcement_literal => coeff1 * var1 + coeff2 * var2 != rhs;
void LoadEquivalenceNeqAC(const std::vector<Literal> enforcement_literal,
                          IntegerValue coeff1, IntegerVariable var1,
                          IntegerValue coeff2, IntegerVariable var2,
                          const IntegerValue rhs, Model* m) {
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  CHECK(encoder->VariableIsFullyEncoded(var1));
  CHECK(encoder->VariableIsFullyEncoded(var2));
  absl::flat_hash_map<IntegerValue, Literal> term1_value_to_literal;
  for (const auto value_literal : encoder->FullDomainEncoding(var1)) {
    term1_value_to_literal[coeff1 * value_literal.value] =
        value_literal.literal;
  }
  for (const auto value_literal : encoder->FullDomainEncoding(var2)) {
    const IntegerValue target_value = rhs - value_literal.value * coeff2;
    const auto& it = term1_value_to_literal.find(target_value);
    if (it != term1_value_to_literal.end()) {
      const Literal target_literal = it->second;
      m->Add(EnforcedClause(
          enforcement_literal,
          {value_literal.literal.Negated(), target_literal.Negated()}));
    }
  }
}

}  // namespace

void LoadLinearConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  if (ct.linear().vars().empty()) {
    const Domain rhs = ReadDomainFromProto(ct.linear());
    if (rhs.Contains(0)) return;
    if (HasEnforcementLiteral(ct)) {
      std::vector<Literal> clause;
      for (const int ref : ct.enforcement_literal()) {
        clause.push_back(mapping->Literal(ref).Negated());
      }
      m->Add(ClauseConstraint(clause));
    } else {
      VLOG(1) << "Trivially UNSAT constraint: " << ct.DebugString();
      m->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
    }
    return;
  }

  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.linear().vars());
  const std::vector<int64_t> coeffs = ValuesFromProto(ct.linear().coeffs());

  // Compute the min/max to relax the bounds if needed.
  //
  // TODO(user): Reuse ComputeLinearBounds()? but then we need another loop
  // to detect if we only have Booleans.
  IntegerValue min_sum(0);
  IntegerValue max_sum(0);
  IntegerValue max_domain_size(0);
  bool all_booleans = true;
  for (int i = 0; i < vars.size(); ++i) {
    if (all_booleans && !mapping->IsBoolean(ct.linear().vars(i))) {
      all_booleans = false;
    }
    const IntegerValue lb = integer_trail->LowerBound(vars[i]);
    const IntegerValue ub = integer_trail->UpperBound(vars[i]);
    max_domain_size = std::max(max_domain_size, ub - lb + 1);
    const IntegerValue term_a = coeffs[i] * lb;
    const IntegerValue term_b = coeffs[i] * ub;
    min_sum += std::min(term_a, term_b);
    max_sum += std::max(term_a, term_b);
  }

  const SatParameters& params = *m->GetOrCreate<SatParameters>();
  const IntegerValue domain_size_limit(
      params.max_domain_size_when_encoding_eq_neq_constraints());
  if (ct.linear().vars_size() == 2 && !integer_trail->IsFixed(vars[0]) &&
      !integer_trail->IsFixed(vars[1]) &&
      max_domain_size <= domain_size_limit) {
    auto* encoder = m->GetOrCreate<IntegerEncoder>();
    if (params.boolean_encoding_level() > 0 && ConstraintIsEq(ct.linear()) &&
        ct.linear().domain(0) != min_sum && ct.linear().domain(0) != max_sum &&
        encoder->VariableIsFullyEncoded(vars[0]) &&
        encoder->VariableIsFullyEncoded(vars[1])) {
      VLOG(3) << "Load AC version of " << ct.DebugString() << ", var0 domain = "
              << integer_trail->InitialVariableDomain(vars[0])
              << ", var1 domain = "
              << integer_trail->InitialVariableDomain(vars[1]);
      return LoadEquivalenceAC(mapping->Literals(ct.enforcement_literal()),
                               IntegerValue(coeffs[0]), vars[0],
                               IntegerValue(coeffs[1]), vars[1],
                               IntegerValue(ct.linear().domain(0)), m);
    }

    int64_t single_value = 0;
    if (params.boolean_encoding_level() > 0 &&
        ConstraintIsNEq(ct.linear(), mapping, integer_trail, &single_value) &&
        single_value != min_sum && single_value != max_sum &&
        encoder->VariableIsFullyEncoded(vars[0]) &&
        encoder->VariableIsFullyEncoded(vars[1])) {
      VLOG(3) << "Load NAC version of " << ct.DebugString()
              << ", var0 domain = "
              << integer_trail->InitialVariableDomain(vars[0])
              << ", var1 domain = "
              << integer_trail->InitialVariableDomain(vars[1])
              << ", value = " << single_value;
      return LoadEquivalenceNeqAC(mapping->Literals(ct.enforcement_literal()),
                                  IntegerValue(coeffs[0]), vars[0],
                                  IntegerValue(coeffs[1]), vars[1],
                                  IntegerValue(single_value), m);
    }
  }

  if (ct.linear().domain_size() == 2) {
    int64_t lb = ct.linear().domain(0);
    int64_t ub = ct.linear().domain(1);
    if (min_sum >= lb) lb = std::numeric_limits<int64_t>::min();
    if (max_sum <= ub) ub = std::numeric_limits<int64_t>::max();

    if (!HasEnforcementLiteral(ct)) {
      if (all_booleans) {
        // TODO(user): we should probably also implement an
        // half-reified version of this constraint.
        std::vector<LiteralWithCoeff> cst;
        for (int i = 0; i < vars.size(); ++i) {
          const int ref = ct.linear().vars(i);
          cst.push_back({mapping->Literal(ref), coeffs[i]});
        }
        m->Add(BooleanLinearConstraint(lb, ub, &cst));
      } else {
        if (lb != std::numeric_limits<int64_t>::min()) {
          m->Add(WeightedSumGreaterOrEqual(vars, coeffs, lb));
        }
        if (ub != std::numeric_limits<int64_t>::max()) {
          m->Add(WeightedSumLowerOrEqual(vars, coeffs, ub));
        }
      }
    } else {
      const std::vector<Literal> enforcement_literals =
          mapping->Literals(ct.enforcement_literal());
      if (lb != std::numeric_limits<int64_t>::min()) {
        m->Add(ConditionalWeightedSumGreaterOrEqual(enforcement_literals, vars,
                                                    coeffs, lb));
      }
      if (ub != std::numeric_limits<int64_t>::max()) {
        m->Add(ConditionalWeightedSumLowerOrEqual(enforcement_literals, vars,
                                                  coeffs, ub));
      }
    }
  } else {
    // In this case, we can create just one Boolean instead of two since one
    // is the negation of the other.
    const bool special_case =
        ct.enforcement_literal().empty() && ct.linear().domain_size() == 4;

    std::vector<Literal> clause;
    for (int i = 0; i < ct.linear().domain_size(); i += 2) {
      int64_t lb = ct.linear().domain(i);
      int64_t ub = ct.linear().domain(i + 1);
      if (min_sum >= lb) lb = std::numeric_limits<int64_t>::min();
      if (max_sum <= ub) ub = std::numeric_limits<int64_t>::max();

      const Literal subdomain_literal(
          special_case && i > 0 ? clause.back().Negated()
                                : Literal(m->Add(NewBooleanVariable()), true));
      clause.push_back(subdomain_literal);

      if (lb != std::numeric_limits<int64_t>::min()) {
        m->Add(ConditionalWeightedSumGreaterOrEqual({subdomain_literal}, vars,
                                                    coeffs, lb));
      }
      if (ub != std::numeric_limits<int64_t>::max()) {
        m->Add(ConditionalWeightedSumLowerOrEqual({subdomain_literal}, vars,
                                                  coeffs, ub));
      }
    }
    for (const int ref : ct.enforcement_literal()) {
      clause.push_back(mapping->Literal(ref).Negated());
    }
    if (!special_case) m->Add(ClauseConstraint(clause));
  }
}

void LoadAllDiffConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<AffineExpression> expressions =
      mapping->Affines(ct.all_diff().exprs());
  m->Add(AllDifferentOnBounds(expressions));
}

void LoadIntProdConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const AffineExpression prod = mapping->Affine(ct.int_prod().target());
  CHECK_EQ(ct.int_prod().exprs_size(), 2)
      << "General int_prod not supported yet.";

  const AffineExpression expr0 = mapping->Affine(ct.int_prod().exprs(0));
  const AffineExpression expr1 = mapping->Affine(ct.int_prod().exprs(1));
  if (VLOG_IS_ON(1)) {
    LinearConstraintBuilder builder(m);
    if (DetectLinearEncodingOfProducts(expr0, expr1, m, &builder)) {
      VLOG(1) << "Product " << ct.DebugString() << " can be linearized";
    }
  }
  m->Add(ProductConstraint(expr0, expr1, prod));
}

void LoadIntDivConstraint(const ConstraintProto& ct, Model* m) {
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const AffineExpression div = mapping->Affine(ct.int_div().target());
  const AffineExpression num = mapping->Affine(ct.int_div().exprs(0));
  const AffineExpression denom = mapping->Affine(ct.int_div().exprs(1));
  if (integer_trail->IsFixed(denom)) {
    m->Add(FixedDivisionConstraint(num, integer_trail->FixedValue(denom), div));
  } else {
    if (VLOG_IS_ON(1)) {
      LinearConstraintBuilder builder(m);
      if (DetectLinearEncodingOfProducts(num, denom, m, &builder)) {
        VLOG(1) << "Division " << ct.DebugString() << " can be linearized";
      }
    }
    m->Add(DivisionConstraint(num, denom, div));
  }
}

void LoadIntModConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();

  const AffineExpression target = mapping->Affine(ct.int_mod().target());
  const AffineExpression expr = mapping->Affine(ct.int_mod().exprs(0));
  const AffineExpression mod = mapping->Affine(ct.int_mod().exprs(1));
  CHECK(integer_trail->IsFixed(mod));
  const IntegerValue fixed_modulo = integer_trail->FixedValue(mod);
  m->Add(FixedModuloConstraint(expr, fixed_modulo, target));
}

void LoadLinMaxConstraint(const ConstraintProto& ct, Model* m) {
  if (ct.lin_max().exprs().empty()) {
    m->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
    return;
  }

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const LinearExpression max = mapping->GetExprFromProto(ct.lin_max().target());
  std::vector<LinearExpression> negated_exprs;
  negated_exprs.reserve(ct.lin_max().exprs_size());
  for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
    negated_exprs.push_back(
        NegationOf(mapping->GetExprFromProto(ct.lin_max().exprs(i))));
  }
  // TODO(user): Consider replacing the min propagator by max.
  m->Add(IsEqualToMinOf(NegationOf(max), negated_exprs));
}

void LoadNoOverlapConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  m->Add(Disjunctive(mapping->Intervals(ct.no_overlap().intervals())));
}

void LoadNoOverlap2dConstraint(const ConstraintProto& ct, Model* m) {
  if (ct.no_overlap_2d().x_intervals().empty()) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntervalVariable> x_intervals =
      mapping->Intervals(ct.no_overlap_2d().x_intervals());
  const std::vector<IntervalVariable> y_intervals =
      mapping->Intervals(ct.no_overlap_2d().y_intervals());
  m->Add(NonOverlappingRectangles(
      x_intervals, y_intervals,
      !ct.no_overlap_2d().boxes_with_null_area_can_overlap(),
      m->GetOrCreate<SatParameters>()->use_cumulative_in_no_overlap_2d()));
}

void LoadCumulativeConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  const AffineExpression capacity = mapping->Affine(ct.cumulative().capacity());
  const std::vector<AffineExpression> demands =
      mapping->Affines(ct.cumulative().demands());
  m->Add(Cumulative(intervals, demands, capacity));
}

void LoadCircuitConstraint(const ConstraintProto& ct, Model* m) {
  const auto& circuit = ct.circuit();
  if (circuit.tails().empty()) return;

  std::vector<int> tails(circuit.tails().begin(), circuit.tails().end());
  std::vector<int> heads(circuit.heads().begin(), circuit.heads().end());
  std::vector<Literal> literals =
      m->GetOrCreate<CpModelMapping>()->Literals(circuit.literals());
  const int num_nodes = ReindexArcs(&tails, &heads);
  m->Add(SubcircuitConstraint(num_nodes, tails, heads, literals));
}

void LoadRoutesConstraint(const ConstraintProto& ct, Model* m) {
  const auto& routes = ct.routes();
  if (routes.tails().empty()) return;

  std::vector<int> tails(routes.tails().begin(), routes.tails().end());
  std::vector<int> heads(routes.heads().begin(), routes.heads().end());
  std::vector<Literal> literals =
      m->GetOrCreate<CpModelMapping>()->Literals(routes.literals());
  const int num_nodes = ReindexArcs(&tails, &heads);
  m->Add(SubcircuitConstraint(num_nodes, tails, heads, literals,
                              /*multiple_subcircuit_through_zero=*/true));
}

bool LoadConstraint(const ConstraintProto& ct, Model* m) {
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      return true;
    case ConstraintProto::ConstraintCase::kBoolOr:
      LoadBoolOrConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      LoadBoolAndConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kAtMostOne:
      LoadAtMostOneConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kExactlyOne:
      LoadExactlyOneConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kBoolXor:
      LoadBoolXorConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kLinear:
      LoadLinearConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kAllDiff:
      LoadAllDiffConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntProd:
      LoadIntProdConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntDiv:
      LoadIntDivConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntMod:
      LoadIntModConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kLinMax:
      LoadLinMaxConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kInterval:
      // Already dealt with.
      return true;
    case ConstraintProto::ConstraintProto::kNoOverlap:
      LoadNoOverlapConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kNoOverlap2D:
      LoadNoOverlap2dConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kCumulative:
      LoadCumulativeConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kCircuit:
      LoadCircuitConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kRoutes:
      LoadRoutesConstraint(ct, m);
      return true;
    default:
      return false;
  }
}

}  // namespace sat
}  // namespace operations_research
