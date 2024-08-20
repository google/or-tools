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

#include "ortools/sat/go/cpmodel/cp_solver_c.h"

#include <atomic>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/internal/memutil.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {

namespace {

char* memdup(const char* s, size_t slen) {
  void* copy;
  if ((copy = malloc(slen)) == nullptr) return nullptr;
  memcpy(copy, s, slen);
  return reinterpret_cast<char*>(copy);
}

CpSolverResponse solveWithParameters(std::atomic<bool>* const limit_reached,
                                     const CpModelProto& proto,
                                     const SatParameters& params) {
  Model model;
  model.Add(NewSatParameters(params));
  model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(limit_reached);
  return SolveCpModel(proto, &model);
}

}  // namespace

extern "C" {

void SolveCpModelWithParameters(const void* creq, int creq_len,
                                const void* cparams, int cparams_len,
                                void** cres, int* cres_len) {
  return SolveCpInterruptible(nullptr, creq, creq_len, cparams, cparams_len,
                              cres, cres_len);
}

void* SolveCpNewAtomicBool() { return new std::atomic<bool>(false); }

void SolveCpDestroyAtomicBool(void* const atomic_bool) {
  delete static_cast<std::atomic<bool>*>(atomic_bool);
}

void SolveCpStopSolve(void* const atomic_bool) {
  *static_cast<std::atomic<bool>*>(atomic_bool) = true;
}

void SolveCpInterruptible(void* const limit_reached, const void* creq,
                          int creq_len, const void* cparams, int cparams_len,
                          void** cres, int* cres_len) {
  CpModelProto req;
  CHECK(req.ParseFromArray(creq, creq_len));

  SatParameters params;
  CHECK(params.ParseFromArray(cparams, cparams_len));

  CpSolverResponse res = solveWithParameters(
      static_cast<std::atomic<bool>*>(limit_reached), req, params);

  std::string res_str;
  CHECK(res.SerializeToString(&res_str));

  *cres_len = static_cast<int>(res_str.size());
  *cres = memdup(res_str.data(), *cres_len);
  CHECK(*cres != nullptr);
}

}  // extern "C"

}  // namespace operations_research::sat
