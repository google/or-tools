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

#include "ortools/gurobi/environment.h"

#include <mutex>
#include <string>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"

namespace operations_research {

bool GurobiIsCorrectlyInstalled() {
  absl::StatusOr<GRBenv*> status = GetGurobiEnv();
  if (!status.ok() || status.value() == nullptr) {
    LOG(WARNING) << status.status();
    return false;
  }

  GRBfreeenv(status.value());

  return true;
}

// This was generated with the parse_header.py script.
// See the comment at the top of the script.

// This is the 'define' section.
std::function<int(GRBmodel* model, const char* attrname)> GRBisattravailable =
    nullptr;
std::function<int(GRBmodel* model, const char* attrname, int* valueP)>
    GRBgetintattr = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int newvalue)>
    GRBsetintattr = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  int* valueP)>
    GRBgetintattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  int newvalue)>
    GRBsetintattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  int* values)>
    GRBgetintattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  int* newvalues)>
    GRBsetintattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int len, int* ind,
                  int* newvalues)>
    GRBsetintattrlist = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  char* valueP)>
    GRBgetcharattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  char newvalue)>
    GRBsetcharattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  char* values)>
    GRBgetcharattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  char* newvalues)>
    GRBsetcharattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int len, int* ind,
                  char* newvalues)>
    GRBsetcharattrlist = nullptr;
std::function<int(GRBmodel* model, const char* attrname, double* valueP)>
    GRBgetdblattr = nullptr;
std::function<int(GRBmodel* model, const char* attrname, double newvalue)>
    GRBsetdblattr = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  double* valueP)>
    GRBgetdblattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  double newvalue)>
    GRBsetdblattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  double* values)>
    GRBgetdblattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  double* newvalues)>
    GRBsetdblattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int len, int* ind,
                  double* newvalues)>
    GRBsetdblattrlist = nullptr;
std::function<int(GRBmodel* model, const char* attrname, char** valueP)>
    GRBgetstrattr = nullptr;
std::function<int(GRBmodel* model, const char* attrname, const char* newvalue)>
    GRBsetstrattr = nullptr;
std::function<int(GRBmodel* model, int(GUROBI_STDCALL* cb)(CB_ARGS),
                  void* usrdata)>
    GRBsetcallbackfunc = nullptr;
std::function<int(void* cbdata, int where, int what, void* resultP)> GRBcbget =
    nullptr;
std::function<int(void* cbdata, const double* solution, double* objvalP)>
    GRBcbsolution = nullptr;
std::function<int(void* cbdata, int cutlen, const int* cutind,
                  const double* cutval, char cutsense, double cutrhs)>
    GRBcbcut = nullptr;
std::function<int(void* cbdata, int lazylen, const int* lazyind,
                  const double* lazyval, char lazysense, double lazyrhs)>
    GRBcblazy = nullptr;
std::function<int(GRBmodel* model, int* numnzP, int* vbeg, int* vind,
                  double* vval, int start, int len)>
    GRBgetvars = nullptr;
std::function<int(GRBmodel* model)> GRBoptimize = nullptr;
std::function<int(GRBmodel* model)> GRBcomputeIIS = nullptr;
std::function<int(GRBmodel* model, const char* filename)> GRBwrite = nullptr;
std::function<int(GRBenv* env, GRBmodel** modelP, const char* Pname,
                  int numvars, double* obj, double* lb, double* ub, char* vtype,
                  char** varnames)>
    GRBnewmodel = nullptr;
std::function<int(GRBmodel* model, int numnz, int* vind, double* vval,
                  double obj, double lb, double ub, char vtype,
                  const char* varname)>
    GRBaddvar = nullptr;
std::function<int(GRBmodel* model, int numvars, int numnz, int* vbeg, int* vind,
                  double* vval, double* obj, double* lb, double* ub,
                  char* vtype, char** varnames)>
    GRBaddvars = nullptr;
std::function<int(GRBmodel* model, int numnz, int* cind, double* cval,
                  char sense, double rhs, const char* constrname)>
    GRBaddconstr = nullptr;
std::function<int(GRBmodel* model, int numconstrs, int numnz, int* cbeg,
                  int* cind, double* cval, char* sense, double* rhs,
                  char** constrnames)>
    GRBaddconstrs = nullptr;
std::function<int(GRBmodel* model, int numnz, int* cind, double* cval,
                  double lower, double upper, const char* constrname)>
    GRBaddrangeconstr = nullptr;
std::function<int(GRBmodel* model, int numsos, int nummembers, int* types,
                  int* beg, int* ind, double* weight)>
    GRBaddsos = nullptr;
std::function<int(GRBmodel* model, const char* name, int resvar, int nvars,
                  const int* vars, double constant)>
    GRBaddgenconstrMax = nullptr;
std::function<int(GRBmodel* model, const char* name, int resvar, int nvars,
                  const int* vars, double constant)>
    GRBaddgenconstrMin = nullptr;
std::function<int(GRBmodel* model, const char* name, int resvar, int argvar)>
    GRBaddgenconstrAbs = nullptr;
std::function<int(GRBmodel* model, const char* name, int resvar, int nvars,
                  const int* vars)>
    GRBaddgenconstrAnd = nullptr;
std::function<int(GRBmodel* model, const char* name, int resvar, int nvars,
                  const int* vars)>
    GRBaddgenconstrOr = nullptr;
std::function<int(GRBmodel* model, const char* name, int binvar, int binval,
                  int nvars, const int* vars, const double* vals, char sense,
                  double rhs)>
    GRBaddgenconstrIndicator = nullptr;
std::function<int(GRBmodel* model, int numlnz, int* lind, double* lval,
                  int numqnz, int* qrow, int* qcol, double* qval, char sense,
                  double rhs, const char* QCname)>
    GRBaddqconstr = nullptr;
std::function<int(GRBmodel* model, int numqnz, int* qrow, int* qcol,
                  double* qval)>
    GRBaddqpterms = nullptr;
std::function<int(GRBmodel* model, int len, int* ind)> GRBdelvars = nullptr;
std::function<int(GRBmodel* model, int len, int* ind)> GRBdelconstrs = nullptr;
std::function<int(GRBmodel* model, int len, int* ind)> GRBdelsos = nullptr;
std::function<int(GRBmodel* model, int len, int* ind)> GRBdelgenconstrs =
    nullptr;
std::function<int(GRBmodel* model, int len, int* ind)> GRBdelqconstrs = nullptr;
std::function<int(GRBmodel* model)> GRBdelq = nullptr;
std::function<int(GRBmodel* model, int cnt, int* cind, int* vind, double* val)>
    GRBchgcoeffs = nullptr;
std::function<int(GRBmodel* model)> GRBupdatemodel = nullptr;
std::function<int(GRBmodel* model)> GRBfreemodel = nullptr;
std::function<void(GRBmodel* model)> GRBterminate = nullptr;
std::function<int(GRBmodel* model, int index, int priority, double weight,
                  double abstol, double reltol, const char* name,
                  double constant, int lnz, int* lind, double* lval)>
    GRBsetobjectiven = nullptr;
std::function<int(GRBenv* env, const char* paramname, int* valueP)>
    GRBgetintparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, double* valueP)>
    GRBgetdblparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, char* valueP)>
    GRBgetstrparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, int* valueP, int* minP,
                  int* maxP, int* defP)>
    GRBgetintparaminfo = nullptr;
std::function<int(GRBenv* env, const char* paramname, double* valueP,
                  double* minP, double* maxP, double* defP)>
    GRBgetdblparaminfo = nullptr;
std::function<int(GRBenv* env, const char* paramname, char* valueP, char* defP)>
    GRBgetstrparaminfo = nullptr;
std::function<int(GRBenv* env, const char* paramname)> GRBgetparamtype =
    nullptr;
std::function<int(GRBenv* env, int i, char** paramnameP)> GRBgetparamname =
    nullptr;
std::function<int(GRBenv* env, const char* paramname, const char* value)>
    GRBsetparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, int value)>
    GRBsetintparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, double value)>
    GRBsetdblparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, const char* value)>
    GRBsetstrparam = nullptr;
std::function<int(GRBenv* env)> GRBresetparams = nullptr;
std::function<int(GRBenv* dest, GRBenv* src)> GRBcopyparams = nullptr;
std::function<int(GRBenv* env)> GRBgetnumparams = nullptr;
std::function<int(GRBenv** envP)> GRBemptyenv = nullptr;
std::function<int(GRBenv** envP, const char* logfilename)> GRBloadenv = nullptr;
std::function<int(GRBenv* env)> GRBstartenv = nullptr;
std::function<GRBenv*(GRBmodel* model)> GRBgetenv = nullptr;
std::function<GRBenv*(GRBmodel* model, int num)> GRBgetmultiobjenv = nullptr;
std::function<GRBenv*(GRBmodel* model)> GRBdiscardmultiobjenvs = nullptr;
std::function<void(GRBenv* env)> GRBfreeenv = nullptr;
std::function<const char*(GRBenv* env)> GRBgeterrormsg = nullptr;
std::function<void(int* majorP, int* minorP, int* technicalP)> GRBversion =
    nullptr;
std::function<char*(void)> GRBplatform = nullptr;

void LoadGurobiFunctions(DynamicLibrary* gurobi_dynamic_library) {
  // This was generated with the parse_header.py script.
  // See the comment at the top of the script.

  // This is the 'assign' section.
  gurobi_dynamic_library->GetFunction(&GRBisattravailable,
                                      "GRBisattravailable");
  gurobi_dynamic_library->GetFunction(&GRBgetintattr, "GRBgetintattr");
  gurobi_dynamic_library->GetFunction(&GRBsetintattr, "GRBsetintattr");
  gurobi_dynamic_library->GetFunction(&GRBgetintattrelement,
                                      "GRBgetintattrelement");
  gurobi_dynamic_library->GetFunction(&GRBsetintattrelement,
                                      "GRBsetintattrelement");
  gurobi_dynamic_library->GetFunction(&GRBgetintattrarray,
                                      "GRBgetintattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetintattrarray,
                                      "GRBsetintattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetintattrlist, "GRBsetintattrlist");
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrelement,
                                      "GRBgetcharattrelement");
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrelement,
                                      "GRBsetcharattrelement");
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrarray,
                                      "GRBgetcharattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrarray,
                                      "GRBsetcharattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrlist,
                                      "GRBsetcharattrlist");
  gurobi_dynamic_library->GetFunction(&GRBgetdblattr, "GRBgetdblattr");
  gurobi_dynamic_library->GetFunction(&GRBsetdblattr, "GRBsetdblattr");
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrelement,
                                      "GRBgetdblattrelement");
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrelement,
                                      "GRBsetdblattrelement");
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrarray,
                                      "GRBgetdblattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrarray,
                                      "GRBsetdblattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrlist, "GRBsetdblattrlist");
  gurobi_dynamic_library->GetFunction(&GRBgetstrattr, "GRBgetstrattr");
  gurobi_dynamic_library->GetFunction(&GRBsetstrattr, "GRBsetstrattr");
  gurobi_dynamic_library->GetFunction(&GRBsetcallbackfunc,
                                      "GRBsetcallbackfunc");
  gurobi_dynamic_library->GetFunction(&GRBcbget, "GRBcbget");
  gurobi_dynamic_library->GetFunction(&GRBcbsolution, "GRBcbsolution");
  gurobi_dynamic_library->GetFunction(&GRBcbcut, "GRBcbcut");
  gurobi_dynamic_library->GetFunction(&GRBcblazy, "GRBcblazy");
  gurobi_dynamic_library->GetFunction(&GRBgetvars, "GRBgetvars");
  gurobi_dynamic_library->GetFunction(&GRBoptimize, "GRBoptimize");
  gurobi_dynamic_library->GetFunction(&GRBcomputeIIS, "GRBcomputeIIS");
  gurobi_dynamic_library->GetFunction(&GRBwrite, "GRBwrite");
  gurobi_dynamic_library->GetFunction(&GRBnewmodel, "GRBnewmodel");
  gurobi_dynamic_library->GetFunction(&GRBaddvar, "GRBaddvar");
  gurobi_dynamic_library->GetFunction(&GRBaddvars, "GRBaddvars");
  gurobi_dynamic_library->GetFunction(&GRBaddconstr, "GRBaddconstr");
  gurobi_dynamic_library->GetFunction(&GRBaddconstrs, "GRBaddconstrs");
  gurobi_dynamic_library->GetFunction(&GRBaddrangeconstr, "GRBaddrangeconstr");
  gurobi_dynamic_library->GetFunction(&GRBaddsos, "GRBaddsos");
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrMax,
                                      "GRBaddgenconstrMax");
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrMin,
                                      "GRBaddgenconstrMin");
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrAbs,
                                      "GRBaddgenconstrAbs");
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrAnd,
                                      "GRBaddgenconstrAnd");
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrOr, "GRBaddgenconstrOr");
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrIndicator,
                                      "GRBaddgenconstrIndicator");
  gurobi_dynamic_library->GetFunction(&GRBaddqconstr, "GRBaddqconstr");
  gurobi_dynamic_library->GetFunction(&GRBaddqpterms, "GRBaddqpterms");
  gurobi_dynamic_library->GetFunction(&GRBdelvars, "GRBdelvars");
  gurobi_dynamic_library->GetFunction(&GRBdelconstrs, "GRBdelconstrs");
  gurobi_dynamic_library->GetFunction(&GRBdelsos, "GRBdelsos");
  gurobi_dynamic_library->GetFunction(&GRBdelgenconstrs, "GRBdelgenconstrs");
  gurobi_dynamic_library->GetFunction(&GRBdelqconstrs, "GRBdelqconstrs");
  gurobi_dynamic_library->GetFunction(&GRBdelq, "GRBdelq");
  gurobi_dynamic_library->GetFunction(&GRBchgcoeffs, "GRBchgcoeffs");
  gurobi_dynamic_library->GetFunction(&GRBupdatemodel, "GRBupdatemodel");
  gurobi_dynamic_library->GetFunction(&GRBfreemodel, "GRBfreemodel");
  gurobi_dynamic_library->GetFunction(&GRBterminate, "GRBterminate");
  gurobi_dynamic_library->GetFunction(&GRBsetobjectiven, "GRBsetobjectiven");
  gurobi_dynamic_library->GetFunction(&GRBgetintparam, "GRBgetintparam");
  gurobi_dynamic_library->GetFunction(&GRBgetdblparam, "GRBgetdblparam");
  gurobi_dynamic_library->GetFunction(&GRBgetstrparam, "GRBgetstrparam");
  gurobi_dynamic_library->GetFunction(&GRBsetparam, "GRBsetparam");
  gurobi_dynamic_library->GetFunction(&GRBsetintparam, "GRBsetintparam");
  gurobi_dynamic_library->GetFunction(&GRBsetdblparam, "GRBsetdblparam");
  gurobi_dynamic_library->GetFunction(&GRBsetstrparam, "GRBsetstrparam");
  gurobi_dynamic_library->GetFunction(&GRBresetparams, "GRBresetparams");
  gurobi_dynamic_library->GetFunction(&GRBcopyparams, "GRBcopyparams");
  gurobi_dynamic_library->GetFunction(&GRBloadenv, "GRBloadenv");
  gurobi_dynamic_library->GetFunction(&GRBstartenv, "GRBstartenv");
  gurobi_dynamic_library->GetFunction(&GRBemptyenv, "GRBemptyenv");
  gurobi_dynamic_library->GetFunction(&GRBgetnumparams, "GRBgetnumparams");
  gurobi_dynamic_library->GetFunction(&GRBgetparamname, "GRBgetparamname");
  gurobi_dynamic_library->GetFunction(&GRBgetparamtype, "GRBgetparamtype");
  gurobi_dynamic_library->GetFunction(&GRBgetintparaminfo,
                                      "GRBgetintparaminfo");
  gurobi_dynamic_library->GetFunction(&GRBgetdblparaminfo,
                                      "GRBgetdblparaminfo");
  gurobi_dynamic_library->GetFunction(&GRBgetstrparaminfo,
                                      "GRBgetstrparaminfo");
  gurobi_dynamic_library->GetFunction(&GRBgetenv, "GRBgetenv");
  gurobi_dynamic_library->GetFunction(&GRBgetmultiobjenv, "GRBgetmultiobjenv");
  gurobi_dynamic_library->GetFunction(&GRBdiscardmultiobjenvs,
                                      "GRBdiscardmultiobjenvs");
  gurobi_dynamic_library->GetFunction(&GRBfreeenv, "GRBfreeenv");
  gurobi_dynamic_library->GetFunction(&GRBgeterrormsg, "GRBgeterrormsg");
  gurobi_dynamic_library->GetFunction(&GRBversion, "GRBversion");
  gurobi_dynamic_library->GetFunction(&GRBplatform, "GRBplatform");
}

std::vector<std::string> GurobiDynamicLibraryPotentialPaths() {
  std::vector<std::string> potential_paths;
  const std::vector<std::string> kGurobiVersions = {
      "1201", "1200", "1103", "1102", "1101", "1100", "1003",
      "1002", "1001", "1000", "952",  "951",  "950",  "911",
      "910",  "903",  "902",  "811",  "801",  "752"};
  potential_paths.reserve(kGurobiVersions.size() * 3);

  // Look for libraries pointed by GUROBI_HOME first.
  const char* gurobi_home_from_env = getenv("GUROBI_HOME");
  if (gurobi_home_from_env != nullptr) {
    for (const std::string& version : kGurobiVersions) {
      const std::string lib = version.substr(0, version.size() - 1);
#if defined(_MSC_VER)  // Windows
      potential_paths.push_back(
          absl::StrCat(gurobi_home_from_env, "\\bin\\gurobi", lib, ".dll"));
#elif defined(__APPLE__)  // OS X
      potential_paths.push_back(
          absl::StrCat(gurobi_home_from_env, "/lib/libgurobi", lib, ".dylib"));
#elif defined(__GNUC__)   // Linux
      potential_paths.push_back(
          absl::StrCat(gurobi_home_from_env, "/lib/libgurobi", lib, ".so"));
      potential_paths.push_back(
          absl::StrCat(gurobi_home_from_env, "/lib64/libgurobi", lib, ".so"));
#else
      LOG(ERROR) << "OS Not recognized by gurobi/environment.cc."
                 << " You won't be able to use Gurobi.";
#endif
    }
  }

  // Search for canonical places.
  for (const std::string& version : kGurobiVersions) {
    const std::string lib = version.substr(0, version.size() - 1);
#if defined(_MSC_VER)  // Windows
    potential_paths.push_back(absl::StrCat("C:\\Program Files\\gurobi", version,
                                           "\\win64\\bin\\gurobi", lib,
                                           ".dll"));
    potential_paths.push_back(absl::StrCat(
        "C:\\gurobi", version, "\\win64\\bin\\gurobi", lib, ".dll"));
    potential_paths.push_back(absl::StrCat("gurobi", lib, ".dll"));
#elif defined(__APPLE__)  // OS X
    potential_paths.push_back(absl::StrCat(
        "/Library/gurobi", version, "/mac64/lib/libgurobi", lib, ".dylib"));
    potential_paths.push_back(absl::StrCat("/Library/gurobi", version,
                                           "/macos_universal2/lib/libgurobi",
                                           lib, ".dylib"));
#elif defined(__GNUC__)   // Linux
    potential_paths.push_back(absl::StrCat(
        "/opt/gurobi", version, "/linux64/lib/libgurobi", lib, ".so"));
    potential_paths.push_back(absl::StrCat(
        "/opt/gurobi", version, "/linux64/lib64/libgurobi", lib, ".so"));
    potential_paths.push_back(
        absl::StrCat("/opt/gurobi/linux64/lib/libgurobi", lib, ".so"));
    potential_paths.push_back(
        absl::StrCat("/opt/gurobi/linux64/lib64/libgurobi", lib, ".so"));
#else
    LOG(ERROR) << "OS Not recognized by gurobi/environment.cc."
               << " You won't be able to use Gurobi.";
#endif
  }

#if defined(__GNUC__)  // path in linux64 gurobi/optimizer docker image.
  for (const std::string& version :
       {"12.0.1", "12.0.0", "11.0.3", "11.0.2", "11.0.1", "11.0.0", "10.0.3",
        "10.0.2", "10.0.1", "10.0.0", "9.5.2", "9.5.1", "9.5.0"}) {
    potential_paths.push_back(
        absl::StrCat("/opt/gurobi/linux64/lib/libgurobi.so.", version));
  }
#endif
  return potential_paths;
}

absl::Status LoadGurobiDynamicLibrary(
    std::vector<std::string> potential_paths) {
  static std::once_flag gurobi_loading_done;
  static absl::Status gurobi_load_status;
  static DynamicLibrary gurobi_library;
  static absl::Mutex mutex;

  absl::MutexLock lock(&mutex);

  std::call_once(gurobi_loading_done, [&potential_paths]() {
    const std::vector<std::string> canonical_paths =
        GurobiDynamicLibraryPotentialPaths();
    potential_paths.insert(potential_paths.end(), canonical_paths.begin(),
                           canonical_paths.end());
    for (const std::string& path : potential_paths) {
      if (gurobi_library.TryToLoad(path)) {
        LOG(INFO) << "Found the Gurobi library in '" << path << ".";
        break;
      }
    }

    if (gurobi_library.LibraryIsLoaded()) {
      LoadGurobiFunctions(&gurobi_library);
      gurobi_load_status = absl::OkStatus();
    } else {
      gurobi_load_status = absl::NotFoundError(absl::StrCat(
          "Could not find the Gurobi shared library. Looked in: [",
          absl::StrJoin(potential_paths, "', '"),
          "]. If you know where it"
          " is, pass the full path to 'LoadGurobiDynamicLibrary()'."));
    }
  });
  return gurobi_load_status;
}

absl::StatusOr<GRBenv*> GetGurobiEnv() {
  RETURN_IF_ERROR(LoadGurobiDynamicLibrary({}));

  GRBenv* env = nullptr;

  if (GRBloadenv(&env, nullptr) != 0 || env == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Found the Gurobi shared library, but could not create "
                     "Gurobi environment: is Gurobi licensed on this machine?",
                     GRBgeterrormsg(env)));
  }
  return env;
}

}  // namespace operations_research
