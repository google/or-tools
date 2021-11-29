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
      if (ct.interval().has_start_view()) {
        mapping->intervals_[c] = intervals_repository->CreateInterval(
            mapping->LoadAffineView(ct.interval().start_view()),
            mapping->LoadAffineView(ct.interval().end_view()),
            mapping->LoadAffineView(ct.interval().size_view()),
            enforcement_literal.Index(),
            /*add_linear_relation=*/false);
      } else {
        mapping->intervals_[c] = m->Add(NewOptionalInterval(
            mapping->Integer(ct.interval().start()),
            mapping->Integer(ct.interval().end()),
            mapping->Integer(ct.interval().size()), enforcement_literal));
      }
    } else {
      if (ct.interval().has_start_view()) {
        mapping->intervals_[c] = intervals_repository->CreateInterval(
            mapping->LoadAffineView(ct.interval().start_view()),
            mapping->LoadAffineView(ct.interval().end_view()),
            mapping->LoadAffineView(ct.interval().size_view()), kNoLiteralIndex,
            /*add_linear_relation=*/false);
      } else {
        mapping->intervals_[c] =
            m->Add(NewInterval(mapping->Integer(ct.interval().start()),
                               mapping->Integer(ct.interval().end()),
                               mapping->Integer(ct.interval().size())));
      }
    }
    mapping->already_loaded_ct_.insert(&ct);
  }
}

void LoadBooleanSymmetries(const CpModelProto& model_proto, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const SymmetryProto symmetry = model_proto.symmetry();
  if (symmetry.permutations().empty()) return;

  auto* sat_solver = m->GetOrCreate<SatSolver>();
  auto* symmetry_handler = m->GetOrCreate<SymmetryPropagator>();
  sat_solver->AddPropagator(symmetry_handler);
  const int num_literals = 2 * sat_solver->NumVariables();

  for (const SparsePermutationProto& perm : symmetry.permutations()) {
    bool all_bool = true;
    for (const int var : perm.support()) {
      if (!mapping->IsBoolean(var)) {
        all_bool = false;
        break;
      }
    }
    if (!all_bool) continue;

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

// ============================================================================
// A class that detects when variables should be fully encoded by computing a
// fixed point. It also fully encodes such variables.
// ============================================================================

class FullEncodingFixedPointComputer {
 public:
  FullEncodingFixedPointComputer(const CpModelProto& model_proto, Model* model)
      : model_proto_(model_proto),
        parameters_(*(model->GetOrCreate<SatParameters>())),
        model_(model),
        mapping_(model->GetOrCreate<CpModelMapping>()),
        integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

  void ComputeFixedPoint();

 private:
  DEFINE_INT_TYPE(ConstraintIndex, int32_t);

  // Constraint ct is interested by (full-encoding) state of variable.
  void Register(ConstraintIndex ct_index, int variable) {
    variable = PositiveRef(variable);
    constraint_is_registered_[ct_index] = true;
    if (variable_watchers_.size() <= variable) {
      variable_watchers_.resize(variable + 1);
      variable_was_added_in_to_propagate_.resize(variable + 1);
    }
    variable_watchers_[variable].push_back(ct_index);
  }

  void AddVariableToPropagationQueue(int variable) {
    variable = PositiveRef(variable);
    if (variable_was_added_in_to_propagate_.size() <= variable) {
      variable_watchers_.resize(variable + 1);
      variable_was_added_in_to_propagate_.resize(variable + 1);
    }
    if (!variable_was_added_in_to_propagate_[variable]) {
      variable_was_added_in_to_propagate_[variable] = true;
      variables_to_propagate_.push_back(variable);
    }
  }

  // Note that we always consider a fixed variable to be fully encoded here.
  const bool IsFullyEncoded(int v) {
    const IntegerVariable variable = mapping_->Integer(v);
    if (variable == kNoIntegerVariable) return false;
    return integer_trail_->IsFixed(variable) ||
           integer_encoder_->VariableIsFullyEncoded(variable);
  }

  const bool VariableIsFixed(int v) {
    const IntegerVariable variable = mapping_->Integer(v);
    if (v == kNoIntegerVariable) return false;
    return integer_trail_->IsFixed(variable);
  }

  void FullyEncode(int v) {
    v = PositiveRef(v);
    const IntegerVariable variable = mapping_->Integer(v);
    if (v == kNoIntegerVariable) return;
    if (!integer_trail_->IsFixed(variable)) {
      model_->Add(FullyEncodeVariable(variable));
    }
    AddVariableToPropagationQueue(v);
  }

  bool ProcessConstraint(ConstraintIndex ct_index);
  bool ProcessElement(ConstraintIndex ct_index);
  bool ProcessTable(ConstraintIndex ct_index);
  bool ProcessAutomaton(ConstraintIndex ct_index);
  bool ProcessLinear(ConstraintIndex ct_index);

  const CpModelProto& model_proto_;
  const SatParameters& parameters_;

  Model* model_;
  CpModelMapping* mapping_;
  IntegerEncoder* integer_encoder_;
  IntegerTrail* integer_trail_;

  std::vector<bool> variable_was_added_in_to_propagate_;
  std::vector<int> variables_to_propagate_;
  std::vector<std::vector<ConstraintIndex>> variable_watchers_;

  absl::StrongVector<ConstraintIndex, bool> constraint_is_finished_;
  absl::StrongVector<ConstraintIndex, bool> constraint_is_registered_;

  absl::flat_hash_map<int, absl::flat_hash_set<int>>
      variables_to_equal_or_diff_variables_;
};

// We only add to the propagation queue variable that are fully encoded.
// Note that if a variable was already added once, we never add it again.
void FullEncodingFixedPointComputer::ComputeFixedPoint() {
  const int num_constraints = model_proto_.constraints_size();
  const int num_vars = model_proto_.variables_size();
  constraint_is_finished_.assign(num_constraints, false);
  constraint_is_registered_.assign(num_constraints, false);

  // Process all constraint once.
  for (ConstraintIndex ct_index(0); ct_index < num_constraints; ++ct_index) {
    constraint_is_finished_[ct_index] = ProcessConstraint(ct_index);
  }

  // We run a heuristics to decide if we want to fully encode a variable or not.
  // We decide to fully encode a variable if:
  //   - a variable appears in enough a1 * x1 + a2 + x2 ==/!= value and the
  //     domain is small.
  //   - the number of values that appears in b => x ==/!= value that are not
  //     the bounds of the variables is more that half the size of the domain.
  // . - the size of the domain is > 2
  int num_variables_fully_encoded_by_heuristics = 0;
  for (int var = 0; var < num_vars; ++var) {
    if (!mapping_->IsInteger(var) || IsFullyEncoded(var)) continue;
    const IntegerVariableProto& int_var_proto = model_proto_.variables(var);
    const Domain domain = ReadDomainFromProto(int_var_proto);
    int64_t domain_size = domain.Size();
    int64_t num_diff_or_equal_var_constraints = 0;
    int64_t num_potential_encoded_values_without_bounds = 0;

    if (domain_size <= 2) continue;

    const absl::flat_hash_set<int64_t>& value_set =
        mapping_->PotentialEncodedValues(var);
    for (const int value : value_set) {
      if (value > domain.Min() && value < domain.Max() &&
          domain.Contains(value)) {
        num_potential_encoded_values_without_bounds++;
      }
    }

    const auto& it = variables_to_equal_or_diff_variables_.find(var);
    if (it != variables_to_equal_or_diff_variables_.end()) {
      num_diff_or_equal_var_constraints = it->second.size();
    }

    if (num_potential_encoded_values_without_bounds >= domain_size / 2 ||
        (num_diff_or_equal_var_constraints >= domain_size / 2 &&
         domain_size < 16)) {
      VLOG(3) << model_proto_.variables(var).ShortDebugString()
              << " is encoded with "
              << num_potential_encoded_values_without_bounds
              << " unary constraints, and " << num_diff_or_equal_var_constraints
              << " binary constraints on a domain of size " << domain_size;
      FullyEncode(var);
      num_variables_fully_encoded_by_heuristics++;
    }
  }
  if (num_variables_fully_encoded_by_heuristics > 0) {
    VLOG(2) << num_variables_fully_encoded_by_heuristics
            << " variables fully encoded after model introspection.";
  }

  // Make sure all fully encoded variables of interest are in the queue.
  for (int v = 0; v < variable_watchers_.size(); v++) {
    if (!variable_watchers_[v].empty() && IsFullyEncoded(v)) {
      AddVariableToPropagationQueue(v);
    }
  }

  // Loop until no additional variable can be fully encoded.
  while (!variables_to_propagate_.empty()) {
    const int variable = variables_to_propagate_.back();
    variables_to_propagate_.pop_back();
    for (const ConstraintIndex ct_index : variable_watchers_[variable]) {
      if (constraint_is_finished_[ct_index]) continue;
      constraint_is_finished_[ct_index] = ProcessConstraint(ct_index);
    }
  }
}

// Returns true if the constraint has finished encoding what it wants.
bool FullEncodingFixedPointComputer::ProcessConstraint(
    ConstraintIndex ct_index) {
  const ConstraintProto& ct = model_proto_.constraints(ct_index.value());
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintProto::kElement:
      return ProcessElement(ct_index);
    case ConstraintProto::ConstraintProto::kTable:
      return ProcessTable(ct_index);
    case ConstraintProto::ConstraintProto::kAutomaton:
      return ProcessAutomaton(ct_index);
    case ConstraintProto::ConstraintProto::kLinear:
      return ProcessLinear(ct_index);
    default:
      return true;
  }
}

bool FullEncodingFixedPointComputer::ProcessElement(ConstraintIndex ct_index) {
  const ConstraintProto& ct = model_proto_.constraints(ct_index.value());

  // Index must always be full encoded.
  FullyEncode(ct.element().index());

  const int target = ct.element().target();

  // If target is fixed, do not encode variables.
  if (VariableIsFixed(target)) return true;

  // If target is a constant or fully encoded, variables must be fully encoded.
  if (IsFullyEncoded(target)) {
    for (const int v : ct.element().vars()) FullyEncode(v);
  }

  // If all non-target variables are fully encoded, target must be too.
  bool all_variables_are_fully_encoded = true;
  for (const int v : ct.element().vars()) {
    if (v == target) continue;
    if (!IsFullyEncoded(v)) {
      all_variables_are_fully_encoded = false;
      break;
    }
  }
  if (all_variables_are_fully_encoded) {
    if (!IsFullyEncoded(target)) FullyEncode(target);
    return true;
  }

  // If some variables are not fully encoded, register on those.
  if (constraint_is_registered_[ct_index]) {
    for (const int v : ct.element().vars()) Register(ct_index, v);
    Register(ct_index, target);
  }
  return false;
}

bool FullEncodingFixedPointComputer::ProcessTable(ConstraintIndex ct_index) {
  const ConstraintProto& ct = model_proto_.constraints(ct_index.value());

  if (ct.table().negated()) return true;

  for (const int variable : ct.table().vars()) {
    FullyEncode(variable);
  }

  return true;
}

bool FullEncodingFixedPointComputer::ProcessAutomaton(
    ConstraintIndex ct_index) {
  const ConstraintProto& ct = model_proto_.constraints(ct_index.value());
  for (const int variable : ct.automaton().vars()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::ProcessLinear(ConstraintIndex ct_index) {
  // We are only interested in linear equations of the form:
  //     [b =>] a1 * x1 + a2 * x2 ==|!= value
  const ConstraintProto& ct = model_proto_.constraints(ct_index.value());
  if (parameters_.boolean_encoding_level() == 0 ||
      ct.linear().vars_size() != 2) {
    return true;
  }

  if (!ConstraintIsEq(ct.linear()) &&
      !ConstraintIsNEq(ct.linear(), mapping_, integer_trail_, nullptr)) {
    return true;
  }

  const int var0 = ct.linear().vars(0);
  const int var1 = ct.linear().vars(1);
  if (!IsFullyEncoded(var0)) {
    variables_to_equal_or_diff_variables_[var0].insert(var1);
  }
  if (!IsFullyEncoded(var1)) {
    variables_to_equal_or_diff_variables_[var1].insert(var0);
  }
  return true;
}

void MaybeFullyEncodeMoreVariables(const CpModelProto& model_proto, Model* m) {
  FullEncodingFixedPointComputer fixpoint(model_proto, m);
  fixpoint.ComputeFixedPoint();
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
    if (!gtl::ContainsKey(term1_value_to_literal, target)) {
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
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.all_diff().vars());
  // If all variables are fully encoded and domains are not too large, use
  // arc-consistent reasoning. Otherwise, use bounds-consistent reasoning.
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  IntegerEncoder* encoder = m->GetOrCreate<IntegerEncoder>();
  int num_fully_encoded = 0;
  int64_t max_domain_size = 0;
  for (const IntegerVariable variable : vars) {
    if (encoder->VariableIsFullyEncoded(variable)) num_fully_encoded++;

    IntegerValue lb = integer_trail->LowerBound(variable);
    IntegerValue ub = integer_trail->UpperBound(variable);
    const int64_t domain_size = ub.value() - lb.value() + 1;
    max_domain_size = std::max(max_domain_size, domain_size);
  }

  if (num_fully_encoded == vars.size() && max_domain_size < 1024) {
    m->Add(AllDifferentBinary(vars));
    m->Add(AllDifferentAC(vars));
  } else {
    m->Add(AllDifferentOnBounds(vars));
  }
}

void LoadIntProdConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable prod = mapping->Integer(ct.int_prod().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.int_prod().vars());
  CHECK_EQ(vars.size(), 2) << "General int_prod not supported yet.";
  m->Add(ProductConstraint(vars[0], vars[1], prod));
}

void LoadIntDivConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable div = mapping->Integer(ct.int_div().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.int_div().vars());
  if (m->Get(IsFixed(vars[1]))) {
    const IntegerValue denom(m->Get(Value(vars[1])));
    if (denom == 1) {
      m->Add(Equality(vars[0], div));
    } else {
      m->Add(FixedDivisionConstraint(vars[0], denom, div));
    }
  } else {
    m->Add(DivisionConstraint(vars[0], vars[1], div));
  }
}

void LoadIntMinConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable min = mapping->Integer(ct.int_min().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.int_min().vars());
  m->Add(IsEqualToMinOf(min, vars));
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

void LoadIntMaxConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable max = mapping->Integer(ct.int_max().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.int_max().vars());
  m->Add(IsEqualToMaxOf(max, vars));
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
      !ct.no_overlap_2d().boxes_with_null_area_can_overlap()));
}

void LoadCumulativeConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  const AffineExpression capacity(mapping->Integer(ct.cumulative().capacity()));
  std::vector<AffineExpression> demands;
  for (const IntegerVariable var :
       mapping->Integers(ct.cumulative().demands())) {
    demands.push_back(AffineExpression(var));
  }
  m->Add(Cumulative(intervals, demands, capacity));
}

void LoadReservoirConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  std::vector<AffineExpression> times;
  std::vector<IntegerValue> deltas;
  std::vector<Literal> presences;
  const int size = ct.reservoir().times().size();
  for (int i = 0; i < size; ++i) {
    times.push_back(mapping->Integer(ct.reservoir().times(i)));
    deltas.push_back(IntegerValue(ct.reservoir().demands(i)));
    if (!ct.reservoir().actives().empty()) {
      presences.push_back(mapping->Literal(ct.reservoir().actives(i)));
    } else {
      presences.push_back(encoder->GetTrueLiteral());
    }
  }
  AddReservoirConstraint(times, deltas, presences, ct.reservoir().min_level(),
                         ct.reservoir().max_level(), m);
}

// If a variable is constant and its value appear in no other variable domains,
// then the literal encoding the index and the one encoding the target at this
// value are equivalent.
bool DetectEquivalencesInElementConstraint(const ConstraintProto& ct,
                                           Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  IntegerEncoder* encoder = m->GetOrCreate<IntegerEncoder>();
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();

  const IntegerVariable index = mapping->Integer(ct.element().index());
  const IntegerVariable target = mapping->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.element().vars());
  CHECK(!m->Get(IsFixed(index)));
  CHECK(!m->Get(IsFixed(target)));

  Domain union_of_non_constant_domains;
  std::map<IntegerValue, int> constant_to_num;
  for (const auto literal_value : m->Add(FullyEncodeVariable(index))) {
    const int i = literal_value.value.value();
    if (m->Get(IsFixed(vars[i]))) {
      const IntegerValue value(m->Get(Value(vars[i])));
      constant_to_num[value]++;
    } else {
      union_of_non_constant_domains = union_of_non_constant_domains.UnionWith(
          integer_trail->InitialVariableDomain(vars[i]));
    }
  }

  // Bump the number if the constant appear in union_of_non_constant_domains.
  for (const auto entry : constant_to_num) {
    if (union_of_non_constant_domains.Contains(entry.first.value())) {
      constant_to_num[entry.first]++;
    }
  }

  // Use the literal from the index encoding to encode the target at the
  // "unique" values.
  bool is_one_to_one_mapping = true;
  for (const auto literal_value : m->Add(FullyEncodeVariable(index))) {
    const int i = literal_value.value.value();
    if (!m->Get(IsFixed(vars[i]))) {
      is_one_to_one_mapping = false;
      continue;
    }

    const IntegerValue value(m->Get(Value(vars[i])));
    if (constant_to_num[value] == 1) {
      const Literal r = literal_value.literal;
      encoder->AssociateToIntegerEqualValue(r, target, value);
    } else {
      is_one_to_one_mapping = false;
    }
  }

  return is_one_to_one_mapping;
}

// TODO(user): Be more efficient when the element().vars() are constants.
// Ideally we should avoid creating them as integer variable since we don't
// use them.
void LoadElementConstraintBounds(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable index = mapping->Integer(ct.element().index());
  const IntegerVariable target = mapping->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.element().vars());
  CHECK(!m->Get(IsFixed(index)));

  // We always fully encode the index on an element constraint.
  const auto encoding = m->Add(FullyEncodeVariable((index)));
  std::vector<Literal> selectors;
  std::vector<IntegerVariable> possible_vars;
  for (const auto literal_value : encoding) {
    const int i = literal_value.value.value();
    CHECK_GE(i, 0);
    CHECK_LT(i, vars.size());
    possible_vars.push_back(vars[i]);
    selectors.push_back(literal_value.literal);
    const Literal r = literal_value.literal;

    if (vars[i] == target) continue;
    if (m->Get(IsFixed(target))) {
      const int64_t value = m->Get(Value(target));
      m->Add(ImpliesInInterval(r, vars[i], value, value));
    } else if (m->Get(IsFixed(vars[i]))) {
      const int64_t value = m->Get(Value(vars[i]));
      m->Add(ImpliesInInterval(r, target, value, value));
    } else {
      m->Add(ConditionalLowerOrEqualWithOffset(vars[i], target, 0, r));
      m->Add(ConditionalLowerOrEqualWithOffset(target, vars[i], 0, r));
    }
  }

  if (!m->Get(IsFixed(target))) {
    m->Add(PartialIsOneOfVar(target, possible_vars, selectors));
  }
}

// Arc-Consistent encoding of the element constraint as SAT clauses.
// The constraint enforces vars[index] == target.
//
// The AC propagation can be decomposed in three rules:
// Rule 1: dom(index) == i => dom(vars[i]) == dom(target).
// Rule 2: dom(target) \subseteq \Union_{i \in dom(index)} dom(vars[i]).
// Rule 3: dom(index) \subseteq { i | |dom(vars[i]) \inter dom(target)| > 0 }.
//
// We encode this in a way similar to the table constraint, except that the
// set of admissible tuples is not explicit.
// First, we add Booleans selected[i][value] <=> (index == i /\ vars[i] ==
// value). Rules 1 and 2 are enforced by target == value <=> \Or_{i}
// selected[i][value]. Rule 3 is enforced by index == i <=> \Or_{value}
// selected[i][value].
void LoadElementConstraintAC(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable index = mapping->Integer(ct.element().index());
  const IntegerVariable target = mapping->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.element().vars());
  CHECK(!m->Get(IsFixed(index)));
  CHECK(!m->Get(IsFixed(target)));

  absl::flat_hash_map<IntegerValue, Literal> target_map;
  const auto target_encoding = m->Add(FullyEncodeVariable(target));
  for (const auto literal_value : target_encoding) {
    target_map[literal_value.value] = literal_value.literal;
  }

  // For i \in index and value in vars[i], make (index == i /\ vars[i] == value)
  // literals and store them by value in vectors.
  absl::flat_hash_map<IntegerValue, std::vector<Literal>> value_to_literals;
  const auto index_encoding = m->Add(FullyEncodeVariable(index));
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  for (const auto literal_value : index_encoding) {
    const int i = literal_value.value.value();
    const Literal i_lit = literal_value.literal;

    // Special case where vars[i] == value /\ i_lit is actually i_lit.
    if (m->Get(IsFixed(vars[i]))) {
      value_to_literals[integer_trail->LowerBound(vars[i])].push_back(i_lit);
      continue;
    }

    const auto var_encoding = m->Add(FullyEncodeVariable(vars[i]));
    std::vector<Literal> var_selected_literals;
    for (const auto var_literal_value : var_encoding) {
      const IntegerValue value = var_literal_value.value;
      const Literal var_is_value = var_literal_value.literal;

      if (!gtl::ContainsKey(target_map, value)) {
        // No need to add to value_to_literals, selected[i][value] is always
        // false.
        m->Add(Implication(i_lit, var_is_value.Negated()));
        continue;
      }

      const Literal var_is_value_and_selected =
          Literal(m->Add(NewBooleanVariable()), true);
      m->Add(ReifiedBoolAnd({i_lit, var_is_value}, var_is_value_and_selected));
      value_to_literals[value].push_back(var_is_value_and_selected);
      var_selected_literals.push_back(var_is_value_and_selected);
    }
    // index == i <=> \Or_{value} selected[i][value].
    m->Add(ReifiedBoolOr(var_selected_literals, i_lit));
  }

  // target == value <=> \Or_{i \in index} (vars[i] == value /\ index == i).
  for (const auto& entry : target_map) {
    const IntegerValue value = entry.first;
    const Literal target_is_value = entry.second;

    if (!gtl::ContainsKey(value_to_literals, value)) {
      m->Add(ClauseConstraint({target_is_value.Negated()}));
    } else {
      m->Add(ReifiedBoolOr(value_to_literals[value], target_is_value));
    }
  }
}

namespace {

// This Boolean encoding is enough for consistency, but does not propagate as
// much as LoadElementConstraintAC(). However, setting any of the non-propagated
// Booleans to its "wrong" value will result directly in a conflict, so the
// solver will easily learn an AC encoding...
//
// The advantage is that this does not introduce extra BooleanVariables.
void LoadElementConstraintHalfAC(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable index = mapping->Integer(ct.element().index());
  const IntegerVariable target = mapping->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.element().vars());
  CHECK(!m->Get(IsFixed(index)));
  CHECK(!m->Get(IsFixed(target)));

  m->Add(FullyEncodeVariable(target));
  for (const auto value_literal : m->Add(FullyEncodeVariable(index))) {
    const int i = value_literal.value.value();
    m->Add(FullyEncodeVariable(vars[i]));
    LoadEquivalenceAC({value_literal.literal}, IntegerValue(1), vars[i],
                      IntegerValue(-1), target, IntegerValue(0), m);
  }
}

void LoadBooleanElement(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable index = mapping->Integer(ct.element().index());
  const std::vector<Literal> literals = mapping->Literals(ct.element().vars());
  const Literal target = mapping->Literal(ct.element().target());

  if (m->Get(IsFixed(index))) {
    m->Add(Equality(target, literals[m->Get(Value(index))]));
    return;
  }

  std::vector<Literal> all_true;
  std::vector<Literal> all_false;
  for (const auto value_literal : m->Add(FullyEncodeVariable(index))) {
    const Literal a_lit = literals[value_literal.value.value()];
    const Literal i_lit = value_literal.literal;
    m->Add(ClauseConstraint({i_lit.Negated(), a_lit.Negated(), target}));
    m->Add(ClauseConstraint({i_lit.Negated(), a_lit, target.Negated()}));
    all_true.push_back(a_lit.Negated());
    all_false.push_back(a_lit);
  }
  all_true.push_back(target);
  all_false.push_back(target.Negated());
  m->Add(ClauseConstraint(all_true));
  m->Add(ClauseConstraint(all_false));
  // TODO(user): Investigate filtering this with active literals.
}

}  // namespace

void LoadElementConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable index = mapping->Integer(ct.element().index());

  bool boolean_array = true;
  for (const int ref : ct.element().vars()) {
    if (!mapping->IsBoolean(ref)) {
      boolean_array = false;
      break;
    }
  }
  if (boolean_array && !mapping->IsBoolean(ct.element().target())) {
    // Should have been reduced but presolve.
    VLOG(1) << "Fix boolean_element not propagated on target";
    boolean_array = false;
  }

  // TODO(user): Move this to presolve. Leads to a larger discussion on
  // adding full encoding to model during presolve.
  if (boolean_array) {
    LoadBooleanElement(ct, m);
    return;
  }

  const IntegerVariable target = mapping->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.element().vars());

  // Retrict the domain of index in case there was no presolve.
  if (!m->GetOrCreate<IntegerTrail>()->UpdateInitialDomain(
          index, Domain(0, vars.size() - 1))) {
    return;
  }

  // This returns true if there is nothing else to do after the equivalences
  // of the form (index literal <=> target_literal) have been added.
  if (!m->Get(IsFixed(index)) && !m->Get(IsFixed(target)) &&
      DetectEquivalencesInElementConstraint(ct, m)) {
    return;
  }

  // Special case when index is fixed.
  if (m->Get(IsFixed(index))) {
    m->Add(Equality(target, vars[m->Get(Value(index))]));
    return;
  }

  // Special case when target is fixed.
  if (m->Get(IsFixed(target))) {
    return LoadElementConstraintBounds(ct, m);
  }

  IntegerEncoder* encoder = m->GetOrCreate<IntegerEncoder>();
  const bool target_is_AC = encoder->VariableIsFullyEncoded(target);

  int num_AC_variables = 0;
  const int num_vars = ct.element().vars().size();
  for (const int v : ct.element().vars()) {
    IntegerVariable variable = mapping->Integer(v);
    const bool is_full =
        m->Get(IsFixed(variable)) || encoder->VariableIsFullyEncoded(variable);
    if (is_full) num_AC_variables++;
  }

  const SatParameters& params = *m->GetOrCreate<SatParameters>();
  if (params.boolean_encoding_level() > 0 &&
      (target_is_AC || num_AC_variables >= num_vars - 1)) {
    if (params.boolean_encoding_level() > 1) {
      LoadElementConstraintAC(ct, m);
    } else {
      LoadElementConstraintHalfAC(ct, m);
    }
  } else {
    LoadElementConstraintBounds(ct, m);
  }
}

void LoadTableConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.table().vars());
  const std::vector<int64_t> values = ValuesFromProto(ct.table().values());
  const int num_vars = vars.size();
  const int num_tuples = values.size() / num_vars;
  std::vector<std::vector<int64_t>> tuples(num_tuples);
  int count = 0;
  for (int i = 0; i < num_tuples; ++i) {
    for (int j = 0; j < num_vars; ++j) {
      tuples[i].push_back(values[count++]);
    }
  }
  if (ct.table().negated()) {
    AddNegatedTableConstraint(vars, std::move(tuples), m);
  } else {
    AddTableConstraint(vars, std::move(tuples), m);
  }
}

void LoadAutomatonConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.automaton().vars());

  const int num_transitions = ct.automaton().transition_tail_size();
  std::vector<std::vector<int64_t>> transitions;
  transitions.reserve(num_transitions);
  for (int i = 0; i < num_transitions; ++i) {
    transitions.push_back({ct.automaton().transition_tail(i),
                           ct.automaton().transition_label(i),
                           ct.automaton().transition_head(i)});
  }

  const int64_t starting_state = ct.automaton().starting_state();
  const std::vector<int64_t> final_states =
      ValuesFromProto(ct.automaton().final_states());
  m->Add(TransitionConstraint(vars, transitions, starting_state, final_states));
}

// From vector of n IntegerVariables, returns an n x n matrix of Literal
// such that matrix[i][j] is the Literal corresponding to vars[i] == j.
std::vector<std::vector<Literal>> GetSquareMatrixFromIntegerVariables(
    const std::vector<IntegerVariable>& vars, Model* m) {
  const int n = vars.size();
  const Literal kTrueLiteral =
      m->GetOrCreate<IntegerEncoder>()->GetTrueLiteral();
  const Literal kFalseLiteral =
      m->GetOrCreate<IntegerEncoder>()->GetFalseLiteral();
  std::vector<std::vector<Literal>> matrix(
      n, std::vector<Literal>(n, kFalseLiteral));
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (m->Get(IsFixed(vars[i]))) {
        const int value = m->Get(Value(vars[i]));
        DCHECK_LE(0, value);
        DCHECK_LT(value, n);
        matrix[i][value] = kTrueLiteral;
      } else {
        const auto encoding = m->Add(FullyEncodeVariable(vars[i]));
        for (const auto& entry : encoding) {
          const int value = entry.value.value();
          DCHECK_LE(0, value);
          DCHECK_LT(value, n);
          matrix[i][value] = entry.literal;
        }
      }
    }
  }
  return matrix;
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
    case ConstraintProto::ConstraintProto::kIntMin:
      LoadIntMinConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kLinMax:
      LoadLinMaxConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntMax:
      LoadIntMaxConstraint(ct, m);
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
    case ConstraintProto::ConstraintProto::kReservoir:
      LoadReservoirConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kElement:
      LoadElementConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kTable:
      LoadTableConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kAutomaton:
      LoadAutomatonConstraint(ct, m);
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
