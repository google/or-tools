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

#ifndef OR_TOOLS_GLPK_GLPK_ENV_DELETER_H_
#define OR_TOOLS_GLPK_GLPK_ENV_DELETER_H_

namespace operations_research {

// Setups a thread local that will call glp_free_env() when the calling thread
// exits. This method is safe to be called multiple times on the same thread and
// is thread-safe.
//
// This function needs to be called on any thread where glp_create_prob() is
// called since Glpk automatically creates an environment on each thread if it
// does not already exist but does not have any code to free it. Thus it leaks.
void SetupGlpkEnvAutomaticDeletion();

}  // namespace operations_research

#endif  // OR_TOOLS_GLPK_GLPK_ENV_DELETER_H_
