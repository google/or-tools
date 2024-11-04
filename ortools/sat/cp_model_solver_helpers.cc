// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/cp_model_solver_helpers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#endif  // __PORTABLE_PLATFORM__
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/connected_components.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_postsolve.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/feasibility_pump.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/lb_tree_search.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/max_hs.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/symmetry_util.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/sat/work_assignment.h"
#include "ortools/util/logging.h"
#if !defined(__PORTABLE_PLATFORM__)
#endif  // __PORTABLE_PLATFORM__
#include "ortools/base/version.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

ABSL_FLAG(bool, cp_model_dump_models, false,
          "DEBUG ONLY. When set to true, SolveCpModel() will dump its model "
          "protos (original model, presolved model, mapping model) in text "
          "format to 'FLAGS_cp_model_dump_prefix'{model|presolved_model|"
          "mapping_model}.pb.txt.");

#if defined(_MSC_VER)
ABSL_FLAG(std::string, cp_model_dump_prefix, ".\\",
          "Prefix filename for all dumped files");
#else
ABSL_FLAG(std::string, cp_model_dump_prefix, "/tmp/",
          "Prefix filename for all dumped files");
#endif

ABSL_FLAG(bool, cp_model_dump_submodels, false,
          "DEBUG ONLY. When set to true, solve will dump all "
          "lns or objective_shaving submodels proto in text format to "
          "'FLAGS_cp_model_dump_prefix'xxx.pb.txt.");

ABSL_FLAG(
    std::string, cp_model_load_debug_solution, "",
    "DEBUG ONLY. When this is set to a non-empty file name, "
    "we will interpret this as an internal solution which can be used for "
    "debugging. For instance we use it to identify wrong cuts/reasons.");

namespace operations_research {
namespace sat {

// This should be called on the presolved model. It will read the file
// specified by --cp_model_load_debug_solution and properly fill the
// model->Get<DebugSolution>() proto vector.
void LoadDebugSolution(const CpModelProto& model_proto, Model* model) {
#if !defined(__PORTABLE_PLATFORM__)
  if (absl::GetFlag(FLAGS_cp_model_load_debug_solution).empty()) return;

  CpSolverResponse response;
  SOLVER_LOG(model->GetOrCreate<SolverLogger>(),
             "Reading debug solution from '",
             absl::GetFlag(FLAGS_cp_model_load_debug_solution), "'.");
  CHECK_OK(file::GetTextProto(absl::GetFlag(FLAGS_cp_model_load_debug_solution),
                              &response, file::Defaults()));

  // Make sure we load a solution with the same number of variable has in the
  // presolved model.
  CHECK_EQ(response.solution().size(), model_proto.variables().size());
  model->GetOrCreate<SharedResponseManager>()->LoadDebugSolution(
      response.solution());
#endif  // __PORTABLE_PLATFORM__
}

// This both copy the "main" DebugSolution to a local_model and also cache
// the value of the integer variables in that solution.
void InitializeDebugSolution(const CpModelProto& model_proto, Model* model) {
  auto* shared_response = model->Get<SharedResponseManager>();
  if (shared_response == nullptr) return;
  if (shared_response->DebugSolution().empty()) return;

  // Copy the proto values.
  DebugSolution& debug_sol = *model->GetOrCreate<DebugSolution>();
  debug_sol.proto_values = shared_response->DebugSolution();

  // Fill the values by integer variable.
  const int num_integers =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value();
  debug_sol.ivar_has_value.assign(num_integers, false);
  debug_sol.ivar_values.assign(num_integers, 0);

  std::vector<Literal> boolean_solution;

  const auto& mapping = *model->GetOrCreate<CpModelMapping>();
  for (int i = 0; i < debug_sol.proto_values.size(); ++i) {
    if (mapping.IsBoolean(i)) {
      Literal l = mapping.Literal(i);
      if (debug_sol.proto_values[i] == 0) {
        l = l.Negated();
      }
      boolean_solution.push_back(l);
    }

    if (!mapping.IsInteger(i)) continue;
    const IntegerVariable var = mapping.Integer(i);
    debug_sol.ivar_has_value[var] = true;
    debug_sol.ivar_has_value[NegationOf(var)] = true;
    debug_sol.ivar_values[var] = debug_sol.proto_values[i];
    debug_sol.ivar_values[NegationOf(var)] = -debug_sol.proto_values[i];
  }

  // If the solution is fully boolean (there is no integer variable), and
  // we have a decision problem (so no new boolean should be created), we load
  // it in the sat solver for debugging too.
  if (boolean_solution.size() == debug_sol.proto_values.size() &&
      !model_proto.has_objective()) {
    LOG(INFO) << "Loaded pure Boolean debugging solution.";
    model->GetOrCreate<SatSolver>()->LoadDebugSolution(boolean_solution);
  }

  // The objective variable is usually not part of the proto, but it is still
  // nice to have it, so we recompute it here.
  auto* objective_def = model->Get<ObjectiveDefinition>();
  if (objective_def != nullptr) {
    const IntegerVariable objective_var = objective_def->objective_var;
    const int64_t objective_value =
        ComputeInnerObjective(model_proto.objective(), debug_sol.proto_values);
    debug_sol.ivar_has_value[objective_var] = true;
    debug_sol.ivar_has_value[NegationOf(objective_var)] = true;
    debug_sol.ivar_values[objective_var] = objective_value;
    debug_sol.ivar_values[NegationOf(objective_var)] = -objective_value;
  }

  // We also register a DEBUG callback to check our reasons.
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  const auto checker = [mapping, encoder, debug_sol, model](
                           absl::Span<const Literal> clause,
                           absl::Span<const IntegerLiteral> integers) {
    bool is_satisfied = false;
    int num_bools = 0;
    int num_ints = 0;
    std::vector<std::tuple<Literal, IntegerLiteral, int>> to_print;
    for (const Literal l : clause) {
      // First case, this Boolean is mapped.
      {
        const int proto_var =
            mapping.GetProtoVariableFromBooleanVariable(l.Variable());
        if (proto_var != -1) {
          to_print.push_back({l, IntegerLiteral(), proto_var});
          if (debug_sol.proto_values[proto_var] == (l.IsPositive() ? 1 : 0)) {
            is_satisfied = true;
            break;
          }
          ++num_bools;
          continue;
        }
      }

      // Second case, it is associated to IntVar >= value.
      // We can use any of them, so if one is false, we use this one.
      bool all_true = true;
      for (const IntegerLiteral associated : encoder->GetIntegerLiterals(l)) {
        const int proto_var = mapping.GetProtoVariableFromIntegerVariable(
            PositiveVariable(associated.var));
        if (proto_var == -1) break;
        int64_t value = debug_sol.proto_values[proto_var];
        to_print.push_back({l, associated, proto_var});

        if (!VariableIsPositive(associated.var)) value = -value;
        if (value < associated.bound) {
          ++num_ints;
          all_true = false;
          break;
        }
      }
      if (all_true) {
        is_satisfied = true;
        break;
      }
    }
    for (const IntegerLiteral i_lit : integers) {
      const int proto_var = mapping.GetProtoVariableFromIntegerVariable(
          PositiveVariable(i_lit.var));
      if (proto_var == -1) {
        is_satisfied = true;
        break;
      }

      int64_t value = debug_sol.proto_values[proto_var];
      to_print.push_back({Literal(kNoLiteralIndex), i_lit, proto_var});

      if (!VariableIsPositive(i_lit.var)) value = -value;
      // Note the sign is inversed, we cannot have all literal false and all
      // integer literal true.
      if (value >= i_lit.bound) {
        is_satisfied = true;
        break;
      }
    }
    if (!is_satisfied) {
      LOG(INFO) << "Reason clause is not satisfied by loaded solution:";
      LOG(INFO) << "Worker '" << model->Name() << "', level="
                << model->GetOrCreate<SatSolver>()->CurrentDecisionLevel();
      LOG(INFO) << "literals (neg): " << clause;
      LOG(INFO) << "integer literals: " << integers;
      for (const auto [l, i_lit, proto_var] : to_print) {
        LOG(INFO) << l << " " << i_lit << " var=" << proto_var
                  << " value_in_sol=" << debug_sol.proto_values[proto_var];
      }
    }
    return is_satisfied;
  };
  const auto lit_checker = [checker](absl::Span<const Literal> clause) {
    return checker(clause, {});
  };

  model->GetOrCreate<Trail>()->RegisterDebugChecker(lit_checker);
  model->GetOrCreate<IntegerTrail>()->RegisterDebugChecker(checker);
}

std::vector<int64_t> GetSolutionValues(const CpModelProto& model_proto,
                                       const Model& model) {
  auto* mapping = model.Get<CpModelMapping>();
  auto* trail = model.Get<Trail>();

  std::vector<int64_t> solution;
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsInteger(i)) {
      const IntegerVariable var = mapping->Integer(i);

      // For ignored or not fully instantiated variable, we just use the
      // lower bound.
      solution.push_back(model.Get(LowerBound(var)));
    } else {
      DCHECK(mapping->IsBoolean(i));
      const Literal literal = mapping->Literal(i);
      if (trail->Assignment().LiteralIsAssigned(literal)) {
        solution.push_back(model.Get(Value(literal)));
      } else {
        // Just use the lower bound if the variable is not fully instantiated.
        solution.push_back(0);
      }
    }
  }
  return solution;
}

namespace {

IntegerVariable GetOrCreateVariableWithTightBound(
    const std::vector<std::pair<IntegerVariable, int64_t>>& terms,
    Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  int64_t sum_min = 0;
  int64_t sum_max = 0;
  for (const std::pair<IntegerVariable, int64_t>& var_coeff : terms) {
    const int64_t min_domain = model->Get(LowerBound(var_coeff.first));
    const int64_t max_domain = model->Get(UpperBound(var_coeff.first));
    const int64_t coeff = var_coeff.second;
    const int64_t prod1 = min_domain * coeff;
    const int64_t prod2 = max_domain * coeff;
    sum_min += std::min(prod1, prod2);
    sum_max += std::max(prod1, prod2);
  }
  return model->Add(NewIntegerVariable(sum_min, sum_max));
}

IntegerVariable GetOrCreateVariableLinkedToSumOf(
    const std::vector<std::pair<IntegerVariable, int64_t>>& terms,
    bool lb_required, bool ub_required, Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  const IntegerVariable new_var =
      GetOrCreateVariableWithTightBound(terms, model);

  // TODO(user): use the same format, i.e. LinearExpression in both code!
  std::vector<IntegerVariable> vars;
  std::vector<int64_t> coeffs;
  for (const auto [var, coeff] : terms) {
    vars.push_back(var);
    coeffs.push_back(coeff);
  }
  vars.push_back(new_var);
  coeffs.push_back(-1);

  // Split if linear is large.
  if (vars.size() > model->GetOrCreate<SatParameters>()->linear_split_size()) {
    SplitAndLoadIntermediateConstraints(lb_required, ub_required, &vars,
                                        &coeffs, model);
  }

  // Load the top-level constraint with the required sides.
  if (lb_required) {
    model->Add(WeightedSumGreaterOrEqual(vars, coeffs, 0));
  }
  if (ub_required) {
    model->Add(WeightedSumLowerOrEqual(vars, coeffs, 0));
  }

  return new_var;
}

// Currently, the LP will exploit symmetry if we load some in the
// LinearConstraintSymmetrizer. So not loading them disable the feature.
//
// TODO(user): We probably want to separate the two as we could still use orbits
// in other places while not doing so in the LP.
void InitializeLinearConstraintSymmetrizerIfRequested(
    const CpModelProto& model_proto, const LinearRelaxation& linear_relaxation,
    Model* m) {
  if (!model_proto.has_symmetry()) return;

  auto* params = m->GetOrCreate<SatParameters>();
  if (params->linearization_level() < 2) return;
  if (!params->use_symmetry_in_lp()) return;

  // Tricky: while we load the model, we might create new integer-variables, and
  // in some rare case, these variable can appear in the LP relaxation. This
  // might happen when we extend an at most one or when we use an integer
  // encoding.
  //
  // The issue with this and having symmetry is that we didn't extend the
  // problem symmetries to include these new variables, so we can derive wrong
  // conclusion. When we use symmetry in the LP we cannot have any variable like
  // this part of a LinearProgrammingConstraint.
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  int num_constraints_with_non_proto_variables = 0;
  for (const auto& lp_constraint : linear_relaxation.linear_constraints) {
    bool has_non_proto_variable = false;
    for (const IntegerVariable var : lp_constraint.VarsAsSpan()) {
      if (mapping->GetProtoVariableFromIntegerVariable(var) == -1) {
        has_non_proto_variable = true;
        break;
      }
    }
    if (has_non_proto_variable) {
      ++num_constraints_with_non_proto_variables;
    }
  }
  if (num_constraints_with_non_proto_variables > 0) {
    // TODO(user): Logging like this is not visible in multi-thread, so we will
    // not have a lot of warning if this happens a lot.
    auto* logger = m->GetOrCreate<SolverLogger>();
    SOLVER_LOG(logger, num_constraints_with_non_proto_variables,
               " LP constraints uses new variables not appearing in the "
               "presolved model. ");

    // TODO(user): We currently disable symmetries in LP completely when this
    // happen, but we could probably be smarter about this. I am not really
    // sure we want to create such extra variable in the first place :)
    return;
  }

  // Convert to SparsePermutation.
  const int num_vars = model_proto.variables().size();
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  for (const SparsePermutationProto& perm :
       model_proto.symmetry().permutations()) {
    generators.emplace_back(CreateSparsePermutationFromProto(num_vars, perm));
  }

  // Get orbits in term of IntegerVariable.
  const std::vector<int> var_to_orbit_index = GetOrbits(num_vars, generators);
  std::vector<bool> orbit_is_ok;
  std::vector<std::vector<IntegerVariable>> orbits;
  for (int proto_var = 0; proto_var < num_vars; ++proto_var) {
    const int orbit_index = var_to_orbit_index[proto_var];
    if (orbit_index == -1) continue;
    if (orbit_index >= orbits.size()) {
      orbits.resize(orbit_index + 1);
      orbit_is_ok.resize(orbit_index + 1, true);
    }

    // In linerization level >=2, all variables should have a view.
    // Otherwise revisit and skip orbit without a full view.
    const IntegerVariable var = mapping->Integer(proto_var);
    CHECK_NE(var, kNoIntegerVariable);
    orbits[orbit_index].push_back(var);
  }

  // Lets create the orbit sum vars and register each orbit.
  auto* symmetrizer = m->GetOrCreate<LinearConstraintSymmetrizer>();
  std::vector<std::pair<IntegerVariable, int64_t>> terms;
  for (const std::vector<IntegerVariable>& orbit : orbits) {
    terms.clear();
    for (const IntegerVariable var : orbit) {
      terms.push_back({var, 1});
    }
    const IntegerVariable sum_var =
        GetOrCreateVariableLinkedToSumOf(terms, true, true, m);
    symmetrizer->AddSymmetryOrbit(sum_var, orbit);
  }
}

// Adds one LinearProgrammingConstraint per connected component of the model.
IntegerVariable AddLPConstraints(bool objective_need_to_be_tight,
                                 const CpModelProto& model_proto, Model* m) {
  // Non const as we will std::move() stuff out of there.
  LinearRelaxation relaxation = ComputeLinearRelaxation(model_proto, m);
  if (m->GetOrCreate<SatSolver>()->ModelIsUnsat()) return kNoIntegerVariable;

  // Load symmetry?
  InitializeLinearConstraintSymmetrizerIfRequested(model_proto, relaxation, m);

  // The bipartite graph of LP constraints might be disconnected:
  // make a partition of the variables into connected components.
  // Constraint nodes are indexed by [0..num_lp_constraints),
  // variable nodes by [num_lp_constraints..num_lp_constraints+num_variables).
  //
  // TODO(user): look into biconnected components.
  const int num_lp_constraints =
      static_cast<int>(relaxation.linear_constraints.size());
  const int num_lp_cut_generators =
      static_cast<int>(relaxation.cut_generators.size());
  const int num_integer_variables =
      m->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value();

  DenseConnectedComponentsFinder components;
  components.SetNumberOfNodes(num_lp_constraints + num_lp_cut_generators +
                              num_integer_variables);
  auto get_constraint_index = [](int ct_index) { return ct_index; };
  auto get_cut_generator_index = [num_lp_constraints](int cut_index) {
    return num_lp_constraints + cut_index;
  };
  auto get_var_index = [num_lp_constraints,
                        num_lp_cut_generators](IntegerVariable var) {
    return num_lp_constraints + num_lp_cut_generators +
           PositiveVariable(var).value();
  };
  for (int i = 0; i < num_lp_constraints; i++) {
    for (const IntegerVariable var :
         relaxation.linear_constraints[i].VarsAsSpan()) {
      components.AddEdge(get_constraint_index(i), get_var_index(var));
    }
  }
  for (int i = 0; i < num_lp_cut_generators; ++i) {
    for (const IntegerVariable var : relaxation.cut_generators[i].vars) {
      components.AddEdge(get_cut_generator_index(i), get_var_index(var));
    }
  }

  // Make sure variables from the same orbit end up in same components.
  auto* symmetrizer = m->GetOrCreate<LinearConstraintSymmetrizer>();
  for (int i = 0; i < symmetrizer->NumOrbits(); ++i) {
    const int representative = get_var_index(symmetrizer->OrbitSumVar(i));
    for (const IntegerVariable var : symmetrizer->Orbit(i)) {
      components.AddEdge(representative, get_var_index(var));
    }
  }

  const int num_components = components.GetNumberOfComponents();
  std::vector<int> component_sizes(num_components, 0);
  const std::vector<int> index_to_component = components.GetComponentIds();
  for (int i = 0; i < num_lp_constraints; i++) {
    ++component_sizes[index_to_component[get_constraint_index(i)]];
  }
  for (int i = 0; i < num_lp_cut_generators; i++) {
    ++component_sizes[index_to_component[get_cut_generator_index(i)]];
  }

  // TODO(user): Optimize memory layout.
  std::vector<std::vector<IntegerVariable>> component_to_var(num_components);
  for (IntegerVariable var(0); var < num_integer_variables; var += 2) {
    DCHECK(VariableIsPositive(var));
    component_to_var[index_to_component[get_var_index(var)]].push_back(var);
  }

  // Make sure any constraint that touch the objective is not discarded even
  // if it is the only one in its component. This is important to propagate
  // as much as possible the objective bound by using any bounds the LP give
  // us on one of its components. This is critical on the zephyrus problems for
  // instance.
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
    const IntegerVariable var =
        mapping->Integer(model_proto.objective().vars(i));
    ++component_sizes[index_to_component[get_var_index(var)]];
  }

  // Dispatch every constraint to its LinearProgrammingConstraint.
  std::vector<LinearProgrammingConstraint*> lp_constraints(num_components,
                                                           nullptr);
  for (int i = 0; i < num_lp_constraints; i++) {
    const int c = index_to_component[get_constraint_index(i)];
    if (component_sizes[c] <= 1) continue;
    if (lp_constraints[c] == nullptr) {
      lp_constraints[c] =
          new LinearProgrammingConstraint(m, component_to_var[c]);
      m->TakeOwnership(lp_constraints[c]);
    }
    // Load the constraint.
    if (!lp_constraints[c]->AddLinearConstraint(
            std::move(relaxation.linear_constraints[i]))) {
      m->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
      return kNoIntegerVariable;
    }
  }

  // Dispatch every cut generator to its LinearProgrammingConstraint.
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int c = index_to_component[get_cut_generator_index(i)];
    if (lp_constraints[c] == nullptr) {
      lp_constraints[c] =
          new LinearProgrammingConstraint(m, component_to_var[c]);
      m->TakeOwnership(lp_constraints[c]);
    }
    lp_constraints[c]->AddCutGenerator(std::move(relaxation.cut_generators[i]));
  }

  // We deal with the clique cut generator here now that the component have
  // been computed. As we don't want to merge independent component with it.
  auto* params = m->GetOrCreate<SatParameters>();
  if (params->linearization_level() > 1 && params->add_clique_cuts() &&
      params->cut_level() > 0) {
    for (LinearProgrammingConstraint* lp : lp_constraints) {
      if (lp == nullptr) continue;
      lp->AddCutGenerator(CreateCliqueCutGenerator(lp->integer_variables(), m));
    }
  }

  // Add the objective.
  std::vector<std::vector<std::pair<IntegerVariable, int64_t>>>
      component_to_cp_terms(num_components);
  std::vector<std::pair<IntegerVariable, int64_t>> top_level_cp_terms;
  int num_components_containing_objective = 0;
  if (model_proto.has_objective()) {
    // First convert the proto objective to an IntegerVariable one. In case of
    // "use_symmetry_in_lp", we also rewrite it in terms of the sum of the
    // variables in the orbits.
    std::vector<std::pair<IntegerVariable, int64_t>> objective;
    const int num_orbits = symmetrizer->NumOrbits();
    if (num_orbits > 0) {
      // We use the orbit_sum var instead.
      std::vector<int64_t> orbit_obj_coeff(num_orbits, 0);
      for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
        const IntegerVariable var =
            mapping->Integer(model_proto.objective().vars(i));
        const int64_t coeff = model_proto.objective().coeffs(i);
        const int orbit_index = symmetrizer->OrbitIndex(var);
        if (orbit_index != -1) {
          if (orbit_obj_coeff[orbit_index] == 0) {
            orbit_obj_coeff[orbit_index] = coeff;
          } else {
            CHECK_EQ(orbit_obj_coeff[orbit_index], coeff);
          }
          continue;
        }
        objective.push_back({var, coeff});
      }
      for (int i = 0; i < num_orbits; ++i) {
        if (orbit_obj_coeff[i] == 0) continue;
        objective.push_back({symmetrizer->OrbitSumVar(i), orbit_obj_coeff[i]});
      }
    } else {
      for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
        const IntegerVariable var =
            mapping->Integer(model_proto.objective().vars(i));
        const int64_t coeff = model_proto.objective().coeffs(i);
        objective.push_back({var, coeff});
      }
    }

    // First pass: set objective coefficients on the lp constraints, and store
    // the cp terms in one vector per component.
    for (const auto [var, coeff] : objective) {
      const int c = index_to_component[get_var_index(var)];
      if (lp_constraints[c] != nullptr) {
        lp_constraints[c]->SetObjectiveCoefficient(var, IntegerValue(coeff));
        component_to_cp_terms[c].push_back(std::make_pair(var, coeff));
      } else {
        // Component is too small. We still need to store the objective term.
        top_level_cp_terms.push_back(std::make_pair(var, coeff));
      }
    }
    // Second pass: Build the cp sub-objectives per component.
    for (int c = 0; c < num_components; ++c) {
      if (component_to_cp_terms[c].empty()) continue;
      const IntegerVariable sub_obj_var = GetOrCreateVariableLinkedToSumOf(
          component_to_cp_terms[c], objective_need_to_be_tight, true, m);
      top_level_cp_terms.push_back(std::make_pair(sub_obj_var, 1));
      lp_constraints[c]->SetMainObjectiveVariable(sub_obj_var);
      num_components_containing_objective++;
    }
  }

  const IntegerVariable main_objective_var =
      model_proto.has_objective()
          ? GetOrCreateVariableLinkedToSumOf(
                top_level_cp_terms, objective_need_to_be_tight, true, m)
          : kNoIntegerVariable;

  // Register LP constraints. Note that this needs to be done after all the
  // constraints have been added.
  for (LinearProgrammingConstraint* lp_constraint : lp_constraints) {
    if (lp_constraint == nullptr) continue;
    lp_constraint->RegisterWith(m);
    VLOG(3) << "LP constraint: " << lp_constraint->DimensionString() << ".";
  }

  VLOG(3) << top_level_cp_terms.size()
          << " terms in the main objective linear equation ("
          << num_components_containing_objective << " from LP constraints).";
  return main_objective_var;
}

}  // namespace

// Registers a callback that will export variables bounds fixed at level 0 of
// the search. This should not be registered to a LNS search.
void RegisterVariableBoundsLevelZeroExport(
    const CpModelProto& /*model_proto*/,
    SharedBoundsManager* shared_bounds_manager, Model* model) {
  CHECK(shared_bounds_manager != nullptr);

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  auto* trail = model->Get<Trail>();
  auto* integer_trail = model->Get<IntegerTrail>();

  int saved_trail_index = 0;
  std::vector<int> model_variables;
  std::vector<int64_t> new_lower_bounds;
  std::vector<int64_t> new_upper_bounds;
  absl::flat_hash_set<int> visited_variables;
  const std::string name = model->Name();

  auto broadcast_level_zero_bounds =
      [=](const std::vector<IntegerVariable>& modified_vars) mutable {
        // Inspect the modified IntegerVariables.
        for (const IntegerVariable& var : modified_vars) {
          const IntegerVariable positive_var = PositiveVariable(var);
          const int model_var =
              mapping->GetProtoVariableFromIntegerVariable(positive_var);

          if (model_var == -1) continue;
          const auto [_, inserted] = visited_variables.insert(model_var);
          if (!inserted) continue;

          const int64_t new_lb =
              integer_trail->LevelZeroLowerBound(positive_var).value();
          const int64_t new_ub =
              integer_trail->LevelZeroUpperBound(positive_var).value();

          // TODO(user): We could imagine an API based on atomic<int64_t>
          // that could preemptively check if this new bounds are improving.
          model_variables.push_back(model_var);
          new_lower_bounds.push_back(new_lb);
          new_upper_bounds.push_back(new_ub);
        }

        // Inspect the newly modified Booleans.
        for (; saved_trail_index < trail->Index(); ++saved_trail_index) {
          const Literal fixed_literal = (*trail)[saved_trail_index];
          const int model_var = mapping->GetProtoVariableFromBooleanVariable(
              fixed_literal.Variable());

          if (model_var == -1) continue;
          const auto [_, inserted] = visited_variables.insert(model_var);
          if (!inserted) continue;

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
          shared_bounds_manager->ReportPotentialNewBounds(
              model->Name(), model_variables, new_lower_bounds,
              new_upper_bounds);

          // Clear for next call.
          model_variables.clear();
          new_lower_bounds.clear();
          new_upper_bounds.clear();
          visited_variables.clear();

          // If we are not in interleave_search we synchronize right away.
          if (!model->Get<SatParameters>()->interleave_search()) {
            shared_bounds_manager->Synchronize();
          }
        }
      };

  // The callback will just be called on NEWLY modified var. So initially,
  // we do want to read all variables.
  //
  // TODO(user): Find a better way? It seems nicer to register this before
  // any variable is modified. But then we don't want to call it each time
  // we reach level zero during probing. It should be better to only call
  // it when a new variable has been fixed.
  const IntegerVariable num_vars =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables();
  std::vector<IntegerVariable> all_variables;
  all_variables.reserve(num_vars.value());
  for (IntegerVariable var(0); var < num_vars; ++var) {
    all_variables.push_back(var);
  }
  broadcast_level_zero_bounds(all_variables);

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
  const std::string name = model->Name();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* trail = model->GetOrCreate<Trail>();
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const int id = shared_bounds_manager->RegisterNewId();

  const auto& import_level_zero_bounds = [&model_proto, shared_bounds_manager,
                                          name, sat_solver, integer_trail,
                                          trail, id, mapping]() {
    std::vector<int> model_variables;
    std::vector<int64_t> new_lower_bounds;
    std::vector<int64_t> new_upper_bounds;
    shared_bounds_manager->GetChangedBounds(
        id, &model_variables, &new_lower_bounds, &new_upper_bounds);
    for (int i = 0; i < model_variables.size(); ++i) {
      const int model_var = model_variables[i];

      // If this is a Boolean, fix it if not already done.
      // Note that it is important not to use AddUnitClause() as we do not
      // want to propagate after each addition.
      if (mapping->IsBoolean(model_var)) {
        Literal lit = mapping->Literal(model_var);
        if (new_upper_bounds[i] == 0) lit = lit.Negated();
        if (trail->Assignment().LiteralIsTrue(lit)) continue;
        if (trail->Assignment().LiteralIsFalse(lit)) {
          sat_solver->NotifyThatModelIsUnsat();
          return false;
        }
        trail->EnqueueWithUnitReason(lit);
        continue;
      }

      // Deal with integer.
      if (!mapping->IsInteger(model_var)) continue;
      const IntegerVariable var = mapping->Integer(model_var);
      const IntegerValue new_lb(new_lower_bounds[i]);
      const IntegerValue new_ub(new_upper_bounds[i]);
      const IntegerValue old_lb = integer_trail->LowerBound(var);
      const IntegerValue old_ub = integer_trail->UpperBound(var);
      const bool changed_lb = new_lb > old_lb;
      const bool changed_ub = new_ub < old_ub;
      if (!changed_lb && !changed_ub) continue;

      if (VLOG_IS_ON(3)) {
        const IntegerVariableProto& var_proto =
            model_proto.variables(model_var);
        const std::string& var_name =
            var_proto.name().empty()
                ? absl::StrCat("anonymous_var(", model_var, ")")
                : var_proto.name();
        LOG(INFO) << "  '" << name << "' imports new bounds for " << var_name
                  << ": from [" << old_lb << ", " << old_ub << "] to ["
                  << new_lb << ", " << new_ub << "]";
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

    // Note that we will propagate if they are new bounds separately.
    // See BeforeTakingDecision().
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
  auto* integer_trail = model->Get<IntegerTrail>();
  const auto broadcast_objective_lower_bound =
      [objective_var, integer_trail, shared_response_manager, model,
       best_obj_lb =
           kMinIntegerValue](const std::vector<IntegerVariable>&) mutable {
        const IntegerValue objective_lb =
            integer_trail->LevelZeroLowerBound(objective_var);
        if (objective_lb > best_obj_lb) {
          best_obj_lb = objective_lb;
          shared_response_manager->UpdateInnerObjectiveBounds(
              model->Name(), objective_lb,
              integer_trail->LevelZeroUpperBound(objective_var));
          // If we are not in interleave_search we synchronize right away.
          if (!model->Get<SatParameters>()->interleave_search()) {
            shared_response_manager->Synchronize();
          }
        }
      };
  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(
          broadcast_objective_lower_bound);
}

// Registers a callback to import new objective bounds. It will be called each
// time the search main loop is back to level zero. Note that it the presence of
// assumptions, this will not happen until the set of assumptions is changed.
void RegisterObjectiveBoundsImport(
    SharedResponseManager* shared_response_manager, Model* model) {
  auto* solver = model->GetOrCreate<SatSolver>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* objective = model->GetOrCreate<ObjectiveDefinition>();
  const std::string name = model->Name();
  const auto import_objective_bounds = [name, solver, integer_trail, objective,
                                        shared_response_manager]() {
    if (solver->AssumptionLevel() != 0) return true;
    bool tighter_bounds = false;

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
      tighter_bounds = true;
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
      tighter_bounds = true;
    }

    // Note that we will propagate if they are new bounds separately.
    // See BeforeTakingDecision().
    if (tighter_bounds) {
      VLOG(3) << "'" << name << "' imports objective bounds: external ["
              << objective->ScaleIntegerObjective(external_lb) << ", "
              << objective->ScaleIntegerObjective(external_ub) << "], current ["
              << objective->ScaleIntegerObjective(current_lb) << ", "
              << objective->ScaleIntegerObjective(current_ub) << "]";
    }

    return true;
  };

  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_objective_bounds);
}

// Registers a callback that will export good clauses discovered during search.
void RegisterClausesExport(int id, SharedClausesManager* shared_clauses_manager,
                           Model* model) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const auto& share_binary_clause = [mapping, id, shared_clauses_manager](
                                        Literal l1, Literal l2) {
    const int var1 =
        mapping->GetProtoVariableFromBooleanVariable(l1.Variable());
    if (var1 == -1) return;
    const int var2 =
        mapping->GetProtoVariableFromBooleanVariable(l2.Variable());
    if (var2 == -1) return;
    const int lit1 = l1.IsPositive() ? var1 : NegatedRef(var1);
    const int lit2 = l2.IsPositive() ? var2 : NegatedRef(var2);
    shared_clauses_manager->AddBinaryClause(id, lit1, lit2);
  };
  model->GetOrCreate<BinaryImplicationGraph>()->SetAdditionCallback(
      share_binary_clause);
  if (!model->GetOrCreate<SatParameters>()->share_glue_clauses()) {
    return;
  }
  auto* clause_stream = shared_clauses_manager->GetClauseStream(id);
  const int max_lbd =
      model->GetOrCreate<SatParameters>()->clause_cleanup_lbd_bound();
  // Note that this callback takes no global locks, everything operates on this
  // worker's own clause stream, whose lock is only used by this worker, and
  // briefly when generating a batch in SharedClausesManager::Synchronize().
  auto share_clause = [mapping, clause_stream, max_lbd,
                       clause = std::vector<int>()](
                          int lbd, absl::Span<const Literal> literals) mutable {
    if (lbd <= 0 || lbd > max_lbd ||
        !clause_stream->CanAccept(literals.size(), lbd)) {
      return;
    }
    clause.clear();
    for (const Literal& lit : literals) {
      const int var =
          mapping->GetProtoVariableFromBooleanVariable(lit.Variable());
      if (var == -1) return;
      clause.push_back(lit.IsPositive() ? var : NegatedRef(var));
    }
    clause_stream->Add(clause);
  };
  model->GetOrCreate<ClauseManager>()->SetAddClauseCallback(
      std::move(share_clause));
}

// Registers a callback to import new clauses stored in the
// shared_clausess_manager. These clauses are imported at level 0 of the search
// in the linear scan minimize function.
// it returns the id of the worker in the shared clause manager.
//
// TODO(user): Can we import them in the core worker ?
int RegisterClausesLevelZeroImport(int id,
                                   SharedClausesManager* shared_clauses_manager,
                                   Model* model) {
  CHECK(shared_clauses_manager != nullptr);
  CpModelMapping* const mapping = model->GetOrCreate<CpModelMapping>();
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* implications = model->GetOrCreate<BinaryImplicationGraph>();
  const bool share_glue_clauses =
      model->GetOrCreate<SatParameters>()->share_glue_clauses();
  const bool minimize_shared_clauses =
      model->GetOrCreate<SatParameters>()->minimize_shared_clauses();
  auto* clause_stream = share_glue_clauses
                            ? shared_clauses_manager->GetClauseStream(id)
                            : nullptr;
  auto* clause_manager = model->GetOrCreate<ClauseManager>();
  const auto& import_level_zero_clauses = [shared_clauses_manager, id, mapping,
                                           sat_solver, implications,
                                           clause_stream, clause_manager,
                                           minimize_shared_clauses]() {
    std::vector<std::pair<int, int>> new_binary_clauses;
    shared_clauses_manager->GetUnseenBinaryClauses(id, &new_binary_clauses);
    implications->EnableSharing(false);
    for (const auto& [ref1, ref2] : new_binary_clauses) {
      const Literal l1 = mapping->Literal(ref1);
      const Literal l2 = mapping->Literal(ref2);
      if (!sat_solver->AddBinaryClause(l1, l2)) {
        return false;
      }
    }
    implications->EnableSharing(true);
    if (clause_stream == nullptr) return true;

    int new_clauses = 0;
    std::array<Literal, UniqueClauseStream::kMaxClauseSize> local_clause;
    sat_solver->EnsureNewClauseIndexInitialized();
    // Temporarily disable clause sharing so we don't immediately re-export the
    // clauses we just imported.
    auto callback = clause_manager->TakeAddClauseCallback();
    for (const absl::Span<const int> shared_clause :
         shared_clauses_manager->GetUnseenClauses(id)) {
      // Check this clause was not already learned by this worker.
      // We can delete the fingerprint because we should not learn an identical
      // clause, and the global stream will not emit the same clause while any
      // worker hasn't consumed this clause (and thus also shouldn't relearn the
      // clause).
      if (clause_stream->Delete(shared_clause)) continue;
      for (int i = 0; i < shared_clause.size(); ++i) {
        local_clause[i] = mapping->Literal(shared_clause[i]);
      }
      if (!sat_solver->AddProblemClause(
              absl::MakeSpan(local_clause).subspan(0, shared_clause.size()))) {
        return false;
      }
      ++new_clauses;
    }
    clause_manager->SetAddClauseCallback(std::move(callback));
    clause_stream->RemoveWorstClauses();
    if (minimize_shared_clauses && new_clauses > 0) {
      // The new clauses may be subsumed, so try to minimize them to reduce
      // overhead of sharing.
      // We only share up to 1024 literals worth of new clauses per second, so
      // at most 1024 decisions to vivify all new clauses, so this should be
      // relatively cheap.
      return sat_solver->MinimizeByPropagation(
          /*dtime=*/0.5, /*minimize_new_clauses_only=*/true);
    }
    return true;
  };
  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_level_zero_clauses);
  return id;
}

void LoadBaseModel(const CpModelProto& model_proto, Model* model) {
  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  CHECK(shared_response_manager != nullptr);
  auto* sat_solver = model->GetOrCreate<SatSolver>();

  // Simple function for the few places where we do "return unsat()".
  const auto unsat = [shared_response_manager, sat_solver, model] {
    sat_solver->NotifyThatModelIsUnsat();
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        absl::StrCat(model->Name(), " [loading]"));
  };

  // We will add them all at once after model_proto is loaded.
  model->GetOrCreate<IntegerEncoder>()->DisableImplicationBetweenLiteral();

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  const bool view_all_booleans_as_integers =
      (parameters.linearization_level() >= 2) ||
      (parameters.search_branching() == SatParameters::FIXED_SEARCH &&
       model_proto.search_strategy().empty()) ||
      parameters.optimize_with_max_hs();
  LoadVariables(model_proto, view_all_booleans_as_integers, model);
  DetectOptionalVariables(model_proto, model);

  // TODO(user): The core algo and symmetries seems to be problematic in some
  // cases. See for instance: neos-691058.mps.gz. This is probably because as
  // we modify the model, our symmetry might be wrong? investigate.
  //
  // TODO(user): More generally, we cannot load the symmetry if we create
  // new Booleans and constraints that link them to some Booleans of the model.
  // Creating Booleans related to integer variable is fine since we only deal
  // with Boolean only symmetry here. It is why we disable this when we have
  // linear relaxation as some of them create new constraints.
  if (!parameters.optimize_with_core() && parameters.symmetry_level() > 1 &&
      !parameters.enumerate_all_solutions() &&
      parameters.linearization_level() == 0) {
    LoadBooleanSymmetries(model_proto, model);
  }

  ExtractEncoding(model_proto, model);
  PropagateEncodingFromEquivalenceRelations(model_proto, model);

  // Check the model is still feasible before continuing.
  if (sat_solver->ModelIsUnsat()) return unsat();

  // Fully encode variables as needed by the search strategy.
  AddFullEncodingFromSearchBranching(model_proto, model);
  if (sat_solver->ModelIsUnsat()) return unsat();

  // Reserve space for the precedence relations.
  model->GetOrCreate<PrecedenceRelations>()->Resize(
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value());

  // Load the constraints.
  int num_ignored_constraints = 0;
  absl::flat_hash_set<ConstraintProto::ConstraintCase> unsupported_types;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) {
      ++num_ignored_constraints;
      continue;
    }

    if (!LoadConstraint(ct, model)) {
      unsupported_types.insert(ct.constraint_case());
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
    if (sat_solver->ModelIsUnsat()) {
      VLOG(2) << "UNSAT during extraction (after adding '"
              << ConstraintCaseName(ct.constraint_case()) << "'). "
              << ProtobufDebugString(ct);
      return unsat();
    }
  }
  if (num_ignored_constraints > 0) {
    VLOG(3) << num_ignored_constraints << " constraints were skipped.";
  }
  if (!unsupported_types.empty()) {
    auto* logger = model->GetOrCreate<SolverLogger>();
    SOLVER_LOG(logger,
               "There is unsupported constraints types in this model: ");
    std::vector<absl::string_view> names;
    for (const ConstraintProto::ConstraintCase type : unsupported_types) {
      names.push_back(ConstraintCaseName(type));
    }
    std::sort(names.begin(), names.end());
    for (const absl::string_view name : names) {
      SOLVER_LOG(logger, " - ", name);
    }

    // TODO(user): This is wrong. We should support a MODEL_INVALID end of solve
    // in the SharedResponseManager.
    SOLVER_LOG(logger, "BUG: We will wrongly report INFEASIBLE now.");
    return unsat();
  }

  model->GetOrCreate<IntegerEncoder>()
      ->AddAllImplicationsBetweenAssociatedLiterals();
  if (!sat_solver->FinishPropagation()) return unsat();

  model->GetOrCreate<ProductDetector>()->ProcessImplicationGraph(
      model->GetOrCreate<BinaryImplicationGraph>());
  model->GetOrCreate<PrecedenceRelations>()->Build();
}

void LoadFeasibilityPump(const CpModelProto& model_proto, Model* model) {
  LoadBaseModel(model_proto, model);

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  if (parameters.linearization_level() == 0) return;

  // Add linear constraints to Feasibility Pump.
  const LinearRelaxation relaxation =
      ComputeLinearRelaxation(model_proto, model);
  if (model->GetOrCreate<SatSolver>()->ModelIsUnsat()) return;

  const int num_lp_constraints =
      static_cast<int>(relaxation.linear_constraints.size());
  if (num_lp_constraints == 0) return;
  auto* feasibility_pump = model->GetOrCreate<FeasibilityPump>();
  for (int i = 0; i < num_lp_constraints; i++) {
    feasibility_pump->AddLinearConstraint(relaxation.linear_constraints[i]);
  }

  if (model_proto.has_objective()) {
    for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
      const IntegerVariable var =
          mapping->Integer(model_proto.objective().vars(i));
      const int64_t coeff = model_proto.objective().coeffs(i);
      feasibility_pump->SetObjectiveCoefficient(var, IntegerValue(coeff));
    }
  }
}

// Loads a CpModelProto inside the given model.
// This should only be called once on a given 'Model' class.
void LoadCpModel(const CpModelProto& model_proto, Model* model) {
  LoadBaseModel(model_proto, model);

  // We want to load the debug solution before the initial propag.
  // But at this point the objective is not loaded yet, so we will not have
  // a value for the objective integer variable, so we do it again later.
  InitializeDebugSolution(model_proto, model);

  // Simple function for the few places where we do "return unsat()".
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  const auto unsat = [shared_response_manager, sat_solver, model] {
    sat_solver->NotifyThatModelIsUnsat();
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        absl::StrCat(model->Name(), " [loading]"));
  };

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());

  // Auto detect "at least one of" constraints in the PrecedencesPropagator.
  // Note that we do that before we finish loading the problem (objective and
  // LP relaxation), because propagation will be faster at this point and it
  // should be enough for the purpose of this auto-detection.
  if (parameters.auto_detect_greater_than_at_least_one_of()) {
    model->GetOrCreate<GreaterThanAtLeastOneOfDetector>()
        ->AddGreaterThanAtLeastOneOfConstraints(model);
    if (!sat_solver->FinishPropagation()) return unsat();
  }

  // Note that this is already done in the presolve, but it is important to redo
  // it here to collect literal => integer >= bound constraints that are used in
  // many places. Without it, we don't detect them if they depends on long chain
  // of implications.
  //
  // TODO(user): We don't have a good deterministic time on all constraints,
  // so this might take more time than wanted.
  if (parameters.cp_model_probing_level() > 1) {
    Prober* prober = model->GetOrCreate<Prober>();
    prober->ProbeBooleanVariables(/*deterministic_time_limit=*/1.0);
    if (!model->GetOrCreate<BinaryImplicationGraph>()
             ->ComputeTransitiveReduction()) {
      return unsat();
    }
  }
  if (sat_solver->ModelIsUnsat()) return unsat();

  // Note that it is important to do that after the probing.
  ExtractElementEncoding(model_proto, model);

  // Compute decomposed energies on demands helper.
  IntervalsRepository* repository = model->Mutable<IntervalsRepository>();
  if (repository != nullptr) {
    repository->InitAllDecomposedEnergies();
  }

  // We need to know beforehand if the objective var can just be >= terms or
  // needs to be == terms.
  bool objective_need_to_be_tight = false;
  if (model_proto.has_objective() &&
      !model_proto.objective().domain().empty()) {
    int64_t min_value = 0;
    int64_t max_value = 0;
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    const CpObjectiveProto& obj = model_proto.objective();
    for (int i = 0; i < obj.vars_size(); ++i) {
      const int64_t coeff = obj.coeffs(i);
      const IntegerVariable var = mapping->Integer(obj.vars(i));
      if (coeff > 0) {
        min_value += coeff * integer_trail->LowerBound(var).value();
        max_value += coeff * integer_trail->UpperBound(var).value();
      } else {
        min_value += coeff * integer_trail->UpperBound(var).value();
        max_value += coeff * integer_trail->LowerBound(var).value();
      }
    }
    const Domain user_domain = ReadDomainFromProto(model_proto.objective());
    const Domain automatic_domain = Domain(min_value, max_value);
    objective_need_to_be_tight = !automatic_domain.IsIncludedIn(user_domain);
  }

  // Create an objective variable and its associated linear constraint if
  // needed.
  IntegerVariable objective_var = kNoIntegerVariable;
  if (parameters.linearization_level() > 0) {
    // Linearize some part of the problem and register LP constraint(s).
    objective_var =
        AddLPConstraints(objective_need_to_be_tight, model_proto, model);
    if (sat_solver->ModelIsUnsat()) return unsat();
  } else if (model_proto.has_objective()) {
    const CpObjectiveProto& obj = model_proto.objective();
    std::vector<std::pair<IntegerVariable, int64_t>> terms;
    terms.reserve(obj.vars_size());
    for (int i = 0; i < obj.vars_size(); ++i) {
      terms.push_back(
          std::make_pair(mapping->Integer(obj.vars(i)), obj.coeffs(i)));
    }
    if (parameters.optimize_with_core()) {
      if (objective_need_to_be_tight) {
        // We do not care about the <= obj for core, we only need the other side
        // to enforce a restriction of the objective lower bound.
        //
        // TODO(user): This might still create intermediate variables to
        // decompose the objective for no reason. Just deal directly with the
        // objective domain in the core algo by forbidding bad assumptions?
        // Alternatively, just ignore the core solution if it is "too" good and
        // rely on other solvers?
        objective_var =
            GetOrCreateVariableLinkedToSumOf(terms, true, false, model);
      } else {
        objective_var = GetOrCreateVariableWithTightBound(terms, model);
      }
    } else {
      objective_var = GetOrCreateVariableLinkedToSumOf(
          terms, objective_need_to_be_tight, true, model);
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

    // Register an objective special propagator.
    model->TakeOwnership(
        new LevelZeroEquality(objective_var, objective_definition->vars,
                              objective_definition->coeffs, model));
  }

  // Intersect the objective domain with the given one if any.
  if (!model_proto.objective().domain().empty()) {
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    const Domain user_domain = ReadDomainFromProto(model_proto.objective());
    const Domain automatic_domain =
        integer_trail->InitialVariableDomain(objective_var);
    VLOG(3) << "Objective offset:" << model_proto.objective().offset()
            << " scaling_factor:" << model_proto.objective().scaling_factor();
    VLOG(3) << "Automatic internal objective domain: " << automatic_domain;
    VLOG(3) << "User specified internal objective domain: " << user_domain;
    CHECK_NE(objective_var, kNoIntegerVariable);
    if (!integer_trail->UpdateInitialDomain(objective_var, user_domain)) {
      VLOG(2) << "UNSAT due to the objective domain.";
      return unsat();
    }
  }

  // Note that we do one last propagation at level zero once all the
  // constraints were added.
  SOLVER_LOG(model->GetOrCreate<SolverLogger>(),
             "Initial num_bool: ", sat_solver->NumVariables());
  if (!sat_solver->FinishPropagation()) return unsat();

  if (model_proto.has_objective()) {
    // Report the initial objective variable bounds.
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    shared_response_manager->UpdateInnerObjectiveBounds(
        absl::StrCat(model->Name(), " (initial_propagation)"),
        integer_trail->LowerBound(objective_var),
        integer_trail->UpperBound(objective_var));

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

  // Initialize the search strategies.
  auto* search_heuristics = model->GetOrCreate<SearchHeuristics>();
  search_heuristics->user_search =
      ConstructUserSearchStrategy(model_proto, model);
  search_heuristics->heuristic_search =
      ConstructHeuristicSearchStrategy(model_proto, model);
  search_heuristics->integer_completion_search =
      ConstructIntegerCompletionSearchStrategy(mapping->GetVariableMapping(),
                                               objective_var, model);
  search_heuristics->fixed_search = ConstructFixedSearchStrategy(
      search_heuristics->user_search, search_heuristics->heuristic_search,
      search_heuristics->integer_completion_search);
  if (VLOG_IS_ON(3)) {
    search_heuristics->fixed_search =
        InstrumentSearchStrategy(model_proto, mapping->GetVariableMapping(),
                                 search_heuristics->fixed_search, model);
  }
  search_heuristics->hint_search =
      ConstructHintSearchStrategy(model_proto, mapping, model);

  // Create the CoreBasedOptimizer class if needed.
  if (parameters.optimize_with_core()) {
    // TODO(user): Remove code duplication with the solution_observer in
    // SolveLoadedCpModel().
    const auto solution_observer = [&model_proto, model,
                                    shared_response_manager,
                                    best_obj_ub = kMaxIntegerValue]() mutable {
      const std::vector<int64_t> solution =
          GetSolutionValues(model_proto, *model);
      const IntegerValue obj_ub =
          ComputeInnerObjective(model_proto.objective(), solution);
      if (obj_ub < best_obj_ub) {
        best_obj_ub = obj_ub;
        shared_response_manager->NewSolution(solution, model->Name(), model);
      }
    };

    const auto& objective = *model->GetOrCreate<ObjectiveDefinition>();
    if (parameters.optimize_with_max_hs()) {
      HittingSetOptimizer* max_hs = new HittingSetOptimizer(
          model_proto, objective, solution_observer, model);
      model->Register<HittingSetOptimizer>(max_hs);
      model->TakeOwnership(max_hs);
    } else {
      CoreBasedOptimizer* core =
          new CoreBasedOptimizer(objective_var, objective.vars,
                                 objective.coeffs, solution_observer, model);
      model->Register<CoreBasedOptimizer>(core);
      model->TakeOwnership(core);
    }
  }

  InitializeDebugSolution(model_proto, model);
}

// Solves an already loaded cp_model_proto.
// The final CpSolverResponse must be read from the shared_response_manager.
//
// TODO(user): This should be transformed so that it can be called many times
// and resume from the last search state as if it wasn't interrupted. That would
// allow use to easily interleave different heuristics in the same thread.
void SolveLoadedCpModel(const CpModelProto& model_proto, Model* model) {
  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  if (shared_response_manager->ProblemIsSolved()) return;

  const SatParameters& parameters = *model->GetOrCreate<SatParameters>();
  if (parameters.stop_after_root_propagation()) return;

  auto solution_observer = [&model_proto, model, shared_response_manager,
                            best_obj_ub = kMaxIntegerValue]() mutable {
    const std::vector<int64_t> solution =
        GetSolutionValues(model_proto, *model);
    if (model_proto.has_objective()) {
      const IntegerValue obj_ub =
          ComputeInnerObjective(model_proto.objective(), solution);
      if (obj_ub < best_obj_ub) {
        best_obj_ub = obj_ub;
        shared_response_manager->NewSolution(solution, model->Name(), model);
      }
    } else {
      shared_response_manager->NewSolution(solution, model->Name(), model);
    }
  };

  // Make sure we are not at a positive level.
  if (!model->GetOrCreate<SatSolver>()->ResetToLevelZero()) {
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        model->Name());
    return;
  }

  // Reconfigure search heuristic if it was changed.
  ConfigureSearchHeuristics(model);

  const auto& mapping = *model->GetOrCreate<CpModelMapping>();
  SatSolver::Status status;

  if (parameters.use_probing_search()) {
    ContinuousProber prober(model_proto, model);
    while (true) {
      status = prober.Probe();
      if (status == SatSolver::INFEASIBLE) {
        shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
            model->Name());
        break;
      }
      if (status == SatSolver::FEASIBLE) {
        solution_observer();
      } else {
        break;
      }
    }
  } else if (!model_proto.has_objective()) {
    while (true) {
      if (parameters.use_shared_tree_search()) {
        auto* subtree_worker = model->GetOrCreate<SharedTreeWorker>();
        status = subtree_worker->Search(solution_observer);
      } else {
        status = ResetAndSolveIntegerProblem(
            mapping.Literals(model_proto.assumptions()), model);
      }
      if (status != SatSolver::Status::FEASIBLE) break;
      solution_observer();
      if (!parameters.enumerate_all_solutions()) break;
      model->Add(ExcludeCurrentSolutionAndBacktrack());
    }
    if (status == SatSolver::INFEASIBLE) {
      shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
          model->Name());
    }
    if (status == SatSolver::ASSUMPTIONS_UNSAT) {
      shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
          model->Name());

      // Extract a good subset of assumptions and add it to the response.
      auto* time_limit = model->GetOrCreate<TimeLimit>();
      auto* sat_solver = model->GetOrCreate<SatSolver>();
      std::vector<Literal> core = sat_solver->GetLastIncompatibleDecisions();
      MinimizeCoreWithPropagation(time_limit, sat_solver, &core);
      std::vector<int> core_in_proto_format;
      for (const Literal l : core) {
        core_in_proto_format.push_back(
            mapping.GetProtoVariableFromBooleanVariable(l.Variable()));
        if (!l.IsPositive()) {
          core_in_proto_format.back() = NegatedRef(core_in_proto_format.back());
        }
      }
      shared_response_manager->AddUnsatCore(core_in_proto_format);
    }
  } else {
    // Optimization problem.
    const auto& objective = *model->GetOrCreate<ObjectiveDefinition>();
    const IntegerVariable objective_var = objective.objective_var;
    CHECK_NE(objective_var, kNoIntegerVariable);

    if (parameters.optimize_with_lb_tree_search()) {
      auto* search = model->GetOrCreate<LbTreeSearch>();
      status = search->Search(solution_observer);
    } else if (parameters.optimize_with_core()) {
      // TODO(user): This doesn't work with splitting in chunk for now. It
      // shouldn't be too hard to fix.
      if (parameters.optimize_with_max_hs()) {
        status = model->Mutable<HittingSetOptimizer>()->Optimize();
      } else {
        status = model->Mutable<CoreBasedOptimizer>()->Optimize();
      }
    } else if (parameters.use_shared_tree_search()) {
      auto* subtree_worker = model->GetOrCreate<SharedTreeWorker>();
      status = subtree_worker->Search(solution_observer);
    } else {
      // TODO(user): This parameter breaks the splitting in chunk of a Solve().
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
          model->Name());
    }
  }
}

// Try to find a solution by following the hint and using a low conflict limit.
// The CpModelProto must already be loaded in the Model.
void QuickSolveWithHint(const CpModelProto& model_proto, Model* model) {
  if (!model_proto.has_solution_hint()) return;

  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  if (shared_response_manager->ProblemIsSolved()) return;

  // Temporarily change the parameters.
  auto* parameters = model->GetOrCreate<SatParameters>();

  // If the model was loaded with "optimize_with_core" then the objective
  // variable is not linked to its linear expression. Because of that, we can
  // return a solution that does not satisfy the objective domain.
  //
  // TODO(user): This is fixable, but then do we need the hint when optimizing
  // with core?
  if (parameters->optimize_with_core()) return;

  const SatParameters saved_params = *parameters;
  parameters->set_max_number_of_conflicts(parameters->hint_conflict_limit());
  parameters->set_search_branching(SatParameters::HINT_SEARCH);
  parameters->set_optimize_with_core(false);
  parameters->set_use_sat_inprocessing(false);
  auto cleanup = ::absl::MakeCleanup(
      [parameters, saved_params]() { *parameters = saved_params; });

  // Solve decision problem.
  ConfigureSearchHeuristics(model);
  const auto& mapping = *model->GetOrCreate<CpModelMapping>();
  const SatSolver::Status status = ResetAndSolveIntegerProblem(
      mapping.Literals(model_proto.assumptions()), model);

  const std::string& solution_info = model->Name();
  if (status == SatSolver::Status::FEASIBLE) {
    const std::vector<int64_t> solution =
        GetSolutionValues(model_proto, *model);
    shared_response_manager->NewSolution(
        solution, absl::StrCat(solution_info, " [hint]"), model);

    if (!model_proto.has_objective()) {
      if (parameters->enumerate_all_solutions()) {
        model->Add(ExcludeCurrentSolutionAndBacktrack());
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
      }
    }
    return;
  }

  // This code is here to debug bad presolve during LNS that corrupt the hint.
  // Note that sometime the deterministic limit is hit before the hint can be
  // completed, so we don't report that has an error.
  //
  // Tricky: We can only test that if we don't already have a feasible solution
  // like we do if the hint is complete.
  if (parameters->debug_crash_on_bad_hint() &&
      shared_response_manager->SolutionsRepository().NumSolutions() == 0 &&
      !model->GetOrCreate<TimeLimit>()->LimitReached() &&
      status != SatSolver::Status::FEASIBLE) {
    LOG(FATAL) << "QuickSolveWithHint() didn't find a feasible solution."
               << " The model name is '" << model_proto.name() << "'."
               << " Status: " << status << ".";
  }

  if (status == SatSolver::INFEASIBLE) {
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        absl::StrCat(solution_info, " [hint]"));
    return;
  }
}

// Solve a model with a different objective consisting of minimizing the L1
// distance with the provided hint. Note that this method creates an in-memory
// copy of the model and loads a local Model object from the copied model.
void MinimizeL1DistanceWithHint(const CpModelProto& model_proto, Model* model) {
  Model local_model;

  // Forward some shared class.
  local_model.Register<ModelSharedTimeLimit>(
      model->GetOrCreate<ModelSharedTimeLimit>());
  local_model.Register<WallTimer>(model->GetOrCreate<WallTimer>());

  if (!model_proto.has_solution_hint()) return;

  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  if (shared_response_manager->ProblemIsSolved()) return;

  auto* parameters = local_model.GetOrCreate<SatParameters>();
  // TODO(user): As of now the repair hint doesn't support when
  // enumerate_all_solutions is set since the solution is created on a different
  // model.
  if (parameters->enumerate_all_solutions()) return;

  // Change the parameters.
  const SatParameters saved_params = *model->GetOrCreate<SatParameters>();
  *parameters = saved_params;
  parameters->set_max_number_of_conflicts(parameters->hint_conflict_limit());
  parameters->set_optimize_with_core(false);

  // Update the model to introduce penalties to go away from hinted values.
  CpModelProto updated_model_proto = model_proto;
  updated_model_proto.clear_objective();

  // TODO(user): For boolean variables we can avoid creating new variables.
  for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
    const int var = model_proto.solution_hint().vars(i);
    const int64_t value = model_proto.solution_hint().values(i);

    // Add a new var to represent the difference between var and value.
    const int new_var_index = updated_model_proto.variables_size();
    IntegerVariableProto* var_proto = updated_model_proto.add_variables();
    const int64_t min_domain = model_proto.variables(var).domain(0) - value;
    const int64_t max_domain =
        model_proto.variables(var).domain(
            model_proto.variables(var).domain_size() - 1) -
        value;
    var_proto->add_domain(min_domain);
    var_proto->add_domain(max_domain);

    // new_var = var - value.
    ConstraintProto* const linear_constraint_proto =
        updated_model_proto.add_constraints();
    LinearConstraintProto* linear = linear_constraint_proto->mutable_linear();
    linear->add_vars(new_var_index);
    linear->add_coeffs(1);
    linear->add_vars(var);
    linear->add_coeffs(-1);
    linear->add_domain(-value);
    linear->add_domain(-value);

    // abs_var = abs(new_var).
    const int abs_var_index = updated_model_proto.variables_size();
    IntegerVariableProto* abs_var_proto = updated_model_proto.add_variables();
    const int64_t abs_min_domain = 0;
    const int64_t abs_max_domain =
        std::max(std::abs(min_domain), std::abs(max_domain));
    abs_var_proto->add_domain(abs_min_domain);
    abs_var_proto->add_domain(abs_max_domain);
    auto* abs_ct = updated_model_proto.add_constraints()->mutable_lin_max();
    abs_ct->mutable_target()->add_vars(abs_var_index);
    abs_ct->mutable_target()->add_coeffs(1);
    LinearExpressionProto* left = abs_ct->add_exprs();
    left->add_vars(new_var_index);
    left->add_coeffs(1);
    LinearExpressionProto* right = abs_ct->add_exprs();
    right->add_vars(new_var_index);
    right->add_coeffs(-1);

    updated_model_proto.mutable_objective()->add_vars(abs_var_index);
    updated_model_proto.mutable_objective()->add_coeffs(1);
  }

  auto* local_response_manager =
      local_model.GetOrCreate<SharedResponseManager>();
  local_response_manager->InitializeObjective(updated_model_proto);

  // Solve optimization problem.
  LoadCpModel(updated_model_proto, &local_model);

  ConfigureSearchHeuristics(&local_model);
  const auto& mapping = *local_model.GetOrCreate<CpModelMapping>();
  const SatSolver::Status status = ResetAndSolveIntegerProblem(
      mapping.Literals(updated_model_proto.assumptions()), &local_model);

  const std::string& solution_info = model->Name();
  if (status == SatSolver::Status::FEASIBLE) {
    const std::vector<int64_t> solution =
        GetSolutionValues(model_proto, local_model);
    if (DEBUG_MODE) {
      const std::vector<int64_t> updated_solution =
          GetSolutionValues(updated_model_proto, local_model);
      LOG(INFO) << "Found solution with repaired hint penalty = "
                << ComputeInnerObjective(updated_model_proto.objective(),
                                         updated_solution);
    }
    shared_response_manager->NewSolution(
        solution, absl::StrCat(solution_info, " [repaired]"), &local_model);
  }
}

// TODO(user): If this ever shows up in the profile, we could avoid copying
// the mapping_proto if we are careful about how we modify the variable domain
// before postsolving it. Note that 'num_variables_in_original_model' refers to
// the model before presolve.
void PostsolveResponseWithFullSolver(int num_variables_in_original_model,
                                     CpModelProto mapping_proto,
                                     const std::vector<int>& postsolve_mapping,
                                     std::vector<int64_t>* solution) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Fix the correct variable in the mapping_proto.
  for (int i = 0; i < solution->size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    var_proto->clear_domain();
    var_proto->add_domain((*solution)[i]);
    var_proto->add_domain((*solution)[i]);
  }

  // Postosolve parameters.
  // TODO(user): this problem is usually trivial, but we may still want to
  // impose a time limit or copy some of the parameters passed by the user.
  Model postsolve_model;
  postsolve_model.Register<WallTimer>(&wall_timer);
  {
    SatParameters& params = *postsolve_model.GetOrCreate<SatParameters>();
    params.set_linearization_level(0);
    params.set_cp_model_probing_level(0);
  }

  auto* response_manager = postsolve_model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(mapping_proto);

  LoadCpModel(mapping_proto, &postsolve_model);
  SolveLoadedCpModel(mapping_proto, &postsolve_model);
  const CpSolverResponse postsolve_response = response_manager->GetResponse();
  CHECK(postsolve_response.status() == CpSolverStatus::FEASIBLE ||
        postsolve_response.status() == CpSolverStatus::OPTIMAL)
      << postsolve_response.status();

  // We only copy the solution from the postsolve_response to the response.
  CHECK_LE(num_variables_in_original_model,
           postsolve_response.solution().size());
  solution->assign(
      postsolve_response.solution().begin(),
      postsolve_response.solution().begin() + num_variables_in_original_model);
}

void PostsolveResponseWrapper(const SatParameters& params,
                              int num_variable_in_original_model,
                              const CpModelProto& mapping_proto,
                              const std::vector<int>& postsolve_mapping,
                              std::vector<int64_t>* solution) {
  if (params.debug_postsolve_with_full_solver()) {
    PostsolveResponseWithFullSolver(num_variable_in_original_model,
                                    mapping_proto, postsolve_mapping, solution);
  } else {
    PostsolveResponse(num_variable_in_original_model, mapping_proto,
                      postsolve_mapping, solution);
  }
}

void AdaptGlobalParameters(const CpModelProto& model_proto, Model* model) {
  auto* params = model->GetOrCreate<SatParameters>();
  auto* logger = model->GetOrCreate<SolverLogger>();

  // Update params.num_workers() if the old field was used.
  if (params->num_workers() == 0) {
    params->set_num_workers(params->num_search_workers());
  }

  if (params->enumerate_all_solutions()) {
    if (params->num_workers() >= 1) {
      SOLVER_LOG(logger,
                 "Forcing sequential search as enumerating all solutions is "
                 "not supported in multi-thread.");
    }
    params->set_num_workers(1);
  }

  if (!model_proto.assumptions().empty()) {
    if (params->num_workers() >= 1) {
      SOLVER_LOG(logger,
                 "Forcing sequential search as assumptions are not supported "
                 "in multi-thread.");
    }
    params->set_num_workers(1);
  }

  if (params->num_workers() == 0) {
    // Initialize the number of workers if set to 0.
#if !defined(__PORTABLE_PLATFORM__)
    // Sometimes, hardware_concurrency will return 0. So always default to 1.
    const int num_cores = std::max<int>(std::thread::hardware_concurrency(), 1);
#else
    const int num_cores = 1;
#endif
    SOLVER_LOG(logger, "Setting number of workers to ", num_cores);
    params->set_num_workers(num_cores);
  }

  // We currently only use the feasibility pump or rins/rens if it is enabled
  // and some other parameters are not on.
  //
  // TODO(user): for now this is not deterministic so we disable it on
  // interleave search. Fix.
  if (params->interleave_search() || params->num_workers() == 1 ||
      !params->use_lns()) {
    params->set_use_rins_lns(false);
    params->set_use_feasibility_pump(false);
  }

  // We disable this if the global param asked for no LP.
  if (params->linearization_level() == 0) {
    params->set_use_feasibility_pump(false);
  }

  // Disable shared bounds if we are in single thread and we are not
  // tightening the domains.
  if (!params->fill_tightened_domains_in_response() &&
      params->num_workers() == 1) {
    params->set_share_level_zero_bounds(false);
  }
}

SharedClasses::SharedClasses(const CpModelProto* proto, Model* global_model)
    : model_proto(*proto),
      wall_timer(global_model->GetOrCreate<WallTimer>()),
      time_limit(global_model->GetOrCreate<ModelSharedTimeLimit>()),
      logger(global_model->GetOrCreate<SolverLogger>()),
      stats(global_model->GetOrCreate<SharedStatistics>()),
      response(global_model->GetOrCreate<SharedResponseManager>()),
      shared_tree_manager(global_model->GetOrCreate<SharedTreeManager>()) {
  const SatParameters& params = *global_model->GetOrCreate<SatParameters>();

  if (params.share_level_zero_bounds()) {
    bounds = std::make_unique<SharedBoundsManager>(*proto);
    bounds->set_dump_prefix(absl::GetFlag(FLAGS_cp_model_dump_prefix));
    bounds->LoadDebugSolution(response->DebugSolution());
  }

  // Create extra shared classes if needed. Note that while these parameters
  // are true by default, we disable them if we don't have enough workers for
  // them in AdaptGlobalParameters().
  //
  // Registering them to the global model should not really be necessary,
  // except if one wants to expect them from outside SolveCpModel().
  if (params.use_rins_lns() || params.use_feasibility_pump()) {
    lp_solutions = std::make_unique<SharedLPSolutionRepository>(
        /*num_solutions_to_keep=*/10);
    global_model->Register<SharedLPSolutionRepository>(lp_solutions.get());

    incomplete_solutions = std::make_unique<SharedIncompleteSolutionManager>();
    global_model->Register<SharedIncompleteSolutionManager>(
        incomplete_solutions.get());
  }

  // Set up synchronization mode in parallel.
  const bool always_synchronize =
      !params.interleave_search() || params.num_workers() <= 1;
  response->SetSynchronizationMode(always_synchronize);
  if (params.share_binary_clauses() && params.num_workers() > 1) {
    clauses = std::make_unique<SharedClausesManager>(always_synchronize,
                                                     absl::Seconds(1));
  }
}

bool SharedClasses::SearchIsDone() {
  if (response->ProblemIsSolved()) {
    // This is for cases where the time limit is checked more often.
    time_limit->Stop();
    return true;
  }
  if (time_limit->LimitReached()) return true;
  return false;
}

}  // namespace sat
}  // namespace operations_research
