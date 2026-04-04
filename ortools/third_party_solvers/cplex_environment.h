// Copyright 2010-2026 Google LLC
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

#ifndef ORTOOLS_THIRD_PARTY_SOLVERS_CPLEX_ENVIRONMENT_H_
#define ORTOOLS_THIRD_PARTY_SOLVERS_CPLEX_ENVIRONMENT_H_

#include <functional>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace operations_research {

#ifndef CPXPUBLIC
#ifdef _WIN32
#define CPXPUBLIC __stdcall
#else
#define CPXPUBLIC
#endif
#endif

typedef const char* CPXCCHARptr; /* to simplify CPXPUBLIC syntax */

#define CPX_BIGINT 2100000000

#define CPX_INFBOUND 1.0E+20
#define CPX_MINBOUND 1.0E-13

#define CPX_MIN 1
#define CPX_MAX -1

#define CPX_NO_SOLN 0
#define CPX_BASIC_SOLN 1
#define CPX_NONBASIC_SOLN 2
#define CPX_PRIMAL_SOLN 3

// Solution Status Symbols
// -----------------------
#define CPX_STAT_ABORT_DETTIME_LIM 25
#define CPX_STAT_ABORT_DUAL_OBJ_LIM 22
#define CPX_STAT_ABORT_IT_LIM 10
#define CPX_STAT_ABORT_OBJ_LIM 12
#define CPX_STAT_ABORT_PRIM_OBJ_LIM 21
#define CPX_STAT_ABORT_TIME_LIM 11
#define CPX_STAT_ABORT_USER 13
#define CPX_STAT_BENDERS_MASTER_UNBOUNDED 40
#define CPX_STAT_BENDERS_NUM_BEST 41
#define CPX_STAT_CONFLICT_ABORT_CONTRADICTION 32
#define CPX_STAT_CONFLICT_ABORT_DETTIME_LIM 39
#define CPX_STAT_CONFLICT_ABORT_IT_LIM 34
#define CPX_STAT_CONFLICT_ABORT_MEM_LIM 37
#define CPX_STAT_CONFLICT_ABORT_NODE_LIM 35
#define CPX_STAT_CONFLICT_ABORT_OBJ_LIM 36
#define CPX_STAT_CONFLICT_ABORT_TIME_LIM 33
#define CPX_STAT_CONFLICT_ABORT_USER 38
#define CPX_STAT_CONFLICT_FEASIBLE 30
#define CPX_STAT_CONFLICT_MINIMAL 31
#define CPX_STAT_FEASIBLE 23
#define CPX_STAT_FEASIBLE_RELAXED_INF 16
#define CPX_STAT_FEASIBLE_RELAXED_QUAD 18
#define CPX_STAT_FEASIBLE_RELAXED_SUM 14
#define CPX_STAT_FIRSTORDER 24
#define CPX_STAT_INFEASIBLE 3
#define CPX_STAT_INForUNBD 4
#define CPX_STAT_MULTIOBJ_INFEASIBLE 302
#define CPX_STAT_MULTIOBJ_INForUNBD 303
#define CPX_STAT_MULTIOBJ_NON_OPTIMAL 305
#define CPX_STAT_MULTIOBJ_OPTIMAL 301
#define CPX_STAT_MULTIOBJ_STOPPED 306
#define CPX_STAT_MULTIOBJ_UNBOUNDED 304
#define CPX_STAT_NUM_BEST 6
#define CPX_STAT_OPTIMAL 1
#define CPX_STAT_OPTIMAL_FACE_UNBOUNDED 20
#define CPX_STAT_OPTIMAL_INFEAS 5
#define CPX_STAT_OPTIMAL_RELAXED_INF 17
#define CPX_STAT_OPTIMAL_RELAXED_QUAD 19
#define CPX_STAT_OPTIMAL_RELAXED_SUM 15
#define CPX_STAT_UNBOUNDED 2

// MIP prefix
#define CPXMIP_ABORT_FEAS 113
#define CPXMIP_ABORT_INFEAS 114
#define CPXMIP_ABORT_RELAXATION_UNBOUNDED 133
#define CPXMIP_ABORT_RELAXED 126
#define CPXMIP_DETTIME_LIM_FEAS 131
#define CPXMIP_DETTIME_LIM_INFEAS 132
#define CPXMIP_FAIL_FEAS 109
#define CPXMIP_FAIL_FEAS_NO_TREE 116
#define CPXMIP_FAIL_INFEAS 110
#define CPXMIP_FAIL_INFEAS_NO_TREE 117
#define CPXMIP_FEASIBLE 127
#define CPXMIP_FEASIBLE_RELAXED_INF 122
#define CPXMIP_FEASIBLE_RELAXED_QUAD 124
#define CPXMIP_FEASIBLE_RELAXED_SUM 120
#define CPXMIP_INFEASIBLE 103
#define CPXMIP_INForUNBD 119
#define CPXMIP_MEM_LIM_FEAS 111
#define CPXMIP_MEM_LIM_INFEAS 112
#define CPXMIP_NODE_LIM_FEAS 105
#define CPXMIP_NODE_LIM_INFEAS 106
#define CPXMIP_OPTIMAL 101
#define CPXMIP_OPTIMAL_INFEAS 115
#define CPXMIP_OPTIMAL_POPULATED 129
#define CPXMIP_OPTIMAL_POPULATED_TOL 130
#define CPXMIP_OPTIMAL_RELAXED_INF 123
#define CPXMIP_OPTIMAL_RELAXED_QUAD 125
#define CPXMIP_OPTIMAL_RELAXED_SUM 121
#define CPXMIP_OPTIMAL_TOL 102
#define CPXMIP_POPULATESOL_LIM 128
#define CPXMIP_SOL_LIM 104
#define CPXMIP_TIME_LIM_FEAS 107
#define CPXMIP_TIME_LIM_INFEAS 108
#define CPXMIP_UNBOUNDED 118

#define CPXPROB_LP 0
#define CPXPROB_MILP 1
#define CPXPROB_FIXEDMILP 3
#define CPXPROB_NODELP 4
#define CPXPROB_QP 5
#define CPXPROB_MIQP 7
#define CPXPROB_FIXEDMIQP 8
#define CPXPROB_NODEQP 9
#define CPXPROB_QCP 10
#define CPXPROB_MIQCP 11
#define CPXPROB_NODEQCP 12

#define CPXPARAM_Simplex_Limits_LowerObj 1025
#define CPXPARAM_Simplex_Limits_UpperObj 1026
#define CPXPARAM_Read_Scale 1034
#define CPXPARAM_ScreenOutput 1035
#define CPXPARAM_TimeLimit 1039
#define CPXPARAM_LPMethod 1062
#define CPXPARAM_Threads 1067
// #define CPXPARAM_Emphasis_Numerical 1083
#define CPXPARAM_RandomSeed 1124
#define CPXPARAM_ParamDisplay 1163
#define CPXPARAM_MIP_Tolerances_LowerCutoff 2006
#define CPXPARAM_MIP_Tolerances_UpperCutoff 2007
#define CPXPARAM_MIP_Tolerances_AbsMIPGap 2008
#define CPXPARAM_MIP_Tolerances_MIPGap 2009
#define CPXPARAM_MIP_Limits_Solutions 2015
#define CPXPARAM_MIP_Limits_Nodes 2017
#define CPXPARAM_MIP_Pool_Capacity 2103

#define CPX_ALG_PRIMAL 1
#define CPX_ALG_DUAL 2
#define CPX_ALG_NET 3
#define CPX_ALG_BARRIER 4

#define CPXPARAM_MIP_Cuts_BQP 2195
#define CPXPARAM_MIP_Cuts_Cliques 2003
#define CPXPARAM_MIP_Cuts_Covers 2005
#define CPXPARAM_MIP_Cuts_Disjunctive 2053
#define CPXPARAM_MIP_Cuts_FlowCovers 2040
#define CPXPARAM_MIP_Cuts_PathCut 2051
#define CPXPARAM_MIP_Cuts_Gomory 2049
#define CPXPARAM_MIP_Cuts_GUBCovers 2044
#define CPXPARAM_MIP_Cuts_Implied 2041
#define CPXPARAM_MIP_Cuts_LocalImplied 2181
#define CPXPARAM_MIP_Cuts_LiftProj 2152
#define CPXPARAM_MIP_Cuts_MIRCut 2052
#define CPXPARAM_MIP_Cuts_ZeroHalfCut 2111
#define CPXPARAM_MIP_Cut_MCFCut 2134
#define CPXPARAM_MIP_Cuts_Nodecuts 2157
#define CPXPARAM_MIP_Cuts_RLT 2196

#define CPXPARAM_MIP_Strategy_HeuristicEffort 2120

#define CPXPARAM_Preprocessing_Presolve 1030
#define CPXPARAM_MIP_Strategy_Probe 2042
#define CPXPARAM_Preprocessing_RepeatPresolve 2064

#define CPXPARAM_Simplex_Limits_Iterations 1020
#define CPXPARAM_Barrier_Limits_Iteration 3012

#define CPXERR_SUBPROB_SOLVE 3019

#define CPXMESSAGEBUFSIZE 1024

#define CPX_ON 1
#define CPX_OFF 0

#ifndef CPXLONG_DEFINED
#define CPXLONG_DEFINED 1
#ifdef _MSC_VER
typedef __int64 CPXLONG;
#else
typedef long long CPXLONG;
#endif
#endif

// ***************************************************************************
// * variable types                                                          *
// ***************************************************************************
#define CPX_CONTINUOUS 'C'
#define CPX_INTEGER 'I'

struct cpxenv;
typedef struct cpxenv* CPXENVptr;
typedef struct cpxenv const* CPXCENVptr;

struct cpxlp;
typedef struct cpxlp* CPXLPptr;
#ifndef CPXCLPptr
typedef const struct cpxlp* CPXCLPptr;
#endif

struct cpxchannel;
typedef struct cpxchannel* CPXCHANNELptr;

// Force the loading of the cplex dynamic library. It returns true if the
// library was successfully loaded. This method can only be called once.
// Successive calls are no-op.
absl::Status LoadCplexDynamicLibrary(std::string& cplexpath);

// clang-format off

extern std::function<CPXENVptr(int *status_p)> CPXopenCPLEX;
extern std::function<int(CPXENVptr *env_p)> CPXcloseCPLEX;

extern std::function<CPXLPptr(CPXCENVptr env, int * status_p, char const * probname_str)> CPXcreateprob;
extern std::function<int(CPXCENVptr env, CPXLPptr * lp_p)> CPXfreeprob;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, char const *probname)> CPXchgprobname;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int ccnt,
                         double const *obj, double const *lb, double const *ub,
                         char const *xctype, char **colname)> CPXnewcols;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt,
                         int const *indices, double const *values)> CPXchgrngval;

extern std::function<int(CPXCENVptr env, CPXLPptr lp)> CPXlpopt;

extern std::function<int(CPXCENVptr env, CPXLPptr lp)> CPXmipopt;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp)> CPXgetstat;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, double *objval_p)> CPXgetobjval;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, double *objval_p)> CPXgetbestobjval;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, double *x, int begin, int end)> CPXgetx;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, double *pi, int begin, int end)> CPXgetpi;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, double *dj, int begin, int end)> CPXgetdj;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp)> CPXgetnumcols;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp)> CPXgetnumrows;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int maxormin)> CPXchgobjsen;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, double offset)> CPXchgobjoffset;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, double const * values)> CPXchgobj;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp)> CPXgetprobtype;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, int *solnmethod_p, int *solntype_p, int *pfeasind_p, int *dfeasind_p)> CPXsolninfo;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp)> CPXgetobjsen;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int numcoefs, int const *rowlist, int const *collist, double const *vallist)> CPXchgcoeflist;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const *indices, char const *lu, double const *bd)> CPXchgbds;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, char const * xctype)> CPXchgctype;

extern std::function<int(CPXCENVptr env, int errcode, char * buffer_str)> CPXgeterrorstring;

extern std::function<int(CPXCENVptr env, CPXLPptr lp)> CPXgetitcnt;

extern std::function<int(CPXCENVptr env, CPXLPptr lp)> CPXgetmipitcnt;

extern std::function<int(CPXCENVptr env, CPXLPptr lp)> CPXgetbaritcnt;

extern std::function<int(CPXCENVptr env, CPXLPptr lp)> CPXgetnodecnt;

extern std::function<int(CPXCENVptr env, CPXLPptr lp)> CPXgetsolnpoolnumsolns;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int soln, double * objval_p)> CPXgetsolnpoolobjval;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, int soln, double * x, int begin, int end)> CPXgetsolnpoolx;

extern std::function<int(CPXCENVptr env, int whichparam, double newvalue)> CPXsetdblparam;

extern std::function<int(CPXCENVptr env, int whichparam, int newvalue)> CPXsetintparam;

extern std::function<int(CPXCENVptr env, int whichparam, CPXLONG newvalue)> CPXsetlongparam;

extern std::function<int(CPXCENVptr env, int whichparam, char const *)> CPXsetstrparam;

extern std::function<int(CPXENVptr env)> CPXsetdefaults;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, char const * sense)> CPXchgsense;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int cnt, int const * indices, double const * values)> CPXchgrhs;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int *delstat)> CPXdelsetcols;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int *delstat)> CPXdelsetrows;

extern std::function<int(CPXCENVptr env, char const * name_str, int * whichparam_p)> CPXgetparamnum;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, double * lb, int begin, int end)> CPXgetlb;

extern std::function<int(CPXCENVptr env, CPXCLPptr lp, double * lb, int begin, int end)> CPXgetub;

extern std::function<int(CPXENVptr env, volatile int * terminate_p )> CPXsetterminate;

extern std::function<int(CPXCENVptr env, CPXCHANNELptr * cpxresults_p, CPXCHANNELptr * cpxwarning_p, CPXCHANNELptr * cpxerror_p, CPXCHANNELptr * cpxlog_p)> CPXgetchannels;

extern std::function<int(CPXCENVptr env, CPXCHANNELptr channel, void * handle, void(CPXPUBLIC *msgfunction)(void *, const char *))> CPXaddfuncdest;

extern std::function<int(CPXCENVptr env, CPXCHANNELptr channel, void * handle, void(CPXPUBLIC *msgfunction)(void *, const char *))> CPXdelfuncdest;

extern std::function<int(CPXCENVptr env, CPXLPptr lp, int rcnt, double const * rhs, char const * sense, double const * rngval, char ** rowname)> CPXnewrows;

extern std::function<CPXCCHARptr(CPXCENVptr env)> CPXversion;

// clang-format on
}  // namespace operations_research

#endif  // ORTOOLS_THIRD_PARTY_SOLVERS_CPLEX_ENVIRONMENT_H_
