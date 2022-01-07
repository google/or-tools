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

#include "ortools/sat/cp_model_symmetries.h"

#include <cstdint>
#include <limits>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include "google/protobuf/repeated_field.h"
#include "ortools/algorithms/find_graph_symmetries.h"
#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/symmetry_util.h"

namespace operations_research {
namespace sat {

namespace {
struct VectorHash {
  std::size_t operator()(const std::vector<int64_t>& values) const {
    size_t hash = 0;
    for (const int64_t value : values) {
      hash = util_hash::Hash(value, hash);
    }
    return hash;
  }
};

// A simple class to generate equivalence class number for
// GenerateGraphForSymmetryDetection().
class IdGenerator {
 public:
  IdGenerator() {}

  // If the color was never seen before, then generate a new id, otherwise
  // return the previously generated id.
  int GetId(const std::vector<int64_t>& color) {
    return gtl::LookupOrInsert(&id_map_, color, id_map_.size());
  }

  int NextFreeId() const { return id_map_.size(); }

 private:
  absl::flat_hash_map<std::vector<int64_t>, int, VectorHash> id_map_;
};

// Appends values in `repeated_field` to `vector`.
//
// We use a template as proto int64_t != C++ int64_t in open source.
template <typename FieldInt64Type>
void Append(
    const google::protobuf::RepeatedField<FieldInt64Type>& repeated_field,
    std::vector<int64_t>* vector) {
  CHECK(vector != nullptr);
  for (const FieldInt64Type value : repeated_field) {
    vector->push_back(value);
  }
}

// Returns a graph whose automorphisms can be mapped back to the symmetries of
// the model described in the given CpModelProto.
//
// Any permutation of the graph that respects the initial_equivalence_classes
// output can be mapped to a symmetry of the given problem simply by taking its
// restriction on the first num_variables nodes and interpreting its index as a
// variable index. In a sense, a node with a low enough index #i is in
// one-to-one correspondence with the variable #i (using the index
// representation of variables).
//
// The format of the initial_equivalence_classes is the same as the one
// described in GraphSymmetryFinder::FindSymmetries(). The classes must be dense
// in [0, num_classes) and any symmetry will only map nodes with the same class
// between each other.
template <typename Graph>
std::unique_ptr<Graph> GenerateGraphForSymmetryDetection(
    const CpModelProto& problem, std::vector<int>* initial_equivalence_classes,
    SolverLogger* logger) {
  CHECK(initial_equivalence_classes != nullptr);

  const int num_variables = problem.variables_size();
  auto graph = absl::make_unique<Graph>();

  // Each node will be created with a given color. Two nodes of different color
  // can never be send one into another by a symmetry. The first element of
  // the color vector will always be the NodeType.
  //
  // TODO(user): Using a full int64_t for storing 3 values is not great. We
  // can optimize this at the price of a bit more code.
  enum NodeType {
    VARIABLE_NODE,
    VAR_COEFFICIENT_NODE,
    CONSTRAINT_NODE,
  };
  IdGenerator color_id_generator;
  initial_equivalence_classes->clear();
  auto new_node = [&initial_equivalence_classes, &graph,
                   &color_id_generator](const std::vector<int64_t>& color) {
    // Since we add nodes one by one, initial_equivalence_classes->size() gives
    // the number of nodes at any point, which we use as the next node index.
    const int node = initial_equivalence_classes->size();
    initial_equivalence_classes->push_back(color_id_generator.GetId(color));

    // In some corner cases, we create a node but never uses it. We still
    // want it to be there.
    graph->AddNode(node);
    return node;
  };

  // For two variables to be in the same equivalence class, they need to have
  // the same objective coefficient, and the same possible bounds.
  //
  // TODO(user): We could ignore the objective coefficients, and just make sure
  // that when we break symmetry amongst variables, we choose the possibility
  // with the smallest cost?
  std::vector<int64_t> objective_by_var(num_variables, 0);
  for (int i = 0; i < problem.objective().vars_size(); ++i) {
    const int ref = problem.objective().vars(i);
    const int var = PositiveRef(ref);
    const int64_t coeff = problem.objective().coeffs(i);
    objective_by_var[var] = RefIsPositive(ref) ? coeff : -coeff;
  }

  // Create one node for each variable. Note that the code rely on the fact that
  // the index of a VARIABLE_NODE type is the same as the variable index.
  std::vector<int64_t> tmp_color;
  for (int v = 0; v < num_variables; ++v) {
    tmp_color = {VARIABLE_NODE, objective_by_var[v]};
    Append(problem.variables(v).domain(), &tmp_color);
    CHECK_EQ(v, new_node(tmp_color));
  }

  // We will lazily create "coefficient nodes" that correspond to a variable
  // with a given coefficient.
  absl::flat_hash_map<std::pair<int64_t, int64_t>, int> coefficient_nodes;
  auto get_coefficient_node = [&new_node, &graph, &coefficient_nodes,
                               &tmp_color](int var, int64_t coeff) {
    const int var_node = var;
    DCHECK(RefIsPositive(var));

    // For a coefficient of one, which are the most common, we can optimize the
    // size of the graph by omitting the coefficient node altogether and using
    // directly the var_node in this case.
    if (coeff == 1) return var_node;

    const auto insert =
        coefficient_nodes.insert({std::make_pair(var, coeff), 0});
    if (!insert.second) return insert.first->second;

    tmp_color = {VAR_COEFFICIENT_NODE, coeff};
    const int secondary_node = new_node(tmp_color);
    graph->AddArc(var_node, secondary_node);
    insert.first->second = secondary_node;
    return secondary_node;
  };

  // For a literal we use the same as a coefficient 1 or -1. We can do that
  // because literal and (var, coefficient) never appear together in the same
  // constraint.
  auto get_literal_node = [&get_coefficient_node](int ref) {
    return get_coefficient_node(PositiveRef(ref), RefIsPositive(ref) ? 1 : -1);
  };

  // Because the implications can be numerous, we encode them without
  // constraints node by using an arc from the lhs to the rhs. Note that we also
  // always add the other direction. We use a set to remove duplicates both for
  // efficiency and to not artificially break symmetries by using multi-arcs.
  //
  // Tricky: We cannot use the base variable node here to avoid situation like
  // both a variable a and b having the same children (not(a), not(b)) in the
  // graph. Because if that happen, we can permute a and b without permuting
  // their associated not(a) and not(b) node! To be sure this cannot happen, a
  // variable node can not have as children a VAR_COEFFICIENT_NODE from another
  // node. This makes sure that any permutation that touch a variable, must
  // permute its coefficient nodes accordingly.
  absl::flat_hash_set<std::pair<int, int>> implications;
  auto get_implication_node = [&new_node, &graph, &coefficient_nodes,
                               &tmp_color](int ref) {
    const int var = PositiveRef(ref);
    const int64_t coeff = RefIsPositive(ref) ? 1 : -1;
    const auto insert =
        coefficient_nodes.insert({std::make_pair(var, coeff), 0});
    if (!insert.second) return insert.first->second;
    tmp_color = {VAR_COEFFICIENT_NODE, coeff};
    const int secondary_node = new_node(tmp_color);
    graph->AddArc(var, secondary_node);
    insert.first->second = secondary_node;
    return secondary_node;
  };
  auto add_implication = [&get_implication_node, &graph, &implications](
                             int ref_a, int ref_b) {
    const auto insert = implications.insert({ref_a, ref_b});
    if (!insert.second) return;
    graph->AddArc(get_implication_node(ref_a), get_implication_node(ref_b));

    // Always add the other side.
    implications.insert({NegatedRef(ref_b), NegatedRef(ref_a)});
    graph->AddArc(get_implication_node(NegatedRef(ref_b)),
                  get_implication_node(NegatedRef(ref_a)));
  };

  // Add constraints to the graph.
  for (const ConstraintProto& constraint : problem.constraints()) {
    const int constraint_node = initial_equivalence_classes->size();
    std::vector<int64_t> color = {CONSTRAINT_NODE,
                                  constraint.constraint_case()};

    switch (constraint.constraint_case()) {
      case ConstraintProto::CONSTRAINT_NOT_SET:
        // TODO(user): We continue for the corner case of a constraint not set
        // with enforcement literal. We should probably clear this constraint
        // before reaching here.
        continue;
      case ConstraintProto::kLinear: {
        // TODO(user): We can use the same trick as for the implications to
        // encode relations of the form coeff * var_a <= coeff * var_b without
        // creating a constraint node by directly adding an arc between the two
        // var coefficient nodes.
        Append(constraint.linear().domain(), &color);
        CHECK_EQ(constraint_node, new_node(color));
        for (int i = 0; i < constraint.linear().vars_size(); ++i) {
          const int ref = constraint.linear().vars(i);
          const int variable_node = PositiveRef(ref);
          const int64_t coeff = RefIsPositive(ref)
                                    ? constraint.linear().coeffs(i)
                                    : -constraint.linear().coeffs(i);
          graph->AddArc(get_coefficient_node(variable_node, coeff),
                        constraint_node);
        }
        break;
      }
      case ConstraintProto::kBoolOr: {
        CHECK_EQ(constraint_node, new_node(color));
        for (const int ref : constraint.bool_or().literals()) {
          graph->AddArc(get_literal_node(ref), constraint_node);
        }
        break;
      }
      case ConstraintProto::kAtMostOne: {
        if (constraint.at_most_one().literals().size() == 2) {
          // Treat it as an implication to avoid creating a node.
          add_implication(constraint.at_most_one().literals(0),
                          NegatedRef(constraint.at_most_one().literals(1)));
          break;
        }

        CHECK_EQ(constraint_node, new_node(color));
        for (const int ref : constraint.at_most_one().literals()) {
          graph->AddArc(get_literal_node(ref), constraint_node);
        }
        break;
      }
      case ConstraintProto::kExactlyOne: {
        CHECK_EQ(constraint_node, new_node(color));
        for (const int ref : constraint.exactly_one().literals()) {
          graph->AddArc(get_literal_node(ref), constraint_node);
        }
        break;
      }
      case ConstraintProto::kBoolXor: {
        CHECK_EQ(constraint_node, new_node(color));
        for (const int ref : constraint.bool_xor().literals()) {
          graph->AddArc(get_literal_node(ref), constraint_node);
        }
        break;
      }
      case ConstraintProto::kBoolAnd: {
        // The other cases should be presolved before this is called.
        // TODO(user): not 100% true, this happen on rmatr200-p5, Fix.
        if (constraint.enforcement_literal_size() != 1) {
          SOLVER_LOG(
              logger,
              "[Symmetry] BoolAnd with multiple enforcement literal are not "
              "supported in symmetry code:",
              constraint.ShortDebugString());
          return nullptr;
        }

        CHECK_EQ(constraint.enforcement_literal_size(), 1);
        const int ref_a = constraint.enforcement_literal(0);
        for (const int ref_b : constraint.bool_and().literals()) {
          add_implication(ref_a, ref_b);
        }
        break;
      }
      default: {
        // If the model contains any non-supported constraints, return an empty
        // graph.
        //
        // TODO(user): support other types of constraints. Or at least, we
        // could associate to them an unique node so that their variables can
        // appear in no symmetry.
        VLOG(1) << "Unsupported constraint type "
                << ConstraintCaseName(constraint.constraint_case());
        return nullptr;
      }
    }

    // For enforcement, we use a similar trick than for the implications.
    // Because all our constraint arcs are in the direction var_node to
    // constraint_node, we just use the reverse direction for the enforcement
    // part. This way we can reuse the same get_literal_node() function.
    if (constraint.constraint_case() != ConstraintProto::kBoolAnd) {
      for (const int ref : constraint.enforcement_literal()) {
        graph->AddArc(constraint_node, get_literal_node(ref));
      }
    }
  }

  graph->Build();
  DCHECK_EQ(graph->num_nodes(), initial_equivalence_classes->size());

  // TODO(user): The symmetry code does not officially support multi-arcs. And
  // we shouldn't have any as long as there is no duplicates variable in our
  // constraints (but of course, we can't always guarantee that). That said,
  // because the symmetry code really only look at the degree, it works as long
  // as the maximum degree is bounded by num_nodes.
  const int num_nodes = graph->num_nodes();
  std::vector<int> in_degree(num_nodes, 0);
  std::vector<int> out_degree(num_nodes, 0);
  for (int i = 0; i < num_nodes; ++i) {
    out_degree[i] = graph->OutDegree(i);
    for (const int head : (*graph)[i]) {
      in_degree[head]++;
    }
  }
  for (int i = 0; i < num_nodes; ++i) {
    if (in_degree[i] >= num_nodes || out_degree[i] >= num_nodes) {
      SOLVER_LOG(logger, "[Symmetry] Too many multi-arcs in symmetry code.");
      return nullptr;
    }
  }

  // Because this code is running during presolve, a lot a variable might have
  // no edges. We do not want to detect symmetries between these.
  //
  // Note that this code forces us to "densify" the ids afterwards because the
  // symmetry detection code relies on that.
  //
  // TODO(user): It will probably be more efficient to not even create these
  // nodes, but we will need a mapping to know the variable <-> node index.
  int next_id = color_id_generator.NextFreeId();
  for (int i = 0; i < num_variables; ++i) {
    if ((*graph)[i].empty()) {
      (*initial_equivalence_classes)[i] = next_id++;
    }
  }

  // Densify ids.
  int id = 0;
  std::vector<int> mapping(next_id, -1);
  for (int& ref : *initial_equivalence_classes) {
    if (mapping[ref] == -1) {
      ref = mapping[ref] = id++;
    } else {
      ref = mapping[ref];
    }
  }

  return graph;
}
}  // namespace

void FindCpModelSymmetries(
    const SatParameters& params, const CpModelProto& problem,
    std::vector<std::unique_ptr<SparsePermutation>>* generators,
    double deterministic_limit, SolverLogger* logger) {
  CHECK(generators != nullptr);
  generators->clear();

  typedef GraphSymmetryFinder::Graph Graph;

  std::vector<int> equivalence_classes;
  std::unique_ptr<Graph> graph(GenerateGraphForSymmetryDetection<Graph>(
      problem, &equivalence_classes, logger));
  if (graph == nullptr) return;

  SOLVER_LOG(logger, "[Symmetry] Graph for symmetry has ", graph->num_nodes(),
             " nodes and ", graph->num_arcs(), " arcs.");
  if (graph->num_nodes() == 0) return;

  GraphSymmetryFinder symmetry_finder(*graph, /*is_undirected=*/false);
  std::vector<int> factorized_automorphism_group_size;
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromDeterministicTime(deterministic_limit);
  const absl::Status status = symmetry_finder.FindSymmetries(
      &equivalence_classes, generators, &factorized_automorphism_group_size,
      time_limit.get());

  // TODO(user): Change the API to not return an error when the time limit is
  // reached.
  if (!status.ok()) {
    SOLVER_LOG(logger,
               "[Symmetry] GraphSymmetryFinder error: ", status.message());
  }

  // Remove from the permutations the part not concerning the variables.
  // Note that some permutations may become empty, which means that we had
  // duplicate constraints.
  double average_support_size = 0.0;
  int num_generators = 0;
  int num_duplicate_constraints = 0;
  for (int i = 0; i < generators->size(); ++i) {
    SparsePermutation* permutation = (*generators)[i].get();
    std::vector<int> to_delete;
    for (int j = 0; j < permutation->NumCycles(); ++j) {
      // Because variable nodes are in a separate equivalence class than any
      // other node, a cycle can either contain only variable nodes or none, so
      // we just need to check one element of the cycle.
      if (*(permutation->Cycle(j).begin()) >= problem.variables_size()) {
        to_delete.push_back(j);
        if (DEBUG_MODE) {
          // Verify that the cycle's entire support does not touch any variable.
          for (const int node : permutation->Cycle(j)) {
            DCHECK_GE(node, problem.variables_size());
          }
        }
      }
    }

    permutation->RemoveCycles(to_delete);
    if (!permutation->Support().empty()) {
      average_support_size += permutation->Support().size();
      swap((*generators)[num_generators], (*generators)[i]);
      ++num_generators;
    } else {
      ++num_duplicate_constraints;
    }
  }
  generators->resize(num_generators);
  average_support_size /= num_generators;
  SOLVER_LOG(logger, "[Symmetry] Symmetry computation done. time: ",
             time_limit->GetElapsedTime(),
             " dtime: ", time_limit->GetElapsedDeterministicTime());
  if (num_generators > 0) {
    SOLVER_LOG(logger, "[Symmetry] #generators: ", num_generators,
               ", average support size: ", average_support_size);
    if (num_duplicate_constraints > 0) {
      SOLVER_LOG(logger, "[Symmetry] The model contains ",
                 num_duplicate_constraints, " duplicate constraints !");
    }
  }
}

void DetectAndAddSymmetryToProto(const SatParameters& params,
                                 CpModelProto* proto, SolverLogger* logger) {
  SymmetryProto* symmetry = proto->mutable_symmetry();
  symmetry->Clear();

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindCpModelSymmetries(params, *proto, &generators,
                        /*deterministic_limit=*/1.0, logger);
  if (generators.empty()) {
    proto->clear_symmetry();
    return;
  }

  for (const std::unique_ptr<SparsePermutation>& perm : generators) {
    SparsePermutationProto* perm_proto = symmetry->add_permutations();
    const int num_cycle = perm->NumCycles();
    for (int i = 0; i < num_cycle; ++i) {
      const int old_size = perm_proto->support().size();
      for (const int var : perm->Cycle(i)) {
        perm_proto->add_support(var);
      }
      perm_proto->add_cycle_sizes(perm_proto->support().size() - old_size);
    }
  }

  std::vector<std::vector<int>> orbitope = BasicOrbitopeExtraction(generators);
  if (orbitope.empty()) return;
  SOLVER_LOG(logger, "[Symmetry] Found orbitope of size ", orbitope.size(),
             " x ", orbitope[0].size());
  DenseMatrixProto* matrix = symmetry->add_orbitopes();
  matrix->set_num_rows(orbitope.size());
  matrix->set_num_cols(orbitope[0].size());
  for (const std::vector<int>& row : orbitope) {
    for (const int entry : row) {
      matrix->add_entries(entry);
    }
  }
}

namespace {

// Given one Boolean orbit under symmetry, if there is a Boolean at one in this
// orbit, then we can always move it to a fixed position (i.e. the given
// variable var). Moreover, any variable implied to zero in this orbit by var
// being at one can be fixed to zero. This is because, after symmetry breaking,
// either var is one, or all the orbit is zero. We also add implications to
// enforce this fact, but this is not done in this function.
//
// TODO(user): If an exactly one / at least one is included in the orbit, then
// we can set a given variable to one directly. We can also detect this by
// trying to propagate the orbit to all false.
//
// TODO(user): The same reasonning can be done if fixing the variable to
// zero leads to many propagations at one. For general variables, we might be
// able to do something too.
void OrbitAndPropagation(const std::vector<int>& orbits, int var,
                         std::vector<int>* can_be_fixed_to_false,
                         PresolveContext* context) {
  // Note that if a variable is fixed in the orbit, then everything should be
  // fixed.
  if (context->IsFixed(var)) return;
  if (!context->CanBeUsedAsLiteral(var)) return;

  // Lets fix var to true and see what is propagated.
  //
  // TODO(user): Ideally we should have a propagator ready for this. Right now
  // we load the full model if we detected symmetries. We should really combine
  // this with probing even though this is "breaking" the symmetry so it cannot
  // be applied as generally as probing.
  //
  // TODO(user): Note that probing can also benefit from symmetry, since in
  // each orbit, only one variable needs to be probed, and any conclusion can
  // be duplicated to all the variables from an orbit! It is also why we just
  // need to propagate one variable here.
  Model model;
  if (!LoadModelForProbing(context, &model)) return;

  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* mapping = model.GetOrCreate<CpModelMapping>();
  const Literal to_propagate = mapping->Literal(var);

  const VariablesAssignment& assignment = sat_solver->Assignment();
  if (assignment.LiteralIsAssigned(to_propagate)) return;
  sat_solver->EnqueueDecisionAndBackjumpOnConflict(to_propagate);
  if (sat_solver->CurrentDecisionLevel() != 1) return;

  // We can fix to false any variable that is in the orbit and set to false!
  can_be_fixed_to_false->clear();
  int orbit_size = 0;
  const int orbit_index = orbits[var];
  const int num_variables = orbits.size();
  for (int var = 0; var < num_variables; ++var) {
    if (orbits[var] != orbit_index) continue;
    ++orbit_size;

    // By symmetry since same orbit.
    DCHECK(!context->IsFixed(var));
    DCHECK(context->CanBeUsedAsLiteral(var));

    if (assignment.LiteralIsFalse(mapping->Literal(var))) {
      can_be_fixed_to_false->push_back(var);
    }
  }
  if (!can_be_fixed_to_false->empty()) {
    SOLVER_LOG(context->logger(),
               "[Symmetry] Num fixable by binary propagation in orbit: ",
               can_be_fixed_to_false->size(), " / ", orbit_size);
  }
}

}  // namespace

bool DetectAndExploitSymmetriesInPresolve(PresolveContext* context) {
  const SatParameters& params = context->params();
  const CpModelProto& proto = *context->working_model;

  // We need to make sure the proto is up to date before computing symmetries!
  if (context->working_model->has_objective()) {
    context->WriteObjectiveToProto();
  }
  context->WriteVariableDomainsToProto();

  // Tricky: the equivalence relation are not part of the proto.
  // We thus add them temporarily to compute the symmetry.
  int64_t num_added = 0;
  const int initial_ct_index = proto.constraints().size();
  const int num_vars = proto.variables_size();
  for (int var = 0; var < num_vars; ++var) {
    if (context->IsFixed(var)) continue;
    if (context->VariableWasRemoved(var)) continue;
    if (context->VariableIsNotUsedAnymore(var)) continue;

    const AffineRelation::Relation r = context->GetAffineRelation(var);
    if (r.representative == var) continue;

    ++num_added;
    ConstraintProto* ct = context->working_model->add_constraints();
    auto* arg = ct->mutable_linear();
    arg->add_vars(var);
    arg->add_coeffs(1);
    arg->add_vars(r.representative);
    arg->add_coeffs(-r.coeff);
    arg->add_domain(r.offset);
    arg->add_domain(r.offset);
  }

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindCpModelSymmetries(params, proto, &generators,
                        /*deterministic_limit=*/1.0, context->logger());

  // Remove temporary affine relation.
  context->working_model->mutable_constraints()->DeleteSubrange(
      initial_ct_index, num_added);

  if (generators.empty()) return true;

  // Collect the at most ones.
  //
  // Note(user): This relies on the fact that the pointers remain stable when
  // we adds new constraints. It should be the case, but it is a bit unsafe.
  // On the other hand it is annoying to deal with both cases below.
  std::vector<const google::protobuf::RepeatedField<int32_t>*> at_most_ones;
  for (int i = 0; i < proto.constraints_size(); ++i) {
    if (proto.constraints(i).constraint_case() == ConstraintProto::kAtMostOne) {
      at_most_ones.push_back(&proto.constraints(i).at_most_one().literals());
    }
    if (proto.constraints(i).constraint_case() ==
        ConstraintProto::kExactlyOne) {
      at_most_ones.push_back(&proto.constraints(i).exactly_one().literals());
    }
  }

  // We have a few heuristics. The firsts only look at the gobal orbits under
  // the symmetry group and try to infer Boolean variable fixing via symmetry
  // breaking. Note that nothing is fixed yet, we will decide later if we fix
  // these Booleans or not.
  int distinguished_var = -1;
  std::vector<int> can_be_fixed_to_false;

  // Get the global orbits and their size.
  const std::vector<int> orbits = GetOrbits(num_vars, generators);
  std::vector<int> orbit_sizes;
  int max_orbit_size = 0;
  for (int var = 0; var < num_vars; ++var) {
    const int rep = orbits[var];
    if (rep == -1) continue;
    if (rep >= orbit_sizes.size()) orbit_sizes.resize(rep + 1, 0);
    orbit_sizes[rep]++;
    if (orbit_sizes[rep] > max_orbit_size) {
      distinguished_var = var;
      max_orbit_size = orbit_sizes[rep];
    }
  }

  // Log orbit info.
  if (context->logger()->LoggingIsEnabled()) {
    std::vector<int> sorted_sizes;
    for (const int s : orbit_sizes) {
      if (s != 0) sorted_sizes.push_back(s);
    }
    std::sort(sorted_sizes.begin(), sorted_sizes.end(), std::greater<int>());
    const int num_orbits = sorted_sizes.size();
    if (num_orbits > 10) sorted_sizes.resize(10);
    SOLVER_LOG(context->logger(), "[Symmetry] ", num_orbits,
               " orbits with sizes: ", absl::StrJoin(sorted_sizes, ","),
               (num_orbits > sorted_sizes.size() ? ",..." : ""));
  }

  // First heuristic based on propagation, see the function comment.
  if (max_orbit_size > 2) {
    OrbitAndPropagation(orbits, distinguished_var, &can_be_fixed_to_false,
                        context);
  }
  const int first_heuristic_size = can_be_fixed_to_false.size();

  // If an at most one intersect with one or more orbit, in each intersection,
  // we can fix all but one variable to zero. For now we only test positive
  // literal, and maximize the number of fixing.
  //
  // TODO(user): Doing that is not always good, on cod105.mps, fixing variables
  // instead of letting the innner solver handle Boolean symmetries make the
  // problem unsolvable instead of easily solved. This is probably because this
  // fixing do not exploit the full structure of these symmeteries. Note
  // however that the fixing via propagation above close cod105 even more
  // efficiently.
  {
    std::vector<int> tmp_to_clear;
    std::vector<int> tmp_sizes(num_vars, 0);
    for (const google::protobuf::RepeatedField<int32_t>* literals :
         at_most_ones) {
      tmp_to_clear.clear();

      // Compute how many variables we can fix with this at most one.
      int num_fixable = 0;
      for (const int literal : *literals) {
        if (!RefIsPositive(literal)) continue;
        if (context->IsFixed(literal)) continue;

        const int var = PositiveRef(literal);
        const int rep = orbits[var];
        if (rep == -1) continue;

        // We count all but the first one in each orbit.
        if (tmp_sizes[rep] == 0) tmp_to_clear.push_back(rep);
        if (tmp_sizes[rep] > 0) ++num_fixable;
        tmp_sizes[rep]++;
      }

      // Redo a pass to copy the intersection.
      if (num_fixable > can_be_fixed_to_false.size()) {
        distinguished_var = -1;
        can_be_fixed_to_false.clear();
        for (const int literal : *literals) {
          if (!RefIsPositive(literal)) continue;
          if (context->IsFixed(literal)) continue;

          const int var = PositiveRef(literal);
          const int rep = orbits[var];
          if (rep == -1) continue;
          if (distinguished_var == -1 ||
              orbit_sizes[rep] > orbit_sizes[orbits[distinguished_var]]) {
            distinguished_var = var;
          }

          // We push all but the first one in each orbit.
          if (tmp_sizes[rep] == 0) can_be_fixed_to_false.push_back(var);
          tmp_sizes[rep] = 0;
        }
      } else {
        // Sparse clean up.
        for (const int rep : tmp_to_clear) tmp_sizes[rep] = 0;
      }
    }

    if (can_be_fixed_to_false.size() > first_heuristic_size) {
      SOLVER_LOG(
          context->logger(),
          "[Symmetry] Num fixable by intersecting at_most_one with orbits: ",
          can_be_fixed_to_false.size(), " largest_orbit: ", max_orbit_size);
    }
  }

  // Orbitope approach.
  //
  // This is basically the same as the generic approach, but because of the
  // extra structure, computing the orbit of any stabilizer subgroup is easy.
  // We look for orbits intersecting at most one constraints, so we can break
  // symmetry by fixing variables.
  //
  // TODO(user): The same effect could be achieved by adding symmetry breaking
  // constraints of the form "a >= b " between Booleans and let the presolve do
  // the reduction. This might be less code, but it is also less efficient.
  // Similarly, when we cannot just fix variables to break symmetries, we could
  // add these constraints, but it is unclear if we should do it all the time or
  // not.
  //
  // TODO(user): code the generic approach with orbits and stabilizer.
  std::vector<std::vector<int>> orbitope = BasicOrbitopeExtraction(generators);
  if (!orbitope.empty()) {
    SOLVER_LOG(context->logger(), "[Symmetry] Found orbitope of size ",
               orbitope.size(), " x ", orbitope[0].size());
  }

  // Supper simple heuristic to use the orbitope or not.
  //
  // In an orbitope with an at most one on each row, we can fix the upper right
  // triangle. We could use a formula, but the loop is fast enough.
  //
  // TODO(user): Compute the stabilizer under the only non-fixed element and
  // iterate!
  int max_num_fixed_in_orbitope = 0;
  if (!orbitope.empty()) {
    const int num_rows = orbitope[0].size();
    int size_left = num_rows;
    for (int col = 0; size_left > 1 && col < orbitope.size(); ++col) {
      max_num_fixed_in_orbitope += size_left - 1;
      --size_left;
    }
  }
  if (max_num_fixed_in_orbitope < can_be_fixed_to_false.size()) {
    const int orbit_index = orbits[distinguished_var];
    int num_in_orbit = 0;
    for (int i = 0; i < can_be_fixed_to_false.size(); ++i) {
      const int var = can_be_fixed_to_false[i];
      if (orbits[var] == orbit_index) ++num_in_orbit;
      context->UpdateRuleStats("symmetry: fixed to false in general orbit");
      if (!context->SetLiteralToFalse(var)) return false;
    }

    // Moreover, we can add the implication that in the orbit of
    // distinguished_var, either everything is false, or var is at one.
    if (orbit_sizes[orbit_index] > num_in_orbit + 1) {
      context->UpdateRuleStats(
          "symmetry: added orbit symmetry breaking implications");
      auto* ct = context->working_model->add_constraints();
      auto* bool_and = ct->mutable_bool_and();
      ct->add_enforcement_literal(NegatedRef(distinguished_var));
      for (int var = 0; var < num_vars; ++var) {
        if (orbits[var] != orbit_index) continue;
        if (var == distinguished_var) continue;
        if (context->IsFixed(var)) continue;
        bool_and->add_literals(NegatedRef(var));
      }
      context->UpdateNewConstraintsVariableUsage();
    }
    return true;
  }
  if (orbitope.empty()) return true;

  // This will always be kept all zero after usage.
  std::vector<int> tmp_to_clear;
  std::vector<int> tmp_sizes(num_vars, 0);
  std::vector<int> tmp_num_positive(num_vars, 0);

  // TODO(user): The code below requires that no variable appears twice in the
  // same at most one. In particular lit and not(lit) cannot appear in the same
  // at most one.
  for (const google::protobuf::RepeatedField<int32_t>* literals :
       at_most_ones) {
    for (const int lit : *literals) {
      const int var = PositiveRef(lit);
      CHECK_NE(tmp_sizes[var], 1);
      tmp_sizes[var] = 1;
    }
    for (const int lit : *literals) {
      tmp_sizes[PositiveRef(lit)] = 0;
    }
  }

  while (!orbitope.empty() && orbitope[0].size() > 1) {
    const int num_cols = orbitope[0].size();
    const std::vector<int> orbits = GetOrbitopeOrbits(num_vars, orbitope);

    // Because in the orbitope case, we have a full symmetry group of the
    // columns, we can infer more than just using the orbits under a general
    // permutation group. If an at most one contains two variables from the
    // orbit, we can infer:
    // 1/ If the two variables appear positively, then there is an at most one
    //    on the full orbit, and we can set n - 1 variables to zero to break the
    //    symmetry.
    // 2/ If the two variables appear negatively, then the opposite situation
    //    arise and there is at most one zero on the orbit, we can set n - 1
    //    variables to one.
    // 3/ If two literals of opposite sign appear, then the only possibility
    //    for the orbit are all at one or all at zero, thus we can mark all
    //    variables as equivalent.
    //
    // These property comes from the fact that when we permute a line of the
    // orbitope in any way, then the position than ends up in the at most one
    // must never be both at one.
    //
    // Note that 1/ can be done without breaking any symmetry, but for 2/ and 3/
    // by choosing which variable is not fixed, we will break some symmetry, and
    // we will need to update the orbitope to stabilize this choice before
    // continuing.
    //
    // TODO(user): for 2/ and 3/ we could add an at most one constraint on the
    // full orbit if it is not already there!
    //
    // Note(user): On the miplib, only 1/ happens currently. Not sure with LNS
    // though.
    std::vector<bool> all_equivalent_rows(orbitope.size(), false);

    // The result described above can be generalized if an at most one intersect
    // many of the orbitope rows, each in at leat two positions. We will track
    // the set of best rows on which we have an at most one (or at most one
    // zero) on all their entries.
    bool at_most_one_in_best_rows;  // The alternative is at most one zero.
    int64_t best_score = 0;
    std::vector<int> best_rows;

    std::vector<int> rows_in_at_most_one;
    for (const google::protobuf::RepeatedField<int32_t>* literals :
         at_most_ones) {
      tmp_to_clear.clear();
      for (const int literal : *literals) {
        if (context->IsFixed(literal)) continue;
        const int var = PositiveRef(literal);
        const int rep = orbits[var];
        if (rep == -1) continue;

        if (tmp_sizes[rep] == 0) tmp_to_clear.push_back(rep);
        tmp_sizes[rep]++;
        if (RefIsPositive(literal)) tmp_num_positive[rep]++;
      }

      int num_positive_direction = 0;
      int num_negative_direction = 0;

      // An at most one touching two positions in an orbitope row can possibly
      // be extended, depending if it has singleton intersection swith other
      // rows and where.
      bool possible_extension = false;

      rows_in_at_most_one.clear();
      for (const int row : tmp_to_clear) {
        const int size = tmp_sizes[row];
        const int num_positive = tmp_num_positive[row];
        const int num_negative = tmp_sizes[row] - tmp_num_positive[row];
        tmp_sizes[row] = 0;
        tmp_num_positive[row] = 0;

        if (num_positive > 1 && num_negative == 0) {
          if (size < num_cols) possible_extension = true;
          rows_in_at_most_one.push_back(row);
          ++num_positive_direction;
        } else if (num_positive == 0 && num_negative > 1) {
          if (size < num_cols) possible_extension = true;
          rows_in_at_most_one.push_back(row);
          ++num_negative_direction;
        } else if (num_positive > 0 && num_negative > 0) {
          all_equivalent_rows[row] = true;
        }
      }

      if (possible_extension) {
        context->UpdateRuleStats(
            "TODO symmetry: possible at most one extension.");
      }

      if (num_positive_direction > 0 && num_negative_direction > 0) {
        return context->NotifyThatModelIsUnsat("Symmetry and at most ones");
      }
      const bool direction = num_positive_direction > 0;

      // Because of symmetry, the choice of the column shouldn't matter (they
      // will all appear in the same number of constraints of the same types),
      // however we prefer to fix the variables that seems to touch more
      // constraints.
      //
      // TODO(user): maybe we should simplify the constraint using the variable
      // we fix before choosing the next row to break symmetry on. If there are
      // multiple row involved, we could also take the intersection instead of
      // probably counting the same constraints more than once.
      int64_t score = 0;
      for (const int row : rows_in_at_most_one) {
        score +=
            context->VarToConstraints(PositiveRef(orbitope[row][0])).size();
      }
      if (score > best_score) {
        at_most_one_in_best_rows = direction;
        best_score = score;
        best_rows = rows_in_at_most_one;
      }
    }

    // Mark all the equivalence.
    // Note that this operation do not change the symmetry group.
    //
    // TODO(user): We could remove these rows from the orbitope. Note that
    // currently this never happen on the miplib (maybe in LNS though).
    for (int i = 0; i < all_equivalent_rows.size(); ++i) {
      if (all_equivalent_rows[i]) {
        for (int j = 1; j < num_cols; ++j) {
          context->StoreBooleanEqualityRelation(orbitope[i][0], orbitope[i][j]);
          context->UpdateRuleStats("symmetry: all equivalent in orbit");
          if (context->ModelIsUnsat()) return false;
        }
      }
    }

    // Break the symmetry on our set of best rows by picking one columns
    // and setting all the other entries to zero or one. Note that the at most
    // one applies to all entries in all rows.
    //
    // TODO(user): We don't have any at most one relation on this orbitope,
    // but we could still add symmetry breaking inequality by picking any matrix
    // entry and making it the largest/lowest value on its row. This also work
    // for non-Booleans.
    if (best_score == 0) {
      context->UpdateRuleStats(
          "TODO symmetry: add symmetry breaking inequalities?");
      break;
    }

    // If our symmetry group is valid, they cannot be any variable already
    // fixed to one (or zero if !at_most_one_in_best_rows). Otherwise all would
    // be fixed to one and the problem would be unsat.
    for (const int i : best_rows) {
      for (int j = 0; j < num_cols; ++j) {
        const int var = orbitope[i][j];
        if ((at_most_one_in_best_rows && context->LiteralIsTrue(var)) ||
            (!at_most_one_in_best_rows && context->LiteralIsFalse(var))) {
          return context->NotifyThatModelIsUnsat("Symmetry and at most one");
        }
      }
    }

    // We have an at most one on a set of rows, we will pick a column, and set
    // all other entries on these rows to zero.
    //
    // TODO(user): All choices should be equivalent, but double check?
    const int best_col = 0;
    for (const int i : best_rows) {
      for (int j = 0; j < num_cols; ++j) {
        if (j == best_col) continue;
        const int var = orbitope[i][j];
        if (at_most_one_in_best_rows) {
          context->UpdateRuleStats("symmetry: fixed to false");
          if (!context->SetLiteralToFalse(var)) return false;
        } else {
          context->UpdateRuleStats("symmetry: fixed to true");
          if (!context->SetLiteralToTrue(var)) return false;
        }
      }
    }

    // Remove all best rows.
    for (const int i : best_rows) orbitope[i].clear();
    int new_size = 0;
    for (int i = 0; i < orbitope.size(); ++i) {
      if (!orbitope[i].empty()) orbitope[new_size++] = orbitope[i];
    }
    CHECK_LT(new_size, orbitope.size());
    orbitope.resize(new_size);

    // Remove best_col.
    for (int i = 0; i < orbitope.size(); ++i) {
      std::swap(orbitope[i][best_col], orbitope[i].back());
      orbitope[i].pop_back();
    }
  }

  // If we are left with a set of variable than can all be permuted, lets
  // break the symmetry by ordering them.
  if (orbitope.size() == 1) {
    const int num_cols = orbitope[0].size();
    for (int i = 0; i + 1 < num_cols; ++i) {
      // Add orbitope[0][i] >= orbitope[0][i+1].
      ConstraintProto* ct = context->working_model->add_constraints();
      ct->mutable_linear()->add_coeffs(1);
      ct->mutable_linear()->add_vars(orbitope[0][i]);
      ct->mutable_linear()->add_coeffs(-1);
      ct->mutable_linear()->add_vars(orbitope[0][i + 1]);
      ct->mutable_linear()->add_domain(0);
      ct->mutable_linear()->add_domain(std::numeric_limits<int64_t>::max());
      context->UpdateRuleStats("symmetry: added symmetry breaking inequality");
    }
    context->UpdateNewConstraintsVariableUsage();
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
