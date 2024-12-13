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

#ifndef OR_TOOLS_SAT_CP_MODEL_SOLVER_HELPERS_H_
#define OR_TOOLS_SAT_CP_MODEL_SOLVER_HELPERS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/types/span.h"
#include "ortools/base/timer.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/sat/work_assignment.h"
#include "ortools/util/logging.h"

ABSL_DECLARE_FLAG(bool, cp_model_dump_models);
ABSL_DECLARE_FLAG(std::string, cp_model_dump_prefix);
ABSL_DECLARE_FLAG(bool, cp_model_dump_submodels);

namespace operations_research {
namespace sat {

// Small wrapper containing all the shared classes between our subsolver
// threads. Note that all these classes can also be retrieved with something
// like global_model->GetOrCreate<Class>() but it is not thread-safe to do so.
//
// All the classes here should be thread-safe, or at least safe in the way they
// are accessed. For instance the model_proto will be kept constant for the
// whole duration of the solve.
struct SharedClasses {
  SharedClasses(const CpModelProto* proto, Model* global_model);

  // These are never nullptr.
  const CpModelProto& model_proto;
  WallTimer* const wall_timer;
  ModelSharedTimeLimit* const time_limit;
  SolverLogger* const logger;
  SharedStatistics* const stats;
  SharedStatTables* const stat_tables;
  SharedResponseManager* const response;
  SharedTreeManager* const shared_tree_manager;
  SharedLsSolutionRepository* const ls_hints;

  // These can be nullptr depending on the options.
  std::unique_ptr<SharedBoundsManager> bounds;
  std::unique_ptr<SharedLPSolutionRepository> lp_solutions;
  std::unique_ptr<SharedIncompleteSolutionManager> incomplete_solutions;
  std::unique_ptr<SharedClausesManager> clauses;

  // call local_model->Register() on most of the class here, this allow to
  // more easily depends on one of the shared class deep within the solver.
  void RegisterSharedClassesInLocalModel(Model* local_model);

  bool SearchIsDone();
};

// Loads a CpModelProto inside the given model.
// This should only be called once on a given 'Model' class.
void LoadCpModel(const CpModelProto& model_proto, Model* model);

// Solves an already loaded cp_model_proto.
// The final CpSolverResponse must be read from the shared_response_manager.
//
// TODO(user): This should be transformed so that it can be called many times
// and resume from the last search state as if it wasn't interrupted. That would
// allow use to easily interleave different heuristics in the same thread.
void SolveLoadedCpModel(const CpModelProto& model_proto, Model* model);

// Registers a callback that will export variables bounds fixed at level 0 of
// the search. This should not be registered to a LNS search.
void RegisterVariableBoundsLevelZeroExport(
    const CpModelProto& /*model_proto*/,
    SharedBoundsManager* shared_bounds_manager, Model* model);

// Registers a callback to import new variables bounds stored in the
// shared_bounds_manager. These bounds are imported at level 0 of the search
// in the linear scan minimize function.
void RegisterVariableBoundsLevelZeroImport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model);

// Registers a callback that will report improving objective best bound.
// It will be called each time new objective bound are propagated at level zero.
void RegisterObjectiveBestBoundExport(
    IntegerVariable objective_var,
    SharedResponseManager* shared_response_manager, Model* model);

// Registers a callback to import new objective bounds. It will be called each
// time the search main loop is back to level zero. Note that it the presence of
// assumptions, this will not happen until the set of assumptions is changed.
void RegisterObjectiveBoundsImport(
    SharedResponseManager* shared_response_manager, Model* model);

// Registers a callback that will export good clauses discovered during search.
void RegisterClausesExport(int id, SharedClausesManager* shared_clauses_manager,
                           Model* model);

// Registers a callback to import new clauses stored in the
// shared_clausess_manager. These clauses are imported at level 0 of the search
// in the linear scan minimize function.
// it returns the id of the worker in the shared clause manager.
//
// TODO(user): Can we import them in the core worker ?
int RegisterClausesLevelZeroImport(int id,
                                   SharedClausesManager* shared_clauses_manager,
                                   Model* model);

void PostsolveResponseWrapper(const SatParameters& params,
                              int num_variable_in_original_model,
                              const CpModelProto& mapping_proto,
                              absl::Span<const int> postsolve_mapping,
                              std::vector<int64_t>* solution);

// Try to find a solution by following the hint and using a low conflict limit.
// The CpModelProto must already be loaded in the Model.
void QuickSolveWithHint(const CpModelProto& model_proto, Model* model);

// Solve a model with a different objective consisting of minimizing the L1
// distance with the provided hint. Note that this method creates an in-memory
// copy of the model and loads a local Model object from the copied model.
void MinimizeL1DistanceWithHint(const CpModelProto& model_proto, Model* model);

void LoadFeasibilityPump(const CpModelProto& model_proto, Model* model);

void AdaptGlobalParameters(const CpModelProto& model_proto, Model* model);

// This should be called on the presolved model. It will read the file
// specified by --cp_model_load_debug_solution and properly fill the
// model->Get<DebugSolution>() proto vector.
void LoadDebugSolution(const CpModelProto& model_proto, Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SOLVER_HELPERS_H_
