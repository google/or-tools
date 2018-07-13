// Copyright 2010-2017 Google
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

#include <unordered_map>
#include "google/protobuf/repeated_field.h"
#include "ortools/algorithms/find_graph_symmetries.h"
#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/base/memory.h"
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
  std::unordered_map<std::vector<int64>, int, VectorHash> id_map_;
};

// Appends values in `repeated_field` to `vector`.
template <typename FieldIntType>
void Append(const google::protobuf::RepeatedField<FieldIntType>& repeated_field,
            std::vector<int64>* vector) {
  CHECK(vector != nullptr);
  for (const FieldIntType value : repeated_field) {
    vector->push_back(value);
  }
}

template <typename Graph>
void AddEdge(int node_index_1, int node_index_2, Graph* graph) {
  CHECK(graph != nullptr);
  graph->AddArc(node_index_1, node_index_2);
  graph->AddArc(node_index_2, node_index_1);
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

  // If the model contains any variables with enforcement literals, return here.
  for (const IntegerVariableProto& variable : problem.variables()) {
    if (variable.enforcement_literal_size() > 0) {
      // TODO(user): support enforcement literals on variables.
      LOG(ERROR) << "Unsupported enforcement literal on variables.";
      return nullptr;
    }
  }

  const int num_variables = problem.variables_size();
  auto graph = absl::make_unique<Graph>();
  initial_equivalence_classes->clear();
  enum NodeType { VARIABLE_NODE, CONSTRAINT_NODE, CONSTRAINT_COEFFICIENT_NODE };
  IdGenerator id_generator;

  // For two variables to be in the same equivalence class, they need to have
  // the same objective coefficient, and the same possible bounds.
  std::vector<int64> objective_by_var(num_variables, 0);
  for (int i = 0; i < problem.objective().vars_size(); ++i) {
    objective_by_var[problem.objective().vars(i)] =
        problem.objective().coeffs(i);
  }

  for (int v = 0; v < num_variables; ++v) {
    const IntegerVariableProto& variable = problem.variables(v);
    std::vector<int64> key = {VARIABLE_NODE, objective_by_var[v]};
    Append(variable.domain(), &key);
    initial_equivalence_classes->push_back(id_generator.GetId(key));
    // Make sure the graph contains all the variable nodes, even if no edges are
    // attached to them through constraints.
    graph->AddNode(v);
  }

  // Add constraints to the graph.
  for (const ConstraintProto& constraint : problem.constraints()) {
    // Since we add nodes one by one, initial_equivalence_classes->size() gives
    // the number of nodes at any point, which we use as the next node index.
    const int constraint_node_index = initial_equivalence_classes->size();
    switch (constraint.constraint_case()) {
      case ConstraintProto::kLinear: {
        std::vector<int64> key = {CONSTRAINT_NODE, ConstraintProto::kLinear};
        Append(constraint.linear().domain(), &key);
        initial_equivalence_classes->push_back(id_generator.GetId(key));

        for (int i = 0; i < constraint.linear().vars_size(); ++i) {
          if (constraint.enforcement_literal_size() > 0) {
            // TODO(user): support enforcement literals on constraints.
            LOG(ERROR) << "Unsupported enforcement literal on constraints.";
            return nullptr;
          }
          if (constraint.linear().coeffs(i) == 0) continue;
          const int ref = constraint.linear().vars(i);
          const int variable_node_index = PositiveRef(ref);
          const int64 coeff = RefIsPositive(ref)
                                  ? constraint.linear().coeffs(i)
                                  : -constraint.linear().coeffs(i);
          if (coeff == 1) {
            // For all coefficients equal to one, which are the most common, we
            // can optimize the size of the graph by omitting the coefficient
            // node altogether.
            AddEdge(variable_node_index, constraint_node_index, graph.get());
          } else {
            const int coefficient_node_index =
                initial_equivalence_classes->size();
            initial_equivalence_classes->push_back(
                id_generator.GetId({CONSTRAINT_COEFFICIENT_NODE, coeff}));

            AddEdge(variable_node_index, coefficient_node_index, graph.get());
            AddEdge(coefficient_node_index, constraint_node_index, graph.get());
          }
        }
        break;
      }
      default: {
        // If the model contains any non-supported constraints, return an empty
        // graph.
        // TODO(user): support other types of constraints.
        LOG(ERROR) << "Unsupported constraint type "
                   << constraint.constraint_case();
        return nullptr;
      }
    }
  }

  graph->Build();
  DCHECK_EQ(graph->num_nodes(), initial_equivalence_classes->size());
  return graph;
}
}  // namespace

void FindCpModelSymmetries(
    const CpModelProto& problem,
    std::vector<std::unique_ptr<SparsePermutation>>* generators,
    double time_limit_seconds) {
  CHECK(generators != nullptr);
  generators->clear();

  typedef GraphSymmetryFinder::Graph Graph;

  std::vector<int> equivalence_classes;
  std::unique_ptr<Graph> graph(
      GenerateGraphForSymmetryDetection<Graph>(problem, &equivalence_classes));
  if (graph == nullptr) return;

  LOG(INFO) << "Graph has " << graph->num_nodes() << " nodes and "
            << graph->num_arcs() / 2 << " edges.";

  GraphSymmetryFinder symmetry_finder(*graph, /*is_undirected=*/true);
  std::vector<int> factorized_automorphism_group_size;
  CHECK_OK(symmetry_finder.FindSymmetries(time_limit_seconds,
                                          &equivalence_classes, generators,
                                          &factorized_automorphism_group_size));

  // Remove from the permutations the part not concerning the variables.
  // Note that some permutation may become empty, which means that we had
  // duplicate constraints.
  double average_support_size = 0.0;
  int num_generators = 0;
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
      LOG(INFO) << "The model contains duplicate constraints!";
    }
  }
  generators->resize(num_generators);
  average_support_size /= num_generators;
  LOG(INFO) << "# of generators: " << num_generators;
  LOG(INFO) << "Average support size: " << average_support_size;
}

}  // namespace sat
}  // namespace operations_research
