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

#include "ortools/sat/boolean_problem.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <limits>
#include <numeric>
#include <utility>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/graph/io.h"
#include "ortools/graph/util.h"
#include "ortools/base/int_type.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/algorithms/find_graph_symmetries.h"
#include "ortools/sat/sat_parameters.pb.h"

DEFINE_string(debug_dump_symmetry_graph_to_file, "",
              "If this flag is non-empty, an undirected graph whose"
              " automorphism group is in one-to-one correspondence with the"
              " symmetries of the SAT problem will be dumped to a file every"
              " time FindLinearBooleanProblemSymmetries() is called.");

namespace operations_research {
namespace sat {

using util::RemapGraph;

void ExtractAssignment(const LinearBooleanProblem& problem,
                       const SatSolver& solver, std::vector<bool>* assignemnt) {
  assignemnt->clear();
  for (int i = 0; i < problem.num_variables(); ++i) {
    assignemnt->push_back(
        solver.Assignment().LiteralIsTrue(Literal(BooleanVariable(i), true)));
  }
}

namespace {

// Used by BooleanProblemIsValid() to test that there is no duplicate literals,
// that they are all within range and that there is no zero coefficient.
//
// A non-empty std::string indicates an error.
template <typename LinearTerms>
std::string ValidateLinearTerms(const LinearTerms& terms,
                           std::vector<bool>* variable_seen) {
  // variable_seen already has all items false and is reset before return.
  std::string err_str;
  int num_errs = 0;
  const int max_num_errs = 100;
  for (int i = 0; i < terms.literals_size(); ++i) {
    if (terms.literals(i) == 0) {
      if (++num_errs <= max_num_errs) {
        err_str += StringPrintf("Zero literal at position %d\n", i);
      }
    }
    if (terms.coefficients(i) == 0) {
      if (++num_errs <= max_num_errs) {
        err_str += StringPrintf("Literal %d has a zero coefficient\n",
                                terms.literals(i));
      }
    }
    const int var = Literal(terms.literals(i)).Variable().value();
    if (var >= variable_seen->size()) {
      if (++num_errs <= max_num_errs) {
        err_str += StringPrintf("Out of bound variable %d\n", var);
      }
    }
    if ((*variable_seen)[var]) {
      if (++num_errs <= max_num_errs) {
        err_str += StringPrintf("Duplicated variable %d\n", var);
      }
    }
    (*variable_seen)[var] = true;
  }

  for (int i = 0; i < terms.literals_size(); ++i) {
    const int var = Literal(terms.literals(i)).Variable().value();
    (*variable_seen)[var] = false;
  }
  if (num_errs) {
    if (num_errs <= max_num_errs) {
      err_str = StringPrintf("%d validation errors:\n", num_errs) + err_str;
    } else {
      err_str = StringPrintf("%d validation errors; here are the first %d:\n",
                             num_errs, max_num_errs) + err_str;
    }
  }
  return err_str;
}

// Converts a linear expression from the protocol buffer format to a vector
// of LiteralWithCoeff.
template <typename ProtoFormat>
std::vector<LiteralWithCoeff> ConvertLinearExpression(
    const ProtoFormat& input) {
  std::vector<LiteralWithCoeff> cst;
  cst.reserve(input.literals_size());
  for (int i = 0; i < input.literals_size(); ++i) {
    const Literal literal(input.literals(i));
    cst.push_back(LiteralWithCoeff(literal, input.coefficients(i)));
  }
  return cst;
}

}  // namespace

util::Status ValidateBooleanProblem(const LinearBooleanProblem& problem) {
  std::vector<bool> variable_seen(problem.num_variables(), false);
  for (int i = 0; i < problem.constraints_size(); ++i) {
    const LinearBooleanConstraint& constraint = problem.constraints(i);
    const std::string error = ValidateLinearTerms(constraint, &variable_seen);
    if (!error.empty()) {
      return util::Status(util::error::INVALID_ARGUMENT,
                          StringPrintf("Invalid constraint %i: ", i) + error);
    }
  }
  const std::string error = ValidateLinearTerms(problem.objective(), &variable_seen);
  if (!error.empty()) {
    return util::Status(util::error::INVALID_ARGUMENT,
                        StringPrintf("Invalid objective: ") + error);
  }
  return util::Status::OK;
}

CpModelProto BooleanProblemToCpModelproto(const LinearBooleanProblem& problem) {
  CpModelProto result;
  for (int i = 0; i < problem.num_variables(); ++i) {
    IntegerVariableProto* var = result.add_variables();
    if (problem.var_names_size() > i) {
      var->set_name(problem.var_names(i));
    }
    var->add_domain(0);
    var->add_domain(1);
  }
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    ConstraintProto* ct = result.add_constraints();
    ct->set_name(constraint.name());
    LinearConstraintProto* linear = ct->mutable_linear();
    int64 offset = 0;
    for (int i = 0; i < constraint.literals_size(); ++i) {
      // Note that the new format is slightly different.
      const int lit = constraint.literals(i);
      const int64 coeff = constraint.coefficients(i);
      if (lit > 0) {
        linear->add_vars(lit - 1);
        linear->add_coeffs(coeff);
      } else {
        // The term was coeff * (1 - var).
        linear->add_vars(-lit - 1);
        linear->add_coeffs(-coeff);
        offset -= coeff;
      }
    }
    linear->add_domain(constraint.has_lower_bound()
                           ? constraint.lower_bound() + offset
                           : kint32min + offset);
    linear->add_domain(constraint.has_upper_bound()
                           ? constraint.upper_bound() + offset
                           : kint32max + offset);
  }
  if (problem.has_objective()) {
    CpObjectiveProto* objective = result.mutable_objective();
    int64 offset = 0;
    for (int i = 0; i < problem.objective().literals_size(); ++i) {
      const int lit = problem.objective().literals(i);
      const int64 coeff = problem.objective().coefficients(i);
      if (lit > 0) {
        objective->add_vars(lit - 1);
        objective->add_coeffs(coeff);
      } else {
        objective->add_vars(-lit - 1);
        objective->add_coeffs(-coeff);
        offset -= coeff;
      }
    }
    objective->set_offset(offset + problem.objective().offset());
    objective->set_scaling_factor(problem.objective().scaling_factor());
  }
  return result;
}

void ChangeOptimizationDirection(LinearBooleanProblem* problem) {
  LinearObjective* objective = problem->mutable_objective();
  objective->set_scaling_factor(-objective->scaling_factor());
  objective->set_offset(-objective->offset());
  // We need 'auto' here to keep the open-source compilation happy
  // (it uses the public protobuf release).
  for (auto& coefficients_ref : *objective->mutable_coefficients()) {
    coefficients_ref = -coefficients_ref;
  }
}

bool LoadBooleanProblem(const LinearBooleanProblem& problem,
                        SatSolver* solver) {
  // TODO(user): Currently, the sat solver can load without any issue
  // constraints with duplicate variables, so we just output a warning if the
  // problem is not "valid". Make this a strong check once we have some
  // preprocessing step to remove duplicates variable in the constraints.
  const util::Status status = ValidateBooleanProblem(problem);
  if (!status.ok()) {
    LOG(WARNING) << "The given problem is invalid!";
  }

  if (solver->parameters().log_search_progress()) {
    LOG(INFO) << "Loading problem '" << problem.name() << "', "
              << problem.num_variables() << " variables, "
              << problem.constraints_size() << " constraints.";
  }
  solver->SetNumVariables(problem.num_variables());
  std::vector<LiteralWithCoeff> cst;
  int64 num_terms = 0;
  int num_constraints = 0;
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    num_terms += constraint.literals_size();
    cst = ConvertLinearExpression(constraint);
    if (!solver->AddLinearConstraint(
            constraint.has_lower_bound(), Coefficient(constraint.lower_bound()),
            constraint.has_upper_bound(), Coefficient(constraint.upper_bound()),
            &cst)) {
      LOG(INFO) << "Problem detected to be UNSAT when "
                << "adding the constraint #" << num_constraints
                << " with name '" << constraint.name() << "'";
      return false;
    }
    ++num_constraints;
  }
  if (solver->parameters().log_search_progress()) {
    LOG(INFO) << "The problem contains " << num_terms << " terms.";
  }
  return true;
}

bool LoadAndConsumeBooleanProblem(LinearBooleanProblem* problem,
                                  SatSolver* solver) {
  const util::Status status = ValidateBooleanProblem(*problem);
  if (!status.ok()) {
    LOG(WARNING) << "The given problem is invalid! " << status.error_message();
  }
  if (solver->parameters().log_search_progress()) {
    LOG(INFO) << "LinearBooleanProblem memory: " << problem->SpaceUsed();
    LOG(INFO) << "Loading problem '" << problem->name() << "', "
              << problem->num_variables() << " variables, "
              << problem->constraints_size() << " constraints.";
  }
  solver->SetNumVariables(problem->num_variables());
  std::vector<LiteralWithCoeff> cst;
  int64 num_terms = 0;
  int num_constraints = 0;

  // We will process the constraints backward so we can free the memory used by
  // each constraint just after processing it. Because of that, we initially
  // reverse all the constraints to add them in the same order.
  std::reverse(problem->mutable_constraints()->begin(),
               problem->mutable_constraints()->end());
  for (int i = problem->constraints_size() - 1; i >= 0; --i) {
    const LinearBooleanConstraint& constraint = problem->constraints(i);
    num_terms += constraint.literals_size();
    cst = ConvertLinearExpression(constraint);
    if (!solver->AddLinearConstraint(
            constraint.has_lower_bound(), Coefficient(constraint.lower_bound()),
            constraint.has_upper_bound(), Coefficient(constraint.upper_bound()),
            &cst)) {
      LOG(INFO) << "Problem detected to be UNSAT when "
                << "adding the constraint #" << num_constraints
                << " with name '" << constraint.name() << "'";
      return false;
    }
    delete problem->mutable_constraints()->ReleaseLast();
    ++num_constraints;
  }
  LinearBooleanProblem empty_problem;
  problem->mutable_constraints()->Swap(empty_problem.mutable_constraints());
  if (solver->parameters().log_search_progress()) {
    LOG(INFO) << "The problem contains " << num_terms << " terms.";
  }
  return true;
}

void UseObjectiveForSatAssignmentPreference(const LinearBooleanProblem& problem,
                                            SatSolver* solver) {
  const LinearObjective& objective = problem.objective();
  CHECK_EQ(objective.literals_size(), objective.coefficients_size());
  int64 max_abs_weight = 0;
  for (const int64 coefficient : objective.coefficients()) {
    max_abs_weight = std::max(max_abs_weight, std::abs(coefficient));
  }
  const double max_abs_weight_double = max_abs_weight;
  for (int i = 0; i < objective.literals_size(); ++i) {
    const Literal literal(objective.literals(i));
    const int64 coefficient = objective.coefficients(i);
    const double abs_weight = std::abs(coefficient) / max_abs_weight_double;
    // Because this is a minimization problem, we prefer to assign a Boolean
    // variable to its "low" objective value. So if a literal has a positive
    // weight when true, we want to set it to false.
    solver->SetAssignmentPreference(
        coefficient > 0 ? literal.Negated() : literal, abs_weight);
  }
}

bool AddObjectiveUpperBound(const LinearBooleanProblem& problem,
                            Coefficient upper_bound, SatSolver* solver) {
  std::vector<LiteralWithCoeff> cst =
      ConvertLinearExpression(problem.objective());
  return solver->AddLinearConstraint(false, Coefficient(0), true, upper_bound,
                                     &cst);
}

bool AddObjectiveConstraint(const LinearBooleanProblem& problem,
                            bool use_lower_bound, Coefficient lower_bound,
                            bool use_upper_bound, Coefficient upper_bound,
                            SatSolver* solver) {
  std::vector<LiteralWithCoeff> cst =
      ConvertLinearExpression(problem.objective());
  return solver->AddLinearConstraint(use_lower_bound, lower_bound,
                                     use_upper_bound, upper_bound, &cst);
}

Coefficient ComputeObjectiveValue(const LinearBooleanProblem& problem,
                                  const std::vector<bool>& assignment) {
  CHECK_EQ(assignment.size(), problem.num_variables());
  Coefficient sum(0);
  const LinearObjective& objective = problem.objective();
  for (int i = 0; i < objective.literals_size(); ++i) {
    const Literal literal(objective.literals(i));
    if (assignment[literal.Variable().value()] == literal.IsPositive()) {
      sum += objective.coefficients(i);
    }
  }
  return sum;
}

bool IsAssignmentValid(const LinearBooleanProblem& problem,
                       const std::vector<bool>& assignment) {
  CHECK_EQ(assignment.size(), problem.num_variables());

  // Check that all constraints are satisfied.
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    Coefficient sum(0);
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const Literal literal(constraint.literals(i));
      if (assignment[literal.Variable().value()] == literal.IsPositive()) {
        sum += constraint.coefficients(i);
      }
    }
    if (constraint.has_lower_bound() && sum < constraint.lower_bound()) {
      LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                   << constraint.DebugString();
      return false;
    }
    if (constraint.has_upper_bound() && sum > constraint.upper_bound()) {
      LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                   << constraint.DebugString();
      return false;
    }
  }
  return true;
}

// Note(user): This function makes a few assumptions about the format of the
// given LinearBooleanProblem. All constraint coefficients must be 1 (and of the
// form >= 1) and all objective weights must be strictly positive.
std::string LinearBooleanProblemToCnfString(const LinearBooleanProblem& problem) {
  std::string output;
  const bool is_wcnf = (problem.objective().coefficients_size() > 0);
  const LinearObjective& objective = problem.objective();

  // Hack: We know that all the variables with index greater than this have been
  // created "artificially" in order to encode a max-sat problem into our
  // format. Each extra variable appear only once, and was used as a slack to
  // reify a soft clause.
  const int first_slack_variable = problem.original_num_variables();

  // This will contains the objective.
  std::unordered_map<int, int64> literal_to_weight;
  std::vector<std::pair<int, int64>> non_slack_objective;

  // This will be the weight of the "hard" clauses in the wcnf format. It must
  // be greater than the sum of the weight of all the soft clauses, so we will
  // just set it to this sum + 1.
  int64 hard_weigth = 1;
  if (is_wcnf) {
    int i = 0;
    for (int64 weight : objective.coefficients()) {
      CHECK_NE(weight, 0);
      int signed_literal = objective.literals(i);

      // There is no direct support for an objective offset in the wcnf format.
      // So this is not a perfect translation of the objective. It is however
      // possible to achieve the same effect by adding a new variable x, and two
      // soft clauses: x with weight offset, and -x with weight offset.
      //
      // TODO(user): implement this trick.
      if (weight < 0) {
        signed_literal = -signed_literal;
        weight = -weight;
      }
      literal_to_weight[objective.literals(i)] = weight;
      if (Literal(signed_literal).Variable() < first_slack_variable) {
        non_slack_objective.push_back(std::make_pair(signed_literal, weight));
      }
      hard_weigth += weight;
      ++i;
    }
    output += StringPrintf("p wcnf %d %d %lld\n", first_slack_variable,
                           static_cast<int>(problem.constraints_size() +
                                            non_slack_objective.size()),
                           hard_weigth);
  } else {
    output += StringPrintf("p cnf %d %d\n", problem.num_variables(),
                           problem.constraints_size());
  }

  std::string constraint_output;
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    if (constraint.literals_size() == 0) return "";  // Assumption.
    constraint_output.clear();
    int64 weight = hard_weigth;
    for (int i = 0; i < constraint.literals_size(); ++i) {
      if (constraint.coefficients(i) != 1) return "";  // Assumption.
      if (is_wcnf && abs(constraint.literals(i)) - 1 >= first_slack_variable) {
        weight = literal_to_weight[constraint.literals(i)];
      } else {
        if (i > 0) constraint_output += " ";
        constraint_output += Literal(constraint.literals(i)).DebugString();
      }
    }
    if (is_wcnf) {
      output += StringPrintf("%lld ", weight);
    }
    output += constraint_output + " 0\n";
  }

  // Output the rest of the objective as singleton constraints.
  if (is_wcnf) {
    for (std::pair<int, int64> p : non_slack_objective) {
      // Since it is falsifying this clause that cost "weigth", we need to take
      // its negation.
      const Literal literal(-p.first);
      output +=
          StringPrintf("%lld %s 0\n", p.second, literal.DebugString().c_str());
    }
  }

  return output;
}

void StoreAssignment(const VariablesAssignment& assignment,
                     BooleanAssignment* output) {
  output->clear_literals();
  for (BooleanVariable var(0); var < assignment.NumberOfVariables(); ++var) {
    if (assignment.VariableIsAssigned(var)) {
      output->add_literals(
          assignment.GetTrueLiteralForAssignedVariable(var).SignedValue());
    }
  }
}

void ExtractSubproblem(const LinearBooleanProblem& problem,
                       const std::vector<int>& constraint_indices,
                       LinearBooleanProblem* subproblem) {
  *subproblem = problem;
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
  std::unordered_map<std::pair<int, int64>, int> id_map_;
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
    cst = ConvertLinearExpression(constraint);
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
    const Literal literal = Literal(BooleanVariable(i), true);
    graph->AddArc(literal.Index().value(), literal.NegatedIndex().value());
    graph->AddArc(literal.NegatedIndex().value(), literal.Index().value());
  }

  // We use 0 for their initial equivalence class, but that may be modified
  // with the objective coefficient (see below).
  initial_equivalence_classes->assign(
      2 * num_variables,
      id_generator.GetId(NodeType::LITERAL_NODE, Coefficient(0)));

  // Literals with different objective coeffs shouldn't be in the same class.
  //
  // We need to canonicalize the objective to regroup literals corresponding
  // to the same variables. Note that we don't care about the offset or
  // optimization direction here, we just care about literals with the same
  // canonical coefficient.
  Coefficient shift;
  Coefficient max_value;
  std::vector<LiteralWithCoeff> expr =
      ConvertLinearExpression(problem.objective());
  ComputeBooleanLinearExpressionCanonicalForm(&expr, &shift, &max_value);
  for (LiteralWithCoeff term : expr) {
    (*initial_equivalence_classes)[term.literal.Index().value()] =
        id_generator.GetId(NodeType::LITERAL_NODE, term.coefficient);
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

void MakeAllLiteralsPositive(LinearBooleanProblem* problem) {
  // Objective.
  LinearObjective* mutable_objective = problem->mutable_objective();
  int64 objective_offset = 0;
  for (int i = 0; i < mutable_objective->literals_size(); ++i) {
    const int signed_literal = mutable_objective->literals(i);
    if (signed_literal < 0) {
      const int64 coefficient = mutable_objective->coefficients(i);
      mutable_objective->set_literals(i, -signed_literal);
      mutable_objective->set_coefficients(i, -coefficient);
      objective_offset += coefficient;
    }
  }
  mutable_objective->set_offset(mutable_objective->offset() + objective_offset);

  // Constraints.
  for (LinearBooleanConstraint& constraint :
       *(problem->mutable_constraints())) {
    int64 sum = 0;
    for (int i = 0; i < constraint.literals_size(); ++i) {
      if (constraint.literals(i) < 0) {
        sum += constraint.coefficients(i);
        constraint.set_literals(i, -constraint.literals(i));
        constraint.set_coefficients(i, -constraint.coefficients(i));
      }
    }
    if (constraint.has_lower_bound()) {
      constraint.set_lower_bound(constraint.lower_bound() - sum);
    }
    if (constraint.has_upper_bound()) {
      constraint.set_upper_bound(constraint.upper_bound() - sum);
    }
  }
}

void FindLinearBooleanProblemSymmetries(
    const LinearBooleanProblem& problem,
    std::vector<std::unique_ptr<SparsePermutation>>* generators) {
  typedef GraphSymmetryFinder::Graph Graph;
  std::vector<int> equivalence_classes;
  std::unique_ptr<Graph> graph(
      GenerateGraphForSymmetryDetection<Graph>(problem, &equivalence_classes));
  LOG(INFO) << "Graph has " << graph->num_nodes() << " nodes and "
            << graph->num_arcs() / 2 << " edges.";
  if (!FLAGS_debug_dump_symmetry_graph_to_file.empty()) {
    // Remap the graph nodes to sort them by equivalence classes.
    std::vector<int> new_node_index(graph->num_nodes(), -1);
    const int num_classes = 1 + *std::max_element(equivalence_classes.begin(),
                                                  equivalence_classes.end());
    std::vector<int> class_size(num_classes, 0);
    for (const int c : equivalence_classes) ++class_size[c];
    std::vector<int> next_index_by_class(num_classes, 0);
    std::partial_sum(class_size.begin(), class_size.end() - 1,
                     next_index_by_class.begin() + 1);
    for (int node = 0; node < graph->num_nodes(); ++node) {
      new_node_index[node] = next_index_by_class[equivalence_classes[node]]++;
    }
    std::unique_ptr<Graph> remapped_graph = RemapGraph(*graph, new_node_index);
    const util::Status status = util::WriteGraphToFile(
        *remapped_graph, FLAGS_debug_dump_symmetry_graph_to_file,
        /*directed=*/false, class_size);
    if (!status.ok()) {
      LOG(DFATAL) << "Error when writing the symmetry graph to file: "
                  << status;
    }
  }
  GraphSymmetryFinder symmetry_finder(*graph,
                                      /*is_undirected=*/true);
  std::vector<int> factorized_automorphism_group_size;
  // TODO(user): inject the appropriate time limit here.
  CHECK_OK(symmetry_finder.FindSymmetries(
      /*time_limit_seconds=*/std::numeric_limits<double>::infinity(),
      &equivalence_classes, generators, &factorized_automorphism_group_size));

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

void ApplyLiteralMappingToBooleanProblem(
    const ITIVector<LiteralIndex, LiteralIndex>& mapping,
    LinearBooleanProblem* problem) {
  Coefficient bound_shift;
  Coefficient max_value;
  std::vector<LiteralWithCoeff> cst;

  // First the objective.
  cst = ConvertLinearExpression(problem->objective());
  ApplyLiteralMapping(mapping, &cst, &bound_shift, &max_value);
  LinearObjective* mutable_objective = problem->mutable_objective();
  mutable_objective->clear_literals();
  mutable_objective->clear_coefficients();
  mutable_objective->set_offset(mutable_objective->offset() -
                                bound_shift.value());
  for (const LiteralWithCoeff& entry : cst) {
    mutable_objective->add_literals(entry.literal.SignedValue());
    mutable_objective->add_coefficients(entry.coefficient.value());
  }

  // Now the clauses.
  for (LinearBooleanConstraint& constraint : *problem->mutable_constraints()) {
    cst = ConvertLinearExpression(constraint);
    constraint.clear_literals();
    constraint.clear_coefficients();
    ApplyLiteralMapping(mapping, &cst, &bound_shift, &max_value);

    // Add bound_shift to the bounds and remove a bound if it is now trivial.
    if (constraint.has_upper_bound()) {
      constraint.set_upper_bound(constraint.upper_bound() +
                                 bound_shift.value());
      if (max_value <= constraint.upper_bound()) {
        constraint.clear_upper_bound();
      }
    }
    if (constraint.has_lower_bound()) {
      constraint.set_lower_bound(constraint.lower_bound() +
                                 bound_shift.value());
      // This is because ApplyLiteralMapping make all coefficient positive.
      if (constraint.lower_bound() <= 0) {
        constraint.clear_lower_bound();
      }
    }

    // If the constraint is always true, we just leave it empty.
    if (constraint.has_lower_bound() || constraint.has_upper_bound()) {
      for (const LiteralWithCoeff& entry : cst) {
        constraint.add_literals(entry.literal.SignedValue());
        constraint.add_coefficients(entry.coefficient.value());
      }
    }
  }

  // Remove empty constraints.
  int new_index = 0;
  const int num_constraints = problem->constraints_size();
  for (int i = 0; i < num_constraints; ++i) {
    if (!(problem->constraints(i).literals_size() == 0)) {
      problem->mutable_constraints()->SwapElements(i, new_index);
      ++new_index;
    }
  }
  problem->mutable_constraints()->DeleteSubrange(new_index,
                                                 num_constraints - new_index);

  // Computes the new number of variables and set it.
  int num_vars = 0;
  for (LiteralIndex index : mapping) {
    if (index >= 0) {
      num_vars = std::max(num_vars, Literal(index).Variable().value() + 1);
    }
  }
  problem->set_num_variables(num_vars);

  // TODO(user): The names is currently all scrambled. Do something about it
  // so that non-fixed variables keep their names.
  problem->mutable_var_names()->DeleteSubrange(
      num_vars, problem->var_names_size() - num_vars);
}

// A simple preprocessing step that does basic probing and removes the
// equivalent literals.
void ProbeAndSimplifyProblem(SatPostsolver* postsolver,
                             LinearBooleanProblem* problem) {
  // TODO(user): expose the number of iterations as a parameter.
  for (int iter = 0; iter < 6; ++iter) {
    SatSolver solver;
    if (!LoadBooleanProblem(*problem, &solver)) {
      LOG(INFO) << "UNSAT when loading the problem.";
    }

    ITIVector<LiteralIndex, LiteralIndex> equiv_map;
    ProbeAndFindEquivalentLiteral(&solver, postsolver, /*drat_writer=*/nullptr,
                                  &equiv_map);

    // We can abort if no information is learned.
    if (equiv_map.empty() && solver.LiteralTrail().Index() == 0) break;

    if (equiv_map.empty()) {
      const int num_literals = 2 * solver.NumVariables();
      for (LiteralIndex index(0); index < num_literals; ++index) {
        equiv_map.push_back(index);
      }
    }

    // Fix fixed variables in the equivalence map and in the postsolver.
    solver.Backtrack(0);
    for (int i = 0; i < solver.LiteralTrail().Index(); ++i) {
      const Literal l = solver.LiteralTrail()[i];
      equiv_map[l.Index()] = kTrueLiteralIndex;
      equiv_map[l.NegatedIndex()] = kFalseLiteralIndex;
      postsolver->FixVariable(l);
    }

    // Remap the variables into a dense set. All the variables for which the
    // equiv_map is not the identity are no longer needed.
    BooleanVariable new_var(0);
    ITIVector<BooleanVariable, BooleanVariable> var_map;
    for (BooleanVariable var(0); var < solver.NumVariables(); ++var) {
      if (equiv_map[Literal(var, true).Index()] == Literal(var, true).Index()) {
        var_map.push_back(new_var);
        ++new_var;
      } else {
        var_map.push_back(BooleanVariable(-1));
      }
    }

    // Apply the variable mapping.
    postsolver->ApplyMapping(var_map);
    for (LiteralIndex index(0); index < equiv_map.size(); ++index) {
      if (equiv_map[index] >= 0) {
        const Literal l(equiv_map[index]);
        const BooleanVariable image = var_map[l.Variable()];
        CHECK_NE(image, BooleanVariable(-1));
        equiv_map[index] = Literal(image, l.IsPositive()).Index();
      }
    }
    ApplyLiteralMappingToBooleanProblem(equiv_map, problem);
  }
}

}  // namespace sat
}  // namespace operations_research
