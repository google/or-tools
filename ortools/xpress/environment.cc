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

#include "ortools/xpress/environment.h"

#include <filesystem>
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

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

// This was generated with the parse_header_xpress.py script.
// See the comment at the top of the script.

// This is the 'define' section.
std::function<int(XPRSprob dest, XPRSprob src)> XPRScopycallbacks = nullptr;
std::function<int(XPRSprob dest, XPRSprob src)> XPRScopycontrols = nullptr;
std::function<int(XPRSprob dest, XPRSprob src, const char* name)> XPRScopyprob =
    nullptr;
std::function<int(XPRSprob* p_prob)> XPRScreateprob = nullptr;
std::function<int(XPRSprob prob)> XPRSdestroyprob = nullptr;
std::function<int(const char* path)> XPRSinit = nullptr;
std::function<int(void)> XPRSfree = nullptr;
std::function<int(char* buffer, int maxbytes)> XPRSgetlicerrmsg = nullptr;
std::function<int(int* p_i, char* p_c)> XPRSlicense = nullptr;
std::function<int(int* p_notyet)> XPRSbeginlicensing = nullptr;
std::function<int(void)> XPRSendlicensing = nullptr;
std::function<int(int checkedmode)> XPRSsetcheckedmode = nullptr;
std::function<int(int* p_checkedmode)> XPRSgetcheckedmode = nullptr;
std::function<int(char* banner)> XPRSgetbanner = nullptr;
std::function<int(char* version)> XPRSgetversion = nullptr;
std::function<int(int* p_daysleft)> XPRSgetdaysleft = nullptr;
std::function<int(const char* feature, int* p_status)> XPRSfeaturequery =
    nullptr;
std::function<int(XPRSprob prob, const char* probname)> XPRSsetprobname =
    nullptr;
std::function<int(XPRSprob prob, const char* filename)> XPRSsetlogfile =
    nullptr;
std::function<int(XPRSprob prob, int control)> XPRSsetdefaultcontrol = nullptr;
std::function<int(XPRSprob prob)> XPRSsetdefaults = nullptr;
std::function<int(XPRSprob prob, int reason)> XPRSinterrupt = nullptr;
std::function<int(XPRSprob prob, char* name)> XPRSgetprobname = nullptr;
std::function<int(XPRSprob prob, int control, int value)> XPRSsetintcontrol =
    nullptr;
std::function<int(XPRSprob prob, int control, XPRSint64 value)>
    XPRSsetintcontrol64 = nullptr;
std::function<int(XPRSprob prob, int control, double value)> XPRSsetdblcontrol =
    nullptr;
std::function<int(XPRSprob prob, int control, const char* value)>
    XPRSsetstrcontrol = nullptr;
std::function<int(XPRSprob prob, int control, int* p_value)> XPRSgetintcontrol =
    nullptr;
std::function<int(XPRSprob prob, int control, XPRSint64* p_value)>
    XPRSgetintcontrol64 = nullptr;
std::function<int(XPRSprob prob, int control, double* p_value)>
    XPRSgetdblcontrol = nullptr;
std::function<int(XPRSprob prob, int control, char* value)> XPRSgetstrcontrol =
    nullptr;
std::function<int(XPRSprob prob, int control, char* value, int maxbytes,
                  int* p_nbytes)>
    XPRSgetstringcontrol = nullptr;
std::function<int(XPRSprob prob, int attrib, int* p_value)> XPRSgetintattrib =
    nullptr;
std::function<int(XPRSprob prob, int attrib, XPRSint64* p_value)>
    XPRSgetintattrib64 = nullptr;
std::function<int(XPRSprob prob, int attrib, char* value)> XPRSgetstrattrib =
    nullptr;
std::function<int(XPRSprob prob, int attrib, char* value, int maxbytes,
                  int* p_nbytes)>
    XPRSgetstringattrib = nullptr;
std::function<int(XPRSprob prob, int attrib, double* p_value)>
    XPRSgetdblattrib = nullptr;
std::function<int(XPRSprob prob, const char* name, int* p_id, int* p_type)>
    XPRSgetcontrolinfo = nullptr;
std::function<int(XPRSprob prob, const char* name, int* p_id, int* p_type)>
    XPRSgetattribinfo = nullptr;
std::function<int(XPRSprob prob, int objqcol1, int objqcol2,
                  double* p_objqcoef)>
    XPRSgetqobj = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSreadprob = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows,
                  const char rowtype[], const double rhs[], const double rng[],
                  const double objcoef[], const int start[], const int collen[],
                  const int rowind[], const double rowcoef[], const double lb[],
                  const double ub[])>
    XPRSloadlp = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows,
                  const char rowtype[], const double rhs[], const double rng[],
                  const double objcoef[], const XPRSint64 start[],
                  const int collen[], const int rowind[],
                  const double rowcoef[], const double lb[], const double ub[])>
    XPRSloadlp64 = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows,
                  const char rowtype[], const double rhs[], const double rng[],
                  const double objcoef[], const int start[], const int collen[],
                  const int rowind[], const double rowcoef[], const double lb[],
                  const double ub[], int nobjqcoefs, const int objqcol1[],
                  const int objqcol2[], const double objqcoef[])>
    XPRSloadqp = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows,
                  const char rowtype[], const double rhs[], const double rng[],
                  const double objcoef[], const XPRSint64 start[],
                  const int collen[], const int rowind[],
                  const double rowcoef[], const double lb[], const double ub[],
                  XPRSint64 nobjqcoefs, const int objqcol1[],
                  const int objqcol2[], const double objqcoef[])>
    XPRSloadqp64 = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows,
                  const char rowtype[], const double rhs[], const double rng[],
                  const double objcoef[], const int start[], const int collen[],
                  const int rowind[], const double rowcoef[], const double lb[],
                  const double ub[], int nobjqcoefs, const int objqcol1[],
                  const int objqcol2[], const double objqcoef[],
                  const int nentities, const int nsets, const char coltype[],
                  const int entind[], const double limit[],
                  const char settype[], const int setstart[],
                  const int setind[], const double refval[])>
    XPRSloadqglobal = nullptr;
std::function<int(
    XPRSprob prob, const char* probname, int ncols, int nrows,
    const char rowtype[], const double rhs[], const double rng[],
    const double objcoef[], const XPRSint64 start[], const int collen[],
    const int rowind[], const double rowcoef[], const double lb[],
    const double ub[], XPRSint64 nobjqcoefs, const int objqcol1[],
    const int objqcol2[], const double objqcoef[], const int nentities,
    const int nsets, const char coltype[], const int entind[],
    const double limit[], const char settype[], const XPRSint64 setstart[],
    const int setind[], const double refval[])>
    XPRSloadqglobal64 = nullptr;
std::function<int(XPRSprob prob, int options)> XPRSfixglobals = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[])>
    XPRSloadmodelcuts = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[])>
    XPRSloaddelayedrows = nullptr;
std::function<int(XPRSprob prob, int ndirs, const int colind[],
                  const int priority[], const char dir[],
                  const double uppseudo[], const double downpseudo[])>
    XPRSloaddirs = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[],
                  const int dir[])>
    XPRSloadbranchdirs = nullptr;
std::function<int(XPRSprob prob, int ndirs, const int colind[],
                  const int priority[], const char dir[],
                  const double uppseudo[], const double downpseudo[])>
    XPRSloadpresolvedirs = nullptr;
std::function<int(
    XPRSprob prob, const char* probname, int ncols, int nrows,
    const char rowtype[], const double rhs[], const double rng[],
    const double objcoef[], const int start[], const int collen[],
    const int rowind[], const double rowcoef[], const double lb[],
    const double ub[], int nentities, int nsets, const char coltype[],
    const int entind[], const double limit[], const char settype[],
    const int setstart[], const int setind[], const double refval[])>
    XPRSloadglobal = nullptr;
std::function<int(
    XPRSprob prob, const char* probname, int ncols, int nrows,
    const char rowtype[], const double rhs[], const double rng[],
    const double objcoef[], const XPRSint64 start[], const int collen[],
    const int rowind[], const double rowcoef[], const double lb[],
    const double ub[], int nentities, int nsets, const char coltype[],
    const int entind[], const double limit[], const char settype[],
    const XPRSint64 setstart[], const int setind[], const double refval[])>
    XPRSloadglobal64 = nullptr;
std::function<int(XPRSprob prob, int type, const char names[], int first,
                  int last)>
    XPRSaddnames = nullptr;
std::function<int(XPRSprob prob, const char names[], int first, int last)>
    XPRSaddsetnames = nullptr;
std::function<int(XPRSprob prob, const int rowscale[], const int colscale[])>
    XPRSscale = nullptr;
std::function<int(XPRSprob prob, const char* filename)> XPRSreaddirs = nullptr;
std::function<int(XPRSprob prob, const char* filename)> XPRSwritedirs = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[],
                  const int colind[], const int complement[])>
    XPRSsetindicators = nullptr;
std::function<int(XPRSprob prob, int npwls, int npoints, const int colind[],
                  const int resultant[], const int start[], const double xval[],
                  const double yval[])>
    XPRSaddpwlcons = nullptr;
std::function<int(XPRSprob prob, int npwls, XPRSint64 npoints,
                  const int colind[], const int resultant[],
                  const XPRSint64 start[], const double xval[],
                  const double yval[])>
    XPRSaddpwlcons64 = nullptr;
std::function<int(XPRSprob prob, int colind[], int resultant[], int start[],
                  double xval[], double yval[], int maxpoints, int* p_npoints,
                  int first, int last)>
    XPRSgetpwlcons = nullptr;
std::function<int(XPRSprob prob, int colind[], int resultant[],
                  XPRSint64 start[], double xval[], double yval[],
                  XPRSint64 maxpoints, XPRSint64* p_npoints, int first,
                  int last)>
    XPRSgetpwlcons64 = nullptr;
std::function<int(XPRSprob prob, int ncons, int ncols, int nvals,
                  const int contype[], const int resultant[],
                  const int colstart[], const int colind[],
                  const int valstart[], const double val[])>
    XPRSaddgencons = nullptr;
std::function<int(XPRSprob prob, int ncons, XPRSint64 ncols, XPRSint64 nvals,
                  const int contype[], const int resultant[],
                  const XPRSint64 colstart[], const int colind[],
                  const XPRSint64 valstart[], const double val[])>
    XPRSaddgencons64 = nullptr;
std::function<int(XPRSprob prob, int contype[], int resultant[], int colstart[],
                  int colind[], int maxcols, int* p_ncols, int valstart[],
                  double val[], int maxvals, int* p_nvals, int first, int last)>
    XPRSgetgencons = nullptr;
std::function<int(XPRSprob prob, int contype[], int resultant[],
                  XPRSint64 colstart[], int colind[], XPRSint64 maxcols,
                  XPRSint64* p_ncols, XPRSint64 valstart[], double val[],
                  XPRSint64 maxvals, XPRSint64* p_nvals, int first, int last)>
    XPRSgetgencons64 = nullptr;
std::function<int(XPRSprob prob, int npwls, const int pwlind[])>
    XPRSdelpwlcons = nullptr;
std::function<int(XPRSprob prob, int ncons, const int conind[])>
    XPRSdelgencons = nullptr;
std::function<int(XPRSprob prob)> XPRSdumpcontrols = nullptr;
std::function<int(XPRSprob prob, int colind[], int complement[], int first,
                  int last)>
    XPRSgetindicators = nullptr;
std::function<int(XPRSprob prob, int first, int last)> XPRSdelindicators =
    nullptr;
std::function<int(XPRSprob prob, int* p_ndir, int indices[], int prios[],
                  char branchdirs[], double uppseudo[], double downpseudo[])>
    XPRSgetdirs = nullptr;
std::function<int(XPRSprob prob, const char* flags)> XPRSlpoptimize = nullptr;
std::function<int(XPRSprob prob, const char* flags)> XPRSmipoptimize = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSreadslxsol = nullptr;
std::function<int(XPRSprob prob, const char* filename)> XPRSalter = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSreadbasis = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSreadbinsol = nullptr;
std::function<int(XPRSprob prob, int* p_nprimalcols, int* p_nprimalrows,
                  int* p_ndualrows, int* p_ndualcols, int x[], int slack[],
                  int duals[], int djs[])>
    XPRSgetinfeas = nullptr;
std::function<int(XPRSprob prob, int* p_nprimalcols, int* p_nprimalrows,
                  int* p_ndualrows, int* p_ndualcols, int x[], int slack[],
                  int duals[], int djs[])>
    XPRSgetscaledinfeas = nullptr;
std::function<int(XPRSprob prob, int* p_seq)> XPRSgetunbvec = nullptr;
std::function<int(XPRSprob prob, int* p_status)> XPRScrossoverlpsol = nullptr;
std::function<int(XPRSprob prob, const char* flags)> XPRStune = nullptr;
std::function<int(XPRSprob prob, const char* methodfile)> XPRStunerwritemethod =
    nullptr;
std::function<int(XPRSprob prob, const char* methodfile)> XPRStunerreadmethod =
    nullptr;
std::function<int(XPRSprob prob, int colstab[], int rowstab[])>
    XPRSgetbarnumstability = nullptr;
std::function<int(XPRSprob prob, int pivotorder[])> XPRSgetpivotorder = nullptr;
std::function<int(XPRSprob prob, int rowmap[], int colmap[])>
    XPRSgetpresolvemap = nullptr;
std::function<int(XPRSprob prob, double vec[])> XPRSbtran = nullptr;
std::function<int(XPRSprob prob, double vec[])> XPRSftran = nullptr;
std::function<int(XPRSprob prob, double val[], int ind[], int* p_ncoefs)>
    XPRSsparsebtran = nullptr;
std::function<int(XPRSprob prob, double val[], int ind[], int* p_ncoefs)>
    XPRSsparseftran = nullptr;
std::function<int(XPRSprob prob, double objcoef[], int first, int last)>
    XPRSgetobj = nullptr;
std::function<int(XPRSprob prob, double rhs[], int first, int last)>
    XPRSgetrhs = nullptr;
std::function<int(XPRSprob prob, double rng[], int first, int last)>
    XPRSgetrhsrange = nullptr;
std::function<int(XPRSprob prob, double lb[], int first, int last)> XPRSgetlb =
    nullptr;
std::function<int(XPRSprob prob, double ub[], int first, int last)> XPRSgetub =
    nullptr;
std::function<int(XPRSprob prob, int start[], int rowind[], double rowcoef[],
                  int maxcoefs, int* p_ncoefs, int first, int last)>
    XPRSgetcols = nullptr;
std::function<int(XPRSprob prob, XPRSint64 start[], int rowind[],
                  double rowcoef[], XPRSint64 maxcoefs, XPRSint64* p_ncoefs,
                  int first, int last)>
    XPRSgetcols64 = nullptr;
std::function<int(XPRSprob prob, int start[], int colind[], double colcoef[],
                  int maxcoefs, int* p_ncoefs, int first, int last)>
    XPRSgetrows = nullptr;
std::function<int(XPRSprob prob, XPRSint64 start[], int colind[],
                  double colcoef[], XPRSint64 maxcoefs, XPRSint64* p_ncoefs,
                  int first, int last)>
    XPRSgetrows64 = nullptr;
std::function<int(XPRSprob prob, int flags[], int first, int last)>
    XPRSgetrowflags = nullptr;
std::function<int(XPRSprob prob, const int flags[], int first, int last)>
    XPRSclearrowflags = nullptr;
std::function<int(XPRSprob prob, int row, int col, double* p_coef)>
    XPRSgetcoef = nullptr;
std::function<int(XPRSprob prob, int start[], int colind[], double objqcoef[],
                  int maxcoefs, int* p_ncoefs, int first, int last)>
    XPRSgetmqobj = nullptr;
std::function<int(XPRSprob prob, XPRSint64 start[], int colind[],
                  double objqcoef[], XPRSint64 maxcoefs, XPRSint64* p_ncoefs,
                  int first, int last)>
    XPRSgetmqobj64 = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSwritebasis = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSwritesol = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSwritebinsol = nullptr;
std::function<int(XPRSprob prob, double x[], double slack[], double duals[],
                  double djs[])>
    XPRSgetsol = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSwriteprtsol = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSwriteslxsol = nullptr;
std::function<int(XPRSprob prob, double x[], double slack[], double duals[],
                  double djs[])>
    XPRSgetpresolvesol = nullptr;
std::function<int(XPRSprob prob, double x[], double slack[], double duals[],
                  double djs[], int* p_status)>
    XPRSgetlastbarsol = nullptr;
std::function<int(XPRSprob prob)> XPRSiisclear = nullptr;
std::function<int(XPRSprob prob, int mode, int* p_status)> XPRSiisfirst =
    nullptr;
std::function<int(XPRSprob prob, int* p_status)> XPRSiisnext = nullptr;
std::function<int(XPRSprob prob, int* p_niis, int nrows[], int ncols[],
                  double suminfeas[], int numinfeas[])>
    XPRSiisstatus = nullptr;
std::function<int(XPRSprob prob)> XPRSiisall = nullptr;
std::function<int(XPRSprob prob, int iis, const char* filename, int filetype,
                  const char* flags)>
    XPRSiiswrite = nullptr;
std::function<int(XPRSprob prob, int iis)> XPRSiisisolations = nullptr;
std::function<int(XPRSprob prob, int iis, int* p_nrows, int* p_ncols,
                  int rowind[], int colind[], char contype[], char bndtype[],
                  double duals[], double djs[], char isolationrows[],
                  char isolationcols[])>
    XPRSgetiisdata = nullptr;
std::function<int(XPRSprob prob, int* p_ncols, int* p_nrows, int colind[],
                  int rowind[])>
    XPRSgetiis = nullptr;
std::function<int(XPRSprob prob, const int rowstat[], const int colstat[])>
    XPRSloadpresolvebasis = nullptr;
std::function<int(XPRSprob prob, int* p_nentities, int* p_nsets, char coltype[],
                  int colind[], double limit[], char settype[], int start[],
                  int setcols[], double refval[])>
    XPRSgetglobal = nullptr;
std::function<int(XPRSprob prob, int* p_nentities, int* p_nsets, char coltype[],
                  int colind[], double limit[], char settype[],
                  XPRSint64 start[], int setcols[], double refval[])>
    XPRSgetglobal64 = nullptr;
std::function<int(XPRSprob prob, int nrows, int ncols, const int rowind[],
                  const int colind[])>
    XPRSloadsecurevecs = nullptr;
std::function<int(XPRSprob prob, int nrows, int ncoefs, const char rowtype[],
                  const double rhs[], const double rng[], const int start[],
                  const int colind[], const double rowcoef[])>
    XPRSaddrows = nullptr;
std::function<int(XPRSprob prob, int nrows, XPRSint64 ncoefs,
                  const char rowtype[], const double rhs[], const double rng[],
                  const XPRSint64 start[], const int colind[],
                  const double rowcoef[])>
    XPRSaddrows64 = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[])> XPRSdelrows =
    nullptr;
std::function<int(XPRSprob prob, int ncols, int ncoefs, const double objcoef[],
                  const int start[], const int rowind[], const double rowcoef[],
                  const double lb[], const double ub[])>
    XPRSaddcols = nullptr;
std::function<int(XPRSprob prob, int ncols, XPRSint64 ncoefs,
                  const double objcoef[], const XPRSint64 start[],
                  const int rowind[], const double rowcoef[], const double lb[],
                  const double ub[])>
    XPRSaddcols64 = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[])> XPRSdelcols =
    nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[],
                  const char coltype[])>
    XPRSchgcoltype = nullptr;
std::function<int(XPRSprob prob, const int rowstat[], const int colstat[])>
    XPRSloadbasis = nullptr;
std::function<int(XPRSprob prob)> XPRSpostsolve = nullptr;
std::function<int(XPRSprob prob, int nsets, const int setind[])> XPRSdelsets =
    nullptr;
std::function<int(XPRSprob prob, int nsets, int nelems, const char settype[],
                  const int start[], const int colind[], const double refval[])>
    XPRSaddsets = nullptr;
std::function<int(XPRSprob prob, int nsets, XPRSint64 nelems,
                  const char settype[], const XPRSint64 start[],
                  const int colind[], const double refval[])>
    XPRSaddsets64 = nullptr;
std::function<int(XPRSprob prob, const int nbounds, const int colind[],
                  const char bndtype[], const double bndval[],
                  const int iterlim, double objval[], int status[])>
    XPRSstrongbranch = nullptr;
std::function<int(XPRSprob prob, const int nrows, const int rowind[],
                  const int iterlim, double mindual[], double maxdual[])>
    XPRSestimaterowdualranges = nullptr;
std::function<int(XPRSprob prob, int msgcode, int status)>
    XPRSsetmessagestatus = nullptr;
std::function<int(XPRSprob prob, int msgcode, int* p_status)>
    XPRSgetmessagestatus = nullptr;
std::function<int(XPRSprob prob, int objsense)> XPRSchgobjsense = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[],
                  const double limit[])>
    XPRSchgglblimit = nullptr;
std::function<int(XPRSprob prob, const char* probname, const char* flags)>
    XPRSrestore = nullptr;
std::function<int(XPRSprob prob, int enter, int leave)> XPRSpivot = nullptr;
std::function<int(XPRSprob prob, const double x[], const double slack[],
                  const double duals[], const double djs[], int* p_status)>
    XPRSloadlpsol = nullptr;
std::function<int(XPRSobject xprsobj, void* cbdata, void* thread,
                  const char* msg, int msgtype, int msgcode)>
    XPRSlogfilehandler = nullptr;
std::function<int(XPRSprob prob, int* p_status, const double lepref[],
                  const double gepref[], const double lbpref[],
                  const double ubpref[], char phase2, double delta,
                  const char* flags)>
    XPRSrepairweightedinfeas = nullptr;
std::function<int(
    XPRSprob prob, int* p_status, const double lepref[], const double gepref[],
    const double lbpref[], const double ubpref[], const double lerelax[],
    const double gerelax[], const double lbrelax[], const double ubrelax[],
    char phase2, double delta, const char* flags)>
    XPRSrepairweightedinfeasbounds = nullptr;
std::function<int(XPRSprob prob, int* p_status, char penalty, char phase2,
                  char flags, double lepref, double gepref, double lbpref,
                  double ubpref, double delta)>
    XPRSrepairinfeas = nullptr;
std::function<int(XPRSprob prob, int type, int norm, int scaled,
                  double* p_value)>
    XPRSbasisstability = nullptr;
std::function<int(XPRSprob prob, int type, const char* name, int* p_index)>
    XPRSgetindex = nullptr;
std::function<int(XPRSprob prob, char* errmsg)> XPRSgetlasterror = nullptr;
std::function<int(XPRSobject xprsobj, const char** p_name)>
    XPRSgetobjecttypename = nullptr;
std::function<int(XPRSprob prob, double ray[], int* p_hasray)>
    XPRSgetprimalray = nullptr;
std::function<int(XPRSprob prob, double ray[], int* p_hasray)> XPRSgetdualray =
    nullptr;
std::function<int(
    XPRSprob prob, const int nbounds, const int colind[], const char bndtype[],
    const double bndval[], const int iterlim, double objval[], int status[],
    int(XPRS_CC* callback)(XPRSprob prob, void* vContext, int ibnd),
    void* data)>
    XPRSstrongbranchcb = nullptr;
std::function<int(XPRSprob prob, const double x[], int* p_status)>
    XPRSloadmipsol = nullptr;
std::function<int(XPRSprob prob, int rowstat[], int colstat[])> XPRSgetbasis =
    nullptr;
std::function<int(XPRSprob prob, int row, int col, int* p_rowstat,
                  int* p_colstat)>
    XPRSgetbasisval = nullptr;
std::function<int(XPRSprob prob, int ncuts, const int cuttype[],
                  const char rowtype[], const double rhs[], const int start[],
                  const int colind[], const double cutcoef[])>
    XPRSaddcuts = nullptr;
std::function<int(XPRSprob prob, int ncuts, const int cuttype[],
                  const char rowtype[], const double rhs[],
                  const XPRSint64 start[], const int colind[],
                  const double cutcoef[])>
    XPRSaddcuts64 = nullptr;
std::function<int(XPRSprob prob, int basis, int cuttype, int interp,
                  double delta, int ncuts, const XPRScut cutind[])>
    XPRSdelcuts = nullptr;
std::function<int(XPRSprob prob, int cuttype, int interp, int ncuts,
                  const XPRScut cutind[])>
    XPRSdelcpcuts = nullptr;
std::function<int(XPRSprob prob, int cuttype, int interp, int* p_ncuts,
                  int maxcuts, XPRScut cutind[])>
    XPRSgetcutlist = nullptr;
std::function<int(XPRSprob prob, int cuttype, int interp, double delta,
                  int* p_ncuts, int maxcuts, XPRScut cutind[], double viol[])>
    XPRSgetcpcutlist = nullptr;
std::function<int(XPRSprob prob, const XPRScut rowind[], int ncuts,
                  int maxcoefs, int cuttype[], char rowtype[], int start[],
                  int colind[], double cutcoef[], double rhs[])>
    XPRSgetcpcuts = nullptr;
std::function<int(XPRSprob prob, const XPRScut rowind[], int ncuts,
                  XPRSint64 maxcoefs, int cuttype[], char rowtype[],
                  XPRSint64 start[], int colind[], double cutcoef[],
                  double rhs[])>
    XPRSgetcpcuts64 = nullptr;
std::function<int(XPRSprob prob, int coltype, int interp, int ncuts,
                  const XPRScut cutind[])>
    XPRSloadcuts = nullptr;
std::function<int(XPRSprob prob, int ncuts, int nodups, const int cuttype[],
                  const char rowtype[], const double rhs[], const int start[],
                  XPRScut cutind[], const int colind[], const double cutcoef[])>
    XPRSstorecuts = nullptr;
std::function<int(XPRSprob prob, int ncuts, int nodups, const int cuttype[],
                  const char rowtype[], const double rhs[],
                  const XPRSint64 start[], XPRScut cutind[], const int colind[],
                  const double cutcoef[])>
    XPRSstorecuts64 = nullptr;
std::function<int(XPRSprob prob, char rowtype, int norigcoefs,
                  const int origcolind[], const double origrowcoef[],
                  double origrhs, int maxcoefs, int* p_ncoefs, int colind[],
                  double rowcoef[], double* p_rhs, int* p_status)>
    XPRSpresolverow = nullptr;
std::function<int(XPRSprob prob, int nbounds, const int colind[],
                  const char bndtype[], const double bndval[], void** p_bounds)>
    XPRSstorebounds = nullptr;
std::function<int(XPRSprob prob, int ncuts, const XPRScut cutind[])>
    XPRSsetbranchcuts = nullptr;
std::function<int(XPRSprob prob, void* bounds)> XPRSsetbranchbounds = nullptr;
std::function<int(XPRSprob prob, int enter, int outlist[], double x[],
                  double* p_objval, int* p_npivots, int maxpivots)>
    XPRSgetpivots = nullptr;
std::function<int(XPRSprob prob, const char* filename, const char* flags)>
    XPRSwriteprob = nullptr;
std::function<int(XPRSprob prob, const double solution[], double slacks[])>
    XPRScalcslacks = nullptr;
std::function<int(XPRSprob prob, const double duals[], const double solution[],
                  double djs[])>
    XPRScalcreducedcosts = nullptr;
std::function<int(XPRSprob prob, const double solution[], double* p_objval)>
    XPRScalcobjective = nullptr;
std::function<int(XPRSprob prob, const double solution[], const double duals[],
                  int property, double* p_value)>
    XPRScalcsolinfo = nullptr;
std::function<int(XPRSprob prob, char rowtype[], int first, int last)>
    XPRSgetrowtype = nullptr;
std::function<int(XPRSprob prob, int rowstat[], int colstat[])>
    XPRSgetpresolvebasis = nullptr;
std::function<int(XPRSprob prob, char coltype[], int first, int last)>
    XPRSgetcoltype = nullptr;
std::function<int(XPRSprob prob)> XPRSsave = nullptr;
std::function<int(XPRSprob prob, const char* filename)> XPRSsaveas = nullptr;
std::function<int(XPRSprob prob, int type, char names[], int maxbytes,
                  int* p_nbytes, int first, int last)>
    XPRSgetnamelist = nullptr;
std::function<int(XPRSprob prob, int type, XPRSnamelist* p_nml)>
    XPRSgetnamelistobject = nullptr;
std::function<int(XPRSprob prob, int length, const double solval[],
                  const int colind[], const char* name)>
    XPRSaddmipsol = nullptr;
std::function<int(XPRSprob prob, XPRScut cutind, double* p_slack)>
    XPRSgetcutslack = nullptr;
std::function<int(XPRSprob prob, int ncuts, const XPRScut cutind[],
                  int cutmap[])>
    XPRSgetcutmap = nullptr;
std::function<int(XPRSprob prob, int type, char names[], int first, int last)>
    XPRSgetnames = nullptr;
std::function<int(XPRSprob prob, double x[], double slack[], double duals[],
                  double djs[])>
    XPRSgetlpsol = nullptr;
std::function<int(XPRSprob prob, int col, int row, double* p_x, double* p_slack,
                  double* p_dual, double* p_dj)>
    XPRSgetlpsolval = nullptr;
std::function<int(XPRSprob prob, double x[], double slack[])> XPRSgetmipsol =
    nullptr;
std::function<int(XPRSprob prob, int col, int row, double* p_x,
                  double* p_slack)>
    XPRSgetmipsolval = nullptr;
std::function<int(XPRSprob prob, int nbounds, const int colind[],
                  const char bndtype[], const double bndval[])>
    XPRSchgbounds = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[],
                  const double objcoef[])>
    XPRSchgobj = nullptr;
std::function<int(XPRSprob prob, int row, int col, double coef)> XPRSchgcoef =
    nullptr;
std::function<int(XPRSprob prob, int ncoefs, const int rowind[],
                  const int colind[], const double rowcoef[])>
    XPRSchgmcoef = nullptr;
std::function<int(XPRSprob prob, XPRSint64 ncoefs, const int rowind[],
                  const int colind[], const double rowcoef[])>
    XPRSchgmcoef64 = nullptr;
std::function<int(XPRSprob prob, int ncoefs, const int objqcol1[],
                  const int objqcol2[], const double objqcoef[])>
    XPRSchgmqobj = nullptr;
std::function<int(XPRSprob prob, XPRSint64 ncoefs, const int objqcol1[],
                  const int objqcol2[], const double objqcoef[])>
    XPRSchgmqobj64 = nullptr;
std::function<int(XPRSprob prob, int objqcol1, int objqcol2, double objqcoef)>
    XPRSchgqobj = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[],
                  const double rhs[])>
    XPRSchgrhs = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[],
                  const double rng[])>
    XPRSchgrhsrange = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[],
                  const char rowtype[])>
    XPRSchgrowtype = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_lplog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcblplog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC** f_lplog)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcblplog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_lplog)(XPRSprob cbprob, void* cbdata), void* p,
                  int priority)>
    XPRSaddcblplog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_lplog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecblplog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_globallog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbgloballog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC** f_globallog)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbgloballog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_globallog)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbgloballog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_globallog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbgloballog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_cutlog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbcutlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC** f_cutlog)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbcutlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_cutlog)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbcutlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_cutlog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbcutlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_barlog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbbarlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC** f_barlog)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbbarlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_barlog)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbbarlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_barlog)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbbarlog = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_cutmgr)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbcutmgr = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC** f_cutmgr)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbcutmgr = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_cutmgr)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbcutmgr = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_cutmgr)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbcutmgr = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node),
    void* p)>
    XPRSsetcbchgnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node),
    void** p)>
    XPRSgetcbchgnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node),
    void* p, int priority)>
    XPRSaddcbchgnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node),
    void* p)>
    XPRSremovecbchgnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void* p)>
    XPRSsetcboptnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void** p)>
    XPRSgetcboptnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void* p, int priority)>
    XPRSaddcboptnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void* p)>
    XPRSremovecboptnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void* p)>
    XPRSsetcbprenode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void** p)>
    XPRSgetcbprenode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void* p, int priority)>
    XPRSaddcbprenode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible),
    void* p)>
    XPRSremovecbprenode = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_infnode)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbinfnode = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_infnode)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbinfnode = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_infnode)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbinfnode = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_infnode)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbinfnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node),
    void* p)>
    XPRSsetcbnodecutoff = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node),
    void** p)>
    XPRSgetcbnodecutoff = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node),
    void* p, int priority)>
    XPRSaddcbnodecutoff = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node),
    void* p)>
    XPRSremovecbnodecutoff = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_intsol)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbintsol = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_intsol)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbintsol = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_intsol)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbintsol = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_intsol)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbintsol = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype,
                               int* p_reject, double* p_cutoff),
    void* p)>
    XPRSsetcbpreintsol = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype,
                                int* p_reject, double* p_cutoff),
    void** p)>
    XPRSgetcbpreintsol = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype,
                               int* p_reject, double* p_cutoff),
    void* p, int priority)>
    XPRSaddcbpreintsol = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype,
                               int* p_reject, double* p_cutoff),
    void* p)>
    XPRSremovecbpreintsol = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity,
                               int* p_up, double* p_estdeg),
    void* p)>
    XPRSsetcbchgbranch = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity,
                                int* p_up, double* p_estdeg),
    void** p)>
    XPRSgetcbchgbranch = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity,
                               int* p_up, double* p_estdeg),
    void* p, int priority)>
    XPRSaddcbchgbranch = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity,
                               int* p_up, double* p_estdeg),
    void* p)>
    XPRSremovecbchgbranch = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC* f_estimate)(XPRSprob cbprob, void* cbdata, int* p_entity,
                             int* p_prio, double* p_degbest, double* p_degworst,
                             double* p_current, int* p_preferred, int* p_ninf,
                             double* p_degsum, int* p_nbranches),
    void* p)>
    XPRSsetcbestimate = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC** f_estimate)(
        XPRSprob cbprob, void* cbdata, int* p_entity, int* p_prio,
        double* p_degbest, double* p_degworst, double* p_current,
        int* p_preferred, int* p_ninf, double* p_degsum, int* p_nbranches),
    void** p)>
    XPRSgetcbestimate = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC* f_estimate)(XPRSprob cbprob, void* cbdata, int* p_entity,
                             int* p_prio, double* p_degbest, double* p_degworst,
                             double* p_current, int* p_preferred, int* p_ninf,
                             double* p_degsum, int* p_nbranches),
    void* p, int priority)>
    XPRSaddcbestimate = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC* f_estimate)(XPRSprob cbprob, void* cbdata, int* p_entity,
                             int* p_prio, double* p_degbest, double* p_degworst,
                             double* p_current, int* p_preferred, int* p_ninf,
                             double* p_degsum, int* p_nbranches),
    void* p)>
    XPRSremovecbestimate = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC* f_sepnode)(XPRSprob cbprob, void* cbdata, int branch,
                            int entity, int up, double current),
    void* p)>
    XPRSsetcbsepnode = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC** f_sepnode)(XPRSprob cbprob, void* cbdata, int branch,
                             int entity, int up, double current),
    void** p)>
    XPRSgetcbsepnode = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC* f_sepnode)(XPRSprob cbprob, void* cbdata, int branch,
                            int entity, int up, double current),
    void* p, int priority)>
    XPRSaddcbsepnode = nullptr;
std::function<int(
    XPRSprob prob,
    int(XPRS_CC* f_sepnode)(XPRSprob cbprob, void* cbdata, int branch,
                            int entity, int up, double current),
    void* p)>
    XPRSremovecbsepnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_message)(XPRSprob cbprob, void* cbdata, const char* msg,
                             int msglen, int msgtype),
    void* p)>
    XPRSsetcbmessage = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_message)(XPRSprob cbprob, void* cbdata, const char* msg,
                              int msglen, int msgtype),
    void** p)>
    XPRSgetcbmessage = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_message)(XPRSprob cbprob, void* cbdata, const char* msg,
                             int msglen, int msgtype),
    void* p, int priority)>
    XPRSaddcbmessage = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_message)(XPRSprob cbprob, void* cbdata, const char* msg,
                             int msglen, int msgtype),
    void* p)>
    XPRSremovecbmessage = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_mipthread)(XPRSprob cbprob, void* cbdata,
                                             XPRSprob threadprob),
                  void* p)>
    XPRSsetcbmipthread = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_mipthread)(XPRSprob cbprob, void* cbdata,
                                              XPRSprob threadprob),
                  void** p)>
    XPRSgetcbmipthread = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_mipthread)(XPRSprob cbprob, void* cbdata,
                                             XPRSprob threadprob),
                  void* p, int priority)>
    XPRSaddcbmipthread = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_mipthread)(XPRSprob cbprob, void* cbdata,
                                             XPRSprob threadprob),
                  void* p)>
    XPRSremovecbmipthread = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_destroymt)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbdestroymt = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_destroymt)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbdestroymt = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_destroymt)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbdestroymt = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_destroymt)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbdestroymt = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode,
                             int newnode, int branch),
    void* p)>
    XPRSsetcbnewnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode,
                              int newnode, int branch),
    void** p)>
    XPRSgetcbnewnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode,
                             int newnode, int branch),
    void* p, int priority)>
    XPRSaddcbnewnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode,
                             int newnode, int branch),
    void* p)>
    XPRSremovecbnewnode = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_bariteration)(XPRSprob cbprob, void* cbdata, int* p_action),
    void* p)>
    XPRSsetcbbariteration = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_bariteration)(XPRSprob cbprob, void* cbdata,
                                                 int* p_action),
                  void** p)>
    XPRSgetcbbariteration = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_bariteration)(XPRSprob cbprob, void* cbdata, int* p_action),
    void* p, int priority)>
    XPRSaddcbbariteration = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_bariteration)(XPRSprob cbprob, void* cbdata, int* p_action),
    void* p)>
    XPRSremovecbbariteration = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_presolve)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbpresolve = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_presolve)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbpresolve = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_presolve)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbpresolve = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_presolve)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbpresolve = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_chgbranchobject)(
                      XPRSprob cbprob, void* cbdata, XPRSbranchobject branch,
                      XPRSbranchobject* p_newbranch),
                  void* p)>
    XPRSsetcbchgbranchobject = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_chgbranchobject)(
                      XPRSprob cbprob, void* cbdata, XPRSbranchobject branch,
                      XPRSbranchobject* p_newbranch),
                  void** p)>
    XPRSgetcbchgbranchobject = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_chgbranchobject)(
                      XPRSprob cbprob, void* cbdata, XPRSbranchobject branch,
                      XPRSbranchobject* p_newbranch),
                  void* p, int priority)>
    XPRSaddcbchgbranchobject = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_chgbranchobject)(
                      XPRSprob cbprob, void* cbdata, XPRSbranchobject branch,
                      XPRSbranchobject* p_newbranch),
                  void* p)>
    XPRSremovecbchgbranchobject = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_computerestart)(XPRSprob cbprob, void* cbdata), void* p)>
    XPRSsetcbcomputerestart = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_computerestart)(XPRSprob cbprob, void* cbdata), void** p)>
    XPRSgetcbcomputerestart = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_computerestart)(XPRSprob cbprob, void* cbdata), void* p,
    int priority)>
    XPRSaddcbcomputerestart = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_computerestart)(XPRSprob cbprob, void* cbdata), void* p)>
    XPRSremovecbcomputerestart = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_gapnotify)(XPRSprob cbprob, void* cbdata,
                                             double* p_relgapnotifytarget,
                                             double* p_absgapnotifytarget,
                                             double* p_absgapnotifyobjtarget,
                                             double* p_absgapnotifyboundtarget),
                  void* p)>
    XPRSsetcbgapnotify = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_gapnotify)(
        XPRSprob cbprob, void* cbdata, double* p_relgapnotifytarget,
        double* p_absgapnotifytarget, double* p_absgapnotifyobjtarget,
        double* p_absgapnotifyboundtarget),
    void** p)>
    XPRSgetcbgapnotify = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_gapnotify)(XPRSprob cbprob, void* cbdata,
                                             double* p_relgapnotifytarget,
                                             double* p_absgapnotifytarget,
                                             double* p_absgapnotifyobjtarget,
                                             double* p_absgapnotifyboundtarget),
                  void* p, int priority)>
    XPRSaddcbgapnotify = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_gapnotify)(XPRSprob cbprob, void* cbdata,
                                             double* p_relgapnotifytarget,
                                             double* p_absgapnotifytarget,
                                             double* p_absgapnotifyobjtarget,
                                             double* p_absgapnotifyboundtarget),
                  void* p)>
    XPRSremovecbgapnotify = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_usersolnotify)(XPRSprob cbprob, void* cbdata,
                                   const char* solname, int status),
    void* p)>
    XPRSsetcbusersolnotify = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC** f_usersolnotify)(XPRSprob cbprob, void* cbdata,
                                    const char* solname, int status),
    void** p)>
    XPRSgetcbusersolnotify = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_usersolnotify)(XPRSprob cbprob, void* cbdata,
                                   const char* solname, int status),
    void* p, int priority)>
    XPRSaddcbusersolnotify = nullptr;
std::function<int(
    XPRSprob prob,
    void(XPRS_CC* f_usersolnotify)(XPRSprob cbprob, void* cbdata,
                                   const char* solname, int status),
    void* p)>
    XPRSremovecbusersolnotify = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_beforesolve)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbbeforesolve = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC** f_beforesolve)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbbeforesolve = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_beforesolve)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbbeforesolve = nullptr;
std::function<int(XPRSprob prob,
                  void(XPRS_CC* f_beforesolve)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbbeforesolve = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_checktime)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSsetcbchecktime = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC** f_checktime)(XPRSprob cbprob, void* cbdata),
                  void** p)>
    XPRSgetcbchecktime = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_checktime)(XPRSprob cbprob, void* cbdata),
                  void* p, int priority)>
    XPRSaddcbchecktime = nullptr;
std::function<int(XPRSprob prob,
                  int(XPRS_CC* f_checktime)(XPRSprob cbprob, void* cbdata),
                  void* p)>
    XPRSremovecbchecktime = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[], double lower[],
                  double upper[])>
    XPRSobjsa = nullptr;
std::function<int(XPRSprob prob, int ncols, const int colind[],
                  double lblower[], double lbupper[], double ublower[],
                  double ubupper[])>
    XPRSbndsa = nullptr;
std::function<int(XPRSprob prob, int nrows, const int rowind[], double lower[],
                  double upper[])>
    XPRSrhssa = nullptr;
std::function<int(int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p)>
    XPRS_ge_setcbmsghandler = nullptr;
std::function<int(int(XPRS_CC** f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void** p)>
    XPRS_ge_getcbmsghandler = nullptr;
std::function<int(int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p, int priority)>
    XPRS_ge_addcbmsghandler = nullptr;
std::function<int(int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p)>
    XPRS_ge_removecbmsghandler = nullptr;
std::function<int(int consistent)> XPRS_ge_setarchconsistency = nullptr;
std::function<int(int safemode)> XPRS_ge_setsafemode = nullptr;
std::function<int(int* p_safemode)> XPRS_ge_getsafemode = nullptr;
std::function<int(int debugmode)> XPRS_ge_setdebugmode = nullptr;
std::function<int(int* p_debugmode)> XPRS_ge_getdebugmode = nullptr;
std::function<int(int* p_msgcode, char* msg, int maxbytes, int* p_nbytes)>
    XPRS_ge_getlasterror = nullptr;
std::function<int(int allow)> XPRS_ge_setcomputeallowed = nullptr;
std::function<int(int* p_allow)> XPRS_ge_getcomputeallowed = nullptr;
std::function<int(XPRSmipsolpool* msp)> XPRS_msp_create = nullptr;
std::function<int(XPRSmipsolpool msp)> XPRS_msp_destroy = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob)> XPRS_msp_probattach =
    nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob)> XPRS_msp_probdetach =
    nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against,
                  int iRankAttrib, int bRankAscending, int iRankFirstIndex_Ob,
                  int iRankLastIndex_Ob, int iSolutionIds_Zb[],
                  int* nReturnedSolIds, int* nSols)>
    XPRS_msp_getsollist = nullptr;
std::function<int(
    XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int iRankAttrib,
    int bRankAscending, int iRankFirstIndex_Ob, int iRankLastIndex_Ob,
    int bUseUserBitFilter, int iUserBitMask, int iUserBitPattern,
    int bUseInternalBitFilter, int iInternalBitMask, int iInternalBitPattern,
    int iSolutionIds_Zb[], int* nReturnedSolIds, int* nSols)>
    XPRS_msp_getsollist2 = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_,
                  double x[], int iColFirst, int iColLast,
                  int* nValuesReturned)>
    XPRS_msp_getsol = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against,
                  int iSolutionId, int* iSolutionIdStatus_, double slack[],
                  int iRowFirst, int iRowLast, int* nValuesReturned)>
    XPRS_msp_getslack = nullptr;
std::function<int(XPRSmipsolpool msp, int* iSolutionId, const double x[],
                  int nCols, const char* sSolutionName,
                  int* bNameModifiedForUniqueness,
                  int* iSolutionIdOfExistingDuplicatePreventedLoad)>
    XPRS_msp_loadsol = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_)>
    XPRS_msp_delsol = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against,
                  int iSolutionId, int* iSolutionIdStatus_, int iAttribId,
                  int* Dst)>
    XPRS_msp_getintattribprobsol = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against,
                  int iSolutionId, int* iSolutionIdStatus_, int iAttribId,
                  double* Dst)>
    XPRS_msp_getdblattribprobsol = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob, int iAttribId, int* Dst)>
    XPRS_msp_getintattribprob = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob, int iAttribId,
                  double* Dst)>
    XPRS_msp_getdblattribprob = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_,
                  int iAttribId, int* Dst)>
    XPRS_msp_getintattribsol = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_,
                  int iAttribId, double* Dst)>
    XPRS_msp_getdblattribsol = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_,
                  int iControlId, int* Val)>
    XPRS_msp_getintcontrolsol = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_,
                  int iControlId, double* Val)>
    XPRS_msp_getdblcontrolsol = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_,
                  int iControlId, int Val)>
    XPRS_msp_setintcontrolsol = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_,
                  int iControlId, double Val)>
    XPRS_msp_setdblcontrolsol = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against,
                  int bGet_Max_Otherwise_Min, int* iSolutionId, int iAttribId,
                  int* ExtremeVal)>
    XPRS_msp_getintattribprobextreme = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against,
                  int bGet_Max_Otherwise_Min, int* iSolutionId, int iAttribId,
                  double* ExtremeVal)>
    XPRS_msp_getdblattribprobextreme = nullptr;
std::function<int(XPRSmipsolpool msp, int iAttribId, int* Val)>
    XPRS_msp_getintattrib = nullptr;
std::function<int(XPRSmipsolpool msp, int iAttribId, double* Val)>
    XPRS_msp_getdblattrib = nullptr;
std::function<int(XPRSmipsolpool msp, int iControlId, int* Val)>
    XPRS_msp_getintcontrol = nullptr;
std::function<int(XPRSmipsolpool msp, int iControlId, double* Val)>
    XPRS_msp_getdblcontrol = nullptr;
std::function<int(XPRSmipsolpool msp, int iControlId, int Val)>
    XPRS_msp_setintcontrol = nullptr;
std::function<int(XPRSmipsolpool msp, int iControlId, double Val)>
    XPRS_msp_setdblcontrol = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId,
                  const char* sNewSolutionBaseName,
                  int* bNameModifiedForUniqueness, int* iSolutionIdStatus_)>
    XPRS_msp_setsolname = nullptr;
std::function<int(XPRSmipsolpool msp, int iSolutionId, char* _sname,
                  int _iStringBufferBytes, int* _iBytesInInternalString,
                  int* iSolutionIdStatus_)>
    XPRS_msp_getsolname = nullptr;
std::function<int(XPRSmipsolpool msp, const char* sSolutionName,
                  int* iSolutionId)>
    XPRS_msp_findsolbyname = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSprob prob_context, int iSolutionId,
                  int* iSolutionIdStatus_, const char* sFileName,
                  const char* sFlags)>
    XPRS_msp_writeslxsol = nullptr;
std::function<int(XPRSmipsolpool msp, XPRSnamelist col_name_list,
                  const char* sFileName, const char* sFlags,
                  int* iSolutionId_Beg, int* iSolutionId_End)>
    XPRS_msp_readslxsol = nullptr;
std::function<int(XPRSmipsolpool msp, int* iMsgCode, char* _msg,
                  int _iStringBufferBytes, int* _iBytesInInternalString)>
    XPRS_msp_getlasterror = nullptr;
std::function<int(XPRSmipsolpool msp,
                  int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p)>
    XPRS_msp_setcbmsghandler = nullptr;
std::function<int(XPRSmipsolpool msp,
                  int(XPRS_CC** f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void** p)>
    XPRS_msp_getcbmsghandler = nullptr;
std::function<int(XPRSmipsolpool msp,
                  int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p, int priority)>
    XPRS_msp_addcbmsghandler = nullptr;
std::function<int(XPRSmipsolpool msp,
                  int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p)>
    XPRS_msp_removecbmsghandler = nullptr;
std::function<int(XPRSnamelist* p_nml)> XPRS_nml_create = nullptr;
std::function<int(XPRSnamelist nml)> XPRS_nml_destroy = nullptr;
std::function<int(XPRSnamelist nml, int* p_count)> XPRS_nml_getnamecount =
    nullptr;
std::function<int(XPRSnamelist nml, int* p_namelen)> XPRS_nml_getmaxnamelen =
    nullptr;
std::function<int(XPRSnamelist nml, int pad, char buffer[], int maxbytes,
                  int* p_nbytes, int first, int last)>
    XPRS_nml_getnames = nullptr;
std::function<int(XPRSnamelist nml, const char names[], int first, int last)>
    XPRS_nml_addnames = nullptr;
std::function<int(XPRSnamelist nml, int first, int last)> XPRS_nml_removenames =
    nullptr;
std::function<int(XPRSnamelist nml, const char* name, int* p_index)>
    XPRS_nml_findname = nullptr;
std::function<int(XPRSnamelist dest, XPRSnamelist src)> XPRS_nml_copynames =
    nullptr;
std::function<int(XPRSnamelist nml, int* p_msgcode, char* msg, int maxbytes,
                  int* p_nbytes)>
    XPRS_nml_getlasterror = nullptr;
std::function<int(XPRSprob prob, int row, int ncoefs, const int rowqcol1[],
                  const int rowqcol2[], const double rowqcoef[])>
    XPRSaddqmatrix = nullptr;
std::function<int(XPRSprob prob, int row, XPRSint64 ncoefs,
                  const int rowqcol1[], const int rowqcol2[],
                  const double rowqcoef[])>
    XPRSaddqmatrix64 = nullptr;
std::function<int(XPRSprob prob, int row)> XPRSdelqmatrix = nullptr;
std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows,
                  const char rowtype[], const double rhs[], const double rng[],
                  const double objcoef[], const int start[], const int collen[],
                  const int rowind[], const double rowcoef[], const double lb[],
                  const double ub[], int nobjqcoefs, const int objqcol1[],
                  const int objqcol2[], const double objqcoef[], int nqrows,
                  const int qrowind[], const int nrowqcoef[],
                  const int rowqcol1[], const int rowqcol2[],
                  const double rowqcoef[])>
    XPRSloadqcqp = nullptr;
std::function<int(
    XPRSprob prob, const char* probname, int ncols, int nrows,
    const char rowtype[], const double rhs[], const double rng[],
    const double objcoef[], const XPRSint64 start[], const int collen[],
    const int rowind[], const double rowcoef[], const double lb[],
    const double ub[], XPRSint64 nobjqcoefs, const int objqcol1[],
    const int objqcol2[], const double objqcoef[], int nqrows,
    const int qrowind[], const XPRSint64 nrowqcoef[], const int rowqcol1[],
    const int rowqcol2[], const double rowqcoef[])>
    XPRSloadqcqp64 = nullptr;
std::function<int(
    XPRSprob prob, const char* probname, int ncols, int nrows,
    const char rowtype[], const double rhs[], const double rng[],
    const double objcoef[], const int start[], const int collen[],
    const int rowind[], const double rowcoef[], const double lb[],
    const double ub[], int nobjqcoefs, const int objqcol1[],
    const int objqcol2[], const double objqcoef[], int nqrows,
    const int qrowind[], const int nrowqcoefs[], const int rowqcol1[],
    const int rowqcol2[], const double rowqcoef[], const int nentities,
    const int nsets, const char coltype[], const int entind[],
    const double limit[], const char settype[], const int setstart[],
    const int setind[], const double refval[])>
    XPRSloadqcqpglobal = nullptr;
std::function<int(
    XPRSprob prob, const char* probname, int ncols, int nrows,
    const char rowtype[], const double rhs[], const double rng[],
    const double objcoef[], const XPRSint64 start[], const int collen[],
    const int rowind[], const double rowcoef[], const double lb[],
    const double ub[], XPRSint64 nobjqcoefs, const int objqcol1[],
    const int objqcol2[], const double objqcoef[], int nqrows,
    const int qrowind[], const XPRSint64 nrowqcoefs[], const int rowqcol1[],
    const int rowqcol2[], const double rowqcoef[], const int nentities,
    const int nsets, const char coltype[], const int entind[],
    const double limit[], const char settype[], const XPRSint64 setstart[],
    const int setind[], const double refval[])>
    XPRSloadqcqpglobal64 = nullptr;
std::function<int(XPRSprob prob, int row, int rowqcol1, int rowqcol2,
                  double* p_rowqcoef)>
    XPRSgetqrowcoeff = nullptr;
std::function<int(XPRSprob prob, int row, int start[], int colind[],
                  double rowqcoef[], int maxcoefs, int* p_ncoefs, int first,
                  int last)>
    XPRSgetqrowqmatrix = nullptr;
std::function<int(XPRSprob prob, int row, int* p_ncoefs, int rowqcol1[],
                  int rowqcol2[], double rowqcoef[])>
    XPRSgetqrowqmatrixtriplets = nullptr;
std::function<int(XPRSprob prob, int row, int rowqcol1, int rowqcol2,
                  double rowqcoef)>
    XPRSchgqrowcoeff = nullptr;
std::function<int(XPRSprob prob, int* p_nrows, int rowind[])> XPRSgetqrows =
    nullptr;
std::function<int(XPRSmipsolenum* mse)> XPRS_mse_create = nullptr;
std::function<int(XPRSmipsolenum mse)> XPRS_mse_destroy = nullptr;
std::function<int(
    XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp,
    int(XPRS_CC* f_mse_handler)(
        XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, void* vContext,
        int* nMaxSols, const double x_Zb[], const int nCols,
        const double dMipObject, double* dModifiedObject, int* bRejectSoln,
        int* bUpdateMipAbsCutOffOnCurrentSet),
    void* p, int* nMaxSols)>
    XPRS_mse_minim = nullptr;
std::function<int(
    XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp,
    int(XPRS_CC* f_mse_handler)(
        XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, void* vContext,
        int* nMaxSols, const double x_Zb[], const int nCols,
        const double dMipObject, double* dModifiedObject, int* bRejectSoln,
        int* bUpdateMipAbsCutOffOnCurrentSet),
    void* p, int* nMaxSols)>
    XPRS_mse_maxim = nullptr;
std::function<int(
    XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp,
    int(XPRS_CC* f_mse_handler)(
        XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, void* vContext,
        int* nMaxSols, const double x_Zb[], const int nCols,
        const double dMipObject, double* dModifiedObject, int* bRejectSoln,
        int* bUpdateMipAbsCutOffOnCurrentSet),
    void* p, int* nMaxSols)>
    XPRS_mse_opt = nullptr;
std::function<int(XPRSmipsolenum mse, int iMetricId, int iRankFirstIndex_Ob,
                  int iRankLastIndex_Ob, int iSolutionIds[],
                  int* nReturnedSolIds, int* nSols)>
    XPRS_mse_getsollist = nullptr;
std::function<int(XPRSmipsolenum mse, int iSolutionId, int* iSolutionIdStatus,
                  int iMetricId, double* dMetric)>
    XPRS_mse_getsolmetric = nullptr;
std::function<int(XPRSmipsolenum mse, int iMetricId, int cull_sol_id_list[],
                  int nMaxSolsToCull, int* nSolsToCull, double dNewSolMetric,
                  const double x[], int nCols, int* bRejectSoln)>
    XPRS_mse_getcullchoice = nullptr;
std::function<int(XPRSmipsolenum mse, int iAttribId, int* Val)>
    XPRS_mse_getintattrib = nullptr;
std::function<int(XPRSmipsolenum mse, int iAttribId, double* Val)>
    XPRS_mse_getdblattrib = nullptr;
std::function<int(XPRSmipsolenum mse, int iAttribId, int* Val)>
    XPRS_mse_getintcontrol = nullptr;
std::function<int(XPRSmipsolenum mse, int iAttribId, double* Val)>
    XPRS_mse_getdblcontrol = nullptr;
std::function<int(XPRSmipsolenum mse, int iAttribId, int Val)>
    XPRS_mse_setintcontrol = nullptr;
std::function<int(XPRSmipsolenum mse, int iAttribId, double Val)>
    XPRS_mse_setdblcontrol = nullptr;
std::function<int(XPRSmipsolenum mse, int* iMsgCode, char* _msg,
                  int _iStringBufferBytes, int* _iBytesInInternalString)>
    XPRS_mse_getlasterror = nullptr;
std::function<int(XPRSmipsolenum mse, const char* sSolutionBaseName)>
    XPRS_mse_setsolbasename = nullptr;
std::function<int(XPRSmipsolenum mse, char* _sname, int _iStringBufferBytes,
                  int* _iBytesInInternalString)>
    XPRS_mse_getsolbasename = nullptr;
std::function<int(
    XPRSmipsolenum mse,
    int(XPRS_CC* f_mse_getsolutiondiff)(
        XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1,
        int iElemCount_1, double dMipObj_1, const double Vals_1[],
        const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2,
        double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[],
        double* dDiffMetric),
    void* p)>
    XPRS_mse_setcbgetsolutiondiff = nullptr;
std::function<int(
    XPRSmipsolenum mse,
    int(XPRS_CC** f_mse_getsolutiondiff)(
        XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1,
        int iElemCount_1, double dMipObj_1, const double Vals_1[],
        const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2,
        double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[],
        double* dDiffMetric),
    void** p)>
    XPRS_mse_getcbgetsolutiondiff = nullptr;
std::function<int(
    XPRSmipsolenum mse,
    int(XPRS_CC* f_mse_getsolutiondiff)(
        XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1,
        int iElemCount_1, double dMipObj_1, const double Vals_1[],
        const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2,
        double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[],
        double* dDiffMetric),
    void* p, int priority)>
    XPRS_mse_addcbgetsolutiondiff = nullptr;
std::function<int(
    XPRSmipsolenum mse,
    int(XPRS_CC* f_mse_getsolutiondiff)(
        XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1,
        int iElemCount_1, double dMipObj_1, const double Vals_1[],
        const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2,
        double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[],
        double* dDiffMetric),
    void* p)>
    XPRS_mse_removecbgetsolutiondiff = nullptr;
std::function<int(XPRSmipsolenum mse,
                  int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p)>
    XPRS_mse_setcbmsghandler = nullptr;
std::function<int(XPRSmipsolenum mse,
                  int(XPRS_CC** f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void** p)>
    XPRS_mse_getcbmsghandler = nullptr;
std::function<int(XPRSmipsolenum mse,
                  int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p, int priority)>
    XPRS_mse_addcbmsghandler = nullptr;
std::function<int(XPRSmipsolenum mse,
                  int(XPRS_CC* f_msghandler)(
                      XPRSobject vXPRSObject, void* cbdata, void* thread,
                      const char* msg, int msgtype, int msgcode),
                  void* p)>
    XPRS_mse_removecbmsghandler = nullptr;
std::function<int(XPRSbranchobject* p_bo, XPRSprob prob, int isoriginal)>
    XPRS_bo_create = nullptr;
std::function<int(XPRSbranchobject bo)> XPRS_bo_destroy = nullptr;
std::function<int(XPRSbranchobject bo, int* p_status)> XPRS_bo_store = nullptr;
std::function<int(XPRSbranchobject bo, int nbranches)> XPRS_bo_addbranches =
    nullptr;
std::function<int(XPRSbranchobject bo, int* p_nbranches)> XPRS_bo_getbranches =
    nullptr;
std::function<int(XPRSbranchobject bo, int priority)> XPRS_bo_setpriority =
    nullptr;
std::function<int(XPRSbranchobject bo, int branch)> XPRS_bo_setpreferredbranch =
    nullptr;
std::function<int(XPRSbranchobject bo, int branch, int nbounds,
                  const char bndtype[], const int colind[],
                  const double bndval[])>
    XPRS_bo_addbounds = nullptr;
std::function<int(XPRSbranchobject bo, int branch, int* p_nbounds,
                  int maxbounds, char bndtype[], int colind[], double bndval[])>
    XPRS_bo_getbounds = nullptr;
std::function<int(XPRSbranchobject bo, int branch, int nrows, int ncoefs,
                  const char rowtype[], const double rhs[], const int start[],
                  const int colind[], const double rowcoef[])>
    XPRS_bo_addrows = nullptr;
std::function<int(XPRSbranchobject bo, int branch, int* p_nrows, int maxrows,
                  int* p_ncoefs, int maxcoefs, char rowtype[], double rhs[],
                  int start[], int colind[], double rowcoef[])>
    XPRS_bo_getrows = nullptr;
std::function<int(XPRSbranchobject bo, int branch, int ncuts,
                  const XPRScut cutind[])>
    XPRS_bo_addcuts = nullptr;
std::function<int(XPRSbranchobject bo, int* p_id)> XPRS_bo_getid = nullptr;
std::function<int(XPRSbranchobject bo, int* p_msgcode, char* msg, int maxbytes,
                  int* p_nbytes)>
    XPRS_bo_getlasterror = nullptr;
std::function<int(XPRSbranchobject bo, int* p_status)> XPRS_bo_validate =
    nullptr;
std::function<int(XPRSprob prob, const char* flags)> XPRSminim = nullptr;
std::function<int(XPRSprob prob, const char* flags)> XPRSmaxim = nullptr;
std::function<int(XPRSprob prob)> XPRSinitglobal = nullptr;
std::function<int(XPRSprob prob)> XPRSglobal = nullptr;
std::function<int(XPRSprob prob, double* p_cond, double* p_scaledcond)>
    XPRSbasiscondition = nullptr;
std::function<int(XPRSprob prob, int options, const char* flags,
                  const double solution[], double refined[], int* p_status)>
    XPRSrefinemipsol = nullptr;

void LoadXpressFunctions(DynamicLibrary* xpress_dynamic_library) {
  // This was generated with the parse_header_xpress.py script.
  // See the comment at the top of the script.

  // This is the 'assign' section.
  xpress_dynamic_library->GetFunction(&XPRScopycallbacks, "XPRScopycallbacks");
  xpress_dynamic_library->GetFunction(&XPRScopycontrols, "XPRScopycontrols");
  xpress_dynamic_library->GetFunction(&XPRScopyprob, "XPRScopyprob");
  xpress_dynamic_library->GetFunction(&XPRScreateprob, "XPRScreateprob");
  xpress_dynamic_library->GetFunction(&XPRSdestroyprob, "XPRSdestroyprob");
  xpress_dynamic_library->GetFunction(&XPRSinit, "XPRSinit");
  xpress_dynamic_library->GetFunction(&XPRSfree, "XPRSfree");
  xpress_dynamic_library->GetFunction(&XPRSgetlicerrmsg, "XPRSgetlicerrmsg");
  xpress_dynamic_library->GetFunction(&XPRSlicense, "XPRSlicense");
  xpress_dynamic_library->GetFunction(&XPRSbeginlicensing,
                                      "XPRSbeginlicensing");
  xpress_dynamic_library->GetFunction(&XPRSendlicensing, "XPRSendlicensing");
  xpress_dynamic_library->GetFunction(&XPRSsetcheckedmode,
                                      "XPRSsetcheckedmode");
  xpress_dynamic_library->GetFunction(&XPRSgetcheckedmode,
                                      "XPRSgetcheckedmode");
  xpress_dynamic_library->GetFunction(&XPRSgetbanner, "XPRSgetbanner");
  xpress_dynamic_library->GetFunction(&XPRSgetversion, "XPRSgetversion");
  xpress_dynamic_library->GetFunction(&XPRSgetdaysleft, "XPRSgetdaysleft");
  xpress_dynamic_library->GetFunction(&XPRSfeaturequery, "XPRSfeaturequery");
  xpress_dynamic_library->GetFunction(&XPRSsetprobname, "XPRSsetprobname");
  xpress_dynamic_library->GetFunction(&XPRSsetlogfile, "XPRSsetlogfile");
  xpress_dynamic_library->GetFunction(&XPRSsetdefaultcontrol,
                                      "XPRSsetdefaultcontrol");
  xpress_dynamic_library->GetFunction(&XPRSsetdefaults, "XPRSsetdefaults");
  xpress_dynamic_library->GetFunction(&XPRSinterrupt, "XPRSinterrupt");
  xpress_dynamic_library->GetFunction(&XPRSgetprobname, "XPRSgetprobname");
  xpress_dynamic_library->GetFunction(&XPRSsetintcontrol, "XPRSsetintcontrol");
  xpress_dynamic_library->GetFunction(&XPRSsetintcontrol64,
                                      "XPRSsetintcontrol64");
  xpress_dynamic_library->GetFunction(&XPRSsetdblcontrol, "XPRSsetdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRSsetstrcontrol, "XPRSsetstrcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetintcontrol, "XPRSgetintcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetintcontrol64,
                                      "XPRSgetintcontrol64");
  xpress_dynamic_library->GetFunction(&XPRSgetdblcontrol, "XPRSgetdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetstrcontrol, "XPRSgetstrcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetstringcontrol,
                                      "XPRSgetstringcontrol");
  xpress_dynamic_library->GetFunction(&XPRSgetintattrib, "XPRSgetintattrib");
  xpress_dynamic_library->GetFunction(&XPRSgetintattrib64,
                                      "XPRSgetintattrib64");
  xpress_dynamic_library->GetFunction(&XPRSgetstrattrib, "XPRSgetstrattrib");
  xpress_dynamic_library->GetFunction(&XPRSgetstringattrib,
                                      "XPRSgetstringattrib");
  xpress_dynamic_library->GetFunction(&XPRSgetdblattrib, "XPRSgetdblattrib");
  xpress_dynamic_library->GetFunction(&XPRSgetcontrolinfo,
                                      "XPRSgetcontrolinfo");
  xpress_dynamic_library->GetFunction(&XPRSgetattribinfo, "XPRSgetattribinfo");
  xpress_dynamic_library->GetFunction(&XPRSgetqobj, "XPRSgetqobj");
  xpress_dynamic_library->GetFunction(&XPRSreadprob, "XPRSreadprob");
  xpress_dynamic_library->GetFunction(&XPRSloadlp, "XPRSloadlp");
  xpress_dynamic_library->GetFunction(&XPRSloadlp64, "XPRSloadlp64");
  xpress_dynamic_library->GetFunction(&XPRSloadqp, "XPRSloadqp");
  xpress_dynamic_library->GetFunction(&XPRSloadqp64, "XPRSloadqp64");
  xpress_dynamic_library->GetFunction(&XPRSloadqglobal, "XPRSloadqglobal");
  xpress_dynamic_library->GetFunction(&XPRSloadqglobal64, "XPRSloadqglobal64");
  xpress_dynamic_library->GetFunction(&XPRSfixglobals, "XPRSfixglobals");
  xpress_dynamic_library->GetFunction(&XPRSloadmodelcuts, "XPRSloadmodelcuts");
  xpress_dynamic_library->GetFunction(&XPRSloaddelayedrows,
                                      "XPRSloaddelayedrows");
  xpress_dynamic_library->GetFunction(&XPRSloaddirs, "XPRSloaddirs");
  xpress_dynamic_library->GetFunction(&XPRSloadbranchdirs,
                                      "XPRSloadbranchdirs");
  xpress_dynamic_library->GetFunction(&XPRSloadpresolvedirs,
                                      "XPRSloadpresolvedirs");
  xpress_dynamic_library->GetFunction(&XPRSloadglobal, "XPRSloadglobal");
  xpress_dynamic_library->GetFunction(&XPRSloadglobal64, "XPRSloadglobal64");
  xpress_dynamic_library->GetFunction(&XPRSaddnames, "XPRSaddnames");
  xpress_dynamic_library->GetFunction(&XPRSaddsetnames, "XPRSaddsetnames");
  xpress_dynamic_library->GetFunction(&XPRSscale, "XPRSscale");
  xpress_dynamic_library->GetFunction(&XPRSreaddirs, "XPRSreaddirs");
  xpress_dynamic_library->GetFunction(&XPRSwritedirs, "XPRSwritedirs");
  xpress_dynamic_library->GetFunction(&XPRSsetindicators, "XPRSsetindicators");
  xpress_dynamic_library->GetFunction(&XPRSaddpwlcons, "XPRSaddpwlcons");
  xpress_dynamic_library->GetFunction(&XPRSaddpwlcons64, "XPRSaddpwlcons64");
  xpress_dynamic_library->GetFunction(&XPRSgetpwlcons, "XPRSgetpwlcons");
  xpress_dynamic_library->GetFunction(&XPRSgetpwlcons64, "XPRSgetpwlcons64");
  xpress_dynamic_library->GetFunction(&XPRSaddgencons, "XPRSaddgencons");
  xpress_dynamic_library->GetFunction(&XPRSaddgencons64, "XPRSaddgencons64");
  xpress_dynamic_library->GetFunction(&XPRSgetgencons, "XPRSgetgencons");
  xpress_dynamic_library->GetFunction(&XPRSgetgencons64, "XPRSgetgencons64");
  xpress_dynamic_library->GetFunction(&XPRSdelpwlcons, "XPRSdelpwlcons");
  xpress_dynamic_library->GetFunction(&XPRSdelgencons, "XPRSdelgencons");
  xpress_dynamic_library->GetFunction(&XPRSdumpcontrols, "XPRSdumpcontrols");
  xpress_dynamic_library->GetFunction(&XPRSgetindicators, "XPRSgetindicators");
  xpress_dynamic_library->GetFunction(&XPRSdelindicators, "XPRSdelindicators");
  xpress_dynamic_library->GetFunction(&XPRSgetdirs, "XPRSgetdirs");
  xpress_dynamic_library->GetFunction(&XPRSlpoptimize, "XPRSlpoptimize");
  xpress_dynamic_library->GetFunction(&XPRSmipoptimize, "XPRSmipoptimize");
  xpress_dynamic_library->GetFunction(&XPRSreadslxsol, "XPRSreadslxsol");
  xpress_dynamic_library->GetFunction(&XPRSalter, "XPRSalter");
  xpress_dynamic_library->GetFunction(&XPRSreadbasis, "XPRSreadbasis");
  xpress_dynamic_library->GetFunction(&XPRSreadbinsol, "XPRSreadbinsol");
  xpress_dynamic_library->GetFunction(&XPRSgetinfeas, "XPRSgetinfeas");
  xpress_dynamic_library->GetFunction(&XPRSgetscaledinfeas,
                                      "XPRSgetscaledinfeas");
  xpress_dynamic_library->GetFunction(&XPRSgetunbvec, "XPRSgetunbvec");
  xpress_dynamic_library->GetFunction(&XPRScrossoverlpsol,
                                      "XPRScrossoverlpsol");
  xpress_dynamic_library->GetFunction(&XPRStune, "XPRStune");
  xpress_dynamic_library->GetFunction(&XPRStunerwritemethod,
                                      "XPRStunerwritemethod");
  xpress_dynamic_library->GetFunction(&XPRStunerreadmethod,
                                      "XPRStunerreadmethod");
  xpress_dynamic_library->GetFunction(&XPRSgetbarnumstability,
                                      "XPRSgetbarnumstability");
  xpress_dynamic_library->GetFunction(&XPRSgetpivotorder, "XPRSgetpivotorder");
  xpress_dynamic_library->GetFunction(&XPRSgetpresolvemap,
                                      "XPRSgetpresolvemap");
  xpress_dynamic_library->GetFunction(&XPRSbtran, "XPRSbtran");
  xpress_dynamic_library->GetFunction(&XPRSftran, "XPRSftran");
  xpress_dynamic_library->GetFunction(&XPRSsparsebtran, "XPRSsparsebtran");
  xpress_dynamic_library->GetFunction(&XPRSsparseftran, "XPRSsparseftran");
  xpress_dynamic_library->GetFunction(&XPRSgetobj, "XPRSgetobj");
  xpress_dynamic_library->GetFunction(&XPRSgetrhs, "XPRSgetrhs");
  xpress_dynamic_library->GetFunction(&XPRSgetrhsrange, "XPRSgetrhsrange");
  xpress_dynamic_library->GetFunction(&XPRSgetlb, "XPRSgetlb");
  xpress_dynamic_library->GetFunction(&XPRSgetub, "XPRSgetub");
  xpress_dynamic_library->GetFunction(&XPRSgetcols, "XPRSgetcols");
  xpress_dynamic_library->GetFunction(&XPRSgetcols64, "XPRSgetcols64");
  xpress_dynamic_library->GetFunction(&XPRSgetrows, "XPRSgetrows");
  xpress_dynamic_library->GetFunction(&XPRSgetrows64, "XPRSgetrows64");
  xpress_dynamic_library->GetFunction(&XPRSgetrowflags, "XPRSgetrowflags");
  xpress_dynamic_library->GetFunction(&XPRSclearrowflags, "XPRSclearrowflags");
  xpress_dynamic_library->GetFunction(&XPRSgetcoef, "XPRSgetcoef");
  xpress_dynamic_library->GetFunction(&XPRSgetmqobj, "XPRSgetmqobj");
  xpress_dynamic_library->GetFunction(&XPRSgetmqobj64, "XPRSgetmqobj64");
  xpress_dynamic_library->GetFunction(&XPRSwritebasis, "XPRSwritebasis");
  xpress_dynamic_library->GetFunction(&XPRSwritesol, "XPRSwritesol");
  xpress_dynamic_library->GetFunction(&XPRSwritebinsol, "XPRSwritebinsol");
  xpress_dynamic_library->GetFunction(&XPRSgetsol, "XPRSgetsol");
  xpress_dynamic_library->GetFunction(&XPRSwriteprtsol, "XPRSwriteprtsol");
  xpress_dynamic_library->GetFunction(&XPRSwriteslxsol, "XPRSwriteslxsol");
  xpress_dynamic_library->GetFunction(&XPRSgetpresolvesol,
                                      "XPRSgetpresolvesol");
  xpress_dynamic_library->GetFunction(&XPRSgetlastbarsol, "XPRSgetlastbarsol");
  xpress_dynamic_library->GetFunction(&XPRSiisclear, "XPRSiisclear");
  xpress_dynamic_library->GetFunction(&XPRSiisfirst, "XPRSiisfirst");
  xpress_dynamic_library->GetFunction(&XPRSiisnext, "XPRSiisnext");
  xpress_dynamic_library->GetFunction(&XPRSiisstatus, "XPRSiisstatus");
  xpress_dynamic_library->GetFunction(&XPRSiisall, "XPRSiisall");
  xpress_dynamic_library->GetFunction(&XPRSiiswrite, "XPRSiiswrite");
  xpress_dynamic_library->GetFunction(&XPRSiisisolations, "XPRSiisisolations");
  xpress_dynamic_library->GetFunction(&XPRSgetiisdata, "XPRSgetiisdata");
  xpress_dynamic_library->GetFunction(&XPRSgetiis, "XPRSgetiis");
  xpress_dynamic_library->GetFunction(&XPRSloadpresolvebasis,
                                      "XPRSloadpresolvebasis");
  xpress_dynamic_library->GetFunction(&XPRSgetglobal, "XPRSgetglobal");
  xpress_dynamic_library->GetFunction(&XPRSgetglobal64, "XPRSgetglobal64");
  xpress_dynamic_library->GetFunction(&XPRSloadsecurevecs,
                                      "XPRSloadsecurevecs");
  xpress_dynamic_library->GetFunction(&XPRSaddrows, "XPRSaddrows");
  xpress_dynamic_library->GetFunction(&XPRSaddrows64, "XPRSaddrows64");
  xpress_dynamic_library->GetFunction(&XPRSdelrows, "XPRSdelrows");
  xpress_dynamic_library->GetFunction(&XPRSaddcols, "XPRSaddcols");
  xpress_dynamic_library->GetFunction(&XPRSaddcols64, "XPRSaddcols64");
  xpress_dynamic_library->GetFunction(&XPRSdelcols, "XPRSdelcols");
  xpress_dynamic_library->GetFunction(&XPRSchgcoltype, "XPRSchgcoltype");
  xpress_dynamic_library->GetFunction(&XPRSloadbasis, "XPRSloadbasis");
  xpress_dynamic_library->GetFunction(&XPRSpostsolve, "XPRSpostsolve");
  xpress_dynamic_library->GetFunction(&XPRSdelsets, "XPRSdelsets");
  xpress_dynamic_library->GetFunction(&XPRSaddsets, "XPRSaddsets");
  xpress_dynamic_library->GetFunction(&XPRSaddsets64, "XPRSaddsets64");
  xpress_dynamic_library->GetFunction(&XPRSstrongbranch, "XPRSstrongbranch");
  xpress_dynamic_library->GetFunction(&XPRSestimaterowdualranges,
                                      "XPRSestimaterowdualranges");
  xpress_dynamic_library->GetFunction(&XPRSsetmessagestatus,
                                      "XPRSsetmessagestatus");
  xpress_dynamic_library->GetFunction(&XPRSgetmessagestatus,
                                      "XPRSgetmessagestatus");
  xpress_dynamic_library->GetFunction(&XPRSchgobjsense, "XPRSchgobjsense");
  xpress_dynamic_library->GetFunction(&XPRSchgglblimit, "XPRSchgglblimit");
  xpress_dynamic_library->GetFunction(&XPRSrestore, "XPRSrestore");
  xpress_dynamic_library->GetFunction(&XPRSpivot, "XPRSpivot");
  xpress_dynamic_library->GetFunction(&XPRSloadlpsol, "XPRSloadlpsol");
  xpress_dynamic_library->GetFunction(&XPRSlogfilehandler,
                                      "XPRSlogfilehandler");
  xpress_dynamic_library->GetFunction(&XPRSrepairweightedinfeas,
                                      "XPRSrepairweightedinfeas");
  xpress_dynamic_library->GetFunction(&XPRSrepairweightedinfeasbounds,
                                      "XPRSrepairweightedinfeasbounds");
  xpress_dynamic_library->GetFunction(&XPRSrepairinfeas, "XPRSrepairinfeas");
  xpress_dynamic_library->GetFunction(&XPRSbasisstability,
                                      "XPRSbasisstability");
  xpress_dynamic_library->GetFunction(&XPRSgetindex, "XPRSgetindex");
  xpress_dynamic_library->GetFunction(&XPRSgetlasterror, "XPRSgetlasterror");
  xpress_dynamic_library->GetFunction(&XPRSgetobjecttypename,
                                      "XPRSgetobjecttypename");
  xpress_dynamic_library->GetFunction(&XPRSgetprimalray, "XPRSgetprimalray");
  xpress_dynamic_library->GetFunction(&XPRSgetdualray, "XPRSgetdualray");
  xpress_dynamic_library->GetFunction(&XPRSstrongbranchcb,
                                      "XPRSstrongbranchcb");
  xpress_dynamic_library->GetFunction(&XPRSloadmipsol, "XPRSloadmipsol");
  xpress_dynamic_library->GetFunction(&XPRSgetbasis, "XPRSgetbasis");
  xpress_dynamic_library->GetFunction(&XPRSgetbasisval, "XPRSgetbasisval");
  xpress_dynamic_library->GetFunction(&XPRSaddcuts, "XPRSaddcuts");
  xpress_dynamic_library->GetFunction(&XPRSaddcuts64, "XPRSaddcuts64");
  xpress_dynamic_library->GetFunction(&XPRSdelcuts, "XPRSdelcuts");
  xpress_dynamic_library->GetFunction(&XPRSdelcpcuts, "XPRSdelcpcuts");
  xpress_dynamic_library->GetFunction(&XPRSgetcutlist, "XPRSgetcutlist");
  xpress_dynamic_library->GetFunction(&XPRSgetcpcutlist, "XPRSgetcpcutlist");
  xpress_dynamic_library->GetFunction(&XPRSgetcpcuts, "XPRSgetcpcuts");
  xpress_dynamic_library->GetFunction(&XPRSgetcpcuts64, "XPRSgetcpcuts64");
  xpress_dynamic_library->GetFunction(&XPRSloadcuts, "XPRSloadcuts");
  xpress_dynamic_library->GetFunction(&XPRSstorecuts, "XPRSstorecuts");
  xpress_dynamic_library->GetFunction(&XPRSstorecuts64, "XPRSstorecuts64");
  xpress_dynamic_library->GetFunction(&XPRSpresolverow, "XPRSpresolverow");
  xpress_dynamic_library->GetFunction(&XPRSstorebounds, "XPRSstorebounds");
  xpress_dynamic_library->GetFunction(&XPRSsetbranchcuts, "XPRSsetbranchcuts");
  xpress_dynamic_library->GetFunction(&XPRSsetbranchbounds,
                                      "XPRSsetbranchbounds");
  xpress_dynamic_library->GetFunction(&XPRSgetpivots, "XPRSgetpivots");
  xpress_dynamic_library->GetFunction(&XPRSwriteprob, "XPRSwriteprob");
  xpress_dynamic_library->GetFunction(&XPRScalcslacks, "XPRScalcslacks");
  xpress_dynamic_library->GetFunction(&XPRScalcreducedcosts,
                                      "XPRScalcreducedcosts");
  xpress_dynamic_library->GetFunction(&XPRScalcobjective, "XPRScalcobjective");
  xpress_dynamic_library->GetFunction(&XPRScalcsolinfo, "XPRScalcsolinfo");
  xpress_dynamic_library->GetFunction(&XPRSgetrowtype, "XPRSgetrowtype");
  xpress_dynamic_library->GetFunction(&XPRSgetpresolvebasis,
                                      "XPRSgetpresolvebasis");
  xpress_dynamic_library->GetFunction(&XPRSgetcoltype, "XPRSgetcoltype");
  xpress_dynamic_library->GetFunction(&XPRSsave, "XPRSsave");
  xpress_dynamic_library->GetFunction(&XPRSsaveas, "XPRSsaveas");
  xpress_dynamic_library->GetFunction(&XPRSgetnamelist, "XPRSgetnamelist");
  xpress_dynamic_library->GetFunction(&XPRSgetnamelistobject,
                                      "XPRSgetnamelistobject");
  xpress_dynamic_library->GetFunction(&XPRSaddmipsol, "XPRSaddmipsol");
  xpress_dynamic_library->GetFunction(&XPRSgetcutslack, "XPRSgetcutslack");
  xpress_dynamic_library->GetFunction(&XPRSgetcutmap, "XPRSgetcutmap");
  xpress_dynamic_library->GetFunction(&XPRSgetnames, "XPRSgetnames");
  xpress_dynamic_library->GetFunction(&XPRSgetlpsol, "XPRSgetlpsol");
  xpress_dynamic_library->GetFunction(&XPRSgetlpsolval, "XPRSgetlpsolval");
  xpress_dynamic_library->GetFunction(&XPRSgetmipsol, "XPRSgetmipsol");
  xpress_dynamic_library->GetFunction(&XPRSgetmipsolval, "XPRSgetmipsolval");
  xpress_dynamic_library->GetFunction(&XPRSchgbounds, "XPRSchgbounds");
  xpress_dynamic_library->GetFunction(&XPRSchgobj, "XPRSchgobj");
  xpress_dynamic_library->GetFunction(&XPRSchgcoef, "XPRSchgcoef");
  xpress_dynamic_library->GetFunction(&XPRSchgmcoef, "XPRSchgmcoef");
  xpress_dynamic_library->GetFunction(&XPRSchgmcoef64, "XPRSchgmcoef64");
  xpress_dynamic_library->GetFunction(&XPRSchgmqobj, "XPRSchgmqobj");
  xpress_dynamic_library->GetFunction(&XPRSchgmqobj64, "XPRSchgmqobj64");
  xpress_dynamic_library->GetFunction(&XPRSchgqobj, "XPRSchgqobj");
  xpress_dynamic_library->GetFunction(&XPRSchgrhs, "XPRSchgrhs");
  xpress_dynamic_library->GetFunction(&XPRSchgrhsrange, "XPRSchgrhsrange");
  xpress_dynamic_library->GetFunction(&XPRSchgrowtype, "XPRSchgrowtype");
  xpress_dynamic_library->GetFunction(&XPRSsetcblplog, "XPRSsetcblplog");
  xpress_dynamic_library->GetFunction(&XPRSgetcblplog, "XPRSgetcblplog");
  xpress_dynamic_library->GetFunction(&XPRSaddcblplog, "XPRSaddcblplog");
  xpress_dynamic_library->GetFunction(&XPRSremovecblplog, "XPRSremovecblplog");
  xpress_dynamic_library->GetFunction(&XPRSsetcbgloballog,
                                      "XPRSsetcbgloballog");
  xpress_dynamic_library->GetFunction(&XPRSgetcbgloballog,
                                      "XPRSgetcbgloballog");
  xpress_dynamic_library->GetFunction(&XPRSaddcbgloballog,
                                      "XPRSaddcbgloballog");
  xpress_dynamic_library->GetFunction(&XPRSremovecbgloballog,
                                      "XPRSremovecbgloballog");
  xpress_dynamic_library->GetFunction(&XPRSsetcbcutlog, "XPRSsetcbcutlog");
  xpress_dynamic_library->GetFunction(&XPRSgetcbcutlog, "XPRSgetcbcutlog");
  xpress_dynamic_library->GetFunction(&XPRSaddcbcutlog, "XPRSaddcbcutlog");
  xpress_dynamic_library->GetFunction(&XPRSremovecbcutlog,
                                      "XPRSremovecbcutlog");
  xpress_dynamic_library->GetFunction(&XPRSsetcbbarlog, "XPRSsetcbbarlog");
  xpress_dynamic_library->GetFunction(&XPRSgetcbbarlog, "XPRSgetcbbarlog");
  xpress_dynamic_library->GetFunction(&XPRSaddcbbarlog, "XPRSaddcbbarlog");
  xpress_dynamic_library->GetFunction(&XPRSremovecbbarlog,
                                      "XPRSremovecbbarlog");
  xpress_dynamic_library->GetFunction(&XPRSsetcbcutmgr, "XPRSsetcbcutmgr");
  xpress_dynamic_library->GetFunction(&XPRSgetcbcutmgr, "XPRSgetcbcutmgr");
  xpress_dynamic_library->GetFunction(&XPRSaddcbcutmgr, "XPRSaddcbcutmgr");
  xpress_dynamic_library->GetFunction(&XPRSremovecbcutmgr,
                                      "XPRSremovecbcutmgr");
  xpress_dynamic_library->GetFunction(&XPRSsetcbchgnode, "XPRSsetcbchgnode");
  xpress_dynamic_library->GetFunction(&XPRSgetcbchgnode, "XPRSgetcbchgnode");
  xpress_dynamic_library->GetFunction(&XPRSaddcbchgnode, "XPRSaddcbchgnode");
  xpress_dynamic_library->GetFunction(&XPRSremovecbchgnode,
                                      "XPRSremovecbchgnode");
  xpress_dynamic_library->GetFunction(&XPRSsetcboptnode, "XPRSsetcboptnode");
  xpress_dynamic_library->GetFunction(&XPRSgetcboptnode, "XPRSgetcboptnode");
  xpress_dynamic_library->GetFunction(&XPRSaddcboptnode, "XPRSaddcboptnode");
  xpress_dynamic_library->GetFunction(&XPRSremovecboptnode,
                                      "XPRSremovecboptnode");
  xpress_dynamic_library->GetFunction(&XPRSsetcbprenode, "XPRSsetcbprenode");
  xpress_dynamic_library->GetFunction(&XPRSgetcbprenode, "XPRSgetcbprenode");
  xpress_dynamic_library->GetFunction(&XPRSaddcbprenode, "XPRSaddcbprenode");
  xpress_dynamic_library->GetFunction(&XPRSremovecbprenode,
                                      "XPRSremovecbprenode");
  xpress_dynamic_library->GetFunction(&XPRSsetcbinfnode, "XPRSsetcbinfnode");
  xpress_dynamic_library->GetFunction(&XPRSgetcbinfnode, "XPRSgetcbinfnode");
  xpress_dynamic_library->GetFunction(&XPRSaddcbinfnode, "XPRSaddcbinfnode");
  xpress_dynamic_library->GetFunction(&XPRSremovecbinfnode,
                                      "XPRSremovecbinfnode");
  xpress_dynamic_library->GetFunction(&XPRSsetcbnodecutoff,
                                      "XPRSsetcbnodecutoff");
  xpress_dynamic_library->GetFunction(&XPRSgetcbnodecutoff,
                                      "XPRSgetcbnodecutoff");
  xpress_dynamic_library->GetFunction(&XPRSaddcbnodecutoff,
                                      "XPRSaddcbnodecutoff");
  xpress_dynamic_library->GetFunction(&XPRSremovecbnodecutoff,
                                      "XPRSremovecbnodecutoff");
  xpress_dynamic_library->GetFunction(&XPRSsetcbintsol, "XPRSsetcbintsol");
  xpress_dynamic_library->GetFunction(&XPRSgetcbintsol, "XPRSgetcbintsol");
  xpress_dynamic_library->GetFunction(&XPRSaddcbintsol, "XPRSaddcbintsol");
  xpress_dynamic_library->GetFunction(&XPRSremovecbintsol,
                                      "XPRSremovecbintsol");
  xpress_dynamic_library->GetFunction(&XPRSsetcbpreintsol,
                                      "XPRSsetcbpreintsol");
  xpress_dynamic_library->GetFunction(&XPRSgetcbpreintsol,
                                      "XPRSgetcbpreintsol");
  xpress_dynamic_library->GetFunction(&XPRSaddcbpreintsol,
                                      "XPRSaddcbpreintsol");
  xpress_dynamic_library->GetFunction(&XPRSremovecbpreintsol,
                                      "XPRSremovecbpreintsol");
  xpress_dynamic_library->GetFunction(&XPRSsetcbchgbranch,
                                      "XPRSsetcbchgbranch");
  xpress_dynamic_library->GetFunction(&XPRSgetcbchgbranch,
                                      "XPRSgetcbchgbranch");
  xpress_dynamic_library->GetFunction(&XPRSaddcbchgbranch,
                                      "XPRSaddcbchgbranch");
  xpress_dynamic_library->GetFunction(&XPRSremovecbchgbranch,
                                      "XPRSremovecbchgbranch");
  xpress_dynamic_library->GetFunction(&XPRSsetcbestimate, "XPRSsetcbestimate");
  xpress_dynamic_library->GetFunction(&XPRSgetcbestimate, "XPRSgetcbestimate");
  xpress_dynamic_library->GetFunction(&XPRSaddcbestimate, "XPRSaddcbestimate");
  xpress_dynamic_library->GetFunction(&XPRSremovecbestimate,
                                      "XPRSremovecbestimate");
  xpress_dynamic_library->GetFunction(&XPRSsetcbsepnode, "XPRSsetcbsepnode");
  xpress_dynamic_library->GetFunction(&XPRSgetcbsepnode, "XPRSgetcbsepnode");
  xpress_dynamic_library->GetFunction(&XPRSaddcbsepnode, "XPRSaddcbsepnode");
  xpress_dynamic_library->GetFunction(&XPRSremovecbsepnode,
                                      "XPRSremovecbsepnode");
  xpress_dynamic_library->GetFunction(&XPRSsetcbmessage, "XPRSsetcbmessage");
  xpress_dynamic_library->GetFunction(&XPRSgetcbmessage, "XPRSgetcbmessage");
  xpress_dynamic_library->GetFunction(&XPRSaddcbmessage, "XPRSaddcbmessage");
  xpress_dynamic_library->GetFunction(&XPRSremovecbmessage,
                                      "XPRSremovecbmessage");
  xpress_dynamic_library->GetFunction(&XPRSsetcbmipthread,
                                      "XPRSsetcbmipthread");
  xpress_dynamic_library->GetFunction(&XPRSgetcbmipthread,
                                      "XPRSgetcbmipthread");
  xpress_dynamic_library->GetFunction(&XPRSaddcbmipthread,
                                      "XPRSaddcbmipthread");
  xpress_dynamic_library->GetFunction(&XPRSremovecbmipthread,
                                      "XPRSremovecbmipthread");
  xpress_dynamic_library->GetFunction(&XPRSsetcbdestroymt,
                                      "XPRSsetcbdestroymt");
  xpress_dynamic_library->GetFunction(&XPRSgetcbdestroymt,
                                      "XPRSgetcbdestroymt");
  xpress_dynamic_library->GetFunction(&XPRSaddcbdestroymt,
                                      "XPRSaddcbdestroymt");
  xpress_dynamic_library->GetFunction(&XPRSremovecbdestroymt,
                                      "XPRSremovecbdestroymt");
  xpress_dynamic_library->GetFunction(&XPRSsetcbnewnode, "XPRSsetcbnewnode");
  xpress_dynamic_library->GetFunction(&XPRSgetcbnewnode, "XPRSgetcbnewnode");
  xpress_dynamic_library->GetFunction(&XPRSaddcbnewnode, "XPRSaddcbnewnode");
  xpress_dynamic_library->GetFunction(&XPRSremovecbnewnode,
                                      "XPRSremovecbnewnode");
  xpress_dynamic_library->GetFunction(&XPRSsetcbbariteration,
                                      "XPRSsetcbbariteration");
  xpress_dynamic_library->GetFunction(&XPRSgetcbbariteration,
                                      "XPRSgetcbbariteration");
  xpress_dynamic_library->GetFunction(&XPRSaddcbbariteration,
                                      "XPRSaddcbbariteration");
  xpress_dynamic_library->GetFunction(&XPRSremovecbbariteration,
                                      "XPRSremovecbbariteration");
  xpress_dynamic_library->GetFunction(&XPRSsetcbpresolve, "XPRSsetcbpresolve");
  xpress_dynamic_library->GetFunction(&XPRSgetcbpresolve, "XPRSgetcbpresolve");
  xpress_dynamic_library->GetFunction(&XPRSaddcbpresolve, "XPRSaddcbpresolve");
  xpress_dynamic_library->GetFunction(&XPRSremovecbpresolve,
                                      "XPRSremovecbpresolve");
  xpress_dynamic_library->GetFunction(&XPRSsetcbchgbranchobject,
                                      "XPRSsetcbchgbranchobject");
  xpress_dynamic_library->GetFunction(&XPRSgetcbchgbranchobject,
                                      "XPRSgetcbchgbranchobject");
  xpress_dynamic_library->GetFunction(&XPRSaddcbchgbranchobject,
                                      "XPRSaddcbchgbranchobject");
  xpress_dynamic_library->GetFunction(&XPRSremovecbchgbranchobject,
                                      "XPRSremovecbchgbranchobject");
  xpress_dynamic_library->GetFunction(&XPRSsetcbcomputerestart,
                                      "XPRSsetcbcomputerestart");
  xpress_dynamic_library->GetFunction(&XPRSgetcbcomputerestart,
                                      "XPRSgetcbcomputerestart");
  xpress_dynamic_library->GetFunction(&XPRSaddcbcomputerestart,
                                      "XPRSaddcbcomputerestart");
  xpress_dynamic_library->GetFunction(&XPRSremovecbcomputerestart,
                                      "XPRSremovecbcomputerestart");
  xpress_dynamic_library->GetFunction(&XPRSsetcbgapnotify,
                                      "XPRSsetcbgapnotify");
  xpress_dynamic_library->GetFunction(&XPRSgetcbgapnotify,
                                      "XPRSgetcbgapnotify");
  xpress_dynamic_library->GetFunction(&XPRSaddcbgapnotify,
                                      "XPRSaddcbgapnotify");
  xpress_dynamic_library->GetFunction(&XPRSremovecbgapnotify,
                                      "XPRSremovecbgapnotify");
  xpress_dynamic_library->GetFunction(&XPRSsetcbusersolnotify,
                                      "XPRSsetcbusersolnotify");
  xpress_dynamic_library->GetFunction(&XPRSgetcbusersolnotify,
                                      "XPRSgetcbusersolnotify");
  xpress_dynamic_library->GetFunction(&XPRSaddcbusersolnotify,
                                      "XPRSaddcbusersolnotify");
  xpress_dynamic_library->GetFunction(&XPRSremovecbusersolnotify,
                                      "XPRSremovecbusersolnotify");
  xpress_dynamic_library->GetFunction(&XPRSsetcbbeforesolve,
                                      "XPRSsetcbbeforesolve");
  xpress_dynamic_library->GetFunction(&XPRSgetcbbeforesolve,
                                      "XPRSgetcbbeforesolve");
  xpress_dynamic_library->GetFunction(&XPRSaddcbbeforesolve,
                                      "XPRSaddcbbeforesolve");
  xpress_dynamic_library->GetFunction(&XPRSremovecbbeforesolve,
                                      "XPRSremovecbbeforesolve");
  xpress_dynamic_library->GetFunction(&XPRSsetcbchecktime,
                                      "XPRSsetcbchecktime");
  xpress_dynamic_library->GetFunction(&XPRSgetcbchecktime,
                                      "XPRSgetcbchecktime");
  xpress_dynamic_library->GetFunction(&XPRSaddcbchecktime,
                                      "XPRSaddcbchecktime");
  xpress_dynamic_library->GetFunction(&XPRSremovecbchecktime,
                                      "XPRSremovecbchecktime");
  xpress_dynamic_library->GetFunction(&XPRSobjsa, "XPRSobjsa");
  xpress_dynamic_library->GetFunction(&XPRSbndsa, "XPRSbndsa");
  xpress_dynamic_library->GetFunction(&XPRSrhssa, "XPRSrhssa");
  xpress_dynamic_library->GetFunction(&XPRS_ge_setcbmsghandler,
                                      "XPRS_ge_setcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_ge_getcbmsghandler,
                                      "XPRS_ge_getcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_ge_addcbmsghandler,
                                      "XPRS_ge_addcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_ge_removecbmsghandler,
                                      "XPRS_ge_removecbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_ge_setarchconsistency,
                                      "XPRS_ge_setarchconsistency");
  xpress_dynamic_library->GetFunction(&XPRS_ge_setsafemode,
                                      "XPRS_ge_setsafemode");
  xpress_dynamic_library->GetFunction(&XPRS_ge_getsafemode,
                                      "XPRS_ge_getsafemode");
  xpress_dynamic_library->GetFunction(&XPRS_ge_setdebugmode,
                                      "XPRS_ge_setdebugmode");
  xpress_dynamic_library->GetFunction(&XPRS_ge_getdebugmode,
                                      "XPRS_ge_getdebugmode");
  xpress_dynamic_library->GetFunction(&XPRS_ge_getlasterror,
                                      "XPRS_ge_getlasterror");
  xpress_dynamic_library->GetFunction(&XPRS_ge_setcomputeallowed,
                                      "XPRS_ge_setcomputeallowed");
  xpress_dynamic_library->GetFunction(&XPRS_ge_getcomputeallowed,
                                      "XPRS_ge_getcomputeallowed");
  xpress_dynamic_library->GetFunction(&XPRS_msp_create, "XPRS_msp_create");
  xpress_dynamic_library->GetFunction(&XPRS_msp_destroy, "XPRS_msp_destroy");
  xpress_dynamic_library->GetFunction(&XPRS_msp_probattach,
                                      "XPRS_msp_probattach");
  xpress_dynamic_library->GetFunction(&XPRS_msp_probdetach,
                                      "XPRS_msp_probdetach");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getsollist,
                                      "XPRS_msp_getsollist");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getsollist2,
                                      "XPRS_msp_getsollist2");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getsol, "XPRS_msp_getsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getslack, "XPRS_msp_getslack");
  xpress_dynamic_library->GetFunction(&XPRS_msp_loadsol, "XPRS_msp_loadsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_delsol, "XPRS_msp_delsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getintattribprobsol,
                                      "XPRS_msp_getintattribprobsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getdblattribprobsol,
                                      "XPRS_msp_getdblattribprobsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getintattribprob,
                                      "XPRS_msp_getintattribprob");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getdblattribprob,
                                      "XPRS_msp_getdblattribprob");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getintattribsol,
                                      "XPRS_msp_getintattribsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getdblattribsol,
                                      "XPRS_msp_getdblattribsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getintcontrolsol,
                                      "XPRS_msp_getintcontrolsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getdblcontrolsol,
                                      "XPRS_msp_getdblcontrolsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_setintcontrolsol,
                                      "XPRS_msp_setintcontrolsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_setdblcontrolsol,
                                      "XPRS_msp_setdblcontrolsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getintattribprobextreme,
                                      "XPRS_msp_getintattribprobextreme");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getdblattribprobextreme,
                                      "XPRS_msp_getdblattribprobextreme");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getintattrib,
                                      "XPRS_msp_getintattrib");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getdblattrib,
                                      "XPRS_msp_getdblattrib");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getintcontrol,
                                      "XPRS_msp_getintcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getdblcontrol,
                                      "XPRS_msp_getdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_setintcontrol,
                                      "XPRS_msp_setintcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_setdblcontrol,
                                      "XPRS_msp_setdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_setsolname,
                                      "XPRS_msp_setsolname");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getsolname,
                                      "XPRS_msp_getsolname");
  xpress_dynamic_library->GetFunction(&XPRS_msp_findsolbyname,
                                      "XPRS_msp_findsolbyname");
  xpress_dynamic_library->GetFunction(&XPRS_msp_writeslxsol,
                                      "XPRS_msp_writeslxsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_readslxsol,
                                      "XPRS_msp_readslxsol");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getlasterror,
                                      "XPRS_msp_getlasterror");
  xpress_dynamic_library->GetFunction(&XPRS_msp_setcbmsghandler,
                                      "XPRS_msp_setcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_msp_getcbmsghandler,
                                      "XPRS_msp_getcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_msp_addcbmsghandler,
                                      "XPRS_msp_addcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_msp_removecbmsghandler,
                                      "XPRS_msp_removecbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_nml_create, "XPRS_nml_create");
  xpress_dynamic_library->GetFunction(&XPRS_nml_destroy, "XPRS_nml_destroy");
  xpress_dynamic_library->GetFunction(&XPRS_nml_getnamecount,
                                      "XPRS_nml_getnamecount");
  xpress_dynamic_library->GetFunction(&XPRS_nml_getmaxnamelen,
                                      "XPRS_nml_getmaxnamelen");
  xpress_dynamic_library->GetFunction(&XPRS_nml_getnames, "XPRS_nml_getnames");
  xpress_dynamic_library->GetFunction(&XPRS_nml_addnames, "XPRS_nml_addnames");
  xpress_dynamic_library->GetFunction(&XPRS_nml_removenames,
                                      "XPRS_nml_removenames");
  xpress_dynamic_library->GetFunction(&XPRS_nml_findname, "XPRS_nml_findname");
  xpress_dynamic_library->GetFunction(&XPRS_nml_copynames,
                                      "XPRS_nml_copynames");
  xpress_dynamic_library->GetFunction(&XPRS_nml_getlasterror,
                                      "XPRS_nml_getlasterror");
  xpress_dynamic_library->GetFunction(&XPRSaddqmatrix, "XPRSaddqmatrix");
  xpress_dynamic_library->GetFunction(&XPRSaddqmatrix64, "XPRSaddqmatrix64");
  xpress_dynamic_library->GetFunction(&XPRSdelqmatrix, "XPRSdelqmatrix");
  xpress_dynamic_library->GetFunction(&XPRSloadqcqp, "XPRSloadqcqp");
  xpress_dynamic_library->GetFunction(&XPRSloadqcqp64, "XPRSloadqcqp64");
  xpress_dynamic_library->GetFunction(&XPRSloadqcqpglobal,
                                      "XPRSloadqcqpglobal");
  xpress_dynamic_library->GetFunction(&XPRSloadqcqpglobal64,
                                      "XPRSloadqcqpglobal64");
  xpress_dynamic_library->GetFunction(&XPRSgetqrowcoeff, "XPRSgetqrowcoeff");
  xpress_dynamic_library->GetFunction(&XPRSgetqrowqmatrix,
                                      "XPRSgetqrowqmatrix");
  xpress_dynamic_library->GetFunction(&XPRSgetqrowqmatrixtriplets,
                                      "XPRSgetqrowqmatrixtriplets");
  xpress_dynamic_library->GetFunction(&XPRSchgqrowcoeff, "XPRSchgqrowcoeff");
  xpress_dynamic_library->GetFunction(&XPRSgetqrows, "XPRSgetqrows");
  xpress_dynamic_library->GetFunction(&XPRS_mse_create, "XPRS_mse_create");
  xpress_dynamic_library->GetFunction(&XPRS_mse_destroy, "XPRS_mse_destroy");
  xpress_dynamic_library->GetFunction(&XPRS_mse_minim, "XPRS_mse_minim");
  xpress_dynamic_library->GetFunction(&XPRS_mse_maxim, "XPRS_mse_maxim");
  xpress_dynamic_library->GetFunction(&XPRS_mse_opt, "XPRS_mse_opt");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getsollist,
                                      "XPRS_mse_getsollist");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getsolmetric,
                                      "XPRS_mse_getsolmetric");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getcullchoice,
                                      "XPRS_mse_getcullchoice");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getintattrib,
                                      "XPRS_mse_getintattrib");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getdblattrib,
                                      "XPRS_mse_getdblattrib");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getintcontrol,
                                      "XPRS_mse_getintcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getdblcontrol,
                                      "XPRS_mse_getdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_mse_setintcontrol,
                                      "XPRS_mse_setintcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_mse_setdblcontrol,
                                      "XPRS_mse_setdblcontrol");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getlasterror,
                                      "XPRS_mse_getlasterror");
  xpress_dynamic_library->GetFunction(&XPRS_mse_setsolbasename,
                                      "XPRS_mse_setsolbasename");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getsolbasename,
                                      "XPRS_mse_getsolbasename");
  xpress_dynamic_library->GetFunction(&XPRS_mse_setcbgetsolutiondiff,
                                      "XPRS_mse_setcbgetsolutiondiff");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getcbgetsolutiondiff,
                                      "XPRS_mse_getcbgetsolutiondiff");
  xpress_dynamic_library->GetFunction(&XPRS_mse_addcbgetsolutiondiff,
                                      "XPRS_mse_addcbgetsolutiondiff");
  xpress_dynamic_library->GetFunction(&XPRS_mse_removecbgetsolutiondiff,
                                      "XPRS_mse_removecbgetsolutiondiff");
  xpress_dynamic_library->GetFunction(&XPRS_mse_setcbmsghandler,
                                      "XPRS_mse_setcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_mse_getcbmsghandler,
                                      "XPRS_mse_getcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_mse_addcbmsghandler,
                                      "XPRS_mse_addcbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_mse_removecbmsghandler,
                                      "XPRS_mse_removecbmsghandler");
  xpress_dynamic_library->GetFunction(&XPRS_bo_create, "XPRS_bo_create");
  xpress_dynamic_library->GetFunction(&XPRS_bo_destroy, "XPRS_bo_destroy");
  xpress_dynamic_library->GetFunction(&XPRS_bo_store, "XPRS_bo_store");
  xpress_dynamic_library->GetFunction(&XPRS_bo_addbranches,
                                      "XPRS_bo_addbranches");
  xpress_dynamic_library->GetFunction(&XPRS_bo_getbranches,
                                      "XPRS_bo_getbranches");
  xpress_dynamic_library->GetFunction(&XPRS_bo_setpriority,
                                      "XPRS_bo_setpriority");
  xpress_dynamic_library->GetFunction(&XPRS_bo_setpreferredbranch,
                                      "XPRS_bo_setpreferredbranch");
  xpress_dynamic_library->GetFunction(&XPRS_bo_addbounds, "XPRS_bo_addbounds");
  xpress_dynamic_library->GetFunction(&XPRS_bo_getbounds, "XPRS_bo_getbounds");
  xpress_dynamic_library->GetFunction(&XPRS_bo_addrows, "XPRS_bo_addrows");
  xpress_dynamic_library->GetFunction(&XPRS_bo_getrows, "XPRS_bo_getrows");
  xpress_dynamic_library->GetFunction(&XPRS_bo_addcuts, "XPRS_bo_addcuts");
  xpress_dynamic_library->GetFunction(&XPRS_bo_getid, "XPRS_bo_getid");
  xpress_dynamic_library->GetFunction(&XPRS_bo_getlasterror,
                                      "XPRS_bo_getlasterror");
  xpress_dynamic_library->GetFunction(&XPRS_bo_validate, "XPRS_bo_validate");
  xpress_dynamic_library->GetFunction(&XPRSminim, "XPRSminim");
  xpress_dynamic_library->GetFunction(&XPRSmaxim, "XPRSmaxim");
  xpress_dynamic_library->GetFunction(&XPRSinitglobal, "XPRSinitglobal");
  xpress_dynamic_library->GetFunction(&XPRSglobal, "XPRSglobal");
  xpress_dynamic_library->GetFunction(&XPRSbasiscondition,
                                      "XPRSbasiscondition");
  xpress_dynamic_library->GetFunction(&XPRSrefinemipsol, "XPRSrefinemipsol");
}

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
  const char* xpress_home_from_env = getenv("XPRESSDIR");
  if (xpress_home_from_env != nullptr) {
#if defined(_MSC_VER)  // Windows
    potential_paths.push_back(
        absl::StrCat(xpress_home_from_env, "\\bin\\xprs.dll"));
#elif defined(__APPLE__)  // OS X
    potential_paths.push_back(
        absl::StrCat(xpress_home_from_env, "/lib/libxprs.dylib"));
#elif defined(__GNUC__)   // Linux
    potential_paths.push_back(
        absl::StrCat(xpress_home_from_env, "/lib/libxprs.so"));
#else
    LOG(ERROR) << "OS Not recognized by xpress/environment.cc."
               << " You won't be able to use Xpress.";
#endif
  } else {
    LOG(WARNING) << "Environment variable XPRESSDIR undefined.\n";
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
  static absl::Mutex mutex;

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
      LoadXpressFunctions(&xpress_library);
      xpress_load_status = absl::OkStatus();
    } else {
      xpress_load_status = absl::NotFoundError(absl::StrCat(
          "Could not find the Xpress shared library. Looked in: [",
          absl::StrJoin(canonical_paths, "', '"),
          "]. Please check environment variable XPRESSDIR"));
    }
  });
  xpresspath.clear();
  xpresspath.append(xpress_lib_path);
  return xpress_load_status;
}

/** init XPRESS environment */
bool initXpressEnv(bool verbose, int xpress_oem_license_key) {
  std::string xpresspath;
  absl::Status status = LoadXpressDynamicLibrary(xpresspath);
  if (!status.ok()) {
    LOG(ERROR) << status << "\n";
    return false;
  }

  const char* xpress_from_env = getenv("XPRESS");
  if (xpress_from_env == nullptr) {
    if (verbose) {
      LOG(WARNING)
          << "XpressInterface Error : Environment variable XPRESS undefined.\n";
    }
    if (xpresspath.empty()) {
      return false;
    }
  } else {
    xpresspath = xpress_from_env;
  }

  int code;

  // if not an OEM key
  if (xpress_oem_license_key == 0) {
    if (verbose) {
      LOG(WARNING) << "XpressInterface : Initialising xpress-MP with parameter "
                   << xpresspath << "\n";
    }

    code = XPRSinit(xpresspath.c_str());

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
      LOG(ERROR) << "XpressInterface: Xpress found at " << xpresspath << "\n";
      char errmsg[256];
      XPRSgetlicerrmsg(errmsg, 256);

      LOG(ERROR) << "XpressInterface : License error : " << errmsg
                 << " (XPRSinit returned code " << code << "). Please check"
                 << " environment variable XPRESS.\n";

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

bool XpressIsCorrectlyInstalled() {
  bool correctlyInstalled = initXpressEnv(false);
  if (correctlyInstalled) {
    XPRSfree();
  }
  return correctlyInstalled;
}

}  // namespace operations_research
