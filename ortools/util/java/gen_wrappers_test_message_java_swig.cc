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

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/die_if_null.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_format.h"
#include "ortools/util/java/wrappers.h"
#include "ortools/util/testdata/wrappers_test_message.pb.h"

namespace operations_research::util::java {

void ParseAndGenerate() {
  absl::PrintF(
      R"(
// This is a generated file, do not edit.
%%module(directors="1") wrappers_test_message

%%{
#include "ortools/port/proto_utils.h"
#include "ortools/util/testdata/wrappers_test_message.pb.h"
%%}

%%include "ortools/base/base.i"

%s
)",
      GenerateJavaSwigCode({ABSL_DIE_IF_NULL(
          operations_research::util::WrappersTestMessage::descriptor())}));
}

}  // namespace operations_research::util::java

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);
  operations_research::util::java::ParseAndGenerate();
  return 0;
}
