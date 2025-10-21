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
template <typename T>
/** Forward an optional span to the C API. */
T* forwardSpan(std::optional<absl::Span<T>> const& span) {
  return span.has_value() ? span.value().data() : nullptr;
}
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

// All span arguments can be missing to indicate "use default values".
// Default objective value: 0
// Default lower bound: 0
// Default upper bound: infinity
// Default type: continuous
absl::Status Xpress::AddVars(std::size_t count,
                             const absl::Span<const double> obj,
                             const absl::Span<const double> lb,
                             const absl::Span<const double> ub,
                             const absl::Span<const char> vtype) {
  ASSIGN_OR_RETURN(int const oldCols, GetIntAttr(XPRS_ORIGINALCOLS));
  if (checkInt32Overflow(count) ||
      checkInt32Overflow(std::size_t(oldCols) + std::size_t(count))) {
    return absl::InvalidArgumentError(
        "XPRESS cannot handle more than 2^31 variables");
  }
  const int num_vars = static_cast<int>(count);
  double const* c_obj = nullptr;
  if (!obj.empty()) {
    if (obj.size() != count)
      return absl::InvalidArgumentError(
          "Xpress::AddVars objective argument has bad size");
    c_obj = obj.data();
  }
  if (!lb.empty() && lb.size() != count)
    return absl::InvalidArgumentError(
        "Xpress::AddVars lower bound argument has bad size");
  if (!ub.empty() && ub.size() != count)
    return absl::InvalidArgumentError(
        "Xpress::AddVars upper bound argument has bad size");
  std::vector<int> colind;
  if (!vtype.empty()) {
    if (vtype.size() != count)
      return absl::InvalidArgumentError(
          "Xpress::AddVars type argument has bad size");
    colind.reserve(count);  // So that we don't OOM after adding
  }
  // XPRSaddcols64() allows to add variables with more than INT_MAX
  // non-zero coefficients here. It does NOT allow adding more than INT_MAX
  // variables.
  // Since we don't add any non-zeros here, it is safe to use XPRSaddcols().
  RETURN_IF_ERROR(ToStatus(XPRSaddcols(
      xpress_model_, num_vars, 0, c_obj, nullptr, nullptr, nullptr,
      lb.size() ? lb.data() : nullptr, ub.size() ? ub.data() : nullptr)));
  if (!vtype.empty()) {
    for (int i = 0; i < num_vars; ++i) colind.push_back(oldCols + i);
    int const ret =
        XPRSchgcoltype(xpress_model_, num_vars, colind.data(), vtype.data());
    if (ret != 0) {
      // Changing the column type failed. We must roll back XPRSaddcols() and
      // then return an error.
      XPRSdelcols(xpress_model_, num_vars, colind.data());
    }
    return ToStatus(ret);
  }
  return absl::OkStatus();
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

absl::Status Xpress::Optimize(std::string const& flags, int* p_solvestatus,
                              int* p_solstatus) {
  return ToStatus(
      XPRSoptimize(xpress_model_, flags.c_str(), p_solvestatus, p_solstatus));
}

void Xpress::Terminate() { XPRSinterrupt(xpress_model_, XPRS_STOP_USER); };

absl::Status Xpress::GetControlInfo(char const* name, int* p_id,
                                    int* p_type) const {
  return ToStatus(XPRSgetcontrolinfo(xpress_model_, name, p_id, p_type));
}

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

absl::StatusOr<int64_t> Xpress::GetIntControl64(int control) const {
  XPRSint64 result;
  RETURN_IF_ERROR(
      ToStatus(XPRSgetintcontrol64(xpress_model_, control, &result)))
      << "Error getting Xpress int64 control: " << control;
  return result;
}

absl::Status Xpress::SetIntControl64(int control, int64_t value) {
  return ToStatus(XPRSsetintcontrol64(xpress_model_, control, value));
}

absl::StatusOr<double> Xpress::GetDblControl(int control) const {
  double result;
  RETURN_IF_ERROR(ToStatus(XPRSgetdblcontrol(xpress_model_, control, &result)))
      << "Error getting Xpress double control: " << control;
  return result;
}

absl::Status Xpress::SetDblControl(int control, double value) {
  return ToStatus(XPRSsetdblcontrol(xpress_model_, control, value));
}

absl::StatusOr<std::string> Xpress::GetStrControl(int control) const {
  int nbytes = 0;
  RETURN_IF_ERROR(
      ToStatus(XPRSgetstringcontrol(xpress_model_, control, NULL, 0, &nbytes)));
  std::vector<char> result(nbytes,
                           '\0');  // nbytes CONTAINS the terminating nul!
  RETURN_IF_ERROR(ToStatus(XPRSgetstringcontrol(
      xpress_model_, control, result.data(), nbytes, &nbytes)))
      << "Error getting Xpress string control: " << control;
  return std::string(result.data(), nbytes);
}

absl::Status Xpress::SetStrControl(int control, std::string const& value) {
  return ToStatus(XPRSsetstrcontrol(xpress_model_, control, value.c_str()));
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

absl::StatusOr<double> Xpress::GetObjectiveDoubleAttr(int objidx,
                                                      int attribute) const {
  double result = 0.0;
  RETURN_IF_ERROR(
      ToStatus(XPRSgetobjdblattrib(xpress_model_, objidx, attribute, &result)))
      << "Error getting Xpress objective double attribute: " << attribute;
  return result;
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
  ASSIGN_OR_RETURN(int const rows, GetIntAttr(XPRS_ORIGINALROWS));
  ASSIGN_OR_RETURN(int const cols, GetIntAttr(XPRS_ORIGINALCOLS));
  rowBasis.resize(rows);
  colBasis.resize(cols);
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
  ASSIGN_OR_RETURN(int const cols, GetIntAttr(XPRS_ORIGINALCOLS));
  std::vector<double> bounds;
  bounds.reserve(cols);
  RETURN_IF_ERROR(
      ToStatus(XPRSgetlb(xpress_model_, bounds.data(), 0, cols - 1)))
      << "Failed to retrieve variable LB from XPRESS";
  return bounds;
}
absl::StatusOr<std::vector<double>> Xpress::GetVarUb() const {
  ASSIGN_OR_RETURN(int const cols, GetIntAttr(XPRS_ORIGINALCOLS));
  std::vector<double> bounds;
  bounds.reserve(cols);
  RETURN_IF_ERROR(
      ToStatus(XPRSgetub(xpress_model_, bounds.data(), 0, cols - 1)))
      << "Failed to retrieve variable UB from XPRESS";
  return bounds;
}

absl::Status Xpress::Interrupt(int reason) {
  return ToStatus(XPRSinterrupt(xpress_model_, reason));
}

absl::StatusOr<bool> Xpress::IsMIP() const {
  ASSIGN_OR_RETURN(auto ents, GetIntAttr(XPRS_MIPENTS));
  return ents != 0; /** TODO: Check for preintsol callback? */
}

absl::Status Xpress::GetDuals(int* p_status,
                              std::optional<absl::Span<double>> const& duals,
                              int first, int last) {
  return ToStatus(
      XPRSgetduals(xpress_model_, p_status, forwardSpan(duals), first, last));
}
absl::Status Xpress::GetSolution(int* p_status,
                                 std::optional<absl::Span<double>> const& x,
                                 int first, int last) {
  return ToStatus(
      XPRSgetsolution(xpress_model_, p_status, forwardSpan(x), first, last));
}
absl::Status Xpress::GetRedCosts(int* p_status,
                                 std::optional<absl::Span<double>> const& dj,
                                 int first, int last) {
  return ToStatus(
      XPRSgetredcosts(xpress_model_, p_status, forwardSpan(dj), first, last));
}

/** Add a mip start that is specified in the original space, i.e., in terms of
 * ortools variables.
 */
absl::Status Xpress::AddMIPSol(absl::Span<double const> vals,
                               absl::Span<int const> colind, char const* name) {
  if (checkInt32Overflow(colind.size()))
    return absl::InvalidArgumentError("more start values than columns");
  if (colind.size() != vals.size())
    return absl::InvalidArgumentError("inconsitent data to AddMIPSol()");
  // XPRSaddmipsol() supports colind=nullptr, but we do not support that here
  // since we don't need it.
  return ToStatus(XPRSaddmipsol(xpress_model_, static_cast<int>(colind.size()),
                                vals.data(), colind.data(), name));
}

absl::Status Xpress::LoadDelayedRows(absl::Span<int const> rows) {
  if (checkInt32Overflow(rows.size()))
    return absl::InvalidArgumentError("more delayed rows than rows");
  return ToStatus(XPRSloaddelayedrows(
      xpress_model_, static_cast<int>(rows.size()), rows.data()));
}

absl::Status Xpress::LoadDirs(
    absl::Span<int const> cols,
    std::optional<absl::Span<int const>> const& prio,
    std::optional<absl::Span<char const>> const& dir,
    std::optional<absl::Span<double const>> const& up,
    std::optional<absl::Span<double const>> const& down) {
  if (checkInt32Overflow(cols.size()))
    return absl::InvalidArgumentError("more directions than columns");
  return ToStatus(XPRSloaddirs(xpress_model_, static_cast<int>(cols.size()),
                               cols.data(), forwardSpan(prio), forwardSpan(dir),
                               forwardSpan(up), forwardSpan(down)));
}

absl::Status Xpress::SetObjectiveIntControl(int obj, int control, int value) {
  return ToStatus(XPRSsetobjintcontrol(xpress_model_, obj, control, value));
}
absl::Status Xpress::SetObjectiveDoubleControl(int obj, int control,
                                               double value) {
  return ToStatus(XPRSsetobjdblcontrol(xpress_model_, obj, control, value));
}
absl::StatusOr<int> Xpress::AddObjective(double constant, int ncols,
                                         absl::Span<int const> colind,
                                         absl::Span<double const> objcoef,
                                         int priority, double weight) {
  ASSIGN_OR_RETURN(int const objs, GetIntAttr(XPRS_OBJECTIVES));
  if (objs == INT_MAX) {
    return util::StatusBuilder(absl::StatusCode::kInvalidArgument)
           << "too many objectives";
  }
  int ret = XPRSaddobj(xpress_model_, ncols, colind.data(), objcoef.data(),
                       priority, weight);
  if (ret) {
    return ToStatus(ret);
  }
  if (constant != 0.0) {
    ret =
        XPRSsetobjdblcontrol(xpress_model_, objs, XPRS_OBJECTIVE_RHS, constant);
    if (ret) {
      XPRSdelobj(xpress_model_, objs);
      return ToStatus(ret);
    }
  }
  return objs;
}

absl::StatusOr<double> Xpress::CalculateObjectiveN(int objidx,
                                                   double const* solution) {
  double objval = 0.0;
  int ret = XPRScalcobjn(xpress_model_, objidx, solution, &objval);
  if (ret) {
    return ToStatus(ret);
  }
  return objval;
}

absl::Status Xpress::AddSets(absl::Span<char const> settype,
                             absl::Span<XPRSint64 const> start,
                             absl::Span<int const> colind,
                             absl::Span<double const> refval) {
  ASSIGN_OR_RETURN(int const oldSets, GetIntAttr(XPRS_ORIGINALSETS));
  if (checkInt32Overflow(settype.size()) ||
      checkInt32Overflow(std::size_t(oldSets) + settype.size())) {
    return absl::InvalidArgumentError(
        "XPRESS cannot handle more than 2^31 SOSs");
  }
  return ToStatus(XPRSaddsets64(xpress_model_, static_cast<int>(settype.size()),
                                colind.size(), settype.data(), start.data(),
                                colind.data(), refval.data()));
}

absl::Status Xpress::SetIndicators(absl::Span<int const> rowind,
                                   absl::Span<int const> colind,
                                   absl::Span<int const> complement) {
  ASSIGN_OR_RETURN(int const oldInds, GetIntAttr(XPRS_ORIGINALINDICATORS));
  if (checkInt32Overflow(rowind.size()) ||
      checkInt32Overflow(std::size_t(oldInds) + rowind.size())) {
    return absl::InvalidArgumentError(
        "XPRESS cannot handle more than 2^31 indicators");
  }
  if (rowind.size() != colind.size() || rowind.size() != complement.size()) {
    return absl::InvalidArgumentError(
        "inconsistent arguments to SetInidicators");
  }
  return ToStatus(
      XPRSsetindicators(xpress_model_, static_cast<int>(rowind.size()),
                        rowind.data(), colind.data(), complement.data()));
}

absl::Status Xpress::AddRows(absl::Span<char const> rowtype,
                             absl::Span<double const> rhs,
                             absl::Span<double const> rng,
                             absl::Span<XPRSint64 const> start,
                             absl::Span<int const> colind,
                             absl::Span<double const> rowcoef) {
  ASSIGN_OR_RETURN(int const oldRows, GetIntAttr(XPRS_ORIGINALROWS));
  if (checkInt32Overflow(rowtype.size()) ||
      checkInt32Overflow(std::size_t(oldRows) + rowtype.size())) {
    return absl::InvalidArgumentError(
        "XPRESS cannot handle more than 2^31 rows");
  }
  if (rowtype.size() != rhs.size() || rowtype.size() != rng.size() ||
      rowtype.size() != start.size() || colind.size() != rowcoef.size())
    return absl::InvalidArgumentError("inconsistent arguments to AddRows");
  return ToStatus(XPRSaddrows64(xpress_model_, static_cast<int>(rowtype.size()),
                                colind.size(), rowtype.data(), rhs.data(),
                                rng.data(), start.data(), colind.data(),
                                rowcoef.data()));
}

absl::StatusOr<bool> Xpress::IsBinary(int colidx) {
  char ctype = '\0';
  RETURN_IF_ERROR(
      ToStatus(XPRSgetcoltype(xpress_model_, &ctype, colidx, colidx)));
  if (ctype == XPRS_BINARY)
    return true;
  else if (ctype != XPRS_INTEGER)
    return false;
  double bnd;
  RETURN_IF_ERROR(ToStatus(XPRSgetlb(xpress_model_, &bnd, colidx, colidx)));
  if (bnd < 0.0 || bnd > 1.0) return false;
  RETURN_IF_ERROR(ToStatus(XPRSgetub(xpress_model_, &bnd, colidx, colidx)));
  if (bnd < 0.0 || bnd > 1.0) return false;
  return true;
}

absl::Status Xpress::AddQRow(char sense, double rhs, double rng,
                             absl::Span<int const> colind,
                             absl::Span<double const> rowcoef,
                             absl::Span<int const> qcol1,
                             absl::Span<int const> qcol2,
                             absl::Span<double const> qcoef) {
  ASSIGN_OR_RETURN(int const oldRows, GetIntAttr(XPRS_ORIGINALROWS));
  if (checkInt32Overflow(std::size_t(oldRows) + 1))
    return absl::InvalidArgumentError(
        "XPRESS cannot handle more than 2^31 rows");
  XPRSint64 const start = 0;
  RETURN_IF_ERROR(
      ToStatus(XPRSaddrows64(xpress_model_, 1, colind.size(), &sense, &rhs,
                             &rng, &start, colind.data(), rowcoef.data())));
  if (qcol1.size() > 0) {
    int const ret = XPRSaddqmatrix64(xpress_model_, oldRows, qcol1.size(),
                                     qcol1.data(), qcol2.data(), qcoef.data());
    if (ret != 0) {
      XPRSdelrows(xpress_model_, 1, &oldRows);
      return ToStatus(ret);
    }
  }
  return absl::OkStatus();
}

absl::Status Xpress::WriteProb(std::string const& filename,
                               std::string const& flags) {
  return ToStatus(
      XPRSwriteprob(xpress_model_, filename.c_str(), flags.c_str()));
}

absl::Status Xpress::SaveAs(std::string const& filename) {
  return ToStatus(XPRSsaveas(xpress_model_, filename.c_str()));
}

}  // namespace operations_research::math_opt
