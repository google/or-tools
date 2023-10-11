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

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "devtools/build/runtime/get_runfiles_dir.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/examples/cpp/branch_and_bound.h"
#include "ortools/math_opt/io/mps_converter.h"

// NOLINTBEGIN(whitespace/line_length)
// clang-format off
// See
// cs/operations_research_data/MIP_MIPLIB/BUILD symbol:miplib2017_tiny_filegroup
// for values.
// clang-format on
// NOLINTEND(whitespace/line_length)
ABSL_FLAG(std::string, instance, "flugpl", "A miplib problem to solve");
ABSL_FLAG(operations_research::math_opt::SolverType, lp_solver,
          operations_research::math_opt::SolverType::kGlop,
          "The underlying LP solver to use.");
ABSL_FLAG(absl::Duration, time_limit, absl::Minutes(1),
          "A limit on how long to run the solver.");

namespace operations_research_example {
namespace {

namespace math_opt = ::operations_research::math_opt;

absl::Status Main() {
  // TODO(b/303820831): figure out how to make this work in open source.
  ASSIGN_OR_RETURN(
      const math_opt::ModelProto model_proto,
      math_opt::ReadMpsFile(devtools_build::GetDataDependencyFilepath(
          absl::StrCat("operations_research_data/"
                       "MIP_MIPLIB/miplib2017/",
                       absl::GetFlag(FLAGS_instance), ".mps.gz"))));
  ASSIGN_OR_RETURN(const std::unique_ptr<math_opt::Model> model,
                   math_opt::Model::FromModelProto(model_proto));
  const BranchAndBoundParameters params = {
      .lp_solver = absl::GetFlag(FLAGS_lp_solver),
      .enable_output = true,
      .time_limit = absl::GetFlag(FLAGS_time_limit)};
  ASSIGN_OR_RETURN(SimpleSolveResult result,
                   SolveWithBranchAndBound(*model, params));
  return absl::OkStatus();
}

}  // namespace
}  // namespace operations_research_example

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = operations_research_example::Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
