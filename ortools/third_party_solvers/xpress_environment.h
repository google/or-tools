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

// Initial version of this code was provided by RTE

#ifndef ORTOOLS_THIRD_PARTY_SOLVERS_XPRESS_ENVIRONMENT_H_
#define ORTOOLS_THIRD_PARTY_SOLVERS_XPRESS_ENVIRONMENT_H_

#include <functional>
#include <string>

#include "absl/status/status.h"
#include "ortools/base/base_export.h"

extern "C" {
typedef struct xo_prob_struct* XPRSprob;
}

namespace operations_research {

void printXpressBanner(bool error);

bool initXpressEnv(bool verbose = true, int xpress_oem_license_key = 0);

bool XpressIsCorrectlyInstalled();

// Force the loading of the xpress dynamic library. It returns true if the
// library was successfully loaded. This method can only be called once.
// Successive calls are no-op.
//
// Note that it does not check if a token license can be grabbed.
absl::Status LoadXpressDynamicLibrary(std::string& xpresspath);

// The list of #define and extern std::function<> below is generated directly
// from xprs.h via parse_header_xpress.py
// See the top comment on the parse_header_xpress.py file.
// This is the header section
// NOLINTBEGIN(runtime/int)
#if defined(_WIN32)
#define XPRSint64 __int64
#elif defined(__LP64__) || defined(_LP64) || defined(__ILP64__) || \
    defined(_ILP64)
#define XPRSint64 long
#else
#define XPRSint64 long long
#endif
// NOLINTEND(runtime/int)

#if defined(_MSC_VER)
#define XPRS_CC __stdcall
#else
#define XPRS_CC
#endif
// ***************************************************************************
// * values related to XPRSinterrupt                                         *
// ***************************************************************************
#define XPRS_STOP_NONE 0
#define XPRS_STOP_TIMELIMIT 1
#define XPRS_STOP_CTRLC 2
#define XPRS_STOP_NODELIMIT 3
#define XPRS_STOP_ITERLIMIT 4
#define XPRS_STOP_MIPGAP 5
#define XPRS_STOP_SOLLIMIT 6
#define XPRS_STOP_GENERICERROR 7
#define XPRS_STOP_MEMORYERROR 8
#define XPRS_STOP_USER 9
#define XPRS_STOP_SOLVECOMPLETE 10
#define XPRS_STOP_LICENSELOST 11
#define XPRS_STOP_NUMERICALERROR 13
#define XPRS_STOP_WORKLIMIT 14
// ***************************************************************************
// * values related to Set/GetControl/Attribinfo                             *
// ***************************************************************************
#define XPRS_TYPE_NOTDEFINED 0
#define XPRS_TYPE_INT 1
#define XPRS_TYPE_INT64 2
#define XPRS_TYPE_DOUBLE 3
#define XPRS_TYPE_STRING 4
// ***************************************************************************
// * values related to NAMESPACES                                            *
// ***************************************************************************
#define XPRS_NAMES_ROW 1
#define XPRS_NAMES_COLUMN 2
// ***************************************************************************
// * values related to SOLAVAILABLE                                          *
// ***************************************************************************
#define XPRS_SOLAVAILABLE_NOTFOUND 0
#define XPRS_SOLAVAILABLE_OPTIMAL 1
#define XPRS_SOLAVAILABLE_FEASIBLE 2
// ***************************************************************************
// * values related to SOLVESTATUS                                           *
// ***************************************************************************
#define XPRS_SOLVESTATUS_UNSTARTED 0
#define XPRS_SOLVESTATUS_STOPPED 1
#define XPRS_SOLVESTATUS_FAILED 2
#define XPRS_SOLVESTATUS_COMPLETED 3
// ***************************************************************************
// * values related to DEFAULTALG and ALGORITHM                              *
// ***************************************************************************
#define XPRS_ALG_DEFAULT 1
#define XPRS_ALG_DUAL 2
#define XPRS_ALG_PRIMAL 3
#define XPRS_ALG_BARRIER 4
#define XPRS_ALG_NETWORK 5

#define XPRS_PLUSINFINITY 1.0e+20
#define XPRS_MINUSINFINITY -1.0e+20
#define XPRS_MAXBANNERLENGTH 512
#define XPVERSION 45  // >= 45 for XPRS_SOLAVAILABLE flags, XPRSgetduals(), etc.
#define XPRS_MIPENTS 1032
#define XPRS_ALGORITHM 1049
#define XPRS_STOPSTATUS 1179
#define XPRS_SOLVESTATUS 1394
#define XPRS_OBJECTIVES 1397
#define XPRS_SOLVEDOBJS 1399
#define XPRS_ORIGINALCOLS 1214
#define XPRS_ORIGINALROWS 1124
#define XPRS_ORIGINALMIPENTS 1191
#define XPRS_ORIGINALSETS 1194
#define XPRS_OBJVAL 2118
#define XPRS_BARPRIMALOBJ 4001
#define XPRS_BARDUALOBJ 4002
#define XPRS_MPSRHSNAME 6001
#define XPRS_MPSOBJNAME 6002
#define XPRS_MPSRANGENAME 6003
#define XPRS_MPSBOUNDNAME 6004
#define XPRS_OUTPUTMASK 6005
#define XPRS_TUNERMETHODFILE 6017
#define XPRS_TUNEROUTPUTPATH 6018
#define XPRS_TUNERSESSIONNAME 6019
#define XPRS_COMPUTEEXECSERVICE 6022
#define XPRS_MAXCUTTIME 8149
#define XPRS_MAXSTALLTIME 8443
#define XPRS_TUNERMAXTIME 8364
#define XPRS_MATRIXTOL 7001
#define XPRS_PIVOTTOL 7002
#define XPRS_FEASTOL 7003
#define XPRS_OUTPUTTOL 7004
#define XPRS_SOSREFTOL 7005
#define XPRS_OPTIMALITYTOL 7006
#define XPRS_ETATOL 7007
#define XPRS_RELPIVOTTOL 7008
#define XPRS_MIPTOL 7009
#define XPRS_MIPTOLTARGET 7010
#define XPRS_BARPERTURB 7011
#define XPRS_MIPADDCUTOFF 7012
#define XPRS_MIPABSCUTOFF 7013
#define XPRS_MIPRELCUTOFF 7014
#define XPRS_PSEUDOCOST 7015
#define XPRS_PENALTY 7016
#define XPRS_BIGM 7018
#define XPRS_MIPABSSTOP 7019
#define XPRS_MIPRELSTOP 7020
#define XPRS_CROSSOVERACCURACYTOL 7023
#define XPRS_PRIMALPERTURB 7024
#define XPRS_DUALPERTURB 7025
#define XPRS_BAROBJSCALE 7026
#define XPRS_BARRHSSCALE 7027
#define XPRS_CHOLESKYTOL 7032
#define XPRS_BARGAPSTOP 7033
#define XPRS_BARDUALSTOP 7034
#define XPRS_BARPRIMALSTOP 7035
#define XPRS_BARSTEPSTOP 7036
#define XPRS_ELIMTOL 7042
#define XPRS_MARKOWITZTOL 7047
#define XPRS_MIPABSGAPNOTIFY 7064
#define XPRS_MIPRELGAPNOTIFY 7065
#define XPRS_BARLARGEBOUND 7067
#define XPRS_PPFACTOR 7069
#define XPRS_REPAIRINDEFINITEQMAX 7071
#define XPRS_BARGAPTARGET 7073
#define XPRS_DUMMYCONTROL 7075
#define XPRS_BARSTARTWEIGHT 7076
#define XPRS_BARFREESCALE 7077
#define XPRS_SBEFFORT 7086
#define XPRS_HEURDIVERANDOMIZE 7089
#define XPRS_HEURSEARCHEFFORT 7090
#define XPRS_CUTFACTOR 7091
#define XPRS_EIGENVALUETOL 7097
#define XPRS_INDLINBIGM 7099
#define XPRS_TREEMEMORYSAVINGTARGET 7100
#define XPRS_INDPRELINBIGM 7102
#define XPRS_RELAXTREEMEMORYLIMIT 7105
#define XPRS_MIPABSGAPNOTIFYOBJ 7108
#define XPRS_MIPABSGAPNOTIFYBOUND 7109
#define XPRS_PRESOLVEMAXGROW 7110
#define XPRS_HEURSEARCHTARGETSIZE 7112
#define XPRS_CROSSOVERRELPIVOTTOL 7113
#define XPRS_CROSSOVERRELPIVOTTOLSAFE 7114
#define XPRS_DETLOGFREQ 7116
#define XPRS_MAXIMPLIEDBOUND 7120
#define XPRS_FEASTOLTARGET 7121
#define XPRS_OPTIMALITYTOLTARGET 7122
#define XPRS_PRECOMPONENTSEFFORT 7124
#define XPRS_LPLOGDELAY 7127
#define XPRS_HEURDIVEITERLIMIT 7128
#define XPRS_BARKERNEL 7130
#define XPRS_FEASTOLPERTURB 7132
#define XPRS_CROSSOVERFEASWEIGHT 7133
#define XPRS_LUPIVOTTOL 7139
#define XPRS_MIPRESTARTGAPTHRESHOLD 7140
#define XPRS_NODEPROBINGEFFORT 7141
#define XPRS_INPUTTOL 7143
#define XPRS_MIPRESTARTFACTOR 7145
#define XPRS_BAROBJPERTURB 7146
#define XPRS_CPIALPHA 7149
#define XPRS_GLOBALBOUNDINGBOX 7154
#define XPRS_TIMELIMIT 7158
#define XPRS_SOLTIMELIMIT 7159
#define XPRS_REPAIRINFEASTIMELIMIT 7160
#define XPRS_EXTRAROWS 8004
#define XPRS_EXTRACOLS 8005
#define XPRS_LPITERLIMIT 8007
#define XPRS_LPLOG 8009
#define XPRS_SCALING 8010
#define XPRS_PRESOLVE 8011
#define XPRS_CRASH 8012
#define XPRS_PRICINGALG 8013
#define XPRS_INVERTFREQ 8014
#define XPRS_INVERTMIN 8015
#define XPRS_MAXNODE 8018
#define XPRS_MAXTIME 8020
#define XPRS_MAXMIPSOL 8021
#define XPRS_SIFTPASSES 8022
#define XPRS_DEFAULTALG 8023
#define XPRS_VARSELECTION 8025
#define XPRS_NODESELECTION 8026
#define XPRS_BACKTRACK 8027
#define XPRS_MIPLOG 8028
#define XPRS_KEEPNROWS 8030
#define XPRS_MPSECHO 8032
#define XPRS_MAXPAGELINES 8034
#define XPRS_OUTPUTLOG 8035
#define XPRS_BARSOLUTION 8038
#define XPRS_CACHESIZE 8043
#define XPRS_CROSSOVER 8044
#define XPRS_BARITERLIMIT 8045
#define XPRS_CHOLESKYALG 8046
#define XPRS_BAROUTPUT 8047
#define XPRS_EXTRAMIPENTS 8051
#define XPRS_REFACTOR 8052
#define XPRS_BARTHREADS 8053
#define XPRS_KEEPBASIS 8054
#define XPRS_CROSSOVEROPS 8060
#define XPRS_VERSION 8061
#define XPRS_CROSSOVERTHREADS 8065
#define XPRS_BIGMMETHOD 8068
#define XPRS_MPSNAMELENGTH 8071
#define XPRS_ELIMFILLIN 8073
#define XPRS_PRESOLVEOPS 8077
#define XPRS_MIPPRESOLVE 8078
#define XPRS_MIPTHREADS 8079
#define XPRS_BARORDER 8080
#define XPRS_BREADTHFIRST 8082
#define XPRS_AUTOPERTURB 8084
#define XPRS_DENSECOLLIMIT 8086
#define XPRS_CALLBACKFROMMASTERTHREAD 8090
#define XPRS_MAXMCOEFFBUFFERELEMS 8091
#define XPRS_REFINEOPS 8093
#define XPRS_LPREFINEITERLIMIT 8094
#define XPRS_MIPREFINEITERLIMIT 8095
#define XPRS_DUALIZEOPS 8097
#define XPRS_CROSSOVERITERLIMIT 8104
#define XPRS_PREBASISRED 8106
#define XPRS_PRESORT 8107
#define XPRS_PREPERMUTE 8108
#define XPRS_PREPERMUTESEED 8109
#define XPRS_MAXMEMORYSOFT 8112
#define XPRS_CUTFREQ 8116
#define XPRS_SYMSELECT 8117
#define XPRS_SYMMETRY 8118
#define XPRS_MAXMEMORYHARD 8119
#define XPRS_MIQCPALG 8125
#define XPRS_QCCUTS 8126
#define XPRS_QCROOTALG 8127
#define XPRS_PRECONVERTSEPARABLE 8128
#define XPRS_ALGAFTERNETWORK 8129
#define XPRS_TRACE 8130
#define XPRS_MAXIIS 8131
#define XPRS_CPUTIME 8133
#define XPRS_COVERCUTS 8134
#define XPRS_GOMCUTS 8135
#define XPRS_LPFOLDING 8136
#define XPRS_MPSFORMAT 8137
#define XPRS_CUTSTRATEGY 8138
#define XPRS_CUTDEPTH 8139
#define XPRS_TREECOVERCUTS 8140
#define XPRS_TREEGOMCUTS 8141
#define XPRS_CUTSELECT 8142
#define XPRS_TREECUTSELECT 8143
#define XPRS_DUALIZE 8144
#define XPRS_DUALGRADIENT 8145
#define XPRS_SBITERLIMIT 8146
#define XPRS_SBBEST 8147
#define XPRS_BARINDEFLIMIT 8153
#define XPRS_HEURFREQ 8155
#define XPRS_HEURDEPTH 8156
#define XPRS_HEURMAXSOL 8157
#define XPRS_HEURNODES 8158
#define XPRS_LNPBEST 8160
#define XPRS_LNPITERLIMIT 8161
#define XPRS_BRANCHCHOICE 8162
#define XPRS_BARREGULARIZE 8163
#define XPRS_SBSELECT 8164
#define XPRS_LOCALCHOICE 8170
#define XPRS_LOCALBACKTRACK 8171
#define XPRS_DUALSTRATEGY 8174
#define XPRS_L1CACHE 8175
#define XPRS_HEURDIVESTRATEGY 8177
#define XPRS_HEURSELECT 8178
#define XPRS_BARSTART 8180
#define XPRS_PRESOLVEPASSES 8183
#define XPRS_BARNUMSTABILITY 8186
#define XPRS_BARORDERTHREADS 8187
#define XPRS_EXTRASETS 8190
#define XPRS_FEASIBILITYPUMP 8193
#define XPRS_PRECOEFELIM 8194
#define XPRS_PREDOMCOL 8195
#define XPRS_HEURSEARCHFREQ 8196
#define XPRS_HEURDIVESPEEDUP 8197
#define XPRS_SBESTIMATE 8198
#define XPRS_BARCORES 8202
#define XPRS_MAXCHECKSONMAXTIME 8203
#define XPRS_MAXCHECKSONMAXCUTTIME 8204
#define XPRS_HISTORYCOSTS 8206
#define XPRS_ALGAFTERCROSSOVER 8208
#define XPRS_MUTEXCALLBACKS 8210
#define XPRS_BARCRASH 8211
#define XPRS_HEURDIVESOFTROUNDING 8215
#define XPRS_HEURSEARCHROOTSELECT 8216
#define XPRS_HEURSEARCHTREESELECT 8217
#define XPRS_MPS18COMPATIBLE 8223
#define XPRS_ROOTPRESOLVE 8224
#define XPRS_CROSSOVERDRP 8227
#define XPRS_FORCEOUTPUT 8229
#define XPRS_PRIMALOPS 8231
#define XPRS_DETERMINISTIC 8232
#define XPRS_PREPROBING 8238
#define XPRS_TREEMEMORYLIMIT 8242
#define XPRS_TREECOMPRESSION 8243
#define XPRS_TREEDIAGNOSTICS 8244
#define XPRS_MAXTREEFILESIZE 8245
#define XPRS_PRECLIQUESTRATEGY 8247
#define XPRS_REPAIRINFEASMAXTIME 8250
#define XPRS_IFCHECKCONVEXITY 8251
#define XPRS_PRIMALUNSHIFT 8252
#define XPRS_REPAIRINDEFINITEQ 8254
#define XPRS_MIPRAMPUP 8255
#define XPRS_MAXLOCALBACKTRACK 8257
#define XPRS_USERSOLHEURISTIC 8258
#define XPRS_FORCEPARALLELDUAL 8265
#define XPRS_BACKTRACKTIE 8266
#define XPRS_BRANCHDISJ 8267
#define XPRS_MIPFRACREDUCE 8270
#define XPRS_CONCURRENTTHREADS 8274
#define XPRS_MAXSCALEFACTOR 8275
#define XPRS_HEURTHREADS 8276
#define XPRS_THREADS 8278
#define XPRS_HEURBEFORELP 8280
#define XPRS_PREDOMROW 8281
#define XPRS_BRANCHSTRUCTURAL 8282
#define XPRS_QUADRATICUNSHIFT 8284
#define XPRS_BARPRESOLVEOPS 8286
#define XPRS_QSIMPLEXOPS 8288
#define XPRS_MIPRESTART 8290
#define XPRS_CONFLICTCUTS 8292
#define XPRS_PREPROTECTDUAL 8293
#define XPRS_CORESPERCPU 8296
#define XPRS_RESOURCESTRATEGY 8297
#define XPRS_CLAMPING 8301
#define XPRS_SLEEPONTHREADWAIT 8302
#define XPRS_PREDUPROW 8307
#define XPRS_CPUPLATFORM 8312
#define XPRS_BARALG 8315
#define XPRS_SIFTING 8319
#define XPRS_LPLOGSTYLE 8326
#define XPRS_RANDOMSEED 8328
#define XPRS_TREEQCCUTS 8331
#define XPRS_PRELINDEP 8333
#define XPRS_DUALTHREADS 8334
#define XPRS_PREOBJCUTDETECT 8336
#define XPRS_PREBNDREDQUAD 8337
#define XPRS_PREBNDREDCONE 8338
#define XPRS_PRECOMPONENTS 8339
#define XPRS_MAXMIPTASKS 8347
#define XPRS_MIPTERMINATIONMETHOD 8348
#define XPRS_PRECONEDECOMP 8349
#define XPRS_HEURFORCESPECIALOBJ 8350
#define XPRS_HEURSEARCHROOTCUTFREQ 8351
#define XPRS_PREELIMQUAD 8353
#define XPRS_PREIMPLICATIONS 8356
#define XPRS_TUNERMODE 8359
#define XPRS_TUNERMETHOD 8360
#define XPRS_TUNERTARGET 8362
#define XPRS_TUNERTHREADS 8363
#define XPRS_TUNERHISTORY 8365
#define XPRS_TUNERPERMUTE 8366
#define XPRS_TUNERVERBOSE 8370
#define XPRS_TUNEROUTPUT 8372
#define XPRS_PREANALYTICCENTER 8374
#define XPRS_NETCUTS 8382
#define XPRS_LPFLAGS 8385
#define XPRS_MIPKAPPAFREQ 8386
#define XPRS_OBJSCALEFACTOR 8387
#define XPRS_TREEFILELOGINTERVAL 8389
#define XPRS_IGNORECONTAINERCPULIMIT 8390
#define XPRS_IGNORECONTAINERMEMORYLIMIT 8391
#define XPRS_MIPDUALREDUCTIONS 8392
#define XPRS_GENCONSDUALREDUCTIONS 8395
#define XPRS_PWLDUALREDUCTIONS 8396
#define XPRS_BARFAILITERLIMIT 8398
#define XPRS_AUTOSCALING 8406
#define XPRS_GENCONSABSTRANSFORMATION 8408
#define XPRS_COMPUTEJOBPRIORITY 8409
#define XPRS_PREFOLDING 8410
#define XPRS_NETSTALLLIMIT 8412
#define XPRS_SERIALIZEPREINTSOL 8413
#define XPRS_NUMERICALEMPHASIS 8416
#define XPRS_PWLNONCONVEXTRANSFORMATION 8420
#define XPRS_MIPCOMPONENTS 8421
#define XPRS_MIPCONCURRENTNODES 8422
#define XPRS_MIPCONCURRENTSOLVES 8423
#define XPRS_OUTPUTCONTROLS 8424
#define XPRS_SIFTSWITCH 8425
#define XPRS_HEUREMPHASIS 8427
#define XPRS_COMPUTEMATX 8428
#define XPRS_COMPUTEMATX_IIS 8429
#define XPRS_COMPUTEMATX_IISMAXTIME 8430
#define XPRS_BARREFITER 8431
#define XPRS_COMPUTELOG 8434
#define XPRS_SIFTPRESOLVEOPS 8435
#define XPRS_CHECKINPUTDATA 8436
#define XPRS_ESCAPENAMES 8440
#define XPRS_IOTIMEOUT 8442
#define XPRS_AUTOCUTTING 8446
#define XPRS_CALLBACKCHECKTIMEDELAY 8451
#define XPRS_MULTIOBJOPS 8457
#define XPRS_MULTIOBJLOG 8458
#define XPRS_GLOBALSPATIALBRANCHIFPREFERORIG 8465
#define XPRS_PRECONFIGURATION 8470
#define XPRS_FEASIBILITYJUMP 8471
#define XPRS_BARHGMAXRESTARTS 8484
#define XPRS_EXTRAELEMS 8006
#define XPRS_EXTRASETELEMS 8191
#define XPRS_LPOBJVAL 2001
#define XPRS_MIPOBJVAL 2003
#define XPRS_BESTBOUND 2004
#define XPRS_OBJRHS 2005
#define XPRS_OBJSENSE 2008
#define XPRS_ROWS 1001
#define XPRS_SIMPLEXITER 1009
#define XPRS_BARITER 5001
#define XPRS_SOLSTATUS_NOTFOUND 0
#define XPRS_SOLSTATUS_OPTIMAL 1
#define XPRS_SOLSTATUS_FEASIBLE 2
#define XPRS_SOLSTATUS_INFEASIBLE 3
#define XPRS_SOLSTATUS_UNBOUNDED 4
#define XPRS_SOLSTATUS 1053
#define XPRS_LPSTATUS 1010
#define XPRS_MIPSTATUS 1011
#define XPRS_NODES 1013
#define XPRS_COLS 1018
#define XPRS_MAXPROBNAMELENGTH 1158
#define XPRS_LP_UNSTARTED 0
#define XPRS_LP_OPTIMAL 1
#define XPRS_LP_INFEAS 2
#define XPRS_LP_CUTOFF 3
#define XPRS_LP_UNFINISHED 4
#define XPRS_LP_UNBOUNDED 5
#define XPRS_LP_CUTOFF_IN_DUAL 6
#define XPRS_LP_UNSOLVED 7
#define XPRS_LP_NONCONVEX 8
#define XPRS_MIP_SOLUTION 4
#define XPRS_MIP_INFEAS 5
#define XPRS_MIP_OPTIMAL 6
#define XPRS_MIP_UNBOUNDED 7
#define XPRS_ALG_DUAL 2
#define XPRS_ALG_PRIMAL 3
#define XPRS_ALG_BARRIER 4
#define XPRS_OBJ_MINIMIZE 1
#define XPRS_OBJ_MAXIMIZE -1
#define XPRS_UUID 3011
// ***************************************************************************
// * variable types                                                          *
// ***************************************************************************
#define XPRS_INTEGER 'I'
#define XPRS_CONTINUOUS 'C'
// ***************************************************************************
// * constraint types                                                        *
// ***************************************************************************
#define XPRS_LESS_EQUAL 'L'
#define XPRS_GREATER_EQUAL 'G'
#define XPRS_EQUAL 'E'
#define XPRS_RANGE 'R'
#define XPRS_NONBINDING 'N'
// ***************************************************************************
// * basis status                                                            *
// ***************************************************************************
#define XPRS_AT_LOWER 0
#define XPRS_BASIC 1
#define XPRS_AT_UPPER 2
#define XPRS_FREE_SUPER 3

// ***************************************************************************
// * values related to Objective control                                     *
// ***************************************************************************
#define XPRS_OBJECTIVE_PRIORITY 20001
#define XPRS_OBJECTIVE_WEIGHT 20002
#define XPRS_OBJECTIVE_ABSTOL 20003
#define XPRS_OBJECTIVE_RELTOL 20004
#define XPRS_OBJECTIVE_RHS 20005

// Let's not reformat for rest of the file.
// NOLINTBEGIN(whitespace/line_length)
// clang-format off
extern std::function<int(XPRSprob* p_prob)> XPRScreateprob;
extern std::function<int(XPRSprob prob)> XPRSdestroyprob;
extern std::function<int(const char* path)> XPRSinit;
extern std::function<int(void)> XPRSfree;
extern std::function<int(char* buffer, int maxbytes)> XPRSgetlicerrmsg;
extern std::function<int(int* p_i, char* p_c)> XPRSlicense;
extern std::function<int(char* banner)> XPRSgetbanner;
extern std::function<int(char* version)> XPRSgetversion;
extern std::function<int(int *p_major, int *p_minor, int *p_build)> XPRSgetversionnumbers;
extern std::function<int(XPRSprob prob, const char* probname)> XPRSsetprobname;
extern std::function<int(XPRSprob prob, int control)> XPRSsetdefaultcontrol;
extern std::function<int(XPRSprob prob, int reason)> XPRSinterrupt;
extern std::function<int(XPRSprob prob, int control, int value)> XPRSsetintcontrol;
extern std::function<int(XPRSprob prob, int control, XPRSint64 value)> XPRSsetintcontrol64;
extern std::function<int(XPRSprob prob, int control, double value)> XPRSsetdblcontrol;
extern std::function<int(XPRSprob prob, int control, const char* value)> XPRSsetstrcontrol;
extern std::function<int(XPRSprob prob, int objidx, int control, int value)> XPRSsetobjintcontrol;
extern std::function<int(XPRSprob prob, int objidx, int control, double value)> XPRSsetobjdblcontrol;
OR_DLL extern std::function<int(XPRSprob prob, int control, int* p_value)> XPRSgetintcontrol;
OR_DLL extern std::function<int(XPRSprob prob, int control, XPRSint64* p_value)> XPRSgetintcontrol64;
OR_DLL extern std::function<int(XPRSprob prob, int control, double* p_value)> XPRSgetdblcontrol;
OR_DLL extern std::function<int(XPRSprob prob, int control, char* value, int maxbytes, int* p_nbytes)> XPRSgetstringcontrol;
OR_DLL extern std::function<int(XPRSprob prob, int attrib, int* p_value)> XPRSgetintattrib;
OR_DLL extern std::function<int(XPRSprob prob, int attrib, char* value, int maxbytes, int* p_nbytes)> XPRSgetstringattrib;
OR_DLL extern std::function<int(XPRSprob prob, int attrib, double* p_value)> XPRSgetdblattrib;
extern std::function<int(XPRSprob prob, int objidx, int attrib, double* p_value)> XPRSgetobjdblattrib;
extern std::function<int(XPRSprob prob, int objidx, const double solution[], double* p_objval)> XPRScalcobjn;
extern std::function<int(XPRSprob prob, const char* name, int* p_id, int* p_type)> XPRSgetcontrolinfo;
OR_DLL extern std::function<int(XPRSprob prob, double objcoef[], int first, int last)> XPRSgetobj;
OR_DLL extern std::function<int(XPRSprob prob, double rhs[], int first, int last)> XPRSgetrhs;
OR_DLL extern std::function<int(XPRSprob prob, double rng[], int first, int last)> XPRSgetrhsrange;
OR_DLL extern std::function<int(XPRSprob prob, double lb[], int first, int last)> XPRSgetlb;
OR_DLL extern std::function<int(XPRSprob prob, double ub[], int first, int last)> XPRSgetub;
OR_DLL extern std::function<int(XPRSprob prob, int row, int col, double* p_coef)> XPRSgetcoef;
extern std::function<int(XPRSprob prob, int* status, double x[], int first, int last)> XPRSgetsolution;
extern std::function<int(XPRSprob prob, int* status, double duals[], int first, int last)> XPRSgetduals;
extern std::function<int(XPRSprob prob, int* status, double djs[], int first, int last)> XPRSgetredcosts;
extern std::function<int(XPRSprob prob, int nrows, int ncoefs, const char rowtype[], const double rhs[], const double rng[], const int start[], const int colind[], const double rowcoef[])> XPRSaddrows;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[])> XPRSdelrows;
extern std::function<int(XPRSprob prob, int ncols, int ncoefs, const double objcoef[], const int start[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSaddcols;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], const double objcoef[], int priority, double weight)> XPRSaddobj;
extern std::function<int(XPRSprob prob, int type, const char names[], int first, int last)> XPRSaddnames;
extern std::function<int(XPRSprob prob, int type, char names[], int first, int last)>  XPRSgetnames;
extern std::function<int(XPRSprob prob, int ncols, const int colind[])> XPRSdelcols;
extern std::function<int(XPRSprob prob, int nsets, XPRSint64 nelems, const char settype[], const XPRSint64 start[], const int colind[], const double refval[])> XPRSaddsets64;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], const char coltype[])> XPRSchgcoltype;
extern std::function<int(XPRSprob prob, const int rowstat[], const int colstat[])> XPRSloadbasis;
extern std::function<int(XPRSprob prob)> XPRSpostsolve;
extern std::function<int(XPRSprob prob, int objsense)> XPRSchgobjsense;
extern std::function<int(XPRSprob prob, char* errmsg)> XPRSgetlasterror;
extern std::function<int(XPRSprob prob, int rowstat[], int colstat[])> XPRSgetbasis;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwriteprob;
OR_DLL extern std::function<int(XPRSprob prob, char rowtype[], int first, int last)> XPRSgetrowtype;
OR_DLL extern std::function<int(XPRSprob prob, char coltype[], int first, int last)> XPRSgetcoltype;
extern std::function<int(XPRSprob prob, int nbounds, const int colind[], const char bndtype[], const double bndval[])> XPRSchgbounds;
extern std::function<int(XPRSprob prob, int length, const double solval[], const int colind[], const char* name)> XPRSaddmipsol;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[])> XPRSloaddelayedrows;
extern std::function<int(XPRSprob prob, int ndirs, const int colind[], const int priority[], const char dir[], const double uppseudo[], const double downpseudo[])> XPRSloaddirs;
extern std::function<int(XPRSprob prob, double x[], double slack[], double duals[], double djs[])> XPRSgetlpsol;
extern std::function<int(XPRSprob prob, double x[], double slack[])> XPRSgetmipsol;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], const double objcoef[])> XPRSchgobj;
extern std::function<int(XPRSprob prob, int row, int col, double coef)> XPRSchgcoef;
extern std::function<int(XPRSprob prob, int ncoefs, const int rowind[], const int colind[], const double rowcoef[])> XPRSchgmcoef;
extern std::function<int(XPRSprob prob, XPRSint64 ncoefs, const int rowind[], const int colind[], const double rowcoef[])> XPRSchgmcoef64;
extern std::function<int(XPRSprob prob, int ncoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[])> XPRSchgmqobj;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], const double rhs[])> XPRSchgrhs;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], const double rng[])> XPRSchgrhsrange;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], const char rowtype[])> XPRSchgrowtype;
extern std::function<int(XPRSprob prob, int objidx)> XPRSdelobj;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_intsol)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_intsol)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_message)(XPRSprob cbprob, void* cbdata, const char* msg, int msglen, int msgtype), void* p, int priority)> XPRSaddcbmessage;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_message)(XPRSprob cbprob, void* cbdata, const char* msg, int msglen, int msgtype), void* p)> XPRSremovecbmessage;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_checktime)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbchecktime;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_checktime)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbchecktime;
extern std::function<int(XPRSprob prob, const char* flags)> XPRSlpoptimize;
extern std::function<int(XPRSprob prob, const char* flags)> XPRSmipoptimize;
extern std::function<int(XPRSprob prob, const char* flags, int* solvestatus, int* solstatus)> XPRSoptimize;
// clang-format on
// NOLINTEND(whitespace/line_length)

}  // namespace operations_research

#endif  // ORTOOLS_THIRD_PARTY_SOLVERS_XPRESS_ENVIRONMENT_H_
