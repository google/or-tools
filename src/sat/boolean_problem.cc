// Copyright 2010-2013 Google
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
#include "sat/boolean_problem.h"

#include "base/hash.h"

#include "base/join.h"
#include "base/map_util.h"
#include "base/hash.h"
#include "algorithms/find_graph_symmetries.h"

namespace operations_research {
namespace sat {

bool LoadBooleanProblem(const LinearBooleanProblem& problem,
                        SatSolver* solver) {
  if (solver->parameters().log_search_progress()) {
    LOG(INFO) << "Loading problem '" << problem.name() << "', "
              << problem.num_variables() << " variables, "
              << problem.constraints_size() << " constraints.";
  }
  solver->SetNumVariables(problem.num_variables());
  std::vector<LiteralWithCoeff> cst;
  int64 num_terms = 0;
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    cst.clear();
    num_terms += constraint.literals_size();
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const Literal literal(constraint.literals(i));
      if (literal.Variable() >= problem.num_variables()) {
        LOG(WARNING) << "Literal out of bound: " << literal;
        return false;
      }
      cst.push_back(LiteralWithCoeff(literal, constraint.coefficients(i)));
    }
    if (!solver->AddLinearConstraint(
            constraint.has_lower_bound(), Coefficient(constraint.lower_bound()),
            constraint.has_upper_bound(), Coefficient(constraint.upper_bound()),
            &cst)) {
      return false;
    }
  }
  if (solver->parameters().log_search_progress()) {
    LOG(INFO) << "The problem contains " << num_terms << " terms.";
  }
  return true;
}

void UseObjectiveForSatAssignmentPreference(const LinearBooleanProblem& problem,
                                            SatSolver* solver) {
  // Initialize the heuristic to look for a good solution.
  if (problem.type() == LinearBooleanProblem::MINIMIZATION ||
      problem.type() == LinearBooleanProblem::MAXIMIZATION) {
    const int sign =
        (problem.type() == LinearBooleanProblem::MAXIMIZATION) ? -1 : 1;
    const LinearObjective& objective = problem.objective();
    double max_weight = 0;
    for (int i = 0; i < objective.literals_size(); ++i) {
      max_weight =
          std::max(max_weight, fabs(static_cast<double>(objective.coefficients(i))));
    }
    for (int i = 0; i < objective.literals_size(); ++i) {
      const double weight =
          fabs(static_cast<double>(objective.coefficients(i))) / max_weight;
      if (sign * objective.coefficients(i) > 0) {
        solver->SetAssignmentPreference(
            Literal(objective.literals(i)).Negated(), weight);
      } else {
        solver->SetAssignmentPreference(Literal(objective.literals(i)), weight);
      }
    }
  }
}

bool AddObjectiveConstraint(const LinearBooleanProblem& problem,
                            bool use_lower_bound, Coefficient lower_bound,
                            bool use_upper_bound, Coefficient upper_bound,
                            SatSolver* solver) {
  if (problem.type() != LinearBooleanProblem::MINIMIZATION &&
      problem.type() != LinearBooleanProblem::MAXIMIZATION) {
    return true;
  }
  std::vector<LiteralWithCoeff> cst;
  const LinearObjective& objective = problem.objective();
  for (int i = 0; i < objective.literals_size(); ++i) {
    const Literal literal(objective.literals(i));
    if (literal.Variable() >= problem.num_variables()) {
      LOG(WARNING) << "Literal out of bound: " << literal;
      return false;
    }
    cst.push_back(LiteralWithCoeff(literal, objective.coefficients(i)));
  }
  return solver->AddLinearConstraint(use_lower_bound, lower_bound,
                                     use_upper_bound, upper_bound, &cst);
}

Coefficient ComputeObjectiveValue(const LinearBooleanProblem& problem,
                                  const VariablesAssignment& assignment) {
  Coefficient sum(0);
  const LinearObjective& objective = problem.objective();
  for (int i = 0; i < objective.literals_size(); ++i) {
    if (assignment.IsLiteralTrue(objective.literals(i))) {
      sum += objective.coefficients(i);
    }
  }
  return sum;
}

bool IsAssignmentValid(const LinearBooleanProblem& problem,
                       const VariablesAssignment& assignment) {
  // Check that all variables are assigned.
  for (int i = 0; i < problem.num_variables(); ++i) {
    if (!assignment.IsVariableAssigned(VariableIndex(i))) {
      LOG(WARNING) << "Assignment is not complete";
      return false;
    }
  }

  // Check that all constraints are satisfied.
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    Coefficient sum(0);
    for (int i = 0; i < constraint.literals_size(); ++i) {
      if (assignment.IsLiteralTrue(constraint.literals(i))) {
        sum += constraint.coefficients(i);
      }
    }
    if (constraint.has_lower_bound() && constraint.has_upper_bound()) {
      if (sum < constraint.lower_bound() || sum > constraint.upper_bound()) {
        LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                     << constraint.DebugString();
        return false;
      }
    } else if (constraint.has_lower_bound()) {
      if (sum < constraint.lower_bound()) {
        LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                     << constraint.DebugString();
        return false;
      }
    } else if (constraint.has_upper_bound()) {
      if (sum > constraint.upper_bound()) {
        LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                     << constraint.DebugString();
        return false;
      }
    }
  }

  // We also display the objective value of an optimization problem.
  if (problem.type() == LinearBooleanProblem::MINIMIZATION ||
      problem.type() == LinearBooleanProblem::MAXIMIZATION) {
    Coefficient sum(0);
    Coefficient min_value(0);
    Coefficient max_value(0);
    const LinearObjective& objective = problem.objective();
    for (int i = 0; i < objective.literals_size(); ++i) {
      if (objective.coefficients(i) > 0) {
        max_value += objective.coefficients(i);
      } else {
        min_value += objective.coefficients(i);
      }
      if (assignment.IsLiteralTrue(objective.literals(i))) {
        sum += objective.coefficients(i);
      }
    }
    LOG(INFO) << "objective: " << sum.value() * objective.scaling_factor() +
                                      objective.offset() << " trivial_bounds: ["
              << min_value.value() * objective.scaling_factor() +
                     objective.offset() << ", "
              << max_value.value() * objective.scaling_factor() +
                     objective.offset() << "]";
  }
  return true;
}

// Note(user): This function make a few assumption for a max-sat or weighted
// max-sat problem. Namely that the slack variable from the constraints are
// always in first position, and appears in the same order in the objective and
// in the constraints.
std::string LinearBooleanProblemToCnfString(const LinearBooleanProblem& problem) {
  std::string output;
  const bool is_wcnf = (problem.type() == LinearBooleanProblem::MINIMIZATION);
  const LinearObjective& objective = problem.objective();

  int64 hard_weigth = 0;
  if (is_wcnf) {
    for (const int64 weight : objective.coefficients()) {
      hard_weigth = std::max(hard_weigth, weight + 1);
    }
    CHECK_GT(hard_weigth, 0);
    output +=
        StringPrintf("p wcnf %d %d %lld\n",
                     problem.num_variables() - objective.literals().size(),
                     problem.constraints_size(), hard_weigth);
  } else {
    output += StringPrintf("p cnf %d %d\n", problem.num_variables(),
                           problem.constraints_size());
  }

  int slack_index = 0;
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    if (constraint.literals_size() == 0) return "";
    for (int i = 0; i < constraint.literals_size(); ++i) {
      if (constraint.coefficients(i) != 1) return "";
      if (i == 0 && is_wcnf) {
        if (slack_index < objective.literals_size() &&
            constraint.literals(0) == objective.literals(slack_index)) {
          output += StringPrintf("%lld ", objective.coefficients(slack_index));
          ++slack_index;
          continue;
        } else {
          output += StringPrintf("%lld ", hard_weigth);
        }
      }
      output += Literal(constraint.literals(i)).DebugString();
      output += " ";
    }
    output += "0\n";
  }
  return output;
}

void StoreAssignment(const VariablesAssignment& assignment,
                     BooleanAssignment* output) {
  output->clear_literals();
  for (VariableIndex var(0); var < assignment.NumberOfVariables(); ++var) {
    if (assignment.IsVariableAssigned(var)) {
      output->add_literals(
          assignment.GetTrueLiteralForAssignedVariable(var).SignedValue());
    }
  }
}

void ExtractSubproblem(const LinearBooleanProblem& problem,
                       const std::vector<int>& constraint_indices,
                       LinearBooleanProblem* subproblem) {
  subproblem->CopyFrom(problem);
  subproblem->set_name("Subproblem of " + problem.name());
  subproblem->clear_constraints();
  for (int index : constraint_indices) {
    CHECK_LT(index, problem.constraints_size());
    subproblem->add_constraints()->MergeFrom(problem.constraints(index));
  }
}

namespace {
// A simple class to generate equivalence class number for
// GenerateGraphForSymmetryDetection().
class IdGenerator {
 public:
  IdGenerator() {}

  // If the pair (type, coefficient) was never seen before, then generate
  // a new id, otherwise return the previously generated id.
  int GetId(int type, Coefficient coefficient) {
    const std::pair<int, int64> key(type, coefficient.value());
    return LookupOrInsert(&id_map_, key, id_map_.size());
  }

 private:
#if defined(_MSC_VER)
  hash_map<std::pair<int, int64>, int, PairIntInt64Hasher> id_map_;
#else
  hash_map<std::pair<int, int64>, int> id_map_;
#endif
};
}  // namespace.

// Returns a graph whose automorphisms can be mapped back to the symmetries of
// the given LinearBooleanProblem.
//
// Any permutation of the graph that respects the initial_equivalence_classes
// output can be mapped to a symmetry of the given problem simply by taking its
// restriction on the first 2 * num_variables nodes and interpreting its index
// as a literal index. In a sense, a node with a low enough index #i is in
// one-to-one correspondance with a literals #i (using the index representation
// of literal).
//
// The format of the initial_equivalence_classes is the same as the one
// described in GraphSymmetryFinder::FindSymmetries(). The classes must be dense
// in [0, num_classes) and any symmetry will only map nodes with the same class
// between each other.
template <typename Graph>
Graph* GenerateGraphForSymmetryDetection(
    const LinearBooleanProblem& problem,
    std::vector<int>* initial_equivalence_classes) {
  // First, we convert the problem to its canonical representation.
  const int num_variables = problem.num_variables();
  CanonicalBooleanLinearProblem canonical_problem;
  std::vector<LiteralWithCoeff> cst;
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    cst.clear();
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const Literal literal(constraint.literals(i));
      cst.push_back(LiteralWithCoeff(literal, constraint.coefficients(i)));
    }
    CHECK(canonical_problem.AddLinearConstraint(
        constraint.has_lower_bound(), Coefficient(constraint.lower_bound()),
        constraint.has_upper_bound(), Coefficient(constraint.upper_bound()),
        &cst));
  }

  // TODO(user): reserve the memory for the graph? not sure it is worthwhile
  // since it would require some linear scan of the problem though.
  Graph* graph = new Graph();
  initial_equivalence_classes->clear();

  // We will construct a graph with 3 different types of node that must be
  // in different equivalent classes.
  enum NodeType { LITERAL_NODE, CONSTRAINT_NODE, CONSTRAINT_COEFFICIENT_NODE };
  IdGenerator id_generator;

  // First, we need one node per literal with an edge between each literal
  // and its negation.
  for (int i = 0; i < num_variables; ++i) {
    // We have two nodes for each variable.
    // Note that the indices are in [0, 2 * num_variables) and in one to one
    // correspondance with the index representation of a literal.
    const Literal literal = Literal(VariableIndex(i), true);
    graph->AddArc(literal.Index().value(), literal.NegatedIndex().value());
    graph->AddArc(literal.NegatedIndex().value(), literal.Index().value());
  }

  // We use 0 for their initial equivalence class, but that may be modified
  // with the objective coefficient (see below).
  initial_equivalence_classes->assign(
      2 * num_variables,
      id_generator.GetId(NodeType::LITERAL_NODE, Coefficient(0)));

  // Literals with different objective coeffs shouldn't be in the same class.
  if (problem.type() == LinearBooleanProblem::MINIMIZATION ||
      problem.type() == LinearBooleanProblem::MAXIMIZATION) {
    // We need to canonicalize the objective to regroup literals corresponding
    // to the same variables.
    std::vector<LiteralWithCoeff> expr;
    const LinearObjective& objective = problem.objective();
    for (int i = 0; i < objective.literals_size(); ++i) {
      const Literal literal(objective.literals(i));
      expr.push_back(LiteralWithCoeff(literal, objective.coefficients(i)));
    }
    // Note that we don't care about the offset or optimization direction here,
    // we just care about literals with the same canonical coefficient.
    Coefficient shift;
    Coefficient max_value;
    ComputeBooleanLinearExpressionCanonicalForm(&expr, &shift, &max_value);
    for (LiteralWithCoeff term : expr) {
      (*initial_equivalence_classes)[term.literal.Index().value()] =
          id_generator.GetId(NodeType::LITERAL_NODE, term.coefficient);
    }
  }

  // Then, for each constraint, we will have one or more nodes.
  for (int i = 0; i < canonical_problem.NumConstraints(); ++i) {
    // First we have a node for the constraint with an equivalence class
    // depending on the rhs.
    //
    // Note: Since we add nodes one by one, initial_equivalence_classes->size()
    // gives the number of nodes at any point, which we use as next node index.
    const int constraint_node_index = initial_equivalence_classes->size();
    initial_equivalence_classes->push_back(id_generator.GetId(
        NodeType::CONSTRAINT_NODE, canonical_problem.Rhs(i)));

    // This node will also be connected to all literals of the constraint
    // with a coefficient of 1. Literals with new coefficients will be grouped
    // under a new node connected to the constraint_node_index.
    //
    // Note that this works because a canonical constraint is sorted by
    // increasing coefficient value (all positive).
    int current_node_index = constraint_node_index;
    Coefficient previous_coefficient(1);
    for (LiteralWithCoeff term : canonical_problem.Constraint(i)) {
      if (term.coefficient != previous_coefficient) {
        current_node_index = initial_equivalence_classes->size();
        initial_equivalence_classes->push_back(id_generator.GetId(
            NodeType::CONSTRAINT_COEFFICIENT_NODE, term.coefficient));
        previous_coefficient = term.coefficient;

        // Connect this node to the constraint node. Note that we don't
        // technically need the arcs in both directions, but that may help a bit
        // the algorithm to find symmetries.
        graph->AddArc(constraint_node_index, current_node_index);
        graph->AddArc(current_node_index, constraint_node_index);
      }

      // Connect this node to the associated term.literal node. Note that we
      // don't technically need the arcs in both directions, but that may help a
      // bit the algorithm to find symmetries.
      graph->AddArc(current_node_index, term.literal.Index().value());
      graph->AddArc(term.literal.Index().value(), current_node_index);
    }
  }
  graph->Build();
  DCHECK_EQ(graph->num_nodes(), initial_equivalence_classes->size());
  return graph;
}

void FindLinearBooleanProblemSymmetries(
    const LinearBooleanProblem& problem,
    std::vector<std::unique_ptr<SparsePermutation>>* generators) {
  std::vector<int> equivalence_classes;
  std::unique_ptr<GraphSymmetryFinder::Graph> graph(
      GenerateGraphForSymmetryDetection<GraphSymmetryFinder::Graph>(
          problem, &equivalence_classes));
  LOG(INFO) << "Graph has " << graph->num_nodes() << " nodes and "
            << graph->num_arcs() / 2 << " edges.";
  GraphSymmetryFinder symmetry_finder(*graph.get());
  std::vector<int> factorized_automorphism_group_size;
  symmetry_finder.FindSymmetries(&equivalence_classes, generators,
                                 &factorized_automorphism_group_size);

  // Remove from the permutations the part not concerning the literals.
  // Note that some permutation may becomes empty, which means that we had
  // duplicates constraints. TODO(user): Remove them beforehand?
  double average_support_size = 0.0;
  int num_generators = 0;
  for (int i = 0; i < generators->size(); ++i) {
    SparsePermutation* permutation = (*generators)[i].get();
    std::vector<int> to_delete;
    for (int j = 0; j < permutation->NumCycles(); ++j) {
      if (*(permutation->Cycle(j).begin()) >= 2 * problem.num_variables()) {
        to_delete.push_back(j);
        if (DEBUG_MODE) {
          // Verify that the cycle's entire support does not touch any variable.
          for (const int node : permutation->Cycle(j)) {
            DCHECK_GE(node, 2 * problem.num_variables());
          }
        }
      }
    }
    permutation->RemoveCycles(to_delete);
    if (!permutation->Support().empty()) {
      average_support_size += permutation->Support().size();
      swap((*generators)[num_generators], (*generators)[i]);
      ++num_generators;
    }
  }
  generators->resize(num_generators);
  average_support_size /= num_generators;
  LOG(INFO) << "# of generators: " << num_generators;
  LOG(INFO) << "Average support size: " << average_support_size;
}

}  // namespace sat
}  // namespace operations_research
