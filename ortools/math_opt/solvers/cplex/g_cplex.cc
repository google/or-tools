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

#include "ortools/math_opt/solvers/cplex/g_cplex.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/solvers/cplex.pb.h"
#include "ortools/third_party_solvers/cplex_environment.h"

// CPLEX C API uses three different error-signaling conventions:
//   - Most functions return a status code where non-zero = error.
//   - Query functions (e.g., CPXgetstat) return 0 when no result is available.
//   - Value-returning functions (e.g., CPXgetprobtype) return -1 on error.
// The three macros below correspond to these conventions.
#define RETURN_IF_CPX_ERROR_ON_NONZERO(env, status, caller_string)            \
  if (status != 0) {                                                          \
    char buffer[CPXMESSAGEBUFSIZE] = {};                                      \
    CPXgeterrorstring(env, status, buffer);                                   \
    return absl::InternalError(absl::StrCat("CPLEX Error in ", caller_string, \
                                            "-> ", status, ": ", buffer));    \
  }

#define RETURN_IF_CPX_ERROR_ON_ZERO(status, caller_string) \
  if (status == 0) {                                       \
    return absl::InternalError(                            \
        absl::StrCat("CPLEX Error in ", caller_string));   \
  }

#define RETURN_IF_CPX_ERROR_ON_MINUSONE(status, caller_string) \
  if (status == -1) {                                          \
    return absl::InternalError(                                \
        absl::StrCat("CPLEX Error in ", caller_string));       \
  }

namespace operations_research::math_opt {

// CPLEX API frequently has special semantics for nullptr which we want to
// trigger with empty spans in a safe way!
template <typename T>
const T* GetSafeConstantCPtr(absl::Span<const T> s) {
  return s.empty() ? nullptr : s.data();
}

Cplex::Cplex(CPXENVptr& env, CPXLPptr& model)
    : cpx_env_(ABSL_DIE_IF_NULL(env)), cpx_model_(ABSL_DIE_IF_NULL(model)) {}

Cplex::~Cplex() {
  const int free_err = CPXfreeprob(cpx_env_, &cpx_model_);
  if (free_err != 0) {
    LOG(ERROR) << "Error freeing CPLEX model, code: " << free_err;
  }
  const int close_err = CPXcloseCPLEX(&cpx_env_);
  if (close_err != 0) {
    LOG(ERROR) << "Error closing CPLEX environment, code: " << close_err;
  }
}

absl::StatusOr<std::unique_ptr<Cplex>> Cplex::New(
    absl::string_view model_name) {
  std::string cplex_lib_dir;
  ABSL_RETURN_IF_ERROR(LoadCplexDynamicLibrary(cplex_lib_dir));

  int status_code = 0;
  CPXENVptr env = CPXopenCPLEX(&status_code);
  if (status_code != 0) {
    return absl::InternalError(
        absl::StrCat("CPXopenCPLEX failed with error code: ", status_code));
  }

  std::string owned_str(model_name);
  CPXLPptr model = CPXcreateprob(env, &status_code, owned_str.c_str());
  if (status_code != 0) {
    CPXcloseCPLEX(&env);
    return absl::InternalError(
        absl::StrCat("CPXcreateprob failed with error code: ", status_code));
  }

  return absl::WrapUnique(new Cplex(env, model));
}

// cplex does not distinguish between missing problem/env and empty cols -> both
// 0
absl::StatusOr<int> Cplex::GetNumCols() {
  return CPXgetnumcols(cpx_env_, cpx_model_);
}

// cplex does not distinguish between missing problem/env and empty cols -> both
// 0
absl::StatusOr<int> Cplex::GetNumRows() {
  return CPXgetnumrows(cpx_env_, cpx_model_);
}

absl::StatusOr<int> Cplex::GetProbType() {
  int res = CPXgetprobtype(cpx_env_, cpx_model_);
  // we expect a living env -> non-one value!
  RETURN_IF_CPX_ERROR_ON_MINUSONE(res, "GetProbType");
  return res;
}

absl::StatusOr<int> Cplex::GetObjSen() {
  int res = CPXgetobjsen(cpx_env_, cpx_model_);
  // we expect a living env -> non-zero value!
  RETURN_IF_CPX_ERROR_ON_ZERO(res, "GetObjSen");

  return res;
}

absl::StatusOr<std::tuple<int, int, int, int>> Cplex::SolnInfo() {
  int solnmethod;
  int solntype;
  int pfeasind;
  int dfeasind;
  int status_code = CPXsolninfo(cpx_env_, cpx_model_, &solnmethod, &solntype,
                                &pfeasind, &dfeasind);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SolnInfo");

  return {{solnmethod, solntype, pfeasind, dfeasind}};
}

absl::StatusOr<double> Cplex::GetBestObjVal() {
  double res;
  int status_code = CPXgetbestobjval(cpx_env_, cpx_model_, &res);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetBestObjVal");

  return res;
}

absl::StatusOr<int> Cplex::GetStat() {
  int status_code = CPXgetstat(cpx_env_, cpx_model_);
  // CPXgetstat returns 0 when no problem has been solved. Since GetStat() is
  // only called post-solve, 0 genuinely indicates an unexpected state.
  RETURN_IF_CPX_ERROR_ON_ZERO(status_code, "GetStat");

  return status_code;
}

absl::StatusOr<int> Cplex::GetItCnt() {
  return CPXgetitcnt(cpx_env_, cpx_model_);
}

absl::StatusOr<int> Cplex::GetMipItCnt() {
  return CPXgetmipitcnt(cpx_env_, cpx_model_);
}

absl::StatusOr<int> Cplex::GetBarItCnt() {
  return CPXgetbaritcnt(cpx_env_, cpx_model_);
}

absl::StatusOr<int> Cplex::GetNodeCnt() {
  return CPXgetnodecnt(cpx_env_, cpx_model_);
}

absl::StatusOr<int> Cplex::GetSolNPoolNumSolns() {
  return CPXgetsolnpoolnumsolns(cpx_env_, cpx_model_);
}

absl::StatusOr<double> Cplex::GetSolnPoolObjVal(int soln) {
  double res;
  int status_code = CPXgetsolnpoolobjval(cpx_env_, cpx_model_, soln, &res);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetSolnPoolObjVal");

  return res;
}

absl::StatusOr<std::vector<double>> Cplex::GetSolnPoolX(int soln) {
  ABSL_ASSIGN_OR_RETURN(int n_vars, GetNumCols());

  // "empty model" safe-guard
  if (n_vars == 0) return std::vector<double>{};

  std::vector<double> res(n_vars);
  int status_code =
      CPXgetsolnpoolx(cpx_env_, cpx_model_, soln, res.data(), 0, n_vars - 1);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetSolnPoolX");

  return res;
}

absl::StatusOr<double> Cplex::GetObjVal() {
  double res;
  int status_code = CPXgetobjval(cpx_env_, cpx_model_, &res);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetObjVal");

  return res;
}

// original API has ret-type=int but information provided is only success or not
absl::StatusOr<std::vector<double>> Cplex::GetX() {
  ABSL_ASSIGN_OR_RETURN(int n_vars, GetNumCols());

  // "empty model" safe-guard
  if (n_vars == 0) return std::vector<double>{};

  std::vector<double> res(n_vars);
  int status_code = CPXgetx(cpx_env_, cpx_model_, res.data(), 0, n_vars - 1);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetX");

  return res;
}

absl::StatusOr<std::vector<double>> Cplex::GetPi() {
  ABSL_ASSIGN_OR_RETURN(int n_rows, GetNumRows());

  // "empty model" safe-guard
  if (n_rows == 0) return std::vector<double>{};

  std::vector<double> res(n_rows);
  int status_code = CPXgetpi(cpx_env_, cpx_model_, res.data(), 0, n_rows - 1);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetPi");

  return res;
}

absl::StatusOr<std::vector<double>> Cplex::GetDj() {
  ABSL_ASSIGN_OR_RETURN(int n_vars, GetNumCols());

  // "empty model" safe-guard
  if (n_vars == 0) return std::vector<double>{};

  std::vector<double> res(n_vars);
  int status_code = CPXgetdj(cpx_env_, cpx_model_, res.data(), 0, n_vars - 1);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetDj");

  return res;
}

absl::Status Cplex::SetParamDouble(int param_id, double value) {
  int status_code = CPXsetdblparam(cpx_env_, param_id, value);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SetParamDouble");
  return absl::OkStatus();
}

absl::Status Cplex::SetParamBool(int param_id, bool value) {
  int status_code =
      CPXsetintparam(cpx_env_, param_id, value ? CPX_ON : CPX_OFF);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SetParamBool");
  return absl::OkStatus();
}

absl::Status Cplex::SetParamInt32(int param_id, int32_t value) {
  int status_code = CPXsetintparam(cpx_env_, param_id, value);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SetParamInt32");
  return absl::OkStatus();
}

absl::Status Cplex::SetParamInt64(int param_id, int64_t value) {
  int status_code =
      CPXsetlongparam(cpx_env_, param_id, static_cast<CPXLONG>(value));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SetParamInt64");
  return absl::OkStatus();
}

absl::Status Cplex::SetParamString(int param_id, std::string value) {
  int status_code = CPXsetstrparam(cpx_env_, param_id, value.c_str());
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SetParamString");
  return absl::OkStatus();
}

absl::Status Cplex::SetDefaults() {
  int status_code = CPXsetdefaults(cpx_env_);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SetDefaults");
  return absl::OkStatus();
}

absl::Status Cplex::NewCols(absl::Span<const double> lb,
                            absl::Span<const double> ub,
                            absl::Span<const char> xctype,
                            absl::Span<const std::string> names) {
  if (lb.empty()) return absl::OkStatus();

  if ((lb.size() != ub.size()) ||
      ((lb.size() != xctype.size()) && (xctype.size() != 0)) ||
      ((lb.size() != names.size()) && (names.size() != 0)))
    return absl::InvalidArgumentError(
        "Cplex::NewCols arguments are of inconsistent sizes");

  std::vector<const char*> col_names_cstr = TransformCopyStringToCstr(names);
  char** col_names_ptr = col_names_cstr.empty()
                             ? nullptr
                             : const_cast<char**>(col_names_cstr.data());

  int status_code =
      CPXnewcols(cpx_env_, cpx_model_, static_cast<int>(lb.size()), nullptr,
                 GetSafeConstantCPtr(lb), GetSafeConstantCPtr(ub),
                 GetSafeConstantCPtr(xctype), col_names_ptr);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "NewCols");

  return absl::OkStatus();
}

absl::Status Cplex::ChgObjSen(int maxormin) {
  int status_code = CPXchgobjsen(cpx_env_, cpx_model_, maxormin);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgObjSen");

  return absl::OkStatus();
}

absl::Status Cplex::ChgObjOffset(double offset) {
  int status_code = CPXchgobjoffset(cpx_env_, cpx_model_, offset);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgObjOffset");

  return absl::OkStatus();
}

absl::Status Cplex::ChgObj(absl::Span<const int> indices,
                           absl::Span<const double> values) {
  if (indices.empty()) return absl::OkStatus();

  if (indices.size() != values.size())
    return absl::InvalidArgumentError(
        "Cplex::ChgObj arguments are of inconsistent sizes");

  int status_code =
      CPXchgobj(cpx_env_, cpx_model_, static_cast<int>(indices.size()),
                GetSafeConstantCPtr(indices), GetSafeConstantCPtr(values));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgObj");

  return absl::OkStatus();
}

absl::Status Cplex::NewRows(absl::Span<const double> rhs,
                            absl::Span<const char> sense,
                            absl::Span<const double> rngval,
                            absl::Span<const std::string> row_names) {
  if (rhs.empty()) return absl::OkStatus();

  if (rhs.size() != sense.size())
    return absl::InvalidArgumentError(
        "Cplex::NewRows arguments are of inconsistent sizes");

  if (!rngval.empty() && rngval.size() != rhs.size())
    return absl::InvalidArgumentError(
        "Cplex::NewRows rngval argument is of inconsistent size");

  std::vector<const char*> row_names_cstr =
      TransformCopyStringToCstr(row_names);
  char** row_names_ptr = row_names_cstr.empty()
                             ? nullptr
                             : const_cast<char**>(row_names_cstr.data());

  int status_code =
      CPXnewrows(cpx_env_, cpx_model_, static_cast<int>(rhs.size()),
                 GetSafeConstantCPtr(rhs), GetSafeConstantCPtr(sense),
                 GetSafeConstantCPtr(rngval), row_names_ptr);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "NewRows");

  return absl::OkStatus();
}

absl::Status Cplex::ChgRngVal(absl::Span<const int> indices,
                              absl::Span<const double> values) {
  if (indices.empty()) return absl::OkStatus();

  if (indices.size() != values.size())
    return absl::InvalidArgumentError(
        "Cplex::ChgRngVal arguments are of inconsistent sizes");

  int status_code =
      CPXchgrngval(cpx_env_, cpx_model_, static_cast<int>(indices.size()),
                   GetSafeConstantCPtr(indices), GetSafeConstantCPtr(values));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgRngVal");

  return absl::OkStatus();
}

absl::Status Cplex::ChgCoefList(absl::Span<const int> rowlist,
                                absl::Span<const int> collist,
                                absl::Span<const double> vallist) {
  if (rowlist.empty()) return absl::OkStatus();

  int status_code =
      CPXchgcoeflist(cpx_env_, cpx_model_, static_cast<int>(rowlist.size()),
                     GetSafeConstantCPtr(rowlist), GetSafeConstantCPtr(collist),
                     GetSafeConstantCPtr(vallist));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgCoefList");

  return absl::OkStatus();
}

absl::Status Cplex::ChgProbName(absl::string_view name) {
  std::string owned_name(name);
  int status_code = CPXchgprobname(cpx_env_, cpx_model_, owned_name.c_str());
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgProbName");

  return absl::OkStatus();
}

absl::Status Cplex::ChgSense(absl::Span<const int> indices,
                             absl::Span<const char> sense) {
  if (indices.empty()) return absl::OkStatus();

  if (indices.size() != sense.size())
    return absl::InvalidArgumentError(
        "Cplex::ChgSense arguments are of inconsistent sizes");

  int status_code =
      CPXchgsense(cpx_env_, cpx_model_, static_cast<int>(indices.size()),
                  GetSafeConstantCPtr(indices), GetSafeConstantCPtr(sense));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgSense");

  return absl::OkStatus();
}

absl::Status Cplex::ChgRhs(absl::Span<const int> indices,
                           absl::Span<const double> values) {
  if (indices.empty()) return absl::OkStatus();

  if (indices.size() != values.size())
    return absl::InvalidArgumentError(
        "Cplex::ChgRhs arguments are of inconsistent sizes");

  int status_code =
      CPXchgrhs(cpx_env_, cpx_model_, static_cast<int>(indices.size()),
                GetSafeConstantCPtr(indices), GetSafeConstantCPtr(values));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "ChgRhs");

  return absl::OkStatus();
}

absl::Status Cplex::Chgbds(absl::Span<const int> indices,
                           absl::Span<const char> lu,
                           absl::Span<const double> bd) {
  if (indices.empty()) return absl::OkStatus();

  int status_code =
      CPXchgbds(cpx_env_, cpx_model_, static_cast<int>(indices.size()),
                GetSafeConstantCPtr(indices), GetSafeConstantCPtr(lu),
                GetSafeConstantCPtr(bd));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "Chgbds");

  return absl::OkStatus();
}

absl::Status Cplex::Chgctype(absl::Span<const int> indices,
                             absl::Span<const char> xctype) {
  if (indices.empty()) return absl::OkStatus();

  int status_code =
      CPXchgctype(cpx_env_, cpx_model_, static_cast<int>(indices.size()),
                  GetSafeConstantCPtr(indices), GetSafeConstantCPtr(xctype));
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "Chgctype");

  return absl::OkStatus();
}

absl::Status Cplex::DelSetCols(absl::Span<const int> indices) {
  ABSL_ASSIGN_OR_RETURN(const int num_cols, GetNumCols());
  std::vector<int> delstat(num_cols, 0);
  for (const int idx : indices) delstat[idx] = 1;
  int status_code = CPXdelsetcols(cpx_env_, cpx_model_, delstat.data());
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "DelSetCols");
  return absl::OkStatus();
}

absl::Status Cplex::DelSetRows(absl::Span<const int> indices) {
  ABSL_ASSIGN_OR_RETURN(const int num_rows, GetNumRows());
  std::vector<int> delstat(num_rows, 0);
  for (const int idx : indices) delstat[idx] = 1;
  int status_code = CPXdelsetrows(cpx_env_, cpx_model_, delstat.data());
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "DelSetRows");
  return absl::OkStatus();
}

absl::Status Cplex::LpOpt() {
  int status_code = CPXlpopt(cpx_env_, cpx_model_);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "LpOpt");
  return absl::OkStatus();
}

absl::Status Cplex::MipOpt() {
  int status_code = CPXmipopt(cpx_env_, cpx_model_);
  // CPXERR_SUBPROB_SOLVE (3019): an LP subproblem at a branch-and-bound node
  // failed to solve. This is recoverable — the MIP solver continues with other
  // nodes and partial results (bounds, feasible solutions) remain valid. This
  // commonly occurs when user-imposed time or iteration limits interrupt a node
  // solve. Treating it as fatal would discard all partial progress and prevent
  // result extraction (ExtractSolveResultProto).
  if (status_code == CPXERR_SUBPROB_SOLVE) {
    return absl::OkStatus();
  }
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "MipOpt");
  return absl::OkStatus();
}

absl::StatusOr<int> Cplex::GetParamNum(absl::string_view name_str) {
  std::string owned_name(name_str);
  int param_int;
  int status_code = CPXgetparamnum(cpx_env_, owned_name.c_str(), &param_int);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetParamNum");

  return param_int;
}

absl::StatusOr<std::vector<double>> Cplex::GetLb(int begin, int end) {
  std::vector<double> res(end - begin + 1);
  int status_code = CPXgetlb(cpx_env_, cpx_model_, res.data(), begin, end);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetLb");

  return res;
}

absl::StatusOr<std::vector<double>> Cplex::GetUb(int begin, int end) {
  std::vector<double> res(end - begin + 1);
  int status_code = CPXgetub(cpx_env_, cpx_model_, res.data(), begin, end);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetUb");

  return res;
}

absl::Status Cplex::SetTerminate(volatile int* terminate_p) {
  int status_code = CPXsetterminate(cpx_env_, terminate_p);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "SetTerminate");

  return absl::OkStatus();
}

std::vector<const char*> Cplex::TransformCopyStringToCstr(
    absl::Span<const std::string> string_span) {
  std::vector<const char*> ptr_array;
  ptr_array.reserve(string_span.size());
  std::transform(string_span.begin(), string_span.end(),
                 std::back_inserter(ptr_array),
                 std::mem_fn(&std::string::c_str));
  return ptr_array;
}

absl::StatusOr<
    std::tuple<CPXCHANNELptr, CPXCHANNELptr, CPXCHANNELptr, CPXCHANNELptr>>
Cplex::GetChannels() {
  CPXCHANNELptr result_p = nullptr;
  CPXCHANNELptr warning_p = nullptr;
  CPXCHANNELptr error_p = nullptr;
  CPXCHANNELptr log_p = nullptr;

  int status_code =
      CPXgetchannels(cpx_env_, &result_p, &warning_p, &error_p, &log_p);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "GetChannels");

  return std::make_tuple(result_p, warning_p, error_p, log_p);
}

absl::Status Cplex::AddFuncDest(CPXCHANNELptr channel, void* handle,
                                void(CPXPUBLIC* msgfunction)(void*,
                                                             const char*)) {
  int status_code = CPXaddfuncdest(cpx_env_, channel, handle, msgfunction);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "AddFuncDest");
  return absl::OkStatus();
}

absl::Status Cplex::DelFuncDest(CPXCHANNELptr channel, void* handle,
                                void(CPXPUBLIC* msgfunction)(void*,
                                                             const char*)) {
  int status_code = CPXdelfuncdest(cpx_env_, channel, handle, msgfunction);
  RETURN_IF_CPX_ERROR_ON_NONZERO(cpx_env_, status_code, "DelFuncDest");
  return absl::OkStatus();
}

absl::StatusOr<std::string> Cplex::Version() {
  CPXCCHARptr string_ptr = CPXversion(cpx_env_);
  if (string_ptr != nullptr)
    return std::string(string_ptr);
  else
    return absl::InternalError("CPLEX Error in Version");
}

}  // namespace operations_research::math_opt