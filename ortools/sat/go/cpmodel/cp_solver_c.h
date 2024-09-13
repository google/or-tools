// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_SAT_GO_CP_SOLVER_C_H_
#define OR_TOOLS_SAT_GO_CP_SOLVER_C_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SolveCpModelWithParameters(const void* creq, int creq_len,
                                const void* cparams, int cparams_len,
                                void** cres, int* cres_len);

void* SolveCpNewAtomicBool();
void SolveCpDestroyAtomicBool(void* atomic_bool);
void SolveCpStopSolve(void* atomic_bool);
// Allows for interruptible solves. Solves can be interrupted by calling
// `SolveCpStopSolve` with the `limit_reached` atomic Boolean.
void SolveCpInterruptible(void* limit_reached, const void* creq, int creq_len,
                          const void* cparams, int cparams_len, void** cres,
                          int* cres_len);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // OR_TOOLS_SAT_GO_CP_SOLVER_C_H_
