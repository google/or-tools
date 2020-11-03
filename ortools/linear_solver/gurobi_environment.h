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

#ifndef OR_TOOLS_LINEAR_SOLVER_GUROBI_ENVIRONMENT_H_
#define OR_TOOLS_LINEAR_SOLVER_GUROBI_ENVIRONMENT_H_

#include "absl/status/status.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/dynamic_library.h"
#include "ortools/base/logging.h"

#if defined(_MSC_VER)
#define STDCALL __stdcall
#else
#define STDCALL
#endif

extern "C" {
typedef struct _GRBmodel GRBmodel;
typedef struct _GRBenv GRBenv;
}

namespace operations_research {
absl::Status LoadGurobiEnvironment(GRBenv** env);

#define CB_ARGS GRBmodel *model, void *cbdata, int where, void *usrdata
extern std::function<int(GRBmodel*, int, int*, double*, double, double,
                         const char*)>
    GRBaddrangeconstr;

extern std::function<int(GRBmodel* model, int numnz, int* vind, double* vval,
                         double obj, double lb, double ub, char vtype,
                         const char* varname)>
    GRBaddvar;

extern std::function<int(GRBmodel*, int, int, int*, int*, double*, double*,
                         double*, double*, char*, char**)>
    GRBaddvars;

extern std::function<int(GRBmodel* model, int numchgs, int* cind, int* vind,
                         double* val)>
    GRBchgcoeffs;
extern std::function<void(GRBenv*)> GRBfreeenv;
extern std::function<int(GRBmodel*)> GRBfreemodel;
extern std::function<int(GRBmodel*, const char*, int, char*)>
    GRBgetcharattrelement;
extern std::function<int(GRBmodel*, const char*, double*)> GRBgetdblattr;
extern std::function<int(GRBmodel*, const char*, int, int, double*)>
    GRBgetdblattrarray;
extern std::function<int(GRBmodel*, const char*, int, double*)>
    GRBgetdblattrelement;
extern std::function<int(GRBenv*, const char*, double*)> GRBgetdblparam;
extern std::function<GRBenv*(GRBmodel*)> GRBgetenv;
extern std::function<char*(GRBenv*)> GRBgeterrormsg;
extern std::function<int(GRBmodel*, const char*, int*)> GRBgetintattr;
extern std::function<int(GRBmodel*, const char*, int, int*)>
    GRBgetintattrelement;
extern std::function<int(GRBenv**, const char*)> GRBloadenv;
extern std::function<int(GRBenv*, GRBmodel**, const char*, int numvars, double*,
                         double*, double*, char*, char**)>
    GRBnewmodel;
extern std::function<int(GRBmodel*)> GRBoptimize;
extern std::function<int(GRBenv*, const char*)> GRBreadparams;
extern std::function<int(GRBenv*)> GRBresetparams;
extern std::function<int(GRBmodel*, const char*, int, char)>
    GRBsetcharattrelement;
extern std::function<int(GRBmodel*, const char*, double)> GRBsetdblattr;
extern std::function<int(GRBmodel*, const char*, int, double)>
    GRBsetdblattrelement;
extern std::function<int(GRBenv*, const char*, double)> GRBsetdblparam;
extern std::function<int(GRBmodel*, const char*, int)> GRBsetintattr;
extern std::function<int(GRBenv*, const char*, int)> GRBsetintparam;
extern std::function<void(GRBmodel*)> GRBterminate;
extern std::function<int(GRBmodel*)> GRBupdatemodel;
extern std::function<void(int*, int*, int*)> GRBversion;
extern std::function<int(GRBmodel*, const char*)> GRBwrite;
extern std::function<int(void* cbdata, int where, int what, void* resultP)>
    GRBcbget;
extern std::function<int(void* cbdata, int cutlen, const int* cutind,
                         const double* cutval, char cutsense, double cutrhs)>
    GRBcbcut;
extern std::function<int(void* cbdata, int lazylen, const int* lazyind,
                         const double* lazyval, char lazysense, double lazyrhs)>
    GRBcblazy;
extern std::function<int(void* cbdata, const double* solution, double* objvalP)>
    GRBcbsolution;
extern std::function<int(GRBmodel* model, int numnz, int* cind, double* cval,
                         char sense, double rhs, const char* constrname)>
    GRBaddconstr;
extern std::function<int(GRBmodel* model, const char* name, int binvar,
                         int binval, int nvars, const int* vars,
                         const double* vals, char sense, double rhs)>
    GRBaddgenconstrIndicator;
extern std::function<int(GRBmodel* model, const char* attrname, int element,
                         int newvalue)>
    GRBsetintattrelement;
extern std::function<int(GRBmodel* model, int(STDCALL* cb)(CB_ARGS),
                         void* usrdata)>
    GRBsetcallbackfunc;
extern std::function<int(GRBenv* env, const char* paramname, const char* value)>
    GRBsetparam;
extern std::function<int(GRBmodel* model, int numsos, int nummembers,
                         int* types, int* beg, int* ind, double* weight)>
    GRBaddsos;

extern std::function<int(GRBmodel* model, int numlnz, int* lind, double* lval,
                         int numqnz, int* qrow, int* qcol, double* qval,
                         char sense, double rhs, const char* QCname)>
    GRBaddqconstr;
extern std::function<int(GRBmodel* model, const char* name, int resvar,
                         int nvars, const int* vars, double constant)>
    GRBaddgenconstrMax;
extern std::function<int(GRBmodel* model, const char* name, int resvar,
                         int nvars, const int* vars, double constant)>
    GRBaddgenconstrMin;
extern std::function<int(GRBmodel* model, const char* name, int resvar,
                         int argvar)>
    GRBaddgenconstrAbs;
extern std::function<int(GRBmodel* model, const char* name, int resvar,
                         int nvars, const int* vars)>
    GRBaddgenconstrAnd;
extern std::function<int(GRBmodel* model, const char* name, int resvar,
                         int nvars, const int* vars)>
    GRBaddgenconstrOr;
extern std::function<int(GRBmodel* model, int numqnz, int* qrow, int* qcol,
                         double* qval)>
    GRBaddqpterms;

#define GRB_VERSION_MAJOR 9
#define GRB_VERSION_MINOR 0
#define GRB_VERSION_TECHNICAL 2
#define GRB_ERROR_OUT_OF_MEMORY 10001
#define GRB_ERROR_NULL_ARGUMENT 10002
#define GRB_ERROR_INVALID_ARGUMENT 10003
#define GRB_ERROR_UNKNOWN_ATTRIBUTE 10004
#define GRB_ERROR_DATA_NOT_AVAILABLE 10005
#define GRB_ERROR_INDEX_OUT_OF_RANGE 10006
#define GRB_ERROR_UNKNOWN_PARAMETER 10007
#define GRB_ERROR_VALUE_OUT_OF_RANGE 10008
#define GRB_ERROR_NO_LICENSE 10009
#define GRB_ERROR_SIZE_LIMIT_EXCEEDED 10010
#define GRB_ERROR_CALLBACK 10011
#define GRB_ERROR_FILE_READ 10012
#define GRB_ERROR_FILE_WRITE 10013
#define GRB_ERROR_NUMERIC 10014
#define GRB_ERROR_IIS_NOT_INFEASIBLE 10015
#define GRB_ERROR_NOT_FOR_MIP 10016
#define GRB_ERROR_OPTIMIZATION_IN_PROGRESS 10017
#define GRB_ERROR_DUPLICATES 10018
#define GRB_ERROR_NODEFILE 10019
#define GRB_ERROR_Q_NOT_PSD 10020
#define GRB_ERROR_QCP_EQUALITY_CONSTRAINT 10021
#define GRB_ERROR_NETWORK 10022
#define GRB_ERROR_JOB_REJECTED 10023
#define GRB_ERROR_NOT_SUPPORTED 10024
#define GRB_ERROR_EXCEED_2B_NONZEROS 10025
#define GRB_ERROR_INVALID_PIECEWISE_OBJ 10026
#define GRB_ERROR_UPDATEMODE_CHANGE 10027
#define GRB_ERROR_CLOUD 10028
#define GRB_ERROR_MODEL_MODIFICATION 10029
#define GRB_ERROR_CSWORKER 10030
#define GRB_ERROR_TUNE_MODEL_TYPES 10031
#define GRB_ERROR_SECURITY 10032
#define GRB_LESS_EQUAL '<'
#define GRB_GREATER_EQUAL '>'
#define GRB_EQUAL '='
#define GRB_CONTINUOUS 'C'
#define GRB_BINARY 'B'
#define GRB_INTEGER 'I'
#define GRB_SEMICONT 'S'
#define GRB_SEMIINT 'N'
#define GRB_MINIMIZE 1
#define GRB_MAXIMIZE -1
#define GRB_SOS_TYPE1 1
#define GRB_SOS_TYPE2 2
#define GRB_INFINITY 1e100
#define GRB_UNDEFINED 1e101
#define GRB_MAXINT 2000000000
#define GRB_MAX_NAMELEN 255
#define GRB_MAX_STRLEN 512
#define GRB_MAX_TAGLEN 10240
#define GRB_MAX_CONCURRENT 64
#define GRB_INT_ATTR_NUMCONSTRS "NumConstrs" /* # of constraints */
#define GRB_INT_ATTR_NUMVARS "NumVars"       /* # of vars */
#define GRB_INT_ATTR_NUMSOS "NumSOS"         /* # of sos constraints */
#define GRB_INT_ATTR_NUMQCONSTRS                \
  "NumQConstrs" /* # of quadratic constraints \ \
                 */
#define GRB_INT_ATTR_NUMGENCONSTRS \
  "NumGenConstrs"                            /* # of general constraints */
#define GRB_INT_ATTR_NUMNZS "NumNZs"         /* # of nz in A */
#define GRB_DBL_ATTR_DNUMNZS "DNumNZs"       /* # of nz in A */
#define GRB_INT_ATTR_NUMQNZS "NumQNZs"       /* # of nz in Q */
#define GRB_INT_ATTR_NUMQCNZS "NumQCNZs"     /* # of nz in q constraints */
#define GRB_INT_ATTR_NUMINTVARS "NumIntVars" /* # of integer vars */
#define GRB_INT_ATTR_NUMBINVARS "NumBinVars" /* # of binary vars */
#define GRB_INT_ATTR_NUMPWLOBJVARS \
  "NumPWLObjVars"                            /* # of variables with PWL obj. */
#define GRB_STR_ATTR_MODELNAME "ModelName"   /* model name */
#define GRB_INT_ATTR_MODELSENSE "ModelSense" /* 1=min, -1=max */
#define GRB_DBL_ATTR_OBJCON "ObjCon"         /* Objective constant */
#define GRB_INT_ATTR_IS_MIP "IsMIP"          /* Is model a MIP? */
#define GRB_INT_ATTR_IS_QP "IsQP"            /* Model has quadratic obj? */
#define GRB_INT_ATTR_IS_QCP "IsQCP"          /* Model has quadratic constr? */
#define GRB_INT_ATTR_IS_MULTIOBJ \
  "IsMultiObj"                       /* Model has multiple objectives? */
#define GRB_STR_ATTR_SERVER "Server" /* Name of Compute Server */
#define GRB_STR_ATTR_JOBID "JobID"   /* Compute Server job ID */
#define GRB_INT_ATTR_LICENSE_EXPIRATION \
  "LicenseExpiration" /* License expiration date */
#define GRB_INT_ATTR_NUMTAGGED \
  "NumTagged" /* number of tagged elements in model */
#define GRB_INT_ATTR_BATCHERRORCODE "BatchErrorCode"
#define GRB_STR_ATTR_BATCHERRORMESSAGE "BatchErrorMessage"
#define GRB_STR_ATTR_BATCHID "BatchID"
#define GRB_INT_ATTR_BATCHSTATUS "BatchStatus"
#define GRB_DBL_ATTR_LB "LB"         /* Lower bound */
#define GRB_DBL_ATTR_UB "UB"         /* Upper bound */
#define GRB_DBL_ATTR_OBJ "Obj"       /* Objective coeff */
#define GRB_CHAR_ATTR_VTYPE "VType"  /* Integrality type */
#define GRB_DBL_ATTR_START "Start"   /* MIP start value */
#define GRB_DBL_ATTR_PSTART "PStart" /* LP primal solution warm start */
#define GRB_INT_ATTR_BRANCHPRIORITY "BranchPriority" /* MIP branch priority */
#define GRB_STR_ATTR_VARNAME "VarName"               /* Variable name */
#define GRB_INT_ATTR_PWLOBJCVX "PWLObjCvx" /* Convexity of variable PWL obj */
#define GRB_DBL_ATTR_VARHINTVAL "VarHintVal"
#define GRB_INT_ATTR_VARHINTPRI "VarHintPri"
#define GRB_INT_ATTR_PARTITION "Partition"
#define GRB_STR_ATTR_VTAG "VTag"             /* variable tags */
#define GRB_STR_ATTR_CTAG "CTag"             /* linear constraint tags */
#define GRB_DBL_ATTR_RHS "RHS"               /* RHS */
#define GRB_DBL_ATTR_DSTART "DStart"         /* LP dual solution warm start */
#define GRB_CHAR_ATTR_SENSE "Sense"          /* Sense ('<', '>', or '=') */
#define GRB_STR_ATTR_CONSTRNAME "ConstrName" /* Constraint name */
#define GRB_INT_ATTR_LAZY "Lazy"             /* Lazy constraint? */
#define GRB_STR_ATTR_QCTAG "QCTag"           /* quadratic constraint tags */
#define GRB_DBL_ATTR_QCRHS "QCRHS"           /* QC RHS */
#define GRB_CHAR_ATTR_QCSENSE "QCSense"      /* QC sense ('<', '>', or '=') */
#define GRB_STR_ATTR_QCNAME "QCName"         /* QC name */
#define GRB_INT_ATTR_GENCONSTRTYPE \
  "GenConstrType" /* Type of general constraint */
#define GRB_STR_ATTR_GENCONSTRNAME \
  "GenConstrName" /* Name of general constraint */
#define GRB_INT_ATTR_FUNCPIECES                   \
  "FuncPieces" /* An option for PWL translation \ \
                */
#define GRB_DBL_ATTR_FUNCPIECEERROR \
  "FuncPieceError" /* An option for PWL translation */
#define GRB_DBL_ATTR_FUNCPIECELENGTH \
  "FuncPieceLength" /* An option for PWL translation */
#define GRB_DBL_ATTR_FUNCPIECERATIO \
  "FuncPieceRatio"                        /* An option for PWL translation */
#define GRB_DBL_ATTR_MAX_COEFF "MaxCoeff" /* Max (abs) nz coeff in A */
#define GRB_DBL_ATTR_MIN_COEFF "MinCoeff" /* Min (abs) nz coeff in A */
#define GRB_DBL_ATTR_MAX_BOUND "MaxBound" /* Max (abs) finite var bd */
#define GRB_DBL_ATTR_MIN_BOUND "MinBound" /* Min (abs) var bd */
#define GRB_DBL_ATTR_MAX_OBJ_COEFF "MaxObjCoeff" /* Max (abs) obj coeff */
#define GRB_DBL_ATTR_MIN_OBJ_COEFF "MinObjCoeff" /* Min (abs) obj coeff */
#define GRB_DBL_ATTR_MAX_RHS "MaxRHS"            /* Max (abs) rhs coeff */
#define GRB_DBL_ATTR_MIN_RHS "MinRHS"            /* Min (abs) rhs coeff */
#define GRB_DBL_ATTR_MAX_QCCOEFF "MaxQCCoeff"    /* Max (abs) nz coeff in Q */
#define GRB_DBL_ATTR_MIN_QCCOEFF "MinQCCoeff"    /* Min (abs) nz coeff in Q */
#define GRB_DBL_ATTR_MAX_QOBJ_COEFF \
  "MaxQObjCoeff" /* Max (abs) obj coeff of quadratic part */
#define GRB_DBL_ATTR_MIN_QOBJ_COEFF \
  "MinQObjCoeff" /* Min (abs) obj coeff of quadratic part */
#define GRB_DBL_ATTR_MAX_QCLCOEFF \
  "MaxQCLCoeff" /* Max (abs) nz coeff in linear part of Q */
#define GRB_DBL_ATTR_MIN_QCLCOEFF \
  "MinQCLCoeff" /* Min (abs) nz coeff in linear part of Q */
#define GRB_DBL_ATTR_MAX_QCRHS "MaxQCRHS"  /* Max (abs) rhs of Q */
#define GRB_DBL_ATTR_MIN_QCRHS "MinQCRHS"  /* Min (abs) rhs of Q */
#define GRB_DBL_ATTR_RUNTIME "Runtime"     /* Run time for optimization */
#define GRB_INT_ATTR_STATUS "Status"       /* Optimization status */
#define GRB_DBL_ATTR_OBJVAL "ObjVal"       /* Solution objective */
#define GRB_DBL_ATTR_OBJBOUND "ObjBound"   /* Best bound on solution */
#define GRB_DBL_ATTR_OBJBOUNDC "ObjBoundC" /* Continuous bound */
#define GRB_DBL_ATTR_POOLOBJBOUND \
  "PoolObjBound" /* Best bound on pool solution */
#define GRB_DBL_ATTR_POOLOBJVAL \
  "PoolObjVal"                       /* Solution objective for solutionnumber */
#define GRB_DBL_ATTR_MIPGAP "MIPGap" /* MIP optimality gap */
#define GRB_INT_ATTR_SOLCOUNT "SolCount"   /* # of solutions found */
#define GRB_DBL_ATTR_ITERCOUNT "IterCount" /* Iters performed (simplex) */
#define GRB_INT_ATTR_BARITERCOUNT                                         \
  "BarIterCount"                           /* Iters performed (barrier) \ \
                                            */
#define GRB_DBL_ATTR_NODECOUNT "NodeCount" /* Nodes explored (B&C) */
#define GRB_DBL_ATTR_OPENNODECOUNT                                         \
  "OpenNodeCount"                              /* Unexplored nodes (B&C) \ \
                                                */
#define GRB_INT_ATTR_HASDUALNORM "HasDualNorm" /* 0, no basis, */
#define GRB_DBL_ATTR_X "X"                     /* Solution value */
#define GRB_DBL_ATTR_XN "Xn"                   /* Alternate MIP solution */
#define GRB_DBL_ATTR_BARX "BarX"               /* Best barrier iterate */
#define GRB_DBL_ATTR_RC "RC"                   /* Reduced costs */
#define GRB_DBL_ATTR_VDUALNORM "VDualNorm"     /* Dual norm square */
#define GRB_INT_ATTR_VBASIS "VBasis"           /* Variable basis status */
#define GRB_DBL_ATTR_PI "Pi"                   /* Dual value */
#define GRB_DBL_ATTR_QCPI "QCPi"               /* Dual value for QC */
#define GRB_DBL_ATTR_SLACK "Slack"             /* Constraint slack */
#define GRB_DBL_ATTR_QCSLACK "QCSlack"         /* QC Constraint slack */
#define GRB_DBL_ATTR_CDUALNORM "CDualNorm"     /* Dual norm square */
#define GRB_INT_ATTR_CBASIS "CBasis"           /* Constraint basis status */
#define GRB_DBL_ATTR_BOUND_VIO "BoundVio"
#define GRB_DBL_ATTR_BOUND_SVIO "BoundSVio"
#define GRB_INT_ATTR_BOUND_VIO_INDEX "BoundVioIndex"
#define GRB_INT_ATTR_BOUND_SVIO_INDEX "BoundSVioIndex"
#define GRB_DBL_ATTR_BOUND_VIO_SUM "BoundVioSum"
#define GRB_DBL_ATTR_BOUND_SVIO_SUM "BoundSVioSum"
#define GRB_DBL_ATTR_CONSTR_VIO "ConstrVio"
#define GRB_DBL_ATTR_CONSTR_SVIO "ConstrSVio"
#define GRB_INT_ATTR_CONSTR_VIO_INDEX "ConstrVioIndex"
#define GRB_INT_ATTR_CONSTR_SVIO_INDEX "ConstrSVioIndex"
#define GRB_DBL_ATTR_CONSTR_VIO_SUM "ConstrVioSum"
#define GRB_DBL_ATTR_CONSTR_SVIO_SUM "ConstrSVioSum"
#define GRB_DBL_ATTR_CONSTR_RESIDUAL "ConstrResidual"
#define GRB_DBL_ATTR_CONSTR_SRESIDUAL "ConstrSResidual"
#define GRB_INT_ATTR_CONSTR_RESIDUAL_INDEX "ConstrResidualIndex"
#define GRB_INT_ATTR_CONSTR_SRESIDUAL_INDEX "ConstrSResidualIndex"
#define GRB_DBL_ATTR_CONSTR_RESIDUAL_SUM "ConstrResidualSum"
#define GRB_DBL_ATTR_CONSTR_SRESIDUAL_SUM "ConstrSResidualSum"
#define GRB_DBL_ATTR_DUAL_VIO "DualVio"
#define GRB_DBL_ATTR_DUAL_SVIO "DualSVio"
#define GRB_INT_ATTR_DUAL_VIO_INDEX "DualVioIndex"
#define GRB_INT_ATTR_DUAL_SVIO_INDEX "DualSVioIndex"
#define GRB_DBL_ATTR_DUAL_VIO_SUM "DualVioSum"
#define GRB_DBL_ATTR_DUAL_SVIO_SUM "DualSVioSum"
#define GRB_DBL_ATTR_DUAL_RESIDUAL "DualResidual"
#define GRB_DBL_ATTR_DUAL_SRESIDUAL "DualSResidual"
#define GRB_INT_ATTR_DUAL_RESIDUAL_INDEX "DualResidualIndex"
#define GRB_INT_ATTR_DUAL_SRESIDUAL_INDEX "DualSResidualIndex"
#define GRB_DBL_ATTR_DUAL_RESIDUAL_SUM "DualResidualSum"
#define GRB_DBL_ATTR_DUAL_SRESIDUAL_SUM "DualSResidualSum"
#define GRB_DBL_ATTR_INT_VIO "IntVio"
#define GRB_INT_ATTR_INT_VIO_INDEX "IntVioIndex"
#define GRB_DBL_ATTR_INT_VIO_SUM "IntVioSum"
#define GRB_DBL_ATTR_COMPL_VIO "ComplVio"
#define GRB_INT_ATTR_COMPL_VIO_INDEX "ComplVioIndex"
#define GRB_DBL_ATTR_COMPL_VIO_SUM "ComplVioSum"
#define GRB_DBL_ATTR_KAPPA "Kappa"
#define GRB_DBL_ATTR_KAPPA_EXACT "KappaExact"
#define GRB_DBL_ATTR_N2KAPPA "N2Kappa"
#define GRB_DBL_ATTR_SA_OBJLOW "SAObjLow"
#define GRB_DBL_ATTR_SA_OBJUP "SAObjUp"
#define GRB_DBL_ATTR_SA_LBLOW "SALBLow"
#define GRB_DBL_ATTR_SA_LBUP "SALBUp"
#define GRB_DBL_ATTR_SA_UBLOW "SAUBLow"
#define GRB_DBL_ATTR_SA_UBUP "SAUBUp"
#define GRB_DBL_ATTR_SA_RHSLOW "SARHSLow"
#define GRB_DBL_ATTR_SA_RHSUP "SARHSUp"
#define GRB_INT_ATTR_IIS_MINIMAL "IISMinimal" /* Boolean: Is IIS Minimal? */
#define GRB_INT_ATTR_IIS_LB "IISLB"           /* Boolean: Is var LB in IIS? */
#define GRB_INT_ATTR_IIS_UB "IISUB"           /* Boolean: Is var UB in IIS? */
#define GRB_INT_ATTR_IIS_CONSTR "IISConstr"   /* Boolean: Is constr in IIS? */
#define GRB_INT_ATTR_IIS_SOS "IISSOS"         /* Boolean: Is SOS in IIS? */
#define GRB_INT_ATTR_IIS_QCONSTR                \
  "IISQConstr" /* Boolean: Is QConstr in IIS? \ \
                */
#define GRB_INT_ATTR_IIS_GENCONSTR \
  "IISGenConstr" /* Boolean: Is general constr in IIS? */
#define GRB_INT_ATTR_TUNE_RESULTCOUNT "TuneResultCount"
#define GRB_DBL_ATTR_FARKASDUAL "FarkasDual"
#define GRB_DBL_ATTR_FARKASPROOF "FarkasProof"
#define GRB_DBL_ATTR_UNBDRAY "UnbdRay"
#define GRB_INT_ATTR_INFEASVAR "InfeasVar"
#define GRB_INT_ATTR_UNBDVAR "UnbdVar"
#define GRB_INT_ATTR_VARPRESTAT "VarPreStat"
#define GRB_DBL_ATTR_PREFIXVAL "PreFixVal"
#define GRB_DBL_ATTR_OBJN "ObjN" /* ith objective */
#define GRB_DBL_ATTR_OBJNVAL \
  "ObjNVal" /* Solution objective for Multi-objectives */
#define GRB_DBL_ATTR_OBJNCON "ObjNCon"           /* constant term */
#define GRB_DBL_ATTR_OBJNWEIGHT "ObjNWeight"     /* weight */
#define GRB_INT_ATTR_OBJNPRIORITY "ObjNPriority" /* priority */
#define GRB_DBL_ATTR_OBJNRELTOL "ObjNRelTol"     /* relative tolerance */
#define GRB_DBL_ATTR_OBJNABSTOL "ObjNAbsTol"     /* absolute tolerance */
#define GRB_STR_ATTR_OBJNNAME "ObjNName"         /* name */
#define GRB_DBL_ATTR_SCENNLB "ScenNLB"           /* lower bound in scenario i */
#define GRB_DBL_ATTR_SCENNUB "ScenNUB"           /* upper bound in scenario i */
#define GRB_DBL_ATTR_SCENNOBJ "ScenNObj"         /* objective in scenario i */
#define GRB_DBL_ATTR_SCENNRHS "ScenNRHS"   /* right hand side in scenario i */
#define GRB_STR_ATTR_SCENNNAME "ScenNName" /* name of scenario i */
#define GRB_DBL_ATTR_SCENNX "ScenNX"       /* solution value in scenario i */
#define GRB_DBL_ATTR_SCENNOBJBOUND \
  "ScenNObjBound" /* objective bound for scenario i */
#define GRB_DBL_ATTR_SCENNOBJVAL \
  "ScenNObjVal"                      /* objective value for scenario i */
#define GRB_INT_ATTR_NUMOBJ "NumObj" /* number of objectives */
#define GRB_INT_ATTR_NUMSCENARIOS "NumScenarios" /* number of scenarios */
#define GRB_INT_ATTR_NUMSTART "NumStart"         /* number of MIP starts */
#define GRB_DBL_ATTR_Xn "Xn"
#define GRB_GENCONSTR_MAX 0
#define GRB_GENCONSTR_MIN 1
#define GRB_GENCONSTR_ABS 2
#define GRB_GENCONSTR_AND 3
#define GRB_GENCONSTR_OR 4
#define GRB_GENCONSTR_INDICATOR 5
#define GRB_GENCONSTR_PWL 6
#define GRB_GENCONSTR_POLY 7
#define GRB_GENCONSTR_EXP 8
#define GRB_GENCONSTR_EXPA 9
#define GRB_GENCONSTR_LOG 10
#define GRB_GENCONSTR_LOGA 11
#define GRB_GENCONSTR_POW 12
#define GRB_GENCONSTR_SIN 13
#define GRB_GENCONSTR_COS 14
#define GRB_GENCONSTR_TAN 15
#define GRB_CB_POLLING 0
#define GRB_CB_PRESOLVE 1
#define GRB_CB_SIMPLEX 2
#define GRB_CB_MIP 3
#define GRB_CB_MIPSOL 4
#define GRB_CB_MIPNODE 5
#define GRB_CB_MESSAGE 6
#define GRB_CB_BARRIER 7
#define GRB_CB_MULTIOBJ 8
#define GRB_CB_PRE_COLDEL 1000
#define GRB_CB_PRE_ROWDEL 1001
#define GRB_CB_PRE_SENCHG 1002
#define GRB_CB_PRE_BNDCHG 1003
#define GRB_CB_PRE_COECHG 1004
#define GRB_CB_SPX_ITRCNT 2000
#define GRB_CB_SPX_OBJVAL 2001
#define GRB_CB_SPX_PRIMINF 2002
#define GRB_CB_SPX_DUALINF 2003
#define GRB_CB_SPX_ISPERT 2004
#define GRB_CB_MIP_OBJBST 3000
#define GRB_CB_MIP_OBJBND 3001
#define GRB_CB_MIP_NODCNT 3002
#define GRB_CB_MIP_SOLCNT 3003
#define GRB_CB_MIP_CUTCNT 3004
#define GRB_CB_MIP_NODLFT 3005
#define GRB_CB_MIP_ITRCNT 3006
#define GRB_CB_MIP_OBJBNDC 3007
#define GRB_CB_MIPSOL_SOL 4001
#define GRB_CB_MIPSOL_OBJ 4002
#define GRB_CB_MIPSOL_OBJBST 4003
#define GRB_CB_MIPSOL_OBJBND 4004
#define GRB_CB_MIPSOL_NODCNT 4005
#define GRB_CB_MIPSOL_SOLCNT 4006
#define GRB_CB_MIPSOL_OBJBNDC 4007
#define GRB_CB_MIPNODE_STATUS 5001
#define GRB_CB_MIPNODE_REL 5002
#define GRB_CB_MIPNODE_OBJBST 5003
#define GRB_CB_MIPNODE_OBJBND 5004
#define GRB_CB_MIPNODE_NODCNT 5005
#define GRB_CB_MIPNODE_SOLCNT 5006
#define GRB_CB_MIPNODE_BRVAR 5007
#define GRB_CB_MIPNODE_OBJBNDC 5008
#define GRB_CB_MSG_STRING 6001
#define GRB_CB_RUNTIME 6002
#define GRB_CB_BARRIER_ITRCNT 7001
#define GRB_CB_BARRIER_PRIMOBJ 7002
#define GRB_CB_BARRIER_DUALOBJ 7003
#define GRB_CB_BARRIER_PRIMINF 7004
#define GRB_CB_BARRIER_DUALINF 7005
#define GRB_CB_BARRIER_COMPL 7006
#define GRB_CB_MULTIOBJ_OBJCNT 8001
#define GRB_CB_MULTIOBJ_SOLCNT 8002
#define GRB_CB_MULTIOBJ_SOL 8003
#define GRB_FEASRELAX_LINEAR 0
#define GRB_FEASRELAX_QUADRATIC 1
#define GRB_FEASRELAX_CARDINALITY 2
#define GRB_LOADED 1
#define GRB_OPTIMAL 2
#define GRB_INFEASIBLE 3
#define GRB_INF_OR_UNBD 4
#define GRB_UNBOUNDED 5
#define GRB_CUTOFF 6
#define GRB_ITERATION_LIMIT 7
#define GRB_NODE_LIMIT 8
#define GRB_TIME_LIMIT 9
#define GRB_SOLUTION_LIMIT 10
#define GRB_INTERRUPTED 11
#define GRB_NUMERIC 12
#define GRB_SUBOPTIMAL 13
#define GRB_INPROGRESS 14
#define GRB_USER_OBJ_LIMIT 15
#define GRB_BASIC 0
#define GRB_NONBASIC_LOWER -1
#define GRB_NONBASIC_UPPER -2
#define GRB_SUPERBASIC -3
#define GRB_INT_PAR_BARITERLIMIT "BarIterLimit"
#define GRB_DBL_PAR_CUTOFF "Cutoff"
#define GRB_DBL_PAR_ITERATIONLIMIT "IterationLimit"
#define GRB_DBL_PAR_NODELIMIT "NodeLimit"
#define GRB_INT_PAR_SOLUTIONLIMIT "SolutionLimit"
#define GRB_DBL_PAR_TIMELIMIT "TimeLimit"
#define GRB_DBL_PAR_BESTOBJSTOP "BestObjStop"
#define GRB_DBL_PAR_BESTBDSTOP "BestBdStop"
#define GRB_DBL_PAR_FEASIBILITYTOL "FeasibilityTol"
#define GRB_DBL_PAR_INTFEASTOL "IntFeasTol"
#define GRB_DBL_PAR_MARKOWITZTOL "MarkowitzTol"
#define GRB_DBL_PAR_MIPGAP "MIPGap"
#define GRB_DBL_PAR_MIPGAPABS "MIPGapAbs"
#define GRB_DBL_PAR_OPTIMALITYTOL "OptimalityTol"
#define GRB_DBL_PAR_PSDTOL "PSDTol"
#define GRB_INT_PAR_METHOD "Method"
#define GRB_DBL_PAR_PERTURBVALUE "PerturbValue"
#define GRB_DBL_PAR_OBJSCALE "ObjScale"
#define GRB_INT_PAR_SCALEFLAG "ScaleFlag"
#define GRB_INT_PAR_SIMPLEXPRICING "SimplexPricing"
#define GRB_INT_PAR_QUAD "Quad"
#define GRB_INT_PAR_NORMADJUST "NormAdjust"
#define GRB_INT_PAR_SIFTING "Sifting"
#define GRB_INT_PAR_SIFTMETHOD "SiftMethod"
#define GRB_DBL_PAR_BARCONVTOL "BarConvTol"
#define GRB_INT_PAR_BARCORRECTORS "BarCorrectors"
#define GRB_INT_PAR_BARHOMOGENEOUS "BarHomogeneous"
#define GRB_INT_PAR_BARORDER "BarOrder"
#define GRB_DBL_PAR_BARQCPCONVTOL "BarQCPConvTol"
#define GRB_INT_PAR_CROSSOVER "Crossover"
#define GRB_INT_PAR_CROSSOVERBASIS "CrossoverBasis"
#define GRB_INT_PAR_BRANCHDIR "BranchDir"
#define GRB_INT_PAR_DEGENMOVES "DegenMoves"
#define GRB_INT_PAR_DISCONNECTED "Disconnected"
#define GRB_DBL_PAR_HEURISTICS "Heuristics"
#define GRB_DBL_PAR_IMPROVESTARTGAP "ImproveStartGap"
#define GRB_DBL_PAR_IMPROVESTARTTIME "ImproveStartTime"
#define GRB_DBL_PAR_IMPROVESTARTNODES "ImproveStartNodes"
#define GRB_INT_PAR_MINRELNODES "MinRelNodes"
#define GRB_INT_PAR_MIPFOCUS "MIPFocus"
#define GRB_STR_PAR_NODEFILEDIR "NodefileDir"
#define GRB_DBL_PAR_NODEFILESTART "NodefileStart"
#define GRB_INT_PAR_NODEMETHOD "NodeMethod"
#define GRB_INT_PAR_NORELHEURISTIC "NoRelHeuristic"
#define GRB_INT_PAR_PUMPPASSES "PumpPasses"
#define GRB_INT_PAR_RINS "RINS"
#define GRB_STR_PAR_SOLFILES "SolFiles"
#define GRB_INT_PAR_STARTNODELIMIT "StartNodeLimit"
#define GRB_INT_PAR_SUBMIPNODES "SubMIPNodes"
#define GRB_INT_PAR_SYMMETRY "Symmetry"
#define GRB_INT_PAR_VARBRANCH "VarBranch"
#define GRB_INT_PAR_SOLUTIONNUMBER "SolutionNumber"
#define GRB_INT_PAR_ZEROOBJNODES "ZeroObjNodes"
#define GRB_INT_PAR_CUTS "Cuts"
#define GRB_INT_PAR_CLIQUECUTS "CliqueCuts"
#define GRB_INT_PAR_COVERCUTS "CoverCuts"
#define GRB_INT_PAR_FLOWCOVERCUTS "FlowCoverCuts"
#define GRB_INT_PAR_FLOWPATHCUTS "FlowPathCuts"
#define GRB_INT_PAR_GUBCOVERCUTS "GUBCoverCuts"
#define GRB_INT_PAR_IMPLIEDCUTS "ImpliedCuts"
#define GRB_INT_PAR_PROJIMPLIEDCUTS "ProjImpliedCuts"
#define GRB_INT_PAR_MIPSEPCUTS "MIPSepCuts"
#define GRB_INT_PAR_MIRCUTS "MIRCuts"
#define GRB_INT_PAR_STRONGCGCUTS "StrongCGCuts"
#define GRB_INT_PAR_MODKCUTS "ModKCuts"
#define GRB_INT_PAR_ZEROHALFCUTS "ZeroHalfCuts"
#define GRB_INT_PAR_NETWORKCUTS "NetworkCuts"
#define GRB_INT_PAR_SUBMIPCUTS "SubMIPCuts"
#define GRB_INT_PAR_INFPROOFCUTS "InfProofCuts"
#define GRB_INT_PAR_RLTCUTS "RLTCuts"
#define GRB_INT_PAR_RELAXLIFTCUTS "RelaxLiftCuts"
#define GRB_INT_PAR_BQPCUTS "BQPCuts"
#define GRB_INT_PAR_CUTAGGPASSES "CutAggPasses"
#define GRB_INT_PAR_CUTPASSES "CutPasses"
#define GRB_INT_PAR_GOMORYPASSES "GomoryPasses"
#define GRB_STR_PAR_WORKERPOOL "WorkerPool"
#define GRB_STR_PAR_WORKERPASSWORD "WorkerPassword"
#define GRB_STR_PAR_COMPUTESERVER "ComputeServer"
#define GRB_STR_PAR_TOKENSERVER "TokenServer"
#define GRB_STR_PAR_SERVERPASSWORD "ServerPassword"
#define GRB_INT_PAR_SERVERTIMEOUT "ServerTimeout"
#define GRB_STR_PAR_CSROUTER "CSRouter"
#define GRB_STR_PAR_CSGROUP "CSGroup"
#define GRB_DBL_PAR_CSQUEUETIMEOUT "CSQueueTimeout"
#define GRB_INT_PAR_CSPRIORITY "CSPriority"
#define GRB_INT_PAR_CSIDLETIMEOUT "CSIdleTimeout"
#define GRB_INT_PAR_CSTLSINSECURE "CSTLSInsecure"
#define GRB_INT_PAR_TSPORT "TSPort"
#define GRB_STR_PAR_CLOUDACCESSID "CloudAccessID"
#define GRB_STR_PAR_CLOUDSECRETKEY "CloudSecretKey"
#define GRB_STR_PAR_CLOUDPOOL "CloudPool"
#define GRB_STR_PAR_CLOUDHOST "CloudHost"
#define GRB_STR_PAR_CSMANAGER "CSManager"
#define GRB_STR_PAR_CSAUTHTOKEN "CSAuthToken"
#define GRB_STR_PAR_CSAPIACCESSID "CSAPIAccessID"
#define GRB_STR_PAR_CSAPISECRET "CSAPISecret"
#define GRB_INT_PAR_CSBATCHMODE "CSBatchMode"
#define GRB_STR_PAR_USERNAME "Username"
#define GRB_STR_PAR_CSAPPNAME "CSAppName"
#define GRB_INT_PAR_CSCLIENTLOG "CSClientLog"
#define GRB_INT_PAR_AGGREGATE "Aggregate"
#define GRB_INT_PAR_AGGFILL "AggFill"
#define GRB_INT_PAR_CONCURRENTMIP "ConcurrentMIP"
#define GRB_INT_PAR_CONCURRENTJOBS "ConcurrentJobs"
#define GRB_INT_PAR_DISPLAYINTERVAL "DisplayInterval"
#define GRB_INT_PAR_DISTRIBUTEDMIPJOBS "DistributedMIPJobs"
#define GRB_INT_PAR_DUALREDUCTIONS "DualReductions"
#define GRB_DBL_PAR_FEASRELAXBIGM "FeasRelaxBigM"
#define GRB_INT_PAR_IISMETHOD "IISMethod"
#define GRB_INT_PAR_INFUNBDINFO "InfUnbdInfo"
#define GRB_INT_PAR_JSONSOLDETAIL "JSONSolDetail"
#define GRB_INT_PAR_LAZYCONSTRAINTS "LazyConstraints"
#define GRB_STR_PAR_LOGFILE "LogFile"
#define GRB_INT_PAR_LOGTOCONSOLE "LogToConsole"
#define GRB_INT_PAR_MIQCPMETHOD "MIQCPMethod"
#define GRB_INT_PAR_NONCONVEX "NonConvex"
#define GRB_INT_PAR_NUMERICFOCUS "NumericFocus"
#define GRB_INT_PAR_OUTPUTFLAG "OutputFlag"
#define GRB_INT_PAR_PRECRUSH "PreCrush"
#define GRB_INT_PAR_PREDEPROW "PreDepRow"
#define GRB_INT_PAR_PREDUAL "PreDual"
#define GRB_INT_PAR_PREPASSES "PrePasses"
#define GRB_INT_PAR_PREQLINEARIZE "PreQLinearize"
#define GRB_INT_PAR_PRESOLVE "Presolve"
#define GRB_DBL_PAR_PRESOS1BIGM "PreSOS1BigM"
#define GRB_DBL_PAR_PRESOS2BIGM "PreSOS2BigM"
#define GRB_INT_PAR_PRESPARSIFY "PreSparsify"
#define GRB_INT_PAR_PREMIQCPFORM "PreMIQCPForm"
#define GRB_INT_PAR_QCPDUAL "QCPDual"
#define GRB_INT_PAR_RECORD "Record"
#define GRB_STR_PAR_RESULTFILE "ResultFile"
#define GRB_INT_PAR_SEED "Seed"
#define GRB_INT_PAR_THREADS "Threads"
#define GRB_DBL_PAR_TUNETIMELIMIT "TuneTimeLimit"
#define GRB_INT_PAR_TUNERESULTS "TuneResults"
#define GRB_INT_PAR_TUNECRITERION "TuneCriterion"
#define GRB_INT_PAR_TUNETRIALS "TuneTrials"
#define GRB_INT_PAR_TUNEOUTPUT "TuneOutput"
#define GRB_INT_PAR_TUNEJOBS "TuneJobs"
#define GRB_INT_PAR_UPDATEMODE "UpdateMode"
#define GRB_INT_PAR_OBJNUMBER "ObjNumber"
#define GRB_INT_PAR_MULTIOBJMETHOD "MultiObjMethod"
#define GRB_INT_PAR_MULTIOBJPRE "MultiObjPre"
#define GRB_INT_PAR_SCENARIONUMBER "ScenarioNumber"
#define GRB_INT_PAR_POOLSOLUTIONS "PoolSolutions"
#define GRB_DBL_PAR_POOLGAP "PoolGap"
#define GRB_INT_PAR_POOLSEARCHMODE "PoolSearchMode"
#define GRB_INT_PAR_IGNORENAMES "IgnoreNames"
#define GRB_INT_PAR_STARTNUMBER "StartNumber"
#define GRB_INT_PAR_PARTITIONPLACE "PartitionPlace"
#define GRB_INT_PAR_FUNCPIECES "FuncPieces"
#define GRB_DBL_PAR_FUNCPIECELENGTH "FuncPieceLength"
#define GRB_DBL_PAR_FUNCPIECEERROR "FuncPieceError"
#define GRB_DBL_PAR_FUNCPIECERATIO "FuncPieceRatio"
#define GRB_DBL_PAR_FUNCMAXVAL "FuncMaxVal"
#define GRB_STR_PAR_DUMMY "Dummy"
#define GRB_STR_PAR_JOBID "JobID"
#define GRB_CUTS_AUTO -1
#define GRB_CUTS_OFF 0
#define GRB_CUTS_CONSERVATIVE 1
#define GRB_CUTS_AGGRESSIVE 2
#define GRB_CUTS_VERYAGGRESSIVE 3
#define GRB_PRESOLVE_AUTO -1
#define GRB_PRESOLVE_OFF 0
#define GRB_PRESOLVE_CONSERVATIVE 1
#define GRB_PRESOLVE_AGGRESSIVE 2
#define GRB_METHOD_AUTO -1
#define GRB_METHOD_PRIMAL 0
#define GRB_METHOD_DUAL 1
#define GRB_METHOD_BARRIER 2
#define GRB_METHOD_CONCURRENT 3
#define GRB_METHOD_DETERMINISTIC_CONCURRENT 4
#define GRB_METHOD_DETERMINISTIC_CONCURRENT_SIMPLEX 5
#define GRB_BARHOMOGENEOUS_AUTO -1
#define GRB_BARHOMOGENEOUS_OFF 0
#define GRB_BARHOMOGENEOUS_ON 1
#define GRB_MIPFOCUS_BALANCED 0
#define GRB_MIPFOCUS_FEASIBILITY 1
#define GRB_MIPFOCUS_OPTIMALITY 2
#define GRB_MIPFOCUS_BESTBOUND 3
#define GRB_BARORDER_AUTOMATIC -1
#define GRB_BARORDER_AMD 0
#define GRB_BARORDER_NESTEDDISSECTION 1
#define GRB_SIMPLEXPRICING_AUTO -1
#define GRB_SIMPLEXPRICING_PARTIAL 0
#define GRB_SIMPLEXPRICING_STEEPEST_EDGE 1
#define GRB_SIMPLEXPRICING_DEVEX 2
#define GRB_SIMPLEXPRICING_STEEPEST_QUICK 3
#define GRB_VARBRANCH_AUTO -1
#define GRB_VARBRANCH_PSEUDO_REDUCED 0
#define GRB_VARBRANCH_PSEUDO_SHADOW 1
#define GRB_VARBRANCH_MAX_INFEAS 2
#define GRB_VARBRANCH_STRONG 3
#define GRB_PARTITION_EARLY 16
#define GRB_PARTITION_ROOTSTART 8
#define GRB_PARTITION_ROOTEND 4
#define GRB_PARTITION_NODES 2
#define GRB_PARTITION_CLEANUP 1
#define GRB_BATCH_STATUS_UNKNOWN 0
#define GRB_BATCH_CREATED 1
#define GRB_BATCH_SUBMITTED 2
#define GRB_BATCH_ABORTED 3
#define GRB_BATCH_FAILED 4
#define GRB_BATCH_COMPLETED 5
}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_GUROBI_ENVIRONMENT_H_
