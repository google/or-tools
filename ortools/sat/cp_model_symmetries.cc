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
#include "ortools/sat/symmetry_util.h"

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

  int NextFreeId() const { return id_map_.size(); }

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
    tmp_key = {VARIABLE_NODE, objective_by_var[v]};
    Append(problem.variables(v).domain(), &tmp_key);

    // Note that the code rely on the fact that the index of a VARIABLE_NODE
    // type is the same as the variable index.
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
      case ConstraintProto::CONSTRAINT_NOT_SET:
        break;
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

  // Because this code is running during presolve, a lot a variable might have
  // no edges. We do not want to detect symmetries between these.
  //
  // Note that this code forces us to "densify" the ids aftewards because the
  // symmetry detection code relies on that.
  //
  // TODO(user): It will probably be more efficient to not even create these
  // nodes, but we will need a mapping to know the variable <-> node index.
  int next_id = id_generator.NextFreeId();
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
    LOG(INFO) << "Graph for symmetry has " << graph->num_nodes()
              << " nodes and " << graph->num_arcs() / 2 << " edges.";
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
  if (log_info && num_generators > 0) {
    LOG(INFO) << "# of generators: " << num_generators;
    LOG(INFO) << "Average support size: " << average_support_size;
    if (num_duplicate_constraints > 0) {
      LOG(INFO) << "The model contains " << num_duplicate_constraints
                << " duplicate constraints !";
    }
  }
}

bool DetectAndExploitSymmetriesInPresolve(PresolveContext* context) {
  const SatParameters& params = context->params();
  const bool log_info = params.log_search_progress() || VLOG_IS_ON(1);
  const CpModelProto& proto = *context->working_model;

  // We need to make sure the proto is up to date before computing symmetries!
  context->WriteObjectiveToProto();
  const int num_vars = proto.variables_size();
  for (int i = 0; i < num_vars; ++i) {
    FillDomainInProto(context->DomainOf(i),
                      context->working_model->mutable_variables(i));
  }

  // Tricky: the equivalence relation are not part of the proto.
  // We thus add them temporarily to compute the symmetry.
  int64 num_added = 0;
  const int initial_ct_index = proto.constraints().size();
  for (int var = 0; var < num_vars; ++var) {
    if (context->IsFixed(var)) continue;
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
  FindCpModelSymmetries(params, proto, &generators, /*time_limit_seconds=*/1.0);

  // Remove temporary affine relation.
  context->working_model->mutable_constraints()->DeleteSubrange(
      initial_ct_index, num_added);

  if (generators.empty()) return true;

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
  if (orbitope.empty()) return true;

  if (log_info) {
    LOG(INFO) << "Found orbitope of size " << orbitope.size() << " x "
              << orbitope[0].size();
  }

  // Collect the at most ones.
  //
  // Note(user): This relies on the fact that the pointer remain stable when
  // we adds new constraints. It should be the case, but it is a bit unsafe.
  // On the other hand it is annoying to deal with both cases below.
  std::vector<const google::protobuf::RepeatedField<int32>*> at_most_ones;
  for (int i = 0; i < proto.constraints_size(); ++i) {
    if (proto.constraints(i).constraint_case() == ConstraintProto::kAtMostOne) {
      at_most_ones.push_back(&proto.constraints(i).at_most_one().literals());
    }
    if (proto.constraints(i).constraint_case() ==
        ConstraintProto::kExactlyOne) {
      at_most_ones.push_back(&proto.constraints(i).exactly_one().literals());
    }
  }

  // This will always be kept all zero after usage.
  std::vector<int> tmp_to_clear;
  std::vector<int> tmp_sizes(num_vars, 0);
  std::vector<int> tmp_num_positive(num_vars, 0);

  // TODO(user): The code below requires that no variable appears twice in the
  // same at most one. In particular lit and not(lit) cannot appear in the same
  // at most one.
  for (const google::protobuf::RepeatedField<int32>* literals : at_most_ones) {
    for (const int lit : *literals) {
      const int var = PositiveRef(lit);
      CHECK_NE(tmp_sizes[var], 1);
      tmp_sizes[var] = 1;
    }
    for (const int lit : *literals) {
      tmp_sizes[PositiveRef(lit)] = 0;
    }
  }

  while (!orbitope.empty() && !orbitope[0].empty()) {
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
    std::vector<bool> at_most_one_one(orbitope.size(), false);
    std::vector<bool> at_most_one_zero(orbitope.size(), false);

    for (const google::protobuf::RepeatedField<int32>* literals :
         at_most_ones) {
      tmp_to_clear.clear();
      int num_in_intersections = 0;
      for (const int literal : *literals) {
        if (context->IsFixed(literal)) continue;
        const int var = PositiveRef(literal);
        const int rep = orbits[var];
        if (rep == -1) continue;

        ++num_in_intersections;
        if (tmp_sizes[rep] == 0) tmp_to_clear.push_back(rep);
        tmp_sizes[rep]++;
        if (RefIsPositive(literal)) tmp_num_positive[rep]++;
      }

      for (const int row : tmp_to_clear) {
        const int size = tmp_sizes[row];
        const int num_positive = tmp_num_positive[row];
        const int num_negative = tmp_sizes[row] - tmp_num_positive[row];
        tmp_sizes[row] = 0;
        tmp_num_positive[row] = 0;

        if (num_positive > 1 && num_negative == 0) {
          at_most_one_one[row] = true;
        } else if (num_positive == 0 && num_negative > 1) {
          at_most_one_zero[row] = true;
        } else if (num_positive > 0 && num_negative > 0) {
          all_equivalent_rows[row] = true;
        }

        // We might be able to presolve more in these cases.
        if (at_most_one_zero[row] || at_most_one_one[row] ||
            all_equivalent_rows[row]) {
          if (tmp_to_clear.size() > 1) {
            context->UpdateRuleStats(
                "TODO symmetry: at most one across orbits.");
          } else if (size < orbitope[0].size()) {
            context->UpdateRuleStats(
                "TODO symmetry: at most one can be extended");
          }
        }
      }
    }

    // Heuristically choose a "best" row/col to "distinguish" and break the
    // symmetry on.
    int best_row = 0;
    int best_col = 0;
    int best_score = 0;
    bool fix_others_to_zero = true;
    for (int i = 0; i < all_equivalent_rows.size(); ++i) {
      const int num_cols = orbitope[i].size();

      // Note that this operation do not change the symmetry group.
      if (all_equivalent_rows[i]) {
        for (int j = 1; j < num_cols; ++j) {
          context->StoreBooleanEqualityRelation(orbitope[i][0], orbitope[i][j]);
          context->UpdateRuleStats("symmetry: all equivalent in orbit");
          if (context->ModelIsUnsat()) return false;
        }
      }

      // Because of symmetry, the choice of the column shouldn't matter (they
      // will all appear in the same number of constraints of the same types),
      // however we prefer to fix a variable that seems to touch more
      // constraints.
      //
      // TODO(user): maybe we should simplify the constraint using the variable
      // we fix before choosing the next row to break symmetry on.
      const int row_score =
          context->VarToConstraints(PositiveRef(orbitope[i][0])).size();

      // TODO(user): If one variable make the line already fixed, we should just
      // ignore this row. Not too important as actually this shouldn't happen
      // because we never compute symmetries involving fixed variables. But in
      // the future, fixing some literal might have some side effects and fix
      // others.
      if (at_most_one_one[i] && row_score > best_score) {
        best_col = 0;
        for (int j = 0; j < num_cols; ++j) {
          if (context->LiteralIsTrue(orbitope[i][j])) {
            best_col = j;
            break;
          }
        }
        best_row = i;
        best_score = row_score;
        fix_others_to_zero = true;
      }
      if (at_most_one_zero[i] && row_score > best_score) {
        best_col = 0;
        for (int j = 0; j < num_cols; ++j) {
          if (context->LiteralIsFalse(orbitope[i][j])) {
            best_col = j;
            break;
          }
        }
        best_row = i;
        best_score = row_score;
        fix_others_to_zero = false;
      }
    }

    if (best_score == 0) break;
    for (int j = 1; j < orbitope[best_row].size(); ++j) {
      if (fix_others_to_zero) {
        context->UpdateRuleStats("symmetry: fixed to false");
        if (!context->SetLiteralToFalse(orbitope[best_row][j])) return false;
      } else {
        context->UpdateRuleStats("symmetry: fixed to true");
        if (!context->SetLiteralToTrue(orbitope[best_row][j])) return false;
      }
    }

    // We add the symmetry breaking inequalities: best_var >= all other var
    // in orbit. That is not(best_var) => not(other) for Booleans. We only add
    // them if we didn't fix any variable just above.
    //
    // TODO(user): Add the inequality for non-Boolean too? Also note that this
    // code only run if the code above is disabled. It is here for testing
    // alternatives. In particular, if there is no at most one, we cannot fix
    // n-1 variables, but we can still add inequalities.
    const int best_var = orbitope[best_row][best_col];
    const bool maximize_best_var = fix_others_to_zero;
    if (context->CanBeUsedAsLiteral(best_var) && !context->IsFixed(best_var)) {
      ConstraintProto* ct = context->working_model->add_constraints();
      ct->add_enforcement_literal(maximize_best_var ? NegatedRef(best_var)
                                                    : best_var);
      for (const int other : orbitope[best_row]) {
        if (other == best_var) continue;
        if (context->IsFixed(other)) continue;
        ct->mutable_bool_and()->add_literals(
            maximize_best_var ? NegatedRef(other) : other);
        context->UpdateRuleStats("symmetry: added implication");
      }
      context->UpdateNewConstraintsVariableUsage();
    }

    // Remove the column of best_var.
    CHECK_NE(best_col, -1);
    for (int i = 0; i < orbitope.size(); ++i) {
      std::swap(orbitope[i][best_col], orbitope[i].back());
      orbitope[i].pop_back();
    }

    // We also remove the line of best_var since heuristicially, it is better to
    // not add symmetries involving any of the variable on this line.
    std::swap(orbitope[best_row], orbitope.back());
    orbitope.pop_back();
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
