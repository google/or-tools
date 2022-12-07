// Copyright 2010-2022 Google LLC
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

// Google C++ bindings for Gurobi C API.
//
// Attempts to be as close to the Gurobi C API as possible, with the following
// differences:
//   * Use destructors to automatically clean up the environment and model.
//   * Use absl::Status to propagate errors instead of int gurobi error codes.
//   * Use absl::StatusOr instead of output arguments.
//   * Use absl::Span<T> instead of T* and size for array args.
//   * Use std::string instead of null terminated char* for string values (note
//     that attribute names are still char*).
//   * When setting array data, accept const data (absl::Span<const T>).
//   * Callbacks are passed as an argument to optimize and then are cleared.
//   * Callbacks propagate errors with status.
//   * There is no distinction between a GRBmodel and the GRBenv created for a
//     model, they are jointly captured by the newly defined Gurobi object.
//   * Parameters are set on the Gurobi class rather than on a GRBenv. We do not
//     provide an API fo setting parameters on the primary environment, only on
//     the child environment created by GRBnewmodel (for details see
//     https://www.gurobi.com/documentation/9.1/refman/c_newmodel.html ).
#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_G_GUROBI_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_G_GUROBI_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/base/source_location.h"
#include "ortools/gurobi/environment.h"

namespace operations_research::math_opt {

// An ISV key for the Gurobi solver, an alternative to using a license file.
//
// See http://www.gurobi.com/products/licensing-pricing/isv-program.
struct GurobiIsvKey {
  std::string name;
  std::string application_name;
  int32_t expiration = 0;
  std::string key;
};

// Functor to use as deleter for std::unique_ptr that stores a primary GRBenv,
// used by GRBenvUniquePtr. Most users will not use this directly.
struct GurobiFreeEnv {
  void operator()(GRBenv* const env) const;
};

// Unique pointer to a GRBenv. It destroys the environment on destruction
// calling GRBfreeenv. Most users will not use this directly.
using GRBenvUniquePtr = std::unique_ptr<GRBenv, GurobiFreeEnv>;

// Returns a new primary Gurobi environment, using the ISV key if provided, or a
// regular license otherwise. Gurobi::New() creates an environment automatically
// if not provided, so most users will not use this directly.
//
absl::StatusOr<GRBenvUniquePtr> GurobiNewPrimaryEnv(
    const std::optional<GurobiIsvKey>& isv_key = std::nullopt);

// Models and solves optimization problems with Gurobi.
//
// This is a thin wrapper on the Gurobi C API, holding a GRBmodel,
// associated GRBenv that GRBnewmodel creates, and optionally the primary
// environment to clean up on deletion.
//
// Throughout, we refer to the child GRBenv created by GRBnewmodel as the
// "model environment" while the GRBenv that was used to create the model as
// the "primary environment", for details see:
// https://www.gurobi.com/documentation/9.1/refman/c_newmodel.html
//
////////////////////////////////////////////////////////////////////////////////
// Attributes
////////////////////////////////////////////////////////////////////////////////
//
// Most properties of a Gurobi optimization model are set and read with
// attributes, using the attribute names defined in the Gurobi C API. There are
// scalar attributes returning a single value of the following types:
//  * int, e.g. GRB_INT_ATTR_MODELSENSE
//  * double, e.g. GRB_DBL_ATTR_OBJVAL
//  * string, e.g. GRB_STR_ATTR_MODELNAME
// and array attributes returning a list of values of the following types:
//  * int array, e.g. GRB_INT_ATTR_BRANCHPRIORITY
//  * double array, e.g. GRB_DBL_ATTR_LB
//  * char array, e.g. GRB_CHAR_ATTR_VTYPE
//
// You set a scalar attribute with the methods SetXXXAttr, e.g.
//   std::unique_ptr<Gurobi> gurobi = Gurobi::New().value();
//   absl::Status s = gurobi->SetIntAttr(GRB_INT_ATTR_MODELSENSE, 1);
// Note that not all attributes can be set; consult the Gurobi attribute docs.
//
// Attributes can also be read. However, attributes can be unavailable depending
// on the context, e.g. the solution objective value is not available before
// solving. You can determine when an attribute is available either from the
// Gurobi documentation or by directly testing:
//   std::unique_ptr<Gurobi> gurobi = Gurobi::New().value();
//   bool is_avail = gurobi->IsAttrAvailable(GRB_DBL_ATTR_OBJVAL);
// To read an attribute:
//   std::unique_ptr<Gurobi> gurobi = Gurobi::New().value();
//   absl::StatusOr<double> obj = gurobi->GetDoubleAttr(GRB_DBL_ATTR_OBJVAL);
// (The method *should* succeed when IsAttrAvailable() is true and you have
// specified the type of attribute correctly.)
//
// Array attributes are similar, but the API differs slightly. E.g. to set the
// first three variable lower bounds to 1.0:
//   std::unique_ptr<Gurobi> gurobi = Gurobi::New().value();
//   absl::Status s = gurobi->SetDoubleAttrArray(GRB_DBL_ATTR_LB, {1, 1, 1});
// You can also set specific indices, see SetDoubleAttrList. To read, use:
//   Gurobi* gurobi = ...;
//   int num_vars = ...;
//   absl::StatusOr<std::vector<double>> lbs =
//     gurobi->GetDoubleAttrArray(GRB_DBL_ATTR_LB, num_vars);
// An overload to write the result into an absl::Span is also provided.
//
// WARNING: as with the Gurobi C API, attributes cannot be read immediately
// after they have been set. You need to call UpdateModel() (which is called by
// Optimize()) before reading the model back. E.g.
//   std::unique_ptr<Gurobi> gurobi = Gurobi::New().value();
//   CHECK_OK(gurobi->AddVars({1, 1}, {0, 0}, {1, 1},
//                            {GRB_INTEGER, GRB_INTEGER}, {"x", "y"}));
//   int num_vars = gurobi->GetIntAttr(GRB_INT_ATTR_NUMVARS).value();  // Is 0.
//   CHECK_OK(gurobi->UpdateModel());
//   num_vars = gurobi->GetIntAttr(GRB_INT_ATTR_NUMVARS).value();  // Is now 2.
// Calls to UpdateModel() are expensive and should be minimized.
//
////////////////////////////////////////////////////////////////////////////////
// Parameters
////////////////////////////////////////////////////////////////////////////////
//
// Parameters are associated directly with Gurobi rather than a GRBenv as in the
// C API. Parameters have three types: int, double and string. You can get and
// set them by their C API names, e.g.
//   std::unique_ptr<Gurobi> gurobi = Gurobi::New().value();
//   gurobi->SetIntParam(GRB_INT_PAR_LOGTOCONSOLE, 1);
//   gurobi->GetIntParam(GRB_INT_PAR_LOGTOCONSOLE);  // Returns 1.
// Unlike attributes, values can be read immediately, no call to UpdateModel()
// is required.
class Gurobi {
 public:
  // A sparse matrix in compressed sparse column (CSC) format. E.g.
  //   [[2, 0, 4],
  //    [8, 6, 0]]
  // Would be {.begins={0, 2, 3}, .inds={0, 1, 1, 0}, .vals={2, 8, 6, 4}}
  struct SparseMat {
    // Has size equal to the number of columns, the index in inds where this
    // column begins.
    std::vector<int> begins;

    // Has size equal to the number of nonzeros in the matrix, the row for this
    // entry.
    std::vector<int> inds;

    // Has size equal to the number of nonzeros in the matrix, the value for
    // this entry.
    std::vector<double> vals;
  };

  // The argument of Gurobi callbacks, allows you to read callback specific
  // data and send information back to the solver.
  class CallbackContext {
   public:
    // For internal use only.
    CallbackContext(Gurobi* gurobi, void* cb_data, int where);

    // The current event of the callback, see Callback Codes in Gurobi docs.
    int where() const { return where_; }
    Gurobi* gurobi() const { return gurobi_; }

    // Calls GRBcbget() on "what" with result type int, see Callback Codes in
    // Gurobi docs for values of "what".
    absl::StatusOr<int> CbGetInt(int what) const;

    // Calls GRBcbget() on "what" with result type double, see Callback Codes in
    // Gurobi docs for values of "what".
    absl::StatusOr<double> CbGetDouble(int what) const;

    // Calls GRBcbget() on "what" with result type double*, see Callback Codes
    // in Gurobi docs for values of "what".
    //
    // The user is responsible for ensuring that result is large enough to hold
    // the result.
    absl::Status CbGetDoubleArray(int what, absl::Span<double> result) const;

    // Calls GRBcbget() where what=MSG_STRING (call only at where=MESSAGE).
    absl::StatusOr<std::string> CbGetMessage() const;

    // Calls GRBcbcut().
    absl::Status CbCut(absl::Span<const int> cutind,
                       absl::Span<const double> cutval, char cutsense,
                       double cutrhs) const;

    // Calls GRBcblazy().
    absl::Status CbLazy(absl::Span<const int> lazyind,
                        absl::Span<const double> lazyval, char lazysense,
                        double lazyrhs) const;

    // Calls GRBcbsolution().
    absl::StatusOr<double> CbSolution(absl::Span<const double> solution) const;

   private:
    Gurobi* const gurobi_;
    void* const cb_data_;
    const int where_;
  };

  // Invoked regularly by Gurobi while solving if provided as an argument to
  // Gurobi::Optimize(). If the user returns a status error in the callback:
  //  * Termination of the solve is requested.
  //  * The error is propagated to the return value of Gurobi::Optimize().
  //  * The callback will not be invoked again.
  using Callback = std::function<absl::Status(const CallbackContext&)>;

  // Creates a new Gurobi, taking ownership of primary_env if provided (if no
  // environment is given, a new one is created internally from the license
  // file).
  static absl::StatusOr<std::unique_ptr<Gurobi>> New(
      GRBenvUniquePtr primary_env = nullptr);

  // Creates a new Gurobi using an existing GRBenv, where primary_env cannot be
  // nullptr. Unlike Gurobi::New(), the returned Gurobi will not clean up the
  // primary environment on destruction.
  //
  // A GurobiEnv can be shared between models with the following restrictions:
  //   - Environments are not thread-safe (so use one thread or mutual exclusion
  //     for Gurobi::New()).
  //   - The primary environment must outlive each Gurobi instance.
  //   - Every "primary" environment counts as a "use" of a Gurobi License.
  //     Depending on your license type, you may need to share to run concurrent
  //     solves in the same process.
  static absl::StatusOr<std::unique_ptr<Gurobi>> NewWithSharedPrimaryEnv(
      GRBenv* primary_env);

  ~Gurobi();

  //////////////////////////////////////////////////////////////////////////////
  // Model Building
  //////////////////////////////////////////////////////////////////////////////

  // Calls GRBaddvar() to add a variable to the model.
  absl::Status AddVar(double obj, double lb, double ub, char vtype,
                      const std::string& name);

  // Calls GRBaddvar() to add a variable and linear constraint column to the
  // model.
  //
  // The inputs `vind` and `vval` must have the same size. Both can be empty if
  // you do not want to modify the constraint matrix, though this is equivalent
  // to the simpler overload above.
  absl::Status AddVar(absl::Span<const int> vind, absl::Span<const double> vval,
                      double obj, double lb, double ub, char vtype,
                      const std::string& name);

  // Calls GRBaddvars() to add variables to the model.
  //
  // Requirements:
  //  * lb, ub and vtype must have size equal to the number of new variables.
  //  * obj should either:
  //     - have size equal to the number of new variables,
  //     - be empty (all new variables have objective coefficient 0).
  //  * names should either:
  //     - have size equal to the number of new variables,
  //     - be empty (all new variables have name "").
  absl::Status AddVars(absl::Span<const double> obj,
                       absl::Span<const double> lb, absl::Span<const double> ub,
                       absl::Span<const char> vtype,
                       absl::Span<const std::string> names);

  // Calls GRBaddvars() to add variables and linear constraint columns to the
  // model.
  //
  // The new linear constraint matrix columns are given in CSC format (see
  // SparseMat above for an example).
  //
  // Requirements:
  //  * lb, ub and vtype must have size equal to the number of new variables.
  //  * obj should either:
  //     - have size equal to the number of new variables,
  //     - be empty (all new variables have objective coefficient 0).
  //  * names should either:
  //     - have size equal to the number of new variables,
  //     - be empty (all new variables have name "").
  //  * vbegin should have size equal to the number of new variables.
  //  * vind and vsize should have size equal to the number of new nonzeros in
  //    the linear constraint matrix.
  // Note: vbegin, vind and vval can all be empty if you do not want to modify
  // the constraint matrix, this is equivalent to the simpler overload above.
  absl::Status AddVars(absl::Span<const int> vbegin, absl::Span<const int> vind,
                       absl::Span<const double> vval,
                       absl::Span<const double> obj,
                       absl::Span<const double> lb, absl::Span<const double> ub,
                       absl::Span<const char> vtype,
                       absl::Span<const std::string> names);

  // Calls GRBdelvars().
  absl::Status DelVars(absl::Span<const int> ind);

  // Calls GRBaddconstr() to add a constraint to the model.
  //
  // This overload does not add any variable coefficients to the constraint.
  absl::Status AddConstr(char sense, double rhs, const std::string& name);

  // Calls GRBaddconstr() to add a constraint to the model.
  //
  // The inputs `cind` and `cval` must have the same size.
  absl::Status AddConstr(absl::Span<const int> cind,
                         absl::Span<const double> cval, char sense, double rhs,
                         const std::string& name);

  // Calls GRBaddconstrs().
  //
  // Requirements:
  //  * sense and rhs must have size equal to the number of new constraints.
  //  * names should either:
  //     - have size equal to the number of new constraints,
  //     - be empty (all new constraints have name "").
  absl::Status AddConstrs(absl::Span<const char> sense,
                          absl::Span<const double> rhs,
                          absl::Span<const std::string> names);

  // Calls GRBdelconstrs().
  absl::Status DelConstrs(absl::Span<const int> ind);

  // Calls GRBchgcoeffs().
  //
  // Requirements:
  //  * cind, vind, and val have size equal to the number of changed constraint
  //    matrix entries.
  absl::Status ChgCoeffs(absl::Span<const int> cind, absl::Span<const int> vind,
                         absl::Span<const double> val);

  // Calls GRBaddqpterms().
  //
  // Requirements:
  //  * qrow, qcol, and qval have size equal to the number of new quadratic
  //    objective terms.
  absl::Status AddQpTerms(absl::Span<const int> qrow,
                          absl::Span<const int> qcol,
                          absl::Span<const double> qval);

  // Calls GRBdelq().
  //
  // Deletes all quadratic objective coefficients.
  absl::Status DelQ();

  // Calls GRBsetobjectiven().
  //
  // Sets the n-th objective in a multi-objective model.
  //
  // Requirement:
  //  * lind and lval must be of equal length.
  absl::Status SetNthObjective(int index, int priority, double weight,
                               double abs_tol, double rel_tol,
                               const std::string& name, double constant,
                               absl::Span<const int> lind,
                               absl::Span<const double> lval);

  // Calls GRBaddqconstr().
  //
  // Requirements:
  //  * lind and lval must be equal length.
  //  * qrow, qcol, and qval must be equal length.
  absl::Status AddQConstr(absl::Span<const int> lind,
                          absl::Span<const double> lval,
                          absl::Span<const int> qrow,
                          absl::Span<const int> qcol,
                          absl::Span<const double> qval, char sense, double rhs,
                          const std::string& name);

  // Calls GRBdelqconstrs().
  //
  // Deletes the specified quadratic constraints.
  absl::Status DelQConstrs(const absl::Span<const int> ind);

  // Calls GRBaddsos().
  //
  // This adds SOS constraints to the model. You may specify multiple SOS
  // constraints at once, and may mix the types (SOS1 and SOS2) in a single
  // call. The data is specified in CSR format, meaning that the entries of beg
  // indicate the contiguous subranges of ind and weight associated with a
  // particular SOS constraint. Please see the Gurobi documentation for more
  // detail (https://www.gurobi.com/documentation/9.5/refman/c_addsos.html).
  //
  // Requirements:
  //  * types and beg must be of equal length.
  //  * ind and weight must be of equal length.
  absl::Status AddSos(absl::Span<const int> types, absl::Span<const int> beg,
                      absl::Span<const int> ind,
                      absl::Span<const double> weight);

  // Calls GRBdelsos().
  //
  // Deletes the specified SOS constraints.
  absl::Status DelSos(absl::Span<const int> ind);

  // Calls GRBaddgenconstrIndicator().
  //
  // `ind` and `val` must be of equal length.
  absl::Status AddIndicator(const std::string& name, int binvar, int binval,
                            absl::Span<const int> ind,
                            absl::Span<const double> val, char sense,
                            double rhs);

  // Calls GRBdelgenconstrs().
  //
  // Deletes the specified general constraints.
  absl::Status DelGenConstrs(absl::Span<const int> ind);

  //////////////////////////////////////////////////////////////////////////////
  // Linear constraint matrix queries.
  //////////////////////////////////////////////////////////////////////////////

  // Calls GRBgetvars().
  //
  // The number of nonzeros in the constraint matrix for the num_vars columns
  // starting with first_var.
  //
  // Warning: will not reflect pending modifications, call UpdateModel() or
  // Optimize() first.
  absl::StatusOr<int> GetNnz(int first_var, int num_vars);

  // Calls GRBgetvars().
  //
  // Write the nonzeros of the constraint matrix for the num_vars columns
  // starting with first_var out in CSC format to (vbegin, vind, vval).
  //
  // The user is responsible for ensuring that the output Spans are exactly
  // the correct size. See the other GetVars() overload for a simpler version.
  //
  // Warning: will not reflect pending modifications, call UpdateModel() or
  // Optimize() first.
  absl::Status GetVars(absl::Span<int> vbegin, absl::Span<int> vind,
                       absl::Span<double> vval, int first_var, int num_vars);

  // Calls GRBgetvars().
  //
  // Returns the nonzeros of the constraint matrix for the num_vars columns
  // starting with first_var out in CSC format.
  //
  // Warning: will not reflect pending modifications, call UpdateModel() or
  // Optimize() first.
  absl::StatusOr<SparseMat> GetVars(int first_var, int num_vars);

  //////////////////////////////////////////////////////////////////////////////
  // Solving
  //////////////////////////////////////////////////////////////////////////////

  // Calls GRBupdatemodel().
  absl::Status UpdateModel();

  // Calls GRBoptimize().
  //
  // The callback, if specified, is set before solving and cleared after.
  absl::Status Optimize(Callback cb = nullptr);

  // Calls GRBterminate().
  void Terminate();

  //////////////////////////////////////////////////////////////////////////////
  // Attributes
  //////////////////////////////////////////////////////////////////////////////

  bool IsAttrAvailable(const char* name) const;

  absl::StatusOr<int> GetIntAttr(const char* name) const;
  absl::Status SetIntAttr(const char* attr_name, int value);

  absl::StatusOr<double> GetDoubleAttr(const char* name) const;
  absl::Status SetDoubleAttr(const char* attr_name, double value);

  absl::StatusOr<std::string> GetStringAttr(const char* name) const;
  absl::Status SetStringAttr(const char* attr_name, const std::string& value);

  absl::Status GetIntAttrArray(const char* name,
                               absl::Span<int> attr_out) const;
  absl::StatusOr<std::vector<int>> GetIntAttrArray(const char* name,
                                                   int len) const;
  absl::Status SetIntAttrArray(const char* name,
                               absl::Span<const int> new_values);
  absl::Status SetIntAttrList(const char* name, absl::Span<const int> ind,
                              absl::Span<const int> new_values);

  absl::Status GetDoubleAttrArray(const char* name,
                                  absl::Span<double> attr_out) const;
  absl::StatusOr<std::vector<double>> GetDoubleAttrArray(const char* name,
                                                         int len) const;
  absl::Status SetDoubleAttrArray(const char* name,
                                  absl::Span<const double> new_values);
  absl::Status SetDoubleAttrList(const char* name, absl::Span<const int> ind,
                                 absl::Span<const double> new_values);

  absl::Status GetCharAttrArray(const char* name,
                                absl::Span<char> attr_out) const;
  absl::StatusOr<std::vector<char>> GetCharAttrArray(const char* name,
                                                     int len) const;
  absl::Status SetCharAttrArray(const char* name,
                                absl::Span<const char> new_values);
  absl::Status SetCharAttrList(const char* name, absl::Span<const int> ind,
                               absl::Span<const char> new_values);

  absl::StatusOr<double> GetDoubleAttrElement(const char* name,
                                              int element) const;
  absl::Status SetDoubleAttrElement(const char* name, int element,
                                    double new_value);

  absl::StatusOr<char> GetCharAttrElement(const char* name, int element) const;
  absl::Status SetCharAttrElement(const char* name, int element,
                                  char new_value);

  //////////////////////////////////////////////////////////////////////////////
  // Parameters
  //////////////////////////////////////////////////////////////////////////////

  // Calls GRBsetparam().
  //
  // Prefer the typed versions (e.g. SetIntParam()) defined below.
  absl::Status SetParam(const char* name, const std::string& value);

  // Calls GRBsetintparam().
  absl::Status SetIntParam(const char* name, int value);

  // Calls GRBsetdblparam().
  absl::Status SetDoubleParam(const char* name, double value);

  // Calls GRBsetstrparam().
  absl::Status SetStringParam(const char* name, const std::string& value);

  // Calls GRBgetintparam().
  absl::StatusOr<int> GetIntParam(const char* name);

  // Calls GRBgetdblparam().
  absl::StatusOr<double> GetDoubleParam(const char* name);

  // Calls GRBgetstrparam().
  absl::StatusOr<std::string> GetStringParam(const char* name);

  // Calls GRBresetparams().
  absl::Status ResetParameters();

  // Typically not needed.
  GRBmodel* model() const { return gurobi_model_; }

 private:
  // optional_owned_primary_env can be null, model and model_env cannot.
  Gurobi(GRBenvUniquePtr optional_owned_primary_env, GRBmodel* model,
         GRBenv* model_env);
  // optional_owned_primary_env can be null, primary_env cannot.
  static absl::StatusOr<std::unique_ptr<Gurobi>> New(
      GRBenvUniquePtr optional_owned_primary_env, GRBenv* primary_env);

  absl::Status ToStatus(
      int grb_err, absl::StatusCode code = absl::StatusCode::kInvalidArgument,
      absl::SourceLocation loc = absl::SourceLocation::current()) const;

  const GRBenvUniquePtr owned_primary_env_;
  // Invariant: Not null.
  GRBmodel* const gurobi_model_;
  // Invariant: Not null. This is the environment created by GRBnewmodel(), not
  // the primary environment used to create a GRBmodel, see class documentation.
  GRBenv* const model_env_;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_G_GUROBI_H_
