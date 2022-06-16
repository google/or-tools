// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_SAT_MAX_HS_H_
#define OR_TOOLS_SAT_MAX_HS_H_

#include <stdint.h>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/strong_vector.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Generalization of the max-HS algorithm (HS stands for Hitting Set). This is
// similar to MinimizeWithCoreAndLazyEncoding() but it uses a hybrid approach
// with a MIP solver to handle the discovered infeasibility cores.
//
// See, Jessica Davies and Fahiem Bacchus, "Solving MAXSAT by Solving a
// Sequence of Simpler SAT Instances",
// http://www.cs.toronto.edu/~jdavies/daviesCP11.pdf"
//
// Note that an implementation of this approach won the 2016 max-SAT competition
// on the industrial category, see
// http://maxsat.ia.udl.cat/results/#wpms-industrial
//
// TODO(user): This class requires linking with the SCIP MIP solver which is
// quite big, maybe we should find a way not to do that.
class HittingSetOptimizer {
 public:
  HittingSetOptimizer(const CpModelProto& model_proto,
                      const ObjectiveDefinition& objective_definition,
                      const std::function<void()>& feasible_solution_observer,
                      Model* model);

  SatSolver::Status Optimize();

 private:
  const int kUnextracted = -1;

  SatSolver::Status FindMultipleCoresForMaxHs(
      std::vector<Literal> assumptions,
      std::vector<std::vector<Literal>>* core);

  // Extract the objective variables, which is the smallest possible useful set.
  void ExtractObjectiveVariables();

  // Calls ComputeAdditionalVariablesToExtract() and extract all new variables.
  // This must be called after the linear relaxation has been filled.
  void ExtractAdditionalVariables(
      const std::vector<IntegerVariable>& to_extract);

  // Heuristic to decide which variables (in addition to the objective
  // variables) to extract.
  // We use a sorted map to have a deterministic model.
  std::vector<IntegerVariable> ComputeAdditionalVariablesToExtract();

  // Checks whethers a variable is extracted, and return the index in the MIP
  // model. It returns the same indef for both the variable and its negation.
  //
  // Node that the domain of the MIP variable is equal to the domain of the
  // positive variable.
  int GetExtractedIndex(IntegerVariable var) const;

  // Returns false if the model is unsat.
  bool ComputeInitialMpModel();

  // Project the at_most_one constraint on the set of extracted variables.
  void ProjectAndAddAtMostOne(const std::vector<Literal>& literals);

  // Project the linear constraint on the set of extracted variables. Non
  // extracted variables are used to 'extend' the lower and upper bound of the
  // linear equation.
  //
  // It returns a non-null proto if the constraints was successfully extracted.
  MPConstraintProto* ProjectAndAddLinear(const LinearConstraint& linear);

  // Auxiliary method used by ProjectAndAddLinear(), and TightenLinear().
  void ProjectLinear(const LinearConstraint& linear, MPConstraintProto* ct);

  // Imports variable bounds from the shared bound manager (in parallel),
  // updates the domains of the SAT variables, lower and upper bounds of
  // extracted variables. Then it scans the extracted linear constraints and
  // recompute its lower and upper bounds.
  void TightenMpModel();

  // Processes the cores from the SAT solver and add them to the MPModel.
  void AddCoresToTheMpModel(const std::vector<std::vector<Literal>>& cores);

  // Builds the assumptions from the current MP solution.
  std::vector<Literal> BuildAssumptions(
      IntegerValue stratified_threshold,
      IntegerValue* next_stratified_threshold);

  // This will be called each time a feasible solution is found.
  bool ProcessSolution();

  // Import shared information. Returns false if the model is unsat.
  bool ImportFromOtherWorkers();

  // Problem definition
  const CpModelProto& model_proto_;
  const ObjectiveDefinition& objective_definition_;
  const std::function<void()> feasible_solution_observer_;

  // Model object.
  Model* model_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  const SatParameters& parameters_;
  ModelRandomGenerator* random_;
  SharedResponseManager* shared_response_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* integer_encoder_;

  // Linear model.
  MPConstraintProto* obj_constraint_ = nullptr;
  MPModelRequest request_;
  MPSolutionResponse response_;

  // Linear relaxation of the SAT model.
  LinearRelaxation relaxation_;

  // TODO(user): The core is returned in the same order as the assumptions,
  // so we don't really need this map, we could just do a linear scan to
  // recover which node are part of the core.
  absl::flat_hash_map<LiteralIndex, std::vector<int>> assumption_to_indices_;

  // New Booleans variable in the MIP model to represent X OP cte (OP is => if
  // the variable of the objective is positive, <= otherwise).
  absl::flat_hash_map<std::pair<int, int64_t>, int> mp_integer_literals_;

  // Extraction info used in the projection of the model onto the extracted
  // variables.
  // By convention, we always associate the MPVariableProto with both the
  // positive and the negative SAT variable.
  absl::StrongVector<IntegerVariable, int> sat_var_to_mp_var_;

  // The list of <positive sat var, mp var proto> created during the
  // ExtractVariable() method.
  std::vector<std::pair<IntegerVariable, MPVariableProto*>>
      extracted_variables_info_;

  int num_extracted_at_most_ones_ = 0;
  std::vector<std::pair<int, MPConstraintProto*>> linear_extract_info_;

  // Normalized objective definition. All coefficients are positive.
  std::vector<IntegerVariable> normalized_objective_variables_;
  std::vector<IntegerValue> normalized_objective_coefficients_;

  // Temporary vector to store cores.
  std::vector<std::vector<Literal>> temp_cores_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_MAX_HS_H_
