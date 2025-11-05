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

#include "ortools/third_party_solvers/glpk/glpk_env_deleter.h"

#include "ortools/base/logging.h"

extern "C" {
#include <glpk.h>
}

namespace operations_research {
namespace {

// Calls glp_free_env() in its destructor.
struct GlpkEnvDeleter {
  ~GlpkEnvDeleter() {
    VLOG(1) << "calling glp_free_env() for this thread";
    glp_free_env();
  }
};

}  // namespace

void SetupGlpkEnvAutomaticDeletion() {
  // The GlpkEnvDeleter will be created at most once per thread where the
  // function is called and its destructor will be called at the exit of this
  // thread.
  thread_local GlpkEnvDeleter env_deleter;
}

}  // namespace operations_research
