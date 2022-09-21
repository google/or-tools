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

// A reader for files in the MPS format.
// see http://lpsolve.sourceforge.net/5.5/mps-format.htm
// and http://www.ici.ro/camo/language/ml11.htm.
//
// MPS stands for Mathematical Programming System.
//
// The format was invented by IBM in the 60's, and has become the de facto
// standard. We developed this reader to be able to read benchmark data files.
// Using the MPS file format for new models is discouraged.

#ifndef OR_TOOLS_LP_DATA_MPS_READER_H_
#define OR_TOOLS_LP_DATA_MPS_READER_H_

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "ortools/base/hash.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"  // for DISALLOW_COPY_AND_ASSIGN, NULL
#include "ortools/base/map_util.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {
namespace glop {

// Parses an MPS model from a string.
absl::StatusOr<MPModelProto> MpsDataToMPModelProto(const std::string& mps_data);

// Parses an MPS model from a file.
absl::StatusOr<MPModelProto> MpsFileToMPModelProto(const std::string& mps_file);

// Implementation class. Please use the 2 functions above.
//
// Reads a linear program in the mps format.
//
// All Parse() methods clear the previously parsed instance and store the result
// in the given Data class.
//
// TODO(user): Remove the MPSReader class.
class ABSL_DEPRECATED("Use the direct methods instead") MPSReader {
 public:
  enum Form { AUTO_DETECT, FREE, FIXED };

  // Parses instance from a file.
  absl::Status ParseFile(const std::string& file_name, LinearProgram* data,
                         Form form = AUTO_DETECT);

  absl::Status ParseFile(const std::string& file_name, MPModelProto* data,
                         Form form = AUTO_DETECT);
  // Loads instance from string. Useful with MapReduce. Automatically detects
  // the file's format (free or fixed).
  absl::Status ParseProblemFromString(const std::string& source,
                                      LinearProgram* data,
                                      MPSReader::Form form = AUTO_DETECT);
  absl::Status ParseProblemFromString(const std::string& source,
                                      MPModelProto* data,
                                      MPSReader::Form form = AUTO_DETECT);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_MPS_READER_H_
