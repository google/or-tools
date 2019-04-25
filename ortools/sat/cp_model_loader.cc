// Copyright 2010-2018 Google LLC
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
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/diffn.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/table.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

namespace {

template <typename Values>
std::vector<int64> ValuesFromProto(const Values& values) {
  return std::vector<int64>(values.begin(), values.end());
}

}  // namespace

void CpModelMapping::CreateVariables(const CpModelProto& model_proto,
                                     bool view_all_booleans_as_integers,
                                     Model* m) {
  const int num_proto_variables = model_proto.variables_size();

  // All [0, 1] variables always have a corresponding Boolean, even if it is
  // fixed to 0 (domain == [0,0]) or fixed to 1 (domain == [1,1]).
  {
    auto* sat_solver = m->GetOrCreate<SatSolver>();
    CHECK_EQ(sat_solver->NumVariables(), 0);

    BooleanVariable new_var(0);
    std::vector<BooleanVariable> false_variables;
    std::vector<BooleanVariable> true_variables;

    booleans_.resize(num_proto_variables, kNoBooleanVariable);
    reverse_boolean_map_.resize(num_proto_variables, -1);
    for (int i = 0; i < num_proto_variables; ++i) {
      const auto& domain = model_proto.variables(i).domain();
      if (domain.size() != 2) continue;
      if (domain[0] >= 0 && domain[1] <= 1) {
        booleans_[i] = new_var;
        reverse_boolean_map_[new_var] = i;
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

    // Add the objectives and search heuristics variables that needs to be
    // referenceable as integer even if they are only used as Booleans.
    if (model_proto.has_objective()) {
      for (const int obj_var : model_proto.objective().vars()) {
        used_variables.insert(PositiveRef(obj_var));
      }
    }
    for (const DecisionStrategyProto& strategy :
         model_proto.search_strategy()) {
      for (const int var : strategy.variables()) {
        used_variables.insert(PositiveRef(var));
      }
    }

    // Make sure any unused variable, that is not already a Boolean is
    // considered "used".
    for (int i = 0; i < num_proto_variables; ++i) {
      if (booleans_[i] == kNoBooleanVariable) {
        used_variables.insert(i);
      }
    }

    // We want the variable in the problem order.
    var_to_instantiate_as_integer.assign(used_variables.begin(),
                                         used_variables.end());
    gtl::STLSortAndRemoveDuplicates(&var_to_instantiate_as_integer);
  }
  integers_.resize(num_proto_variables, kNoIntegerVariable);

  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  for (const int i : var_to_instantiate_as_integer) {
    const auto& var_proto = model_proto.variables(i);
    integers_[i] =
        integer_trail->AddIntegerVariable(ReadDomainFromProto(var_proto));
    if (integers_[i] >= reverse_integer_map_.size()) {
      reverse_integer_map_.resize(integers_[i].value() + 1, -1);
    }
    reverse_integer_map_[integers_[i]] = i;
  }

  // Link any variable that has both views.
  for (int i = 0; i < num_proto_variables; ++i) {
    if (integers_[i] == kNoIntegerVariable) continue;
    if (booleans_[i] == kNoBooleanVariable) continue;

    // Associate with corresponding integer variable.
    m->GetOrCreate<IntegerEncoder>()->AssociateToIntegerEqualValue(
        sat::Literal(booleans_[i], true), integers_[i], IntegerValue(1));

    // This is needed so that IsFullyEncoded() returns true.
    m->GetOrCreate<IntegerEncoder>()->FullyEncodeVariable(integers_[i]);
  }

  // Create the interval variables.
  intervals_.resize(model_proto.constraints_size(), kNoIntervalVariable);
  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kInterval) {
      continue;
    }
    if (HasEnforcementLiteral(ct)) {
      const sat::Literal enforcement_literal =
          Literal(ct.enforcement_literal(0));
      // TODO(user): Fix the constant variable situation. An optional interval
      // with constant start/end or size cannot share the same constant
      // variable if it is used in non-optional situation.
      intervals_[c] = m->Add(NewOptionalInterval(
          Integer(ct.interval().start()), Integer(ct.interval().end()),
          Integer(ct.interval().size()), enforcement_literal));
    } else {
      intervals_[c] = m->Add(NewInterval(Integer(ct.interval().start()),
                                         Integer(ct.interval().end()),
                                         Integer(ct.interval().size())));
    }
    already_loaded_ct_.insert(&ct);
  }
}

// The logic assumes that the linear constraints have been presolved, so that
// equality with a domain bound have been converted to <= or >= and so that we
// never have any trivial inequalities.
void CpModelMapping::ExtractEncoding(const CpModelProto& model_proto,
                                     Model* m) {
  IntegerEncoder* encoder = m->GetOrCreate<IntegerEncoder>();
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();

  // Detection of literal equivalent to (i_var == value). We collect all the
  // half-reified constraint lit => equality or lit => inequality for a given
  // variable, and we will later sort them to detect equivalence.
  struct EqualityDetectionHelper {
    const ConstraintProto* ct;
    sat::Literal literal;
    int64 value;
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

  // Loop over all contraints and fill var_to_equalities and inequalities.
  for (const ConstraintProto& ct : model_proto.constraints()) {
    // For now, we only look at linear constraints with one term and one
    // enforcement literal.
    if (ct.enforcement_literal().size() != 1) continue;
    if (ct.linear().vars_size() != 1) continue;
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
      continue;
    }

    const sat::Literal enforcement_literal = Literal(ct.enforcement_literal(0));
    const int ref = ct.linear().vars(0);
    const int var = PositiveRef(ref);

    const Domain domain = ReadDomainFromProto(model_proto.variables(var));
    const Domain domain_if_enforced =
        ReadDomainFromProto(ct.linear())
            .InverseMultiplicationBy(ct.linear().coeffs(0) *
                                     (RefIsPositive(ref) ? 1 : -1));

    // Detect enforcement_literal => (var >= value or var <= value).
    if (domain_if_enforced.NumIntervals() == 1) {
      if (domain_if_enforced.Max() >= domain.Max() &&
          domain_if_enforced.Min() > domain.Min()) {
        inequalities.push_back(
            {&ct, enforcement_literal,
             IntegerLiteral::GreaterOrEqual(
                 Integer(var), IntegerValue(domain_if_enforced.Min()))});
      } else if (domain_if_enforced.Min() <= domain.Min() &&
                 domain_if_enforced.Max() < domain.Max()) {
        inequalities.push_back(
            {&ct, enforcement_literal,
             IntegerLiteral::LowerOrEqual(
                 Integer(var), IntegerValue(domain_if_enforced.Max()))});
      }
    }

    // Detect enforcement_literal => (var == value or var != value).
    //
    // Note that for domain with 2 values like [0, 1], we will detect both == 0
    // and != 1. Similarly, for a domain in [min, max], we should both detect
    // (== min) and (<= min), and both detect (== max) and (>= max).
    {
      const Domain inter = domain.IntersectionWith(domain_if_enforced);
      if (!inter.IsEmpty() && inter.Min() == inter.Max()) {
        var_to_equalities[var].push_back(
            {&ct, enforcement_literal, inter.Min(), true});
      }
    }
    {
      const Domain inter =
          domain.IntersectionWith(domain_if_enforced.Complement());
      if (!inter.IsEmpty() && inter.Min() == inter.Max()) {
        var_to_equalities[var].push_back(
            {&ct, enforcement_literal, inter.Min(), false});
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
      already_loaded_ct_.insert(inequalities[i].ct);
      already_loaded_ct_.insert(inequalities[i + 1].ct);
    }
  }

  // Encode the half-inequalities.
  int num_half_inequalities = 0;
  for (const auto inequality : inequalities) {
    if (ConstraintIsAlreadyLoaded(inequality.ct)) continue;
    m->Add(
        Implication(inequality.literal,
                    encoder->GetOrCreateAssociatedLiteral(inequality.i_lit)));

    ++num_half_inequalities;
    already_loaded_ct_.insert(inequality.ct);
    is_half_encoding_ct_.insert(inequality.ct);
  }

  if (!inequalities.empty()) {
    VLOG(1) << num_inequalities << " literals associated to VAR >= value, and "
            << num_half_inequalities << " half-associations.";
  }

  // Detect Literal <=> X == value and fully encoded variables.
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

    absl::flat_hash_set<int64> values;
    for (int j = 0; j + 1 < encoding.size(); j++) {
      if ((encoding[j].value != encoding[j + 1].value) ||
          (encoding[j].literal != encoding[j + 1].literal.Negated()) ||
          (encoding[j].is_equality != true) ||
          (encoding[j + 1].is_equality != false)) {
        continue;
      }

      ++num_equalities;
      encoder->AssociateToIntegerEqualValue(encoding[j].literal, integers_[i],
                                            IntegerValue(encoding[j].value));
      already_loaded_ct_.insert(encoding[j].ct);
      already_loaded_ct_.insert(encoding[j + 1].ct);
      values.insert(encoding[j].value);
    }

    // Encode the half-equalities.
    for (const auto equality : encoding) {
      if (ConstraintIsAlreadyLoaded(equality.ct)) continue;
      const class Literal eq = encoder->GetOrCreateLiteralAssociatedToEquality(
          integers_[i], IntegerValue(equality.value));
      if (equality.is_equality) {
        m->Add(Implication(equality.literal, eq));
      } else {
        m->Add(Implication(equality.literal, eq.Negated()));
      }

      ++num_half_equalities;
      already_loaded_ct_.insert(equality.ct);
      is_half_encoding_ct_.insert(equality.ct);
    }

    // Detect fully encoded variables and mark them as such.
    //
    // TODO(user): Also fully encode variable that are almost fully encoded?
    const Domain domain = ReadDomainFromProto(model_proto.variables(i));
    if (domain.Size() == values.size()) {
      ++num_fully_encoded;
      if (!encoder->VariableIsFullyEncoded(integers_[i])) {
        encoder->FullyEncodeVariable(integers_[i]);
      }
    } else {
      ++num_partially_encoded;
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

void CpModelMapping::DetectOptionalVariables(const CpModelProto& model_proto,
                                             Model* m) {
  const SatParameters& parameters = *(m->GetOrCreate<SatParameters>());
  if (!parameters.use_optional_variables()) return;
  if (parameters.enumerate_all_solutions()) return;

  // Compute for each variables the intersection of the enforcement literals
  // of the constraints in which they appear.
  //
  // TODO(user): This deals with the simplest cases, but we could try to
  // detect literals that implies all the constaints in which a variable
  // appear to false. This can be done with a LCA computation in the tree of
  // Boolean implication (once the presolve remove cycles). Not sure if we can
  // properly exploit that afterwards though. Do some research!
  const int num_proto_variables = model_proto.variables_size();
  std::vector<bool> already_seen(num_proto_variables, false);
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
    const int64 min = var_proto.domain(0);
    const int64 max = var_proto.domain(var_proto.domain().size() - 1);
    if (min == max) continue;
    if (min == 0 && max == 1) continue;
    if (enforcement_intersection[var].empty()) continue;

    ++num_optionals;
    integer_trail->MarkIntegerVariableAsOptional(
        Integer(var), Literal(enforcement_intersection[var].front()));
  }
  VLOG(2) << "Auto-detected " << num_optionals << " optional variables.";
}

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

}  // namespace

void LoadLinearConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.linear().vars());
  const std::vector<int64> coeffs = ValuesFromProto(ct.linear().coeffs());

  const SatParameters& params = *m->GetOrCreate<SatParameters>();
  if (params.boolean_encoding_level() > 0 && vars.size() == 2 &&
      ct.linear().domain_size() == 2 &&
      ct.linear().domain(0) == ct.linear().domain(1)) {
    auto* encoder = m->GetOrCreate<IntegerEncoder>();
    if (encoder->VariableIsFullyEncoded(vars[0]) &&
        encoder->VariableIsFullyEncoded(vars[1])) {
      return LoadEquivalenceAC(mapping->Literals(ct.enforcement_literal()),
                               IntegerValue(coeffs[0]), vars[0],
                               IntegerValue(coeffs[1]), vars[1],
                               IntegerValue(ct.linear().domain(0)), m);
    }
  }

  // Compute the min/max to relax the bounds if needed.
  IntegerValue min_sum(0);
  IntegerValue max_sum(0);
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  bool all_booleans = true;
  for (int i = 0; i < vars.size(); ++i) {
    if (all_booleans && !mapping->IsBoolean(ct.linear().vars(i))) {
      all_booleans = false;
    }
    const IntegerValue term_a = coeffs[i] * integer_trail->LowerBound(vars[i]);
    const IntegerValue term_b = coeffs[i] * integer_trail->UpperBound(vars[i]);
    min_sum += std::min(term_a, term_b);
    max_sum += std::max(term_a, term_b);
  }

  if (ct.linear().domain_size() == 2) {
    int64 lb = ct.linear().domain(0);
    int64 ub = ct.linear().domain(1);
    if (min_sum >= lb) lb = kint64min;
    if (max_sum <= ub) ub = kint64max;

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
        if (lb != kint64min) {
          m->Add(WeightedSumGreaterOrEqual(vars, coeffs, lb));
        }
        if (ub != kint64max) {
          m->Add(WeightedSumLowerOrEqual(vars, coeffs, ub));
        }
      }
    } else {
      const std::vector<Literal> enforcement_literals =
          mapping->Literals(ct.enforcement_literal());
      if (lb != kint64min) {
        m->Add(ConditionalWeightedSumGreaterOrEqual(enforcement_literals, vars,
                                                    coeffs, lb));
      }
      if (ub != kint64max) {
        m->Add(ConditionalWeightedSumLowerOrEqual(enforcement_literals, vars,
                                                  coeffs, ub));
      }
    }
  } else {
    std::vector<Literal> clause;
    for (int i = 0; i < ct.linear().domain_size(); i += 2) {
      int64 lb = ct.linear().domain(i);
      int64 ub = ct.linear().domain(i + 1);
      if (min_sum >= lb) lb = kint64min;
      if (max_sum <= ub) ub = kint64max;

      const Literal subdomain_literal(m->Add(NewBooleanVariable()), true);
      clause.push_back(subdomain_literal);
      if (lb != kint64min) {
        m->Add(ConditionalWeightedSumGreaterOrEqual({subdomain_literal}, vars,
                                                    coeffs, lb));
      }
      if (ub != kint64max) {
        m->Add(ConditionalWeightedSumLowerOrEqual({subdomain_literal}, vars,
                                                  coeffs, ub));
      }
    }
    for (const int ref : ct.enforcement_literal()) {
      clause.push_back(mapping->Literal(ref).Negated());
    }

    // TODO(user): In the cases where this clause only contains two literals,
    // then we could have only used one literal and its negation above.
    m->Add(ClauseConstraint(clause));
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
  int64 max_domain_size = 0;
  for (const IntegerVariable variable : vars) {
    if (encoder->VariableIsFullyEncoded(variable)) num_fully_encoded++;

    IntegerValue lb = integer_trail->LowerBound(variable);
    IntegerValue ub = integer_trail->UpperBound(variable);
    int64 domain_size = ub.value() - lb.value();
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

void LoadIntMaxConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const IntegerVariable max = mapping->Integer(ct.int_max().target());

  if (ct.int_max().vars_size() == 2 &&
      NegatedRef(ct.int_max().vars(0)) == ct.int_max().vars(1)) {
    const IntegerVariable var =
        mapping->Integer(PositiveRef(ct.int_max().vars(0)));
    m->Add(IsEqualToAbsOf(max, var));
    return;
  }

  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.int_max().vars());
  m->Add(IsEqualToMaxOf(max, vars));
}

void LoadNoOverlapConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  m->Add(Disjunctive(mapping->Intervals(ct.no_overlap().intervals())));
}

void LoadNoOverlap2dConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntervalVariable> x_intervals =
      mapping->Intervals(ct.no_overlap_2d().x_intervals());
  const std::vector<IntervalVariable> y_intervals =
      mapping->Intervals(ct.no_overlap_2d().y_intervals());
  m->Add(
      NonOverlappingRectangles(x_intervals, y_intervals, /*is_strict=*/true));
}

void LoadCumulativeConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  const IntegerVariable capacity = mapping->Integer(ct.cumulative().capacity());
  const std::vector<IntegerVariable> demands =
      mapping->Integers(ct.cumulative().demands());
  m->Add(Cumulative(intervals, demands, capacity));
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
      const int64 value = m->Get(Value(target));
      m->Add(ImpliesInInterval(r, vars[i], value, value));
    } else if (m->Get(IsFixed(vars[i]))) {
      const int64 value = m->Get(Value(vars[i]));
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
  const std::vector<int64> values = ValuesFromProto(ct.table().values());
  const int num_vars = vars.size();
  const int num_tuples = values.size() / num_vars;
  std::vector<std::vector<int64>> tuples(num_tuples);
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
  std::vector<std::vector<int64>> transitions;
  transitions.reserve(num_transitions);
  for (int i = 0; i < num_transitions; ++i) {
    transitions.push_back({ct.automaton().transition_tail(i),
                           ct.automaton().transition_label(i),
                           ct.automaton().transition_head(i)});
  }

  const int64 starting_state = ct.automaton().starting_state();
  const std::vector<int64> final_states =
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
  const int num_nodes = ReindexArcs(&tails, &heads, &literals);
  m->Add(SubcircuitConstraint(num_nodes, tails, heads, literals));
}

void LoadRoutesConstraint(const ConstraintProto& ct, Model* m) {
  const auto& routes = ct.routes();
  if (routes.tails().empty()) return;

  std::vector<int> tails(routes.tails().begin(), routes.tails().end());
  std::vector<int> heads(routes.heads().begin(), routes.heads().end());
  std::vector<Literal> literals =
      m->GetOrCreate<CpModelMapping>()->Literals(routes.literals());
  const int num_nodes = ReindexArcs(&tails, &heads, &literals);
  m->Add(SubcircuitConstraint(num_nodes, tails, heads, literals,
                              /*multiple_subcircuit_through_zero=*/true));
}

void LoadCircuitCoveringConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const std::vector<IntegerVariable> nexts =
      mapping->Integers(ct.circuit_covering().nexts());
  const std::vector<std::vector<Literal>> graph =
      GetSquareMatrixFromIntegerVariables(nexts, m);
  const std::vector<int> distinguished(
      ct.circuit_covering().distinguished_nodes().begin(),
      ct.circuit_covering().distinguished_nodes().end());
  m->Add(ExactlyOnePerRowAndPerColumn(graph));
  m->Add(CircuitCovering(graph, distinguished));
}

void LoadInverseConstraint(const ConstraintProto& ct, Model* m) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();

  // Fully encode both arrays of variables, encode the constraint using Boolean
  // equalities: f_direct[i] == j <=> f_inverse[j] == i.
  const int num_variables = ct.inverse().f_direct_size();
  CHECK_EQ(num_variables, ct.inverse().f_inverse_size());
  const std::vector<IntegerVariable> direct =
      mapping->Integers(ct.inverse().f_direct());
  const std::vector<IntegerVariable> inverse =
      mapping->Integers(ct.inverse().f_inverse());

  // Fill LiteralIndex matrices.
  std::vector<std::vector<LiteralIndex>> matrix_direct(
      num_variables,
      std::vector<LiteralIndex>(num_variables, kFalseLiteralIndex));

  std::vector<std::vector<LiteralIndex>> matrix_inverse(
      num_variables,
      std::vector<LiteralIndex>(num_variables, kFalseLiteralIndex));

  auto fill_matrix = [&m](std::vector<std::vector<LiteralIndex>>& matrix,
                          const std::vector<IntegerVariable>& variables) {
    const int num_variables = variables.size();
    for (int i = 0; i < num_variables; i++) {
      if (m->Get(IsFixed(variables[i]))) {
        matrix[i][m->Get(Value(variables[i]))] = kTrueLiteralIndex;
      } else {
        const auto encoding = m->Add(FullyEncodeVariable(variables[i]));
        for (const auto literal_value : encoding) {
          matrix[i][literal_value.value.value()] =
              literal_value.literal.Index();
        }
      }
    }
  };

  fill_matrix(matrix_direct, direct);
  fill_matrix(matrix_inverse, inverse);

  // matrix_direct should be the transpose of matrix_inverse.
  for (int i = 0; i < num_variables; i++) {
    for (int j = 0; j < num_variables; j++) {
      LiteralIndex l_ij = matrix_direct[i][j];
      LiteralIndex l_ji = matrix_inverse[j][i];
      if (l_ij >= 0 && l_ji >= 0) {
        // l_ij <=> l_ji.
        m->Add(ClauseConstraint({Literal(l_ij), Literal(l_ji).Negated()}));
        m->Add(ClauseConstraint({Literal(l_ij).Negated(), Literal(l_ji)}));
      } else if (l_ij < 0 && l_ji < 0) {
        // Problem infeasible if l_ij != l_ji, otherwise nothing to add.
        if (l_ij != l_ji) {
          m->Add(ClauseConstraint({}));
          return;
        }
      } else {
        // One of the LiteralIndex is fixed, let it be l_ij.
        if (l_ij > l_ji) std::swap(l_ij, l_ji);
        const Literal lit = Literal(l_ji);
        m->Add(ClauseConstraint(
            {l_ij == kFalseLiteralIndex ? lit.Negated() : lit}));
      }
    }
  }
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
    case ConstraintProto::ConstraintProto::kCircuitCovering:
      LoadCircuitCoveringConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kInverse:
      LoadInverseConstraint(ct, m);
      return true;
    default:
      return false;
  }
}

}  // namespace sat
}  // namespace operations_research
