// Copyright 2010-2021 Google LLC
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

#include "absl/status/status.h"
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
    return false;
  }

  GRBfreeenv(status.value());

  return true;
}

// This was generated with the parse_header.py script.
// See the comment at the top of the script.

// This is the 'define' section.

std::function<int(GRBenv**, const char*, const char*, const char*, int,
                  const char*)>
    GRBisqp = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int* datatypeP,
                  int* sizeP, int* settableP)>
    GRBgetattrinfo = nullptr;
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
                  int* values)>
    GRBgetintattrlist = nullptr;
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
                  char* values)>
    GRBgetcharattrlist = nullptr;
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
                  double* values)>
    GRBgetdblattrlist = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int len, int* ind,
                  double* newvalues)>
    GRBsetdblattrlist = nullptr;
std::function<int(GRBmodel* model, const char* attrname, char** valueP)>
    GRBgetstrattr = nullptr;
std::function<int(GRBmodel* model, const char* attrname, const char* newvalue)>
    GRBsetstrattr = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  char** valueP)>
    GRBgetstrattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int element,
                  const char* newvalue)>
    GRBsetstrattrelement = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  char** values)>
    GRBgetstrattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int first, int len,
                  char** newvalues)>
    GRBsetstrattrarray = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int len, int* ind,
                  char** values)>
    GRBgetstrattrlist = nullptr;
std::function<int(GRBmodel* model, const char* attrname, int len, int* ind,
                  char** newvalues)>
    GRBsetstrattrlist = nullptr;
std::function<int(GRBmodel* model, int(GUROBI_STDCALL* cb)(CB_ARGS),
                  void* usrdata)>
    GRBsetcallbackfunc = nullptr;
std::function<int(GRBmodel* model, int(GUROBI_STDCALL** cbP)(CB_ARGS))>
    GRBgetcallbackfunc = nullptr;
std::function<int(GRBmodel* model, int(GUROBI_STDCALL* cb)(char* msg))>
    GRBsetlogcallbackfunc = nullptr;
std::function<int(GRBenv* env, int(GUROBI_STDCALL* cb)(char* msg))>
    GRBsetlogcallbackfuncenv = nullptr;
std::function<int(void* cbdata, int where, int what, void* resultP)> GRBcbget =
    nullptr;
std::function<int(void* cbdata, const char* paramname, const char* newvalue)>
    GRBcbsetparam = nullptr;
std::function<int(void* cbdata, const double* solution, double* objvalP)>
    GRBcbsolution = nullptr;
std::function<int(void* cbdata, int cutlen, const int* cutind,
                  const double* cutval, char cutsense, double cutrhs)>
    GRBcbcut = nullptr;
std::function<int(void* cbdata, int lazylen, const int* lazyind,
                  const double* lazyval, char lazysense, double lazyrhs)>
    GRBcblazy = nullptr;
std::function<int(GRBmodel* model, int constr, int var, double* valP)>
    GRBgetcoeff = nullptr;
std::function<int(GRBmodel* model, int* numnzP, int* cbeg, int* cind,
                  double* cval, int start, int len)>
    GRBgetconstrs = nullptr;
std::function<int(GRBmodel* model, size_t* numnzP, size_t* cbeg, int* cind,
                  double* cval, int start, int len)>
    GRBXgetconstrs = nullptr;
std::function<int(GRBmodel* model, int* numnzP, int* vbeg, int* vind,
                  double* vval, int start, int len)>
    GRBgetvars = nullptr;
std::function<int(GRBmodel* model, size_t* numnzP, size_t* vbeg, int* vind,
                  double* vval, int start, int len)>
    GRBXgetvars = nullptr;
std::function<int(GRBmodel* model, int* nummembersP, int* sostype, int* beg,
                  int* ind, double* weight, int start, int len)>
    GRBgetsos = nullptr;
std::function<int(GRBmodel* model, int genconstr, int* resvarP, int* nvarsP,
                  int* vars, double* constantP)>
    GRBgetgenconstrMax = nullptr;
std::function<int(GRBmodel* model, int genconstr, int* resvarP, int* nvarsP,
                  int* vars, double* constantP)>
    GRBgetgenconstrMin = nullptr;
std::function<int(GRBmodel* model, int genconstr, int* resvarP, int* argvarP)>
    GRBgetgenconstrAbs = nullptr;
std::function<int(GRBmodel* model, int genconstr, int* resvarP, int* nvarsP,
                  int* vars)>
    GRBgetgenconstrAnd = nullptr;
std::function<int(GRBmodel* model, int genconstr, int* resvarP, int* nvarsP,
                  int* vars)>
    GRBgetgenconstrOr = nullptr;
std::function<int(GRBmodel* model, int genconstr, int* binvarP, int* binvalP,
                  int* nvarsP, int* vars, double* vals, char* senseP,
                  double* rhsP)>
    GRBgetgenconstrIndicator = nullptr;
std::function<int(GRBmodel* model, int* numqnzP, int* qrow, int* qcol,
                  double* qval)>
    GRBgetq = nullptr;
std::function<int(GRBmodel* model, int qconstr, int* numlnzP, int* lind,
                  double* lval, int* numqnzP, int* qrow, int* qcol,
                  double* qval)>
    GRBgetqconstr = nullptr;
std::function<int(GRBmodel* model, const char* name, int* indexP)>
    GRBgetvarbyname = nullptr;
std::function<int(GRBmodel* model, const char* name, int* indexP)>
    GRBgetconstrbyname = nullptr;
std::function<int(GRBmodel* model, int var, int* pointsP, double* x, double* y)>
    GRBgetpwlobj = nullptr;
std::function<int(GRBmodel* model)> GRBoptimize = nullptr;
std::function<int(GRBmodel* model)> GRBoptimizeasync = nullptr;
std::function<GRBmodel*(GRBmodel* model)> GRBcopymodel = nullptr;
std::function<GRBmodel*(GRBmodel* model)> GRBfixedmodel = nullptr;
std::function<int(GRBmodel* model, int relaxobjtype, int minrelax,
                  double* lbpen, double* ubpen, double* rhspen,
                  double* feasobjP)>
    GRBfeasrelax = nullptr;
std::function<int(void* cbdata, int what, int* typeP, int* sizeP)>
    GRBgetcbwhatinfo = nullptr;
std::function<GRBmodel*(GRBmodel* model)> GRBrelaxmodel = nullptr;
std::function<int(GRBmodel* model)> GRBconverttofixed = nullptr;
std::function<GRBmodel*(GRBmodel* model)> GRBpresolvemodel = nullptr;
std::function<GRBmodel*(GRBmodel* model)> GRBiismodel = nullptr;
std::function<GRBmodel*(GRBmodel* model)> GRBfeasibility = nullptr;
std::function<GRBmodel*(GRBmodel* model)> GRBlinearizemodel = nullptr;
std::function<int(GRBenv** envP, const char* logfilename,
                  void*(GUROBI_STDCALL* malloccb)(MALLOCCB_ARGS),
                  void*(GUROBI_STDCALL* calloccb)(CALLOCCB_ARGS),
                  void*(GUROBI_STDCALL* realloccb)(REALLOCCB_ARGS),
                  void(GUROBI_STDCALL* freecb)(FREECB_ARGS),
                  int(GUROBI_STDCALL* threadcreatecb)(THREADCREATECB_ARGS),
                  void(GUROBI_STDCALL* threadjoincb)(THREADJOINCB_ARGS),
                  void* syscbusrdata)>
    GRBloadenvsyscb = nullptr;
std::function<int(GRBenv* env, const char* filename, GRBmodel** modelP)>
    GRBreadmodel = nullptr;
std::function<int(GRBmodel* model, const char* filename)> GRBread = nullptr;
std::function<int(GRBmodel* model, const char* filename)> GRBwrite = nullptr;
std::function<int(const char* filename)> GRBismodelfile = nullptr;
std::function<int(const char* filename)> GRBfiletype = nullptr;
std::function<int(const char* filename)> GRBisrecordfile = nullptr;
std::function<int(GRBenv* env, GRBmodel** modelP, const char* Pname,
                  int numvars, double* obj, double* lb, double* ub, char* vtype,
                  char** varnames)>
    GRBnewmodel = nullptr;
std::function<int(GRBenv* env, GRBmodel** modelP, const char* Pname,
                  int numvars, int numconstrs, int objsense, double objcon,
                  double* obj, char* sense, double* rhs, int* vbeg, int* vlen,
                  int* vind, double* vval, double* lb, double* ub, char* vtype,
                  char** varnames, char** constrnames)>
    GRBloadmodel = nullptr;
std::function<int(GRBenv* env, GRBmodel** modelP, const char* Pname,
                  int numvars, int numconstrs, int objsense, double objcon,
                  double* obj, char* sense, double* rhs, size_t* vbeg,
                  int* vlen, int* vind, double* vval, double* lb, double* ub,
                  char* vtype, char** varnames, char** constrnames)>
    GRBXloadmodel = nullptr;
std::function<int(GRBmodel* model, int numnz, int* vind, double* vval,
                  double obj, double lb, double ub, char vtype,
                  const char* varname)>
    GRBaddvar = nullptr;
std::function<int(GRBmodel* model, int numvars, int numnz, int* vbeg, int* vind,
                  double* vval, double* obj, double* lb, double* ub,
                  char* vtype, char** varnames)>
    GRBaddvars = nullptr;
std::function<int(GRBmodel* model, int numvars, size_t numnz, size_t* vbeg,
                  int* vind, double* vval, double* obj, double* lb, double* ub,
                  char* vtype, char** varnames)>
    GRBXaddvars = nullptr;
std::function<int(GRBmodel* model, int numnz, int* cind, double* cval,
                  char sense, double rhs, const char* constrname)>
    GRBaddconstr = nullptr;
std::function<int(GRBmodel* model, int numconstrs, int numnz, int* cbeg,
                  int* cind, double* cval, char* sense, double* rhs,
                  char** constrnames)>
    GRBaddconstrs = nullptr;
std::function<int(GRBmodel* model, int numconstrs, size_t numnz, size_t* cbeg,
                  int* cind, double* cval, char* sense, double* rhs,
                  char** constrnames)>
    GRBXaddconstrs = nullptr;
std::function<int(GRBmodel* model, int numnz, int* cind, double* cval,
                  double lower, double upper, const char* constrname)>
    GRBaddrangeconstr = nullptr;
std::function<int(GRBmodel* model, int numconstrs, int numnz, int* cbeg,
                  int* cind, double* cval, double* lower, double* upper,
                  char** constrnames)>
    GRBaddrangeconstrs = nullptr;
std::function<int(GRBmodel* model, int numconstrs, size_t numnz, size_t* cbeg,
                  int* cind, double* cval, double* lower, double* upper,
                  char** constrnames)>
    GRBXaddrangeconstrs = nullptr;
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
std::function<int(GRBmodel* lp, const char* name, int binvar, int binval,
                  int nvars, const int* vars, const double* vals, char sense,
                  double rhs)>
    GRBaddgenconstrIndicator = nullptr;
std::function<int(GRBmodel* model, int numlnz, int* lind, double* lval,
                  int numqnz, int* qrow, int* qcol, double* qval, char sense,
                  double rhs, const char* QCname)>
    GRBaddqconstr = nullptr;
std::function<int(GRBmodel* model, int nummembers, int* members)> GRBaddcone =
    nullptr;
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
std::function<int(GRBmodel* model, size_t cnt, int* cind, int* vind,
                  double* val)>
    GRBXchgcoeffs = nullptr;
std::function<int(GRBmodel* model, int var, int points, double* x, double* y)>
    GRBsetpwlobj = nullptr;
std::function<int(GRBmodel* model)> GRBupdatemodel = nullptr;
std::function<int(GRBmodel* model)> GRBresetmodel = nullptr;
std::function<int(GRBmodel* model)> GRBfreemodel = nullptr;
std::function<int(GRBmodel* model)> GRBcomputeIIS = nullptr;
std::function<int(GRBmodel* model, GRBsvec* b, GRBsvec* x)> GRBFSolve = nullptr;
std::function<int(GRBmodel* model, int j, GRBsvec* x)> GRBBinvColj = nullptr;
std::function<int(GRBmodel* model, int j, GRBsvec* x)> GRBBinvj = nullptr;
std::function<int(GRBmodel* model, GRBsvec* b, GRBsvec* x)> GRBBSolve = nullptr;
std::function<int(GRBmodel* model, int i, GRBsvec* x)> GRBBinvi = nullptr;
std::function<int(GRBmodel* model, int i, GRBsvec* x)> GRBBinvRowi = nullptr;
std::function<int(GRBmodel* model, int* bhead)> GRBgetBasisHead = nullptr;
std::function<int(GRBmodel* model, int num, int* cand, double* downobjbd,
                  double* upobjbd, int* statusP)>
    GRBstrongbranch = nullptr;
std::function<int(GRBmodel* model)> GRBcheckmodel = nullptr;
std::function<void(GRBmodel* model)> GRBsetsignal = nullptr;
std::function<void(GRBmodel* model)> GRBterminate = nullptr;
std::function<int(const char* filename)> GRBreplay = nullptr;
std::function<int(GRBmodel* model, int sense, double constant, int lnz,
                  int* lind, double* lval, int qnz, int* qrow, int* qcol,
                  double* qval)>
    GRBsetobjective = nullptr;
std::function<int(GRBmodel* model, int index, int priority, double weight,
                  double abstol, double reltol, const char* name,
                  double constant, int lnz, int* lind, double* lval)>
    GRBsetobjectiven = nullptr;
std::function<void(GRBenv* env, const char* message)> GRBmsg = nullptr;
std::function<int(GRBenv* env, FILE** logfileP)> GRBgetlogfile = nullptr;
std::function<int(GRBenv* env, FILE* logfile)> GRBsetlogfile = nullptr;
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
std::function<int(GRBenv* env, const char* paramname, const char* value)>
    GRBsetparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, int value)>
    GRBsetintparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, double value)>
    GRBsetdblparam = nullptr;
std::function<int(GRBenv* env, const char* paramname, const char* value)>
    GRBsetstrparam = nullptr;
std::function<int(GRBenv* env, const char* paramname)> GRBgetparamtype =
    nullptr;
std::function<int(GRBenv* env)> GRBresetparams = nullptr;
std::function<int(GRBenv* dest, GRBenv* src)> GRBcopyparams = nullptr;
std::function<int(GRBenv* env, const char* filename)> GRBwriteparams = nullptr;
std::function<int(GRBenv* env, const char* filename)> GRBreadparams = nullptr;
std::function<int(GRBenv* env)> GRBgetnumparams = nullptr;
std::function<int(GRBenv* env, int i, char** paramnameP)> GRBgetparamname =
    nullptr;
std::function<int(GRBmodel* model)> GRBgetnumattributes = nullptr;
std::function<int(GRBmodel* model, int i, char** attrnameP)> GRBgetattrname =
    nullptr;
std::function<int(GRBenv** envP, const char* logfilename)> GRBloadenv = nullptr;
std::function<int(GRBenv** envP, const char* logfilename, int apitype,
                  int major, int minor, int tech,
                  int(GUROBI_STDCALL* cb)(CB_ARGS), void* usrdata)>
    GRBloadenvadv = nullptr;
std::function<int(GRBenv** envP, const char* logfilename,
                  const char* computeservers, int port, const char* password,
                  int priority, double timeout)>
    GRBloadclientenv = nullptr;
std::function<int(GRBenv** envP, const char* logfilename,
                  const char* computeservers, int port, const char* password,
                  int priority, double timeout, int apitype, int major,
                  int minor, int tech, int(GUROBI_STDCALL* cb)(CB_ARGS),
                  void* usrdata)>
    GRBloadclientenvadv = nullptr;
std::function<int(GRBenv** envP, const char* logfilename, const char* accessID,
                  const char* secretKey, const char* pool)>
    GRBloadcloudenv = nullptr;
std::function<int(GRBenv** envP, const char* logfilename, const char* accessID,
                  const char* secretKey, const char* pool, int apitype,
                  int major, int minor, int tech,
                  int(GUROBI_STDCALL* cb)(CB_ARGS), void* usrdata)>
    GRBloadcloudenvadv = nullptr;
std::function<GRBenv*(GRBmodel* model)> GRBgetenv = nullptr;
std::function<GRBenv*(GRBmodel* model, int num)> GRBgetconcurrentenv = nullptr;
std::function<void(GRBmodel* model)> GRBdiscardconcurrentenvs = nullptr;
std::function<GRBenv*(GRBmodel* model, int num)> GRBgetmultiobjenv = nullptr;
std::function<void(GRBmodel* model)> GRBdiscardmultiobjenvs = nullptr;
std::function<void(GRBenv* env)> GRBreleaselicense = nullptr;
std::function<void(GRBenv* env)> GRBfreeenv = nullptr;
std::function<const char*(GRBenv* env)> GRBgeterrormsg = nullptr;
std::function<const char*(GRBmodel* model)> GRBgetmerrormsg = nullptr;
std::function<void(int* majorP, int* minorP, int* technicalP)> GRBversion =
    nullptr;
std::function<char*(void)> GRBplatform = nullptr;
std::function<int(GRBmodel* model)> GRBtunemodel = nullptr;
std::function<int(int nummodels, GRBmodel** models, GRBmodel* ignore,
                  GRBmodel* hint)>
    GRBtunemodels = nullptr;
std::function<int(GRBmodel* model, int i)> GRBgettuneresult = nullptr;
std::function<int(GRBmodel* model, int i, char** logP)> GRBgettunelog = nullptr;
std::function<int(GRBmodel* model, GRBmodel* ignore, GRBmodel* hint)>
    GRBtunemodeladv = nullptr;
std::function<int(GRBmodel* model)> GRBsync = nullptr;

void LoadGurobiFunctions(DynamicLibrary* gurobi_dynamic_library) {
  // This was generated with the parse_header.py script.
  // See the comment at the top of the script.

  // This is the 'assign' section.

  gurobi_dynamic_library->GetFunction(&GRBisqp, "GRBisqp");
  gurobi_dynamic_library->GetFunction(&GRBgetattrinfo, "GRBgetattrinfo");
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
  gurobi_dynamic_library->GetFunction(&GRBgetintattrlist, "GRBgetintattrlist");
  gurobi_dynamic_library->GetFunction(&GRBsetintattrlist, "GRBsetintattrlist");
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrelement,
                                      "GRBgetcharattrelement");
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrelement,
                                      "GRBsetcharattrelement");
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrarray,
                                      "GRBgetcharattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrarray,
                                      "GRBsetcharattrarray");
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrlist,
                                      "GRBgetcharattrlist");
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
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrlist, "GRBgetdblattrlist");
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrlist, "GRBsetdblattrlist");
  gurobi_dynamic_library->GetFunction(&GRBgetstrattr, "GRBgetstrattr");
  gurobi_dynamic_library->GetFunction(&GRBsetstrattr, "GRBsetstrattr");
  gurobi_dynamic_library->GetFunction(&GRBgetstrattrelement,
                                      "GRBgetstrattrelement");
  gurobi_dynamic_library->GetFunction(&GRBsetstrattrelement,
                                      "GRBsetstrattrelement");
  gurobi_dynamic_library->GetFunction(&GRBgetstrattrarray,
                                      "GRBgetstrattrarray");
  gurobi_dynamic_library->GetFunction(&GRBsetstrattrarray,
                                      "GRBsetstrattrarray");
  gurobi_dynamic_library->GetFunction(&GRBgetstrattrlist, "GRBgetstrattrlist");
  gurobi_dynamic_library->GetFunction(&GRBsetstrattrlist, "GRBsetstrattrlist");
  gurobi_dynamic_library->GetFunction(&GRBsetcallbackfunc,
                                      "GRBsetcallbackfunc");
  gurobi_dynamic_library->GetFunction(&GRBgetcallbackfunc,
                                      "GRBgetcallbackfunc");
  gurobi_dynamic_library->GetFunction(&GRBsetlogcallbackfunc,
                                      "GRBsetlogcallbackfunc");
  gurobi_dynamic_library->GetFunction(&GRBsetlogcallbackfuncenv,
                                      "GRBsetlogcallbackfuncenv");
  gurobi_dynamic_library->GetFunction(&GRBcbget, "GRBcbget");
  gurobi_dynamic_library->GetFunction(&GRBcbsetparam, "GRBcbsetparam");
  gurobi_dynamic_library->GetFunction(&GRBcbsolution, "GRBcbsolution");
  gurobi_dynamic_library->GetFunction(&GRBcbcut, "GRBcbcut");
  gurobi_dynamic_library->GetFunction(&GRBcblazy, "GRBcblazy");
  gurobi_dynamic_library->GetFunction(&GRBgetcoeff, "GRBgetcoeff");
  gurobi_dynamic_library->GetFunction(&GRBgetconstrs, "GRBgetconstrs");
  gurobi_dynamic_library->GetFunction(&GRBXgetconstrs, "GRBXgetconstrs");
  gurobi_dynamic_library->GetFunction(&GRBgetvars, "GRBgetvars");
  gurobi_dynamic_library->GetFunction(&GRBXgetvars, "GRBXgetvars");
  gurobi_dynamic_library->GetFunction(&GRBgetsos, "GRBgetsos");
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrMax,
                                      "GRBgetgenconstrMax");
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrMin,
                                      "GRBgetgenconstrMin");
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrAbs,
                                      "GRBgetgenconstrAbs");
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrAnd,
                                      "GRBgetgenconstrAnd");
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrOr, "GRBgetgenconstrOr");
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrIndicator,
                                      "GRBgetgenconstrIndicator");
  gurobi_dynamic_library->GetFunction(&GRBgetq, "GRBgetq");
  gurobi_dynamic_library->GetFunction(&GRBgetqconstr, "GRBgetqconstr");
  gurobi_dynamic_library->GetFunction(&GRBgetvarbyname, "GRBgetvarbyname");
  gurobi_dynamic_library->GetFunction(&GRBgetconstrbyname,
                                      "GRBgetconstrbyname");
  gurobi_dynamic_library->GetFunction(&GRBgetpwlobj, "GRBgetpwlobj");
  gurobi_dynamic_library->GetFunction(&GRBoptimize, "GRBoptimize");
  gurobi_dynamic_library->GetFunction(&GRBoptimizeasync, "GRBoptimizeasync");
  gurobi_dynamic_library->GetFunction(&GRBcopymodel, "GRBcopymodel");
  gurobi_dynamic_library->GetFunction(&GRBfixedmodel, "GRBfixedmodel");
  gurobi_dynamic_library->GetFunction(&GRBfeasrelax, "GRBfeasrelax");
  gurobi_dynamic_library->GetFunction(&GRBgetcbwhatinfo, "GRBgetcbwhatinfo");
  gurobi_dynamic_library->GetFunction(&GRBrelaxmodel, "GRBrelaxmodel");
  gurobi_dynamic_library->GetFunction(&GRBconverttofixed, "GRBconverttofixed");
  gurobi_dynamic_library->GetFunction(&GRBpresolvemodel, "GRBpresolvemodel");
  gurobi_dynamic_library->GetFunction(&GRBiismodel, "GRBiismodel");
  gurobi_dynamic_library->GetFunction(&GRBfeasibility, "GRBfeasibility");
  gurobi_dynamic_library->GetFunction(&GRBlinearizemodel, "GRBlinearizemodel");
  gurobi_dynamic_library->GetFunction(&GRBloadenvsyscb, "GRBloadenvsyscb");
  gurobi_dynamic_library->GetFunction(&GRBreadmodel, "GRBreadmodel");
  gurobi_dynamic_library->GetFunction(&GRBread, "GRBread");
  gurobi_dynamic_library->GetFunction(&GRBwrite, "GRBwrite");
  gurobi_dynamic_library->GetFunction(&GRBismodelfile, "GRBismodelfile");
  gurobi_dynamic_library->GetFunction(&GRBfiletype, "GRBfiletype");
  gurobi_dynamic_library->GetFunction(&GRBisrecordfile, "GRBisrecordfile");
  gurobi_dynamic_library->GetFunction(&GRBnewmodel, "GRBnewmodel");
  gurobi_dynamic_library->GetFunction(&GRBloadmodel, "GRBloadmodel");
  gurobi_dynamic_library->GetFunction(&GRBXloadmodel, "GRBXloadmodel");
  gurobi_dynamic_library->GetFunction(&GRBaddvar, "GRBaddvar");
  gurobi_dynamic_library->GetFunction(&GRBaddvars, "GRBaddvars");
  gurobi_dynamic_library->GetFunction(&GRBXaddvars, "GRBXaddvars");
  gurobi_dynamic_library->GetFunction(&GRBaddconstr, "GRBaddconstr");
  gurobi_dynamic_library->GetFunction(&GRBaddconstrs, "GRBaddconstrs");
  gurobi_dynamic_library->GetFunction(&GRBXaddconstrs, "GRBXaddconstrs");
  gurobi_dynamic_library->GetFunction(&GRBaddrangeconstr, "GRBaddrangeconstr");
  gurobi_dynamic_library->GetFunction(&GRBaddrangeconstrs,
                                      "GRBaddrangeconstrs");
  gurobi_dynamic_library->GetFunction(&GRBXaddrangeconstrs,
                                      "GRBXaddrangeconstrs");
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
  gurobi_dynamic_library->GetFunction(&GRBaddcone, "GRBaddcone");
  gurobi_dynamic_library->GetFunction(&GRBaddqpterms, "GRBaddqpterms");
  gurobi_dynamic_library->GetFunction(&GRBdelvars, "GRBdelvars");
  gurobi_dynamic_library->GetFunction(&GRBdelconstrs, "GRBdelconstrs");
  gurobi_dynamic_library->GetFunction(&GRBdelsos, "GRBdelsos");
  gurobi_dynamic_library->GetFunction(&GRBdelgenconstrs, "GRBdelgenconstrs");
  gurobi_dynamic_library->GetFunction(&GRBdelqconstrs, "GRBdelqconstrs");
  gurobi_dynamic_library->GetFunction(&GRBdelq, "GRBdelq");
  gurobi_dynamic_library->GetFunction(&GRBchgcoeffs, "GRBchgcoeffs");
  gurobi_dynamic_library->GetFunction(&GRBXchgcoeffs, "GRBXchgcoeffs");
  gurobi_dynamic_library->GetFunction(&GRBsetpwlobj, "GRBsetpwlobj");
  gurobi_dynamic_library->GetFunction(&GRBupdatemodel, "GRBupdatemodel");
  gurobi_dynamic_library->GetFunction(&GRBresetmodel, "GRBresetmodel");
  gurobi_dynamic_library->GetFunction(&GRBfreemodel, "GRBfreemodel");
  gurobi_dynamic_library->GetFunction(&GRBcomputeIIS, "GRBcomputeIIS");
  gurobi_dynamic_library->GetFunction(&GRBFSolve, "GRBFSolve");
  gurobi_dynamic_library->GetFunction(&GRBBinvColj, "GRBBinvColj");
  gurobi_dynamic_library->GetFunction(&GRBBinvj, "GRBBinvj");
  gurobi_dynamic_library->GetFunction(&GRBBSolve, "GRBBSolve");
  gurobi_dynamic_library->GetFunction(&GRBBinvi, "GRBBinvi");
  gurobi_dynamic_library->GetFunction(&GRBBinvRowi, "GRBBinvRowi");
  gurobi_dynamic_library->GetFunction(&GRBgetBasisHead, "GRBgetBasisHead");
  gurobi_dynamic_library->GetFunction(&GRBstrongbranch, "GRBstrongbranch");
  gurobi_dynamic_library->GetFunction(&GRBcheckmodel, "GRBcheckmodel");
  gurobi_dynamic_library->GetFunction(&GRBsetsignal, "GRBsetsignal");
  gurobi_dynamic_library->GetFunction(&GRBterminate, "GRBterminate");
  gurobi_dynamic_library->GetFunction(&GRBreplay, "GRBreplay");
  gurobi_dynamic_library->GetFunction(&GRBsetobjective, "GRBsetobjective");
  gurobi_dynamic_library->GetFunction(&GRBsetobjectiven, "GRBsetobjectiven");
  gurobi_dynamic_library->GetFunction(&GRBmsg, "GRBmsg");
  gurobi_dynamic_library->GetFunction(&GRBgetlogfile, "GRBgetlogfile");
  gurobi_dynamic_library->GetFunction(&GRBsetlogfile, "GRBsetlogfile");
  gurobi_dynamic_library->GetFunction(&GRBgetintparam, "GRBgetintparam");
  gurobi_dynamic_library->GetFunction(&GRBgetdblparam, "GRBgetdblparam");
  gurobi_dynamic_library->GetFunction(&GRBgetstrparam, "GRBgetstrparam");
  gurobi_dynamic_library->GetFunction(&GRBgetintparaminfo,
                                      "GRBgetintparaminfo");
  gurobi_dynamic_library->GetFunction(&GRBgetdblparaminfo,
                                      "GRBgetdblparaminfo");
  gurobi_dynamic_library->GetFunction(&GRBgetstrparaminfo,
                                      "GRBgetstrparaminfo");
  gurobi_dynamic_library->GetFunction(&GRBsetparam, "GRBsetparam");
  gurobi_dynamic_library->GetFunction(&GRBsetintparam, "GRBsetintparam");
  gurobi_dynamic_library->GetFunction(&GRBsetdblparam, "GRBsetdblparam");
  gurobi_dynamic_library->GetFunction(&GRBsetstrparam, "GRBsetstrparam");
  gurobi_dynamic_library->GetFunction(&GRBgetparamtype, "GRBgetparamtype");
  gurobi_dynamic_library->GetFunction(&GRBresetparams, "GRBresetparams");
  gurobi_dynamic_library->GetFunction(&GRBcopyparams, "GRBcopyparams");
  gurobi_dynamic_library->GetFunction(&GRBwriteparams, "GRBwriteparams");
  gurobi_dynamic_library->GetFunction(&GRBreadparams, "GRBreadparams");
  gurobi_dynamic_library->GetFunction(&GRBgetnumparams, "GRBgetnumparams");
  gurobi_dynamic_library->GetFunction(&GRBgetparamname, "GRBgetparamname");
  gurobi_dynamic_library->GetFunction(&GRBgetnumattributes,
                                      "GRBgetnumattributes");
  gurobi_dynamic_library->GetFunction(&GRBgetattrname, "GRBgetattrname");
  gurobi_dynamic_library->GetFunction(&GRBloadenv, "GRBloadenv");
  gurobi_dynamic_library->GetFunction(&GRBloadenvadv, "GRBloadenvadv");
  gurobi_dynamic_library->GetFunction(&GRBloadclientenv, "GRBloadclientenv");
  gurobi_dynamic_library->GetFunction(&GRBloadclientenvadv,
                                      "GRBloadclientenvadv");
  gurobi_dynamic_library->GetFunction(&GRBloadcloudenv, "GRBloadcloudenv");
  gurobi_dynamic_library->GetFunction(&GRBloadcloudenvadv,
                                      "GRBloadcloudenvadv");
  gurobi_dynamic_library->GetFunction(&GRBgetenv, "GRBgetenv");
  gurobi_dynamic_library->GetFunction(&GRBgetconcurrentenv,
                                      "GRBgetconcurrentenv");
  gurobi_dynamic_library->GetFunction(&GRBdiscardconcurrentenvs,
                                      "GRBdiscardconcurrentenvs");
  gurobi_dynamic_library->GetFunction(&GRBgetmultiobjenv, "GRBgetmultiobjenv");
  gurobi_dynamic_library->GetFunction(&GRBdiscardmultiobjenvs,
                                      "GRBdiscardmultiobjenvs");
  gurobi_dynamic_library->GetFunction(&GRBreleaselicense, "GRBreleaselicense");
  gurobi_dynamic_library->GetFunction(&GRBfreeenv, "GRBfreeenv");
  gurobi_dynamic_library->GetFunction(&GRBgeterrormsg, "GRBgeterrormsg");
  gurobi_dynamic_library->GetFunction(&GRBgetmerrormsg, "GRBgetmerrormsg");
  gurobi_dynamic_library->GetFunction(&GRBversion, "GRBversion");
  gurobi_dynamic_library->GetFunction(&GRBplatform, "GRBplatform");
  gurobi_dynamic_library->GetFunction(&GRBtunemodel, "GRBtunemodel");
  gurobi_dynamic_library->GetFunction(&GRBtunemodels, "GRBtunemodels");
  gurobi_dynamic_library->GetFunction(&GRBgettuneresult, "GRBgettuneresult");
  gurobi_dynamic_library->GetFunction(&GRBgettunelog, "GRBgettunelog");
  gurobi_dynamic_library->GetFunction(&GRBtunemodeladv, "GRBtunemodeladv");
  gurobi_dynamic_library->GetFunction(&GRBsync, "GRBsync");
}

std::vector<std::string> GurobiDynamicLibraryPotentialPaths() {
  std::vector<std::string> potential_paths;
  const std::vector<std::string> kGurobiVersions = {
      "951", "950", "911", "910", "903", "902", "811", "801", "752"};

  // Look for libraries pointed by GUROBI_HOME first.
  const char* gurobi_home_from_env = getenv("GUROBI_HOME");
  if (gurobi_home_from_env != nullptr) {
    for (const std::string& version : kGurobiVersions) {
      const std::string lib = version.substr(0, 2);
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
    const std::string lib = version.substr(0, 2);
#if defined(_MSC_VER)  // Windows
    potential_paths.push_back(absl::StrCat("C:\\Program Files\\gurobi", version,
                                           "\\win64\\bin\\gurobi", lib,
                                           ".dll"));
#elif defined(__APPLE__)  // OS X
    potential_paths.push_back(absl::StrCat(
        "/Library/gurobi", version, "/mac64/lib/libgurobi", lib, ".dylib"));
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
