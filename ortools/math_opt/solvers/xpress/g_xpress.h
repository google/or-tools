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
#ifndef OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_G_XPRESS_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_G_XPRESS_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/xpress/environment.h"

namespace operations_research::math_opt {

class Xpress {
 public:
  Xpress() = delete;

  // Creates a new Xpress
  static absl::StatusOr<std::unique_ptr<Xpress>> New(
      const std::string& model_name);
  absl::Status SetProbName(const std::string& name);

  ~Xpress();

  absl::StatusOr<int> GetIntControl(int control) const;
  absl::Status SetIntControl(int control, int value);
  absl::Status ResetIntControl(int control);  // reset to default value

  absl::StatusOr<int> GetIntAttr(int attribute) const;

  absl::StatusOr<double> GetDoubleAttr(int attribute) const;

  absl::Status AddVars(absl::Span<const double> obj,
                       absl::Span<const double> lb, absl::Span<const double> ub,
                       absl::Span<const char> vtype);

  absl::Status AddVars(absl::Span<const int> vbegin, absl::Span<const int> vind,
                       absl::Span<const double> vval,
                       absl::Span<const double> obj,
                       absl::Span<const double> lb, absl::Span<const double> ub,
                       absl::Span<const char> vtype);

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
  absl::Status SetLinearObjective(double offset, absl::Span<const int> colind,
                                  absl::Span<const double> values);
  absl::Status SetQuadraticObjective(absl::Span<const int> colind1,
                                     absl::Span<const int> colind2,
                                     absl::Span<const double> coefficients);

  absl::Status ChgCoeffs(absl::Span<const int> cind, absl::Span<const int> vind,
                         absl::Span<const double> val);

  absl::Status LpOptimize(std::string flags);
  // Fetch LP solution (primals, duals, and reduced costs)
  // The user is responsible for ensuring that the three vectors are of correct
  // size (nVars, nCons, and nVars respectively)
  absl::Status GetLpSol(absl::Span<double> primals, absl::Span<double> duals,
                        absl::Span<double> reducedCosts);
  absl::Status MipOptimize();
  absl::Status PostSolve();

  void Terminate();

  absl::StatusOr<int> GetDualStatus() const;
  absl::Status GetBasis(std::vector<int>& rowBasis,
                        std::vector<int>& colBasis) const;
  absl::Status SetStartingBasis(std::vector<int>& rowBasis,
                                std::vector<int>& colBasis) const;

  static void XPRS_CC printXpressMessage(XPRSprob prob, void* data,
                                         const char* sMsg, int nLen,
                                         int nMsgLvl);

  int GetNumberOfConstraints() const;
  int GetNumberOfVariables() const;

  absl::StatusOr<std::vector<double>> GetVarLb() const;
  absl::StatusOr<std::vector<double>> GetVarUb() const;

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

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_G_XPRESS_H_
