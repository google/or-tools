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

// Google C++ bindings for Xpress C API.
//
// Attempts to be as close to the Xpress C API as possible, with the following
// differences:
//   * Use destructors to automatically clean up the environment and model.
//   * Use absl::Status to propagate errors.
//   * Use absl::StatusOr instead of output arguments.
//   * Use absl::Span<T> instead of T* and size for array args.
//   * Use std::string instead of null terminated char* for string values (note
//     that attribute names are still char*).
//   * When setting array data, accept const data (absl::Span<const T>).
#ifndef ORTOOLS_MATH_OPT_SOLVERS_XPRESS_G_XPRESS_H_
#define ORTOOLS_MATH_OPT_SOLVERS_XPRESS_G_XPRESS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/third_party_solvers/xpress_environment.h"

namespace operations_research::math_opt {

class Xpress {
 public:
  Xpress() = delete;

  // Creates a new Xpress
  static absl::StatusOr<std::unique_ptr<Xpress>> New(
      absl::string_view model_name);
  absl::Status SetProbName(absl::string_view name);

  ~Xpress();

  absl::Status GetControlInfo(char const* name, int* p_id, int* p_type) const;

  absl::StatusOr<int> GetIntControl(int control) const;
  absl::Status SetIntControl(int control, int value);
  absl::Status ResetIntControl(int control);  // reset to default value

  absl::StatusOr<int64_t> GetIntControl64(int control) const;
  absl::Status SetIntControl64(int control, int64_t value);

  absl::StatusOr<double> GetDblControl(int control) const;
  absl::Status SetDblControl(int control, double value);

  absl::StatusOr<std::string> GetStrControl(int control) const;
  absl::Status SetStrControl(int control, std::string const& value);

  absl::StatusOr<int> GetIntAttr(int attribute) const;

  absl::StatusOr<double> GetDoubleAttr(int attribute) const;
  absl::StatusOr<double> GetObjectiveDoubleAttr(int objidx,
                                                int attribute) const;

  absl::Status AddVars(std::size_t count, absl::Span<const double> obj,
                       absl::Span<const double> lb, absl::Span<const double> ub,
                       absl::Span<const char> vtype);
  absl::Status AddNames(int type, absl::Span<const char> names, int first,
                        int last);

  absl::Status AddConstrs(absl::Span<const char> sense,
                          absl::Span<const double> rhs,
                          absl::Span<const double> rng);
  absl::Status AddConstrs(absl::Span<const char> rowtype,
                          absl::Span<const double> rhs,
                          absl::Span<const double> rng,
                          absl::Span<const int> start,
                          absl::Span<const int> colind,
                          absl::Span<const double> rowcoef);

  absl::Status SetObjectiveSense(bool maximize);
  absl::Status SetLinearObjective(double constant,
                                  absl::Span<const int> col_index,
                                  absl::Span<const double> obj_coeffs);
  absl::Status SetQuadraticObjective(absl::Span<const int> colind1,
                                     absl::Span<const int> colind2,
                                     absl::Span<const double> coefficients);

  absl::Status ChgCoeffs(absl::Span<const int> rowind,
                         absl::Span<const int> colind,
                         absl::Span<const double> values);

  absl::Status LpOptimize(std::string flags);
  // Fetch LP solution (primals, duals, and reduced costs)
  // The user is responsible for ensuring that the three vectors are of correct
  // size (nVars, nCons, and nVars respectively)
  absl::Status GetLpSol(absl::Span<double> primals, absl::Span<double> duals,
                        absl::Span<double> reducedCosts);
  absl::Status Optimize(std::string const& flags = "",
                        int* p_solvestatus = nullptr,
                        int* p_solstatus = nullptr);
  absl::Status PostSolve();

  absl::Status GetLB(absl::Span<double> lb, int first, int last);
  absl::Status GetUB(absl::Span<double> ub, int first, int last);
  absl::Status GetColType(absl::Span<char> ctype, int first, int last);

  absl::Status ChgBounds(absl::Span<int const> colind,
                         absl::Span<char const> bndtype,
                         absl::Span<double const> bndval);
  absl::Status ChgColType(absl::Span<int const> colind,
                          absl::Span<char const> coltype);

  void Terminate();

  absl::StatusOr<int> GetDualStatus() const;
  absl::Status GetBasis(std::vector<int>& rowBasis,
                        std::vector<int>& colBasis) const;
  absl::Status SetStartingBasis(std::vector<int>& rowBasis,
                                std::vector<int>& colBasis) const;

  absl::Status AddCbMessage(void(XPRS_CC* cb)(XPRSprob, void*, char const*, int,
                                              int),
                            void* cbdata, int prio = 0);
  absl::Status RemoveCbMessage(void(XPRS_CC* cb)(XPRSprob, void*, char const*,
                                                 int, int),
                               void* cbdata = nullptr);
  absl::Status AddCbChecktime(int(XPRS_CC* cb)(XPRSprob, void*), void* cbdata,
                              int prio = 0);
  absl::Status RemoveCbChecktime(int(XPRS_CC* cb)(XPRSprob, void*),
                                 void* cbdata = nullptr);

  absl::StatusOr<std::vector<double>> GetVarLb() const;
  absl::StatusOr<std::vector<double>> GetVarUb() const;

  absl::Status Interrupt(int reason);

  absl::StatusOr<bool> IsMIP() const;
  absl::Status GetDuals(int* p_status,
                        std::optional<absl::Span<double>> const& duals,
                        int first, int last);
  absl::Status GetSolution(int* p_status,
                           std::optional<absl::Span<double>> const& x,
                           int first, int last);
  absl::Status GetRedCosts(int* p_status,
                           std::optional<absl::Span<double>> const& dj,
                           int first, int last);

  absl::Status AddMIPSol(absl::Span<double const> vals,
                         absl::Span<int const> colind,
                         char const* name = nullptr);
  absl::Status LoadDelayedRows(absl::Span<int const> rows);
  absl::Status LoadDirs(absl::Span<int const> cols,
                        std::optional<absl::Span<int const>> const& prio,
                        std::optional<absl::Span<char const>> const& dir,
                        std::optional<absl::Span<double const>> const& up,
                        std::optional<absl::Span<double const>> const& down);

  absl::Status SetObjectiveIntControl(int obj, int control, int value);
  absl::Status SetObjectiveDoubleControl(int obj, int control, double value);
  absl::StatusOr<int> AddObjective(double constant, int ncols,
                                   absl::Span<int const> colind,
                                   absl::Span<double const> objcoef,
                                   int priority, double weight);
  absl::StatusOr<double> CalculateObjectiveN(int objidx,
                                             double const* solution);
  absl::Status AddSets(absl::Span<char const> settype,
                       absl::Span<XPRSint64 const> start,
                       absl::Span<int const> colind,
                       absl::Span<double const> refval);
  absl::Status SetIndicators(absl::Span<int const> rowind,
                             absl::Span<int const> colind,
                             absl::Span<int const> complement);
  absl::Status AddRows(absl::Span<char const> rowtype,
                       absl::Span<double const> rhs,
                       absl::Span<double const> rng,
                       absl::Span<XPRSint64 const> start,
                       absl::Span<int const> colind,
                       absl::Span<double const> rowcoef);
  absl::Status AddQRow(char sense, double rhs, double rng,
                       absl::Span<int const> colind,
                       absl::Span<double const> rowcoef,
                       absl::Span<int const> qcol1, absl::Span<int const> qcol2,
                       absl::Span<double const> qcoef);
  absl::Status WriteProb(std::string const& filename,
                         std::string const& flags = "");
  absl::Status SaveAs(std::string const& filename);

 private:
  XPRSprob xpress_model_;

  explicit Xpress(XPRSprob& model);

  absl::Status ToStatus(
      int xprs_err,
      absl::StatusCode code = absl::StatusCode::kInvalidArgument) const;

  std::map<int, int> int_control_defaults_;
  void initIntControlDefaults();
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVERS_XPRESS_G_XPRESS_H_
