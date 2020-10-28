// Copyright 2010-2018 Google LLC
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

#include "ortools/linear_solver/gurobi_environment.h"

#include <string>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
absl::Status LoadGurobiEnvironment(GRBenv** env) {
  constexpr int GRB_OK = 0;
  const char kGurobiEnvErrorMsg[] =
      "Could not load Gurobi environment. Is gurobi correctly installed and "
      "licensed on this machine?";

  if (GRBloadenv(env, nullptr) != 0 || *env == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s %s", kGurobiEnvErrorMsg, GRBgeterrormsg(*env)));
  }
  return absl::OkStatus();
}

std::function<int(GRBmodel *, int, int *, double *, double, double,
                  const char *)>
    GRBaddrangeconstr = nullptr;
std::function<int(GRBmodel *model, int numnz, int *vind, double *vval,
                  double obj, double lb, double ub, char vtype,
                  const char *varname)>
    GRBaddvar = nullptr;
std::function<int(GRBmodel *, int, int, int *, int *, double *, double *,
                  double *, double *, char *, char **)>
    GRBaddvars = nullptr;
std::function<int(GRBmodel *model, int numchgs, int *cind, int *vind,
                  double *val)>
    GRBchgcoeffs = nullptr;
std::function<void(GRBenv *)> GRBfreeenv = nullptr;
std::function<int(GRBmodel *)> GRBfreemodel = nullptr;
std::function<int(GRBmodel *, const char *, int, char *)>
    GRBgetcharattrelement = nullptr;
std::function<int(GRBmodel *, const char *, double *)> GRBgetdblattr = nullptr;
std::function<int(GRBmodel *, const char *, int, int, double *)>
    GRBgetdblattrarray = nullptr;
std::function<int(GRBmodel *, const char *, int, double *)>
    GRBgetdblattrelement = nullptr;
std::function<int(GRBenv *, const char *, double *)> GRBgetdblparam = nullptr;
std::function<GRBenv *(GRBmodel *)> GRBgetenv = nullptr;
std::function<char *(GRBenv *)> GRBgeterrormsg = nullptr;
std::function<int(GRBmodel *, const char *, int *)> GRBgetintattr = nullptr;
std::function<int(GRBmodel *, const char *, int, int *)> GRBgetintattrelement =
    nullptr;
std::function<int(GRBenv **, const char *)> GRBloadenv = nullptr;
std::function<int(GRBenv *, GRBmodel **, const char *, int numvars, double *,
                  double *, double *, char *, char **)>
    GRBnewmodel = nullptr;
std::function<int(GRBmodel *)> GRBoptimize = nullptr;
std::function<int(GRBenv *, const char *)> GRBreadparams = nullptr;
std::function<int(GRBenv *)> GRBresetparams = nullptr;
std::function<int(GRBmodel *, const char *, int, char)> GRBsetcharattrelement =
    nullptr;
std::function<int(GRBmodel *, const char *, double)> GRBsetdblattr = nullptr;
std::function<int(GRBmodel *, const char *, int, double)> GRBsetdblattrelement =
    nullptr;
std::function<int(GRBenv *, const char *, double)> GRBsetdblparam = nullptr;
std::function<int(GRBmodel *, const char *, int)> GRBsetintattr = nullptr;
std::function<int(GRBenv *, const char *, int)> GRBsetintparam = nullptr;
std::function<void(GRBmodel *)> GRBterminate = nullptr;
std::function<int(GRBmodel *)> GRBupdatemodel = nullptr;
std::function<void(int *, int *, int *)> GRBversion = nullptr;
std::function<int(GRBmodel *, const char *)> GRBwrite = nullptr;
std::function<int(void *cbdata, int where, int what, void *resultP)> GRBcbget =
    nullptr;
std::function<int(void *cbdata, int cutlen, const int *cutind,
                  const double *cutval, char cutsense, double cutrhs)>
    GRBcbcut = nullptr;
std::function<int(void *cbdata, int lazylen, const int *lazyind,
                  const double *lazyval, char lazysense, double lazyrhs)>
    GRBcblazy = nullptr;
std::function<int(void *cbdata, const double *solution, double *objvalP)>
    GRBcbsolution = nullptr;
std::function<int(GRBmodel *model, int numnz, int *cind, double *cval,
                  char sense, double rhs, const char *constrname)>
    GRBaddconstr = nullptr;
std::function<int(GRBmodel *model, const char *name, int binvar, int binval,
                  int nvars, const int *vars, const double *vals, char sense,
                  double rhs)>
    GRBaddgenconstrIndicator = nullptr;
std::function<int(GRBmodel *model, const char *attrname, int element,
                  int newvalue)>
    GRBsetintattrelement = nullptr;
std::function<int(GRBmodel *model, int(STDCALL *cb)(CB_ARGS), void *usrdata)>
    GRBsetcallbackfunc = nullptr;
std::function<int(GRBenv *env, const char *paramname, const char *value)>
    GRBsetparam = nullptr;
std::function<int(GRBmodel *model, int numsos, int nummembers, int *types,
                  int *beg, int *ind, double *weight)>
    GRBaddsos = nullptr;
std::function<int(GRBmodel *model, int numlnz, int *lind, double *lval,
                  int numqnz, int *qrow, int *qcol, double *qval, char sense,
                  double rhs, const char *QCname)>
    GRBaddqconstr = nullptr;
std::function<int(GRBmodel *model, const char *name, int resvar, int nvars,
                  const int *vars, double constant)>
    GRBaddgenconstrMax = nullptr;
std::function<int(GRBmodel *model, const char *name, int resvar, int nvars,
                  const int *vars, double constant)>
    GRBaddgenconstrMin = nullptr;
std::function<int(GRBmodel *model, const char *name, int resvar, int argvar)>
    GRBaddgenconstrAbs = nullptr;
std::function<int(GRBmodel *model, const char *name, int resvar, int nvars,
                  const int *vars)>
    GRBaddgenconstrAnd = nullptr;
std::function<int(GRBmodel *model, const char *name, int resvar, int nvars,
                  const int *vars)>
    GRBaddgenconstrOr = nullptr;
std::function<int(GRBmodel *model, int numqnz, int *qrow, int *qcol,
                  double *qval)>
    GRBaddqpterms = nullptr;

std::unique_ptr<DynamicLibrary> gurobi_dynamic_library;
std::string gurobi_library_path;

void LoadGurobiFunctions() {
  gurobi_dynamic_library->GetFunction(&GRBaddrangeconstr,
                                      NAMEOF(GRBaddrangeconstr));
  gurobi_dynamic_library->GetFunction(&GRBaddvar, NAMEOF(GRBaddvar));
  gurobi_dynamic_library->GetFunction(&GRBaddvars, NAMEOF(GRBaddvars));
  gurobi_dynamic_library->GetFunction(&GRBchgcoeffs, NAMEOF(GRBchgcoeffs));
  gurobi_dynamic_library->GetFunction(&GRBfreeenv, NAMEOF(GRBfreeenv));
  gurobi_dynamic_library->GetFunction(&GRBfreemodel, NAMEOF(GRBfreemodel));
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrelement,
                                      NAMEOF(GRBgetcharattrelement));
  gurobi_dynamic_library->GetFunction(&GRBgetdblattr, NAMEOF(GRBgetdblattr));
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrarray,
                                      NAMEOF(GRBgetdblattrarray));
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrelement,
                                      NAMEOF(GRBgetdblattrelement));
  gurobi_dynamic_library->GetFunction(&GRBgetdblparam, NAMEOF(GRBgetdblparam));
  gurobi_dynamic_library->GetFunction(&GRBgetenv, NAMEOF(GRBgetenv));
  gurobi_dynamic_library->GetFunction(&GRBgeterrormsg, NAMEOF(GRBgeterrormsg));
  gurobi_dynamic_library->GetFunction(&GRBgetintattr, NAMEOF(GRBgetintattr));
  gurobi_dynamic_library->GetFunction(&GRBgetintattrelement,
                                      NAMEOF(GRBgetintattrelement));
  gurobi_dynamic_library->GetFunction(&GRBloadenv, NAMEOF(GRBloadenv));
  gurobi_dynamic_library->GetFunction(&GRBnewmodel, NAMEOF(GRBnewmodel));
  gurobi_dynamic_library->GetFunction(&GRBoptimize, NAMEOF(GRBoptimize));
  gurobi_dynamic_library->GetFunction(&GRBreadparams, NAMEOF(GRBreadparams));
  gurobi_dynamic_library->GetFunction(&GRBresetparams, NAMEOF(GRBresetparams));
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrelement,
                                      NAMEOF(GRBsetcharattrelement));
  gurobi_dynamic_library->GetFunction(&GRBsetdblattr, NAMEOF(GRBsetdblattr));
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrelement,
                                      NAMEOF(GRBsetdblattrelement));
  gurobi_dynamic_library->GetFunction(&GRBsetdblparam, NAMEOF(GRBsetdblparam));
  gurobi_dynamic_library->GetFunction(&GRBsetintattr, NAMEOF(GRBsetintattr));
  gurobi_dynamic_library->GetFunction(&GRBsetintparam, NAMEOF(GRBsetintparam));
  gurobi_dynamic_library->GetFunction(&GRBterminate, NAMEOF(GRBterminate));
  gurobi_dynamic_library->GetFunction(&GRBupdatemodel, NAMEOF(GRBupdatemodel));
  gurobi_dynamic_library->GetFunction(&GRBversion, NAMEOF(GRBversion));
  gurobi_dynamic_library->GetFunction(&GRBwrite, NAMEOF(GRBwrite));
  gurobi_dynamic_library->GetFunction(&GRBcbget, NAMEOF(GRBcbget));
  gurobi_dynamic_library->GetFunction(&GRBcbcut, NAMEOF(GRBcbcut));
  gurobi_dynamic_library->GetFunction(&GRBcblazy, NAMEOF(GRBcblazy));
  gurobi_dynamic_library->GetFunction(&GRBcbsolution, NAMEOF(GRBcbsolution));
  gurobi_dynamic_library->GetFunction(&GRBaddconstr, NAMEOF(GRBaddconstr));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrIndicator,
                                      NAMEOF(GRBaddgenconstrIndicator));
  gurobi_dynamic_library->GetFunction(&GRBsetintattrelement,
                                      NAMEOF(GRBsetintattrelement));
  gurobi_dynamic_library->GetFunction(&GRBsetcallbackfunc,
                                      NAMEOF(GRBsetcallbackfunc));
  gurobi_dynamic_library->GetFunction(&GRBsetparam, NAMEOF(GRBsetparam));
  gurobi_dynamic_library->GetFunction(&GRBaddsos, NAMEOF(GRBaddsos));
  gurobi_dynamic_library->GetFunction(&GRBaddqconstr, NAMEOF(GRBaddqconstr));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrMax,
                                      NAMEOF(GRBaddgenconstrMax));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrMin,
                                      NAMEOF(GRBaddgenconstrMin));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrAbs,
                                      NAMEOF(GRBaddgenconstrAbs));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrAnd,
                                      NAMEOF(GRBaddgenconstrAnd));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrOr,
                                      NAMEOF(GRBaddgenconstrOr));
  gurobi_dynamic_library->GetFunction(&GRBaddqpterms, NAMEOF(GRBaddqpterms));
}

bool LoadSpecificGurobiLibrary(const std::string &full_library_path) {
  CHECK(gurobi_dynamic_library.get() != nullptr);
  VLOG(1) << "Try to load from " << full_library_path;
  return gurobi_dynamic_library->TryToLoad(full_library_path);
}

bool SearchForGurobiDynamicLibrary() {
  const char *gurobi_home_from_env = getenv("GUROBI_HOME");
#if defined(_MSC_VER)  // Windows
  if (!gurobi_library_path.empty() &&
      LoadSpecificGurobiLibrary(gurobi_library_path)) {
    return true;
  }
  if (gurobi_home_from_env != nullptr &&
      LoadSpecificGurobiLibrary(
          absl::StrCat(gurobi_home_from_env, "\\bin\\gurobi90.dll"))) {
    return true;
  }
  if (LoadSpecificGurobiLibrary(
          "C:\\Program Files\\gurobi902\\win64\\bin\\gurobi90.dll")) {
    return true;
  }
#elif defined(__APPLE__)  // OS X
  if (!gurobi_library_path.empty() &&
      LoadSpecificGurobiLibrary(gurobi_library_path)) {
    return true;
  }
  if (gurobi_home_from_env != nullptr &&
      LoadSpecificGurobiLibrary(
          absl::StrCat(gurobi_home_from_env, "/lib/libgurobi90.dylib"))) {
    return true;
  }
  if (LoadSpecificGurobiLibrary(
          "/Library/gurobi902/mac64/lib/libgurobi90.dylib")) {
    return true;
  }
#elif defined(__GNUC__)   // Linux
  if (gurobi_home_from_env != nullptr &&
      LoadSpecificGurobiLibrary(
          absl::StrCat(gurobi_home_from_env, "/lib/libgurobi90.so"))) {
    return true;
  }
  if (!gurobi_library_path.empty() &&
      LoadSpecificGurobiLibrary(gurobi_library_path)) {
    return true;
  }
#endif

  return false;
}

bool MPSolver::LoadGurobiSharedLibrary() {
  if (gurobi_dynamic_library.get() != nullptr) {
    return gurobi_dynamic_library->LibraryIsLoaded();
  }

  gurobi_dynamic_library.reset(new DynamicLibrary());

  if (SearchForGurobiDynamicLibrary()) {
    LoadGurobiFunctions();
    return true;
  }

  return false;
}

void MPSolver::SetGurobiLibraryPath(const std::string &full_library_path) {
  gurobi_library_path = full_library_path;
}

bool MPSolver::GurobiIsCorrectlyInstalled() {
  if (!LoadGurobiSharedLibrary()) return false;
  GRBenv *env;
  if (GRBloadenv(&env, nullptr) != 0 || env == nullptr) return false;

  return true;
}

}  // namespace operations_research
