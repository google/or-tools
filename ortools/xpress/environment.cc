// Copyright 2019-2023 RTE
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

// Initial version of this code was provided by RTE

#include "ortools/xpress/environment.h"

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/dynamic_library.h"
#include "ortools/base/logging.h"

namespace operations_research {

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

// Let's not reformat for rest of the file.
// clang-format off
// This was generated with the parse_header_xpress.py script.
// See the comment at the top of the script.

// This is the 'define' section.
std::function<int(XPRSprob* p_prob)> XPRScreateprob = nullptr;
std::function<int(XPRSprob prob)> XPRSdestroyprob = nullptr;
std::function<int(const char* path)> XPRSinit = nullptr;
std::function<int(void)> XPRSfree = nullptr;
std::function<int(char* buffer, int maxbytes)> XPRSgetlicerrmsg = nullptr;
std::function<int(int* p_i, char* p_c)> XPRSlicense = nullptr;
std::function<int(char* banner)> XPRSgetbanner = nullptr;
std::function<int(char* version)> XPRSgetversion = nullptr;
std::function<int(XPRSprob prob, const char* probname)> XPRSsetprobname = nullptr;
std::function<int(XPRSprob prob, int control)> XPRSsetdefaultcontrol = nullptr;
std::function<int(XPRSprob prob, int reason)> XPRSinterrupt = nullptr;
std::function<int(XPRSprob prob, int control, int value)> XPRSsetintcontrol = nullptr;
std::function<int(XPRSprob prob, int control, XPRSint64 value)> XPRSsetintcontrol64 = nullptr;
std::function<int(XPRSprob prob, int control, double value)> XPRSsetdblcontrol = nullptr;
std::function<int(XPRSprob prob, int control, const char* value)> XPRSsetstrcontrol = nullptr;
std::function<int(XPRSprob prob, int control, int* p_value)> XPRSgetintcontrol = nullptr;
std::function<int(XPRSprob prob, int control, XPRSint64* p_value)> XPRSgetintcontrol64 = nullptr;
std::function<int(XPRSprob prob, int control, double* p_value)> XPRSgetdblcontrol = nullptr;
std::function<int(XPRSprob prob, int control, char* value, int maxbytes, int* p_nbytes)> XPRSgetstringcontrol = nullptr;
std::function<int(XPRSprob prob, int attrib, int* p_value)> XPRSgetintattrib = nullptr;
std::function<int(XPRSprob prob, int attrib, double* p_value)> XPRSgetdblattrib = nullptr;
std::function<int(XPRSprob prob, const char* name, int* p_id, int* p_type)> XPRSgetcontrolinfo = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const int start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSloadlp = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const XPRSint64 start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSloadlp64 = nullptr;
std::function<int(XPRSprob prob, double objcoef[], int first, int last)> XPRSgetobj = nullptr;
std::function<int(XPRSprob prob, double rhs[], int first, int last)> XPRSgetrhs = nullptr;
std::function<int(XPRSprob prob, double rng[], int first, int last)> XPRSgetrhsrange = nullptr;
std::function<int(XPRSprob prob, double lb[], int first, int last)> XPRSgetlb = nullptr;
std::function<int(XPRSprob prob, double ub[], int first, int last)> XPRSgetub = nullptr;
std::function<int(XPRSprob prob, int row, int col, double* p_coef)> XPRSgetcoef = nullptr;
std::function<int(XPRSprob prob, int* status, double x[], int first, int last)> XPRSgetsolution = nullptr;
std::function<int(XPRSprob prob, int* status, double duals[], int first, int last)> XPRSgetduals = nullptr;
std::function<int(XPRSprob prob, int* status, double djs[], int first, int last)> XPRSgetredcosts = nullptr;
std::function<int(XPRSprob prob, int nrows, int ncoefs, const char rowtype[], const double rhs[], const double rng[], const int start[], const int colind[], const double rowcoef[])> XPRSaddrows = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[])> XPRSdelrows = nullptr;
std::function<int(XPRSprob prob, int ncols, int ncoefs, const double objcoef[], const int start[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSaddcols = nullptr;
std::function<int(XPRSprob prob, int type, const char names[], int first, int last)> XPRSaddnames = nullptr;
std::function<int(XPRSprob prob, int type, char names[], int first, int last)> XPRSgetnames = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[])> XPRSdelcols = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[], const char coltype[])> XPRSchgcoltype = nullptr;
std::function<int(XPRSprob prob, const int rowstat[], const int colstat[])> XPRSloadbasis = nullptr;
std::function<int(XPRSprob prob)> XPRSpostsolve = nullptr;
std::function<int(XPRSprob prob, int objsense)> XPRSchgobjsense = nullptr;
std::function<int(XPRSprob prob, char* errmsg)> XPRSgetlasterror = nullptr;
std::function<int(XPRSprob prob, int rowstat[], int colstat[])> XPRSgetbasis = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwriteprob = nullptr;
std::function<int(XPRSprob prob, char rowtype[], int first, int last)> XPRSgetrowtype = nullptr;
std::function<int(XPRSprob prob, char coltype[], int first, int last)> XPRSgetcoltype = nullptr;
std::function<int(XPRSprob prob, int nbounds, const int colind[], const char bndtype[], const double bndval[])> XPRSchgbounds = nullptr;
std::function<int(XPRSprob prob, int length, const double solval[], const int colind[], const char* name)> XPRSaddmipsol = nullptr;
std::function<int(XPRSprob prob, double x[], double slack[], double duals[], double djs[])> XPRSgetlpsol = nullptr;
std::function<int(XPRSprob prob, double x[], double slack[])> XPRSgetmipsol = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[], const double objcoef[])> XPRSchgobj = nullptr;
std::function<int(XPRSprob prob, int row, int col, double coef)> XPRSchgcoef = nullptr;
std::function<int(XPRSprob prob, int ncoefs, const int rowind[], const int colind[], const double rowcoef[])> XPRSchgmcoef = nullptr;
std::function<int(XPRSprob prob, XPRSint64 ncoefs, const int rowind[], const int colind[], const double rowcoef[])> XPRSchgmcoef64 = nullptr;
std::function<int(XPRSprob prob, int ncoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[])> XPRSchgmqobj = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[], const double rhs[])> XPRSchgrhs = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[], const double rng[])> XPRSchgrhsrange = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[], const char rowtype[])> XPRSchgrowtype = nullptr;
std::function<int(XPRSprob prob, void (XPRS_CC *f_intsol)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbintsol = nullptr;
std::function<int(XPRSprob prob, void (XPRS_CC *f_intsol)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbintsol = nullptr;
std::function<int(XPRSprob prob, void (XPRS_CC *f_message)(XPRSprob cbprob, void* cbdata, const char* msg, int msglen, int msgtype), void* p, int priority)> XPRSaddcbmessage = nullptr;
std::function<int(XPRSprob prob, const char* flags)> XPRSlpoptimize = nullptr;
std::function<int(XPRSprob prob, const char* flags)> XPRSmipoptimize = nullptr;
std::function<int(XPRSprob prob, const char* flags, int* solvestatus, int* solstatus)> XPRSoptimize = nullptr;

void LoadXpressFunctions(DynamicLibrary* xpress_dynamic_library) {
  // This was generated with the parse_header_xpress.py script.
  // See the comment at the top of the script.

  // This is the 'assign' section.
  xpress_dynamic_library->GetFunction(&XPRScreateprob, "XPRScreateprob");
  xpress_dynamic_library->GetFunction(&XPRSdestroyprob, "XPRSdestroyprob");
  xpress_dynamic_library->GetFunction(&XPRSinit, "XPRSinit");
  xpress_dynamic_library->GetFunction(&XPRSfree, "XPRSfree");
  xpress_dynamic_library->GetFunction(&XPRSgetlicerrmsg, "XPRSgetlicerrmsg");
  xpress_dynamic_library->GetFunction(&XPRSlicense, "XPRSlicense");
  xpress_dynamic_library->GetFunction(&XPRSgetbanner, "XPRSgetbanner");
  xpress_dynamic_library->GetFunction(&XPRSgetversion, "XPRSgetversion");
  xpress_dynamic_library->GetFunction(&XPRSsetprobname, "XPRSsetprobname");
  xpress_dynamic_library->GetFunction(&XPRSsetdefaultcontrol, "XPRSsetdefaultcontrol");
  xpress_dynamic_library->GetFunction(&XPRSinterrupt, "XPRSinterrupt");
  xpress_dynamic_library->GetFunction(&XPRSsetintcontrol, "XPRSsetintcontrol");
  xpress_dynamic_library->GetFunction(&XPRSsetintcontrol64, "XPRSsetintcontrol64");
  xpress_dynamic_library->GetFunction(&XPRSsetdblcontrol, "XPRSsetdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRSsetstrcontrol, "XPRSsetstrcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetintcontrol, "XPRSgetintcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetintcontrol64, "XPRSgetintcontrol64");
  xpress_dynamic_library->GetFunction(&XPRSgetdblcontrol, "XPRSgetdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetstringcontrol, "XPRSgetstringcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetintattrib, "XPRSgetintattrib");
  xpress_dynamic_library->GetFunction(&XPRSgetdblattrib, "XPRSgetdblattrib");
  xpress_dynamic_library->GetFunction(&XPRSloadlp, "XPRSloadlp");
  xpress_dynamic_library->GetFunction(&XPRSloadlp64, "XPRSloadlp64");
  xpress_dynamic_library->GetFunction(&XPRSgetobj, "XPRSgetobj");
  xpress_dynamic_library->GetFunction(&XPRSgetrhs, "XPRSgetrhs");
  xpress_dynamic_library->GetFunction(&XPRSgetrhsrange, "XPRSgetrhsrange");
  xpress_dynamic_library->GetFunction(&XPRSgetlb, "XPRSgetlb");
  xpress_dynamic_library->GetFunction(&XPRSgetub, "XPRSgetub");
  xpress_dynamic_library->GetFunction(&XPRSgetcoef, "XPRSgetcoef");
  xpress_dynamic_library->GetFunction(&XPRSgetsolution, "XPRSgetsolution");
  xpress_dynamic_library->GetFunction(&XPRSgetduals, "XPRSgetduals");
  xpress_dynamic_library->GetFunction(&XPRSgetredcosts, "XPRSgetredcosts");
  xpress_dynamic_library->GetFunction(&XPRSaddrows, "XPRSaddrows");
  xpress_dynamic_library->GetFunction(&XPRSdelrows, "XPRSdelrows");
  xpress_dynamic_library->GetFunction(&XPRSaddcols, "XPRSaddcols");
  xpress_dynamic_library->GetFunction(&XPRSaddnames, "XPRSaddnames");
  xpress_dynamic_library->GetFunction(&XPRSgetnames, "XPRSgetnames");
  xpress_dynamic_library->GetFunction(&XPRSdelcols, "XPRSdelcols");
  xpress_dynamic_library->GetFunction(&XPRSchgcoltype, "XPRSchgcoltype");
  xpress_dynamic_library->GetFunction(&XPRSloadbasis, "XPRSloadbasis");
  xpress_dynamic_library->GetFunction(&XPRSpostsolve, "XPRSpostsolve");
  xpress_dynamic_library->GetFunction(&XPRSchgobjsense, "XPRSchgobjsense");
  xpress_dynamic_library->GetFunction(&XPRSgetlasterror, "XPRSgetlasterror");
  xpress_dynamic_library->GetFunction(&XPRSgetbasis, "XPRSgetbasis");
  xpress_dynamic_library->GetFunction(&XPRSwriteprob, "XPRSwriteprob");
  xpress_dynamic_library->GetFunction(&XPRSgetrowtype, "XPRSgetrowtype");
  xpress_dynamic_library->GetFunction(&XPRSgetcoltype, "XPRSgetcoltype");
  xpress_dynamic_library->GetFunction(&XPRSchgbounds, "XPRSchgbounds");
  xpress_dynamic_library->GetFunction(&XPRSaddmipsol, "XPRSaddmipsol");
  xpress_dynamic_library->GetFunction(&XPRSgetlpsol, "XPRSgetlpsol");
  xpress_dynamic_library->GetFunction(&XPRSgetmipsol, "XPRSgetmipsol");
  xpress_dynamic_library->GetFunction(&XPRSchgobj, "XPRSchgobj");
  xpress_dynamic_library->GetFunction(&XPRSchgcoef, "XPRSchgcoef");
  xpress_dynamic_library->GetFunction(&XPRSchgmcoef, "XPRSchgmcoef");
  xpress_dynamic_library->GetFunction(&XPRSchgmcoef64, "XPRSchgmcoef64");
  xpress_dynamic_library->GetFunction(&XPRSchgmqobj, "XPRSchgmqobj");
  xpress_dynamic_library->GetFunction(&XPRSchgrhs, "XPRSchgrhs");
  xpress_dynamic_library->GetFunction(&XPRSchgrhsrange, "XPRSchgrhsrange");
  xpress_dynamic_library->GetFunction(&XPRSchgrowtype, "XPRSchgrowtype");
  xpress_dynamic_library->GetFunction(&XPRSaddcbintsol, "XPRSaddcbintsol");
  xpress_dynamic_library->GetFunction(&XPRSremovecbintsol, "XPRSremovecbintsol");
  xpress_dynamic_library->GetFunction(&XPRSaddcbmessage, "XPRSaddcbmessage");
  xpress_dynamic_library->GetFunction(&XPRSlpoptimize, "XPRSlpoptimize");
  xpress_dynamic_library->GetFunction(&XPRSmipoptimize, "XPRSmipoptimize");
  xpress_dynamic_library->GetFunction(&XPRSoptimize, "XPRSoptimize");
}
// clang-format on

void printXpressBanner(bool error) {
  char banner[XPRS_MAXBANNERLENGTH];
  XPRSgetbanner(banner);

  if (error) {
    LOG(ERROR) << "XpressInterface : Xpress banner :\n" << banner << "\n";
  } else {
    LOG(WARNING) << "XpressInterface : Xpress banner :\n" << banner << "\n";
  }
}

std::vector<std::string> XpressDynamicLibraryPotentialPaths() {
  std::vector<std::string> potential_paths;

  // Look for libraries pointed by XPRESSDIR first.
  const char* xpressdir_from_env = getenv("XPRESSDIR");
  if (xpressdir_from_env != nullptr) {
    LOG(INFO) << "Environment variable XPRESSDIR = " << xpressdir_from_env;
#if defined(_MSC_VER)  // Windows
    potential_paths.push_back(
        absl::StrCat(xpressdir_from_env, "\\bin\\xprs.dll"));
#elif defined(__APPLE__)  // OS X
    potential_paths.push_back(
        absl::StrCat(xpressdir_from_env, "/lib/libxprs.dylib"));
#elif defined(__GNUC__)   // Linux
    potential_paths.push_back(
        absl::StrCat(xpressdir_from_env, "/lib/libxprs.so"));
#else
    LOG(ERROR) << "OS Not recognized by xpress/environment.cc."
               << " You won't be able to use Xpress.";
#endif
  } else {
    LOG(WARNING) << "Environment variable XPRESSDIR undefined.";
  }

  // Search for canonical places.
#if defined(_MSC_VER)  // Windows
  potential_paths.push_back(absl::StrCat("C:\\xpressmp\\bin\\xprs.dll"));
  potential_paths.push_back(
      absl::StrCat("C:\\Program Files\\xpressmp\\bin\\xprs.dll"));
#elif defined(__APPLE__)  // OS X
  potential_paths.push_back(
      absl::StrCat("/Library/xpressmp/lib/libxprs.dylib"));
#elif defined(__GNUC__)   // Linux
  potential_paths.push_back(absl::StrCat("/opt/xpressmp/lib/libxprs.so"));
#else
  LOG(ERROR) << "OS Not recognized by xpress/environment.cc."
             << " You won't be able to use Xpress.";
#endif
  return potential_paths;
}

absl::Status LoadXpressDynamicLibrary(std::string& xpresspath) {
  static std::string xpress_lib_path;
  static std::once_flag xpress_loading_done;
  static absl::Status xpress_load_status;
  static DynamicLibrary xpress_library;
  static absl::Mutex mutex(absl::kConstInit);

  absl::MutexLock lock(&mutex);

  std::call_once(xpress_loading_done, []() {
    const std::vector<std::string> canonical_paths =
        XpressDynamicLibraryPotentialPaths();
    for (const std::string& path : canonical_paths) {
      if (xpress_library.TryToLoad(path)) {
        LOG(INFO) << "Found the Xpress library in " << path << ".";
        xpress_lib_path.clear();
        std::filesystem::path p(path);
        p.remove_filename();
        xpress_lib_path.append(p.string());
        break;
      }
    }

    if (xpress_library.LibraryIsLoaded()) {
      LOG(INFO) << "Loading all Xpress functions";
      LoadXpressFunctions(&xpress_library);
      xpress_load_status = absl::OkStatus();
    } else {
      xpress_load_status = absl::NotFoundError(
          absl::StrCat("Could not find the Xpress shared library. Looked in: [",
                       absl::StrJoin(canonical_paths, "', '"),
                       "]. Please check environment variable XPRESSDIR"));
    }
  });
  xpresspath.clear();
  xpresspath.append(xpress_lib_path);
  return xpress_load_status;
}

void log_message_about_XPRSinit_argument();
void log_full_license_error(int code, const std::string& xpress_lib_dir);
//! init XPRESS environment.
bool initXpressEnv(bool verbose, int xpress_oem_license_key) {
  std::string xpress_lib_dir;
  absl::Status status = LoadXpressDynamicLibrary(xpress_lib_dir);
  if (!status.ok()) {
    LOG(WARNING) << status << "\n";
    return false;
  }

  int code;
  // if not an OEM key
  if (xpress_oem_license_key == 0) {
    if (verbose) {
      log_message_about_XPRSinit_argument();
    }

    code = XPRSinit(nullptr);

    if (!code) {
      // XPRSbanner informs about Xpress version, options and error messages
      if (verbose) {
        printXpressBanner(false);
        char version[16];
        XPRSgetversion(version);
        LOG(WARNING) << "Optimizer version: " << version
                     << " (OR-Tools was compiled with version " << XPVERSION
                     << ").\n";
      }
      return true;
    } else {
      log_full_license_error(code, xpress_lib_dir);
      return false;
    }
  } else {
    // if OEM key
    if (verbose) {
      LOG(WARNING) << "XpressInterface : Initialising xpress-MP with OEM key "
                   << xpress_oem_license_key << "\n";
    }

    int nvalue = 0;
    int ierr;
    char slicmsg[256] = "";
    char errmsg[256];

    XPRSlicense(&nvalue, slicmsg);
    if (verbose) {
      VLOG(0) << "XpressInterface : First message from XPRSLicense : "
              << slicmsg << "\n";
    }

    nvalue = xpress_oem_license_key - ((nvalue * nvalue) / 19);
    ierr = XPRSlicense(&nvalue, slicmsg);

    if (verbose) {
      VLOG(0) << "XpressInterface : Second message from XPRSLicense : "
              << slicmsg << "\n";
    }
    if (ierr == 16) {
      if (verbose) {
        VLOG(0)
            << "XpressInterface : Optimizer development software detected\n";
      }
    } else if (ierr != 0) {
      // get the license error message
      XPRSgetlicerrmsg(errmsg, 256);

      LOG(ERROR) << "XpressInterface : " << errmsg << "\n";
      return false;
    }

    code = XPRSinit(NULL);

    if (!code) {
      return true;
    } else {
      LOG(ERROR) << "XPRSinit returned code : " << code << "\n";
      return false;
    }
  }
}
void log_full_license_error(int code, const std::string& xpress_lib_dir) {
  LOG(WARNING) << "XpressInterface: Xpress found at " << xpress_lib_dir << "\n";
  char errmsg[256];
  XPRSgetlicerrmsg(errmsg, 256);

  LOG(ERROR) << "XpressInterface : License error : " << errmsg
             << " (XPRSinit returned code " << code << "). \n";
  LOG(ERROR)
      << "|_Your Xpress installation should have set the env var XPAUTH_PATH"
         " to the full path of your licence file\n";
}
void log_message_about_XPRSinit_argument() {
  LOG(WARNING)
      << "XpressInterface : Initialising xpress-MP with default parameters";
}

bool XpressIsCorrectlyInstalled() {
  bool correctlyInstalled = initXpressEnv(false);
  if (correctlyInstalled) {
    XPRSfree();
  }
  return correctlyInstalled;
}

}  // namespace operations_research
