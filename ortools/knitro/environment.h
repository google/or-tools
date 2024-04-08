// INSERT HEADER

#ifndef OR_TOOLS_KNITRO_ENVIRONMENT_H_
#define OR_TOOLS_KNITRO_ENVIRONMENT_H_

#include <functional>
#include <string>

#include "absl/status/status.h"

// ******************************
// * typedef
// ******************************
extern "C" {
typedef int KNINT;
#ifdef _WIN64
typedef __int64 KNLONG;
#elif _WIN32
typedef int KNLONG;
#else
typedef long long KNLONG;
#endif
typedef KNINT KNBOOL;
typedef struct KN_context KN_context, *KN_context_ptr;
typedef struct LM_context LM_context, *LM_context_ptr;
typedef struct KN_eval_request {
  int type;
  int threadID;
  const double* x;
  const double* lambda;
  const double* sigma;
  const double* vec;
} KN_eval_request, *KN_eval_request_ptr;
typedef struct KN_eval_result {
  double* obj;
  double* c;
  double* objGrad;
  double* jac;
  double* hess;
  double* hessVec;
  double* rsd;
  double* rsdJac;
} KN_eval_result, *KN_eval_result_ptr;
typedef struct CB_context CB_context, *CB_context_ptr;
typedef int KN_eval_callback(KN_context_ptr kc, CB_context_ptr cb,
                             KN_eval_request_ptr const evalRequest,
                             KN_eval_result_ptr const evalResult,
                             void* const userParams);
typedef int KN_user_callback(KN_context_ptr kc, const double* const x,
                             const double* const lambda,
                             void* const userParams);
typedef int KN_ms_initpt_callback(KN_context_ptr kc, const KNINT nSolveNumber,
                                  double* const x, double* const lambda,
                                  void* const userParams);
typedef int KN_puts(const char* const str, void* const userParams);
typedef struct KN_linsolver_request {
  int phase;
  int linsysID;
  int threadID;
  KNINT n;
  KNINT n11;
  const double* rhs;
  const double* values;
  const KNINT* indexRows;
  const KNLONG* ptrCols;
} KN_linsolver_request, *KN_linsolver_request_ptr;
typedef struct KN_linsolver_result {
  double* solution;
  KNINT negeig;
  KNINT poseig;
  KNINT rank;
} KN_linsolver_result, *KN_linsolver_result_ptr;
typedef int KN_linsolver_callback(
    KN_context_ptr kc, KN_linsolver_request_ptr const linsolverRequest,
    KN_linsolver_result_ptr const linsolverResult, void* const userParams);
}

namespace operations_research {

// environment functions
bool KnitroIsCorrectlyInstalled();
absl::Status LoadKnitroDynamicLibrary(std::string& knitropath);

// ******************************
// * misc
// ******************************
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#include <float.h>
#include <stddef.h>
#ifdef _WIN32
#ifdef MAKE_KNITRO_DLL
#define KNITRO_API __declspec(dllexport) __stdcall
#else
#define KNITRO_API __stdcall
#endif
#else
#if (__GNUC__ >= 4) || (__INTEL_COMPILER)
#define KNITRO_API __attribute__((visibility("default")))
#else
#define KNITRO_API
#endif
#endif
#define KNTRUE 1
#define KNFALSE 0
#define KN_LINSOLVER_PHASE_INIT 0
#define KN_LINSOLVER_PHASE_ANALYZE 1
#define KN_LINSOLVER_PHASE_FACTOR 2
#define KN_LINSOLVER_PHASE_SOLVE 3
#define KN_LINSOLVER_PHASE_FREE 4
#define KN_INFINITY DBL_MAX
#define KN_PARAMTYPE_INTEGER 0
#define KN_PARAMTYPE_FLOAT 1
#define KN_PARAMTYPE_STRING 2
#define KN_COMPONENT_VAR 1
#define KN_COMPONENT_OBJ 2
#define KN_COMPONENT_CON 3
#define KN_COMPONENT_RSD 4
#define KN_OBJGOAL_MINIMIZE 0
#define KN_OBJGOAL_MAXIMIZE 1
#define KN_OBJTYPE_CONSTANT -1
#define KN_OBJTYPE_GENERAL 0
#define KN_OBJTYPE_LINEAR 1
#define KN_OBJTYPE_QUADRATIC 2
#define KN_CONTYPE_CONSTANT -1
#define KN_CONTYPE_GENERAL 0
#define KN_CONTYPE_LINEAR 1
#define KN_CONTYPE_QUADRATIC 2
#define KN_CONTYPE_CONIC 3
#define KN_RSDTYPE_CONSTANT -1
#define KN_RSDTYPE_GENERAL 0
#define KN_RSDTYPE_LINEAR 1
#define KN_CCTYPE_VARVAR 0
#define KN_CCTYPE_VARCON 1
#define KN_CCTYPE_CONCON 2
#define KN_VARTYPE_CONTINUOUS 0
#define KN_VARTYPE_INTEGER 1
#define KN_VARTYPE_BINARY 2
#define KN_VAR_LINEAR 1
#define KN_OBJ_CONVEX 1
#define KN_OBJ_CONCAVE 2
#define KN_OBJ_CONTINUOUS 4
#define KN_OBJ_DIFFERENTIABLE 8
#define KN_OBJ_TWICE_DIFFERENTIABLE 16
#define KN_OBJ_NOISY 32
#define KN_OBJ_NONDETERMINISTIC 64
#define KN_CON_CONVEX 1
#define KN_CON_CONCAVE 2
#define KN_CON_CONTINUOUS 4
#define KN_CON_DIFFERENTIABLE 8
#define KN_CON_TWICE_DIFFERENTIABLE 16
#define KN_CON_NOISY 32
#define KN_CON_NONDETERMINISTIC 64
#define KN_DENSE -1
#define KN_DENSE_ROWMAJOR -2
#define KN_DENSE_COLMAJOR -3
#define KN_RC_EVALFC 1
#define KN_RC_EVALGA 2
#define KN_RC_EVALH 3
#define KN_RC_EVALHV 7
#define KN_RC_EVALH_NO_F 8
#define KN_RC_EVALHV_NO_F 9
#define KN_RC_EVALR 10
#define KN_RC_EVALRJ 11
#define KN_RC_EVALFCGA 12
#define KN_RC_OPTIMAL_OR_SATISFACTORY 0
#define KN_RC_OPTIMAL 0
#define KN_RC_NEAR_OPT -100
#define KN_RC_FEAS_XTOL -101
#define KN_RC_FEAS_NO_IMPROVE -102
#define KN_RC_FEAS_FTOL -103
#define KN_RC_INFEASIBLE -200
#define KN_RC_INFEAS_XTOL -201
#define KN_RC_INFEAS_NO_IMPROVE -202
#define KN_RC_INFEAS_MULTISTART -203
#define KN_RC_INFEAS_CON_BOUNDS -204
#define KN_RC_INFEAS_VAR_BOUNDS -205
#define KN_RC_UNBOUNDED -300
#define KN_RC_UNBOUNDED_OR_INFEAS -301
#define KN_RC_ITER_LIMIT_FEAS -400
#define KN_RC_TIME_LIMIT_FEAS -401
#define KN_RC_FEVAL_LIMIT_FEAS -402
#define KN_RC_MIP_EXH_FEAS -403
#define KN_RC_MIP_TERM_FEAS -404
#define KN_RC_MIP_SOLVE_LIMIT_FEAS -405
#define KN_RC_MIP_NODE_LIMIT_FEAS -406
#define KN_RC_ITER_LIMIT_INFEAS -410
#define KN_RC_TIME_LIMIT_INFEAS -411
#define KN_RC_FEVAL_LIMIT_INFEAS -412
#define KN_RC_MIP_EXH_INFEAS -413
#define KN_RC_MIP_SOLVE_LIMIT_INFEAS -415
#define KN_RC_MIP_NODE_LIMIT_INFEAS -416
#define KN_RC_CALLBACK_ERR -500
#define KN_RC_LP_SOLVER_ERR -501
#define KN_RC_EVAL_ERR -502
#define KN_RC_OUT_OF_MEMORY -503
#define KN_RC_USER_TERMINATION -504
#define KN_RC_OPEN_FILE_ERR -505
#define KN_RC_BAD_N_OR_F -506
#define KN_RC_BAD_CONSTRAINT -507
#define KN_RC_BAD_JACOBIAN -508
#define KN_RC_BAD_HESSIAN -509
#define KN_RC_BAD_CON_INDEX -510
#define KN_RC_BAD_JAC_INDEX -511
#define KN_RC_BAD_HESS_INDEX -512
#define KN_RC_BAD_CON_BOUNDS -513
#define KN_RC_BAD_VAR_BOUNDS -514
#define KN_RC_ILLEGAL_CALL -515
#define KN_RC_BAD_KCPTR -516
#define KN_RC_NULL_POINTER -517
#define KN_RC_BAD_INIT_VALUE -518
#define KN_RC_LICENSE_ERROR -520
#define KN_RC_BAD_PARAMINPUT -521
#define KN_RC_LINEAR_SOLVER_ERR -522
#define KN_RC_DERIV_CHECK_FAILED -523
#define KN_RC_DERIV_CHECK_TERMINATE -524
#define KN_RC_OVERFLOW_ERR -525
#define KN_RC_BAD_SIZE -526
#define KN_RC_BAD_VARIABLE -527
#define KN_RC_BAD_VAR_INDEX -528
#define KN_RC_BAD_OBJECTIVE -529
#define KN_RC_BAD_OBJ_INDEX -530
#define KN_RC_BAD_RESIDUAL -531
#define KN_RC_BAD_RSD_INDEX -532
#define KN_RC_INTERNAL_ERROR -600
#define KN_PARAM_NEWPOINT 1001
#define KN_NEWPOINT_NONE 0
#define KN_NEWPOINT_SAVEONE 1
#define KN_NEWPOINT_SAVEALL 2
#define KN_PARAM_HONORBNDS 1002
#define KN_HONORBNDS_AUTO -1
#define KN_HONORBNDS_NO 0
#define KN_HONORBNDS_ALWAYS 1
#define KN_HONORBNDS_INITPT 2
#define KN_PARAM_ALGORITHM 1003
#define KN_PARAM_ALG 1003
#define KN_ALG_AUTOMATIC 0
#define KN_ALG_AUTO 0
#define KN_ALG_BAR_DIRECT 1
#define KN_ALG_BAR_CG 2
#define KN_ALG_ACT_CG 3
#define KN_ALG_ACT_SQP 4
#define KN_ALG_MULTI 5
#define KN_PARAM_BAR_MURULE 1004
#define KN_BAR_MURULE_AUTOMATIC 0
#define KN_BAR_MURULE_AUTO 0
#define KN_BAR_MURULE_MONOTONE 1
#define KN_BAR_MURULE_ADAPTIVE 2
#define KN_BAR_MURULE_PROBING 3
#define KN_BAR_MURULE_DAMPMPC 4
#define KN_BAR_MURULE_FULLMPC 5
#define KN_BAR_MURULE_QUALITY 6
#define KN_PARAM_BAR_FEASIBLE 1006
#define KN_BAR_FEASIBLE_NO 0
#define KN_BAR_FEASIBLE_STAY 1
#define KN_BAR_FEASIBLE_GET 2
#define KN_BAR_FEASIBLE_GET_STAY 3
#define KN_PARAM_GRADOPT 1007
#define KN_GRADOPT_EXACT 1
#define KN_GRADOPT_FORWARD 2
#define KN_GRADOPT_CENTRAL 3
#define KN_GRADOPT_USER_FORWARD 4
#define KN_GRADOPT_USER_CENTRAL 5
#define KN_PARAM_HESSOPT 1008
#define KN_HESSOPT_AUTO 0
#define KN_HESSOPT_EXACT 1
#define KN_HESSOPT_BFGS 2
#define KN_HESSOPT_SR1 3
#define KN_HESSOPT_PRODUCT_FINDIFF 4
#define KN_HESSOPT_PRODUCT 5
#define KN_HESSOPT_LBFGS 6
#define KN_HESSOPT_GAUSS_NEWTON 7
#define KN_PARAM_BAR_INITPT 1009
#define KN_BAR_INITPT_AUTO 0
#define KN_BAR_INITPT_CONVEX 1
#define KN_BAR_INITPT_NEARBND 2
#define KN_BAR_INITPT_CENTRAL 3
#define KN_PARAM_ACT_LPSOLVER 1012
#define KN_ACT_LPSOLVER_INTERNAL 1
#define KN_ACT_LPSOLVER_CPLEX 2
#define KN_ACT_LPSOLVER_XPRESS 3
#define KN_PARAM_CG_MAXIT 1013
#define KN_PARAM_MAXIT 1014
#define KN_PARAM_OUTLEV 1015
#define KN_OUTLEV_NONE 0
#define KN_OUTLEV_SUMMARY 1
#define KN_OUTLEV_ITER_10 2
#define KN_OUTLEV_ITER 3
#define KN_OUTLEV_ITER_VERBOSE 4
#define KN_OUTLEV_ITER_X 5
#define KN_OUTLEV_ALL 6
#define KN_PARAM_OUTMODE 1016
#define KN_OUTMODE_SCREEN 0
#define KN_OUTMODE_FILE 1
#define KN_OUTMODE_BOTH 2
#define KN_PARAM_SCALE 1017
#define KN_SCALE_NEVER 0
#define KN_SCALE_NO 0
#define KN_SCALE_USER_INTERNAL 1
#define KN_SCALE_USER_NONE 2
#define KN_SCALE_INTERNAL 3
#define KN_PARAM_SOC 1019
#define KN_SOC_NO 0
#define KN_SOC_MAYBE 1
#define KN_SOC_YES 2
#define KN_PARAM_DELTA 1020
#define KN_PARAM_BAR_FEASMODETOL 1021
#define KN_PARAM_FEASTOL 1022
#define KN_PARAM_FEASTOLABS 1023
#define KN_PARAM_MAXTIMECPU 1024
#define KN_PARAM_BAR_INITMU 1025
#define KN_PARAM_OBJRANGE 1026
#define KN_PARAM_OPTTOL 1027
#define KN_PARAM_OPTTOLABS 1028
#define KN_PARAM_LINSOLVER_PIVOTTOL 1029
#define KN_PARAM_XTOL 1030
#define KN_PARAM_DEBUG 1031
#define KN_DEBUG_NONE 0
#define KN_DEBUG_PROBLEM 1
#define KN_DEBUG_EXECUTION 2
#define KN_PARAM_MULTISTART 1033
#define KN_PARAM_MSENABLE 1033
#define KN_PARAM_MS_ENABLE 1033
#define KN_MULTISTART_NO 0
#define KN_MS_ENABLE_NO 0
#define KN_MULTISTART_YES 1
#define KN_MS_ENABLE_YES 1
#define KN_PARAM_MSMAXSOLVES 1034
#define KN_PARAM_MS_MAXSOLVES 1034
#define KN_PARAM_MSMAXBNDRANGE 1035
#define KN_PARAM_MS_MAXBNDRANGE 1035
#define KN_PARAM_MSMAXTIMECPU 1036
#define KN_PARAM_MS_MAXTIMECPU 1036
#define KN_PARAM_MSMAXTIMEREAL 1037
#define KN_PARAM_MS_MAXTIMEREAL 1037
#define KN_PARAM_LMSIZE 1038
#define KN_PARAM_BAR_MAXCROSSIT 1039
#define KN_PARAM_MAXTIMEREAL 1040
#define KN_PARAM_CG_PRECOND 1041
#define KN_CG_PRECOND_NONE 0
#define KN_CG_PRECOND_CHOL 1
#define KN_PARAM_BLASOPTION 1042
#define KN_BLASOPTION_AUTO -1
#define KN_BLASOPTION_KNITRO 0
#define KN_BLASOPTION_INTEL 1
#define KN_BLASOPTION_DYNAMIC 2
#define KN_BLASOPTION_BLIS 3
#define KN_BLASOPTION_APPLE 4
#define KN_PARAM_BAR_MAXREFACTOR 1043
#define KN_PARAM_LINESEARCH_MAXTRIALS 1044
#define KN_PARAM_BLASOPTIONLIB 1045
#define KN_PARAM_OUTAPPEND 1046
#define KN_OUTAPPEND_NO 0
#define KN_OUTAPPEND_YES 1
#define KN_PARAM_OUTDIR 1047
#define KN_PARAM_CPLEXLIB 1048
#define KN_PARAM_BAR_PENRULE 1049
#define KN_BAR_PENRULE_AUTO 0
#define KN_BAR_PENRULE_SINGLE 1
#define KN_BAR_PENRULE_FLEX 2
#define KN_PARAM_BAR_PENCONS 1050
#define KN_BAR_PENCONS_AUTO -1
#define KN_BAR_PENCONS_NONE 0
#define KN_BAR_PENCONS_ALL 2
#define KN_BAR_PENCONS_EQUALITIES 3
#define KN_BAR_PENCONS_INFEAS 4
#define KN_PARAM_MSNUMTOSAVE 1051
#define KN_PARAM_MS_NUMTOSAVE 1051
#define KN_PARAM_MSSAVETOL 1052
#define KN_PARAM_MS_SAVETOL 1052
#define KN_PARAM_PRESOLVEDEBUG 1053
#define KN_PRESOLVEDBG_NONE 0
#define KN_PRESOLVEDBG_BASIC 1
#define KN_PRESOLVEDBG_VERBOSE 2
#define KN_PRESOLVEDBG_DETAIL 3
#define KN_PARAM_MSTERMINATE 1054
#define KN_PARAM_MS_TERMINATE 1054
#define KN_MSTERMINATE_MAXSOLVES 0
#define KN_MS_TERMINATE_MAXSOLVES 0
#define KN_MSTERMINATE_OPTIMAL 1
#define KN_MS_TERMINATE_OPTIMAL 1
#define KN_MSTERMINATE_FEASIBLE 2
#define KN_MS_TERMINATE_FEASIBLE 2
#define KN_MSTERMINATE_ANY 3
#define KN_MS_TERMINATE_ANY 3
#define KN_MSTERMINATE_RULEBASED 4
#define KN_MS_TERMINATE_RULEBASED 4
#define KN_PARAM_MSSTARTPTRANGE 1055
#define KN_PARAM_MS_STARTPTRANGE 1055
#define KN_PARAM_INFEASTOL 1056
#define KN_PARAM_LINSOLVER 1057
#define KN_LINSOLVER_AUTO 0
#define KN_LINSOLVER_INTERNAL 1
#define KN_LINSOLVER_HYBRID 2
#define KN_LINSOLVER_DENSEQR 3
#define KN_LINSOLVER_MA27 4
#define KN_LINSOLVER_MA57 5
#define KN_LINSOLVER_MKLPARDISO 6
#define KN_LINSOLVER_MA97 7
#define KN_LINSOLVER_MA86 8
#define KN_PARAM_BAR_DIRECTINTERVAL 1058
#define KN_PARAM_PRESOLVE 1059
#define KN_PRESOLVE_NO 0
#define KN_PRESOLVE_NONE 0
#define KN_PRESOLVE_YES 1
#define KN_PRESOLVE_BASIC 1
#define KN_PRESOLVE_ADVANCED 2
#define KN_PARAM_PRESOLVE_TOL 1060
#define KN_PARAM_BAR_SWITCHRULE 1061
#define KN_BAR_SWITCHRULE_AUTO -1
#define KN_BAR_SWITCHRULE_NEVER 0
#define KN_BAR_SWITCHRULE_MODERATE 2
#define KN_BAR_SWITCHRULE_AGGRESSIVE 3
#define KN_PARAM_HESSIAN_NO_F 1062
#define KN_HESSIAN_NO_F_FORBID 0
#define KN_HESSIAN_NO_F_ALLOW 1
#define KN_PARAM_MA_TERMINATE 1063
#define KN_MA_TERMINATE_ALL 0
#define KN_MA_TERMINATE_OPTIMAL 1
#define KN_MA_TERMINATE_FEASIBLE 2
#define KN_MA_TERMINATE_ANY 3
#define KN_PARAM_MA_MAXTIMECPU 1064
#define KN_PARAM_MA_MAXTIMEREAL 1065
#define KN_PARAM_MSSEED 1066
#define KN_PARAM_MS_SEED 1066
#define KN_PARAM_MA_OUTSUB 1067
#define KN_MA_OUTSUB_NONE 0
#define KN_MA_OUTSUB_YES 1
#define KN_PARAM_MS_OUTSUB 1068
#define KN_MS_OUTSUB_NONE 0
#define KN_MS_OUTSUB_YES 1
#define KN_PARAM_XPRESSLIB 1069
#define KN_PARAM_TUNER 1070
#define KN_TUNER_OFF 0
#define KN_TUNER_ON 1
#define KN_PARAM_TUNER_OPTIONSFILE 1071
#define KN_PARAM_TUNER_MAXTIMECPU 1072
#define KN_PARAM_TUNER_MAXTIMEREAL 1073
#define KN_PARAM_TUNER_OUTSUB 1074
#define KN_TUNER_OUTSUB_NONE 0
#define KN_TUNER_OUTSUB_SUMMARY 1
#define KN_TUNER_OUTSUB_ALL 2
#define KN_PARAM_TUNER_TERMINATE 1075
#define KN_TUNER_TERMINATE_ALL 0
#define KN_TUNER_TERMINATE_OPTIMAL 1
#define KN_TUNER_TERMINATE_FEASIBLE 2
#define KN_TUNER_TERMINATE_ANY 3
#define KN_PARAM_LINSOLVER_OOC 1076
#define KN_LINSOLVER_OOC_NO 0
#define KN_LINSOLVER_OOC_MAYBE 1
#define KN_LINSOLVER_OOC_YES 2
#define KN_PARAM_BAR_RELAXCONS 1077
#define KN_BAR_RELAXCONS_NONE 0
#define KN_BAR_RELAXCONS_EQS 1
#define KN_BAR_RELAXCONS_INEQS 2
#define KN_BAR_RELAXCONS_ALL 3
#define KN_PARAM_MSDETERMINISTIC 1078
#define KN_PARAM_MS_DETERMINISTIC 1078
#define KN_MSDETERMINISTIC_NO 0
#define KN_MS_DETERMINISTIC_NO 0
#define KN_MSDETERMINISTIC_YES 1
#define KN_MS_DETERMINISTIC_YES 1
#define KN_PARAM_BAR_REFINEMENT 1079
#define KN_BAR_REFINEMENT_NO 0
#define KN_BAR_REFINEMENT_YES 1
#define KN_PARAM_DERIVCHECK 1080
#define KN_DERIVCHECK_NONE 0
#define KN_DERIVCHECK_FIRST 1
#define KN_DERIVCHECK_SECOND 2
#define KN_DERIVCHECK_ALL 3
#define KN_PARAM_DERIVCHECK_TYPE 1081
#define KN_DERIVCHECK_FORWARD 1
#define KN_DERIVCHECK_CENTRAL 2
#define KN_PARAM_DERIVCHECK_TOL 1082
#define KN_PARAM_LINSOLVER_INEXACT 1083
#define KN_LINSOLVER_INEXACT_NO 0
#define KN_LINSOLVER_INEXACT_YES 1
#define KN_PARAM_LINSOLVER_INEXACTTOL 1084
#define KN_PARAM_MAXFEVALS 1085
#define KN_PARAM_FSTOPVAL 1086
#define KN_PARAM_DATACHECK 1087
#define KN_DATACHECK_NO 0
#define KN_DATACHECK_YES 1
#define KN_PARAM_DERIVCHECK_TERMINATE 1088
#define KN_DERIVCHECK_STOPERROR 1
#define KN_DERIVCHECK_STOPALWAYS 2
#define KN_PARAM_BAR_WATCHDOG 1089
#define KN_BAR_WATCHDOG_NO 0
#define KN_BAR_WATCHDOG_YES 1
#define KN_PARAM_FTOL 1090
#define KN_PARAM_FTOL_ITERS 1091
#define KN_PARAM_ACT_QPALG 1092
#define KN_ACT_QPALG_AUTO 0
#define KN_ACT_QPALG_BAR_DIRECT 1
#define KN_ACT_QPALG_BAR_CG 2
#define KN_ACT_QPALG_ACT_CG 3
#define KN_PARAM_BAR_INITPI_MPEC 1093
#define KN_PARAM_XTOL_ITERS 1094
#define KN_PARAM_LINESEARCH 1095
#define KN_LINESEARCH_AUTO 0
#define KN_LINESEARCH_BACKTRACK 1
#define KN_LINESEARCH_INTERPOLATE 2
#define KN_LINESEARCH_WEAKWOLFE 3
#define KN_PARAM_OUT_CSVINFO 1096
#define KN_OUT_CSVINFO_NO 0
#define KN_OUT_CSVINFO_YES 1
#define KN_PARAM_INITPENALTY 1097
#define KN_PARAM_ACT_LPFEASTOL 1098
#define KN_PARAM_CG_STOPTOL 1099
#define KN_PARAM_RESTARTS 1100
#define KN_PARAM_RESTARTS_MAXIT 1101
#define KN_PARAM_BAR_SLACKBOUNDPUSH 1102
#define KN_PARAM_CG_PMEM 1103
#define KN_PARAM_BAR_SWITCHOBJ 1104
#define KN_BAR_SWITCHOBJ_NONE 0
#define KN_BAR_SWITCHOBJ_SCALARPROX 1
#define KN_BAR_SWITCHOBJ_DIAGPROX 2
#define KN_PARAM_OUTNAME 1105
#define KN_PARAM_OUT_CSVNAME 1106
#define KN_PARAM_ACT_PARAMETRIC 1107
#define KN_ACT_PARAMETRIC_NO 0
#define KN_ACT_PARAMETRIC_MAYBE 1
#define KN_ACT_PARAMETRIC_YES 2
#define KN_PARAM_ACT_LPDUMPMPS 1108
#define KN_ACT_LPDUMPMPS_NO 0
#define KN_ACT_LPDUMPMPS_YES 1
#define KN_PARAM_ACT_LPALG 1109
#define KN_ACT_LPALG_DEFAULT 0
#define KN_ACT_LPALG_PRIMAL 1
#define KN_ACT_LPALG_DUAL 2
#define KN_ACT_LPALG_BARRIER 3
#define KN_PARAM_ACT_LPPRESOLVE 1110
#define KN_ACT_LPPRESOLVE_OFF 0
#define KN_ACT_LPPRESOLVE_ON 1
#define KN_PARAM_ACT_LPPENALTY 1111
#define KN_ACT_LPPENALTY_ALL 1
#define KN_ACT_LPPENALTY_NONLINEAR 2
#define KN_ACT_LPPENALTY_DYNAMIC 3
#define KN_PARAM_BNDRANGE 1112
#define KN_PARAM_BAR_CONIC_ENABLE 1113
#define KN_BAR_CONIC_ENABLE_AUTO -1
#define KN_BAR_CONIC_ENABLE_NONE 0
#define KN_BAR_CONIC_ENABLE_SOC 1
#define KN_PARAM_CONVEX 1114
#define KN_CONVEX_AUTO -1
#define KN_CONVEX_NO 0
#define KN_CONVEX_YES 1
#define KN_PARAM_OUT_HINTS 1115
#define KN_OUT_HINTS_NO 0
#define KN_OUT_HINTS_YES 1
#define KN_PARAM_EVAL_FCGA 1116
#define KN_EVAL_FCGA_NO 0
#define KN_EVAL_FCGA_YES 1
#define KN_PARAM_BAR_MAXCORRECTORS 1117
#define KN_PARAM_STRAT_WARM_START 1118
#define KN_STRAT_WARM_START_NO 0
#define KN_STRAT_WARM_START_YES 1
#define KN_PARAM_FINDIFF_TERMINATE 1119
#define KN_FINDIFF_TERMINATE_NONE 0
#define KN_FINDIFF_TERMINATE_ERREST 1
#define KN_PARAM_CPUPLATFORM 1120
#define KN_CPUPLATFORM_AUTO -1
#define KN_CPUPLATFORM_COMPATIBLE 1
#define KN_CPUPLATFORM_SSE2 2
#define KN_CPUPLATFORM_AVX 3
#define KN_CPUPLATFORM_AVX2 4
#define KN_CPUPLATFORM_AVX512 5
#define KN_PARAM_PRESOLVE_PASSES 1121
#define KN_PARAM_PRESOLVE_LEVEL 1122
#define KN_PRESOLVE_LEVEL_AUTO -1
#define KN_PRESOLVE_LEVEL_1 1
#define KN_PRESOLVE_LEVEL_2 2
#define KN_PARAM_FINDIFF_RELSTEPSIZE 1123
#define KN_PARAM_INFEASTOL_ITERS 1124
#define KN_PARAM_PRESOLVEOP_TIGHTEN 1125
#define KN_PRESOLVEOP_TIGHTEN_AUTO -1
#define KN_PRESOLVEOP_TIGHTEN_NONE 0
#define KN_PRESOLVEOP_TIGHTEN_VARBND 1
#define KN_PRESOLVEOP_TIGHTEN_COEF 2
#define KN_PRESOLVEOP_TIGHTEN_ALL 3
#define KN_PARAM_BAR_LINSYS 1126
#define KN_BAR_LINSYS_AUTO -1
#define KN_BAR_LINSYS_FULL 0
#define KN_BAR_LINSYS_COMPACT1 1
#define KN_BAR_LINSYS_ELIMINATE_SLACKS 1
#define KN_BAR_LINSYS_COMPACT2 2
#define KN_BAR_LINSYS_ELIMINATE_BOUNDS 2
#define KN_BAR_LINSYS_ELIMINATE_INEQS 3
#define KN_PARAM_PRESOLVE_INITPT 1127
#define KN_PRESOLVE_INITPT_AUTO -1
#define KN_PRESOLVE_INITPT_NOSHIFT 0
#define KN_PRESOLVE_INITPT_LINSHIFT 1
#define KN_PRESOLVE_INITPT_ANYSHIFT 2
#define KN_PARAM_ACT_QPPENALTY 1128
#define KN_ACT_QPPENALTY_AUTO -1
#define KN_ACT_QPPENALTY_NONE 0
#define KN_ACT_QPPENALTY_ALL 1
#define KN_PARAM_BAR_LINSYS_STORAGE 1129
#define KN_BAR_LINSYS_STORAGE_AUTO -1
#define KN_BAR_LINSYS_STORAGE_LOWMEM 1
#define KN_BAR_LINSYS_STORAGE_NORMAL 2
#define KN_PARAM_LINSOLVER_MAXITREF 1130
#define KN_PARAM_BFGS_SCALING 1131
#define KN_BFGS_SCALING_DYNAMIC 0
#define KN_BFGS_SCALING_INVHESS 1
#define KN_BFGS_SCALING_HESS 2
#define KN_PARAM_BAR_INITSHIFTTOL 1132
#define KN_PARAM_NUMTHREADS 1133
#define KN_PARAM_CONCURRENT_EVALS 1134
#define KN_CONCURRENT_EVALS_NO 0
#define KN_CONCURRENT_EVALS_YES 1
#define KN_PARAM_BLAS_NUMTHREADS 1135
#define KN_PARAM_LINSOLVER_NUMTHREADS 1136
#define KN_PARAM_MS_NUMTHREADS 1137
#define KN_PARAM_CONIC_NUMTHREADS 1138
#define KN_PARAM_NCVX_QCQP_INIT 1139
#define KN_NCVX_QCQP_INIT_AUTO -1
#define KN_NCVX_QCQP_INIT_NONE 0
#define KN_NCVX_QCQP_INIT_LINEAR 1
#define KN_NCVX_QCQP_INIT_HYBRID 2
#define KN_NCVX_QCQP_INIT_PENALTY 3
#define KN_NCVX_QCQP_INIT_CVXQUAD 4
#define KN_PARAM_FINDIFF_ESTNOISE 1140
#define KN_FINDIFF_ESTNOISE_NO 0
#define KN_FINDIFF_ESTNOISE_YES 1
#define KN_FINDIFF_ESTNOISE_WITHCURV 2
#define KN_PARAM_FINDIFF_NUMTHREADS 1141
#define KN_PARAM_BAR_MPEC_HEURISTIC 1142
#define KN_BAR_MPEC_HEURISTIC_NO 0
#define KN_BAR_MPEC_HEURISTIC_YES 1
#define KN_PARAM_PRESOLVEOP_REDUNDANT 1143
#define KN_PRESOLVEOP_REDUNDANT_NONE 0
#define KN_PRESOLVEOP_REDUNDANT_DUPCON 1
#define KN_PRESOLVEOP_REDUNDANT_DEPCON 2
#define KN_PARAM_LINSOLVER_ORDERING 1144
#define KN_LINSOLVER_ORDERING_AUTO -1
#define KN_LINSOLVER_ORDERING_BEST 0
#define KN_LINSOLVER_ORDERING_AMD 1
#define KN_LINSOLVER_ORDERING_METIS 2
#define KN_PARAM_LINSOLVER_NODEAMALG 1145
#define KN_PARAM_PRESOLVEOP_SUBSTITUTION 1146
#define KN_PRESOLVEOP_SUBSTITUTION_AUTO -1
#define KN_PRESOLVEOP_SUBSTITUTION_NONE 0
#define KN_PRESOLVEOP_SUBSTITUTION_SIMPLE 1
#define KN_PRESOLVEOP_SUBSTITUTION_ALL 2
#define KN_PARAM_PRESOLVEOP_SUBSTITUTION_TOL 1147
#define KN_PARAM_MS_INITPT_CLUSTER 1149
#define KN_MS_INITPT_CLUSTER_NONE 0
#define KN_MS_INITPT_CLUSTER_SL 1
#define KN_PARAM_SCALE_VARS 1153
#define KN_SCALE_VARS_NONE 0
#define KN_SCALE_VARS_BNDS 1
#define KN_PARAM_BAR_MAXMU 1154
#define KN_PARAM_BAR_GLOBALIZE 1155
#define KN_BAR_GLOBALIZE_NONE 0
#define KN_BAR_GLOBALIZE_KKT 1
#define KN_BAR_GLOBALIZE_FILTER 2
#define KN_PARAM_LINSOLVER_SCALING 1156
#define KN_LINSOLVER_SCALING_NONE 0
#define KN_LINSOLVER_SCALING_ALWAYS 1
#define KN_PARAM_MIP_METHOD 2001
#define KN_MIP_METHOD_AUTO 0
#define KN_MIP_METHOD_BB 1
#define KN_MIP_METHOD_HQG 2
#define KN_MIP_METHOD_MISQP 3
#define KN_PARAM_MIP_BRANCHRULE 2002
#define KN_MIP_BRANCH_AUTO 0
#define KN_MIP_BRANCH_MOSTFRAC 1
#define KN_MIP_BRANCH_PSEUDOCOST 2
#define KN_MIP_BRANCH_STRONG 3
#define KN_PARAM_MIP_SELECTRULE 2003
#define KN_MIP_SEL_AUTO 0
#define KN_MIP_SEL_DEPTHFIRST 1
#define KN_MIP_SEL_BESTBOUND 2
#define KN_MIP_SEL_COMBO_1 3
#define KN_PARAM_MIP_INTGAPABS 2004
#define KN_PARAM_MIP_OPTGAPABS 2004
#define KN_PARAM_MIP_INTGAPREL 2005
#define KN_PARAM_MIP_OPTGAPREL 2005
#define KN_PARAM_MIP_MAXTIMECPU 2006
#define KN_PARAM_MIP_MAXTIMEREAL 2007
#define KN_PARAM_MIP_MAXSOLVES 2008
#define KN_PARAM_MIP_INTEGERTOL 2009
#define KN_PARAM_MIP_OUTLEVEL 2010
#define KN_MIP_OUTLEVEL_NONE 0
#define KN_MIP_OUTLEVEL_ITERS 1
#define KN_MIP_OUTLEVEL_ITERSTIME 2
#define KN_MIP_OUTLEVEL_ROOT 3
#define KN_PARAM_MIP_OUTINTERVAL 2011
#define KN_PARAM_MIP_OUTSUB 2012
#define KN_MIP_OUTSUB_NONE 0
#define KN_MIP_OUTSUB_YES 1
#define KN_MIP_OUTSUB_YESPROB 2
#define KN_PARAM_MIP_DEBUG 2013
#define KN_MIP_DEBUG_NONE 0
#define KN_MIP_DEBUG_ALL 1
#define KN_PARAM_MIP_IMPLICATNS 2014
#define KN_PARAM_MIP_IMPLICATIONS 2014
#define KN_MIP_IMPLICATNS_NO 0
#define KN_MIP_IMPLICATIONS_NO 0
#define KN_MIP_IMPLICATNS_YES 1
#define KN_MIP_IMPLICATIONS_YES 1
#define KN_PARAM_MIP_GUB_BRANCH 2015
#define KN_MIP_GUB_BRANCH_NO 0
#define KN_MIP_GUB_BRANCH_YES 1
#define KN_PARAM_MIP_KNAPSACK 2016
#define KN_MIP_KNAPSACK_AUTO -1
#define KN_MIP_KNAPSACK_NO 0
#define KN_MIP_KNAPSACK_NONE 0
#define KN_MIP_KNAPSACK_ROOT 1
#define KN_MIP_KNAPSACK_TREE 2
#define KN_MIP_KNAPSACK_INEQ 1
#define KN_MIP_KNAPSACK_LIFTED 2
#define KN_MIP_KNAPSACK_ALL 3
#define KN_PARAM_MIP_ROUNDING 2017
#define KN_MIP_ROUND_AUTO -1
#define KN_MIP_ROUND_NONE 0
#define KN_MIP_ROUND_HEURISTIC 2
#define KN_MIP_ROUND_NLP_SOME 3
#define KN_MIP_ROUND_NLP_ALWAYS 4
#define KN_PARAM_MIP_ROOTALG 2018
#define KN_MIP_ROOTALG_AUTO 0
#define KN_MIP_ROOTALG_BAR_DIRECT 1
#define KN_MIP_ROOTALG_BAR_CG 2
#define KN_MIP_ROOTALG_ACT_CG 3
#define KN_MIP_ROOTALG_ACT_SQP 4
#define KN_MIP_ROOTALG_MULTI 5
#define KN_PARAM_MIP_LPALG 2019
#define KN_MIP_LPALG_AUTO 0
#define KN_MIP_LPALG_BAR_DIRECT 1
#define KN_MIP_LPALG_BAR_CG 2
#define KN_MIP_LPALG_ACT_CG 3
#define KN_PARAM_MIP_TERMINATE 2020
#define KN_MIP_TERMINATE_OPTIMAL 0
#define KN_MIP_TERMINATE_FEASIBLE 1
#define KN_PARAM_MIP_MAXNODES 2021
#define KN_PARAM_MIP_HEURISTIC 2022
#define KN_MIP_HEURISTIC_AUTO -1
#define KN_MIP_HEURISTIC_NONE 0
#define KN_MIP_HEURISTIC_FEASPUMP 2
#define KN_MIP_HEURISTIC_MPEC 3
#define KN_MIP_HEURISTIC_DIVING 4
#define KN_PARAM_MIP_HEUR_MAXIT 2023
#define KN_PARAM_MIP_HEUR_MAXTIMECPU 2024
#define KN_PARAM_MIP_HEUR_MAXTIMEREAL 2025
#define KN_PARAM_MIP_PSEUDOINIT 2026
#define KN_MIP_PSEUDOINIT_AUTO 0
#define KN_MIP_PSEUDOINIT_AVE 1
#define KN_MIP_PSEUDOINIT_STRONG 2
#define KN_PARAM_MIP_STRONG_MAXIT 2027
#define KN_PARAM_MIP_STRONG_CANDLIM 2028
#define KN_PARAM_MIP_STRONG_LEVEL 2029
#define KN_PARAM_MIP_INTVAR_STRATEGY 2030
#define KN_MIP_INTVAR_STRATEGY_NONE 0
#define KN_MIP_INTVAR_STRATEGY_RELAX 1
#define KN_MIP_INTVAR_STRATEGY_MPEC 2
#define KN_PARAM_MIP_RELAXABLE 2031
#define KN_MIP_RELAXABLE_NONE 0
#define KN_MIP_RELAXABLE_ALL 1
#define KN_PARAM_MIP_NODEALG 2032
#define KN_MIP_NODEALG_AUTO 0
#define KN_MIP_NODEALG_BAR_DIRECT 1
#define KN_MIP_NODEALG_BAR_CG 2
#define KN_MIP_NODEALG_ACT_CG 3
#define KN_MIP_NODEALG_ACT_SQP 4
#define KN_MIP_NODEALG_MULTI 5
#define KN_PARAM_MIP_HEUR_TERMINATE 2033
#define KN_MIP_HEUR_TERMINATE_FEASIBLE 1
#define KN_MIP_HEUR_TERMINATE_LIMIT 2
#define KN_PARAM_MIP_SELECTDIR 2034
#define KN_MIP_SELECTDIR_DOWN 0
#define KN_MIP_SELECTDIR_UP 1
#define KN_PARAM_MIP_CUTFACTOR 2035
#define KN_PARAM_MIP_ZEROHALF 2036
#define KN_MIP_ZEROHALF_AUTO -1
#define KN_MIP_ZEROHALF_NONE 0
#define KN_MIP_ZEROHALF_ROOT 1
#define KN_MIP_ZEROHALF_TREE 2
#define KN_MIP_ZEROHALF_ALL 3
#define KN_PARAM_MIP_MIR 2037
#define KN_MIP_MIR_AUTO -1
#define KN_MIP_MIR_NONE 0
#define KN_MIP_MIR_ROOT 1
#define KN_MIP_MIR_TREE 2
#define KN_MIP_MIR_NLP 2
#define KN_PARAM_MIP_CLIQUE 2038
#define KN_MIP_CLIQUE_AUTO -1
#define KN_MIP_CLIQUE_NONE 0
#define KN_MIP_CLIQUE_ROOT 1
#define KN_MIP_CLIQUE_TREE 2
#define KN_MIP_CLIQUE_ALL 3
#define KN_PARAM_MIP_HEUR_STRATEGY 2039
#define KN_MIP_HEUR_STRATEGY_AUTO -1
#define KN_MIP_HEUR_STRATEGY_NONE 0
#define KN_MIP_HEUR_STRATEGY_BASIC 1
#define KN_MIP_HEUR_STRATEGY_ADVANCED 2
#define KN_MIP_HEUR_STRATEGY_EXTENSIVE 3
#define KN_PARAM_MIP_HEUR_FEASPUMP 2040
#define KN_MIP_HEUR_FEASPUMP_AUTO -1
#define KN_MIP_HEUR_FEASPUMP_OFF 0
#define KN_MIP_HEUR_FEASPUMP_ON 1
#define KN_PARAM_MIP_HEUR_MPEC 2041
#define KN_MIP_HEUR_MPEC_AUTO -1
#define KN_MIP_HEUR_MPEC_OFF 0
#define KN_MIP_HEUR_MPEC_ON 1
#define KN_PARAM_MIP_HEUR_DIVING 2042
#define KN_PARAM_MIP_CUTTINGPLANE 2043
#define KN_MIP_CUTTINGPLANE_NONE 0
#define KN_MIP_CUTTINGPLANE_ROOT 1
#define KN_PARAM_MIP_CUTOFF 2044
#define KN_PARAM_MIP_HEUR_LNS 2045
#define KN_PARAM_MIP_MULTISTART 2046
#define KN_MIP_MULTISTART_OFF 0
#define KN_MIP_MULTISTART_ON 1
#define KN_PARAM_MIP_LIFTPROJECT 2047
#define KN_MIP_LIFTPROJECT_AUTO -1
#define KN_MIP_LIFTPROJECT_NONE 0
#define KN_MIP_LIFTPROJECT_ROOT 1
#define KN_PARAM_MIP_NUMTHREADS 2048
#define KN_PARAM_MIP_HEUR_MISQP 2049
#define KN_MIP_HEUR_MISQP_AUTO -1
#define KN_MIP_HEUR_MISQP_OFF 0
#define KN_MIP_HEUR_MISQP_ON 1
#define KN_PARAM_MIP_RESTART 2050
#define KN_MIP_RESTART_OFF 0
#define KN_MIP_RESTART_ON 1
#define KN_PARAM_MIP_GOMORY 2051
#define KN_MIP_GOMORY_AUTO -1
#define KN_MIP_GOMORY_NONE 0
#define KN_MIP_GOMORY_ROOT 1
#define KN_MIP_GOMORY_TREE 2
#define KN_PARAM_MIP_CUT_PROBING 2052
#define KN_MIP_CUT_PROBING_AUTO -1
#define KN_MIP_CUT_PROBING_NONE 0
#define KN_MIP_CUT_PROBING_ROOT 1
#define KN_MIP_CUT_PROBING_TREE 2
#define KN_PARAM_MIP_CUT_FLOWCOVER 2053
#define KN_MIP_CUT_FLOWCOVER_AUTO -1
#define KN_MIP_CUT_FLOWCOVER_NONE 0
#define KN_MIP_CUT_FLOWCOVER_ROOT 1
#define KN_MIP_CUT_FLOWCOVER_TREE 2
#define KN_PARAM_MIP_HEUR_LOCALSEARCH 2054
#define KN_MIP_HEUR_LOCALSEARCH_AUTO -1
#define KN_MIP_HEUR_LOCALSEARCH_OFF 0
#define KN_MIP_HEUR_LOCALSEARCH_ON 1
#define KN_PARAM_PAR_NUMTHREADS 3001
#define KN_PARAM_PAR_CONCURRENT_EVALS 3002
#define KN_PAR_CONCURRENT_EVALS_NO 0
#define KN_PAR_CONCURRENT_EVALS_YES 1
#define KN_PARAM_PAR_BLASNUMTHREADS 3003
#define KN_PARAM_PAR_LSNUMTHREADS 3004
#define KN_PARAM_PAR_MSNUMTHREADS 3005
#define KN_PAR_MSNUMTHREADS_AUTO 0
#define KN_PARAM_PAR_CONICNUMTHREADS 3006

// ******************************
// * functions
// ******************************
extern std::function<int KNITRO_API(const int length, char* const release)>
    KN_get_release;
extern std::function<int KNITRO_API(KN_context_ptr* kc)> KN_new;
extern std::function<int KNITRO_API(KN_context_ptr* kc)> KN_free;
extern std::function<int KNITRO_API(LM_context_ptr* lmc)> KN_checkout_license;
extern std::function<int KNITRO_API(LM_context_ptr lmc, KN_context_ptr* kc)>
    KN_new_lm;
extern std::function<int KNITRO_API(LM_context_ptr* lmc)> KN_release_license;
extern std::function<int KNITRO_API(KN_context_ptr kc)>
    KN_reset_params_to_defaults;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const char* const filename)>
    KN_load_param_file;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const char* const filename)>
    KN_load_tuner_file;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const char* const filename)>
    KN_save_param_file;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* const name,
                                    const int value)>
    KN_set_int_param_by_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* const name,
                                    const char* const value)>
    KN_set_char_param_by_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* const name,
                                    const double value)>
    KN_set_double_param_by_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* const name,
                                    const double value)>
    KN_set_param_by_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    const int value)>
    KN_set_int_param;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    const char* const value)>
    KN_set_char_param;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    const double value)>
    KN_set_double_param;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* const name,
                                    int* const value)>
    KN_get_int_param_by_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* const name,
                                    double* const value)>
    KN_get_double_param_by_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    int* const value)>
    KN_get_int_param;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    double* const value)>
    KN_get_double_param;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    char* const param_name,
                                    const size_t output_size)>
    KN_get_param_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    char* const description,
                                    const size_t output_size)>
    KN_get_param_doc;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    int* const param_type)>
    KN_get_param_type;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int param_id,
                                    int* const num_param_values)>
    KN_get_num_param_values;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const int param_id, const int value_id,
    char* const param_value_string, const size_t output_size)>
    KN_get_param_value_doc;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* const name,
                                    int* const param_id)>
    KN_get_param_id;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    KNINT* const indexVars)>
    KN_add_vars;
extern std::function<int KNITRO_API(KN_context_ptr kc, KNINT* const indexVar)>
    KN_add_var;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    KNINT* const indexCons)>
    KN_add_cons;
extern std::function<int KNITRO_API(KN_context_ptr kc, KNINT* const indexCon)>
    KN_add_con;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nR,
                                    KNINT* const indexRsds)>
    KN_add_rsds;
extern std::function<int KNITRO_API(KN_context_ptr kc, KNINT* const indexRsd)>
    KN_add_rsd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const double* const xLoBnds)>
    KN_set_var_lobnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const xLoBnds)>
    KN_set_var_lobnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double xLoBnd)>
    KN_set_var_lobnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const double* const xUpBnds)>
    KN_set_var_upbnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const xUpBnds)>
    KN_set_var_upbnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double xUpBnd)>
    KN_set_var_upbnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const double* const xFxBnds)>
    KN_set_var_fxbnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const xFxBnds)>
    KN_set_var_fxbnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double xFxBnd)>
    KN_set_var_fxbnd;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    double* const xLoBnds)>
    KN_get_var_lobnds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const xLoBnds)>
    KN_get_var_lobnds_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexVar, double* const xLoBnd)>
    KN_get_var_lobnd;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    double* const xUpBnds)>
    KN_get_var_upbnds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const xUpBnds)>
    KN_get_var_upbnds_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexVar, double* const xUpBnd)>
    KN_get_var_upbnd;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    double* const xFxBnds)>
    KN_get_var_fxbnds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const xFxBnds)>
    KN_get_var_fxbnds_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexVar, double* const xFxBnd)>
    KN_get_var_fxbnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const int* const xTypes)>
    KN_set_var_types;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int* const xTypes)>
    KN_set_var_types_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const int xType)>
    KN_set_var_type;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    int* const xTypes)>
    KN_get_var_types;
extern std::function<int KNITRO_API(const KN_context_ptr kc, int* const xTypes)>
    KN_get_var_types_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexVar, int* const xType)>
    KN_get_var_type;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const int* const xProperties)>
    KN_set_var_properties;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const int* const xProperties)>
    KN_set_var_properties_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const int xProperty)>
    KN_set_var_property;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const cLoBnds)>
    KN_set_con_lobnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const cLoBnds)>
    KN_set_con_lobnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double cLoBnd)>
    KN_set_con_lobnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const cUpBnds)>
    KN_set_con_upbnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const cUpBnds)>
    KN_set_con_upbnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double cUpBnd)>
    KN_set_con_upbnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const cEqBnds)>
    KN_set_con_eqbnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const cEqBnds)>
    KN_set_con_eqbnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double cEqBnd)>
    KN_set_con_eqbnd;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    double* const cLoBnds)>
    KN_get_con_lobnds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const cLoBnds)>
    KN_get_con_lobnds_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexCon, double* const cLoBnd)>
    KN_get_con_lobnd;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    double* const cUpBnds)>
    KN_get_con_upbnds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const cUpBnds)>
    KN_get_con_upbnds_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexCon, double* const cUpBnd)>
    KN_get_con_upbnd;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    double* const cEqBnds)>
    KN_get_con_eqbnds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const cEqBnds)>
    KN_get_con_eqbnds_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexCon, double* const cEqBnd)>
    KN_get_con_eqbnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int objProperty)>
    KN_set_obj_property;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const int* const cProperties)>
    KN_set_con_properties;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const int* const cProperties)>
    KN_set_con_properties_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const int cProperty)>
    KN_set_con_property;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int objGoal)>
    KN_set_obj_goal;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const double* const xInitVals)>
    KN_set_var_primal_init_values;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const xInitVals)>
    KN_set_var_primal_init_values_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double xInitVal)>
    KN_set_var_primal_init_value;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const double* const lambdaInitVals)>
    KN_set_var_dual_init_values;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const lambdaInitVals)>
    KN_set_var_dual_init_values_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double lambdaInitVal)>
    KN_set_var_dual_init_value;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const lambdaInitVals)>
    KN_set_con_dual_init_values;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const lambdaInitVals)>
    KN_set_con_dual_init_values_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double lambdaInitVal)>
    KN_set_con_dual_init_value;
extern std::function<int KNITRO_API(KN_context_ptr kc, const double constant)>
    KN_add_obj_constant;
extern std::function<int KNITRO_API(KN_context_ptr kc)> KN_del_obj_constant;
extern std::function<int KNITRO_API(KN_context_ptr kc, const double constant)>
    KN_chg_obj_constant;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const constants)>
    KN_add_con_constants;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const constants)>
    KN_add_con_constants_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double constant)>
    KN_add_con_constant;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons)>
    KN_del_con_constants;
extern std::function<int KNITRO_API(KN_context_ptr kc)>
    KN_del_con_constants_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon)>
    KN_del_con_constant;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const constants)>
    KN_chg_con_constants;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const constants)>
    KN_chg_con_constants_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double constant)>
    KN_chg_con_constant;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nR,
                                    const KNINT* const indexRsds,
                                    const double* const constants)>
    KN_add_rsd_constants;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const constants)>
    KN_add_rsd_constants_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexRsd,
                                    const double constant)>
    KN_add_rsd_constant;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nnz,
                                    const KNINT* const indexVars,
                                    const double* const coefs)>
    KN_add_obj_linear_struct;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double coef)>
    KN_add_obj_linear_term;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nnz,
                                    const KNINT* const indexVars)>
    KN_del_obj_linear_struct;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar)>
    KN_del_obj_linear_term;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nnz,
                                    const KNINT* const indexVars,
                                    const double* const coefs)>
    KN_chg_obj_linear_struct;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double coef)>
    KN_chg_obj_linear_term;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT* const indexCons,
    const KNINT* const indexVars, const double* const coefs)>
    KN_add_con_linear_struct;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT indexCon,
    const KNINT* const indexVars, const double* const coefs)>
    KN_add_con_linear_struct_one;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const KNINT indexVar, const double coef)>
    KN_add_con_linear_term;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNLONG nnz,
                                    const KNINT* const indexCons,
                                    const KNINT* const indexVars)>
    KN_del_con_linear_struct;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNLONG nnz,
                                    const KNINT indexCon,
                                    const KNINT* const indexVars)>
    KN_del_con_linear_struct_one;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const KNINT indexVar)>
    KN_del_con_linear_term;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT* const indexCons,
    const KNINT* const indexVars, const double* const coefs)>
    KN_chg_con_linear_struct;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT indexCon,
    const KNINT* const indexVars, const double* const coefs)>
    KN_chg_con_linear_struct_one;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const KNINT indexVar, const double coef)>
    KN_chg_con_linear_term;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT* const indexRsds,
    const KNINT* const indexVars, const double* const coefs)>
    KN_add_rsd_linear_struct;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT indexRsd,
    const KNINT* const indexVars, const double* const coefs)>
    KN_add_rsd_linear_struct_one;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexRsd,
                                    const KNINT indexVar, const double coef)>
    KN_add_rsd_linear_term;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT* const indexVars1,
    const KNINT* const indexVars2, const double* const coefs)>
    KN_add_obj_quadratic_struct;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar1,
                                    const KNINT indexVar2, const double coef)>
    KN_add_obj_quadratic_term;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT* const indexCons,
    const KNINT* const indexVars1, const KNINT* const indexVars2,
    const double* const coefs)>
    KN_add_con_quadratic_struct;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNLONG nnz, const KNINT indexCon,
    const KNINT* const indexVars1, const KNINT* const indexVars2,
    const double* const coefs)>
    KN_add_con_quadratic_struct_one;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const KNINT indexVar1,
                                    const KNINT indexVar2, const double coef)>
    KN_add_con_quadratic_term;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT indexCon, const KNINT nCoords,
    const KNLONG nnz, const KNINT* const indexCoords,
    const KNINT* const indexVars, const double* const coefs,
    const double* const constants)>
    KN_add_con_L2norm;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT nCC, const int* const ccTypes,
    const KNINT* const indexComps1, const KNINT* const indexComps2)>
    KN_set_compcons;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const char* const filename)>
    KN_load_mps_file;
extern std::function<int KNITRO_API(KN_context_ptr kc, const char* filename)>
    KN_write_mps_file;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNBOOL evalObj, const KNINT nC,
    const KNINT* const indexCons, KN_eval_callback* const funcCallback,
    CB_context_ptr* const cb)>
    KN_add_eval_callback;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    KN_eval_callback* const funcCallback,
                                    CB_context_ptr* const cb)>
    KN_add_eval_callback_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT index,
                                    KN_eval_callback* const funcCallback,
                                    CB_context_ptr* const cb)>
    KN_add_eval_callback_one;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT nR, const KNINT* const indexRsds,
    KN_eval_callback* const rsdCallback, CB_context_ptr* const cb)>
    KN_add_lsq_eval_callback;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    KN_eval_callback* const rsdCallback,
                                    CB_context_ptr* const cb)>
    KN_add_lsq_eval_callback_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexRsd,
                                    KN_eval_callback* const rsdCallback,
                                    CB_context_ptr* const cb)>
    KN_add_lsq_eval_callback_one;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, CB_context_ptr cb, const KNINT nV,
    const KNINT* const objGradIndexVars, const KNLONG nnzJ,
    const KNINT* const jacIndexCons, const KNINT* const jacIndexVars,
    KN_eval_callback* const gradCallback)>
    KN_set_cb_grad;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, CB_context_ptr cb, const KNLONG nnzH,
    const KNINT* const hessIndexVars1, const KNINT* const hessIndexVars2,
    KN_eval_callback* const hessCallback)>
    KN_set_cb_hess;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, CB_context_ptr cb, const KNLONG nnzJ,
    const KNINT* const jacIndexRsds, const KNINT* const jacIndexVars,
    KN_eval_callback* const rsdJacCallback)>
    KN_set_cb_rsd_jac;
extern std::function<int KNITRO_API(KN_context_ptr kc, CB_context_ptr cb,
                                    void* const userParams)>
    KN_set_cb_user_params;
extern std::function<int KNITRO_API(KN_context_ptr kc, CB_context_ptr cb,
                                    const int gradopt)>
    KN_set_cb_gradopt;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, CB_context_ptr cb, const KNINT nV,
    const KNINT* const indexVars, const double* const xRelStepSizes)>
    KN_set_cb_relstepsizes;
extern std::function<int KNITRO_API(KN_context_ptr kc, CB_context_ptr cb,
                                    const double* const xRelStepSizes)>
    KN_set_cb_relstepsizes_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, CB_context_ptr cb,
                                    const KNINT indexVar,
                                    const double xRelStepSize)>
    KN_set_cb_relstepsize;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const CB_context_ptr cb, KNINT* const nC)>
    KN_get_cb_number_cons;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const CB_context_ptr cb, KNINT* const nR)>
    KN_get_cb_number_rsds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const CB_context_ptr cb, KNINT* const nnz)>
    KN_get_cb_objgrad_nnz;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const CB_context_ptr cb, KNLONG* const nnz)>
    KN_get_cb_jacobian_nnz;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const CB_context_ptr cb, KNLONG* const nnz)>
    KN_get_cb_rsd_jacobian_nnz;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const CB_context_ptr cb, KNLONG* const nnz)>
    KN_get_cb_hessian_nnz;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, KN_user_callback* const fnPtr, void* const userParams)>
    KN_set_newpt_callback;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, KN_user_callback* const fnPtr, void* const userParams)>
    KN_set_mip_node_callback;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, KN_user_callback* const fnPtr, void* const userParams)>
    KN_set_mip_usercuts_callback;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, KN_user_callback* const fnPtr, void* const userParams)>
    KN_set_mip_lazyconstraints_callback;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, KN_user_callback* const fnPtr, void* const userParams)>
    KN_set_ms_process_callback;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    KN_ms_initpt_callback* const fnPtr,
                                    void* const userParams)>
    KN_set_ms_initpt_callback;
extern std::function<int KNITRO_API(KN_context_ptr kc, KN_puts* const fnPtr,
                                    void* const userParams)>
    KN_set_puts_callback;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    KN_linsolver_callback* const fnPtr,
                                    void* const userParams)>
    KN_set_linsolver_callback;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT n, const double* const lobjCoefs,
    const double* const xLoBnds, const double* const xUpBnds, const KNINT m,
    const double* const cLoBnds, const double* const cUpBnds, const KNLONG nnzJ,
    const KNINT* const ljacIndexCons, const KNINT* const ljacIndexVars,
    const double* const ljacCoefs)>
    KN_load_lp;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT n, const double* const lobjCoefs,
    const double* const xLoBnds, const double* const xUpBnds, const KNINT m,
    const double* const cLoBnds, const double* const cUpBnds, const KNLONG nnzJ,
    const KNINT* const ljacIndexCons, const KNINT* const ljacIndexVars,
    const double* const ljacCoefs, const KNLONG nnzH,
    const KNINT* const qobjIndexVars1, const KNINT* const qobjIndexVars2,
    const double* const qobjCoefs)>
    KN_load_qp;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT n, const double* const lobjCoefs,
    const double* const xLoBnds, const double* const xUpBnds, const KNINT m,
    const double* const cLoBnds, const double* const cUpBnds, const KNLONG nnzJ,
    const KNINT* const ljacIndexCons, const KNINT* const ljacIndexVars,
    const double* const ljacCoefs, const KNLONG nnzH,
    const KNINT* const qobjIndexVars1, const KNINT* const qobjIndexVars2,
    const double* const qobjCoefs, const KNLONG nnzQ,
    const KNINT* const qconIndexCons, const KNINT* const qconIndexVars1,
    const KNINT* const qconIndexVars2, const double* const qconCoefs)>
    KN_load_qcqp;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const double* const xFeasTols)>
    KN_set_var_feastols;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const xFeasTols)>
    KN_set_var_feastols_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double xFeasTol)>
    KN_set_var_feastol;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const cFeasTols)>
    KN_set_con_feastols;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const cFeasTols)>
    KN_set_con_feastols_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double cFeasTol)>
    KN_set_con_feastol;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nCC,
                                    const KNINT* const indexCompCons,
                                    const double* const ccFeasTols)>
    KN_set_compcon_feastols;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const ccFeasTols)>
    KN_set_compcon_feastols_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCompCon,
                                    const double ccFeasTol)>
    KN_set_compcon_feastol;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT nV, const KNINT* const indexVars,
    const double* const xScaleFactors, const double* const xScaleCenters)>
    KN_set_var_scalings;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const xScaleFactors,
                                    const double* const xScaleCenters)>
    KN_set_var_scalings_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double xScaleFactor,
                                    const double xScaleCenter)>
    KN_set_var_scaling;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const double* const cScaleFactors)>
    KN_set_con_scalings;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const cScaleFactors)>
    KN_set_con_scalings_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const double cScaleFactor)>
    KN_set_con_scaling;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nCC,
                                    const KNINT* const indexCompCons,
                                    const double* const ccScaleFactors)>
    KN_set_compcon_scalings;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const ccScaleFactors)>
    KN_set_compcon_scalings_all;
extern std::function<int KNITRO_API(
    KN_context_ptr kc, const KNINT indexCompCons, const double ccScaleFactor)>
    KN_set_compcon_scaling;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double objScaleFactor)>
    KN_set_obj_scaling;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    char* const xNames[])>
    KN_set_var_names;
extern std::function<int KNITRO_API(KN_context_ptr kc, char* const xNames[])>
    KN_set_var_names_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVars,
                                    char* const xName)>
    KN_set_var_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    char* const cNames[])>
    KN_set_con_names;
extern std::function<int KNITRO_API(KN_context_ptr kc, char* const cNames[])>
    KN_set_con_names_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    char* const cName)>
    KN_set_con_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nCC,
                                    const KNINT* const indexCompCons,
                                    char* const ccNames[])>
    KN_set_compcon_names;
extern std::function<int KNITRO_API(KN_context_ptr kc, char* const ccNames[])>
    KN_set_compcon_names_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const int indexCompCon,
                                    char* const ccName)>
    KN_set_compcon_name;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const char* const objName)>
    KN_set_obj_name;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, const KNINT nV, const KNINT* const indexVars,
    const KNINT nBufferSize, char* const xNames[])>
    KN_get_var_names;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, const KNINT nBufferSize, char* const xNames[])>
    KN_get_var_names_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexVars,
                                    const KNINT nBufferSize, char* const xName)>
    KN_get_var_name;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, const KNINT nC, const KNINT* const indexCons,
    const KNINT nBufferSize, char* const cNames[])>
    KN_get_con_names;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, const KNINT nBufferSize, char* const cNames[])>
    KN_get_con_names_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexCons,
                                    const KNINT nBufferSize, char* const cName)>
    KN_get_con_name;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const int* const xHonorBnds)>
    KN_set_var_honorbnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const int* const xHonorBnds)>
    KN_set_var_honorbnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const int xHonorBnd)>
    KN_set_var_honorbnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    const int* const cHonorBnds)>
    KN_set_con_honorbnds;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const int* const cHonorBnds)>
    KN_set_con_honorbnds_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexCon,
                                    const int cHonorBnd)>
    KN_set_con_honorbnd;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const double* const xInitVals)>
    KN_set_mip_var_primal_init_values;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const double* const xInitVals)>
    KN_set_mip_var_primal_init_values_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const double xInitVal)>
    KN_set_mip_var_primal_init_value;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const int* const xPriorities)>
    KN_set_mip_branching_priorities;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const int* const xPriorities)>
    KN_set_mip_branching_priorities_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const int xPriority)>
    KN_set_mip_branching_priority;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    const int* const xStrategies)>
    KN_set_mip_intvar_strategies;
extern std::function<int KNITRO_API(KN_context_ptr kc,
                                    const int* const xStrategies)>
    KN_set_mip_intvar_strategies_all;
extern std::function<int KNITRO_API(KN_context_ptr kc, const KNINT indexVar,
                                    const int xStrategy)>
    KN_set_mip_intvar_strategy;
extern std::function<int KNITRO_API(KN_context_ptr kc)> KN_solve;
extern std::function<int KNITRO_API(KN_context_ptr kc)> KN_update;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNINT* const nV)>
    KN_get_number_vars;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNINT* const nC)>
    KN_get_number_cons;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNINT* const nCC)>
    KN_get_number_compcons;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNINT* const nR)>
    KN_get_number_rsds;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numFCevals)>
    KN_get_number_FC_evals;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numGAevals)>
    KN_get_number_GA_evals;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numHevals)>
    KN_get_number_H_evals;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numHVevals)>
    KN_get_number_HV_evals;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const time)>
    KN_get_solve_time_cpu;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const time)>
    KN_get_solve_time_real;
extern std::function<int KNITRO_API(const KN_context_ptr kc, int* const status,
                                    double* const obj, double* const x,
                                    double* const lambda)>
    KN_get_solution;
extern std::function<int KNITRO_API(const KN_context_ptr kc, double* const obj)>
    KN_get_obj_value;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const objType)>
    KN_get_obj_type;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    double* const x)>
    KN_get_var_primal_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc, double* const x)>
    KN_get_var_primal_values_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexVar, double* const x)>
    KN_get_var_primal_value;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV,
                                    const KNINT* const indexVars,
                                    double* const lambda)>
    KN_get_var_dual_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const lambda)>
    KN_get_var_dual_values_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexVar, double* const lambda)>
    KN_get_var_dual_value;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    double* const lambda)>
    KN_get_con_dual_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const lambda)>
    KN_get_con_dual_values_all;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, const KNINT indexCons, double* const lambda)>
    KN_get_con_dual_value;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    double* const c)>
    KN_get_con_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc, double* const c)>
    KN_get_con_values_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexCon, double* const c)>
    KN_get_con_value;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    int* const cTypes)>
    KN_get_con_types;
extern std::function<int KNITRO_API(const KN_context_ptr kc, int* const cTypes)>
    KN_get_con_types_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexCon, int* const cType)>
    KN_get_con_type;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nR,
                                    const KNINT* const indexRsds,
                                    double* const r)>
    KN_get_rsd_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc, double* const r)>
    KN_get_rsd_values_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexRsd, double* const r)>
    KN_get_rsd_value;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, const KNINT nV, const KNINT* const indexVars,
    KNINT* const bndInfeas, KNINT* const intInfeas, double* const viols)>
    KN_get_var_viols;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, KNINT* const bndInfeas, KNINT* const intInfeas,
    double* const viols)>
    KN_get_var_viols_all;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, const KNINT indexVar, KNINT* const bndInfeas,
    KNINT* const intInfeas, double* const viol)>
    KN_get_var_viol;
extern std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC,
                                    const KNINT* const indexCons,
                                    KNINT* const infeas, double* const viols)>
    KN_get_con_viols;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    KNINT* const infeas, double* const viols)>
    KN_get_con_viols_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    const KNINT indexCon, KNINT* const infeas,
                                    double* const viol)>
    KN_get_con_viol;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    KNINT* const component, KNINT* const index,
                                    KNINT* const error, double* const viol)>
    KN_get_presolve_error;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numIters)>
    KN_get_number_iters;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numCGiters)>
    KN_get_number_cg_iters;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const absFeasError)>
    KN_get_abs_feas_error;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const relFeasError)>
    KN_get_rel_feas_error;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const absOptError)>
    KN_get_abs_opt_error;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const relOptError)>
    KN_get_rel_opt_error;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNINT* const nnz)>
    KN_get_objgrad_nnz;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, KNINT* const indexVars, double* const objGrad)>
    KN_get_objgrad_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const objGrad)>
    KN_get_objgrad_values_all;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNLONG* const nnz)>
    KN_get_jacobian_nnz;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    KNINT* const indexCons,
                                    KNINT* const indexVars, double* const jac)>
    KN_get_jacobian_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNINT indexCon,
                                    KNINT* const nnz)>
    KN_get_jacobian_nnz_one;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNINT indexCon,
                                    KNINT* const indexVars, double* const jac)>
    KN_get_jacobian_values_one;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNLONG* const nnz)>
    KN_get_rsd_jacobian_nnz;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, KNINT* const indexRsds, KNINT* const indexVars,
    double* const rsdJac)>
    KN_get_rsd_jacobian_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc, KNLONG* const nnz)>
    KN_get_hessian_nnz;
extern std::function<int KNITRO_API(
    const KN_context_ptr kc, KNINT* const indexVars1, KNINT* const indexVars2,
    double* const hess)>
    KN_get_hessian_values;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numNodes)>
    KN_get_mip_number_nodes;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    int* const numSolves)>
    KN_get_mip_number_solves;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const absGap)>
    KN_get_mip_abs_gap;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const relGap)>
    KN_get_mip_rel_gap;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const incumbentObj)>
    KN_get_mip_incumbent_obj;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const relaxBound)>
    KN_get_mip_relaxation_bnd;
extern std::function<int KNITRO_API(const KN_context_ptr kc,
                                    double* const lastNodeObj)>
    KN_get_mip_lastnode_obj;
extern std::function<int KNITRO_API(const KN_context_ptr kc, double* const x)>
    KN_get_mip_incumbent_x;

}  // namespace operations_research

#endif  // OR_TOOLS_KNITRO_ENVIRONMENT_H_
