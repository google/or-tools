// Copyright 2010-2025 Google LLC
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

#include "ortools/init/init.h"

#include <string>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "ortools/gurobi/environment.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_solver_helpers.h"

namespace operations_research {
void CppBridge::InitLogging(const std::string& usage) {
  absl::SetProgramUsageMessage(usage);
  absl::InitializeLog();
}

void CppBridge::SetFlags(const CppFlags& flags) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  absl::EnableLogPrefix(flags.log_prefix);
  if (!flags.cp_model_dump_prefix.empty()) {
    absl::SetFlag(&FLAGS_cp_model_dump_prefix, flags.cp_model_dump_prefix);
  }
  absl::SetFlag(&FLAGS_cp_model_dump_models, flags.cp_model_dump_models);
  absl::SetFlag(&FLAGS_cp_model_dump_submodels, flags.cp_model_dump_submodels);
  absl::SetFlag(&FLAGS_cp_model_dump_response, flags.cp_model_dump_response);
}

bool CppBridge::LoadGurobiSharedLibrary(const std::string& full_library_path) {
  return LoadGurobiDynamicLibrary({full_library_path}).ok();
}

}  // namespace operations_research
