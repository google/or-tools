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

#include "ortools/sat/cp_model_symmetries.h"

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "google/protobuf/repeated_field.h"
#include "ortools/algorithms/find_graph_symmetries.h"
#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

namespace {
struct VectorHash {
  std::size_t operator()(const std::vector<int64>& values) const {
    size_t hash = 0;
    for (const int64 value : values) {
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

  // If the key was never seen before, then generate a new id, otherwise return
  // the previously generated id.
  int GetId(const std::vector<int64>& key) {
    return gtl::LookupOrInsert(&id_map_, key, id_map_.size());
  }

 private:
  absl::flat_hash_map<std::vector<int64>, int, VectorHash> id_map_;
};

// Appends values in `repeated_field` to `vector`.
//
// We use a template as proto int64 != C++ int64 in open source.
template <typename FieldInt64Type>
void Append(
    const google::protobuf::RepeatedField<FieldInt64Type>& repeated_field,
    std::vector<int64>* vector) {
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
    const CpModelProto& problem,
    std::vector<int>* initial_equivalence_classes) {
  CHECK(initial_equivalence_classes != nullptr);

  const int num_variables = problem.variables_size();
  auto graph = absl::make_unique<Graph>();
  initial_equivalence_classes->clear();
  enum NodeType {
    VARIABLE_NODE,
    CONSTRAINT_NODE,
    CONSTRAINT_COEFFICIENT_NODE,
    ENFORCEMENT_LITERAL
  };
  IdGenerator id_generator;

  // For two variables to be in the same equivalence class, they need to have
  // the same objective coefficient, and the same possible bounds.
  std::vector<int64> objective_by_var(num_variables, 0);
  for (int i = 0; i < problem.objective().vars_size(); ++i) {
    objective_by_var[problem.objective().vars(i)] =
        problem.objective().coeffs(i);
  }

  auto new_node = [&initial_equivalence_classes,
                   &id_generator](const std::vector<int64>& key) {
    // Since we add nodes one by one, initial_equivalence_classes->size() gives
    // the number of nodes at any point, which we use as the next node index.
    const int node = initial_equivalence_classes->size();
    initial_equivalence_classes->push_back(id_generator.GetId(key));
    return node;
  };

  std::vector<int64> tmp_key;
  for (int v = 0; v < num_variables; ++v) {
    const IntegerVariableProto& variable = problem.variables(v);
    tmp_key = {VARIABLE_NODE, objective_by_var[v]};
    Append(variable.domain(), &tmp_key);
    CHECK_EQ(v, new_node(tmp_key));

    // Make sure the graph contains all the variable nodes, even if no edges are
    // attached to them through constraints.
    graph->AddNode(v);
  }

  auto add_edge = [&graph](int node_1, int node_2) {
    graph->AddArc(node_1, node_2);
    graph->AddArc(node_2, node_1);
  };

  // We will create a bunch of nodes linked to a variable node. Only one node
  // per (var, type) will be required so we cache them to not create more node
  // than necessary.
  absl::flat_hash_map<std::vector<int64>, int, VectorHash> secondary_var_nodes;
  auto get_secondary_var_node = [&new_node, &add_edge, &secondary_var_nodes,
                                 &tmp_key](int var_node,
                                           absl::Span<const int64> type) {
    tmp_key.assign(type.begin(), type.end());
    tmp_key.push_back(var_node);
    const auto insert = secondary_var_nodes.insert({tmp_key, 0});
    if (!insert.second) return insert.first->second;

    tmp_key.pop_back();
    const int secondary_node = new_node(tmp_key);
    add_edge(var_node, secondary_node);
    insert.first->second = secondary_node;
    return secondary_node;
  };

  auto add_literal_edge = [&add_edge, &get_secondary_var_node](
                              int ref, int constraint_node) {
    const int variable_node = PositiveRef(ref);
    if (RefIsPositive(ref)) {
      // For all coefficients equal to one, which are the most common, we
      // can optimize the size of the graph by omitting the coefficient
      // node altogether.
      add_edge(variable_node, constraint_node);
    } else {
      const int coefficient_node = get_secondary_var_node(
          variable_node, {CONSTRAINT_COEFFICIENT_NODE, -1});
      add_edge(coefficient_node, constraint_node);
    }
  };

  // Add constraints to the graph.
  for (const ConstraintProto& constraint : problem.constraints()) {
    const int constraint_node = initial_equivalence_classes->size();
    std::vector<int64> key = {CONSTRAINT_NODE, constraint.constraint_case()};

    switch (constraint.constraint_case()) {
      case ConstraintProto::kLinear: {
        Append(constraint.linear().domain(), &key);
        CHECK_EQ(constraint_node, new_node(key));

        for (int i = 0; i < constraint.linear().vars_size(); ++i) {
          if (constraint.linear().coeffs(i) == 0) continue;
          const int ref = constraint.linear().vars(i);
          const int variable_node = PositiveRef(ref);
          const int64 coeff = RefIsPositive(ref)
                                  ? constraint.linear().coeffs(i)
                                  : -constraint.linear().coeffs(i);
          if (coeff == 1) {
            // For all coefficients equal to one, which are the most common, we
            // can optimize the size of the graph by omitting the coefficient
            // node altogether.
            add_edge(variable_node, constraint_node);
          } else {
            const int coefficient_node = get_secondary_var_node(
                variable_node, {CONSTRAINT_COEFFICIENT_NODE, coeff});
            add_edge(coefficient_node, constraint_node);
          }
        }
        break;
      }
      case ConstraintProto::kBoolOr: {
        CHECK_EQ(constraint_node, new_node(key));
        for (const int ref : constraint.bool_or().literals()) {
          add_literal_edge(ref, constraint_node);
        }
        break;
      }
      case ConstraintProto::kAtMostOne: {
        CHECK_EQ(constraint_node, new_node(key));
        for (const int ref : constraint.at_most_one().literals()) {
          add_literal_edge(ref, constraint_node);
        }
        break;
      }
      case ConstraintProto::kExactlyOne: {
        CHECK_EQ(constraint_node, new_node(key));
        for (const int ref : constraint.exactly_one().literals()) {
          add_literal_edge(ref, constraint_node);
        }
        break;
      }
      case ConstraintProto::kBoolXor: {
        CHECK_EQ(constraint_node, new_node(key));
        for (const int ref : constraint.bool_xor().literals()) {
          add_literal_edge(ref, constraint_node);
        }
        break;
      }

      // TODO(user): We could directly connect variable node together to
      // deal more efficiently with this constraint. Make sure not to create
      // multi-arc since I am not sure the symmetry code works with these
      // though.
      case ConstraintProto::kBoolAnd: {
        if (constraint.enforcement_literal_size() == 0) {
          // All literals are true in this case.
          CHECK_EQ(constraint_node, new_node(key));
          for (const int ref : constraint.bool_and().literals()) {
            add_literal_edge(ref, constraint_node);
          }
          break;
        }

        // To make the BoolAnd constraint more generic in the graph, we expand
        // it into a set of BoolOr constraints where
        //   not(enforcement_literal) OR literal = true
        // for all constraint's literals. This is equivalent to
        //   enforcement_literal => literal
        // for all literals.
        std::vector<int64> key = {CONSTRAINT_NODE, ConstraintProto::kBoolOr};
        const int non_enforcement_literal =
            NegatedRef(constraint.enforcement_literal(0));
        for (const int literal : constraint.bool_and().literals()) {
          const int constraint_node = new_node(key);
          add_literal_edge(non_enforcement_literal, constraint_node);
          add_literal_edge(literal, constraint_node);
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
        LOG(ERROR) << "Unsupported constraint type "
                   << ConstraintCaseName(constraint.constraint_case());
        return nullptr;
      }
    }

    if (constraint.constraint_case() != ConstraintProto::kBoolAnd) {
      for (const int ref : constraint.enforcement_literal()) {
        const int enforcement_node = get_secondary_var_node(
            PositiveRef(ref), {ENFORCEMENT_LITERAL, RefIsPositive(ref)});
        add_edge(constraint_node, enforcement_node);
      }
    }
  }

  graph->Build();
  DCHECK_EQ(graph->num_nodes(), initial_equivalence_classes->size());
  return graph;
}
}  // namespace

void FindCpModelSymmetries(
    const SatParameters& params, const CpModelProto& problem,
    std::vector<std::unique_ptr<SparsePermutation>>* generators,
    double time_limit_seconds) {
  const bool log_info = params.log_search_progress() || VLOG_IS_ON(1);
  CHECK(generators != nullptr);
  generators->clear();

  typedef GraphSymmetryFinder::Graph Graph;

  std::vector<int> equivalence_classes;
  std::unique_ptr<Graph> graph(
      GenerateGraphForSymmetryDetection<Graph>(problem, &equivalence_classes));
  if (graph == nullptr) return;

  if (log_info) {
    LOG(INFO) << "Graph has " << graph->num_nodes() << " nodes and "
              << graph->num_arcs() / 2 << " edges.";
  }
  if (graph->num_nodes() == 0) return;

  GraphSymmetryFinder symmetry_finder(*graph, /*is_undirected=*/true);
  std::vector<int> factorized_automorphism_group_size;
  const absl::Status status = symmetry_finder.FindSymmetries(
      time_limit_seconds, &equivalence_classes, generators,
      &factorized_automorphism_group_size);

  // TODO(user): Change the API to not return an error when the time limit is
  // reached.
  if (log_info && !status.ok()) {
    LOG(INFO) << "GraphSymmetryFinder error: " << status.message();
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
  if (log_info) {
    LOG(INFO) << "# of generators: " << num_generators;
    LOG(INFO) << "Average support size: " << average_support_size;
    if (num_duplicate_constraints > 0) {
      LOG(INFO) << "The model contains " << num_duplicate_constraints
                << " duplicate constraints !";
    }
  }
}

}  // namespace sat
}  // namespace operations_research
