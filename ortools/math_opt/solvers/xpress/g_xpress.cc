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

#include "ortools/math_opt/solvers/xpress/g_xpress.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/source_location.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/xpress/environment.h"

namespace operations_research::math_opt {
constexpr int kXpressOk = 0;

absl::Status Xpress::ToStatus(const int xprs_err,
                              const absl::StatusCode code) const {
  if (xprs_err == kXpressOk) {
    return absl::OkStatus();
  }
  char errmsg[512];
  XPRSgetlasterror(xpress_model_, errmsg);
  return util::StatusBuilder(code)
         << "Xpress error code: " << xprs_err << ", message: " << errmsg;
}

Xpress::Xpress(XPRSprob& model) : xpress_model_(ABSL_DIE_IF_NULL(model)) {}

absl::StatusOr<std::unique_ptr<Xpress>> Xpress::New(
    const std::string& model_name) {
  bool correctlyLoaded = initXpressEnv();
  CHECK(correctlyLoaded);
  XPRSprob* model;
  CHECK_EQ(kXpressOk, XPRScreateprob(model));
  DCHECK(model != nullptr);  // should not be NULL if status=0
  CHECK_EQ(kXpressOk, XPRSaddcbmessage(*model, printXpressMessage, NULL, 0));
  return absl::WrapUnique(new Xpress(*model));
}

void XPRS_CC Xpress::printXpressMessage(XPRSprob prob, void* data,
                                        const char* sMsg, int nLen,
                                        int nMsgLvl) {
  if (sMsg) {
    std::cout << sMsg << std::endl;
  }
}

Xpress::~Xpress() {
  CHECK_EQ(kXpressOk, XPRSdestroyprob(xpress_model_));
  CHECK_EQ(kXpressOk, XPRSfree());
}

absl::Status Xpress::AddVars(const absl::Span<const double> obj,
                             const absl::Span<const double> lb,
                             const absl::Span<const double> ub,
                             const absl::Span<const char> vtype) {
  return AddVars({}, {}, {}, obj, lb, ub, vtype);
}

absl::Status Xpress::AddVars(const absl::Span<const int> vbegin,
                             const absl::Span<const int> vind,
                             const absl::Span<const double> vval,
                             const absl::Span<const double> obj,
                             const absl::Span<const double> lb,
                             const absl::Span<const double> ub,
                             const absl::Span<const char> vtype) {
  CHECK_EQ(vind.size(), vval.size());
  const int num_vars = static_cast<int>(lb.size());
  CHECK_EQ(ub.size(), num_vars);
  CHECK_EQ(vtype.size(), num_vars);
  double* c_obj = nullptr;
  if (!obj.empty()) {
    CHECK_EQ(obj.size(), num_vars);
    c_obj = const_cast<double*>(obj.data());
  }
  if (!vbegin.empty()) {
    CHECK_EQ(vbegin.size(), num_vars);
  }
  // TODO: look into int64 support for number of vars (use XPRSaddcols64)
  CHECK_EQ(kXpressOk, XPRSaddcols(xpress_model_, num_vars, 0, c_obj, nullptr,
                                  nullptr, nullptr, lb.data(), ub.data()));
  return absl::OkStatus();
}

absl::Status Xpress::AddConstrs(const absl::Span<const char> sense,
                                const absl::Span<const double> rhs,
                                const absl::Span<const double> rng) {
  const int num_cons = static_cast<int>(sense.size());
  CHECK_EQ(rhs.size(), num_cons);
  return ToStatus(XPRSaddrows(xpress_model_, num_cons, 0, sense.data(),
                              rhs.data(), rng.data(), NULL, NULL, NULL));
}

absl::Status Xpress::SetObjective(bool maximize, double offset,
                                  const absl::Span<const int> colind,
                                  const absl::Span<const double> values) {
  const int n_cols = static_cast<int>(colind.size());
  auto c_colind = const_cast<int*>(colind.data());
  auto c_values = const_cast<double*>(values.data());
  CHECK_EQ(kXpressOk, XPRSchgobj(xpress_model_, n_cols, c_colind, c_values));
  static int indexes[1] = {-1};
  double xprs_values[1] = {-offset};
  CHECK_EQ(kXpressOk, XPRSchgobj(xpress_model_, 1, indexes, xprs_values));
  CHECK_EQ(kXpressOk,
           XPRSchgobjsense(xpress_model_,
                           maximize ? XPRS_OBJ_MAXIMIZE : XPRS_OBJ_MINIMIZE));
  return absl::OkStatus();
}

absl::Status Xpress::ChgCoeffs(absl::Span<const int> rowind,
                               absl::Span<const int> colind,
                               absl::Span<const double> values) {
  const int n_coefs = static_cast<int>(rowind.size());
  auto c_rowind = const_cast<int*>(rowind.data());
  auto c_colind = const_cast<int*>(colind.data());
  auto c_values = const_cast<double*>(values.data());
  CHECK_EQ(kXpressOk,
           XPRSchgmcoef(xpress_model_, n_coefs, c_rowind, c_colind, c_values));
  return absl::OkStatus();
}

absl::StatusOr<int> Xpress::LpOptimizeAndGetStatus() {
  RETURN_IF_ERROR(ToStatus(XPRSlpoptimize(xpress_model_, NULL)))
      << "XPRESS LP solve failed";
  int xpress_status;
  RETURN_IF_ERROR(
      ToStatus(XPRSgetintattrib(xpress_model_, XPRS_LPSTATUS, &xpress_status)))
      << "Could not get XPRESS status";
  return xpress_status;
}
absl::Status Xpress::PostSolve() {
  return ToStatus(XPRSpostsolve(xpress_model_));
}

absl::StatusOr<int> Xpress::MipOptimizeAndGetStatus() {
  RETURN_IF_ERROR(ToStatus(XPRSmipoptimize(xpress_model_, NULL)))
      << "XPRESS MIP solve failed";
  int xpress_status;
  RETURN_IF_ERROR(
      ToStatus(XPRSgetintattrib(xpress_model_, XPRS_MIPSTATUS, &xpress_status)))
      << "Could not get XPRESS status";
  return xpress_status;
}

void Xpress::Terminate() { XPRSinterrupt(xpress_model_, XPRS_STOP_USER); };

absl::StatusOr<int> Xpress::GetIntAttr(int attribute) const {
  int result;
  RETURN_IF_ERROR(ToStatus(XPRSgetintattrib(xpress_model_, attribute, &result)))
      << "Error getting Xpress int attribute: " << attribute;
  return result;
}

absl::Status Xpress::SetIntAttr(int attribute, int value) {
  return ToStatus(XPRSsetintcontrol(xpress_model_, attribute, value));
}

absl::StatusOr<double> Xpress::GetDoubleAttr(int attribute) const {
  double result;
  RETURN_IF_ERROR(ToStatus(XPRSgetdblattrib(xpress_model_, attribute, &result)))
      << "Error getting Xpress double attribute: " << attribute;
  return result;
}

absl::StatusOr<std::vector<double>> Xpress::GetPrimalValues() const {
  int nCols = GetNumberOfColumns();
  XPRSgetintattrib(xpress_model_, XPRS_COLS, &nCols);
  std::vector<double> values(nCols);
  RETURN_IF_ERROR(ToStatus(
      XPRSgetlpsol(xpress_model_, values.data(), nullptr, nullptr, nullptr)))
      << "Error getting Xpress LP solution";
  return values;
}

int Xpress::GetNumberOfRows() const {
  int n;
  XPRSgetintattrib(xpress_model_, XPRS_ROWS, &n);
  return n;
}

int Xpress::GetNumberOfColumns() const {
  int n;
  XPRSgetintattrib(xpress_model_, XPRS_COLS, &n);
  return n;
}

std::vector<double> Xpress::GetConstraintDuals() const {
  int nRows = GetNumberOfRows();
  double values[nRows];
  CHECK_EQ(kXpressOk,
           XPRSgetlpsol(xpress_model_, nullptr, nullptr, values, nullptr));
  return {values, values + nRows};
}
std::vector<double> Xpress::GetReducedCostValues() const {
  int nCols = GetNumberOfColumns();
  double values[nCols];
  CHECK_EQ(kXpressOk,
           XPRSgetlpsol(xpress_model_, nullptr, nullptr, nullptr, values));
  return {values, values + nCols};
}

std::vector<int> Xpress::GetVariableBasis() const {
  int const nCols = GetNumberOfColumns();
  std::vector<int> basis(nCols);
  CHECK_EQ(kXpressOk, XPRSgetbasis(xpress_model_, basis.data(), 0));
  return basis;
}

}  // namespace operations_research::math_opt
