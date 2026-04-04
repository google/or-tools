// Copyright 2010-2026 Google LLC
//
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

#include "ortools/third_party_solvers/cplex_environment.h"

// NOLINTNEXTLINE(build/c++17)
#include <filesystem>

#include "absl/base/call_once.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/third_party_solvers/dynamic_library.h"

namespace operations_research {

// This was generated with the parse_header.py script.
// See the comment at the top of the script.
// Let's not reformat the rest of the file.
// clang-format off

// This is the 'define' section.

std::function<CPXENVptr(int *status_p)>
  CPXopenCPLEX = nullptr;

std::function<int(CPXENVptr *env_p)>
  CPXcloseCPLEX = nullptr;

std::function<CPXLPptr(CPXCENVptr env, int * status_p, char const * probname_str)>
  CPXcreateprob = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr * lp_p)>
  CPXfreeprob = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, char const *probname)>
  CPXchgprobname = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int ccnt,
                  double const *obj, double const *lb, double const *ub,
                  char const *xctype, char **colname)>
  CPXnewcols = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt,
                  int const *indices, double const *values)>
  CPXchgrngval = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp)>
  CPXlpopt = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp)>
  CPXmipopt = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp)>
  CPXgetstat = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, double *objval_p)>
  CPXgetobjval = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, double *objval_p)>
  CPXgetbestobjval = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, double *x, int begin, int end)>
  CPXgetx = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, double *pi, int begin, int end)>
  CPXgetpi = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, double *dj, int begin, int end)>
  CPXgetdj = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp)>
  CPXgetnumcols = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp)>
  CPXgetnumrows = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int maxormin)>
  CPXchgobjsen = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, double offset)>
  CPXchgobjoffset = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, double const * values)>
  CPXchgobj = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp)>
  CPXgetprobtype = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, int *solnmethod_p, int *solntype_p, int *pfeasind_p, int *dfeasind_p)>
  CPXsolninfo = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp)> 
  CPXgetobjsen = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int numcoefs, int const *rowlist, int const *collist, double const *vallist)>
  CPXchgcoeflist = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const *indices, char const *lu, double const *bd)>
  CPXchgbds = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, char const * xctype)>
  CPXchgctype = nullptr;

std::function<int(CPXCENVptr env, int errcode, char * buffer_str)>
  CPXgeterrorstring = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp)>
  CPXgetitcnt = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp)>
  CPXgetmipitcnt = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp)>
  CPXgetbaritcnt = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp)>
  CPXgetnodecnt = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp)>
  CPXgetsolnpoolnumsolns = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int soln, double * objval_p)>
  CPXgetsolnpoolobjval = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, int soln, double * x, int begin, int end)>
  CPXgetsolnpoolx = nullptr;

std::function<int(CPXCENVptr env, int whichparam, double newvalue)>
  CPXsetdblparam = nullptr;

std::function<int(CPXCENVptr env, int whichparam, int newvalue)>
  CPXsetintparam = nullptr;

std::function<int(CPXCENVptr env, int whichparam, CPXLONG newvalue)>
  CPXsetlongparam = nullptr;

std::function<int(CPXCENVptr env, int whichparam, char const * newvalue_str)> 
  CPXsetstrparam = nullptr;

std::function<int(CPXENVptr env)>
  CPXsetdefaults = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, char const * sense)>
  CPXchgsense = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, double const * values)> 
  CPXchgrhs = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int *delstat)>
  CPXdelsetcols = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int *delstat)>
  CPXdelsetrows = nullptr;

std::function<int(CPXCENVptr env, char const * name_str, int * whichparam_p)> 
  CPXgetparamnum = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, double * lb, int begin, int end)> 
  CPXgetlb = nullptr;

std::function<int(CPXCENVptr env, CPXCLPptr lp, double * lb, int begin, int end)> 
  CPXgetub = nullptr;

std::function<int(CPXENVptr env, volatile int * terminate_p)> 
  CPXsetterminate = nullptr;

std::function<int(CPXCENVptr env, CPXCHANNELptr * cpxresults_p, CPXCHANNELptr * cpxwarning_p, CPXCHANNELptr * cpxerror_p, CPXCHANNELptr * cpxlog_p)> 
  CPXgetchannels = nullptr;

std::function<int(CPXCENVptr env, CPXCHANNELptr channel, void * handle, void(CPXPUBLIC *msgfunction)(void *, const char *))> 
  CPXaddfuncdest = nullptr;

std::function<int(CPXCENVptr env, CPXCHANNELptr channel, void * handle, void(CPXPUBLIC *msgfunction)(void *, const char *))> 
  CPXdelfuncdest = nullptr;

std::function<int(CPXCENVptr env, CPXLPptr lp, int rcnt, double const * rhs, char const * sense, double const * rngval, char ** rowname)> 
  CPXnewrows = nullptr;

std::function<CPXCCHARptr(CPXCENVptr env)>
  CPXversion = nullptr;

// -----

std::vector<std::string> CplexDynamicLibraryPotentialPaths() {
  std::vector<std::string> potential_paths;
  std::vector<std::string> kCplexVersions = {"2212", "2211", "2210", "2010"};

  // Look for libraries pointed by CPLEXDIR first.
  const char* cplexdir_from_env = getenv("CPLEXDIR");
  if (cplexdir_from_env != nullptr) {
    for (const absl::string_view version : kCplexVersions) {
#if defined(_MSC_VER)  // Windows
      potential_paths.push_back(
        absl::StrCat(cplexdir_from_env, "\\bin\\x64_win64\\cplex", version, ".dll"));
#elif defined(__APPLE__) // macOS
#if defined(__arm64__) || defined(__aarch64__)
      potential_paths.push_back(
        absl::StrCat(cplexdir_from_env, "/bin/arm64_osx/libcplex", version, ".dylib"));
#endif
      potential_paths.push_back(
        absl::StrCat(cplexdir_from_env, "/bin/x86-64_osx/libcplex", version, ".dylib"));
#elif defined(__GNUC__)  // Linux
      potential_paths.push_back(
        absl::StrCat(cplexdir_from_env, "/bin/x86-64_linux/libcplex", version, ".so"));
#else
      LOG(ERROR) << "OS Not recognized by cplex_environment.cc."
                << " You won't be able to use CPLEX.";
#endif
    }
  }

  // Search for canonical places.
  for (const absl::string_view version : kCplexVersions) {

    absl::string_view version_maybe_wo_trailing_0 = version;
    if(version.back() == '0')
      version_maybe_wo_trailing_0 = version.substr(0, version.size()-1);

#if defined(_MSC_VER)  // Windows
    potential_paths.push_back(absl::StrCat(
        "C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio", version_maybe_wo_trailing_0,
        "\\cplex\\bin\\x64_win64\\cplex", version, ".dll"));
#elif defined(__APPLE__)  // macOS
#if defined(__arm64__) || defined(__aarch64__)
    potential_paths.push_back(absl::StrCat(
        "/Applications/CPLEX_Studio", version_maybe_wo_trailing_0,
        "/cplex/bin/arm64_osx/libcplex", version, ".dylib"));
#endif
    potential_paths.push_back(absl::StrCat(
        "/Applications/CPLEX_Studio", version_maybe_wo_trailing_0,
        "/cplex/bin/x86-64_osx/libcplex", version, ".dylib"));
#elif defined(__GNUC__)  // Linux
    potential_paths.push_back(absl::StrCat(
        "/opt/ibm/ILOG/CPLEX_Studio", version_maybe_wo_trailing_0,
        "/cplex/bin/x86-64_linux/libcplex", version, ".so"));
#else
    LOG(ERROR) << "OS Not recognized by cplex_environment.cc."
              << " You won't be able to use CPLEX.";
#endif
  }
  return potential_paths;
}

void LoadCplexFunctions(DynamicLibrary* cplex_dynamic_library) {

  // This is the 'assign' section.

  cplex_dynamic_library->GetFunction(&CPXopenCPLEX,
                                     "CPXopenCPLEX");
  cplex_dynamic_library->GetFunction(&CPXcloseCPLEX,
                                     "CPXcloseCPLEX");
  cplex_dynamic_library->GetFunction(&CPXcreateprob,
                                     "CPXcreateprob");
  cplex_dynamic_library->GetFunction(&CPXfreeprob,
                                     "CPXfreeprob");
  cplex_dynamic_library->GetFunction(&CPXchgprobname,
                                     "CPXchgprobname");
  cplex_dynamic_library->GetFunction(&CPXnewcols,
                                     "CPXnewcols");
  cplex_dynamic_library->GetFunction(&CPXchgrngval,
                                     "CPXchgrngval");
  cplex_dynamic_library->GetFunction(&CPXlpopt,
                                     "CPXlpopt");
  cplex_dynamic_library->GetFunction(&CPXmipopt,
                                     "CPXmipopt");
  cplex_dynamic_library->GetFunction(&CPXgetstat,
                                     "CPXgetstat");
  cplex_dynamic_library->GetFunction(&CPXgetobjval,
                                     "CPXgetobjval");
  cplex_dynamic_library->GetFunction(&CPXgetbestobjval,
                                     "CPXgetbestobjval");
  cplex_dynamic_library->GetFunction(&CPXgetx,
                                     "CPXgetx");
  cplex_dynamic_library->GetFunction(&CPXgetpi,
                                     "CPXgetpi");
  cplex_dynamic_library->GetFunction(&CPXgetdj,
                                     "CPXgetdj");
  cplex_dynamic_library->GetFunction(&CPXgetnumcols,
                                     "CPXgetnumcols");
  cplex_dynamic_library->GetFunction(&CPXgetnumrows,
                                     "CPXgetnumrows");
  cplex_dynamic_library->GetFunction(&CPXchgobjsen,
                                     "CPXchgobjsen");
  cplex_dynamic_library->GetFunction(&CPXchgobjoffset,
                                     "CPXchgobjoffset");
  cplex_dynamic_library->GetFunction(&CPXchgobj,
                                     "CPXchgobj");
  cplex_dynamic_library->GetFunction(&CPXgetprobtype,
                                     "CPXgetprobtype");
  cplex_dynamic_library->GetFunction(&CPXsolninfo,
                                     "CPXsolninfo");
  cplex_dynamic_library->GetFunction(&CPXgetobjsen,
                                     "CPXgetobjsen");
  cplex_dynamic_library->GetFunction(&CPXchgcoeflist,
                                     "CPXchgcoeflist");
  cplex_dynamic_library->GetFunction(&CPXchgbds,
                                     "CPXchgbds");
  cplex_dynamic_library->GetFunction(&CPXchgctype,
                                     "CPXchgctype");
  cplex_dynamic_library->GetFunction(&CPXgeterrorstring,
                                     "CPXgeterrorstring");
  cplex_dynamic_library->GetFunction(&CPXgetitcnt,
                                     "CPXgetitcnt");
  cplex_dynamic_library->GetFunction(&CPXgetmipitcnt,
                                     "CPXgetmipitcnt");
  cplex_dynamic_library->GetFunction(&CPXgetbaritcnt,
                                     "CPXgetbaritcnt");
  cplex_dynamic_library->GetFunction(&CPXgetnodecnt,
                                     "CPXgetnodecnt");
  cplex_dynamic_library->GetFunction(&CPXgetsolnpoolnumsolns,
                                     "CPXgetsolnpoolnumsolns");
  cplex_dynamic_library->GetFunction(&CPXgetsolnpoolobjval,
                                     "CPXgetsolnpoolobjval");
  cplex_dynamic_library->GetFunction(&CPXgetsolnpoolx,
                                     "CPXgetsolnpoolx");
  cplex_dynamic_library->GetFunction(&CPXsetdblparam,
                                     "CPXsetdblparam");
  cplex_dynamic_library->GetFunction(&CPXsetintparam,
                                     "CPXsetintparam");
  cplex_dynamic_library->GetFunction(&CPXsetlongparam,
                                     "CPXsetlongparam");
  cplex_dynamic_library->GetFunction(&CPXsetstrparam,
                                     "CPXsetstrparam");
  cplex_dynamic_library->GetFunction(&CPXsetdefaults,
                                     "CPXsetdefaults");
  cplex_dynamic_library->GetFunction(&CPXchgsense,
                                     "CPXchgsense");
  cplex_dynamic_library->GetFunction(&CPXchgrhs,
                                     "CPXchgrhs");
  cplex_dynamic_library->GetFunction(&CPXdelsetcols,
                                     "CPXdelsetcols");
  cplex_dynamic_library->GetFunction(&CPXdelsetrows,
                                     "CPXdelsetrows");
  cplex_dynamic_library->GetFunction(&CPXgetparamnum,
                                     "CPXgetparamnum");
  cplex_dynamic_library->GetFunction(&CPXgetlb,
                                     "CPXgetlb");
  cplex_dynamic_library->GetFunction(&CPXgetub,
                                     "CPXgetub");
  cplex_dynamic_library->GetFunction(&CPXsetterminate,
                                     "CPXsetterminate");
  cplex_dynamic_library->GetFunction(&CPXgetchannels,
                                     "CPXgetchannels");
  cplex_dynamic_library->GetFunction(&CPXaddfuncdest,
                                     "CPXaddfuncdest");
  cplex_dynamic_library->GetFunction(&CPXdelfuncdest,
                                     "CPXdelfuncdest");
  cplex_dynamic_library->GetFunction(&CPXnewrows,
                                     "CPXnewrows");
  cplex_dynamic_library->GetFunction(&CPXversion,
                                     "CPXversion");
}

absl::Status LoadCplexDynamicLibrary(std::string& cplexpath) {
  static std::string* cplex_lib_path = new std::string;
  static absl::once_flag cplex_loading_done;
  static absl::Status* cplex_load_status = new absl::Status;
  static DynamicLibrary* cplex_library = new DynamicLibrary;
  static absl::Mutex mutex(absl::kConstInit);

  absl::MutexLock lock(&mutex);

  absl::call_once(cplex_loading_done, []() {
    const std::vector<std::string> canonical_paths = 
      CplexDynamicLibraryPotentialPaths();
    for (const std::string& path : canonical_paths) {
      if (cplex_library->TryToLoad(path)) {
        LOG(INFO) << "Found the CPLEX library in " << path << ".";
        cplex_lib_path->clear();
        std::filesystem::path p(path);
        p.remove_filename();
        cplex_lib_path->append(p.string());
        break;
      }
    }

    if (cplex_library->LibraryIsLoaded()) {
      LOG(INFO) << "Loading all CPLEX functions";
      LoadCplexFunctions(cplex_library);
      *cplex_load_status = absl::OkStatus();
    }
    else {
      *cplex_load_status = absl::NotFoundError(
        absl::StrCat("Could not find the CPLEX shared library. Looked in: [",
                     absl::StrJoin(canonical_paths, "', '"),
                     "]. Please check environment variable CPLEXDIR"));
    }
  });
  cplexpath.clear();
  cplexpath.append(*cplex_lib_path);
  return *cplex_load_status;
}

}  // namespace operations_research