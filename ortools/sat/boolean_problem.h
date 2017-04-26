// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_SAT_BOOLEAN_PROBLEM_H_
#define OR_TOOLS_SAT_BOOLEAN_PROBLEM_H_

#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/simplification.h"
#include "ortools/base/status.h"

namespace operations_research {
namespace sat {

// Adds the offset and returns the scaled version of the given objective value.
inline double AddOffsetAndScaleObjectiveValue(
    const LinearBooleanProblem& problem, Coefficient v) {
  return (static_cast<double>(v.value()) + problem.objective().offset()) *
         problem.objective().scaling_factor();
}

// Keeps the same objective but change the optimization direction from a
// minimization problem to a maximization problem.
//
// Ex: if the problem was to minimize 2 + x, the new problem will be to maximize
// 2 + x subject to exactly the same constraints.
void ChangeOptimizationDirection(LinearBooleanProblem* problem);

// Copies the assignment from the solver into the given Boolean vector. Note
// that variables with a greater index that the given num_variables are ignored.
void ExtractAssignment(const LinearBooleanProblem& problem,
                       const SatSolver& solver, std::vector<bool>* assignment);

// Tests the preconditions of the given problem (as described in the proto) and
// returns an error if they are not all satisfied.
util::Status ValidateBooleanProblem(const LinearBooleanProblem& problem);

// Loads a BooleanProblem into a given SatSolver instance.
bool LoadBooleanProblem(const LinearBooleanProblem& problem, SatSolver* solver);

// Same as LoadBooleanProblem() but also free the memory used by the problem
// during the loading. This allows to use less peak memory. Note that this
// function clear all the constraints of the given problem (not the objective
// though).
bool LoadAndConsumeBooleanProblem(LinearBooleanProblem* problem,
                                  SatSolver* solver);

// Uses the objective coefficient to drive the SAT search towards an
// heuristically better solution.
void UseObjectiveForSatAssignmentPreference(const LinearBooleanProblem& problem,
                                            SatSolver* solver);

// Adds the constraint that the objective is smaller than the given upper bound.
bool AddObjectiveUpperBound(const LinearBooleanProblem& problem,
                            Coefficient upper_bound, SatSolver* solver);

// Adds the constraint that the objective is smaller or equals to the given
// upper bound.
bool AddObjectiveConstraint(const LinearBooleanProblem& problem,
                            bool use_lower_bound, Coefficient lower_bound,
                            bool use_upper_bound, Coefficient upper_bound,
                            SatSolver* solver);

// Returns the objective value under the current assignment.
Coefficient ComputeObjectiveValue(const LinearBooleanProblem& problem,
                                  const std::vector<bool>& assignment);

// Checks that an assignment is valid for the given BooleanProblem.
bool IsAssignmentValid(const LinearBooleanProblem& problem,
                       const std::vector<bool>& assignment);

// Converts a LinearBooleanProblem to the cnf file format.
// Note that this only works for pure SAT problems (only clauses), max-sat or
// weighted max-sat problems. Returns an empty std::string on error.
std::string LinearBooleanProblemToCnfString(const LinearBooleanProblem& problem);

// Store a variable assignment into the given BooleanAssignement proto.
// Note that only the assigned variables are stored, so the assignment may be
// incomplete.
void StoreAssignment(const VariablesAssignment& assignment,
                     BooleanAssignment* output);

// Constructs a sub-problem formed by the constraints with given indices.
void ExtractSubproblem(const LinearBooleanProblem& problem,
                       const std::vector<int>& constraint_indices,
                       LinearBooleanProblem* subproblem);

// Modifies the given LinearBooleanProblem so that all the literals appearing
// inside are positive.
void MakeAllLiteralsPositive(LinearBooleanProblem* problem);

// Returns a list of generators of the symmetry group of the given problem. Each
// generator is a permutation of the integer range [0, 2n) where n is the number
// of variables of the problem. They are permutations of the (index
// representation of the) problem literals.
void FindLinearBooleanProblemSymmetries(
    const LinearBooleanProblem& problem,
    std::vector<std::unique_ptr<SparsePermutation>>* generators);

// Maps all the literals of the problem. Note that this converts the cost of a
// variable correctly, that is if a variable with cost is mapped to another, the
// cost of the later is updated.
//
// Preconditions: the mapping must map l and not(l) to the same variable and be
// of the correct size. It can also map a literal index to kTrueLiteralIndex
// or kFalseLiteralIndex in order to fix the variable.
void ApplyLiteralMappingToBooleanProblem(
    const ITIVector<LiteralIndex, LiteralIndex>& mapping,
    LinearBooleanProblem* problem);

// A simple preprocessing step that does basic probing and removes the fixed and
// equivalent variables. Note that the variable indices will also be remapped in
// order to be dense. The given postsolver will be updated with the information
// needed during postsolve.
void ProbeAndSimplifyProblem(SatPostsolver* postsolver,
                             LinearBooleanProblem* problem);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_BOOLEAN_PROBLEM_H_
