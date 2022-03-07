// Copyright 2010-2021 Google LLC
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

#include "ortools/pdlp/quadratic_program_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/base/casts.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "ortools/base/basictypes.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/lp_data/mps_reader.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/util/file_util.h"

namespace operations_research::pdlp {

// TODO(user): Update internal helper functions to use references instead of
// pointers.

using ::operations_research::glop::MPSReader;

QuadraticProgram ReadQuadraticProgramOrDie(const std::string& filename,
                                           bool include_names) {
  if (absl::EndsWith(filename, ".mps") || absl::EndsWith(filename, ".mps.gz") ||
      absl::EndsWith(filename, ".mps.bz2")) {
    return ReadMpsLinearProgramOrDie(filename, include_names);
  }
  if (absl::EndsWith(filename, ".pb") ||
      absl::EndsWith(filename, ".textproto") ||
      absl::EndsWith(filename, ".json") ||
      absl::EndsWith(filename, ".json.gz")) {
    return ReadMPModelProtoFileOrDie(filename, include_names);
  }
  LOG(QFATAL) << "Invalid filename suffix in " << filename
              << ".  Valid suffixes are .mps, .mps.gz, .pb, .textproto,"
              << ".json, and .json.gz";
}

QuadraticProgram ReadMpsLinearProgramOrDie(const std::string& lp_file,
                                           bool include_names) {
  MPModelProto lp_proto;
  LOG(INFO) << "Reading " << lp_file;
  QCHECK_OK(MPSReader().ParseFile(lp_file, &lp_proto));
  // MPSReader sometimes fails silently if the file isn't read properly.
  QCHECK_GT(lp_proto.variable_size(), 0)
      << "No variables in LP. Error reading file? " << lp_file;
  auto result = QpFromMpModelProto(lp_proto, /*relax_integer_variables=*/true,
                                   include_names);
  QCHECK_OK(result);
  return *std::move(result);
}

QuadraticProgram ReadMPModelProtoFileOrDie(
    const std::string& mpmodel_proto_file, bool include_names) {
  MPModelProto lp_proto;
  QCHECK(ReadFileToProto(mpmodel_proto_file, &lp_proto)) << mpmodel_proto_file;
  auto result = QpFromMpModelProto(lp_proto, /*relax_integer_variables=*/true,
                                   include_names);
  QCHECK_OK(result);
  return *std::move(result);
}

absl::Status WriteLinearProgramToMps(const QuadraticProgram& linear_program,
                                     const std::string& mps_file) {
  if (!IsLinearProgram(linear_program)) {
    return absl::InvalidArgumentError(
        "'linear_program' has a quadratic objective");
  }
  ASSIGN_OR_RETURN(MPModelProto proto, QpToMpModelProto(linear_program));
  ASSIGN_OR_RETURN(std::string mps_export, ExportModelAsMpsFormat(proto));
  File* file;
  RETURN_IF_ERROR(file::Open(mps_file, "w", &file, file::Defaults()));
  auto status = file::WriteString(file, mps_export, file::Defaults());
  status.Update(file->Close(file::Defaults()));
  return status;
}

absl::Status WriteQuadraticProgramToMPModelProto(
    const QuadraticProgram& quadratic_program,
    const std::string& mpmodel_proto_file) {
  ASSIGN_OR_RETURN(MPModelProto proto, QpToMpModelProto(quadratic_program));

  return file::SetBinaryProto(mpmodel_proto_file, proto, file::Defaults());
}

}  // namespace operations_research::pdlp
