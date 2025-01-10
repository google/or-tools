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

// See kUsageStr below, and ./model_exporter.h.

#include <cstdio>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/util/file_util.h"

ABSL_FLAG(std::string, input, "", "REQUIRED: Input file name");
ABSL_FLAG(std::string, output, "", "REQUIRED: Output file name");
ABSL_FLAG(bool, input_is_mp_model_request, false,
          "Whether the input is a MPModelRequest proto.");
ABSL_FLAG(bool, obfuscate, false,
          "Whether variable and constaint names should be obfuscated.");

static const char kUsageStr[] =
    "Convert an operations_research::MPModelProto or"
    " operations_research::MPModelRequest file (containing a single"
    " proto, in ascii or wire format, possibly gzipped)"
    " into a .lp or .mps file.";

int main(int argc, char** argv) {
  InitGoogle(kUsageStr, &argc, &argv, /*remove_flags=*/true);
  std::string input_ascii_pb;
  CHECK(!absl::GetFlag(FLAGS_input).empty() &&
        !absl::GetFlag(FLAGS_output).empty())
      << "--input and --output are required.";
  operations_research::MPModelProto model_proto;
  if (absl::GetFlag(FLAGS_input_is_mp_model_request)) {
    operations_research::MPModelRequest request_proto;
    CHECK_OK(operations_research::ReadFileToProto(absl::GetFlag(FLAGS_input),
                                                  &request_proto));
    model_proto.Swap(request_proto.mutable_model());
  } else {
    CHECK_OK(operations_research::ReadFileToProto(absl::GetFlag(FLAGS_input),
                                                  &model_proto));
  }
  std::string output_contents;
  operations_research::MPModelExportOptions options;
  options.obfuscate = absl::GetFlag(FLAGS_obfuscate);
  if (absl::EndsWith(absl::GetFlag(FLAGS_output), ".lp")) {
    output_contents = ExportModelAsLpFormat(model_proto, options).value();
  } else if (absl::EndsWith(absl::GetFlag(FLAGS_output), ".mps")) {
    output_contents = ExportModelAsMpsFormat(model_proto, options).value();
  } else if (absl::EndsWith(absl::GetFlag(FLAGS_output), ".pb.txt")) {
    google::protobuf::io::StringOutputStream stream(&output_contents);
    CHECK(google::protobuf::TextFormat::PrintToString(model_proto,
                                                      &output_contents));
  } else {
    LOG(FATAL) << "Unsupported extension: " << absl::GetFlag(FLAGS_output)
               << " (try: .lp, .mps or .pb.txt)";
  }
  CHECK_OK(file::SetContents(absl::GetFlag(FLAGS_output), output_contents,
                             file::Defaults()));
  fprintf(stderr, "Wrote '%s' successfully\n",
          absl::GetFlag(FLAGS_output).c_str());
}
