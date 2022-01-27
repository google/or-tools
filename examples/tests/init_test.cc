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

#include "absl/strings/str_cat.h"
#include "ortools/init/init.h"

namespace operations_research {
void TestLogging() {
  LOG(INFO) << "Test Logging";
  CppBridge::InitLogging("init");
  CppBridge::ShutdownLogging();
}

void TestFlags() {
  LOG(INFO) << "Test Flags";
  auto cpp_flags = CppFlags();
  cpp_flags.logtostderr = true;
  cpp_flags.log_prefix = true;
  cpp_flags.cp_model_dump_prefix = "init";
  cpp_flags.cp_model_dump_models = true;
  cpp_flags.cp_model_dump_lns = true;
  cpp_flags.cp_model_dump_response = true;
  CppBridge::SetFlags(cpp_flags);
}

void TestVersion() {
  LOG(INFO) << "Test Version";
  using version = OrToolsVersion;
  int major = version::MajorNumber();
  int minor = version::MinorNumber();
  int patch = version::PatchNumber();
  std::string vers = absl::StrCat(major, ".", minor, ".", patch);
  assert(vers == version::VersionString());
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::TestLogging();
  operations_research::TestFlags();
  operations_research::TestVersion();
  return 0;
}
