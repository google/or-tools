// Copyright 2010-2026 Google LLC
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

#ifndef ORTOOLS_MATH_OPT_SOLVERS_CPLEX_G_CPLEX_H_
#define ORTOOLS_MATH_OPT_SOLVERS_CPLEX_G_CPLEX_H_

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/third_party_solvers/cplex_environment.h"

namespace operations_research::math_opt {

class Cplex {
 public:
  Cplex() = delete;

  // Creates a new CPLEX environment and problem instance.
  static absl::StatusOr<std::unique_ptr<Cplex>> New(
      absl::string_view model_name);

  ~Cplex();

  absl::StatusOr<int> GetNumCols();

  absl::StatusOr<int> GetNumRows();

  absl::StatusOr<int> GetObjSen();

  absl::StatusOr<int> GetProbType();

  absl::StatusOr<std::tuple<int, int, int, int>> SolnInfo();

  absl::StatusOr<double> GetBestObjVal();

  absl::StatusOr<int> GetStat();

  absl::StatusOr<int> GetItCnt();

  absl::StatusOr<int> GetMipItCnt();

  absl::StatusOr<int> GetBarItCnt();

  absl::StatusOr<int> GetNodeCnt();

  absl::StatusOr<int> GetSolNPoolNumSolns();

  absl::StatusOr<double> GetSolnPoolObjVal(int soln);

  absl::StatusOr<std::vector<double>> GetSolnPoolX(int soln);

  absl::StatusOr<double> GetObjVal();

  absl::StatusOr<std::vector<double>> GetX();

  absl::StatusOr<std::vector<double>> GetPi();

  absl::StatusOr<std::vector<double>> GetDj();

  absl::Status SetParamDouble(int param_id, double value);
  absl::Status SetParamBool(int param_id, bool value);
  absl::Status SetParamInt32(int param_id, int32_t value);
  absl::Status SetParamInt64(int param_id, int64_t value);
  absl::Status SetParamString(int param_id, std::string value);

  absl::Status NewCols(absl::Span<const double> lb, absl::Span<const double> ub,
                       absl::Span<const char> xctype,
                       absl::Span<const std::string> names);

  absl::Status ChgObjSen(int maxormin);
  absl::Status ChgObjOffset(double offset);
  absl::Status ChgObj(absl::Span<const int> indices,
                      absl::Span<const double> values);

  absl::Status NewRows(absl::Span<const double> rhs,
                       absl::Span<const char> sense,
                       absl::Span<const double> rngval,
                       absl::Span<const std::string> row_names);

  absl::Status ChgRngVal(absl::Span<const int> indices,
                         absl::Span<const double> values);

  absl::Status ChgCoefList(absl::Span<const int> rowlist,
                           absl::Span<const int> collist,
                           absl::Span<const double> vallist);

  absl::Status ChgProbName(absl::string_view name);

  absl::Status ChgSense(absl::Span<const int> indices,
                        absl::Span<const char> sense);

  absl::Status ChgRhs(absl::Span<const int> indices,
                      absl::Span<const double> values);

  absl::Status Chgbds(absl::Span<const int> indices, absl::Span<const char> lu,
                      absl::Span<const double> bd);

  absl::Status Chgctype(absl::Span<const int> indices,
                        absl::Span<const char> xctype);

  absl::Status DelSetCols(absl::Span<const int> indices);

  absl::Status DelSetRows(absl::Span<const int> indices);

  absl::Status LpOpt();

  absl::Status MipOpt();

  absl::StatusOr<int> GetParamNum(absl::string_view name_str);

  absl::StatusOr<std::vector<double>> GetLb(int begin, int end);

  absl::StatusOr<std::vector<double>> GetUb(int begin, int end);

  absl::Status SetTerminate(volatile int* terminate_p);

  absl::StatusOr<
      std::tuple<CPXCHANNELptr, CPXCHANNELptr, CPXCHANNELptr, CPXCHANNELptr>>
  GetChannels();

  absl::Status AddFuncDest(CPXCHANNELptr channel, void* handle,
                           void(CPXPUBLIC* msgfunction)(void*, const char*));

  absl::Status DelFuncDest(CPXCHANNELptr channel, void* handle,
                           void(CPXPUBLIC* msgfunction)(void*, const char*));

  absl::StatusOr<std::string> Version();

 private:
  CPXENVptr cpx_env_;
  CPXLPptr cpx_model_;

  explicit Cplex(CPXENVptr& env, CPXLPptr& model);

  static std::vector<const char*> TransformCopyStringToCstr(
      absl::Span<const std::string> string_span);
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVERS_CPLEX_G_CPLEX_H_