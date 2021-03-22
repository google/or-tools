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

#include "ortools/gurobi/environment.h"

#include <string>
#include <mutex>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
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
  gurobi_dynamic_library->GetFunction(&GRBisqp, STRINGIFY(GRBisqp));
  gurobi_dynamic_library->GetFunction(&GRBgetattrinfo,
                                      STRINGIFY(GRBgetattrinfo));
  gurobi_dynamic_library->GetFunction(&GRBisattravailable,
                                      STRINGIFY(GRBisattravailable));
  gurobi_dynamic_library->GetFunction(&GRBgetintattr, STRINGIFY(GRBgetintattr));
  gurobi_dynamic_library->GetFunction(&GRBsetintattr, STRINGIFY(GRBsetintattr));
  gurobi_dynamic_library->GetFunction(&GRBgetintattrelement,
                                      STRINGIFY(GRBgetintattrelement));
  gurobi_dynamic_library->GetFunction(&GRBsetintattrelement,
                                      STRINGIFY(GRBsetintattrelement));
  gurobi_dynamic_library->GetFunction(&GRBgetintattrarray,
                                      STRINGIFY(GRBgetintattrarray));
  gurobi_dynamic_library->GetFunction(&GRBsetintattrarray,
                                      STRINGIFY(GRBsetintattrarray));
  gurobi_dynamic_library->GetFunction(&GRBgetintattrlist,
                                      STRINGIFY(GRBgetintattrlist));
  gurobi_dynamic_library->GetFunction(&GRBsetintattrlist,
                                      STRINGIFY(GRBsetintattrlist));
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrelement,
                                      STRINGIFY(GRBgetcharattrelement));
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrelement,
                                      STRINGIFY(GRBsetcharattrelement));
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrarray,
                                      STRINGIFY(GRBgetcharattrarray));
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrarray,
                                      STRINGIFY(GRBsetcharattrarray));
  gurobi_dynamic_library->GetFunction(&GRBgetcharattrlist,
                                      STRINGIFY(GRBgetcharattrlist));
  gurobi_dynamic_library->GetFunction(&GRBsetcharattrlist,
                                      STRINGIFY(GRBsetcharattrlist));
  gurobi_dynamic_library->GetFunction(&GRBgetdblattr, STRINGIFY(GRBgetdblattr));
  gurobi_dynamic_library->GetFunction(&GRBsetdblattr, STRINGIFY(GRBsetdblattr));
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrelement,
                                      STRINGIFY(GRBgetdblattrelement));
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrelement,
                                      STRINGIFY(GRBsetdblattrelement));
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrarray,
                                      STRINGIFY(GRBgetdblattrarray));
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrarray,
                                      STRINGIFY(GRBsetdblattrarray));
  gurobi_dynamic_library->GetFunction(&GRBgetdblattrlist,
                                      STRINGIFY(GRBgetdblattrlist));
  gurobi_dynamic_library->GetFunction(&GRBsetdblattrlist,
                                      STRINGIFY(GRBsetdblattrlist));
  gurobi_dynamic_library->GetFunction(&GRBgetstrattr, STRINGIFY(GRBgetstrattr));
  gurobi_dynamic_library->GetFunction(&GRBsetstrattr, STRINGIFY(GRBsetstrattr));
  gurobi_dynamic_library->GetFunction(&GRBgetstrattrelement,
                                      STRINGIFY(GRBgetstrattrelement));
  gurobi_dynamic_library->GetFunction(&GRBsetstrattrelement,
                                      STRINGIFY(GRBsetstrattrelement));
  gurobi_dynamic_library->GetFunction(&GRBgetstrattrarray,
                                      STRINGIFY(GRBgetstrattrarray));
  gurobi_dynamic_library->GetFunction(&GRBsetstrattrarray,
                                      STRINGIFY(GRBsetstrattrarray));
  gurobi_dynamic_library->GetFunction(&GRBgetstrattrlist,
                                      STRINGIFY(GRBgetstrattrlist));
  gurobi_dynamic_library->GetFunction(&GRBsetstrattrlist,
                                      STRINGIFY(GRBsetstrattrlist));
  gurobi_dynamic_library->GetFunction(&GRBsetcallbackfunc,
                                      STRINGIFY(GRBsetcallbackfunc));
  gurobi_dynamic_library->GetFunction(&GRBgetcallbackfunc,
                                      STRINGIFY(GRBgetcallbackfunc));
  gurobi_dynamic_library->GetFunction(&GRBsetlogcallbackfunc,
                                      STRINGIFY(GRBsetlogcallbackfunc));
  gurobi_dynamic_library->GetFunction(&GRBsetlogcallbackfuncenv,
                                      STRINGIFY(GRBsetlogcallbackfuncenv));
  gurobi_dynamic_library->GetFunction(&GRBcbget, STRINGIFY(GRBcbget));
  gurobi_dynamic_library->GetFunction(&GRBcbsetparam, STRINGIFY(GRBcbsetparam));
  gurobi_dynamic_library->GetFunction(&GRBcbsolution, STRINGIFY(GRBcbsolution));
  gurobi_dynamic_library->GetFunction(&GRBcbcut, STRINGIFY(GRBcbcut));
  gurobi_dynamic_library->GetFunction(&GRBcblazy, STRINGIFY(GRBcblazy));
  gurobi_dynamic_library->GetFunction(&GRBgetcoeff, STRINGIFY(GRBgetcoeff));
  gurobi_dynamic_library->GetFunction(&GRBgetconstrs, STRINGIFY(GRBgetconstrs));
  gurobi_dynamic_library->GetFunction(&GRBXgetconstrs,
                                      STRINGIFY(GRBXgetconstrs));
  gurobi_dynamic_library->GetFunction(&GRBgetvars, STRINGIFY(GRBgetvars));
  gurobi_dynamic_library->GetFunction(&GRBXgetvars, STRINGIFY(GRBXgetvars));
  gurobi_dynamic_library->GetFunction(&GRBgetsos, STRINGIFY(GRBgetsos));
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrMax,
                                      STRINGIFY(GRBgetgenconstrMax));
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrMin,
                                      STRINGIFY(GRBgetgenconstrMin));
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrAbs,
                                      STRINGIFY(GRBgetgenconstrAbs));
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrAnd,
                                      STRINGIFY(GRBgetgenconstrAnd));
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrOr,
                                      STRINGIFY(GRBgetgenconstrOr));
  gurobi_dynamic_library->GetFunction(&GRBgetgenconstrIndicator,
                                      STRINGIFY(GRBgetgenconstrIndicator));
  gurobi_dynamic_library->GetFunction(&GRBgetq, STRINGIFY(GRBgetq));
  gurobi_dynamic_library->GetFunction(&GRBgetqconstr, STRINGIFY(GRBgetqconstr));
  gurobi_dynamic_library->GetFunction(&GRBgetvarbyname,
                                      STRINGIFY(GRBgetvarbyname));
  gurobi_dynamic_library->GetFunction(&GRBgetconstrbyname,
                                      STRINGIFY(GRBgetconstrbyname));
  gurobi_dynamic_library->GetFunction(&GRBgetpwlobj, STRINGIFY(GRBgetpwlobj));
  gurobi_dynamic_library->GetFunction(&GRBoptimize, STRINGIFY(GRBoptimize));
  gurobi_dynamic_library->GetFunction(&GRBoptimizeasync,
                                      STRINGIFY(GRBoptimizeasync));
  gurobi_dynamic_library->GetFunction(&GRBcopymodel, STRINGIFY(GRBcopymodel));
  gurobi_dynamic_library->GetFunction(&GRBfixedmodel, STRINGIFY(GRBfixedmodel));
  gurobi_dynamic_library->GetFunction(&GRBfeasrelax, STRINGIFY(GRBfeasrelax));
  gurobi_dynamic_library->GetFunction(&GRBgetcbwhatinfo,
                                      STRINGIFY(GRBgetcbwhatinfo));
  gurobi_dynamic_library->GetFunction(&GRBrelaxmodel, STRINGIFY(GRBrelaxmodel));
  gurobi_dynamic_library->GetFunction(&GRBconverttofixed,
                                      STRINGIFY(GRBconverttofixed));
  gurobi_dynamic_library->GetFunction(&GRBpresolvemodel,
                                      STRINGIFY(GRBpresolvemodel));
  gurobi_dynamic_library->GetFunction(&GRBiismodel, STRINGIFY(GRBiismodel));
  gurobi_dynamic_library->GetFunction(&GRBfeasibility,
                                      STRINGIFY(GRBfeasibility));
  gurobi_dynamic_library->GetFunction(&GRBlinearizemodel,
                                      STRINGIFY(GRBlinearizemodel));
  gurobi_dynamic_library->GetFunction(&GRBloadenvsyscb,
                                      STRINGIFY(GRBloadenvsyscb));
  gurobi_dynamic_library->GetFunction(&GRBreadmodel, STRINGIFY(GRBreadmodel));
  gurobi_dynamic_library->GetFunction(&GRBread, STRINGIFY(GRBread));
  gurobi_dynamic_library->GetFunction(&GRBwrite, STRINGIFY(GRBwrite));
  gurobi_dynamic_library->GetFunction(&GRBismodelfile,
                                      STRINGIFY(GRBismodelfile));
  gurobi_dynamic_library->GetFunction(&GRBfiletype, STRINGIFY(GRBfiletype));
  gurobi_dynamic_library->GetFunction(&GRBisrecordfile,
                                      STRINGIFY(GRBisrecordfile));
  gurobi_dynamic_library->GetFunction(&GRBnewmodel, STRINGIFY(GRBnewmodel));
  gurobi_dynamic_library->GetFunction(&GRBloadmodel, STRINGIFY(GRBloadmodel));
  gurobi_dynamic_library->GetFunction(&GRBXloadmodel, STRINGIFY(GRBXloadmodel));
  gurobi_dynamic_library->GetFunction(&GRBaddvar, STRINGIFY(GRBaddvar));
  gurobi_dynamic_library->GetFunction(&GRBaddvars, STRINGIFY(GRBaddvars));
  gurobi_dynamic_library->GetFunction(&GRBXaddvars, STRINGIFY(GRBXaddvars));
  gurobi_dynamic_library->GetFunction(&GRBaddconstr, STRINGIFY(GRBaddconstr));
  gurobi_dynamic_library->GetFunction(&GRBaddconstrs, STRINGIFY(GRBaddconstrs));
  gurobi_dynamic_library->GetFunction(&GRBXaddconstrs,
                                      STRINGIFY(GRBXaddconstrs));
  gurobi_dynamic_library->GetFunction(&GRBaddrangeconstr,
                                      STRINGIFY(GRBaddrangeconstr));
  gurobi_dynamic_library->GetFunction(&GRBaddrangeconstrs,
                                      STRINGIFY(GRBaddrangeconstrs));
  gurobi_dynamic_library->GetFunction(&GRBXaddrangeconstrs,
                                      STRINGIFY(GRBXaddrangeconstrs));
  gurobi_dynamic_library->GetFunction(&GRBaddsos, STRINGIFY(GRBaddsos));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrMax,
                                      STRINGIFY(GRBaddgenconstrMax));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrMin,
                                      STRINGIFY(GRBaddgenconstrMin));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrAbs,
                                      STRINGIFY(GRBaddgenconstrAbs));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrAnd,
                                      STRINGIFY(GRBaddgenconstrAnd));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrOr,
                                      STRINGIFY(GRBaddgenconstrOr));
  gurobi_dynamic_library->GetFunction(&GRBaddgenconstrIndicator,
                                      STRINGIFY(GRBaddgenconstrIndicator));
  gurobi_dynamic_library->GetFunction(&GRBaddqconstr, STRINGIFY(GRBaddqconstr));
  gurobi_dynamic_library->GetFunction(&GRBaddcone, STRINGIFY(GRBaddcone));
  gurobi_dynamic_library->GetFunction(&GRBaddqpterms, STRINGIFY(GRBaddqpterms));
  gurobi_dynamic_library->GetFunction(&GRBdelvars, STRINGIFY(GRBdelvars));
  gurobi_dynamic_library->GetFunction(&GRBdelconstrs, STRINGIFY(GRBdelconstrs));
  gurobi_dynamic_library->GetFunction(&GRBdelsos, STRINGIFY(GRBdelsos));
  gurobi_dynamic_library->GetFunction(&GRBdelgenconstrs,
                                      STRINGIFY(GRBdelgenconstrs));
  gurobi_dynamic_library->GetFunction(&GRBdelqconstrs,
                                      STRINGIFY(GRBdelqconstrs));
  gurobi_dynamic_library->GetFunction(&GRBdelq, STRINGIFY(GRBdelq));
  gurobi_dynamic_library->GetFunction(&GRBchgcoeffs, STRINGIFY(GRBchgcoeffs));
  gurobi_dynamic_library->GetFunction(&GRBXchgcoeffs, STRINGIFY(GRBXchgcoeffs));
  gurobi_dynamic_library->GetFunction(&GRBsetpwlobj, STRINGIFY(GRBsetpwlobj));
  gurobi_dynamic_library->GetFunction(&GRBupdatemodel,
                                      STRINGIFY(GRBupdatemodel));
  gurobi_dynamic_library->GetFunction(&GRBresetmodel, STRINGIFY(GRBresetmodel));
  gurobi_dynamic_library->GetFunction(&GRBfreemodel, STRINGIFY(GRBfreemodel));
  gurobi_dynamic_library->GetFunction(&GRBcomputeIIS, STRINGIFY(GRBcomputeIIS));
  gurobi_dynamic_library->GetFunction(&GRBFSolve, STRINGIFY(GRBFSolve));
  gurobi_dynamic_library->GetFunction(&GRBBinvColj, STRINGIFY(GRBBinvColj));
  gurobi_dynamic_library->GetFunction(&GRBBinvj, STRINGIFY(GRBBinvj));
  gurobi_dynamic_library->GetFunction(&GRBBSolve, STRINGIFY(GRBBSolve));
  gurobi_dynamic_library->GetFunction(&GRBBinvi, STRINGIFY(GRBBinvi));
  gurobi_dynamic_library->GetFunction(&GRBBinvRowi, STRINGIFY(GRBBinvRowi));
  gurobi_dynamic_library->GetFunction(&GRBgetBasisHead,
                                      STRINGIFY(GRBgetBasisHead));
  gurobi_dynamic_library->GetFunction(&GRBstrongbranch,
                                      STRINGIFY(GRBstrongbranch));
  gurobi_dynamic_library->GetFunction(&GRBcheckmodel, STRINGIFY(GRBcheckmodel));
  gurobi_dynamic_library->GetFunction(&GRBsetsignal, STRINGIFY(GRBsetsignal));
  gurobi_dynamic_library->GetFunction(&GRBterminate, STRINGIFY(GRBterminate));
  gurobi_dynamic_library->GetFunction(&GRBreplay, STRINGIFY(GRBreplay));
  gurobi_dynamic_library->GetFunction(&GRBsetobjective,
                                      STRINGIFY(GRBsetobjective));
  gurobi_dynamic_library->GetFunction(&GRBsetobjectiven,
                                      STRINGIFY(GRBsetobjectiven));
  gurobi_dynamic_library->GetFunction(&GRBmsg, STRINGIFY(GRBmsg));
  gurobi_dynamic_library->GetFunction(&GRBgetlogfile, STRINGIFY(GRBgetlogfile));
  gurobi_dynamic_library->GetFunction(&GRBsetlogfile, STRINGIFY(GRBsetlogfile));
  gurobi_dynamic_library->GetFunction(&GRBgetintparam,
                                      STRINGIFY(GRBgetintparam));
  gurobi_dynamic_library->GetFunction(&GRBgetdblparam,
                                      STRINGIFY(GRBgetdblparam));
  gurobi_dynamic_library->GetFunction(&GRBgetstrparam,
                                      STRINGIFY(GRBgetstrparam));
  gurobi_dynamic_library->GetFunction(&GRBgetintparaminfo,
                                      STRINGIFY(GRBgetintparaminfo));
  gurobi_dynamic_library->GetFunction(&GRBgetdblparaminfo,
                                      STRINGIFY(GRBgetdblparaminfo));
  gurobi_dynamic_library->GetFunction(&GRBgetstrparaminfo,
                                      STRINGIFY(GRBgetstrparaminfo));
  gurobi_dynamic_library->GetFunction(&GRBsetparam, STRINGIFY(GRBsetparam));
  gurobi_dynamic_library->GetFunction(&GRBsetintparam,
                                      STRINGIFY(GRBsetintparam));
  gurobi_dynamic_library->GetFunction(&GRBsetdblparam,
                                      STRINGIFY(GRBsetdblparam));
  gurobi_dynamic_library->GetFunction(&GRBsetstrparam,
                                      STRINGIFY(GRBsetstrparam));
  gurobi_dynamic_library->GetFunction(&GRBgetparamtype,
                                      STRINGIFY(GRBgetparamtype));
  gurobi_dynamic_library->GetFunction(&GRBresetparams,
                                      STRINGIFY(GRBresetparams));
  gurobi_dynamic_library->GetFunction(&GRBcopyparams, STRINGIFY(GRBcopyparams));
  gurobi_dynamic_library->GetFunction(&GRBwriteparams,
                                      STRINGIFY(GRBwriteparams));
  gurobi_dynamic_library->GetFunction(&GRBreadparams, STRINGIFY(GRBreadparams));
  gurobi_dynamic_library->GetFunction(&GRBgetnumparams,
                                      STRINGIFY(GRBgetnumparams));
  gurobi_dynamic_library->GetFunction(&GRBgetparamname,
                                      STRINGIFY(GRBgetparamname));
  gurobi_dynamic_library->GetFunction(&GRBgetnumattributes,
                                      STRINGIFY(GRBgetnumattributes));
  gurobi_dynamic_library->GetFunction(&GRBgetattrname,
                                      STRINGIFY(GRBgetattrname));
  gurobi_dynamic_library->GetFunction(&GRBloadenv, STRINGIFY(GRBloadenv));
  gurobi_dynamic_library->GetFunction(&GRBloadenvadv, STRINGIFY(GRBloadenvadv));
  gurobi_dynamic_library->GetFunction(&GRBloadclientenv,
                                      STRINGIFY(GRBloadclientenv));
  gurobi_dynamic_library->GetFunction(&GRBloadclientenvadv,
                                      STRINGIFY(GRBloadclientenvadv));
  gurobi_dynamic_library->GetFunction(&GRBloadcloudenv,
                                      STRINGIFY(GRBloadcloudenv));
  gurobi_dynamic_library->GetFunction(&GRBloadcloudenvadv,
                                      STRINGIFY(GRBloadcloudenvadv));
  gurobi_dynamic_library->GetFunction(&GRBgetenv, STRINGIFY(GRBgetenv));
  gurobi_dynamic_library->GetFunction(&GRBgetconcurrentenv,
                                      STRINGIFY(GRBgetconcurrentenv));
  gurobi_dynamic_library->GetFunction(&GRBdiscardconcurrentenvs,
                                      STRINGIFY(GRBdiscardconcurrentenvs));
  gurobi_dynamic_library->GetFunction(&GRBgetmultiobjenv,
                                      STRINGIFY(GRBgetmultiobjenv));
  gurobi_dynamic_library->GetFunction(&GRBdiscardmultiobjenvs,
                                      STRINGIFY(GRBdiscardmultiobjenvs));
  gurobi_dynamic_library->GetFunction(&GRBreleaselicense,
                                      STRINGIFY(GRBreleaselicense));
  gurobi_dynamic_library->GetFunction(&GRBfreeenv, STRINGIFY(GRBfreeenv));
  gurobi_dynamic_library->GetFunction(&GRBgeterrormsg,
                                      STRINGIFY(GRBgeterrormsg));
  gurobi_dynamic_library->GetFunction(&GRBgetmerrormsg,
                                      STRINGIFY(GRBgetmerrormsg));
  gurobi_dynamic_library->GetFunction(&GRBversion, STRINGIFY(GRBversion));
  gurobi_dynamic_library->GetFunction(&GRBplatform, STRINGIFY(GRBplatform));
  gurobi_dynamic_library->GetFunction(&GRBtunemodel, STRINGIFY(GRBtunemodel));
  gurobi_dynamic_library->GetFunction(&GRBtunemodels, STRINGIFY(GRBtunemodels));
  gurobi_dynamic_library->GetFunction(&GRBgettuneresult,
                                      STRINGIFY(GRBgettuneresult));
  gurobi_dynamic_library->GetFunction(&GRBgettunelog, STRINGIFY(GRBgettunelog));
  gurobi_dynamic_library->GetFunction(&GRBtunemodeladv,
                                      STRINGIFY(GRBtunemodeladv));
  gurobi_dynamic_library->GetFunction(&GRBsync, STRINGIFY(GRBsync));
}

std::vector<std::string> GurobiDynamicLibraryPotentialPaths() {
  std::vector<std::string> potential_paths;
  const std::vector<std::vector<std::string>> GurobiVersionLib = {
      {"911", "91"}, {"910", "91"}, {"903", "90"}, {"902", "90"}};

  const char* gurobi_home_from_env = getenv("GUROBI_HOME");
  for (const std::vector<std::string>& version_lib : GurobiVersionLib) {
#if !defined(__GNUC__) || defined(__APPLE__)
    const std::string& dir = version_lib[0];
#endif
    const std::string& number = version_lib[1];
#if defined(_MSC_VER)  // Windows
    if (gurobi_home_from_env != nullptr) {
      potential_paths.push_back(
          absl::StrCat(gurobi_home_from_env, "\\bin\\gurobi", number, ".dll"));
    }
    potential_paths.push_back(absl::StrCat("C:\\Program Files\\gurobi", dir,
                                           "\\win64\\bin\\gurobi", number,
                                           ".dll"));
#elif defined(__APPLE__)  // OS X
    if (gurobi_home_from_env != nullptr) {
      potential_paths.push_back(absl::StrCat(
          gurobi_home_from_env, "/lib/libgurobi", number, ".dylib"));
    }
    potential_paths.push_back(absl::StrCat(
        "/Library/gurobi", dir, "/mac64/lib/libgurobi", number, ".dylib"));
#elif defined(__GNUC__)   // Linux
    if (gurobi_home_from_env != nullptr) {
      potential_paths.push_back(
          absl::StrCat(gurobi_home_from_env, "/lib/libgurobi", number, ".so"));
    }
    if (gurobi_home_from_env != nullptr) {
      potential_paths.push_back(absl::StrCat(
          gurobi_home_from_env, "/lib64/libgurobi", number, ".so"));
    }
#else
    LOG(ERROR) << "OS Not recognized by gurobi/environment.cc."
               << " You won't be able to use Gurobi.";
#endif
  }
  return potential_paths;
}

namespace {
std::once_flag gurobi_loading_done;
std::atomic<bool> gurobi_successfully_loaded = false;
std::unique_ptr<DynamicLibrary> keep_gurobi_library_alive;
}  // namespace

bool LoadGurobiDynamicLibrary(
    const std::vector<std::string>& additional_paths) {
  std::call_once(gurobi_loading_done, [&additional_paths]() {
    keep_gurobi_library_alive = absl::make_unique<DynamicLibrary>();
    // Check additional paths first.
    for (const std::string &path : additional_paths) {
      if (keep_gurobi_library_alive->TryToLoad(path)) {
        LOG(INFO) << "Found the Gurobi library in '" << path << ".";
        break;
      }
    }
    // Fallback to canonical paths.
    const std::vector<std::string> &canonical_paths =
        GurobiDynamicLibraryPotentialPaths();
    if (!keep_gurobi_library_alive->LibraryIsLoaded()) {
      for (const std::string &path : canonical_paths) {
        if (keep_gurobi_library_alive->TryToLoad(path)) {
          LOG(INFO) << "Found the Gurobi library in '" << path << ".";
          break;
        }
      }
    }
    if (!keep_gurobi_library_alive->LibraryIsLoaded()) {
      LOG(INFO) << "Could not find the Gurobi shared library. Looked in: ["
                << absl::StrJoin(additional_paths, "', '") << "], and ["
                << absl::StrJoin(canonical_paths, "', '")
                << "]. If you know where it is, pass the full path to "
                   "'LoadGurobiDynamicLibrary()'.";
      return;
    }

    LoadGurobiFunctions(keep_gurobi_library_alive.get());
    gurobi_successfully_loaded = true;
  });
  return gurobi_successfully_loaded;
}

absl::StatusOr<GRBenv*> GetGurobiEnv() {
  LoadGurobiDynamicLibrary(std::vector<std::string>());

  if (!gurobi_successfully_loaded) {
    return absl::FailedPreconditionError(
        "The gurobi shared library was not successfully loaded.");
  }

  GRBenv* env = nullptr;
  const char kGurobiEnvErrorMsg[] =
      "Could not create a Gurobi environment. Is gurobi correctly installed and"
      " licensed on this machine ? ";

  if (GRBloadenv(&env, nullptr) != 0 || env == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s %s", kGurobiEnvErrorMsg, GRBgeterrormsg(env)));
  }
  return env;
}

}  // namespace operations_research
