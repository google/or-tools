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

#include "ortools/math_opt/io/lp_parser.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/temp_path.h"
#include "ortools/gscip/gscip.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "ortools/math_opt/io/mps_converter.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/util/status_macros.h"
#include "scip/def.h"
#include "scip/scip_prob.h"

namespace operations_research::math_opt {
namespace {

absl::Status ScipConvertLpToMps(const std::string& lp_filename_in,
                                const std::string& mps_filename_out) {
  ASSIGN_OR_RETURN(const std::unique_ptr<GScip> gscip, GScip::Create(""));
  // Warning: puts gscip into an invalid state, but the underlying SCIP is fine.
  RETURN_IF_SCIP_ERROR(
      SCIPreadProb(gscip->scip(), lp_filename_in.c_str(), "lp"));
  RETURN_IF_SCIP_ERROR(SCIPwriteOrigProblem(
      gscip->scip(), mps_filename_out.c_str(), "mps", /*genericnames=*/FALSE));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<ModelProto> ModelProtoFromLp(const absl::string_view lp_data) {
  // Set up temporary files
  const auto dir = absl::WrapUnique(TempPath::Create(TempPath::Local));
  if (dir == nullptr) {
    return absl::InternalError(
        "creating temporary directory when parsing LP file failed");
  }
  const std::string lp_file = file::JoinPath(dir->path(), "model.lp");
  RETURN_IF_ERROR(file::SetContents(lp_file, lp_data, file::Defaults()));
  const std::string mps_file = file::JoinPath(dir->path(), "model.mps");

  // Do the conversion
  RETURN_IF_ERROR(ScipConvertLpToMps(lp_file, mps_file))
      << "failed to convert LP file with SCIP";
  ASSIGN_OR_RETURN(const std::string mps_data,
                   file::GetContents(mps_file, file::Defaults()));
  OR_ASSIGN_OR_RETURN3(
      ModelProto result, MpsToModelProto(mps_data),
      _ << "failed to parse MPS (produced by SCIP from LP file)");
  result.clear_name();
  return result;
}

}  // namespace operations_research::math_opt
