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

#ifndef OR_TOOLS_OPEN_SOURCE_INIT_INIT_H_
#define OR_TOOLS_OPEN_SOURCE_INIT_INIT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "ortools/base/logging.h"
#include "ortools/base/version.h"
#include "ortools/gurobi/environment.h"
#include "ortools/sat/cp_model_solver.h"

ABSL_DECLARE_FLAG(std::string, cp_model_dump_prefix);
ABSL_DECLARE_FLAG(bool, cp_model_dump_models);
ABSL_DECLARE_FLAG(bool, cp_model_dump_lns);
ABSL_DECLARE_FLAG(bool, cp_model_dump_response);
ABSL_DECLARE_FLAG(int, stderrthreshold);

namespace operations_research {

/**
 * Simple structure that holds useful C++ flags to setup from non-C++ languages.
 */
struct CppFlags {
  /**
   * @brief Controls the logging level shown on stderr.
   *
   * By default, the logger will only display ERROR and FATAL logs (value 2 and
   * 3) to stderr. To display INFO and WARNING logs (value 0 and 1), change the
   * threshold to the min value of the message that should be printed.
   *
   */
  int stderrthreshold = 2;

  /**
   * @brief Controls if time and source code info are used to prefix logging
   * messages.
   */
  bool log_prefix = false;

  /**
   * Prefix filename for all dumped files (models, solutions, lns sub-models).
   */
  std::string cp_model_dump_prefix;
  /**
   * DEBUG ONLY: Dump CP-SAT models during solve.
   *
   *  When set to true, SolveCpModel() will dump its model protos
   * (original model, presolved model, mapping model) in text  format to
   * 'FLAGS_cp_model_dump_prefix'{model|presolved_model|mapping_model}.pbtxt.
   */
  bool cp_model_dump_models = false;

  /**
   * DEBUG ONLY: Dump CP-SAT LNS models during solve.
   *
   * When set to true, solve will dump all lns models proto in text format to
   * 'FLAGS_cp_model_dump_prefix'lns_xxx.pbtxt.
   */
  bool cp_model_dump_lns;

  /**
   * DEBUG ONLY: Dump the CP-SAT final response found during solve.
   *
   * If true, the final response of each solve will be dumped to
   * 'FLAGS_cp_model_dump_prefix'response.pbtxt.
   */
  bool cp_model_dump_response;
};

/**
 * This class performs various C++ initialization.
 *
 * It is meant to be used once at the start of a program.
 */
class CppBridge {
 public:
  /**
   * Initialize the C++ logging layer.
   *
   * This must be called once before any other library from OR-Tools are used.
   */
  static void InitLogging(const std::string& usage) {
    absl::SetProgramUsageMessage(usage);
    absl::InitializeLog();
  }

  /**
   * Shutdown the C++ logging layer.
   *
   * This can be called to shutdown the C++ logging layer from OR-Tools.
   * It should only be called once.
   *
   * Deprecated: this is a no-op.
   */
  static void ShutdownLogging() {}

  /**
   * Sets all the C++ flags contained in the CppFlags structure.
   */
  static void SetFlags(const CppFlags& flags) {
    absl::SetFlag(&FLAGS_stderrthreshold, flags.stderrthreshold);
    absl::EnableLogPrefix(flags.log_prefix);
    if (!flags.cp_model_dump_prefix.empty()) {
      absl::SetFlag(&FLAGS_cp_model_dump_prefix, flags.cp_model_dump_prefix);
    }
    absl::SetFlag(&FLAGS_cp_model_dump_models, flags.cp_model_dump_models);
    absl::SetFlag(&FLAGS_cp_model_dump_lns, flags.cp_model_dump_lns);
    absl::SetFlag(&FLAGS_cp_model_dump_response, flags.cp_model_dump_response);
  }

  /**
   * Load the gurobi shared library.
   *
   * This is necessary if the library is installed in a non canonical
   * directory, or if for any reason, it is not found.
   * You need to pass the full path, including the shared library file.
   * It returns true if the library was found and correctly loaded.
   */
  static bool LoadGurobiSharedLibrary(const std::string& full_library_path) {
    return LoadGurobiDynamicLibrary({full_library_path}).ok();
  }

  /**
   * Delete a temporary C++ byte array.
   */
  static void DeleteByteArray(uint8_t* buffer) { delete[] buffer; }
};

class OrToolsVersion {
 public:
  /**
   * Returns the major version of OR-Tools.
   */
  static int MajorNumber() {
    return ::operations_research::OrToolsMajorVersion();
  }

  /**
   * Returns the minor version of OR-Tools.
   */
  static int MinorNumber() {
    return ::operations_research::OrToolsMinorVersion();
  }

  /**
   * Returns the patch version of OR-Tools.
   */
  static int PatchNumber() {
    return ::operations_research::OrToolsPatchVersion();
  }

  /**
   * Returns the string version of OR-Tools.
   */
  static std::string VersionString() {
    return ::operations_research::OrToolsVersionString();
  }
};

}  // namespace operations_research

#endif  // OR_TOOLS_OPEN_SOURCE_INIT_INIT_H_
