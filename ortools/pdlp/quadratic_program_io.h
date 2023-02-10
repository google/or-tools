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

#ifndef PDLP_QUADRATIC_PROGRAM_IO_H_
#define PDLP_QUADRATIC_PROGRAM_IO_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "ortools/pdlp/quadratic_program.h"

namespace operations_research::pdlp {

// Reads a quadratic program, determining the type based on the filename's
// suffix:
//   *.mps, *.mps.gz, *.mps.bz2 -> `ReadMpsLinearProgramOrDie`
//   *.pb, *.textproto, *.json, *.json.gz -> `ReadMPModelProtoFileOrDie`
// otherwise CHECK-fails.
QuadraticProgram ReadQuadraticProgramOrDie(const std::string& filename,
                                           bool include_names = false);

QuadraticProgram ReadMpsLinearProgramOrDie(const std::string& lp_file,
                                           bool include_names = false);
// The input may be `MPModelProto` in text format, binary format, or JSON,
// possibly gzipped.
QuadraticProgram ReadMPModelProtoFileOrDie(
    const std::string& mpmodel_proto_file, bool include_names = false);

// NOTE: This will fail if `linear_program` is actually a quadratic program
// (that is, has a non-empty quadratic objective term).
absl::Status WriteLinearProgramToMps(const QuadraticProgram& linear_program,
                                     const std::string& mps_file);
absl::Status WriteQuadraticProgramToMPModelProto(
    const QuadraticProgram& quadratic_program,
    const std::string& mpmodel_proto_file);

}  // namespace operations_research::pdlp
#endif  // PDLP_QUADRATIC_PROGRAM_IO_H_
