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

#include "ortools/util/testing_utils.h"

#include <cstdlib>

#ifdef __GLIBC__
#include <errno.h>
#endif

#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace operations_research {

namespace {

absl::string_view GetEnv(absl::string_view name) {
  const char* val = getenv(name.data());
  return val != nullptr ? val : "";
}

absl::string_view GetProgramInvocationName() {
#ifdef __GLIBC__
  return program_invocation_name;
#else
  return "";
#endif
}

}  // namespace

bool UncachedProbablyRunningInsideUnitTest() {
  // To also catch local test runs, we rely on the TEST_TMPDIR, which is an
  // environment variable set by blaze when running a test.
  if (!GetEnv("TEST_TMPDIR").empty()) {
    LOG(INFO) << "'TEST_TMPDIR' environment variable exists and is non-empty";
    return true;
  }

  const absl::string_view program_invocation_name = GetProgramInvocationName();

  // The latter doesn't cover direct invocations of a blaze-bin/ target.
  // For those, we rely on a somewhat questionable hack (hence the 'Probably'
  // in the function name): whether the binary name ends with "_test".
  // TODO(user): Verify whether this covers the most usual py_test, sh_test and
  // java_test rules.
  if (program_invocation_name.ends_with("_test")) {
    LOG(INFO) << "ProgramInvocationName() ends with '_test'";
    return true;
  }

  // Finally, if this program runs in a subprocess spawned by a test, which is
  // itself invoked manually via blaze-bin, its executable may not end with
  // "_test", but it will certainly contain the test's .runfiles directory
  // in its path.
  if (absl::StrContains(program_invocation_name, "_test.runfiles/")) {
    LOG(INFO) << "ProgramInvocationName() contains '_test.runfiles/'.";
    return true;
  }

  // This isn't being run inside a unit test.
  VLOG(1) << "This does not seem to run inside a unit test.";
  return false;
}

bool ProbablyRunningInsideUnitTest() {
  enum State {
    UNKNOWN = 0,
    INSIDE_TEST = 1,
    NOT_INSIDE_TEST = 2,
  };
  static absl::Mutex* cached_state_mutex_ = new absl::Mutex;
  static State* cached_state = new State(UNKNOWN);
  absl::MutexLock lock(*cached_state_mutex_);
  if (*cached_state == UNKNOWN) {
    *cached_state =
        UncachedProbablyRunningInsideUnitTest() ? INSIDE_TEST : NOT_INSIDE_TEST;
  }
  return *cached_state == INSIDE_TEST;
}

bool HACK_CodeCoverageIsEnabled() { return false; }

}  // namespace operations_research
