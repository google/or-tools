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

#ifndef OR_TOOLS_SAT_STAT_TABLES_H_
#define OR_TOOLS_SAT_STAT_TABLES_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/sat/cp_model_lns.h"
#include "ortools/sat/model.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"

namespace operations_research::sat {

// Contains the table we display after the solver is done.
class SharedStatTables {
 public:
  SharedStatTables();

  // Add a line to the corresponding table.
  void AddTimingStat(const SubSolver& subsolver);

  void AddSearchStat(absl::string_view name, Model* model);

  void AddLpStat(absl::string_view name, Model* model);

  void AddLnsStat(absl::string_view name,
                  const NeighborhoodGenerator& generator);

  void AddLsStat(absl::string_view name, int64_t num_batches,
                 int64_t num_restarts, int64_t num_linear_moves,
                 int64_t num_general_moves, int64_t num_compound_moves,
                 int64_t num_weight_updates);

  // Display the set of table at the end.
  void Display(SolverLogger* logger);

 private:
  mutable absl::Mutex mutex_;

  std::vector<std::vector<std::string>> timing_table_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::vector<std::string>> search_table_ ABSL_GUARDED_BY(mutex_);

  std::vector<std::vector<std::string>> lp_table_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::vector<std::string>> lp_dim_table_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::vector<std::string>> lp_debug_table_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::vector<std::string>> lp_manager_table_
      ABSL_GUARDED_BY(mutex_);

  std::vector<std::vector<std::string>> lns_table_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::vector<std::string>> ls_table_ ABSL_GUARDED_BY(mutex_);

  // This one is dynamic, so we generate it in Display().
  std::vector<std::pair<std::string, absl::btree_map<std::string, int>>>
      lp_cut_table_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace operations_research::sat

#endif  // OR_TOOLS_SAT_STAT_TABLES_H_
