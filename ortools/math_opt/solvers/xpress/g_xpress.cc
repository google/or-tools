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

#include "ortools/math_opt/solvers/xpress/g_xpress.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/third_party_solvers/xpress_environment.h"

namespace operations_research::math_opt {

namespace {
bool checkInt32Overflow(const std::size_t value) { return value > INT32_MAX; }
}  // namespace

constexpr int kXpressOk = 0;

absl::Status Xpress::ToStatus(const int xprs_err,
                              const absl::StatusCode code) const {
  if (xprs_err == kXpressOk) {
    return absl::OkStatus();
  }
  char errmsg[512];
  int status = XPRSgetlasterror(xpress_model_, errmsg);
  if (status == kXpressOk) {
    return util::StatusBuilder(code)
           << "Xpress error code: " << xprs_err << ", message: " << errmsg;
  }
  return util::StatusBuilder(code) << "Xpress error code: " << xprs_err
                                   << " (message could not be fetched)";
}

Xpress::Xpress(XPRSprob& model) : xpress_model_(ABSL_DIE_IF_NULL(model)) {
  initIntControlDefaults();
}

absl::StatusOr<std::unique_ptr<Xpress>> Xpress::New(absl::string_view) {
  bool correctlyLoaded = initXpressEnv();
  CHECK(correctlyLoaded);
  XPRSprob model;
  CHECK_EQ(kXpressOk, XPRScreateprob(&model));
  return absl::WrapUnique(new Xpress(model));
}

absl::Status Xpress::SetProbName(absl::string_view name) {
  std::string truncated(name);
  auto maxLength = GetIntAttr(XPRS_MAXPROBNAMELENGTH);
  if (truncated.length() > maxLength.value_or(INT_MAX)) {
    truncated = truncated.substr(0, maxLength.value_or(INT_MAX));
  }
  return ToStatus(XPRSsetprobname(xpress_model_, truncated.c_str()));
}

absl::Status Xpress::AddCbMessage(void(XPRS_CC* cb)(XPRSprob, void*,
                                                    char const*, int, int),
                                  void* cbdata, int prio) {
  return ToStatus(XPRSaddcbmessage(xpress_model_, cb, cbdata, prio));
}

absl::Status Xpress::RemoveCbMessage(void(XPRS_CC* cb)(XPRSprob, void*,
                                                       char const*, int, int),
                                     void* cbdata) {
  return ToStatus(XPRSremovecbmessage(xpress_model_, cb, cbdata));
}

absl::Status Xpress::AddCbChecktime(int(XPRS_CC* cb)(XPRSprob, void*),
                                    void* cbdata, int prio) {
  return ToStatus(XPRSaddcbchecktime(xpress_model_, cb, cbdata, prio));
}

absl::Status Xpress::RemoveCbChecktime(int(XPRS_CC* cb)(XPRSprob, void*),
				       void* cbdata) {
  return ToStatus(XPRSremovecbchecktime(xpress_model_, cb, cbdata));
}

Xpress::~Xpress() {
  CHECK_EQ(kXpressOk, XPRSdestroyprob(xpress_model_));
  CHECK_EQ(kXpressOk, XPRSfree());
}

void Xpress::initIntControlDefaults() {
  std::vector controls = {XPRS_LPITERLIMIT, XPRS_BARITERLIMIT};
  for (auto control : controls) {
    int_control_defaults_[control] = GetIntControl(control).value();
  }
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
  if (checkInt32Overflow(lb.size())) {
    return absl::InvalidArgumentError(
        "XPRESS cannot handle more than 2^31 variables");
  }
  const int num_vars = static_cast<int>(lb.size());
  if (vind.size() != vval.size() || ub.size() != num_vars ||
      vtype.size() != num_vars || (!obj.empty() && obj.size() != num_vars) ||
      (!vbegin.empty() && vbegin.size() != num_vars)) {
    return absl::InvalidArgumentError(
        "Xpress::AddVars arguments are of inconsistent sizes");
  }
  double* c_obj = nullptr;
  if (!obj.empty()) {
    c_obj = const_cast<double*>(obj.data());
  }
  // TODO: look into int64_t support for number of vars (use XPRSaddcols64)
  return ToStatus(XPRSaddcols(xpress_model_, num_vars, 0, c_obj, nullptr,
                              nullptr, nullptr, lb.data(), ub.data()));
}

absl::Status Xpress::AddConstrs(const absl::Span<const char> sense,
                                const absl::Span<const double> rhs,
                                const absl::Span<const double> rng) {
  const int num_cons = static_cast<int>(sense.size());
  if (rhs.size() != num_cons) {
    return absl::InvalidArgumentError(
        "RHS must have one element per constraint.");
  }
  return ToStatus(XPRSaddrows(xpress_model_, num_cons, 0, sense.data(),
                              rhs.data(), rng.data(), nullptr, nullptr,
                              nullptr));
}

absl::Status Xpress::AddConstrs(const absl::Span<const char> rowtype,
                                const absl::Span<const double> rhs,
                                const absl::Span<const double> rng,
                                const absl::Span<const int> start,
                                const absl::Span<const int> colind,
                                const absl::Span<const double> rowcoef) {
  const int num_cons = static_cast<int>(rowtype.size());
  if (rhs.size() != num_cons) {
    return absl::InvalidArgumentError(
        "RHS must have one element per constraint.");
  }
  if (start.size() != num_cons) {
    return absl::InvalidArgumentError(
        "START must have one element per constraint.");
  }
  if (colind.size() != rowcoef.size()) {
    return absl::InvalidArgumentError(
        "COLIND and ROWCOEF must be of the same size.");
  }
  return ToStatus(XPRSaddrows(xpress_model_, num_cons, 0, rowtype.data(),
                              rhs.data(), rng.data(), start.data(),
                              colind.data(), rowcoef.data()));
}

absl::Status Xpress::SetObjectiveSense(bool maximize) {
  return ToStatus(XPRSchgobjsense(
      xpress_model_, maximize ? XPRS_OBJ_MAXIMIZE : XPRS_OBJ_MINIMIZE));
}

absl::Status Xpress::SetLinearObjective(
    double constant, const absl::Span<const int> col_index,
    const absl::Span<const double> obj_coeffs) {
  static int indexes[1] = {-1};
  double xprs_values[1] = {-constant};
  RETURN_IF_ERROR(ToStatus(XPRSchgobj(xpress_model_, 1, indexes, xprs_values)))
      << "Failed to set objective offset in XPRESS";

  const int n_cols = static_cast<int>(col_index.size());
  return ToStatus(
      XPRSchgobj(xpress_model_, n_cols, col_index.data(), obj_coeffs.data()));
}

absl::Status Xpress::SetQuadraticObjective(
    const absl::Span<const int> colind1, const absl::Span<const int> colind2,
    const absl::Span<const double> coefficients) {
  const int ncoefs = static_cast<int>(coefficients.size());
  return ToStatus(XPRSchgmqobj(xpress_model_, ncoefs, colind1.data(),
                               colind2.data(), coefficients.data()));
}

absl::Status Xpress::ChgCoeffs(absl::Span<const int> rowind,
                               absl::Span<const int> colind,
                               absl::Span<const double> values) {
  const XPRSint64 n_coefs = static_cast<XPRSint64>(rowind.size());
  return ToStatus(XPRSchgmcoef64(xpress_model_, n_coefs, rowind.data(),
                                 colind.data(), values.data()));
}

absl::Status Xpress::LpOptimize(std::string flags) {
  return ToStatus(XPRSlpoptimize(xpress_model_, flags.c_str()));
}

absl::Status Xpress::GetLpSol(absl::Span<double> primals,
                              absl::Span<double> duals,
                              absl::Span<double> reducedCosts) {
  return ToStatus(XPRSgetlpsol(xpress_model_, primals.data(), nullptr,
                               duals.data(), reducedCosts.data()));
}

absl::Status Xpress::PostSolve() {
  return ToStatus(XPRSpostsolve(xpress_model_));
}

absl::Status Xpress::MipOptimize() {
  return ToStatus(XPRSmipoptimize(xpress_model_, nullptr));
}

void Xpress::Terminate() { XPRSinterrupt(xpress_model_, XPRS_STOP_USER); };

absl::StatusOr<int> Xpress::GetIntControl(int control) const {
  int result;
  RETURN_IF_ERROR(ToStatus(XPRSgetintcontrol(xpress_model_, control, &result)))
      << "Error getting Xpress int control: " << control;
  return result;
}

absl::Status Xpress::SetIntControl(int control, int value) {
  return ToStatus(XPRSsetintcontrol(xpress_model_, control, value));
}

absl::Status Xpress::ResetIntControl(int control) {
  if (int_control_defaults_.count(control)) {
    return ToStatus(XPRSsetintcontrol(xpress_model_, control,
                                      int_control_defaults_[control]));
  }
  return absl::InvalidArgumentError(
      "Default value unknown for control " + std::to_string(control) +
      ", consider adding it to Xpress::initIntControlDefaults");
}

absl::StatusOr<int> Xpress::GetIntAttr(int attribute) const {
  int result;
  RETURN_IF_ERROR(ToStatus(XPRSgetintattrib(xpress_model_, attribute, &result)))
      << "Error getting Xpress int attribute: " << attribute;
  return result;
}

absl::StatusOr<double> Xpress::GetDoubleAttr(int attribute) const {
  double result;
  RETURN_IF_ERROR(ToStatus(XPRSgetdblattrib(xpress_model_, attribute, &result)))
      << "Error getting Xpress double attribute: " << attribute;
  return result;
}

int Xpress::GetNumberOfConstraints() const {
  int n;
  XPRSgetintattrib(xpress_model_, XPRS_ROWS, &n);
  return n;
}

int Xpress::GetNumberOfVariables() const {
  int n;
  XPRSgetintattrib(xpress_model_, XPRS_COLS, &n);
  return n;
}

absl::StatusOr<int> Xpress::GetDualStatus() const {
  int status = 0;
  double values[1];
  // Even though we do not need the values, we have to fetch them, otherwise
  // we'd get a segmentation fault
  RETURN_IF_ERROR(ToStatus(XPRSgetduals(xpress_model_, &status, values, 0, 0)))
      << "Failed to retrieve dual status from XPRESS";
  return status;
}

absl::Status Xpress::GetBasis(std::vector<int>& rowBasis,
                              std::vector<int>& colBasis) const {
  rowBasis.resize(GetNumberOfConstraints());
  colBasis.resize(GetNumberOfVariables());
  return ToStatus(
      XPRSgetbasis(xpress_model_, rowBasis.data(), colBasis.data()));
}

absl::Status Xpress::SetStartingBasis(std::vector<int>& rowBasis,
                                      std::vector<int>& colBasis) const {
  if (rowBasis.size() != colBasis.size()) {
    return absl::InvalidArgumentError(
        "Row basis and column basis must be of same size.");
  }
  return ToStatus(
      XPRSloadbasis(xpress_model_, rowBasis.data(), colBasis.data()));
}

absl::StatusOr<std::vector<double>> Xpress::GetVarLb() const {
  int nVars = GetNumberOfVariables();
  std::vector<double> bounds;
  bounds.reserve(nVars);
  RETURN_IF_ERROR(
      ToStatus(XPRSgetlb(xpress_model_, bounds.data(), 0, nVars - 1)))
      << "Failed to retrieve variable LB from XPRESS";
  return bounds;
}
absl::StatusOr<std::vector<double>> Xpress::GetVarUb() const {
  int nVars = GetNumberOfVariables();
  std::vector<double> bounds;
  bounds.reserve(nVars);
  RETURN_IF_ERROR(
      ToStatus(XPRSgetub(xpress_model_, bounds.data(), 0, nVars - 1)))
      << "Failed to retrieve variable UB from XPRESS";
  return bounds;
}

absl::Status Xpress::Interrupt(int reason) {
  return ToStatus(XPRSinterrupt(xpress_model_, reason));
}

}  // namespace operations_research::math_opt
