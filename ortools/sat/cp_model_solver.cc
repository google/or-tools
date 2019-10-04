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

#include "ortools/sat/cp_model_solver.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#if !defined(__PORTABLE_PLATFORM__)
#include "absl/synchronization/notification.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/file.h"
#include "ortools/util/sigint.h"
#endif  // __PORTABLE_PLATFORM__

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "glog/vlog_is_on.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status.h"
#include "ortools/base/threadpool.h"
#include "ortools/base/timer.h"
#include "ortools/graph/connectivity.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_lns.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/drat_proof_handler.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

DEFINE_string(cp_model_dump_file, "",
              "DEBUG ONLY. When this is set to a non-empty file name, "
              "SolveCpModel() will dump its model to this file. Note that the "
              "file will be ovewritten with the last such model. "
              "TODO(fdid): dump all model to a recordio file instead?");
DEFINE_string(cp_model_params, "",
              "This is interpreted as a text SatParameters proto. The "
              "specified fields will override the normal ones for all solves.");
DEFINE_bool(cp_model_dump_lns, false,
            "Useful to debug presolve issues on LNS fragments");

DEFINE_string(
    drat_output, "",
    "If non-empty, a proof in DRAT format will be written to this file. "
    "This will only be used for pure-SAT problems.");

DEFINE_bool(drat_check, false,
            "If true, a proof in DRAT format will be stored in memory and "
            "checked if the problem is UNSAT. This will only be used for "
            "pure-SAT problems.");

DEFINE_double(max_drat_time_in_seconds, std::numeric_limits<double>::infinity(),
              "Maximum time in seconds to check the DRAT proof. This will only "
              "be used is the drat_check flag is enabled.");

DEFINE_bool(cp_model_check_intermediate_solutions, false,
            "When true, all intermediate solutions found by the solver will be "
            "checked. This can be expensive, therefore it is off by default.");

namespace operations_research {
namespace sat {

namespace {

// Makes the std::string fit in one line by cutting it in the middle if
// necessary.
std::string Summarize(const std::string& input) {
  if (input.size() < 105) return input;
  const int half = 50;
  return absl::StrCat(input.substr(0, half), " ... ",
                      input.substr(input.size() - half, half));
}

}  // namespace.

// =============================================================================
// Public API.
// =============================================================================

std::string CpModelStats(const CpModelProto& model_proto) {
  std::map<std::string, int> num_constraints_by_name;
  std::map<std::string, int> num_reif_constraints_by_name;
  std::map<std::string, int> name_to_num_literals;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    std::string name = ConstraintCaseName(ct.constraint_case());

    // We split the linear constraints into 3 buckets has it gives more insight
    // on the type of problem we are facing.
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
      if (ct.linear().vars_size() == 1) name += "1";
      if (ct.linear().vars_size() == 2) name += "2";
      if (ct.linear().vars_size() == 3) name += "3";
      if (ct.linear().vars_size() > 3) name += "N";
    }

    num_constraints_by_name[name]++;
    if (!ct.enforcement_literal().empty()) {
      num_reif_constraints_by_name[name]++;
    }

    // For pure Boolean constraints, we also display the total number of literal
    // involved as this gives a good idea of the problem size.
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
      name_to_num_literals[name] += ct.bool_or().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kBoolAnd) {
      name_to_num_literals[name] += ct.bool_and().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kAtMostOne) {
      name_to_num_literals[name] += ct.at_most_one().literals().size();
    }
  }

  int num_constants = 0;
  std::set<int64> constant_values;
  std::map<Domain, int> num_vars_per_domains;
  for (const IntegerVariableProto& var : model_proto.variables()) {
    if (var.domain_size() == 2 && var.domain(0) == var.domain(1)) {
      ++num_constants;
      constant_values.insert(var.domain(0));
    } else {
      num_vars_per_domains[ReadDomainFromProto(var)]++;
    }
  }

  std::string result;
  if (model_proto.has_objective()) {
    absl::StrAppend(&result, "Optimization model '", model_proto.name(),
                    "':\n");
  } else {
    absl::StrAppend(&result, "Satisfaction model '", model_proto.name(),
                    "':\n");
  }

  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    absl::StrAppend(
        &result, "Search strategy: on ", strategy.variables_size(),
        " variables, ",
        ProtoEnumToString<DecisionStrategyProto::VariableSelectionStrategy>(
            strategy.variable_selection_strategy()),
        ", ",
        ProtoEnumToString<DecisionStrategyProto::DomainReductionStrategy>(
            strategy.domain_reduction_strategy()),
        "\n");
  }

  const std::string objective_string =
      model_proto.has_objective()
          ? absl::StrCat(" (", model_proto.objective().vars_size(),
                         " in objective)")
          : "";
  absl::StrAppend(&result, "#Variables: ", model_proto.variables_size(),
                  objective_string, "\n");
  if (num_vars_per_domains.size() < 100) {
    for (const auto& entry : num_vars_per_domains) {
      const std::string temp = absl::StrCat(" - ", entry.second, " in ",
                                            entry.first.ToString(), "\n");
      absl::StrAppend(&result, Summarize(temp));
    }
  } else {
    int64 max_complexity = 0;
    int64 min = kint64max;
    int64 max = kint64min;
    for (const auto& entry : num_vars_per_domains) {
      min = std::min(min, entry.first.Min());
      max = std::max(max, entry.first.Max());
      max_complexity = std::max(max_complexity,
                                static_cast<int64>(entry.first.NumIntervals()));
    }
    absl::StrAppend(&result, " - ", num_vars_per_domains.size(),
                    " different domains in [", min, ",", max,
                    "] with a largest complexity of ", max_complexity, ".\n");
  }

  if (num_constants > 0) {
    const std::string temp =
        absl::StrCat(" - ", num_constants, " constants in {",
                     absl::StrJoin(constant_values, ","), "} \n");
    absl::StrAppend(&result, Summarize(temp));
  }

  std::vector<std::string> constraints;
  constraints.reserve(num_constraints_by_name.size());
  for (const auto entry : num_constraints_by_name) {
    const std::string& name = entry.first;
    constraints.push_back(absl::StrCat("#", name, ": ", entry.second));
    if (gtl::ContainsKey(num_reif_constraints_by_name, name)) {
      absl::StrAppend(&constraints.back(),
                      " (#enforced: ", num_reif_constraints_by_name[name], ")");
    }
    if (gtl::ContainsKey(name_to_num_literals, name)) {
      absl::StrAppend(&constraints.back(),
                      " (#literals: ", name_to_num_literals[name], ")");
    }
  }
  std::sort(constraints.begin(), constraints.end());
  absl::StrAppend(&result, absl::StrJoin(constraints, "\n"));

  return result;
}

std::string CpSolverResponseStats(const CpSolverResponse& response) {
  std::string result;
  absl::StrAppend(&result, "CpSolverResponse:");
  absl::StrAppend(&result, "\nstatus: ",
                  ProtoEnumToString<CpSolverStatus>(response.status()));

  // We special case the pure-decision problem for clarity.
  //
  // TODO(user): This test is not ideal for the corner case where the status is
  // still UNKNOWN yet we already know that if there is a solution, then its
  // objective is zero...
  if (response.status() == CpSolverStatus::INFEASIBLE ||
      (response.status() != CpSolverStatus::OPTIMAL &&
       response.objective_value() == 0 &&
       response.best_objective_bound() == 0)) {
    absl::StrAppend(&result, "\nobjective: NA");
    absl::StrAppend(&result, "\nbest_bound: NA");
  } else {
    absl::StrAppendFormat(&result, "\nobjective: %.9g",
                          response.objective_value());
    absl::StrAppendFormat(&result, "\nbest_bound: %.9g",
                          response.best_objective_bound());
  }

  absl::StrAppend(&result, "\nbooleans: ", response.num_booleans());
  absl::StrAppend(&result, "\nconflicts: ", response.num_conflicts());
  absl::StrAppend(&result, "\nbranches: ", response.num_branches());

  // TODO(user): This is probably better named "binary_propagation", but we just
  // output "propagations" to be consistent with sat/analyze.sh.
  absl::StrAppend(&result,
                  "\npropagations: ", response.num_binary_propagations());
  absl::StrAppend(
      &result, "\ninteger_propagations: ", response.num_integer_propagations());
  absl::StrAppend(&result, "\nwalltime: ", response.wall_time());
  absl::StrAppend(&result, "\nusertime: ", response.user_time());
  absl::StrAppend(&result,
                  "\ndeterministic_time: ", response.deterministic_time());
  absl::StrAppend(&result, "\nprimal_integral: ", response.primal_integral());
  absl::StrAppend(&result, "\n");
  return result;
}

namespace {

void FillSolutionInResponse(const CpModelProto& model_proto, const Model& model,
                            CpSolverResponse* response) {
  response->clear_solution();
  response->clear_solution_lower_bounds();
  response->clear_solution_upper_bounds();

  auto* mapping = model.Get<CpModelMapping>();
  auto* trail = model.Get<Trail>();
  auto* integer_trail = model.Get<IntegerTrail>();

  std::vector<int64> solution;
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsInteger(i)) {
      const IntegerVariable var = mapping->Integer(i);
      if (integer_trail->IsCurrentlyIgnored(var)) {
        // This variable is "ignored" so it may not be fixed, simply use
        // the current lower bound. Any value in its domain should lead to
        // a feasible solution.
        solution.push_back(model.Get(LowerBound(var)));
      } else {
        if (model.Get(LowerBound(var)) != model.Get(UpperBound(var))) {
          solution.clear();
          break;
        }
        solution.push_back(model.Get(Value(var)));
      }
    } else {
      DCHECK(mapping->IsBoolean(i));
      const Literal literal = mapping->Literal(i);
      if (trail->Assignment().LiteralIsAssigned(literal)) {
        solution.push_back(model.Get(Value(literal)));
      } else {
        solution.clear();
        break;
      }
    }
  }

  if (!solution.empty()) {
    if (DEBUG_MODE || FLAGS_cp_model_check_intermediate_solutions) {
      // TODO(user): Checks against initial model.
      CHECK(SolutionIsFeasible(model_proto, solution));
    }
    for (const int64 value : solution) response->add_solution(value);
  } else {
    // Not all variables are fixed.
    // We fill instead the lb/ub of each variables.
    const auto& assignment = trail->Assignment();
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      if (mapping->IsBoolean(i)) {
        if (assignment.VariableIsAssigned(mapping->Literal(i).Variable())) {
          const int value = model.Get(Value(mapping->Literal(i)));
          response->add_solution_lower_bounds(value);
          response->add_solution_upper_bounds(value);
        } else {
          response->add_solution_lower_bounds(0);
          response->add_solution_upper_bounds(1);
        }
      } else {
        response->add_solution_lower_bounds(
            model.Get(LowerBound(mapping->Integer(i))));
        response->add_solution_upper_bounds(
            model.Get(UpperBound(mapping->Integer(i))));
      }
    }
  }
}

namespace {

IntegerVariable GetOrCreateVariableWithTightBound(
    const std::vector<std::pair<IntegerVariable, int64>>& terms, Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  int64 sum_min = 0;
  int64 sum_max = 0;
  for (const std::pair<IntegerVariable, int64> var_coeff : terms) {
    const int64 min_domain = model->Get(LowerBound(var_coeff.first));
    const int64 max_domain = model->Get(UpperBound(var_coeff.first));
    const int64 coeff = var_coeff.second;
    const int64 prod1 = min_domain * coeff;
    const int64 prod2 = max_domain * coeff;
    sum_min += std::min(prod1, prod2);
    sum_max += std::max(prod1, prod2);
  }
  return model->Add(NewIntegerVariable(sum_min, sum_max));
}

IntegerVariable GetOrCreateVariableGreaterOrEqualToSumOf(
    const std::vector<std::pair<IntegerVariable, int64>>& terms, Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  // Create a new variable and link it with the linear terms.
  const IntegerVariable new_var =
      GetOrCreateVariableWithTightBound(terms, model);
  std::vector<IntegerVariable> vars;
  std::vector<int64> coeffs;
  for (const auto& term : terms) {
    vars.push_back(term.first);
    coeffs.push_back(term.second);
  }
  vars.push_back(new_var);
  coeffs.push_back(-1);
  model->Add(WeightedSumLowerOrEqual(vars, coeffs, 0));
  return new_var;
}

void TryToAddCutGenerators(const CpModelProto& model_proto,
                           const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation) {
  const int linearization_level =
      m->GetOrCreate<SatParameters>()->linearization_level();
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kCircuit &&
      linearization_level > 1) {
    std::vector<int> tails(ct.circuit().tails().begin(),
                           ct.circuit().tails().end());
    std::vector<int> heads(ct.circuit().heads().begin(),
                           ct.circuit().heads().end());
    std::vector<Literal> literals = mapping->Literals(ct.circuit().literals());
    const int num_nodes = ReindexArcs(&tails, &heads, &literals);

    relaxation->cut_generators.push_back(
        CreateStronglyConnectedGraphCutGenerator(num_nodes, tails, heads,
                                                 literals, m));
  }
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kRoutes &&
      linearization_level > 1) {
    std::vector<int> tails(ct.routes().tails().begin(),
                           ct.routes().tails().end());
    std::vector<int> heads(ct.routes().heads().begin(),
                           ct.routes().heads().end());
    std::vector<Literal> literals = mapping->Literals(ct.routes().literals());

    int num_nodes = 0;
    for (int i = 0; i < ct.routes().tails_size(); ++i) {
      num_nodes = std::max(num_nodes, 1 + ct.routes().tails(i));
      num_nodes = std::max(num_nodes, 1 + ct.routes().heads(i));
    }
    if (ct.routes().demands().empty() || ct.routes().capacity() == 0) {
      relaxation->cut_generators.push_back(
          CreateStronglyConnectedGraphCutGenerator(num_nodes, tails, heads,
                                                   literals, m));
    } else {
      const std::vector<int64> demands(ct.routes().demands().begin(),
                                       ct.routes().demands().end());
      relaxation->cut_generators.push_back(
          CreateCVRPCutGenerator(num_nodes, tails, heads, literals, demands,
                                 ct.routes().capacity(), m));
    }
  }
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntProd) {
    if (HasEnforcementLiteral(ct)) return;
    if (ct.int_prod().vars_size() != 2) return;

    // Constraint is z == x * y.

    IntegerVariable z = mapping->Integer(ct.int_prod().target());
    IntegerVariable x = mapping->Integer(ct.int_prod().vars(0));
    IntegerVariable y = mapping->Integer(ct.int_prod().vars(1));

    IntegerTrail* const integer_trail = m->GetOrCreate<IntegerTrail>();
    IntegerValue x_lb = integer_trail->LowerBound(x);
    IntegerValue x_ub = integer_trail->UpperBound(x);
    IntegerValue y_lb = integer_trail->LowerBound(y);
    IntegerValue y_ub = integer_trail->UpperBound(y);

    if (x == y) {
      // We currently only support variables with non-negative domains.
      if (x_lb < 0 && x_ub > 0) return;

      // Change the sigh of x if its domain is non-positive.
      if (x_ub <= 0) {
        x = NegationOf(x);
      }

      relaxation->cut_generators.push_back(CreateSquareCutGenerator(z, x, m));
    } else {
      // We currently only support variables with non-negative domains.
      if (x_lb < 0 && x_ub > 0) return;
      if (y_lb < 0 && y_ub > 0) return;

      // Change signs to return to the case where all variables are a domain
      // with non negative values only.
      if (x_ub <= 0) {
        x = NegationOf(x);
        z = NegationOf(z);
      }
      if (y_ub <= 0) {
        y = NegationOf(y);
        z = NegationOf(z);
      }

      relaxation->cut_generators.push_back(
          CreatePositiveMultiplicationCutGenerator(z, x, y, m));
    }
  }
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kAllDiff) {
    if (linearization_level < 2) return;
    if (HasEnforcementLiteral(ct)) return;
    const int num_vars = ct.all_diff().vars_size();
    if (num_vars <= m->GetOrCreate<SatParameters>()->max_all_diff_cut_size()) {
      std::vector<IntegerVariable> vars =
          mapping->Integers(ct.all_diff().vars());
      relaxation->cut_generators.push_back(
          CreateAllDifferentCutGenerator(vars, m));
    }
  }
}

}  // namespace

// Adds one LinearProgrammingConstraint per connected component of the model.
IntegerVariable AddLPConstraints(const CpModelProto& model_proto,
                                 int linearization_level, Model* m) {
  LinearRelaxation relaxation;

  // Linearize the constraints.
  absl::flat_hash_set<int> used_integer_variable;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  auto* trail = m->GetOrCreate<Trail>();
  for (const auto& ct : model_proto.constraints()) {
    // Make sure the literals from a circuit constraint always have a view.
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kCircuit) {
      for (const int ref : ct.circuit().literals()) {
        const Literal l = mapping->Literal(ref);
        if (encoder->GetLiteralView(l) == kNoIntegerVariable &&
            encoder->GetLiteralView(l.Negated()) == kNoIntegerVariable) {
          m->Add(NewIntegerVariableFromLiteral(l));
        }
      }
    }

    // For now, we skip any constraint with literals that do not have an integer
    // view. Ideally it should be up to the constraint to decide if creating a
    // view is worth it.
    //
    // TODO(user): It should be possible to speed this up if needed.
    const IndexReferences refs = GetReferencesUsedByConstraint(ct);
    bool ok = true;
    for (const int literal_ref : refs.literals) {
      const Literal literal = mapping->Literal(literal_ref);
      if (trail->Assignment().LiteralIsAssigned(literal)) {
        // Create a view to the constant 0 or 1.
        m->Add(NewIntegerVariableFromLiteral(literal));
      } else if (encoder->GetLiteralView(literal) == kNoIntegerVariable &&
                 encoder->GetLiteralView(literal.Negated()) ==
                     kNoIntegerVariable) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;

    TryToLinearizeConstraint(model_proto, ct, m, linearization_level,
                             &relaxation);
    TryToAddCutGenerators(model_proto, ct, m, &relaxation);
  }

  // Linearize the encoding of variable that are fully encoded in the proto.
  int num_full_encoding_relaxations = 0;
  int num_partial_encoding_relaxations = 0;
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsBoolean(i)) continue;

    const IntegerVariable var = mapping->Integer(i);
    if (m->Get(IsFixed(var))) continue;

    // TODO(user): This different encoding for the partial variable might be
    // better (less LP constraints), but we do need more investigation to
    // decide.
    if (/* DISABLES CODE */ (false)) {
      AppendPartialEncodingRelaxation(var, *m, &relaxation);
      continue;
    }

    if (encoder->VariableIsFullyEncoded(var)) {
      if (AppendFullEncodingRelaxation(var, *m, &relaxation)) {
        ++num_full_encoding_relaxations;
        continue;
      }
    }

    // Even if the variable is fully encoded, sometimes not all its associated
    // literal have a view (if they are not part of the original model for
    // instance).
    //
    // TODO(user): Should we add them to the LP anyway? this isn't clear as
    // we can sometimes create a lot of Booleans like this.
    const int old = relaxation.linear_constraints.size();
    AppendPartialGreaterThanEncodingRelaxation(var, *m, &relaxation);
    if (relaxation.linear_constraints.size() > old) {
      ++num_partial_encoding_relaxations;
    }
  }

  // Linearize the at most one constraints. Note that we transform them
  // into maximum "at most one" first and we removes redundant ones.
  m->GetOrCreate<BinaryImplicationGraph>()->TransformIntoMaxCliques(
      &relaxation.at_most_ones);
  for (const std::vector<Literal>& at_most_one : relaxation.at_most_ones) {
    if (at_most_one.empty()) continue;
    LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(1));
    for (const Literal literal : at_most_one) {
      // Note that it is okay to simply ignore the literal if it has no
      // integer view.
      const bool unused ABSL_ATTRIBUTE_UNUSED =
          lc.AddLiteralTerm(literal, IntegerValue(1));
    }
    relaxation.linear_constraints.push_back(lc.Build());
  }

  // Remove size one LP constraints, they are not useful.
  {
    int new_size = 0;
    for (int i = 0; i < relaxation.linear_constraints.size(); ++i) {
      if (relaxation.linear_constraints[i].vars.size() <= 1) continue;
      std::swap(relaxation.linear_constraints[new_size++],
                relaxation.linear_constraints[i]);
    }
    relaxation.linear_constraints.resize(new_size);
  }

  VLOG(3) << "num_full_encoding_relaxations: " << num_full_encoding_relaxations;
  VLOG(3) << "num_partial_encoding_relaxations: "
          << num_partial_encoding_relaxations;
  VLOG(3) << relaxation.linear_constraints.size()
          << " constraints in the LP relaxation.";
  VLOG(3) << relaxation.cut_generators.size() << " cuts generators.";

  // The bipartite graph of LP constraints might be disconnected:
  // make a partition of the variables into connected components.
  // Constraint nodes are indexed by [0..num_lp_constraints),
  // variable nodes by [num_lp_constraints..num_lp_constraints+num_variables).
  //
  // TODO(user): look into biconnected components.
  const int num_lp_constraints = relaxation.linear_constraints.size();
  const int num_lp_cut_generators = relaxation.cut_generators.size();
  const int num_integer_variables =
      m->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value();
  ConnectedComponents<int, int> components;
  components.Init(num_lp_constraints + num_lp_cut_generators +
                  num_integer_variables);
  auto get_constraint_index = [](int ct_index) { return ct_index; };
  auto get_cut_generator_index = [num_lp_constraints](int cut_index) {
    return num_lp_constraints + cut_index;
  };
  auto get_var_index = [num_lp_constraints,
                        num_lp_cut_generators](IntegerVariable var) {
    return num_lp_constraints + num_lp_cut_generators + var.value();
  };
  for (int i = 0; i < num_lp_constraints; i++) {
    for (const IntegerVariable var : relaxation.linear_constraints[i].vars) {
      components.AddArc(get_constraint_index(i), get_var_index(var));
    }
  }
  for (int i = 0; i < num_lp_cut_generators; ++i) {
    for (const IntegerVariable var : relaxation.cut_generators[i].vars) {
      components.AddArc(get_cut_generator_index(i), get_var_index(var));
    }
  }

  std::map<int, int> components_to_size;
  for (int i = 0; i < num_lp_constraints; i++) {
    const int id = components.GetClassRepresentative(get_constraint_index(i));
    components_to_size[id] += 1;
  }
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int id =
        components.GetClassRepresentative(get_cut_generator_index(i));
    components_to_size[id] += 1;
  }

  // Make sure any constraint that touch the objective is not discarded even
  // if it is the only one in its component. This is important to propagate
  // as much as possible the objective bound by using any bounds the LP give
  // us on one of its components. This is critical on the zephyrus problems for
  // instance.
  for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
    const IntegerVariable var =
        mapping->Integer(model_proto.objective().vars(i));
    const int id = components.GetClassRepresentative(get_var_index(var));
    components_to_size[id] += 1;
  }

  // Dispatch every constraint to its LinearProgrammingConstraint.
  std::map<int, LinearProgrammingConstraint*> representative_to_lp_constraint;
  std::vector<LinearProgrammingConstraint*> lp_constraints;
  std::map<int, std::vector<LinearConstraint>> id_to_constraints;
  for (int i = 0; i < num_lp_constraints; i++) {
    const int id = components.GetClassRepresentative(get_constraint_index(i));
    if (components_to_size[id] <= 1) continue;
    id_to_constraints[id].push_back(relaxation.linear_constraints[i]);
    if (!gtl::ContainsKey(representative_to_lp_constraint, id)) {
      auto* lp = m->Create<LinearProgrammingConstraint>();
      representative_to_lp_constraint[id] = lp;
      lp_constraints.push_back(lp);
    }

    // Load the constraint.
    gtl::FindOrDie(representative_to_lp_constraint, id)
        ->AddLinearConstraint(relaxation.linear_constraints[i]);
  }

  // Dispatch every cut generator to its LinearProgrammingConstraint.
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int id =
        components.GetClassRepresentative(get_cut_generator_index(i));
    if (!gtl::ContainsKey(representative_to_lp_constraint, id)) {
      auto* lp = m->Create<LinearProgrammingConstraint>();
      representative_to_lp_constraint[id] = lp;
      lp_constraints.push_back(lp);
    }
    LinearProgrammingConstraint* lp = representative_to_lp_constraint[id];
    lp->AddCutGenerator(std::move(relaxation.cut_generators[i]));
  }

  const SatParameters& params = *(m->GetOrCreate<SatParameters>());
  if (params.add_knapsack_cuts()) {
    for (const auto entry : id_to_constraints) {
      const int id = entry.first;
      LinearProgrammingConstraint* lp =
          gtl::FindOrDie(representative_to_lp_constraint, id);
      lp->AddCutGenerator(CreateKnapsackCoverCutGenerator(
          id_to_constraints[id], lp->integer_variables(), m));
    }
  }

  // Add the objective.
  std::map<int, std::vector<std::pair<IntegerVariable, int64>>>
      representative_to_cp_terms;
  std::vector<std::pair<IntegerVariable, int64>> top_level_cp_terms;
  int num_components_containing_objective = 0;
  if (model_proto.has_objective()) {
    // First pass: set objective coefficients on the lp constraints, and store
    // the cp terms in one vector per component.
    for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
      const IntegerVariable var =
          mapping->Integer(model_proto.objective().vars(i));
      const int64 coeff = model_proto.objective().coeffs(i);
      const int id = components.GetClassRepresentative(get_var_index(var));
      if (gtl::ContainsKey(representative_to_lp_constraint, id)) {
        representative_to_lp_constraint[id]->SetObjectiveCoefficient(
            var, IntegerValue(coeff));
        representative_to_cp_terms[id].push_back(std::make_pair(var, coeff));
      } else {
        // Component is too small. We still need to store the objective term.
        top_level_cp_terms.push_back(std::make_pair(var, coeff));
      }
    }
    // Second pass: Build the cp sub-objectives per component.
    for (const auto& it : representative_to_cp_terms) {
      const int id = it.first;
      LinearProgrammingConstraint* lp =
          gtl::FindOrDie(representative_to_lp_constraint, id);
      const std::vector<std::pair<IntegerVariable, int64>>& terms = it.second;
      const IntegerVariable sub_obj_var =
          GetOrCreateVariableGreaterOrEqualToSumOf(terms, m);
      top_level_cp_terms.push_back(std::make_pair(sub_obj_var, 1));
      lp->SetMainObjectiveVariable(sub_obj_var);
      num_components_containing_objective++;
    }
  }

  const IntegerVariable main_objective_var =
      model_proto.has_objective()
          ? GetOrCreateVariableGreaterOrEqualToSumOf(top_level_cp_terms, m)
          : kNoIntegerVariable;

  // Register LP constraints. Note that this needs to be done after all the
  // constraints have been added.
  for (auto* lp_constraint : lp_constraints) {
    lp_constraint->RegisterWith(m);
    VLOG(3) << "LP constraint: " << lp_constraint->DimensionString() << ".";
  }

  VLOG(3) << top_level_cp_terms.size()
          << " terms in the main objective linear equation ("
          << num_components_containing_objective << " from LP constraints).";
  return main_objective_var;
}

}  // namespace

// Used by NewFeasibleSolutionObserver to register observers.
struct SolutionObservers {
  explicit SolutionObservers(Model* model) {}
  std::vector<std::function<void(const CpSolverResponse& response)>> observers;
};

std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& observer) {
  return [=](Model* model) {
    model->GetOrCreate<SolutionObservers>()->observers.push_back(observer);
  };
}

#if !defined(__PORTABLE_PLATFORM__)
// TODO(user): Support it on android.
std::function<SatParameters(Model*)> NewSatParameters(
    const std::string& params) {
  sat::SatParameters parameters;
  if (!params.empty()) {
    CHECK(google::protobuf::TextFormat::ParseFromString(params, &parameters))
        << params;
  }
  return NewSatParameters(parameters);
}
#endif  // __PORTABLE_PLATFORM__

std::function<SatParameters(Model*)> NewSatParameters(
    const sat::SatParameters& parameters) {
  return [=](Model* model) {
    // Tricky: It is important to initialize the model parameters before any
    // of the solver object are created, so that by default they use the given
    // parameters.
    *model->GetOrCreate<SatParameters>() = parameters;
    model->GetOrCreate<SatSolver>()->SetParameters(parameters);
    return parameters;
  };
}

namespace {

// Registers a callback that will export variables bounds fixed at level 0 of
// the search. This should not be registered to a LNS search.
void RegisterVariableBoundsLevelZeroExport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model) {
  CHECK(shared_bounds_manager != nullptr);
  int saved_trail_index = 0;
  const auto broadcast_level_zero_bounds =
      [&model_proto, saved_trail_index, model, shared_bounds_manager](
          const std::vector<IntegerVariable>& modified_vars) mutable {
        CpModelMapping* const mapping = model->GetOrCreate<CpModelMapping>();

        std::vector<int> model_variables;
        std::vector<int64> new_lower_bounds;
        std::vector<int64> new_upper_bounds;
        absl::flat_hash_set<int> visited_variables;

        // Inspect the modified IntegerVariables.
        auto* integer_trail = model->Get<IntegerTrail>();
        for (const IntegerVariable& var : modified_vars) {
          const IntegerVariable positive_var = PositiveVariable(var);
          const int model_var =
              mapping->GetProtoVariableFromIntegerVariable(positive_var);
          if (model_var == -1 || visited_variables.contains(model_var)) {
            // TODO(user): I don't think we should see the same model_var twice
            // here so maybe we don't need the visited_variables.contains()
            // part.
            continue;
          }

          visited_variables.insert(model_var);
          const int64 new_lb =
              integer_trail->LevelZeroLowerBound(positive_var).value();
          const int64 new_ub =
              integer_trail->LevelZeroUpperBound(positive_var).value();
          // TODO(user): We could imagine an API based on atomic<int64>
          // that could preemptively check if this new bounds are improving.
          model_variables.push_back(model_var);
          new_lower_bounds.push_back(new_lb);
          new_upper_bounds.push_back(new_ub);
        }

        // Inspect the newly modified Booleans.
        auto* trail = model->Get<Trail>();
        for (; saved_trail_index < trail->Index(); ++saved_trail_index) {
          const Literal fixed_literal = (*trail)[saved_trail_index];
          const int model_var = mapping->GetProtoVariableFromBooleanVariable(
              fixed_literal.Variable());
          if (model_var == -1 || visited_variables.contains(model_var)) {
            // If the variable is already visited, it should mean that this
            // Boolean also has an IntegerVariable view, and we should already
            // have set its bound correctly.
            continue;
          }

          visited_variables.insert(model_var);
          model_variables.push_back(model_var);
          if (fixed_literal.IsPositive()) {
            new_lower_bounds.push_back(1);
            new_upper_bounds.push_back(1);
          } else {
            new_lower_bounds.push_back(0);
            new_upper_bounds.push_back(0);
          }
        }

        if (!model_variables.empty()) {
          const WorkerInfo* const worker_info =
              model->GetOrCreate<WorkerInfo>();
          shared_bounds_manager->ReportPotentialNewBounds(
              model_proto, worker_info->worker_id, worker_info->worker_name,
              model_variables, new_lower_bounds, new_upper_bounds);
        }
      };

  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(broadcast_level_zero_bounds);
}

// Registers a callback to import new variables bounds stored in the
// shared_bounds_manager. These bounds are imported at level 0 of the search
// in the linear scan minimize function.
void RegisterVariableBoundsLevelZeroImport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model) {
  CHECK(shared_bounds_manager != nullptr);
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const WorkerInfo* const worker_info = model->GetOrCreate<WorkerInfo>();
  CpModelMapping* const mapping = model->GetOrCreate<CpModelMapping>();

  const auto& import_level_zero_bounds = [&model_proto, shared_bounds_manager,
                                          model, integer_trail, worker_info,
                                          mapping]() {
    std::vector<int> model_variables;
    std::vector<int64> new_lower_bounds;
    std::vector<int64> new_upper_bounds;
    shared_bounds_manager->GetChangedBounds(worker_info->worker_id,
                                            &model_variables, &new_lower_bounds,
                                            &new_upper_bounds);
    bool new_bounds_have_been_imported = false;
    for (int i = 0; i < model_variables.size(); ++i) {
      const int model_var = model_variables[i];
      // This can happen if a boolean variables is forced to have an
      // integer view in one thread, and not in another thread.
      if (!mapping->IsInteger(model_var)) continue;
      const IntegerVariable var = mapping->Integer(model_var);
      const IntegerValue new_lb(new_lower_bounds[i]);
      const IntegerValue new_ub(new_upper_bounds[i]);
      const IntegerValue old_lb = integer_trail->LowerBound(var);
      const IntegerValue old_ub = integer_trail->UpperBound(var);
      const bool changed_lb = new_lb > old_lb;
      const bool changed_ub = new_ub < old_ub;
      if (!changed_lb && !changed_ub) continue;

      new_bounds_have_been_imported = true;
      if (VLOG_IS_ON(3)) {
        const IntegerVariableProto& var_proto =
            model_proto.variables(model_var);
        const std::string& var_name =
            var_proto.name().empty()
                ? absl::StrCat("anonymous_var(", model_var, ")")
                : var_proto.name();
        LOG(INFO) << "  '" << worker_info->worker_name
                  << "' imports new bounds for " << var_name << ": from ["
                  << old_lb << ", " << old_ub << "] to [" << new_lb << ", "
                  << new_ub << "]";
      }

      if (changed_lb &&
          !integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(var, new_lb),
                                  {}, {})) {
        return false;
      }
      if (changed_ub &&
          !integer_trail->Enqueue(IntegerLiteral::LowerOrEqual(var, new_ub), {},
                                  {})) {
        return false;
      }
    }
    if (new_bounds_have_been_imported &&
        !model->GetOrCreate<SatSolver>()->FinishPropagation()) {
      return false;
    }
    return true;
  };
  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_level_zero_bounds);
}

// Registers a callback that will report improving objective best bound.
// It will be called each time new objective bound are propagated at level zero.
void RegisterObjectiveBestBoundExport(
    IntegerVariable objective_var,
    SharedResponseManager* shared_response_manager, Model* model) {
  std::string worker_name = model->GetOrCreate<WorkerInfo>()->worker_name;
  auto* integer_trail = model->Get<IntegerTrail>();
  const auto broadcast_objective_lower_bound =
      [worker_name, objective_var, integer_trail,
       shared_response_manager](const std::vector<IntegerVariable>& unused) {
        shared_response_manager->UpdateInnerObjectiveBounds(
            worker_name, integer_trail->LevelZeroLowerBound(objective_var),
            integer_trail->LevelZeroUpperBound(objective_var));
      };
  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(
          broadcast_objective_lower_bound);
}

// Registers a callback to import new objective bounds. It will be called each
// time the search main loop is back to level zero. Note that it the presence of
// assumptions, this will not happend until the set of assumptions is changed.
void RegisterObjectiveBoundsImport(
    SharedResponseManager* shared_response_manager, Model* model) {
  auto* solver = model->GetOrCreate<SatSolver>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* worker_info = model->GetOrCreate<WorkerInfo>();
  auto* objective = model->GetOrCreate<ObjectiveDefinition>();
  const auto import_objective_bounds = [solver, integer_trail, worker_info,
                                        objective, shared_response_manager]() {
    if (solver->AssumptionLevel() != 0) return true;
    bool propagate = false;

    const IntegerValue external_lb =
        shared_response_manager->GetInnerObjectiveLowerBound();
    const IntegerValue current_lb =
        integer_trail->LowerBound(objective->objective_var);
    if (external_lb > current_lb) {
      if (!integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(
                                      objective->objective_var, external_lb),
                                  {}, {})) {
        return false;
      }
      propagate = true;
    }

    const IntegerValue external_ub =
        shared_response_manager->GetInnerObjectiveUpperBound();
    const IntegerValue current_ub =
        integer_trail->UpperBound(objective->objective_var);
    if (external_ub < current_ub) {
      if (!integer_trail->Enqueue(IntegerLiteral::LowerOrEqual(
                                      objective->objective_var, external_ub),
                                  {}, {})) {
        return false;
      }
      propagate = true;
    }

    if (!propagate) return true;

    VLOG(2) << "'" << worker_info->worker_name
            << "' imports objective bounds: external ["
            << objective->ScaleIntegerObjective(external_lb) << ", "
            << objective->ScaleIntegerObjective(external_ub) << "], current ["
            << objective->ScaleIntegerObjective(current_lb) << ", "
            << objective->ScaleIntegerObjective(current_ub) << "]";

    return solver->FinishPropagation();
  };

  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_objective_bounds);
}

// Loads a CpModelProto inside the given model.
// This should only be called once on a given 'Model' class.
//
// TODO(user): move to cp_model_loader.h/.cc
void LoadCpModel(const CpModelProto& model_proto,
                 SharedResponseManager* shared_response_manager, Model* model) {
  CHECK(shared_response_manager != nullptr);
  auto* sat_solver = model->GetOrCreate<SatSolver>();

  // Simple function for the few places where we do "return unsat()".
  const auto unsat = [shared_response_manager, sat_solver, model] {
    sat_solver->NotifyThatModelIsUnsat();
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        absl::StrCat(model->GetOrCreate<WorkerInfo>()->worker_name,
                     " [loading]"));
  };

  // We will add them all at once after model_proto is loaded.
  model->GetOrCreate<IntegerEncoder>()->DisableImplicationBetweenLiteral();

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  const bool view_all_booleans_as_integers =
      (parameters.linearization_level() >= 2) ||
      (parameters.search_branching() == SatParameters::FIXED_SEARCH &&
       model_proto.search_strategy().empty());
  mapping->CreateVariables(model_proto, view_all_booleans_as_integers, model);
  mapping->DetectOptionalVariables(model_proto, model);
  mapping->ExtractEncoding(model_proto, model);
  mapping->PropagateEncodingFromEquivalenceRelations(model_proto, model);

  // Check the model is still feasible before continuing.
  if (sat_solver->IsModelUnsat()) return unsat();

  // Force some variables to be fully encoded.
  MaybeFullyEncodeMoreVariables(model_proto, model);

  // Load the constraints.
  std::set<std::string> unsupported_types;
  int num_ignored_constraints = 0;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) {
      ++num_ignored_constraints;
      continue;
    }

    if (!LoadConstraint(ct, model)) {
      unsupported_types.insert(ConstraintCaseName(ct.constraint_case()));
      continue;
    }

    // We propagate after each new Boolean constraint but not the integer
    // ones. So we call FinishPropagation() manually here.
    //
    // Note that we only do that in debug mode as this can be really slow on
    // certain types of problems with millions of constraints.
    if (DEBUG_MODE) {
      if (sat_solver->FinishPropagation()) {
        Trail* trail = model->GetOrCreate<Trail>();
        const int old_num_fixed = trail->Index();
        if (trail->Index() > old_num_fixed) {
          VLOG(3) << "Constraint fixed " << trail->Index() - old_num_fixed
                  << " Boolean variable(s): " << ProtobufDebugString(ct);
        }
      }
    }
    if (sat_solver->IsModelUnsat()) {
      VLOG(2) << "UNSAT during extraction (after adding '"
              << ConstraintCaseName(ct.constraint_case()) << "'). "
              << ProtobufDebugString(ct);
      break;
    }
  }
  if (num_ignored_constraints > 0) {
    VLOG(3) << num_ignored_constraints << " constraints were skipped.";
  }
  if (!unsupported_types.empty()) {
    VLOG(1) << "There is unsuported constraints types in this model: ";
    for (const std::string& type : unsupported_types) {
      VLOG(1) << " - " << type;
    }
    return unsat();
  }

  model->GetOrCreate<IntegerEncoder>()
      ->AddAllImplicationsBetweenAssociatedLiterals();
  if (!sat_solver->FinishPropagation()) return unsat();

  // Auto detect "at least one of" constraints in the PrecedencesPropagator.
  // Note that we do that before we finish loading the problem (objective and
  // LP relaxation), because propagation will be faster at this point and it
  // should be enough for the purpose of this auto-detection.
  if (model->Mutable<PrecedencesPropagator>() != nullptr &&
      parameters.auto_detect_greater_than_at_least_one_of()) {
    model->Mutable<PrecedencesPropagator>()
        ->AddGreaterThanAtLeastOneOfConstraints(model);
    if (!sat_solver->FinishPropagation()) return unsat();
  }

  // TODO(user): This should be done in the presolve instead.
  // TODO(user): We don't have a good deterministic time on all constraints,
  // so this might take more time than wanted.
  if (parameters.cp_model_probing_level() > 1) {
    ProbeBooleanVariables(/*deterministic_time_limit=*/1.0, model);
    if (model->GetOrCreate<SatSolver>()->IsModelUnsat()) {
      return unsat();
    }
    if (!model->GetOrCreate<BinaryImplicationGraph>()
             ->ComputeTransitiveReduction()) {
      return unsat();
    }
  }

  // Create an objective variable and its associated linear constraint if
  // needed.
  IntegerVariable objective_var = kNoIntegerVariable;
  if (parameters.linearization_level() > 0) {
    // Linearize some part of the problem and register LP constraint(s).
    objective_var =
        AddLPConstraints(model_proto, parameters.linearization_level(), model);
  } else if (model_proto.has_objective()) {
    const CpObjectiveProto& obj = model_proto.objective();
    std::vector<std::pair<IntegerVariable, int64>> terms;
    terms.reserve(obj.vars_size());
    for (int i = 0; i < obj.vars_size(); ++i) {
      terms.push_back(
          std::make_pair(mapping->Integer(obj.vars(i)), obj.coeffs(i)));
    }
    if (parameters.optimize_with_core()) {
      objective_var = GetOrCreateVariableWithTightBound(terms, model);
    } else {
      objective_var = GetOrCreateVariableGreaterOrEqualToSumOf(terms, model);
    }
  }

  // Create the objective definition inside the Model so that it can be accessed
  // by the heuristics than needs it.
  if (objective_var != kNoIntegerVariable) {
    const CpObjectiveProto& objective_proto = model_proto.objective();
    auto* objective_definition = model->GetOrCreate<ObjectiveDefinition>();

    objective_definition->scaling_factor = objective_proto.scaling_factor();
    if (objective_definition->scaling_factor == 0.0) {
      objective_definition->scaling_factor = 1.0;
    }
    objective_definition->offset = objective_proto.offset();
    objective_definition->objective_var = objective_var;

    const int size = objective_proto.vars_size();
    objective_definition->vars.resize(size);
    objective_definition->coeffs.resize(size);
    for (int i = 0; i < objective_proto.vars_size(); ++i) {
      // Note that if there is no mapping, then the variable will be
      // kNoIntegerVariable.
      objective_definition->vars[i] = mapping->Integer(objective_proto.vars(i));
      objective_definition->coeffs[i] = IntegerValue(objective_proto.coeffs(i));

      // Fill the objective heuristics data.
      const int ref = objective_proto.vars(i);
      if (mapping->IsInteger(ref)) {
        const IntegerVariable var = mapping->Integer(objective_proto.vars(i));
        objective_definition->objective_impacting_variables.insert(
            objective_proto.coeffs(i) > 0 ? var : NegationOf(var));
      }
    }
  }

  // Intersect the objective domain with the given one if any.
  if (!model_proto.objective().domain().empty()) {
    const Domain user_domain = ReadDomainFromProto(model_proto.objective());
    const Domain automatic_domain =
        model->GetOrCreate<IntegerTrail>()->InitialVariableDomain(
            objective_var);
    VLOG(3) << "Objective offset:" << model_proto.objective().offset()
            << " scaling_factor:" << model_proto.objective().scaling_factor();
    VLOG(3) << "Automatic internal objective domain: " << automatic_domain;
    VLOG(3) << "User specified internal objective domain: " << user_domain;
    CHECK_NE(objective_var, kNoIntegerVariable);
    const bool ok = model->GetOrCreate<IntegerTrail>()->UpdateInitialDomain(
        objective_var, user_domain);
    if (!ok) {
      VLOG(2) << "UNSAT due to the objective domain.";
      return unsat();
    }

    // Make sure the sum take a value inside the objective domain by adding
    // the other side: objective <= sum terms.
    //
    // TODO(user): Use a better condition to detect when this is not useful.
    // Note also that for the core algorithm, we might need the other side too,
    // otherwise we could return feasible solution with an objective above the
    // user specified upper bound.
    if (!automatic_domain.IsIncludedIn(user_domain)) {
      std::vector<IntegerVariable> vars;
      std::vector<int64> coeffs;
      const CpObjectiveProto& obj = model_proto.objective();
      for (int i = 0; i < obj.vars_size(); ++i) {
        vars.push_back(mapping->Integer(obj.vars(i)));
        coeffs.push_back(obj.coeffs(i));
      }
      vars.push_back(objective_var);
      coeffs.push_back(-1);
      model->Add(WeightedSumGreaterOrEqual(vars, coeffs, 0));
    }
  }

  // Note that we do one last propagation at level zero once all the
  // constraints were added.
  if (!sat_solver->FinishPropagation()) return unsat();

  if (model_proto.has_objective()) {
    // Watch improved objective best bounds.
    RegisterObjectiveBestBoundExport(objective_var, shared_response_manager,
                                     model);

    // Import objective bounds.
    // TODO(user): Support objective bounds import in LNS and Core based
    // search.
    if (model->GetOrCreate<SatParameters>()->share_objective_bounds()) {
      RegisterObjectiveBoundsImport(shared_response_manager, model);
    }
  }

  // Cache the relavant data for RINS variables.
  if (model->Get<SharedRINSNeighborhoodManager>() != nullptr) {
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    auto* lp_dispatcher = model->GetOrCreate<LinearProgrammingDispatcher>();
    auto* rins_vars = model->GetOrCreate<RINSVariables>();
    IntegerVariable size = integer_trail->NumIntegerVariables();
    for (IntegerVariable positive_var(0); positive_var < size;
         positive_var += 2) {
      RINSVariable rins_var;
      rins_var.positive_var = positive_var;
      rins_var.model_var =
          mapping->GetProtoVariableFromIntegerVariable(positive_var);
      rins_var.lp = gtl::FindWithDefault(*lp_dispatcher, positive_var, nullptr);

      if (rins_var.lp != nullptr && rins_var.model_var >= 0) {
        rins_vars->vars.push_back(rins_var);
      }
    }
  }

  // Initialize the fixed_search strategy.
  auto* search_heuristics = model->GetOrCreate<SearchHeuristics>();
  search_heuristics->fixed_search = ConstructSearchStrategy(
      model_proto, mapping->GetVariableMapping(), objective_var, model);
  if (VLOG_IS_ON(3)) {
    search_heuristics->fixed_search =
        InstrumentSearchStrategy(model_proto, mapping->GetVariableMapping(),
                                 search_heuristics->fixed_search, model);
  }

  // Initialize the "follow hint" strategy.
  std::vector<BooleanOrIntegerVariable> vars;
  std::vector<IntegerValue> values;
  for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
    const int ref = model_proto.solution_hint().vars(i);
    CHECK(RefIsPositive(ref));
    BooleanOrIntegerVariable var;
    if (mapping->IsBoolean(ref)) {
      var.bool_var = mapping->Literal(ref).Variable();
    } else {
      var.int_var = mapping->Integer(ref);
    }
    vars.push_back(var);
    values.push_back(IntegerValue(model_proto.solution_hint().values(i)));
  }
  search_heuristics->hint_search = FollowHint(vars, values, model);

  // Create the CoreBasedOptimizer class if needed.
  if (parameters.optimize_with_core()) {
    // TODO(user): Remove code duplication with the solution_observer in
    // SolveLoadedCpModel().
    const std::string solution_info =
        model->GetOrCreate<WorkerInfo>()->worker_name;
    const auto solution_observer = [&model_proto, model, solution_info,
                                    shared_response_manager]() {
      CpSolverResponse response;
      FillSolutionInResponse(model_proto, *model, &response);
      response.set_solution_info(solution_info);
      shared_response_manager->NewSolution(response, model);
    };

    const auto& objective = *model->GetOrCreate<ObjectiveDefinition>();
    CoreBasedOptimizer* core =
        new CoreBasedOptimizer(objective_var, objective.vars, objective.coeffs,
                               solution_observer, model);
    model->Register<CoreBasedOptimizer>(core);
    model->TakeOwnership(core);
  }
}

// Solves an already loaded cp_model_proto.
// The final CpSolverResponse must be read from the shared_response_manager.
//
// TODO(user): This should be transformed so that it can be called many times
// and resume from the last search state as if it wasn't interuped. That would
// allow use to easily interleave different heuristics in the same thread.
void SolveLoadedCpModel(const CpModelProto& model_proto,
                        SharedResponseManager* shared_response_manager,
                        Model* model) {
  if (shared_response_manager->ProblemIsSolved()) return;

  const std::string& solution_info =
      model->GetOrCreate<WorkerInfo>()->worker_name;
  const auto solution_observer = [&model_proto, &model, &solution_info,
                                  &shared_response_manager]() {
    CpSolverResponse response;
    FillSolutionInResponse(model_proto, *model, &response);
    response.set_solution_info(solution_info);
    shared_response_manager->NewSolution(response, model);
  };

  // Reconfigure search heuristic if it was changed.
  ConfigureSearchHeuristics(model);

  SatSolver::Status status;
  const SatParameters& parameters = *model->GetOrCreate<SatParameters>();
  if (!model_proto.has_objective()) {
    while (true) {
      status = ResetAndSolveIntegerProblem(/*assumptions=*/{}, model);
      if (status != SatSolver::Status::FEASIBLE) break;
      solution_observer();
      if (!parameters.enumerate_all_solutions()) break;
      model->Add(ExcludeCurrentSolutionWithoutIgnoredVariableAndBacktrack());
    }
    if (status == SatSolver::INFEASIBLE) {
      shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
          solution_info);
    }
  } else {
    // Optimization problem.
    const auto& objective = *model->GetOrCreate<ObjectiveDefinition>();
    const IntegerVariable objective_var = objective.objective_var;
    CHECK_NE(objective_var, kNoIntegerVariable);

    if (parameters.optimize_with_core()) {
      // TODO(user): This doesn't work with splitting in chunk for now. It
      // shouldn't be too hard to fix.
      if (parameters.optimize_with_max_hs()) {
        status = MinimizeWithHittingSetAndLazyEncoding(
            objective_var, objective.vars, objective.coeffs, solution_observer,
            model);
      } else {
        status = model->Mutable<CoreBasedOptimizer>()->Optimize();
      }
    } else {
      // TODO(user): This parameter break the splitting in chunk of a Solve().
      // It should probably be moved into another SubSolver altogether.
      if (parameters.binary_search_num_conflicts() >= 0) {
        RestrictObjectiveDomainWithBinarySearch(objective_var,
                                                solution_observer, model);
      }
      status = MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          objective_var, solution_observer, model);
    }

    // The search is done in both case.
    //
    // TODO(user): Remove the weird translation INFEASIBLE->FEASIBLE in the
    // function above?
    if (status == SatSolver::INFEASIBLE || status == SatSolver::FEASIBLE) {
      shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
          solution_info);
    }
  }

  // TODO(user): Remove from here when we split in chunk. We just want to
  // do that at the end.
  shared_response_manager->SetStatsFromModel(model);
}

// Try to find a solution by following the hint and using a low conflict limit.
// The CpModelProto must already be loaded in the Model.
void QuickSolveWithHint(const CpModelProto& model_proto,
                        SharedResponseManager* shared_response_manager,
                        Model* model) {
  if (!model_proto.has_solution_hint()) return;
  if (shared_response_manager->ProblemIsSolved()) return;

  // Temporarily change the parameters.
  auto* parameters = model->GetOrCreate<SatParameters>();
  const SatParameters saved_params = *parameters;
  parameters->set_max_number_of_conflicts(10);
  parameters->set_search_branching(SatParameters::HINT_SEARCH);
  parameters->set_optimize_with_core(false);
  auto cleanup = ::gtl::MakeCleanup(
      [parameters, saved_params]() { *parameters = saved_params; });

  // Solve decision problem.
  ConfigureSearchHeuristics(model);
  const SatSolver::Status status =
      ResetAndSolveIntegerProblem(/*assumptions=*/{}, model);

  const std::string& solution_info =
      model->GetOrCreate<WorkerInfo>()->worker_name;
  if (status == SatSolver::Status::FEASIBLE) {
    CpSolverResponse response;
    FillSolutionInResponse(model_proto, *model, &response);
    response.set_solution_info(absl::StrCat(solution_info, " [hint]"));
    shared_response_manager->NewSolution(response, model);

    if (!model_proto.has_objective()) {
      if (parameters->enumerate_all_solutions()) {
        model->Add(ExcludeCurrentSolutionWithoutIgnoredVariableAndBacktrack());
      }
    } else {
      // Restrict the objective.
      const IntegerVariable objective_var =
          model->GetOrCreate<ObjectiveDefinition>()->objective_var;
      model->GetOrCreate<SatSolver>()->Backtrack(0);
      IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
      if (!integer_trail->Enqueue(
              IntegerLiteral::LowerOrEqual(
                  objective_var,
                  shared_response_manager->GetInnerObjectiveUpperBound()),
              {}, {})) {
        shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
            absl::StrCat(solution_info, " [hint]"));
        shared_response_manager->SetStatsFromModel(model);
        return;
      }
    }
  }
}

// TODO(user): If this ever shows up in the profile, we could avoid copying
// the mapping_proto if we are careful about how we modify the variable domain
// before postsolving it.
void PostsolveResponse(const std::string& debug_info,
                       const CpModelProto& model_proto,
                       CpModelProto mapping_proto,
                       const std::vector<int>& postsolve_mapping,
                       WallTimer* wall_timer, CpSolverResponse* response) {
  if (response->status() != CpSolverStatus::FEASIBLE &&
      response->status() != CpSolverStatus::OPTIMAL) {
    return;
  }
  // Postsolve.
  for (int i = 0; i < response->solution_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    var_proto->clear_domain();
    var_proto->add_domain(response->solution(i));
    var_proto->add_domain(response->solution(i));
  }
  for (int i = 0; i < response->solution_lower_bounds_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    FillDomainInProto(
        ReadDomainFromProto(*var_proto)
            .IntersectionWith({response->solution_lower_bounds(i),
                               response->solution_upper_bounds(i)}),
        var_proto);
  }

  // Postosolve parameters.
  // TODO(user): this problem is usually trivial, but we may still want to
  // impose a time limit or copy some of the parameters passed by the user.
  Model postsolve_model;
  {
    SatParameters params;
    params.set_linearization_level(0);
    params.set_cp_model_probing_level(0);
    postsolve_model.Add(operations_research::sat::NewSatParameters(params));
  }

  std::unique_ptr<TimeLimit> time_limit(TimeLimit::Infinite());
  SharedTimeLimit shared_time_limit(time_limit.get());
  SharedResponseManager local_response_manager(
      /*log_updates=*/false, /*enumerate_all_solutions=*/false, &mapping_proto,
      wall_timer, &shared_time_limit);
  LoadCpModel(mapping_proto, &local_response_manager, &postsolve_model);
  SolveLoadedCpModel(mapping_proto, &local_response_manager, &postsolve_model);
  const CpSolverResponse postsolve_response =
      local_response_manager.GetResponse();
  CHECK(postsolve_response.status() == CpSolverStatus::FEASIBLE ||
        postsolve_response.status() == CpSolverStatus::OPTIMAL);

  // We only copy the solution from the postsolve_response to the response.
  response->clear_solution();
  response->clear_solution_lower_bounds();
  response->clear_solution_upper_bounds();
  if (!postsolve_response.solution().empty()) {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response->add_solution(postsolve_response.solution(i));
    }
    CHECK(SolutionIsFeasible(model_proto,
                             std::vector<int64>(response->solution().begin(),
                                                response->solution().end())))
        << debug_info;
  } else {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response->add_solution_lower_bounds(
          postsolve_response.solution_lower_bounds(i));
      response->add_solution_upper_bounds(
          postsolve_response.solution_upper_bounds(i));
    }
  }
}

// TODO(user): Uniformize this function with the other one.
CpSolverResponse SolvePureSatModel(const CpModelProto& model_proto,
                                   WallTimer* wall_timer, Model* model) {
  std::unique_ptr<SatSolver> solver(new SatSolver());
  SatParameters parameters = *model->GetOrCreate<SatParameters>();
  parameters.set_log_search_progress(true);
  solver->SetParameters(parameters);
  model->GetOrCreate<TimeLimit>()->ResetLimitFromParameters(parameters);

  // Create a DratProofHandler?
  std::unique_ptr<DratProofHandler> drat_proof_handler;
#if !defined(__PORTABLE_PLATFORM__)
  if (!FLAGS_drat_output.empty() || FLAGS_drat_check) {
    if (!FLAGS_drat_output.empty()) {
      File* output;
      CHECK_OK(file::Open(FLAGS_drat_output, "w", &output, file::Defaults()));
      drat_proof_handler = absl::make_unique<DratProofHandler>(
          /*in_binary_format=*/false, output, FLAGS_drat_check);
    } else {
      drat_proof_handler = absl::make_unique<DratProofHandler>();
    }
    solver->SetDratProofHandler(drat_proof_handler.get());
  }
#endif  // __PORTABLE_PLATFORM__

  auto get_literal = [](int ref) {
    if (ref >= 0) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };

  std::vector<Literal> temp;
  const int num_variables = model_proto.variables_size();
  solver->SetNumVariables(num_variables);
  if (drat_proof_handler != nullptr) {
    drat_proof_handler->SetNumVariables(num_variables);
  }

  for (const ConstraintProto& ct : model_proto.constraints()) {
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolAnd: {
        if (ct.enforcement_literal_size() == 0) {
          for (const int ref : ct.bool_and().literals()) {
            const Literal b = get_literal(ref);
            solver->AddUnitClause(b);
          }
        } else {
          // a => b
          const Literal not_a =
              get_literal(ct.enforcement_literal(0)).Negated();
          for (const int ref : ct.bool_and().literals()) {
            const Literal b = get_literal(ref);
            solver->AddProblemClause({not_a, b});
            if (drat_proof_handler != nullptr) {
              drat_proof_handler->AddProblemClause({not_a, b});
            }
          }
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kBoolOr:
        temp.clear();
        for (const int ref : ct.bool_or().literals()) {
          temp.push_back(get_literal(ref));
        }
        solver->AddProblemClause(temp);
        if (drat_proof_handler != nullptr) {
          drat_proof_handler->AddProblemClause(temp);
        }
        break;
      default:
        LOG(FATAL) << "Not supported";
    }
  }

  // Deal with fixed variables.
  for (int ref = 0; ref < num_variables; ++ref) {
    const Domain domain = ReadDomainFromProto(model_proto.variables(ref));
    if (domain.Min() == domain.Max()) {
      const Literal ref_literal =
          domain.Min() == 0 ? get_literal(ref).Negated() : get_literal(ref);
      solver->AddUnitClause(ref_literal);
      if (drat_proof_handler != nullptr) {
        drat_proof_handler->AddProblemClause({ref_literal});
      }
    }
  }

  SatSolver::Status status;
  CpSolverResponse response;
  if (parameters.cp_model_presolve()) {
    std::vector<bool> solution;
    status = SolveWithPresolve(&solver, model->GetOrCreate<TimeLimit>(),
                               &solution, drat_proof_handler.get());
    if (status == SatSolver::FEASIBLE) {
      response.clear_solution();
      for (int ref = 0; ref < num_variables; ++ref) {
        response.add_solution(solution[ref]);
      }
    }
  } else {
    status = solver->SolveWithTimeLimit(model->GetOrCreate<TimeLimit>());
    if (status == SatSolver::FEASIBLE) {
      response.clear_solution();
      for (int ref = 0; ref < num_variables; ++ref) {
        response.add_solution(
            solver->Assignment().LiteralIsTrue(get_literal(ref)) ? 1 : 0);
      }
    }
  }

  switch (status) {
    case SatSolver::LIMIT_REACHED: {
      response.set_status(CpSolverStatus::UNKNOWN);
      break;
    }
    case SatSolver::FEASIBLE: {
      CHECK(SolutionIsFeasible(model_proto,
                               std::vector<int64>(response.solution().begin(),
                                                  response.solution().end())));
      response.set_status(CpSolverStatus::FEASIBLE);
      break;
    }
    case SatSolver::INFEASIBLE: {
      response.set_status(CpSolverStatus::INFEASIBLE);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected SatSolver::Status " << status;
  }
  response.set_num_booleans(solver->NumVariables());
  response.set_num_branches(solver->num_branches());
  response.set_num_conflicts(solver->num_failures());
  response.set_num_binary_propagations(solver->num_propagations());
  response.set_num_integer_propagations(0);
  response.set_wall_time(wall_timer->Get());
  response.set_deterministic_time(
      model->Get<TimeLimit>()->GetElapsedDeterministicTime());

  if (status == SatSolver::INFEASIBLE && drat_proof_handler != nullptr) {
    WallTimer drat_timer;
    drat_timer.Start();
    DratChecker::Status drat_status =
        drat_proof_handler->Check(FLAGS_max_drat_time_in_seconds);
    switch (drat_status) {
      case DratChecker::UNKNOWN:
        LOG(INFO) << "DRAT status: UNKNOWN";
        break;
      case DratChecker::VALID:
        LOG(INFO) << "DRAT status: VALID";
        break;
      case DratChecker::INVALID:
        LOG(ERROR) << "DRAT status: INVALID";
        break;
      default:
        // Should not happen.
        break;
    }
    LOG(INFO) << "DRAT wall time: " << drat_timer.Get();
  } else if (drat_proof_handler != nullptr) {
    // Always log a DRAT status to make it easier to extract it from a multirun
    // result with awk.
    LOG(INFO) << "DRAT status: NA";
    LOG(INFO) << "DRAT wall time: NA";
    LOG(INFO) << "DRAT user time: NA";
  }
  return response;
}

#if !defined(__PORTABLE_PLATFORM__)

// Small wrapper to simplify the constructions of the two SubSolver below.
struct SharedClasses {
  CpModelProto const* model_proto;
  WallTimer* wall_timer;
  SharedTimeLimit* time_limit;
  SharedBoundsManager* bounds;
  SharedResponseManager* response;
  SharedRINSNeighborhoodManager* rins_manager;
};

// Encapsulate a full CP-SAT solve without presolve in the SubSolver API.
class FullProblemSolver : public SubSolver {
 public:
  FullProblemSolver(int id, const std::string& name,
                    const SatParameters& local_parameters, bool split_in_chunks,
                    SharedClasses* shared)
      : SubSolver(id, name),
        shared_(shared),
        split_in_chunks_(split_in_chunks),
        local_model_(absl::make_unique<Model>()) {
    // Setup the local model parameters and time limit.
    local_model_->Add(NewSatParameters(local_parameters));
    shared_->time_limit->UpdateLocalLimit(
        local_model_->GetOrCreate<TimeLimit>());

    // Stores info that will be used for logs in the local model.
    WorkerInfo* worker_info = local_model_->GetOrCreate<WorkerInfo>();
    worker_info->worker_name = name;
    worker_info->worker_id = id;

    // Add shared neighborhood only if RINS is enabled in global parameters.
    if (shared_->rins_manager != nullptr) {
      local_model_->Register<SharedRINSNeighborhoodManager>(
          shared_->rins_manager);
    }

    // Level zero variable bounds sharing.
    if (shared_->bounds != nullptr) {
      RegisterVariableBoundsLevelZeroExport(
          *shared_->model_proto, shared_->bounds, local_model_.get());
      RegisterVariableBoundsLevelZeroImport(
          *shared_->model_proto, shared_->bounds, local_model_.get());
    }
  }

  bool SearchIsDone() const {
    return shared_->response->ProblemIsSolved() ||
           shared_->time_limit->LimitReached();
  }

  bool TaskIsAvailable() override {
    if (SearchIsDone()) return false;

    absl::MutexLock mutex_lock(&mutex_);
    return previous_task_is_completed_;
  }

  std::function<void()> GenerateTask(int64 task_id) override {
    {
      absl::MutexLock mutex_lock(&mutex_);
      previous_task_is_completed_ = false;
    }
    return [this]() {
      if (solving_first_chunk_) {
        LoadCpModel(*shared_->model_proto, shared_->response,
                    local_model_.get());
        QuickSolveWithHint(*shared_->model_proto, shared_->response,
                           local_model_.get());

        // No need for mutex since we only run one task at the time.
        solving_first_chunk_ = false;

        if (split_in_chunks_) {
          // Abort first chunk and allow to schedule the next.
          absl::MutexLock mutex_lock(&mutex_);
          previous_task_is_completed_ = true;
          return;
        }
      }

      auto* time_limit = local_model_->GetOrCreate<TimeLimit>();
      if (split_in_chunks_) {
        // Configure time limit for chunk solving. Note that we do not want
        // to do that for the hint search for now.
        auto* params = local_model_->GetOrCreate<SatParameters>();
        params->set_max_deterministic_time(1);
        time_limit->ResetLimitFromParameters(*params);
        shared_->time_limit->UpdateLocalLimit(time_limit);
      }

      const double saved_dtime = time_limit->GetElapsedDeterministicTime();
      SolveLoadedCpModel(*shared_->model_proto, shared_->response,
                         local_model_.get());
      {
        absl::MutexLock mutex_lock(&mutex_);
        deterministic_time_since_last_synchronize_ +=
            time_limit->GetElapsedDeterministicTime() - saved_dtime;
      }

      // Abort if the problem is solved.
      if (SearchIsDone()) {
        shared_->time_limit->Stop();
        return;
      }

      // In this mode, we allow to generate more task.
      if (split_in_chunks_) {
        absl::MutexLock mutex_lock(&mutex_);
        previous_task_is_completed_ = true;
        return;
      }

      // Once a solver is done clear its memory and do not wait for the
      // destruction of the SubSolver. This is important because the full solve
      // might not be done at all, for instance this might have been configured
      // with stop_after_first_solution.
      local_model_.reset();
    };
  }

  // TODO(user): A few of the information sharing we do between threads does not
  // happen here (bound sharing, RINS neighborhood, objective). Fix that so we
  // can have a deterministic parallel mode.
  void Synchronize() override {
    absl::MutexLock mutex_lock(&mutex_);
    deterministic_time_ += deterministic_time_since_last_synchronize_;
    shared_->time_limit->AdvanceDeterministicTime(
        deterministic_time_since_last_synchronize_);
    deterministic_time_since_last_synchronize_ = 0.0;
  }

 private:
  SharedClasses* shared_;
  const bool split_in_chunks_;
  std::unique_ptr<Model> local_model_;

  // The first chunk is special. It is the one in which we load the model and
  // try to follow the hint.
  bool solving_first_chunk_ = true;

  absl::Mutex mutex_;
  double deterministic_time_since_last_synchronize_ GUARDED_BY(mutex_) =
      0.0;
  bool previous_task_is_completed_ GUARDED_BY(mutex_) = true;
};

// A Subsolver that generate LNS solve from a given neighborhood.
class LnsSolver : public SubSolver {
 public:
  LnsSolver(int id, std::unique_ptr<NeighborhoodGenerator> generator,
            const SatParameters& parameters,
            NeighborhoodGeneratorHelper* helper, SharedClasses* shared)
      : SubSolver(id, generator->name()),
        generator_(std::move(generator)),
        helper_(helper),
        parameters_(parameters),
        shared_(shared) {}

  bool SearchIsDone() const {
    return shared_->response->ProblemIsSolved() ||
           shared_->time_limit->LimitReached();
  }

  bool TaskIsAvailable() override {
    if (SearchIsDone()) return false;
    return generator_->ReadyToGenerate();
  }

  std::function<void()> GenerateTask(int64 task_id) override {
    return [task_id, this]() {
      if (SearchIsDone()) return;

      // Create a random number generator whose seed depends both on the task_id
      // and on the parameters_.random_seed() so that changing the later will
      // change the LNS behavior.
      const int32 low = static_cast<int32>(task_id);
      const int32 high = task_id >> 32;
      std::seed_seq seed{low, high, parameters_.random_seed()};
      random_engine_t random(seed);

      NeighborhoodGenerator::SolveData data;
      data.difficulty = generator_->difficulty();
      data.deterministic_limit = generator_->deterministic_limit();

      // Choose a base solution for this neighborhood.
      CpSolverResponse base_response;
      {
        const SharedSolutionRepository& repo =
            shared_->response->SolutionsRepository();
        if (repo.NumSolutions() > 0) {
          base_response.set_status(CpSolverStatus::FEASIBLE);
          const SharedSolutionRepository::Solution solution =
              repo.GetRandomBiasedSolution(&random);
          for (const int64 value : solution.variable_values) {
            base_response.add_solution(value);
          }
          data.initial_best_objective = repo.GetSolution(0).internal_objective;
          data.base_objective = solution.internal_objective;
        } else {
          base_response.set_status(CpSolverStatus::UNKNOWN);

          // If we do not have a solution, we use the current objective upper
          // bound so that our code that compute an "objective" improvement
          // works.
          //
          // TODO(user): this is non-deterministic. Fix.
          data.initial_best_objective =
              shared_->response->GetInnerObjectiveUpperBound();
          data.base_objective = data.initial_best_objective;
        }
      }

      Neighborhood neighborhood;
      {
        absl::MutexLock mutex_lock(helper_->MutableMutex());
        neighborhood =
            generator_->Generate(base_response, data.difficulty, &random);
      }
      neighborhood.cp_model.set_name(absl::StrCat("lns_", task_id));
      if (!neighborhood.is_generated) return;

      const double fully_solved_proportion =
          static_cast<double>(generator_->num_fully_solved_calls()) /
          std::max(int64{1}, generator_->num_calls());
      const std::string solution_info = absl::StrFormat(
          "%s(d=%0.2f s=%i t=%0.2f p=%0.2f)", name(), data.difficulty, task_id,
          data.deterministic_limit, fully_solved_proportion);

      SatParameters local_params(parameters_);
      local_params.set_max_deterministic_time(data.deterministic_limit);
      local_params.set_stop_after_first_solution(false);

      if (FLAGS_cp_model_dump_lns) {
        const std::string name =
            absl::StrCat("/tmp/", neighborhood.cp_model.name(), ".pb.txt");
        LOG(INFO) << "Dumping LNS model to '" << name << "'.";
        CHECK_OK(
            file::SetTextProto(name, neighborhood.cp_model, file::Defaults()));
      }

      Model local_model;
      local_model.Add(NewSatParameters(local_params));
      shared_->time_limit->UpdateLocalLimit(
          local_model.GetOrCreate<TimeLimit>());

      // Presolve and solve the LNS fragment.
      CpModelProto mapping_proto;
      std::vector<int> postsolve_mapping;
      PresolveOptions options;
      options.log_info = VLOG_IS_ON(3);
      options.parameters = *local_model.GetOrCreate<SatParameters>();
      options.time_limit = local_model.GetOrCreate<TimeLimit>();
      auto context = absl::make_unique<PresolveContext>(&neighborhood.cp_model,
                                                        &mapping_proto);
      PresolveCpModel(options, context.get(), &postsolve_mapping);

      // Release the context
      context.reset(nullptr);

      // TODO(user): Depending on the problem, we should probably use the
      // parameters that work bests (core, linearization_level, etc...) or
      // maybe we can just randomize them like for the base solution used.
      SharedResponseManager local_response_manager(
          /*log_updates=*/false, /*enumerate_all_solutions=*/false,
          &neighborhood.cp_model, shared_->wall_timer, shared_->time_limit);
      LoadCpModel(neighborhood.cp_model, &local_response_manager, &local_model);
      QuickSolveWithHint(neighborhood.cp_model, &local_response_manager,
                         &local_model);
      SolveLoadedCpModel(neighborhood.cp_model, &local_response_manager,
                         &local_model);
      CpSolverResponse local_response = local_response_manager.GetResponse();

      // TODO(user): we actually do not need to postsolve if the solution is
      // not going to be used...
      PostsolveResponse(solution_info, *shared_->model_proto, mapping_proto,
                        postsolve_mapping, shared_->wall_timer,
                        &local_response);

      local_response_manager.BestSolutionInnerObjectiveValue();
      if (local_response.solution_info().empty()) {
        local_response.set_solution_info(solution_info);
      } else {
        local_response.set_solution_info(
            absl::StrCat(local_response.solution_info(), " ", solution_info));
      }

      // Finish to fill the SolveData now that the local solve is done.
      data.status = local_response.status();
      data.deterministic_time = local_response.deterministic_time();
      data.new_objective = data.base_objective;
      if (local_response.status() == CpSolverStatus::OPTIMAL ||
          local_response.status() == CpSolverStatus::FEASIBLE) {
        data.new_objective = IntegerValue(ComputeInnerObjective(
            shared_->model_proto->objective(), local_response));
      }
      generator_->AddSolveData(data);

      // The total number of call when this was called is the same as task_id.
      const int total_num_calls = task_id;
      VLOG(2) << name() << ": [difficulty: " << data.difficulty
              << ", id: " << task_id
              << ", deterministic_time: " << data.deterministic_time << " / "
              << data.deterministic_limit
              << ", status: " << ProtoEnumToString<CpSolverStatus>(data.status)
              << ", num calls: " << generator_->num_calls()
              << ", UCB1 Score: " << generator_->GetUCBScore(total_num_calls)
              << ", p: " << fully_solved_proportion << "]";

      // Report any feasible solution we have.
      if (local_response.status() == CpSolverStatus::OPTIMAL ||
          local_response.status() == CpSolverStatus::FEASIBLE) {
        shared_->response->NewSolution(local_response,
                                       /*model=*/nullptr);
      }
      if (!neighborhood.is_reduced &&
          (local_response.status() == CpSolverStatus::OPTIMAL ||
           local_response.status() == CpSolverStatus::INFEASIBLE)) {
        shared_->response->NotifyThatImprovingProblemIsInfeasible(
            local_response.solution_info());
        shared_->time_limit->Stop();
      }
    };
  }

  void Synchronize() override {
    generator_->Synchronize();
    const double old = deterministic_time_;
    deterministic_time_ = generator_->deterministic_time();
    shared_->time_limit->AdvanceDeterministicTime(deterministic_time_ - old);
  }

 private:
  std::unique_ptr<NeighborhoodGenerator> generator_;
  NeighborhoodGeneratorHelper* helper_;
  const SatParameters parameters_;
  SharedClasses* shared_;
};

void SolveCpModelParallel(const CpModelProto& model_proto,
                          SharedResponseManager* shared_response_manager,
                          SharedTimeLimit* shared_time_limit,
                          WallTimer* wall_timer, Model* global_model) {
  CHECK(shared_response_manager != nullptr);
  const SatParameters& parameters = *global_model->GetOrCreate<SatParameters>();
  const int num_search_workers = parameters.num_search_workers();
  const bool log_search = parameters.log_search_progress() || VLOG_IS_ON(1);
  CHECK(!parameters.enumerate_all_solutions())
      << "Enumerating all solutions in parallel is not supported.";

  // If "interleave_search" is true, then the number of strategies is
  // 4 if num_search_workers = 1, or 8 otherwise.
  const int num_strategies =
      parameters.interleave_search()
          ? (parameters.reduce_memory_usage_in_interleave_mode() ? 5 : 8)
          : num_search_workers;

  std::unique_ptr<SharedBoundsManager> shared_bounds_manager;
  if (global_model->GetOrCreate<SatParameters>()->share_level_zero_bounds()) {
    // TODO(user): The current code is a bit brittle because we may have
    // more SubSolver ids than num_strategies, and each SubSolver might
    // need to synchronize bounds. Fix, it should be easy to make this number
    // adapt dynamically in the SharedBoundsManager.
    shared_bounds_manager =
        absl::make_unique<SharedBoundsManager>(num_strategies + 1, model_proto);
  }
  std::unique_ptr<SharedRINSNeighborhoodManager> shared_rins_manager;
  if (global_model->GetOrCreate<SatParameters>()->use_rins_lns()) {
    shared_rins_manager = absl::make_unique<SharedRINSNeighborhoodManager>(
        model_proto.variables_size());
    global_model->Register<SharedRINSNeighborhoodManager>(
        shared_rins_manager.get());
  }

  SharedClasses shared;
  shared.model_proto = &model_proto;
  shared.wall_timer = wall_timer;
  shared.time_limit = shared_time_limit;
  shared.bounds = shared_bounds_manager.get();
  shared.rins_manager = shared_rins_manager.get();
  shared.response = shared_response_manager;

  // The list of all the SubSolver that will be used in this parallel search.
  std::vector<std::unique_ptr<SubSolver>> subsolvers;

  if (parameters.use_lns_only()) {
    // Register something to find a first solution.
    SatParameters local_params = parameters;
    local_params.set_stop_after_first_solution(true);
    subsolvers.push_back(absl::make_unique<FullProblemSolver>(
        /*id=*/subsolvers.size(), "first_solution", local_params,
        /*split_in_chunks=*/false, &shared));
  } else {
    // Add a solver for each non-LNS workers.
    for (int i = 0; i < num_strategies; ++i) {
      std::string worker_name;
      const SatParameters local_params =
          DiversifySearchParameters(parameters, model_proto, i, &worker_name);

      // TODO(user): Refactor DiversifySearchParameters() to not generate LNS
      // config since we now deal with these separately.
      if (local_params.use_lns_only()) continue;

      // TODO(user): This is currently not supported here.
      if (parameters.optimize_with_max_hs()) continue;

      subsolvers.push_back(absl::make_unique<FullProblemSolver>(
          /*id=*/subsolvers.size(), worker_name, local_params,
          /*split_in_chunks=*/parameters.interleave_search(), &shared));
    }
  }

  // Only register LNS SubSolver if there is an objective.
  if (model_proto.has_objective()) {
    // Add the NeighborhoodGeneratorHelper as a special subsolver so that its
    // Synchronize() is called before any LNS neighborhood solvers.
    auto unique_helper = absl::make_unique<NeighborhoodGeneratorHelper>(
        /*id=*/subsolvers.size(), &model_proto, &parameters,
        shared_response_manager, shared_time_limit,
        shared_bounds_manager.get());
    NeighborhoodGeneratorHelper* helper = unique_helper.get();
    subsolvers.push_back(std::move(unique_helper));

    const int num_lns_strategies = parameters.diversify_lns_params() ? 6 : 1;
    for (int i = 0; i < num_lns_strategies; ++i) {
      std::string strategy_name;
      const SatParameters local_params =
          DiversifySearchParameters(parameters, model_proto, i, &strategy_name);
      if (local_params.use_lns_only()) continue;

      // Enqueue all the possible LNS neighborhood subsolvers.
      // Each will have their own metrics.
      subsolvers.push_back(absl::make_unique<LnsSolver>(
          /*id=*/subsolvers.size(),
          absl::make_unique<SimpleNeighborhoodGenerator>(
              helper, absl::StrCat("rnd_lns_", strategy_name)),
          local_params, helper, &shared));
      subsolvers.push_back(absl::make_unique<LnsSolver>(
          /*id=*/subsolvers.size(),
          absl::make_unique<VariableGraphNeighborhoodGenerator>(
              helper, absl::StrCat("var_lns_", strategy_name)),
          local_params, helper, &shared));
      subsolvers.push_back(absl::make_unique<LnsSolver>(
          /*id=*/subsolvers.size(),
          absl::make_unique<ConstraintGraphNeighborhoodGenerator>(
              helper, absl::StrCat("cst_lns_", strategy_name)),
          local_params, helper, &shared));

      if (!helper->TypeToConstraints(ConstraintProto::kNoOverlap).empty()) {
        subsolvers.push_back(absl::make_unique<LnsSolver>(
            /*id=*/subsolvers.size(),
            absl::make_unique<SchedulingTimeWindowNeighborhoodGenerator>(
                helper,
                absl::StrCat("scheduling_time_window_lns_", strategy_name)),
            local_params, helper, &shared));
        subsolvers.push_back(absl::make_unique<LnsSolver>(
            /*id=*/subsolvers.size(),
            absl::make_unique<SchedulingNeighborhoodGenerator>(
                helper, absl::StrCat("scheduling_random_lns_", strategy_name)),
            local_params, helper, &shared));
      }
      if (parameters.use_rins_lns()) {
        subsolvers.push_back(absl::make_unique<LnsSolver>(
            /*id=*/subsolvers.size(),
            absl::make_unique<RelaxationInducedNeighborhoodGenerator>(
                helper, global_model,
                absl::StrCat("rins/rens_lns_", strategy_name)),
            local_params, helper, &shared));
      }
    }
  }

  // Log the name of all our SubSolvers.
  if (log_search) {
    std::vector<std::string> names;
    for (const auto& subsolver : subsolvers) {
      names.push_back(subsolver->name());
    }
    LOG(INFO) << absl::StrFormat(
        "*** starting Search at %.2fs with %i workers and strategies: [ %s ]",
        wall_timer->Get(), num_search_workers, absl::StrJoin(names, ", "));
  }

  // Launch the main search loop.
  if (parameters.deterministic_parallel_search()) {
    // TODO(user): Make the batch_size independent of the number of threads so
    // that we have the same behavior independently of the number of workers!
    const int batch_size = 4 * num_search_workers;
    DeterministicLoop(subsolvers, num_search_workers, batch_size);
  } else {
    NonDeterministicLoop(subsolvers, num_search_workers);
  }
}

#endif  // __PORTABLE_PLATFORM__

}  // namespace

CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model) {
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();
  SharedTimeLimit shared_time_limit(model->GetOrCreate<TimeLimit>());

#if !defined(__PORTABLE_PLATFORM__)
  // Dump?
  if (!FLAGS_cp_model_dump_file.empty()) {
    LOG(INFO) << "Dumping cp model proto to '" << FLAGS_cp_model_dump_file
              << "'.";
    CHECK_OK(file::SetTextProto(FLAGS_cp_model_dump_file, model_proto,
                                file::Defaults()));
  }

  // Override parameters?
  if (!FLAGS_cp_model_params.empty()) {
    SatParameters params = *model->GetOrCreate<SatParameters>();
    SatParameters flag_params;
    CHECK(google::protobuf::TextFormat::ParseFromString(FLAGS_cp_model_params,
                                                        &flag_params));
    params.MergeFrom(flag_params);
    model->Add(NewSatParameters(params));
  }

  // Register SIGINT handler if requested by the parameters.
  SigintHandler handler;
  if (model->GetOrCreate<SatParameters>()->catch_sigint_signal()) {
    handler.Register([&shared_time_limit]() { shared_time_limit.Stop(); });
  }
#endif  // __PORTABLE_PLATFORM__

  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  const bool log_search = params.log_search_progress() || VLOG_IS_ON(1);
  LOG_IF(INFO, log_search) << "Parameters: " << params.ShortDebugString();

  // Validate model_proto.
  // TODO(user): provide an option to skip this step for speed?
  {
    const std::string error = ValidateCpModel(model_proto);
    if (!error.empty()) {
      LOG_IF(INFO, log_search) << error;
      CpSolverResponse response;
      response.set_status(CpSolverStatus::MODEL_INVALID);
      LOG_IF(INFO, log_search) << CpSolverResponseStats(response);
      return response;
    }
  }
  LOG_IF(INFO, log_search) << CpModelStats(model_proto);

  // Special case for pure-sat problem.
  // TODO(user): improve the normal presolver to do the same thing.
  // TODO(user): Support solution hint, but then the first TODO will make it
  // automatic.
  if (!model_proto.has_objective() && !model_proto.has_solution_hint() &&
      !params.enumerate_all_solutions() && !params.use_lns_only()) {
    bool is_pure_sat = true;
    for (const IntegerVariableProto& var : model_proto.variables()) {
      if (var.domain_size() != 2 || var.domain(0) < 0 || var.domain(1) > 1) {
        is_pure_sat = false;
        break;
      }
    }
    if (is_pure_sat) {
      for (const ConstraintProto& ct : model_proto.constraints()) {
        if (ct.constraint_case() != ConstraintProto::ConstraintCase::kBoolOr &&
            ct.constraint_case() != ConstraintProto::ConstraintCase::kBoolAnd) {
          is_pure_sat = false;
          break;
        }
      }
    }
    if (is_pure_sat) {
      // TODO(user): All this duplication will go away when we are fast enough
      // on pure-sat model with the CpModel presolve...
      CpSolverResponse response =
          SolvePureSatModel(model_proto, &wall_timer, model);
      response.set_wall_time(wall_timer.Get());
      response.set_user_time(user_timer.Get());
      response.set_deterministic_time(
          shared_time_limit.GetElapsedDeterministicTime());
      const SatParameters& params = *model->GetOrCreate<SatParameters>();
      if (params.fill_tightened_domains_in_response()) {
        *response.mutable_tightened_variables() = model_proto.variables();
      }
      LOG_IF(INFO, log_search) << CpSolverResponseStats(response);
      return response;
    }
  }

  // Presolve and expansions
  LOG_IF(INFO, log_search) << absl::StrFormat(
      "*** starting model presolve at %.2fs", wall_timer.Get());
  CpModelProto new_cp_model_proto = model_proto;  // Copy.
  CpModelProto mapping_proto;
  PresolveOptions options;
  options.log_info = log_search;
  options.parameters = *model->GetOrCreate<SatParameters>();
  options.time_limit = model->GetOrCreate<TimeLimit>();
  auto context =
      absl::make_unique<PresolveContext>(&new_cp_model_proto, &mapping_proto);

  // This function will be called before any CpSolverResponse is returned
  // to the user (at the end and in callbacks).
  std::function<void(CpSolverResponse * response)> postprocess_solution;

  // Do the actual presolve.
  std::vector<int> postsolve_mapping;
  const bool ok = PresolveCpModel(options, context.get(), &postsolve_mapping);
  if (!ok) {
    LOG(ERROR) << "Error while presolving, likely due to integer overflow.";
    CpSolverResponse response;
    response.set_status(CpSolverStatus::MODEL_INVALID);
    LOG_IF(INFO, log_search) << CpSolverResponseStats(response);
    return response;
  }
  LOG_IF(INFO, log_search) << CpModelStats(new_cp_model_proto);
  if (params.cp_model_presolve()) {
    postprocess_solution = [&model_proto, &params, mapping_proto,
                            &shared_time_limit, postsolve_mapping, &wall_timer,
                            &user_timer](CpSolverResponse* response) {
      // Note that it is okay to use the initial model_proto in the postsolve
      // even though we called PresolveCpModel() on the expanded proto. This is
      // because PostsolveResponse() only use the proto to known the number of
      // variables to fill in the response and to check the solution feasibility
      // of these variables.
      PostsolveResponse("main solver", model_proto, mapping_proto,
                        postsolve_mapping, &wall_timer, response);
      if (params.fill_tightened_domains_in_response()) {
        // TODO(user): for now, we just use the domain infered during presolve.
        if (mapping_proto.variables().size() >=
            model_proto.variables().size()) {
          for (int i = 0; i < model_proto.variables().size(); ++i) {
            *response->add_tightened_variables() = mapping_proto.variables(i);
          }
        }
      }
      response->set_wall_time(wall_timer.Get());
      response->set_user_time(user_timer.Get());
      response->set_deterministic_time(
          shared_time_limit.GetElapsedDeterministicTime());
    };
  } else {
    postprocess_solution = [&model_proto, &params, &wall_timer,
                            &shared_time_limit,
                            &user_timer](CpSolverResponse* response) {
      // Truncate the solution in case model expansion added more variables.
      const int initial_size = model_proto.variables_size();
      if (response->solution_size() > 0) {
        response->mutable_solution()->Truncate(initial_size);
      } else if (response->solution_lower_bounds_size() > 0) {
        response->mutable_solution_lower_bounds()->Truncate(initial_size);
        response->mutable_solution_upper_bounds()->Truncate(initial_size);
      }
      if (params.fill_tightened_domains_in_response()) {
        *response->mutable_tightened_variables() = model_proto.variables();
      }
      response->set_wall_time(wall_timer.Get());
      response->set_user_time(user_timer.Get());
      response->set_deterministic_time(
          shared_time_limit.GetElapsedDeterministicTime());
    };
  }

  // Delete the context.
  context.reset(nullptr);

  SharedResponseManager shared_response_manager(
      log_search, params.enumerate_all_solutions(), &new_cp_model_proto,
      &wall_timer, &shared_time_limit);
  const auto& observers = model->GetOrCreate<SolutionObservers>()->observers;
  if (!observers.empty()) {
    shared_response_manager.AddSolutionCallback(
        [&model_proto, &observers, &wall_timer, &user_timer,
         &postprocess_solution, &shared_time_limit](
            const CpSolverResponse& response_of_presolved_problem) {
          // If we stopped (for instance because of stop_after_first_solution)
          // then we don't want to report solutions that might just be in
          // flight.
          if (shared_time_limit.LimitReached()) return;

          CpSolverResponse response = response_of_presolved_problem;
          postprocess_solution(&response);
          if (!response.solution().empty()) {
            if (DEBUG_MODE || FLAGS_cp_model_check_intermediate_solutions) {
              CHECK(SolutionIsFeasible(
                  model_proto, std::vector<int64>(response.solution().begin(),
                                                  response.solution().end())));
            }
          }

          for (const auto& observer : observers) {
            observer(response);
          }
        });
  }

  // Make sure everything stops when we have a first solution if requested.
  if (params.stop_after_first_solution()) {
    shared_response_manager.AddSolutionCallback(
        [&shared_time_limit](
            const CpSolverResponse& response_of_presolved_problem) {
          shared_time_limit.Stop();
        });
  }

#if defined(__PORTABLE_PLATFORM__)
  if (/* DISABLES CODE */ (false)) {
    // We ignore the multithreading parameter in this case.
#else   // __PORTABLE_PLATFORM__
  if (params.num_search_workers() > 1 || params.interleave_search()) {
    SolveCpModelParallel(new_cp_model_proto, &shared_response_manager,
                         &shared_time_limit, &wall_timer, model);
#endif  // __PORTABLE_PLATFORM__
  } else {
    if (log_search) {
      LOG(INFO) << absl::StrFormat("*** starting to load the model at %.2fs",
                                   wall_timer.Get());
    }
    LoadCpModel(new_cp_model_proto, &shared_response_manager, model);
    if (log_search) {
      LOG(INFO) << absl::StrFormat("*** starting sequential search at %.2fs",
                                   wall_timer.Get());
      LOG(INFO) << "Initial num_bool: "
                << model->Get<SatSolver>()->NumVariables();
    }
    QuickSolveWithHint(new_cp_model_proto, &shared_response_manager, model);
    SolveLoadedCpModel(new_cp_model_proto, &shared_response_manager, model);
  }

  CpSolverResponse response = shared_response_manager.GetResponse();
  postprocess_solution(&response);
  if (!response.solution().empty()) {
    CHECK(SolutionIsFeasible(model_proto,
                             std::vector<int64>(response.solution().begin(),
                                                response.solution().end())));
  }
  LOG_IF(INFO, log_search) << CpSolverResponseStats(response);
  return response;
}

CpSolverResponse Solve(const CpModelProto& model_proto) {
  Model model;
  return SolveCpModel(model_proto, &model);
}

CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const SatParameters& params) {
  Model model;
  model.Add(NewSatParameters(params));
  return SolveCpModel(model_proto, &model);
}

#if !defined(__PORTABLE_PLATFORM__)
CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const std::string& params) {
  Model model;
  model.Add(NewSatParameters(params));
  return SolveCpModel(model_proto, &model);
}
#endif  // !__PORTABLE_PLATFORM__

}  // namespace sat
}  // namespace operations_research
