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

#include "ortools/sat/cp_model_symmetries.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "google/protobuf/message.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/algorithms/find_graph_symmetries.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/hash.h"
#include "ortools/base/logging.h"
#include "ortools/graph/graph.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/symmetry_util.h"
#include "ortools/sat/util.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {
struct VectorHash {
  std::size_t operator()(absl::Span<const int64_t> values) const {
    size_t hash = 0;
    for (const int64_t value : values) {
      hash = util_hash::Hash(value, hash);
    }
    return hash;
  }
};

struct NodeExprCompare {
  bool operator()(const LinearExpressionProto& a,
                  const LinearExpressionProto& b) const {
    if (a.offset() != b.offset()) return a.offset() < b.offset();
    if (a.vars_size() != b.vars_size()) return a.vars_size() < b.vars_size();
    for (int i = 0; i < a.vars_size(); ++i) {
      if (a.vars(i) != b.vars(i)) return a.vars(i) < b.vars(i);
      if (a.coeffs(i) != b.coeffs(i)) return a.coeffs(i) < b.coeffs(i);
    }
    return false;
  }
};

// A simple class to generate equivalence class number for
// GenerateGraphForSymmetryDetection().
class IdGenerator {
 public:
  IdGenerator() = default;

  // If the color was never seen before, then generate a new id, otherwise
  // return the previously generated id.
  int GetId(const std::vector<int64_t>& color) {
    // Do not use try_emplace. It breaks with gcc13 on or-tools.
    return id_map_.insert({color, id_map_.size()}).first->second;
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

bool IsIntervalFixedSize(const IntervalConstraintProto& interval) {
  if (!interval.size().vars().empty()) {
    return false;
  }
  if (interval.start().vars().size() != interval.end().vars().size()) {
    return false;
  }
  for (int i = 0; i < interval.start().vars().size(); ++i) {
    if (interval.start().coeffs(i) != interval.end().coeffs(i)) {
      return false;
    }
    if (interval.start().vars(i) != interval.end().vars(i)) {
      return false;
    }
  }
  if (interval.end().offset() !=
      interval.start().offset() + interval.size().offset()) {
    return false;
  }
  return true;
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
  auto graph = std::make_unique<Graph>();

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
    VAR_LIN_EXPR_NODE,
  };
  IdGenerator color_id_generator;
  initial_equivalence_classes->clear();
  auto new_node_from_id = [&initial_equivalence_classes, &graph](int color_id) {
    // Since we add nodes one by one, initial_equivalence_classes->size() gives
    // the number of nodes at any point, which we use as the next node index.
    const int node = initial_equivalence_classes->size();
    initial_equivalence_classes->push_back(color_id);

    // In some corner cases, we create a node but never uses it. We still
    // want it to be there.
    graph->AddNode(node);
    return node;
  };
  auto new_node = [&new_node_from_id,
                   &color_id_generator](const std::vector<int64_t>& color) {
    return new_node_from_id(color_id_generator.GetId(color));
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

  const int color_id_for_coeff_one =
      color_id_generator.GetId({VAR_COEFFICIENT_NODE, 1});
  const int color_id_for_coeff_minus_one =
      color_id_generator.GetId({VAR_COEFFICIENT_NODE, -1});

  // We will lazily create "coefficient nodes" that correspond to a variable
  // with a given coefficient.
  absl::flat_hash_map<std::pair<int64_t, int64_t>, int> coefficient_nodes;
  auto get_coefficient_node =
      [&new_node_from_id, &graph, &coefficient_nodes, &color_id_generator,
       &tmp_color, color_id_for_coeff_minus_one](int var, int64_t coeff) {
        const int var_node = var;
        DCHECK(RefIsPositive(var));

        // For a coefficient of one, which are the most common, we can optimize
        // the size of the graph by omitting the coefficient node altogether and
        // using directly the var_node in this case.
        if (coeff == 1) return var_node;

        const auto insert =
            coefficient_nodes.insert({std::make_pair(var, coeff), 0});
        if (!insert.second) return insert.first->second;

        int color_id;
        // Because -1 is really common (also used for negated literal), we have
        // a fast path for it.
        if (coeff == -1) {
          color_id = color_id_for_coeff_minus_one;
        } else {
          tmp_color = {VAR_COEFFICIENT_NODE, coeff};
          color_id = color_id_generator.GetId(tmp_color);
        }
        const int secondary_node = new_node_from_id(color_id);
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
  auto get_implication_node = [&new_node_from_id, &graph, &coefficient_nodes,
                               color_id_for_coeff_one,
                               color_id_for_coeff_minus_one](int ref) {
    const int var = PositiveRef(ref);
    const int64_t coeff = RefIsPositive(ref) ? 1 : -1;
    const auto insert =
        coefficient_nodes.insert({std::make_pair(var, coeff), 0});
    if (!insert.second) return insert.first->second;
    const int secondary_node = new_node_from_id(
        coeff == 1 ? color_id_for_coeff_one : color_id_for_coeff_minus_one);
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

  auto make_linear_expr_node = [&new_node, &graph, &get_coefficient_node](
                                   const LinearExpressionProto& expr,
                                   const std::vector<int64_t>& color) {
    std::vector<int64_t> local_color = color;
    local_color.push_back(expr.offset());
    const int local_node = new_node(local_color);

    for (int i = 0; i < expr.vars().size(); ++i) {
      const int ref = expr.vars(i);
      const int var_node = PositiveRef(ref);
      const int64_t coeff =
          RefIsPositive(ref) ? expr.coeffs(i) : -expr.coeffs(i);
      graph->AddArc(get_coefficient_node(var_node, coeff), local_node);
    }
    return local_node;
  };

  absl::btree_map<LinearExpressionProto, int, NodeExprCompare> expr_nodes;
  auto shared_linear_expr_node =
      [&make_linear_expr_node, &expr_nodes](const LinearExpressionProto& expr) {
        const auto [it, inserted] = expr_nodes.insert({expr, 0});
        if (inserted) {
          const std::vector<int64_t> local_color = {VAR_LIN_EXPR_NODE,
                                                    expr.offset()};
          it->second = make_linear_expr_node(expr, local_color);
        }
        return it->second;
      };

  // We need to keep track of this for scheduling constraints.
  absl::flat_hash_map<int, int> interval_constraint_index_to_node;

  // Add constraints to the graph.
  for (int constraint_index = 0; constraint_index < problem.constraints_size();
       ++constraint_index) {
    const ConstraintProto& constraint = problem.constraints(constraint_index);
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
      case ConstraintProto::kAllDiff: {
        CHECK_EQ(constraint_node, new_node(color));
        for (const LinearExpressionProto& expr :
             constraint.all_diff().exprs()) {
          graph->AddArc(shared_linear_expr_node(expr), constraint_node);
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
        if (constraint.enforcement_literal_size() > 1) {
          CHECK_EQ(constraint_node, new_node(color));
          for (const int ref : constraint.bool_and().literals()) {
            graph->AddArc(get_literal_node(ref), constraint_node);
          }
          break;
        }

        CHECK_EQ(constraint.enforcement_literal_size(), 1);
        const int ref_a = constraint.enforcement_literal(0);
        for (const int ref_b : constraint.bool_and().literals()) {
          add_implication(ref_a, ref_b);
        }
        break;
      }
      case ConstraintProto::kLinMax: {
        const LinearExpressionProto& target_expr =
            constraint.lin_max().target();

        const int target_node = make_linear_expr_node(target_expr, color);

        for (int i = 0; i < constraint.lin_max().exprs_size(); ++i) {
          const LinearExpressionProto& expr = constraint.lin_max().exprs(i);
          graph->AddArc(shared_linear_expr_node(expr), target_node);
        }

        break;
      }
      case ConstraintProto::kInterval: {
        static constexpr int kFixedIntervalColor = 0;
        static constexpr int kNonFixedIntervalColor = 1;
        if (IsIntervalFixedSize(constraint.interval())) {
          std::vector<int64_t> local_color = color;
          local_color.push_back(kFixedIntervalColor);
          local_color.push_back(constraint.interval().size().offset());
          const int full_node =
              make_linear_expr_node(constraint.interval().start(), local_color);
          CHECK_EQ(full_node, constraint_node);
        } else {
          // We create 3 constraint nodes (for start, size and end) including
          // the offset. We connect these to their terms like for a linear
          // constraint.
          std::vector<int64_t> local_color = color;
          local_color.push_back(kNonFixedIntervalColor);

          local_color.push_back(0);
          const int start_node =
              make_linear_expr_node(constraint.interval().start(), local_color);
          local_color.pop_back();
          CHECK_EQ(start_node, constraint_node);

          // We can use a shared node for one of the three. Let's use the size
          // since it has the most chance of being reused.
          const int size_node =
              shared_linear_expr_node(constraint.interval().size());

          local_color.push_back(1);
          const int end_node =
              make_linear_expr_node(constraint.interval().end(), local_color);
          local_color.pop_back();

          // Make sure that if one node is mapped to another one, its other two
          // components are the same.
          graph->AddArc(start_node, end_node);
          graph->AddArc(end_node, size_node);
        }
        interval_constraint_index_to_node[constraint_index] = constraint_node;
        break;
      }
      case ConstraintProto::kNoOverlap: {
        // Note(user): This require that intervals appear before they are used.
        // We currently enforce this at validation, otherwise we need two passes
        // here and in a bunch of other places.
        CHECK_EQ(constraint_node, new_node(color));
        for (const int interval : constraint.no_overlap().intervals()) {
          graph->AddArc(interval_constraint_index_to_node.at(interval),
                        constraint_node);
        }
        break;
      }
      case ConstraintProto::kNoOverlap2D: {
        // Note(user): This require that intervals appear before they are used.
        // We currently enforce this at validation, otherwise we need two passes
        // here and in a bunch of other places.
        CHECK_EQ(constraint_node, new_node(color));
        std::vector<int64_t> local_color = color;
        local_color.push_back(0);
        const int size = constraint.no_overlap_2d().x_intervals().size();
        const int node_x = new_node(local_color);
        const int node_y = new_node(local_color);
        local_color.pop_back();
        graph->AddArc(constraint_node, node_x);
        graph->AddArc(constraint_node, node_y);
        local_color.push_back(1);
        for (int i = 0; i < size; ++i) {
          const int box_node = new_node(local_color);
          graph->AddArc(box_node, constraint_node);
          const int x = constraint.no_overlap_2d().x_intervals(i);
          const int y = constraint.no_overlap_2d().y_intervals(i);
          graph->AddArc(interval_constraint_index_to_node.at(x), node_x);
          graph->AddArc(interval_constraint_index_to_node.at(x), box_node);
          graph->AddArc(interval_constraint_index_to_node.at(y), node_y);
          graph->AddArc(interval_constraint_index_to_node.at(y), box_node);
        }
        break;
      }
      case ConstraintProto::kCumulative: {
        // Note(user): This require that intervals appear before they are used.
        // We currently enforce this at validation, otherwise we need two passes
        // here and in a bunch of other places.
        const CumulativeConstraintProto& ct = constraint.cumulative();
        std::vector<int64_t> capacity_color = color;
        capacity_color.push_back(0);
        CHECK_EQ(constraint_node, new_node(capacity_color));
        graph->AddArc(constraint_node,
                      make_linear_expr_node(ct.capacity(), capacity_color));

        std::vector<int64_t> task_color = color;
        task_color.push_back(1);
        for (int i = 0; i < ct.intervals().size(); ++i) {
          const int task_node =
              make_linear_expr_node(ct.demands(i), task_color);
          graph->AddArc(task_node, constraint_node);
          graph->AddArc(task_node,
                        interval_constraint_index_to_node.at(ct.intervals(i)));
        }
        break;
      }
      case ConstraintProto::kCircuit: {
        // Note that this implementation will generate the same graph for a
        // circuit constraint with two disconnected components and two circuit
        // constraints with one component each.
        const int num_arcs = constraint.circuit().literals().size();
        absl::flat_hash_map<int, int> circuit_node_to_symmetry_node;
        std::vector<int64_t> arc_color = color;
        arc_color.push_back(1);
        for (int i = 0; i < num_arcs; ++i) {
          const int literal = constraint.circuit().literals(i);
          const int tail = constraint.circuit().tails(i);
          const int head = constraint.circuit().heads(i);
          const int arc_node = new_node(arc_color);
          if (!circuit_node_to_symmetry_node.contains(head)) {
            circuit_node_to_symmetry_node[head] = new_node(color);
          }
          const int head_node = circuit_node_to_symmetry_node[head];
          if (!circuit_node_to_symmetry_node.contains(tail)) {
            circuit_node_to_symmetry_node[tail] = new_node(color);
          }
          const int tail_node = circuit_node_to_symmetry_node[tail];
          // To make the graph directed, we add two arcs on the head but not on
          // the tail.
          graph->AddArc(tail_node, arc_node);
          graph->AddArc(arc_node, get_literal_node(literal));
          graph->AddArc(arc_node, head_node);
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
    if (constraint.constraint_case() != ConstraintProto::kBoolAnd ||
        constraint.enforcement_literal().size() > 1) {
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

  if (params.symmetry_level() < 3 && problem.variables().size() > 1e6 &&
      problem.constraints().size() > 1e6) {
    SOLVER_LOG(logger,
               "[Symmetry] Problem too large. Skipping. You can use "
               "symmetry_level:3 or more to force it.");
    return;
  }

  typedef GraphSymmetryFinder::Graph Graph;

  std::vector<int> equivalence_classes;
  std::unique_ptr<Graph> graph(GenerateGraphForSymmetryDetection<Graph>(
      problem, &equivalence_classes, logger));
  if (graph == nullptr) return;

  SOLVER_LOG(logger, "[Symmetry] Graph for symmetry has ",
             FormatCounter(graph->num_nodes()), " nodes and ",
             FormatCounter(graph->num_arcs()), " arcs.");
  if (graph->num_nodes() == 0) return;

  if (params.symmetry_level() < 3 && graph->num_nodes() > 1e6 &&
      graph->num_arcs() > 1e6) {
    SOLVER_LOG(logger,
               "[Symmetry] Graph too large. Skipping. You can use "
               "symmetry_level:3 or more to force it.");
    return;
  }

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
  SOLVER_LOG(logger, "[Symmetry] Symmetry computation done. time: ",
             time_limit->GetElapsedTime(),
             " dtime: ", time_limit->GetElapsedDeterministicTime());
  if (num_generators > 0) {
    average_support_size /= num_generators;
    SOLVER_LOG(logger, "[Symmetry] #generators: ", num_generators,
               ", average support size: ", average_support_size);
    if (num_duplicate_constraints > 0) {
      SOLVER_LOG(logger, "[Symmetry] The model contains ",
                 num_duplicate_constraints, " duplicate constraints !");
    }
  }
}

namespace {

void LogOrbitInformation(absl::Span<const int> var_to_orbit_index,
                         SolverLogger* logger) {
  if (logger == nullptr || !logger->LoggingIsEnabled()) return;

  int num_touched_vars = 0;
  std::vector<int> orbit_sizes;
  for (int var = 0; var < var_to_orbit_index.size(); ++var) {
    const int rep = var_to_orbit_index[var];
    if (rep == -1) continue;
    if (rep >= orbit_sizes.size()) orbit_sizes.resize(rep + 1, 0);
    ++num_touched_vars;
    orbit_sizes[rep]++;
  }
  std::sort(orbit_sizes.begin(), orbit_sizes.end(), std::greater<int>());
  const int num_orbits = orbit_sizes.size();
  if (num_orbits > 10) orbit_sizes.resize(10);
  SOLVER_LOG(logger, "[Symmetry] ", num_orbits, " orbits on ", num_touched_vars,
             " variables with sizes: ", absl::StrJoin(orbit_sizes, ","),
             (num_orbits > orbit_sizes.size() ? ",..." : ""));
}

}  // namespace

void DetectAndAddSymmetryToProto(const SatParameters& params,
                                 CpModelProto* proto, SolverLogger* logger) {
  SymmetryProto* symmetry = proto->mutable_symmetry();
  symmetry->Clear();

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindCpModelSymmetries(params, *proto, &generators,
                        params.symmetry_detection_deterministic_time_limit(),
                        logger);
  if (generators.empty()) {
    proto->clear_symmetry();
    return;
  }

  // Log orbit information.
  //
  // TODO(user): It might be nice to just add this to the proto rather than
  // re-reading the generators and recomputing this in a few places.
  const int num_vars = proto->variables().size();
  const std::vector<int> orbits = GetOrbits(num_vars, generators);
  LogOrbitInformation(orbits, logger);

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
void OrbitAndPropagation(absl::Span<const int> orbits, int var,
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

std::vector<int64_t> BuildInequalityCoeffsForOrbitope(
    absl::Span<const int64_t> maximum_values, int64_t max_linear_size,
    bool* is_approximated) {
  std::vector<int64_t> out(maximum_values.size());
  int64_t range_product = 1;
  uint64_t greatest_coeff = 0;
  for (int i = 0; i < maximum_values.size(); ++i) {
    out[i] = range_product;
    greatest_coeff =
        std::max(greatest_coeff, static_cast<uint64_t>(maximum_values[i]));
    range_product = CapProd(range_product, 1 + maximum_values[i]);
  }

  if (range_product <= max_linear_size) {
    // The product of all ranges fit in a int64_t. This is good news, that
    // means we can interpret each row of the matrix as an integer in a
    // mixed-radix representation and impose row[i] <= row[i+1].
    *is_approximated = false;
    return out;
  }
  *is_approximated = true;

  const auto compute_approximate_coeffs =
      [max_linear_size, maximum_values](double scaling_factor,
                                        std::vector<int64_t>* coeffs) -> bool {
    int64_t max_size = 0;
    double cumulative_product_double = 1.0;
    for (int i = 0; i < maximum_values.size(); ++i) {
      const int64_t max = maximum_values[i];
      const int64_t coeff = static_cast<int64_t>(cumulative_product_double);
      (*coeffs)[i] = coeff;
      cumulative_product_double *= scaling_factor * max + 1;
      max_size = CapAdd(max_size, CapProd(max, coeff));
      if (max_size > max_linear_size) return false;
    }
    return true;
  };

  const double scaling = BinarySearch<double>(
      0.0, 1.0, [&compute_approximate_coeffs, &out](double scaling_factor) {
        return compute_approximate_coeffs(scaling_factor, &out);
      });
  CHECK(compute_approximate_coeffs(scaling, &out));
  return out;
}

void UpdateHintAfterFixingBoolToBreakSymmetry(
    PresolveContext* context, int var, bool fixed_value,
    absl::Span<const std::unique_ptr<SparsePermutation>> generators) {
  if (!context->VarHasSolutionHint(var)) {
    return;
  }
  const int64_t hinted_value = context->SolutionHint(var);
  if (hinted_value == static_cast<int64_t>(fixed_value)) {
    return;
  }

  std::vector<int> schrier_vector;
  std::vector<int> orbit;
  GetSchreierVectorAndOrbit(var, generators, &schrier_vector, &orbit);

  bool found_target = false;
  int target_var;
  for (int v : orbit) {
    if (context->VarHasSolutionHint(v) &&
        context->SolutionHint(v) == static_cast<int64_t>(fixed_value)) {
      found_target = true;
      target_var = v;
      break;
    }
  }
  if (!found_target) {
    context->UpdateRuleStats(
        "hint: couldn't transform infeasible hint properly");
    return;
  }

  const std::vector<int> generator_idx =
      TracePoint(target_var, schrier_vector, generators);
  for (const int i : generator_idx) {
    context->PermuteHintValues(*generators[i]);
  }

  DCHECK(context->VarHasSolutionHint(var));
  DCHECK_EQ(context->SolutionHint(var), fixed_value);
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
  FindCpModelSymmetries(
      params, proto, &generators,
      context->params().symmetry_detection_deterministic_time_limit(),
      context->logger());

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

  // We have a few heuristics. The first only look at the global orbits under
  // the symmetry group and try to infer Boolean variable fixing via symmetry
  // breaking. Note that nothing is fixed yet, we will decide later if we fix
  // these Booleans or not.
  int distinguished_var = -1;
  std::vector<int> can_be_fixed_to_false;

  // Get the global orbits and their size.
  const std::vector<int> orbits = GetOrbits(num_vars, generators);
  std::vector<int> orbit_sizes;
  int max_orbit_size = 0;
  int sum_of_orbit_sizes = 0;
  for (int var = 0; var < num_vars; ++var) {
    const int rep = orbits[var];
    if (rep == -1) continue;
    if (rep >= orbit_sizes.size()) orbit_sizes.resize(rep + 1, 0);
    ++sum_of_orbit_sizes;
    orbit_sizes[rep]++;
    if (orbit_sizes[rep] > max_orbit_size) {
      distinguished_var = var;
      max_orbit_size = orbit_sizes[rep];
    }
  }

  // Log orbit info.
  LogOrbitInformation(orbits, context->logger());

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
  // instead of letting the inner solver handle Boolean symmetries make the
  // problem unsolvable instead of easily solved. This is probably because this
  // fixing do not exploit the full structure of these symmetries. Note
  // however that the fixing via propagation above close cod105 even more
  // efficiently.
  std::vector<int> var_can_be_true_per_orbit(num_vars, -1);
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
          if (tmp_sizes[rep] == 0) {
            can_be_fixed_to_false.push_back(var);
          } else {
            var_can_be_true_per_orbit[rep] = var;
          }
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

  // HACK for flatzinc wordpress* problem.
  //
  // If we have a large orbitope, with one objective term by column, we break
  // the symmetry by ordering the objective terms. This usually increase
  // drastically the objective lower bounds we can discover.
  //
  // TODO(user): generalize somehow. See if we can exploit this in
  // lb_tree_search directly. We also have a lot more structure than just the
  // objective can be ordered. Like if the objective is a max, we can still do
  // that.
  //
  // TODO(user): Actually the constraint we add is really just breaking the
  // orbitope symmetry on one line. But this line being the objective is key. We
  // can also explicitly look for a full permutation group of the objective
  // terms directly instead of finding the largest orbitope first.
  if (!orbitope.empty() && context->working_model->has_objective()) {
    const int num_objective_terms = context->ObjectiveMap().size();
    if (orbitope[0].size() == num_objective_terms) {
      int num_in_column = 0;
      for (const std::vector<int>& row : orbitope) {
        if (context->ObjectiveMap().contains(row[0])) ++num_in_column;
      }
      if (num_in_column == 1) {
        context->WriteObjectiveToProto();
        const auto& obj = context->working_model->objective();
        CHECK_EQ(num_objective_terms, obj.vars().size());
        for (int i = 1; i < num_objective_terms; ++i) {
          auto* new_ct =
              context->working_model->add_constraints()->mutable_linear();
          new_ct->add_vars(obj.vars(i - 1));
          new_ct->add_vars(obj.vars(i));
          new_ct->add_coeffs(1);
          new_ct->add_coeffs(-1);
          new_ct->add_domain(0);
          new_ct->add_domain(std::numeric_limits<int64_t>::max());
        }
        context->UpdateNewConstraintsVariableUsage();
        context->UpdateRuleStats("symmetry: objective is one orbitope row.");
        return true;
      }
    }
  }

  // Super simple heuristic to use the orbitope or not.
  //
  // In an orbitope with an at most one on each row, we can fix the upper right
  // triangle. We could use a formula, but the loop is fast enough.
  //
  // TODO(user): Compute the stabilizer under the only non-fixed element and
  // iterate!
  int max_num_fixed_in_orbitope = 0;
  if (!orbitope.empty()) {
    int size_left = orbitope[0].size();
    for (int col = 0; size_left > 1 && col < orbitope.size(); ++col) {
      max_num_fixed_in_orbitope += size_left - 1;
      --size_left;
    }
  }

  // Fixing just a few variables to break large symmetry can be really bad. See
  // for example cdc7-4-3-2.pb.gz where we don't find solution if we do that. On
  // the other hand, enabling this make it worse on neos-3083784-nive.pb.gz.
  //
  // In general, enabling this works better in single thread with max_lp_sym,
  // but worse in multi-thread, where less workers are using symmetries, and so
  // it is better to fix more stuff.
  //
  // TODO(user): Tune more, especially as we handle symmetry better. Also the
  // estimate is pretty bad, we should probably compute stabilizer and decide
  // when we actually know how much we can fix compared to how many symmetry we
  // lose.
  const int num_fixable =
      std::max<int>(max_num_fixed_in_orbitope, can_be_fixed_to_false.size());
  if (/* DISABLES CODE */ (false) && !can_be_fixed_to_false.empty() &&
      100 * num_fixable < sum_of_orbit_sizes) {
    SOLVER_LOG(context->logger(),
               "[Symmetry] Not fixing anything as gain seems too small.");
    return true;
  }

  // Fix "can_be_fixed_to_false" instead of the orbitope if it is larger.
  if (max_num_fixed_in_orbitope < can_be_fixed_to_false.size()) {
    const int orbit_index = orbits[distinguished_var];
    int num_in_orbit = 0;
    for (int i = 0; i < can_be_fixed_to_false.size(); ++i) {
      const int var = can_be_fixed_to_false[i];
      if (orbits[var] == orbit_index) ++num_in_orbit;
      context->UpdateRuleStats("symmetry: fixed to false in general orbit");
      if (context->VarHasSolutionHint(var) && context->SolutionHint(var) == 1 &&
          var_can_be_true_per_orbit[orbits[var]] != -1) {
        // We are breaking the symmetry in a way that makes the hint invalid.
        // We want `var` to be false, so we would naively pick a symmetry to
        // enforce that. But that will be wrong if we do this twice: after we
        // permute the hint to fix the first one we would look for a symmetry
        // group element that fixes the second one to false. But there are many
        // of those, and picking the wrong one would risk making the first one
        // true again. Since this is a AMO, fixing the one that is true doesn't
        // have this problem.
        UpdateHintAfterFixingBoolToBreakSymmetry(
            context, var_can_be_true_per_orbit[orbits[var]], true, generators);
      }
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

  if (!orbitope.empty() && orbitope[0].size() > 1) {
    const int num_cols = orbitope[0].size();
    const std::vector<int> orbitope_orbits =
        GetOrbitopeOrbits(num_vars, orbitope);

    // Using the orbitope orbits and intersecting at most ones, we will be able
    // in some case to derive a property of the literals of one row of the
    // orbitope. Namely that:
    // - All literals of that row take the same value.
    // - At most one literal can be true.
    // - At most one literal can be false.
    //
    // See the comment below for how we can infer this.
    const int num_rows = orbitope.size();
    std::vector<bool> row_is_all_equivalent(num_rows, false);
    std::vector<bool> row_has_at_most_one_true(num_rows, false);
    std::vector<bool> row_has_at_most_one_false(num_rows, false);

    // Because in the orbitope case, we have a full symmetry group of the
    // columns, we can infer more than just using the orbits under a general
    // permutation group. If an at most one contains two variables from the
    // row, we can infer:
    // 1/ If the two variables appear positively, then there is an at most one
    //    on the full row, and we can set n - 1 variables to zero to break the
    //    symmetry.
    // 2/ If the two variables appear negatively, then the opposite situation
    //    arise and there is at most one zero on the row, we can set n - 1
    //    variables to one.
    // 3/ If two literals of opposite sign appear, then the only possibility
    //    for the row are all at one or all at zero, thus we can mark all
    //    variables as equivalent.
    //
    // These property comes from the fact that when we permute a line of the
    // orbitope in any way, then the position than ends up in the at most one
    // must never be both at one.
    //
    // Note that 3/ can be done without breaking any symmetry, but for 1/ and 2/
    // by choosing which variable is not fixed, we will break some symmetry.
    //
    // TODO(user): for 1/ and 2/ we could add an at most one constraint on the
    // full row if it is not already there!
    //
    // Note(user): On the miplib, only 1/ and 2/ happens currently. Not sure
    // with LNS though.
    for (const google::protobuf::RepeatedField<int32_t>* literals :
         at_most_ones) {
      tmp_to_clear.clear();
      for (const int literal : *literals) {
        if (context->IsFixed(literal)) continue;
        const int var = PositiveRef(literal);
        const int row = orbitope_orbits[var];
        if (row == -1) continue;

        if (tmp_sizes[row] == 0) tmp_to_clear.push_back(row);
        tmp_sizes[row]++;
        if (RefIsPositive(literal)) tmp_num_positive[row]++;
      }

      // An at most one touching two positions in an orbitope row can be
      // extended to include the full row.
      //
      // Note(user): I am not sure we care about that here. By symmetry, if we
      // have an at most one touching two positions, then we should have others
      // touching all pair of positions. And the at most one expansion would
      // already have extended it. So this is more FYI.
      bool possible_extension = false;

      // TODO(user): if the same at most one touch more than one row, we can
      // deduce more. It is a bit tricky and maybe not frequent enough to make a
      // big difference. Also, as we start to fix things, at most one might
      // propagate by themselves.
      for (const int row : tmp_to_clear) {
        const int size = tmp_sizes[row];
        const int num_positive = tmp_num_positive[row];
        const int num_negative = tmp_sizes[row] - tmp_num_positive[row];
        tmp_sizes[row] = 0;
        tmp_num_positive[row] = 0;

        if (num_positive > 0 && num_negative > 0) {
          row_is_all_equivalent[row] = true;
        }
        if (num_positive > 1 && num_negative == 0) {
          if (size < num_cols) possible_extension = true;
          row_has_at_most_one_true[row] = true;
        } else if (num_positive == 0 && num_negative > 1) {
          if (size < num_cols) possible_extension = true;
          row_has_at_most_one_false[row] = true;
        }
      }

      if (possible_extension) {
        context->UpdateRuleStats(
            "TODO symmetry: possible at most one extension.");
      }
    }

    // List the row in "at most one" by score. We will be able to fix a
    // "triangle" of literals in order to break some of the symmetry.
    std::vector<std::pair<int, int64_t>> rows_by_score;

    // Mark all the equivalence or fixed rows.
    // Note that this operation do not change the symmetry group.
    //
    // TODO(user): We could remove these rows from the orbitope. Note that
    // currently this never happen on the miplib (maybe in LNS though).
    for (int i = 0; i < num_rows; ++i) {
      if (row_has_at_most_one_true[i] && row_has_at_most_one_false[i]) {
        // If we have both property, it means we have
        // - sum_j orbitope[row][j] <= 1
        // - sum_j not(orbitope[row][j]) <= 1 which is the same as
        //   sum_j orbitope[row][j] >= num_cols - 1.
        // This is only possible if we have two elements and we don't have
        // row_is_all_equivalent.
        if (num_cols == 2 && !row_is_all_equivalent[i]) {
          // We have [1, 0] or [0, 1].
          context->UpdateRuleStats("symmetry: equivalence in orbitope row");
          context->StoreBooleanEqualityRelation(orbitope[i][0],
                                                NegatedRef(orbitope[i][1]));
          if (context->ModelIsUnsat()) return false;
        } else {
          // No solution.
          return context->NotifyThatModelIsUnsat("orbitope and at most one");
        }
        continue;
      }

      if (row_is_all_equivalent[i]) {
        // Here we proved that the row is either all ones or all zeros.
        // This was because we had:
        //   at_most_one = [x, ~y, ...]
        //   orbitope = [x, y, ...]
        // and by symmetry we have
        //   at_most_one = [~x, y, ...]
        // This for all pairs of positions in that row.
        if (row_has_at_most_one_false[i]) {
          context->UpdateRuleStats("symmetry: all true in orbitope row");
          for (int j = 0; j < num_cols; ++j) {
            if (!context->SetLiteralToTrue(orbitope[i][j])) return false;
          }
        } else if (row_has_at_most_one_true[i]) {
          context->UpdateRuleStats("symmetry: all false in orbitope row");
          for (int j = 0; j < num_cols; ++j) {
            if (!context->SetLiteralToFalse(orbitope[i][j])) return false;
          }
        } else {
          context->UpdateRuleStats("symmetry: all equivalent in orbitope row");
          for (int j = 1; j < num_cols; ++j) {
            context->StoreBooleanEqualityRelation(orbitope[i][0],
                                                  orbitope[i][j]);
            if (context->ModelIsUnsat()) return false;
          }
        }
        continue;
      }

      // We use as the score the number of constraint in which variables from
      // this row participate.
      const int64_t score =
          context->VarToConstraints(PositiveRef(orbitope[i][0])).size();
      if (row_has_at_most_one_true[i]) {
        rows_by_score.push_back({i, score});
      } else if (row_has_at_most_one_false[i]) {
        rows_by_score.push_back({i, score});
      }
    }

    // Break the symmetry by fixing at each step all but one literal to true or
    // false. Note that each time we do that for a row, we need to exclude the
    // non-fixed column from the rest of the row processing. We thus fix a
    // "triangle" of literals.
    //
    // This is the same as ordering the columns in some lexicographic order and
    // using the at_most_ones to fix known position. Note that we can still add
    // lexicographic symmetry breaking inequality on the columns as long as we
    // do that in the same order as these fixing.
    absl::c_stable_sort(rows_by_score, [](const std::pair<int, int64_t>& p1,
                                          const std::pair<int, int64_t>& p2) {
      return p1.second > p2.second;
    });
    int num_processed_rows = 0;
    for (const auto [row, score] : rows_by_score) {
      if (num_processed_rows + 1 >= num_cols) break;
      ++num_processed_rows;
      if (row_has_at_most_one_true[row]) {
        context->UpdateRuleStats(
            "symmetry: fixed all but one to false in orbitope row");
        for (int j = num_processed_rows; j < num_cols; ++j) {
          if (!context->SetLiteralToFalse(orbitope[row][j])) return false;
        }
      } else {
        CHECK(row_has_at_most_one_false[row]);
        context->UpdateRuleStats(
            "symmetry: fixed all but one to true in orbitope row");
        for (int j = num_processed_rows; j < num_cols; ++j) {
          if (!context->SetLiteralToTrue(orbitope[row][j])) return false;
        }
      }
    }

    // For correctness of the code below, reduce the orbitope.
    //
    // TODO(user): This is probably not needed if we add lexicographic
    // constraint instead of just breaking a single row below.
    if (num_processed_rows > 0) {
      // Remove the first num_processed_rows.
      int new_size = 0;
      for (int i = num_processed_rows; i < orbitope.size(); ++i) {
        orbitope[new_size++] = std::move(orbitope[i]);
      }
      CHECK_LT(new_size, orbitope.size());
      orbitope.resize(new_size);

      // For each of them remove the first num_processed_rows entries.
      for (int i = 0; i < orbitope.size(); ++i) {
        CHECK_LT(num_processed_rows, orbitope[i].size());
        orbitope[i].erase(orbitope[i].begin(),
                          orbitope[i].begin() + num_processed_rows);
      }
    }
  }

  // The transformations below seems to hurt more than what they help.
  // Especially when we handle symmetry during the search like with max_lp_sym
  // worker. See for instance neos-948346.pb or map06.pb.gz.
  if (params.symmetry_level() <= 3) return true;

  // If we are left with a set of variable than can all be permuted, lets
  // break the symmetry by ordering them.
  if (orbitope.size() == 1) {
    const int num_cols = orbitope[0].size();
    for (int i = 0; i + 1 < num_cols; ++i) {
      // Add orbitope[0][i] >= orbitope[0][i+1].
      if (context->CanBeUsedAsLiteral(orbitope[0][i]) &&
          context->CanBeUsedAsLiteral(orbitope[0][i + 1])) {
        context->AddImplication(orbitope[0][i + 1], orbitope[0][i]);
        context->UpdateRuleStats(
            "symmetry: added symmetry breaking implication");
        continue;
      }

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
  } else if (orbitope.size() > 1) {
    std::vector<int64_t> max_values(orbitope.size());
    for (int i = 0; i < orbitope.size(); ++i) {
      const int var = orbitope[i][0];
      const int64_t max = std::max(std::abs(context->MaxOf(var)),
                                   std::abs(context->MinOf(var)));
      max_values[i] = max;
    }
    constexpr int kMaxBits = 60;
    bool is_approximated;
    const std::vector<int64_t> coeffs = BuildInequalityCoeffsForOrbitope(
        max_values, (int64_t{1} << kMaxBits), &is_approximated);
    for (int i = 0; i + 1 < orbitope[0].size(); ++i) {
      ConstraintProto* ct = context->working_model->add_constraints();
      auto* arg = ct->mutable_linear();
      for (int j = 0; j < orbitope.size(); ++j) {
        const int64_t coeff = coeffs[j];
        arg->add_vars(orbitope[j][i + 1]);
        arg->add_coeffs(coeff);
        arg->add_vars(orbitope[j][i]);
        arg->add_coeffs(-coeff);
        DCHECK_EQ(context->MaxOf(orbitope[j][i + 1]),
                  context->MaxOf(orbitope[j][i]));
        DCHECK_EQ(context->MinOf(orbitope[j][i + 1]),
                  context->MinOf(orbitope[j][i]));
      }
      arg->add_domain(0);
      arg->add_domain(std::numeric_limits<int64_t>::max());
      DCHECK(!PossibleIntegerOverflow(*context->working_model, arg->vars(),
                                      arg->coeffs()));
    }
    context->UpdateRuleStats(
        absl::StrCat("symmetry: added linear ",
                     is_approximated ? "approximated " : "",
                     "inequality ordering orbitope columns"),
        orbitope[0].size());
    context->UpdateNewConstraintsVariableUsage();
    return true;
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
