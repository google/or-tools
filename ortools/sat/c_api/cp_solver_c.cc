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

#include "ortools/sat/c_api/cp_solver_c.h"

#include <string>

#include "absl/log/check.h"
#include "ortools/base/memutil.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"

namespace operations_research::sat {
namespace {

struct CpSatEnv {
  CpSatEnv() : shared_time_limit(model.GetOrCreate<ModelSharedTimeLimit>()) {}
  void StopSearch() { shared_time_limit->Stop(); }

  Model model;
  ModelSharedTimeLimit* shared_time_limit;
};

}  // namespace
}  // namespace operations_research::sat

extern "C" {

void SolveCpModelWithParameters(const void* creq, int creq_len,
                                const void* cparams, int cparams_len,
                                void** cres, int* cres_len) {
  operations_research::sat::CpSatEnv env;
  SolveCpInterruptible(&env, creq, creq_len, cparams, cparams_len, cres,
                       cres_len);
}

void* SolveCpNewEnv() { return new operations_research::sat::CpSatEnv(); }

void SolveCpDestroyEnv(void* const cenv) {
  delete static_cast<operations_research::sat::CpSatEnv*>(cenv);
}

void SolveCpStopSearch(void* cenv) {
  static_cast<operations_research::sat::CpSatEnv*>(cenv)->StopSearch();
}

void SolveCpInterruptible(void* const cenv, const void* creq, int creq_len,
                          const void* cparams, int cparams_len, void** cres,
                          int* cres_len) {
  operations_research::sat::CpModelProto req;
  CHECK(req.ParseFromArray(creq, creq_len));

  operations_research::sat::SatParameters params;
  CHECK(params.ParseFromArray(cparams, cparams_len));

  operations_research::sat::CpSatEnv* env =
      static_cast<operations_research::sat::CpSatEnv*>(cenv);

  env->model.Add(NewSatParameters(params));
  operations_research::sat::CpSolverResponse res =
      SolveCpModel(req, &env->model);

  std::string res_str;
  CHECK(res.SerializeToString(&res_str));

  *cres_len = static_cast<int>(res_str.size());
  *cres = strings::memdup(res_str.data(), *cres_len);
  CHECK(*cres != nullptr);
}

}  // extern "C"
