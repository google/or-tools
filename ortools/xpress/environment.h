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

#ifndef OR_TOOLS_XPRESS_ENVIRONMENT_H
#define OR_TOOLS_XPRESS_ENVIRONMENT_H

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/dynamic_library.h"
#include "ortools/base/logging.h"

extern "C" {
typedef struct XPRSobject_s* XPRSobject;
typedef struct xo_prob_struct* XPRSprob;
typedef struct XPRSmipsolpool_s* XPRSmipsolpool;
typedef struct xo_NameList* XPRSnamelist;
typedef struct XPRSmipsolenum_s* XPRSmipsolenum;
typedef struct xo_user_branch_entity_s* XPRSbranchobject;
typedef struct PoolCut* XPRScut;
}

namespace operations_research {

void printXpressBanner(bool error);

bool initXpressEnv(bool verbose = true, int xpress_oem_license_key = 0);

bool XpressIsCorrectlyInstalled();
// clang-format off
// Force the loading of the xpress dynamic library. It returns true if the
// library was successfully loaded. This method can only be called once.
// Successive calls are no-op.
//
// Note that it does not check if a token license can be grabbed.
absl::Status LoadXpressDynamicLibrary(std::string &xpresspath);

// The list of #define and extern std::function<> below is generated directly
// from xprs.h via parse_header_xpress.py
// See the top comment on the parse_header_xpress.py file.
// This is the header section
#if defined(_WIN32)
#define XPRSint64 __int64
#elif defined(__LP64__) || defined(_LP64) || defined(__ILP64__) || defined(_ILP64)
#define XPRSint64 long
#else
#define XPRSint64 long long
#endif

#if defined(_MSC_VER)
#define XPRS_CC __stdcall
#else
#define XPRS_CC
#endif
#define XPRS_PLUSINFINITY 1.0e+20
#define XPRS_MINUSINFINITY -1.0e+20
#define XPRS_MAXINT 2147483647
#define XPRS_MAXBANNERLENGTH 512
#define XPVERSION 39 ///< Xpress 8.13
#define XPRS_MPSRHSNAME 6001
#define XPRS_MPSOBJNAME 6002
#define XPRS_MPSRANGENAME 6003
#define XPRS_MPSBOUNDNAME 6004
#define XPRS_OUTPUTMASK 6005
#define XPRS_TUNERMETHODFILE 6017
#define XPRS_TUNEROUTPUTPATH 6018
#define XPRS_TUNERSESSIONNAME 6019
#define XPRS_COMPUTEEXECSERVICE 6022
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
#define XPRS_PERTURB 7044
#define XPRS_MARKOWITZTOL 7047
#define XPRS_MIPABSGAPNOTIFY 7064
#define XPRS_MIPRELGAPNOTIFY 7065
#define XPRS_BARLARGEBOUND 7067
#define XPRS_PPFACTOR 7069
#define XPRS_REPAIRINDEFINITEQMAX 7071
#define XPRS_BARGAPTARGET 7073
#define XPRS_BARSTARTWEIGHT 7076
#define XPRS_BARFREESCALE 7077
#define XPRS_SBEFFORT 7086
#define XPRS_HEURDIVERANDOMIZE 7089
#define XPRS_HEURSEARCHEFFORT 7090
#define XPRS_CUTFACTOR 7091
#define XPRS_EIGENVALUETOL 7097
#define XPRS_INDLINBIGM 7099
#define XPRS_TREEMEMORYSAVINGTARGET 7100
#define XPRS_GLOBALFILEBIAS 7101
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
#define XPRS_CSTYLE 8050
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
#define XPRS_LPTHREADS 8124
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
#define XPRS_MAXCUTTIME 8149
#define XPRS_ACTIVESET 8152
#define XPRS_BARINDEFLIMIT 8153
#define XPRS_HEURSTRATEGY 8154
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
#define XPRS_LINELENGTH 8209
#define XPRS_MUTEXCALLBACKS 8210
#define XPRS_BARCRASH 8211
#define XPRS_HEURDIVESOFTROUNDING 8215
#define XPRS_HEURSEARCHROOTSELECT 8216
#define XPRS_HEURSEARCHTREESELECT 8217
#define XPRS_MPS18COMPATIBLE 8223
#define XPRS_ROOTPRESOLVE 8224
#define XPRS_CROSSOVERDRP 8227
#define XPRS_FORCEOUTPUT 8229
#define XPRS_DETERMINISTIC 8232
#define XPRS_PREPROBING 8238
#define XPRS_EXTRAQCELEMENTS 8240
#define XPRS_EXTRAQCROWS 8241
#define XPRS_TREEMEMORYLIMIT 8242
#define XPRS_TREECOMPRESSION 8243
#define XPRS_TREEDIAGNOSTICS 8244
#define XPRS_MAXGLOBALFILESIZE 8245
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
#define XPRS_TUNERMAXTIME 8364
#define XPRS_TUNERHISTORY 8365
#define XPRS_TUNERPERMUTE 8366
#define XPRS_TUNERROOTALG 8367
#define XPRS_TUNERVERBOSE 8370
#define XPRS_TUNEROUTPUT 8372
#define XPRS_PREANALYTICCENTER 8374
#define XPRS_NETCUTS 8382
#define XPRS_LPFLAGS 8385
#define XPRS_MIPKAPPAFREQ 8386
#define XPRS_OBJSCALEFACTOR 8387
#define XPRS_GLOBALFILELOGINTERVAL 8389
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
#define XPRS_COMPUTE 8411
#define XPRS_NETSTALLLIMIT 8412
#define XPRS_SERIALIZEPREINTSOL 8413
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
#define XPRS_ESCAPENAMES 8440
#define XPRS_IOTIMEOUT 8442
#define XPRS_MAXSTALLTIME 8443
#define XPRS_AUTOCUTTING 8446
#define XPRS_EXTRAELEMS 8006
#define XPRS_EXTRAPRESOLVE 8037
#define XPRS_EXTRASETELEMS 8191
#define XPRS_MATRIXNAME 3001
#define XPRS_BOUNDNAME 3002
#define XPRS_OBJNAME 3003
#define XPRS_RHSNAME 3004
#define XPRS_RANGENAME 3005
#define XPRS_XPRESSVERSION 3010
#define XPRS_UUID 3011
#define XPRS_LPOBJVAL 2001
#define XPRS_SUMPRIMALINF 2002
#define XPRS_MIPOBJVAL 2003
#define XPRS_BESTBOUND 2004
#define XPRS_OBJRHS 2005
#define XPRS_MIPBESTOBJVAL 2006
#define XPRS_OBJSENSE 2008
#define XPRS_BRANCHVALUE 2009
#define XPRS_PENALTYVALUE 2061
#define XPRS_CURRMIPCUTOFF 2062
#define XPRS_BARCONDA 2063
#define XPRS_BARCONDD 2064
#define XPRS_MAXABSPRIMALINFEAS 2073
#define XPRS_MAXRELPRIMALINFEAS 2074
#define XPRS_MAXABSDUALINFEAS 2075
#define XPRS_MAXRELDUALINFEAS 2076
#define XPRS_PRIMALDUALINTEGRAL 2079
#define XPRS_MAXMIPINFEAS 2083
#define XPRS_ATTENTIONLEVEL 2097
#define XPRS_MAXKAPPA 2098
#define XPRS_TREECOMPLETION 2104
#define XPRS_PREDICTEDATTLEVEL 2105
#define XPRS_BARPRIMALOBJ 4001
#define XPRS_BARDUALOBJ 4002
#define XPRS_BARPRIMALINF 4003
#define XPRS_BARDUALINF 4004
#define XPRS_BARCGAP 4005
#define XPRS_ROWS 1001
#define XPRS_SETS 1004
#define XPRS_PRIMALINFEAS 1007
#define XPRS_DUALINFEAS 1008
#define XPRS_SIMPLEXITER 1009
#define XPRS_LPSTATUS 1010
#define XPRS_MIPSTATUS 1011
#define XPRS_CUTS 1012
#define XPRS_NODES 1013
#define XPRS_NODEDEPTH 1014
#define XPRS_ACTIVENODES 1015
#define XPRS_MIPSOLNODE 1016
#define XPRS_MIPSOLS 1017
#define XPRS_COLS 1018
#define XPRS_SPAREROWS 1019
#define XPRS_SPARECOLS 1020
#define XPRS_SPAREMIPENTS 1022
#define XPRS_ERRORCODE 1023
#define XPRS_MIPINFEAS 1024
#define XPRS_PRESOLVESTATE 1026
#define XPRS_PARENTNODE 1027
#define XPRS_NAMELENGTH 1028
#define XPRS_QELEMS 1030
#define XPRS_NUMIIS 1031
#define XPRS_MIPENTS 1032
#define XPRS_BRANCHVAR 1036
#define XPRS_MIPTHREADID 1037
#define XPRS_ALGORITHM 1049
#define XPRS_TIME 1122
#define XPRS_ORIGINALROWS 1124
#define XPRS_CALLBACKCOUNT_OPTNODE 1136
#define XPRS_CALLBACKCOUNT_CUTMGR 1137
#define XPRS_ORIGINALQELEMS 1157
#define XPRS_MAXPROBNAMELENGTH 1158
#define XPRS_STOPSTATUS 1179
#define XPRS_ORIGINALMIPENTS 1191
#define XPRS_ORIGINALSETS 1194
#define XPRS_SPARESETS 1203
#define XPRS_CHECKSONMAXTIME 1208
#define XPRS_CHECKSONMAXCUTTIME 1209
#define XPRS_ORIGINALCOLS 1214
#define XPRS_QCELEMS 1232
#define XPRS_QCONSTRAINTS 1234
#define XPRS_ORIGINALQCELEMS 1237
#define XPRS_ORIGINALQCONSTRAINTS 1239
#define XPRS_PEAKTOTALTREEMEMORYUSAGE 1240
#define XPRS_CURRENTNODE 1248
#define XPRS_TREEMEMORYUSAGE 1251
#define XPRS_GLOBALFILESIZE 1252
#define XPRS_GLOBALFILEUSAGE 1253
#define XPRS_INDICATORS 1254
#define XPRS_ORIGINALINDICATORS 1255
#define XPRS_CORESPERCPUDETECTED 1258
#define XPRS_CPUSDETECTED 1259
#define XPRS_CORESDETECTED 1260
#define XPRS_PHYSICALCORESDETECTED 1261
#define XPRS_PHYSICALCORESPERCPUDETECTED 1262
#define XPRS_BARSING 1281
#define XPRS_BARSINGR 1282
#define XPRS_PRESOLVEINDEX 1284
#define XPRS_CONES 1307
#define XPRS_CONEELEMS 1308
#define XPRS_PWLCONS 1325
#define XPRS_GENCONS 1327
#define XPRS_TREERESTARTS 1335
#define XPRS_ORIGINALPWLS 1336
#define XPRS_ORIGINALGENCONS 1338
#define XPRS_COMPUTEEXECUTIONS 1356
#define XPRS_MIPSOLTIME 1371
#define XPRS_BARITER 5001
#define XPRS_BARDENSECOL 5004
#define XPRS_BARCROSSOVER 5005
#define XPRS_IIS XPRS_NUMIIS
#define XPRS_SETMEMBERS 1005
#define XPRS_ELEMS 1006
#define XPRS_SPAREELEMS 1021
#define XPRS_SYSTEMMEMORY 1148
#define XPRS_ORIGINALSETMEMBERS 1195
#define XPRS_SPARESETELEMS 1204
#define XPRS_CURRENTMEMORY 1285
#define XPRS_PEAKMEMORY 1286
#define XPRS_TOTALMEMORY 1322
#define XPRS_AVAILABLEMEMORY 1324
#define XPRS_PWLPOINTS 1326
#define XPRS_GENCONCOLS 1328
#define XPRS_GENCONVALS 1329
#define XPRS_ORIGINALPWLPOINTS 1337
#define XPRS_ORIGINALGENCONCOLS 1339
#define XPRS_ORIGINALGENCONVALS 1340
#define XPRS_BARAASIZE 5002
#define XPRS_BARLSIZE 5003
#define XPRS_MSP_DEFAULTUSERSOLFEASTOL 6209
#define XPRS_MSP_DEFAULTUSERSOLMIPTOL 6210
#define XPRS_MSP_SOL_FEASTOL 6402
#define XPRS_MSP_SOL_MIPTOL 6403
#define XPRS_MSP_DUPLICATESOLUTIONSPOLICY 6203
#define XPRS_MSP_INCLUDEPROBNAMEINLOGGING 6211
#define XPRS_MSP_WRITESLXSOLLOGGING 6212
#define XPRS_MSP_ENABLESLACKSTORAGE 6213
#define XPRS_MSP_OUTPUTLOG 6214
#define XPRS_MSP_SOL_BITFIELDSUSR 6406
#define XPRS_MSP_SOLPRB_OBJ 6500
#define XPRS_MSP_SOLPRB_INFSUM_PRIMAL 6502
#define XPRS_MSP_SOLPRB_INFSUM_MIP 6504
#define XPRS_MSP_SOLUTIONS 6208
#define XPRS_MSP_PRB_VALIDSOLS 6300
#define XPRS_MSP_PRB_FEASIBLESOLS 6301
#define XPRS_MSP_SOL_COLS 6400
#define XPRS_MSP_SOL_NONZEROS 6401
#define XPRS_MSP_SOL_ISUSERSOLUTION 6404
#define XPRS_MSP_SOL_ISREPROCESSEDUSERSOLUTION 6405
#define XPRS_MSP_SOL_BITFIELDSSYS 6407
#define XPRS_MSP_SOLPRB_INFEASCOUNT 6501
#define XPRS_MSP_SOLPRB_INFCNT_PRIMAL 6503
#define XPRS_MSP_SOLPRB_INFCNT_MIP 6505
#define XPRS_MSE_OUTPUTTOL 6609
#define XPRS_MSE_CALLBACKCULLSOLS_MIPOBJECT 6601
#define XPRS_MSE_CALLBACKCULLSOLS_DIVERSITY 6602
#define XPRS_MSE_CALLBACKCULLSOLS_MODOBJECT 6603
#define XPRS_MSE_OPTIMIZEDIVERSITY 6607
#define XPRS_MSE_OUTPUTLOG 6610
#define XPRS_MSE_DIVERSITYSUM 6608
#define XPRS_MSE_SOLUTIONS 6600
#define XPRS_MSE_METRIC_MIPOBJECT 6604
#define XPRS_MSE_METRIC_DIVERSITY 6605
#define XPRS_MSE_METRIC_MODOBJECT 6606
#define XPRS_LP_UNSTARTED 0
#define XPRS_LP_OPTIMAL 1
#define XPRS_LP_INFEAS 2
#define XPRS_LP_CUTOFF 3
#define XPRS_LP_UNFINISHED 4
#define XPRS_LP_UNBOUNDED 5
#define XPRS_LP_CUTOFF_IN_DUAL 6
#define XPRS_LP_UNSOLVED 7
#define XPRS_LP_NONCONVEX 8
#define XPRS_MIP_NOT_LOADED 0
#define XPRS_MIP_LP_NOT_OPTIMAL 1
#define XPRS_MIP_LP_OPTIMAL 2
#define XPRS_MIP_NO_SOL_FOUND 3
#define XPRS_MIP_SOLUTION 4
#define XPRS_MIP_INFEAS 5
#define XPRS_MIP_OPTIMAL 6
#define XPRS_MIP_UNBOUNDED 7
#define XPRS_BAR_DEFAULT 0
#define XPRS_BAR_MIN_DEGREE 1
#define XPRS_BAR_MIN_LOCAL_FILL 2
#define XPRS_BAR_NESTED_DISSECTION 3
#define XPRS_ALG_DEFAULT 1
#define XPRS_ALG_DUAL 2
#define XPRS_ALG_PRIMAL 3
#define XPRS_ALG_BARRIER 4
#define XPRS_ALG_NETWORK 5
#define XPRS_STOP_NONE 0
#define XPRS_STOP_TIMELIMIT 1
#define XPRS_STOP_CTRLC 2
#define XPRS_STOP_NODELIMIT 3
#define XPRS_STOP_ITERLIMIT 4
#define XPRS_STOP_MIPGAP 5
#define XPRS_STOP_SOLLIMIT 6
#define XPRS_STOP_GLOBALERROR 7
#define XPRS_STOP_MEMORYERROR 8
#define XPRS_STOP_USER 9
#define XPRS_STOP_INFEASIBLE 10
#define XPRS_STOP_LICENSELOST 11
#define XPRS_ANA_AUTOMATIC -1
#define XPRS_ANA_NEVER 0
#define XPRS_ANA_ALWAYS 1
#define XPRS_BOOL_OFF 0
#define XPRS_BOOL_ON 1
#define XPRS_BACKTRACKALG_BEST_ESTIMATE 2
#define XPRS_BACKTRACKALG_BEST_BOUND 3
#define XPRS_BACKTRACKALG_DEEPEST_NODE 4
#define XPRS_BACKTRACKALG_HIGHEST_NODE 5
#define XPRS_BACKTRACKALG_EARLIEST_NODE 6
#define XPRS_BACKTRACKALG_LATEST_NODE 7
#define XPRS_BACKTRACKALG_RANDOM 8
#define XPRS_BACKTRACKALG_MIN_INFEAS 9
#define XPRS_BACKTRACKALG_BEST_ESTIMATE_MIN_INFEAS 10
#define XPRS_BACKTRACKALG_DEEPEST_BEST_ESTIMATE 11
#define XPRS_BRANCH_MIN_EST_FIRST 0
#define XPRS_BRANCH_MAX_EST_FIRST 1
#define XPRS_ALG_PULL_CHOLESKY 0
#define XPRS_ALG_PUSH_CHOLESKY 1
#define XPRS_XDRPBEFORE_CROSSOVER 1
#define XPRS_XDRPINSIDE_CROSSOVER 2
#define XPRS_XDRPAGGRESSIVE_BEFORE_CROSSOVER 4
#define XPRS_DUALGRADIENT_AUTOMATIC -1
#define XPRS_DUALGRADIENT_DEVEX 0
#define XPRS_DUALGRADIENT_STEEPESTEDGE 1
#define XPRS_DUALSTRATEGY_REMOVE_INFEAS_WITH_PRIMAL 0
#define XPRS_DUALSTRATEGY_REMOVE_INFEAS_WITH_DUAL 1
#define XPRS_FEASIBILITYPUMP_AUTOMATIC -1
#define XPRS_FEASIBILITYPUMP_NEVER 0
#define XPRS_FEASIBILITYPUMP_ALWAYS 1
#define XPRS_FEASIBILITYPUMP_LASTRESORT 2
#define XPRS_HEURSEARCH_LOCAL_SEARCH_LARGE_NEIGHBOURHOOD 0
#define XPRS_HEURSEARCH_LOCAL_SEARCH_NODE_NEIGHBOURHOOD 1
#define XPRS_HEURSEARCH_LOCAL_SEARCH_SOLUTION_NEIGHBOURHOOD 2
#define XPRS_HEURSTRATEGY_AUTOMATIC -1
#define XPRS_HEURSTRATEGY_NONE 0
#define XPRS_HEURSTRATEGY_BASIC 1
#define XPRS_HEURSTRATEGY_ENHANCED 2
#define XPRS_HEURSTRATEGY_EXTENSIVE 3
#define XPRS_NODESELECTION_LOCAL_FIRST 1
#define XPRS_NODESELECTION_BEST_FIRST 2
#define XPRS_NODESELECTION_LOCAL_DEPTH_FIRST 3
#define XPRS_NODESELECTION_BEST_FIRST_THEN_LOCAL_FIRST 4
#define XPRS_NODESELECTION_DEPTH_FIRST 5
#define XPRS_OUTPUTLOG_NO_OUTPUT 0
#define XPRS_OUTPUTLOG_FULL_OUTPUT 1
#define XPRS_OUTPUTLOG_ERRORS_AND_WARNINGS 2
#define XPRS_OUTPUTLOG_ERRORS 3
#define XPRS_PREPROBING_AUTOMATIC -1
#define XPRS_PREPROBING_DISABLED 0
#define XPRS_PREPROBING_LIGHT 1
#define XPRS_PREPROBING_FULL 2
#define XPRS_PREPROBING_FULL_AND_REPEAT 3
#define XPRS_PRESOLVEOPS_SINGLETONCOLUMNREMOVAL 1
#define XPRS_PRESOLVEOPS_SINGLETONROWREMOVAL 2
#define XPRS_PRESOLVEOPS_FORCINGROWREMOVAL 4
#define XPRS_PRESOLVEOPS_DUALREDUCTIONS 8
#define XPRS_PRESOLVEOPS_REDUNDANTROWREMOVAL 16
#define XPRS_PRESOLVEOPS_DUPLICATECOLUMNREMOVAL 32
#define XPRS_PRESOLVEOPS_DUPLICATEROWREMOVAL 64
#define XPRS_PRESOLVEOPS_STRONGDUALREDUCTIONS 128
#define XPRS_PRESOLVEOPS_VARIABLEELIMINATIONS 256
#define XPRS_PRESOLVEOPS_NOIPREDUCTIONS 512
#define XPRS_PRESOLVEOPS_NOGLOBALDOMAINCHANGE 1024
#define XPRS_PRESOLVEOPS_NOADVANCEDIPREDUCTIONS 2048
#define XPRS_PRESOLVEOPS_LINEARLYDEPENDANTROWREMOVAL 16384
#define XPRS_PRESOLVEOPS_NOINTEGERVARIABLEANDSOSDETECTION 32768
#define XPRS_PRESOLVEOPS_NODUALREDONGLOBALS 536870912
#define XPRS_PRESOLVESTATE_PROBLEMLOADED (1<<0)
#define XPRS_PRESOLVESTATE_PROBLEMLPPRESOLVED (1<<1)
#define XPRS_PRESOLVESTATE_PROBLEMMIPPRESOLVED (1<<2)
#define XPRS_PRESOLVESTATE_SOLUTIONVALID (1<<7)
#define XPRS_MIPPRESOLVE_REDUCED_COST_FIXING 1
#define XPRS_MIPPRESOLVE_LOGIC_PREPROCESSING 2
#define XPRS_MIPPRESOLVE_ALLOW_CHANGE_BOUNDS 8
#define XPRS_MIPPRESOLVE_DUAL_REDUCTIONS 16
#define XPRS_MIPPRESOLVE_GLOBAL_COEFFICIENT_TIGHTENING 32
#define XPRS_MIPPRESOLVE_OBJECTIVE_BASED_REDUCTIONS 64
#define XPRS_MIPPRESOLVE_ALLOW_TREE_RESTART 128
#define XPRS_MIPPRESOLVE_SYMMETRY_REDUCTIONS 256
#define XPRS_PRESOLVE_NOPRIMALINFEASIBILITY -1
#define XPRS_PRESOLVE_NONE 0
#define XPRS_PRESOLVE_DEFAULT 1
#define XPRS_PRESOLVE_KEEPREDUNDANTBOUNDS 2
#define XPRS_PRICING_PARTIAL -1
#define XPRS_PRICING_DEFAULT 0
#define XPRS_PRICING_DEVEX 1
#define XPRS_CUTSTRATEGY_DEFAULT -1
#define XPRS_CUTSTRATEGY_NONE 0
#define XPRS_CUTSTRATEGY_CONSERVATIVE 1
#define XPRS_CUTSTRATEGY_MODERATE 2
#define XPRS_CUTSTRATEGY_AGGRESSIVE 3
#define XPRS_VARSELECTION_AUTOMATIC -1
#define XPRS_VARSELECTION_MIN_UPDOWN_PSEUDO_COSTS 1
#define XPRS_VARSELECTION_SUM_UPDOWN_PSEUDO_COSTS 2
#define XPRS_VARSELECTION_MAX_UPDOWN_PSEUDO_COSTS_PLUS_TWICE_MIN 3
#define XPRS_VARSELECTION_MAX_UPDOWN_PSEUDO_COSTS 4
#define XPRS_VARSELECTION_DOWN_PSEUDO_COST 5
#define XPRS_VARSELECTION_UP_PSEUDO_COST 6
#define XPRS_SCALING_ROW_SCALING 1
#define XPRS_SCALING_COLUMN_SCALING 2
#define XPRS_SCALING_ROW_SCALING_AGAIN 4
#define XPRS_SCALING_MAXIMUM 8
#define XPRS_SCALING_CURTIS_REID 16
#define XPRS_SCALING_BY_MAX_ELEM_NOT_GEO_MEAN 32
#define XPRS_SCALING_BIGM 64
#define XPRS_SCALING_SIMPLEX_OBJECTIVE_SCALING 128
#define XPRS_SCALING_IGNORE_QUADRATIC_ROW_PART 256
#define XPRS_SCALING_BEFORE_PRESOLVE 512
#define XPRS_SCALING_NO_SCALING_ROWS_UP 1024
#define XPRS_SCALING_NO_SCALING_COLUMNS_DOWN 2048
#define XPRS_SCALING_DISABLE_GLOBAL_OBJECTIVE_SCALING 4096
#define XPRS_SCALING_RHS_SCALING 8192
#define XPRS_SCALING_NO_AGGRESSIVE_Q_SCALING 16384
#define XPRS_SCALING_SLACK_SCALING 32768
#define XPRS_SCALING_RUIZ 65536
#define XPRS_SCALING_DOGLEG 131072
#define XPRS_SCALING_BEFORE_AND_AFTER_PRESOLVE (2*131072)
#define XPRS_CUTSELECT_CLIQUE (32+1823)
#define XPRS_CUTSELECT_MIR (64+1823)
#define XPRS_CUTSELECT_COVER (128+1823)
#define XPRS_CUTSELECT_FLOWPATH (2048+1823)
#define XPRS_CUTSELECT_IMPLICATION (4096+1823)
#define XPRS_CUTSELECT_LIFT_AND_PROJECT (8192+1823)
#define XPRS_CUTSELECT_DISABLE_CUT_ROWS (16384+1823)
#define XPRS_CUTSELECT_GUB_COVER (32768+1823)
#define XPRS_CUTSELECT_DEFAULT (-1)
#define XPRS_REFINEOPS_LPOPTIMAL 1
#define XPRS_REFINEOPS_MIPSOLUTION 2
#define XPRS_REFINEOPS_MIPOPTIMAL 4
#define XPRS_REFINEOPS_MIPNODELP 8
#define XPRS_REFINEOPS_LPPRESOLVE 16
#define XPRS_REFINEOPS_ITERATIVEREFINER 32
#define XPRS_REFINEOPS_REFINERPRECISION 64
#define XPRS_REFINEOPS_REFINERUSEPRIMAL 128
#define XPRS_REFINEOPS_REFINERUSEDUAL 256
#define XPRS_REFINEOPS_MIPFIXGLOBALS 512
#define XPRS_REFINEOPS_MIPFIXGLOBALSTARGET 1024
#define XPRS_DUALIZEOPS_SWITCHALGORITHM 1
#define XPRS_TREEDIAGNOSTICS_MEMORY_USAGE_SUMMARIES 1
#define XPRS_TREEDIAGNOSTICS_MEMORY_SAVED_REPORTS 2
#define XPRS_BARPRESOLVEOPS_STANDARD_PRESOLVE 0
#define XPRS_BARPRESOLVEOPS_EXTRA_BARRIER_PRESOLVE 1
#define XPRS_MIPRESTART_DEFAULT -1
#define XPRS_MIPRESTART_OFF 0
#define XPRS_MIPRESTART_MODERATE 1
#define XPRS_MIPRESTART_AGGRESSIVE 2
#define XPRS_PRECOEFELIM_DISABLED 0
#define XPRS_PRECOEFELIM_AGGRESSIVE 1
#define XPRS_PRECOEFELIM_CAUTIOUS 2
#define XPRS_PREDOMROW_AUTOMATIC -1
#define XPRS_PREDOMROW_DISABLED 0
#define XPRS_PREDOMROW_CAUTIOUS 1
#define XPRS_PREDOMROW_MEDIUM 2
#define XPRS_PREDOMROW_AGGRESSIVE 3
#define XPRS_PREDOMCOL_AUTOMATIC -1
#define XPRS_PREDOMCOL_DISABLED 0
#define XPRS_PREDOMCOL_CAUTIOUS 1
#define XPRS_PREDOMCOL_AGGRESSIVE 2
#define XPRS_PRIMALUNSHIFT_ALLOW_DUAL_UNSHIFT 0
#define XPRS_PRIMALUNSHIFT_NO_DUAL_UNSHIFT 1
#define XPRS_REPAIRINDEFINITEQ_REPAIR_IF_POSSIBLE 0
#define XPRS_REPAIRINDEFINITEQ_NO_REPAIR 1
#define XPRS_OBJ_MINIMIZE 1
#define XPRS_OBJ_MAXIMIZE -1
#define XPRS_TYPE_NOTDEFINED 0
#define XPRS_TYPE_INT 1
#define XPRS_TYPE_INT64 2
#define XPRS_TYPE_DOUBLE 3
#define XPRS_TYPE_STRING 4
#define XPRS_QCONVEXITY_UNKNOWN -1
#define XPRS_QCONVEXITY_NONCONVEX 0
#define XPRS_QCONVEXITY_CONVEX 1
#define XPRS_QCONVEXITY_REPAIRABLE 2
#define XPRS_QCONVEXITY_CONVEXCONE 3
#define XPRS_QCONVEXITY_CONECONVERTABLE 4
#define XPRS_SOLINFO_ABSPRIMALINFEAS 0
#define XPRS_SOLINFO_RELPRIMALINFEAS 1
#define XPRS_SOLINFO_ABSDUALINFEAS 2
#define XPRS_SOLINFO_RELDUALINFEAS 3
#define XPRS_SOLINFO_MAXMIPFRACTIONAL 4
#define XPRS_SOLINFO_ABSMIPINFEAS 5
#define XPRS_SOLINFO_RELMIPINFEAS 6
#define XPRS_TUNERMODE_AUTOMATIC -1
#define XPRS_TUNERMODE_OFF 0
#define XPRS_TUNERMODE_ON 1
#define XPRS_TUNERMETHOD_AUTOMATIC -1
#define XPRS_TUNERMETHOD_LPQUICK 0
#define XPRS_TUNERMETHOD_MIPQUICK 1
#define XPRS_TUNERMETHOD_MIPCOMPREHENSIVE 2
#define XPRS_TUNERMETHOD_MIPROOTFOCUS 3
#define XPRS_TUNERMETHOD_MIPTREEFOCUS 4
#define XPRS_TUNERMETHOD_MIPSIMPLE 5
#define XPRS_TUNERMETHOD_SLPQUICK 6
#define XPRS_TUNERMETHOD_MISLPQUICK 7
#define XPRS_TUNERMETHOD_MIPHEURISTICS 8
#define XPRS_TUNERMETHOD_LPNUMERICS 9
#define XPRS_TUNERTARGET_AUTOMATIC -1
#define XPRS_TUNERTARGET_TIMEGAP 0
#define XPRS_TUNERTARGET_TIMEBOUND 1
#define XPRS_TUNERTARGET_TIMEOBJVAL 2
#define XPRS_TUNERTARGET_INTEGRAL 3
#define XPRS_TUNERTARGET_SLPTIME 4
#define XPRS_TUNERTARGET_SLPOBJVAL 5
#define XPRS_TUNERTARGET_SLPVALIDATION 6
#define XPRS_TUNERTARGET_GAP 7
#define XPRS_TUNERTARGET_BOUND 8
#define XPRS_TUNERTARGET_OBJVAL 9
#define XPRS_TUNERHISTORY_IGNORE 0
#define XPRS_TUNERHISTORY_APPEND 1
#define XPRS_TUNERHISTORY_REUSE 2
#define XPRS_TUNERROOTALG_DUAL 1
#define XPRS_TUNERROOTALG_PRIMAL 2
#define XPRS_TUNERROOTALG_BARRIER 4
#define XPRS_TUNERROOTALG_NETWORK 8
#define XPRS_LPFLAGS_DUAL 1
#define XPRS_LPFLAGS_PRIMAL 2
#define XPRS_LPFLAGS_BARRIER 4
#define XPRS_LPFLAGS_NETWORK 8
#define XPRS_GENCONS_MAX 0
#define XPRS_GENCONS_MIN 1
#define XPRS_GENCONS_AND 2
#define XPRS_GENCONS_OR 3
#define XPRS_GENCONS_ABS 4
#define XPRS_CLAMPING_PRIMAL 1
#define XPRS_CLAMPING_DUAL 2
#define XPRS_CLAMPING_SLACKS 4
#define XPRS_CLAMPING_RDJ 8
#define XPRS_ROWFLAG_QUADRATIC 1
#define XPRS_ROWFLAG_DELAYED 2
#define XPRS_ROWFLAG_MODELCUT 4
#define XPRS_ROWFLAG_INDICATOR 8
#define XPRS_ROWFLAG_NONLINEAR 16
#define XPRS_ALLOW_COMPUTE_ALWAYS 1
#define XPRS_ALLOW_COMPUTE_NEVER 0
#define XPRS_ALLOW_COMPUTE_DEFAULT -1
#define XPRS_COMPUTELOG_NEVER 0
#define XPRS_COMPUTELOG_REALTIME 1
#define XPRS_COMPUTELOG_ONCOMPLETION 2
#define XPRS_COMPUTELOG_ONERROR 3
#define XPRS_ISUSERSOLUTION 0x1
#define XPRS_ISREPROCESSEDUSERSOLUTION 0x2
extern std::function<int(XPRSprob dest, XPRSprob src)> XPRScopycallbacks;
extern std::function<int(XPRSprob dest, XPRSprob src)> XPRScopycontrols;
extern std::function<int(XPRSprob dest, XPRSprob src, const char* name)> XPRScopyprob;
extern std::function<int(XPRSprob* p_prob)> XPRScreateprob;
extern std::function<int(XPRSprob prob)> XPRSdestroyprob;
extern std::function<int(const char* path)> XPRSinit;
extern std::function<int(void)> XPRSfree;
extern std::function<int(char* buffer, int maxbytes)> XPRSgetlicerrmsg;
extern std::function<int(int* p_i, char* p_c)> XPRSlicense;
extern std::function<int(int* p_notyet)> XPRSbeginlicensing;
extern std::function<int(void)> XPRSendlicensing;
extern std::function<int(int checkedmode)> XPRSsetcheckedmode;
extern std::function<int(int* p_checkedmode)> XPRSgetcheckedmode;
extern std::function<int(char* banner)> XPRSgetbanner;
extern std::function<int(char* version)> XPRSgetversion;
extern std::function<int(int* p_daysleft)> XPRSgetdaysleft;
extern std::function<int(const char* feature, int* p_status)> XPRSfeaturequery;
extern std::function<int(XPRSprob prob, const char* probname)> XPRSsetprobname;
extern std::function<int(XPRSprob prob, const char* filename)> XPRSsetlogfile;
extern std::function<int(XPRSprob prob, int control)> XPRSsetdefaultcontrol;
extern std::function<int(XPRSprob prob)> XPRSsetdefaults;
extern std::function<int(XPRSprob prob, int reason)> XPRSinterrupt;
extern std::function<int(XPRSprob prob, char* name)> XPRSgetprobname;
extern std::function<int(XPRSprob prob, int control, int value)> XPRSsetintcontrol;
extern std::function<int(XPRSprob prob, int control, XPRSint64 value)> XPRSsetintcontrol64;
extern std::function<int(XPRSprob prob, int control, double value)> XPRSsetdblcontrol;
extern std::function<int(XPRSprob prob, int control, const char* value)> XPRSsetstrcontrol;
extern std::function<int(XPRSprob prob, int control, int* p_value)> XPRSgetintcontrol;
extern std::function<int(XPRSprob prob, int control, XPRSint64* p_value)> XPRSgetintcontrol64;
extern std::function<int(XPRSprob prob, int control, double* p_value)> XPRSgetdblcontrol;
extern std::function<int(XPRSprob prob, int control, char* value)> XPRSgetstrcontrol;
extern std::function<int(XPRSprob prob, int control, char* value, int maxbytes, int* p_nbytes)> XPRSgetstringcontrol;
extern std::function<int(XPRSprob prob, int attrib, int* p_value)> XPRSgetintattrib;
extern std::function<int(XPRSprob prob, int attrib, XPRSint64* p_value)> XPRSgetintattrib64;
extern std::function<int(XPRSprob prob, int attrib, char* value)> XPRSgetstrattrib;
extern std::function<int(XPRSprob prob, int attrib, char* value, int maxbytes, int* p_nbytes)> XPRSgetstringattrib;
extern std::function<int(XPRSprob prob, int attrib, double* p_value)> XPRSgetdblattrib;
extern std::function<int(XPRSprob prob, const char* name, int* p_id, int* p_type)> XPRSgetcontrolinfo;
extern std::function<int(XPRSprob prob, const char* name, int* p_id, int* p_type)> XPRSgetattribinfo;
extern std::function<int(XPRSprob prob, int objqcol1, int objqcol2, double* p_objqcoef)> XPRSgetqobj;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSreadprob;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const int start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSloadlp;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const XPRSint64 start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSloadlp64;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const int start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], int nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[])> XPRSloadqp;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const XPRSint64 start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], XPRSint64 nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[])> XPRSloadqp64;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const int start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], int nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[], const int nentities, const int nsets, const char coltype[], const int entind[], const double limit[], const char settype[], const int setstart[], const int setind[], const double refval[])> XPRSloadqglobal;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const XPRSint64 start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], XPRSint64 nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[], const int nentities, const int nsets, const char coltype[], const int entind[], const double limit[], const char settype[], const XPRSint64 setstart[], const int setind[], const double refval[])> XPRSloadqglobal64;
extern std::function<int(XPRSprob prob, int options)> XPRSfixglobals;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[])> XPRSloadmodelcuts;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[])> XPRSloaddelayedrows;
extern std::function<int(XPRSprob prob, int ndirs, const int colind[], const int priority[], const char dir[], const double uppseudo[], const double downpseudo[])> XPRSloaddirs;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], const int dir[])> XPRSloadbranchdirs;
extern std::function<int(XPRSprob prob, int ndirs, const int colind[], const int priority[], const char dir[], const double uppseudo[], const double downpseudo[])> XPRSloadpresolvedirs;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const int start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], int nentities, int nsets, const char coltype[], const int entind[], const double limit[], const char settype[], const int setstart[], const int setind[], const double refval[])> XPRSloadglobal;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const XPRSint64 start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], int nentities, int nsets, const char coltype[], const int entind[], const double limit[], const char settype[], const XPRSint64 setstart[], const int setind[], const double refval[])> XPRSloadglobal64;
extern std::function<int(XPRSprob prob, int type, const char names[], int first, int last)> XPRSaddnames;
extern std::function<int(XPRSprob prob, const char names[], int first, int last)> XPRSaddsetnames;
extern std::function<int(XPRSprob prob, const int rowscale[], const int colscale[])> XPRSscale;
extern std::function<int(XPRSprob prob, const char* filename)> XPRSreaddirs;
extern std::function<int(XPRSprob prob, const char* filename)> XPRSwritedirs;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], const int colind[], const int complement[])> XPRSsetindicators;
extern std::function<int(XPRSprob prob, int npwls, int npoints, const int colind[], const int resultant[], const int start[], const double xval[], const double yval[])> XPRSaddpwlcons;
extern std::function<int(XPRSprob prob, int npwls, XPRSint64 npoints, const int colind[], const int resultant[], const XPRSint64 start[], const double xval[], const double yval[])> XPRSaddpwlcons64;
extern std::function<int(XPRSprob prob, int colind[], int resultant[], int start[], double xval[], double yval[], int maxpoints, int* p_npoints, int first, int last)> XPRSgetpwlcons;
extern std::function<int(XPRSprob prob, int colind[], int resultant[], XPRSint64 start[], double xval[], double yval[], XPRSint64 maxpoints, XPRSint64* p_npoints, int first, int last)> XPRSgetpwlcons64;
extern std::function<int(XPRSprob prob, int ncons, int ncols, int nvals, const int contype[], const int resultant[], const int colstart[], const int colind[], const int valstart[], const double val[])> XPRSaddgencons;
extern std::function<int(XPRSprob prob, int ncons, XPRSint64 ncols, XPRSint64 nvals, const int contype[], const int resultant[], const XPRSint64 colstart[], const int colind[], const XPRSint64 valstart[], const double val[])> XPRSaddgencons64;
extern std::function<int(XPRSprob prob, int contype[], int resultant[], int colstart[], int colind[], int maxcols, int* p_ncols, int valstart[], double val[], int maxvals, int* p_nvals, int first, int last)> XPRSgetgencons;
extern std::function<int(XPRSprob prob, int contype[], int resultant[], XPRSint64 colstart[], int colind[], XPRSint64 maxcols, XPRSint64* p_ncols, XPRSint64 valstart[], double val[], XPRSint64 maxvals, XPRSint64* p_nvals, int first, int last)> XPRSgetgencons64;
extern std::function<int(XPRSprob prob, int npwls, const int pwlind[])> XPRSdelpwlcons;
extern std::function<int(XPRSprob prob, int ncons, const int conind[])> XPRSdelgencons;
extern std::function<int(XPRSprob prob)> XPRSdumpcontrols;
extern std::function<int(XPRSprob prob, int colind[], int complement[], int first, int last)> XPRSgetindicators;
extern std::function<int(XPRSprob prob, int first, int last)> XPRSdelindicators;
extern std::function<int(XPRSprob prob, int* p_ndir, int indices[], int prios[], char branchdirs[], double uppseudo[], double downpseudo[])> XPRSgetdirs;
extern std::function<int(XPRSprob prob, const char* flags)> XPRSlpoptimize;
extern std::function<int(XPRSprob prob, const char* flags)> XPRSmipoptimize;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSreadslxsol;
extern std::function<int(XPRSprob prob, const char* filename)> XPRSalter;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSreadbasis;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSreadbinsol;
extern std::function<int(XPRSprob prob, int* p_nprimalcols, int* p_nprimalrows, int* p_ndualrows, int* p_ndualcols, int x[], int slack[], int duals[], int djs[])> XPRSgetinfeas;
extern std::function<int(XPRSprob prob, int* p_nprimalcols, int* p_nprimalrows, int* p_ndualrows, int* p_ndualcols, int x[], int slack[], int duals[], int djs[])> XPRSgetscaledinfeas;
extern std::function<int(XPRSprob prob, int* p_seq)> XPRSgetunbvec;
extern std::function<int(XPRSprob prob, int* p_status)> XPRScrossoverlpsol;
extern std::function<int(XPRSprob prob, const char* flags)> XPRStune;
extern std::function<int(XPRSprob prob, const char* methodfile)> XPRStunerwritemethod;
extern std::function<int(XPRSprob prob, const char* methodfile)> XPRStunerreadmethod;
extern std::function<int(XPRSprob prob, int colstab[], int rowstab[])> XPRSgetbarnumstability;
extern std::function<int(XPRSprob prob, int pivotorder[])> XPRSgetpivotorder;
extern std::function<int(XPRSprob prob, int rowmap[], int colmap[])> XPRSgetpresolvemap;
extern std::function<int(XPRSprob prob, double vec[])> XPRSbtran;
extern std::function<int(XPRSprob prob, double vec[])> XPRSftran;
extern std::function<int(XPRSprob prob, double val[], int ind[], int* p_ncoefs)> XPRSsparsebtran;
extern std::function<int(XPRSprob prob, double val[], int ind[], int* p_ncoefs)> XPRSsparseftran;
extern std::function<int(XPRSprob prob, double objcoef[], int first, int last)> XPRSgetobj;
extern std::function<int(XPRSprob prob, double rhs[], int first, int last)> XPRSgetrhs;
extern std::function<int(XPRSprob prob, double rng[], int first, int last)> XPRSgetrhsrange;
extern std::function<int(XPRSprob prob, double lb[], int first, int last)> XPRSgetlb;
extern std::function<int(XPRSprob prob, double ub[], int first, int last)> XPRSgetub;
extern std::function<int(XPRSprob prob, int start[], int rowind[], double rowcoef[], int maxcoefs, int* p_ncoefs, int first, int last)> XPRSgetcols;
extern std::function<int(XPRSprob prob, XPRSint64 start[], int rowind[], double rowcoef[], XPRSint64 maxcoefs, XPRSint64* p_ncoefs, int first, int last)> XPRSgetcols64;
extern std::function<int(XPRSprob prob, int start[], int colind[], double colcoef[], int maxcoefs, int* p_ncoefs, int first, int last)> XPRSgetrows;
extern std::function<int(XPRSprob prob, XPRSint64 start[], int colind[], double colcoef[], XPRSint64 maxcoefs, XPRSint64* p_ncoefs, int first, int last)> XPRSgetrows64;
extern std::function<int(XPRSprob prob, int flags[], int first, int last)> XPRSgetrowflags;
extern std::function<int(XPRSprob prob, const int flags[], int first, int last)> XPRSclearrowflags;
extern std::function<int(XPRSprob prob, int row, int col, double* p_coef)> XPRSgetcoef;
extern std::function<int(XPRSprob prob, int start[], int colind[], double objqcoef[], int maxcoefs, int* p_ncoefs, int first, int last)> XPRSgetmqobj;
extern std::function<int(XPRSprob prob, XPRSint64 start[], int colind[], double objqcoef[], XPRSint64 maxcoefs, XPRSint64* p_ncoefs, int first, int last)> XPRSgetmqobj64;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwritebasis;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwritesol;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwritebinsol;
extern std::function<int(XPRSprob prob, double x[], double slack[], double duals[], double djs[])> XPRSgetsol;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwriteprtsol;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwriteslxsol;
extern std::function<int(XPRSprob prob, double x[], double slack[], double duals[], double djs[])> XPRSgetpresolvesol;
extern std::function<int(XPRSprob prob, double x[], double slack[], double duals[], double djs[], int* p_status)> XPRSgetlastbarsol;
extern std::function<int(XPRSprob prob)> XPRSiisclear;
extern std::function<int(XPRSprob prob, int mode, int* p_status)> XPRSiisfirst;
extern std::function<int(XPRSprob prob, int* p_status)> XPRSiisnext;
extern std::function<int(XPRSprob prob, int* p_niis, int nrows[], int ncols[], double suminfeas[], int numinfeas[])> XPRSiisstatus;
extern std::function<int(XPRSprob prob)> XPRSiisall;
extern std::function<int(XPRSprob prob, int iis, const char* filename, int filetype, const char* flags)> XPRSiiswrite;
extern std::function<int(XPRSprob prob, int iis)> XPRSiisisolations;
extern std::function<int(XPRSprob prob, int iis, int* p_nrows, int* p_ncols, int rowind[], int colind[], char contype[], char bndtype[], double duals[], double djs[], char isolationrows[], char isolationcols[])> XPRSgetiisdata;
extern std::function<int(XPRSprob prob, int* p_ncols, int* p_nrows, int colind[], int rowind[])> XPRSgetiis;
extern std::function<int(XPRSprob prob, const int rowstat[], const int colstat[])> XPRSloadpresolvebasis;
extern std::function<int(XPRSprob prob, int* p_nentities, int* p_nsets, char coltype[], int colind[], double limit[], char settype[], int start[], int setcols[], double refval[])> XPRSgetglobal;
extern std::function<int(XPRSprob prob, int* p_nentities, int* p_nsets, char coltype[], int colind[], double limit[], char settype[], XPRSint64 start[], int setcols[], double refval[])> XPRSgetglobal64;
extern std::function<int(XPRSprob prob, int nrows, int ncols, const int rowind[], const int colind[])> XPRSloadsecurevecs;
extern std::function<int(XPRSprob prob, int nrows, int ncoefs, const char rowtype[], const double rhs[], const double rng[], const int start[], const int colind[], const double rowcoef[])> XPRSaddrows;
extern std::function<int(XPRSprob prob, int nrows, XPRSint64 ncoefs, const char rowtype[], const double rhs[], const double rng[], const XPRSint64 start[], const int colind[], const double rowcoef[])> XPRSaddrows64;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[])> XPRSdelrows;
extern std::function<int(XPRSprob prob, int ncols, int ncoefs, const double objcoef[], const int start[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSaddcols;
extern std::function<int(XPRSprob prob, int ncols, XPRSint64 ncoefs, const double objcoef[], const XPRSint64 start[], const int rowind[], const double rowcoef[], const double lb[], const double ub[])> XPRSaddcols64;
extern std::function<int(XPRSprob prob, int ncols, const int colind[])> XPRSdelcols;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], const char coltype[])> XPRSchgcoltype;
extern std::function<int(XPRSprob prob, const int rowstat[], const int colstat[])> XPRSloadbasis;
extern std::function<int(XPRSprob prob)> XPRSpostsolve;
extern std::function<int(XPRSprob prob, int nsets, const int setind[])> XPRSdelsets;
extern std::function<int(XPRSprob prob, int nsets, int nelems, const char settype[], const int start[], const int colind[], const double refval[])> XPRSaddsets;
extern std::function<int(XPRSprob prob, int nsets, XPRSint64 nelems, const char settype[], const XPRSint64 start[], const int colind[], const double refval[])> XPRSaddsets64;
extern std::function<int(XPRSprob prob, const int nbounds, const int colind[], const char bndtype[], const double bndval[], const int iterlim, double objval[], int status[])> XPRSstrongbranch;
extern std::function<int(XPRSprob prob, const int nrows, const int rowind[], const int iterlim, double mindual[], double maxdual[])> XPRSestimaterowdualranges;
extern std::function<int(XPRSprob prob, int msgcode, int status)> XPRSsetmessagestatus;
extern std::function<int(XPRSprob prob, int msgcode, int* p_status)> XPRSgetmessagestatus;
extern std::function<int(XPRSprob prob, int objsense)> XPRSchgobjsense;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], const double limit[])> XPRSchgglblimit;
extern std::function<int(XPRSprob prob, const char* probname, const char* flags)> XPRSrestore;
extern std::function<int(XPRSprob prob, int enter, int leave)> XPRSpivot;
extern std::function<int(XPRSprob prob, const double x[], const double slack[], const double duals[], const double djs[], int* p_status)> XPRSloadlpsol;
extern std::function<int(XPRSobject xprsobj, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode)> XPRSlogfilehandler;
extern std::function<int(XPRSprob prob, int* p_status, const double lepref[], const double gepref[], const double lbpref[], const double ubpref[], char phase2, double delta, const char* flags)> XPRSrepairweightedinfeas;
extern std::function<int(XPRSprob prob, int* p_status, const double lepref[], const double gepref[], const double lbpref[], const double ubpref[], const double lerelax[], const double gerelax[], const double lbrelax[], const double ubrelax[], char phase2, double delta, const char* flags)> XPRSrepairweightedinfeasbounds;
extern std::function<int(XPRSprob prob, int* p_status, char penalty, char phase2, char flags, double lepref, double gepref, double lbpref, double ubpref, double delta)> XPRSrepairinfeas;
extern std::function<int(XPRSprob prob, int type, int norm, int scaled, double* p_value)> XPRSbasisstability;
extern std::function<int(XPRSprob prob, int type, const char* name, int* p_index)> XPRSgetindex;
extern std::function<int(XPRSprob prob, char* errmsg)> XPRSgetlasterror;
extern std::function<int(XPRSobject xprsobj, const char** p_name)> XPRSgetobjecttypename;
extern std::function<int(XPRSprob prob, double ray[], int* p_hasray)> XPRSgetprimalray;
extern std::function<int(XPRSprob prob, double ray[], int* p_hasray)> XPRSgetdualray;
extern std::function<int(XPRSprob prob, const int nbounds, const int colind[], const char bndtype[], const double bndval[], const int iterlim, double objval[], int status[], int (XPRS_CC *callback)(XPRSprob prob, void* vContext, int ibnd), void* data)> XPRSstrongbranchcb;
extern std::function<int(XPRSprob prob, const double x[], int* p_status)> XPRSloadmipsol;
extern std::function<int(XPRSprob prob, int rowstat[], int colstat[])> XPRSgetbasis;
extern std::function<int(XPRSprob prob, int row, int col, int* p_rowstat, int* p_colstat)> XPRSgetbasisval;
extern std::function<int(XPRSprob prob, int ncuts, const int cuttype[], const char rowtype[], const double rhs[], const int start[], const int colind[], const double cutcoef[])> XPRSaddcuts;
extern std::function<int(XPRSprob prob, int ncuts, const int cuttype[], const char rowtype[], const double rhs[], const XPRSint64 start[], const int colind[], const double cutcoef[])> XPRSaddcuts64;
extern std::function<int(XPRSprob prob, int basis, int cuttype, int interp, double delta, int ncuts, const XPRScut cutind[])> XPRSdelcuts;
extern std::function<int(XPRSprob prob, int cuttype, int interp, int ncuts, const XPRScut cutind[])> XPRSdelcpcuts;
extern std::function<int(XPRSprob prob, int cuttype, int interp, int* p_ncuts, int maxcuts, XPRScut cutind[])> XPRSgetcutlist;
extern std::function<int(XPRSprob prob, int cuttype, int interp, double delta, int* p_ncuts, int maxcuts, XPRScut cutind[], double viol[])> XPRSgetcpcutlist;
extern std::function<int(XPRSprob prob, const XPRScut rowind[], int ncuts, int maxcoefs, int cuttype[], char rowtype[], int start[], int colind[], double cutcoef[], double rhs[])> XPRSgetcpcuts;
extern std::function<int(XPRSprob prob, const XPRScut rowind[], int ncuts, XPRSint64 maxcoefs, int cuttype[], char rowtype[], XPRSint64 start[], int colind[], double cutcoef[], double rhs[])> XPRSgetcpcuts64;
extern std::function<int(XPRSprob prob, int coltype, int interp, int ncuts, const XPRScut cutind[])> XPRSloadcuts;
extern std::function<int(XPRSprob prob, int ncuts, int nodups, const int cuttype[], const char rowtype[], const double rhs[], const int start[], XPRScut cutind[], const int colind[], const double cutcoef[])> XPRSstorecuts;
extern std::function<int(XPRSprob prob, int ncuts, int nodups, const int cuttype[], const char rowtype[], const double rhs[], const XPRSint64 start[], XPRScut cutind[], const int colind[], const double cutcoef[])> XPRSstorecuts64;
extern std::function<int(XPRSprob prob, char rowtype, int norigcoefs, const int origcolind[], const double origrowcoef[], double origrhs, int maxcoefs, int* p_ncoefs, int colind[], double rowcoef[], double* p_rhs, int* p_status)> XPRSpresolverow;
extern std::function<int(XPRSprob prob, int nbounds, const int colind[], const char bndtype[], const double bndval[], void** p_bounds)> XPRSstorebounds;
extern std::function<int(XPRSprob prob, int ncuts, const XPRScut cutind[])> XPRSsetbranchcuts;
extern std::function<int(XPRSprob prob, void* bounds)> XPRSsetbranchbounds;
extern std::function<int(XPRSprob prob, int enter, int outlist[], double x[], double* p_objval, int* p_npivots, int maxpivots)> XPRSgetpivots;
extern std::function<int(XPRSprob prob, const char* filename, const char* flags)> XPRSwriteprob;
extern std::function<int(XPRSprob prob, const double solution[], double slacks[])> XPRScalcslacks;
extern std::function<int(XPRSprob prob, const double duals[], const double solution[], double djs[])> XPRScalcreducedcosts;
extern std::function<int(XPRSprob prob, const double solution[], double* p_objval)> XPRScalcobjective;
extern std::function<int(XPRSprob prob, const double solution[], const double duals[], int property, double* p_value)> XPRScalcsolinfo;
extern std::function<int(XPRSprob prob, char rowtype[], int first, int last)> XPRSgetrowtype;
extern std::function<int(XPRSprob prob, int rowstat[], int colstat[])> XPRSgetpresolvebasis;
extern std::function<int(XPRSprob prob, char coltype[], int first, int last)> XPRSgetcoltype;
extern std::function<int(XPRSprob prob)> XPRSsave;
extern std::function<int(XPRSprob prob, const char* filename)> XPRSsaveas;
extern std::function<int(XPRSprob prob, int type, char names[], int maxbytes, int* p_nbytes, int first, int last)> XPRSgetnamelist;
extern std::function<int(XPRSprob prob, int type, XPRSnamelist* p_nml)> XPRSgetnamelistobject;
extern std::function<int(XPRSprob prob, int length, const double solval[], const int colind[], const char* name)> XPRSaddmipsol;
extern std::function<int(XPRSprob prob, XPRScut cutind, double* p_slack)> XPRSgetcutslack;
extern std::function<int(XPRSprob prob, int ncuts, const XPRScut cutind[], int cutmap[])> XPRSgetcutmap;
extern std::function<int(XPRSprob prob, int type, char names[], int first, int last)> XPRSgetnames;
extern std::function<int(XPRSprob prob, double x[], double slack[], double duals[], double djs[])> XPRSgetlpsol;
extern std::function<int(XPRSprob prob, int col, int row, double* p_x, double* p_slack, double* p_dual, double* p_dj)> XPRSgetlpsolval;
extern std::function<int(XPRSprob prob, double x[], double slack[])> XPRSgetmipsol;
extern std::function<int(XPRSprob prob, int col, int row, double* p_x, double* p_slack)> XPRSgetmipsolval;
extern std::function<int(XPRSprob prob, int nbounds, const int colind[], const char bndtype[], const double bndval[])> XPRSchgbounds;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], const double objcoef[])> XPRSchgobj;
extern std::function<int(XPRSprob prob, int row, int col, double coef)> XPRSchgcoef;
extern std::function<int(XPRSprob prob, int ncoefs, const int rowind[], const int colind[], const double rowcoef[])> XPRSchgmcoef;
extern std::function<int(XPRSprob prob, XPRSint64 ncoefs, const int rowind[], const int colind[], const double rowcoef[])> XPRSchgmcoef64;
extern std::function<int(XPRSprob prob, int ncoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[])> XPRSchgmqobj;
extern std::function<int(XPRSprob prob, XPRSint64 ncoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[])> XPRSchgmqobj64;
extern std::function<int(XPRSprob prob, int objqcol1, int objqcol2, double objqcoef)> XPRSchgqobj;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], const double rhs[])> XPRSchgrhs;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], const double rng[])> XPRSchgrhsrange;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], const char rowtype[])> XPRSchgrowtype;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_lplog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcblplog;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_lplog)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcblplog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_lplog)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcblplog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_lplog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecblplog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_globallog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbgloballog;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_globallog)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbgloballog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_globallog)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbgloballog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_globallog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbgloballog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_cutlog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbcutlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_cutlog)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbcutlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_cutlog)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbcutlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_cutlog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbcutlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_barlog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbbarlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_barlog)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbbarlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_barlog)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbbarlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_barlog)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbbarlog;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_cutmgr)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbcutmgr;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_cutmgr)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbcutmgr;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_cutmgr)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbcutmgr;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_cutmgr)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbcutmgr;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node), void* p)> XPRSsetcbchgnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node), void** p)> XPRSgetcbchgnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node), void* p, int priority)> XPRSaddcbchgnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgnode)(XPRSprob cbprob, void* cbdata, int* p_node), void* p)> XPRSremovecbchgnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void* p)> XPRSsetcboptnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void** p)> XPRSgetcboptnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void* p, int priority)> XPRSaddcboptnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_optnode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void* p)> XPRSremovecboptnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void* p)> XPRSsetcbprenode;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void** p)> XPRSgetcbprenode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void* p, int priority)> XPRSaddcbprenode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_prenode)(XPRSprob cbprob, void* cbdata, int* p_infeasible), void* p)> XPRSremovecbprenode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_infnode)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbinfnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_infnode)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbinfnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_infnode)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbinfnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_infnode)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbinfnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node), void* p)> XPRSsetcbnodecutoff;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node), void** p)> XPRSgetcbnodecutoff;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node), void* p, int priority)> XPRSaddcbnodecutoff;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_nodecutoff)(XPRSprob cbprob, void* cbdata, int node), void* p)> XPRSremovecbnodecutoff;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_intsol)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_intsol)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_intsol)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_intsol)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype, int* p_reject, double* p_cutoff), void* p)> XPRSsetcbpreintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype, int* p_reject, double* p_cutoff), void** p)> XPRSgetcbpreintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype, int* p_reject, double* p_cutoff), void* p, int priority)> XPRSaddcbpreintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_preintsol)(XPRSprob cbprob, void* cbdata, int soltype, int* p_reject, double* p_cutoff), void* p)> XPRSremovecbpreintsol;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_up, double* p_estdeg), void* p)> XPRSsetcbchgbranch;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_up, double* p_estdeg), void** p)> XPRSgetcbchgbranch;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_up, double* p_estdeg), void* p, int priority)> XPRSaddcbchgbranch;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgbranch)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_up, double* p_estdeg), void* p)> XPRSremovecbchgbranch;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_estimate)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_prio, double* p_degbest, double* p_degworst, double* p_current, int* p_preferred, int* p_ninf, double* p_degsum, int* p_nbranches), void* p)> XPRSsetcbestimate;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_estimate)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_prio, double* p_degbest, double* p_degworst, double* p_current, int* p_preferred, int* p_ninf, double* p_degsum, int* p_nbranches), void** p)> XPRSgetcbestimate;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_estimate)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_prio, double* p_degbest, double* p_degworst, double* p_current, int* p_preferred, int* p_ninf, double* p_degsum, int* p_nbranches), void* p, int priority)> XPRSaddcbestimate;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_estimate)(XPRSprob cbprob, void* cbdata, int* p_entity, int* p_prio, double* p_degbest, double* p_degworst, double* p_current, int* p_preferred, int* p_ninf, double* p_degsum, int* p_nbranches), void* p)> XPRSremovecbestimate;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_sepnode)(XPRSprob cbprob, void* cbdata, int branch, int entity, int up, double current), void* p)> XPRSsetcbsepnode;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_sepnode)(XPRSprob cbprob, void* cbdata, int branch, int entity, int up, double current), void** p)> XPRSgetcbsepnode;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_sepnode)(XPRSprob cbprob, void* cbdata, int branch, int entity, int up, double current), void* p, int priority)> XPRSaddcbsepnode;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_sepnode)(XPRSprob cbprob, void* cbdata, int branch, int entity, int up, double current), void* p)> XPRSremovecbsepnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_message)(XPRSprob cbprob, void* cbdata, const char* msg, int msglen, int msgtype), void* p)> XPRSsetcbmessage;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_message)(XPRSprob cbprob, void* cbdata, const char* msg, int msglen, int msgtype), void** p)> XPRSgetcbmessage;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_message)(XPRSprob cbprob, void* cbdata, const char* msg, int msglen, int msgtype), void* p, int priority)> XPRSaddcbmessage;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_message)(XPRSprob cbprob, void* cbdata, const char* msg, int msglen, int msgtype), void* p)> XPRSremovecbmessage;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_mipthread)(XPRSprob cbprob, void* cbdata, XPRSprob threadprob), void* p)> XPRSsetcbmipthread;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_mipthread)(XPRSprob cbprob, void* cbdata, XPRSprob threadprob), void** p)> XPRSgetcbmipthread;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_mipthread)(XPRSprob cbprob, void* cbdata, XPRSprob threadprob), void* p, int priority)> XPRSaddcbmipthread;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_mipthread)(XPRSprob cbprob, void* cbdata, XPRSprob threadprob), void* p)> XPRSremovecbmipthread;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_destroymt)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbdestroymt;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_destroymt)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbdestroymt;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_destroymt)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbdestroymt;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_destroymt)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbdestroymt;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode, int newnode, int branch), void* p)> XPRSsetcbnewnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode, int newnode, int branch), void** p)> XPRSgetcbnewnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode, int newnode, int branch), void* p, int priority)> XPRSaddcbnewnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_newnode)(XPRSprob cbprob, void* cbdata, int parentnode, int newnode, int branch), void* p)> XPRSremovecbnewnode;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_bariteration)(XPRSprob cbprob, void* cbdata, int* p_action), void* p)> XPRSsetcbbariteration;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_bariteration)(XPRSprob cbprob, void* cbdata, int* p_action), void** p)> XPRSgetcbbariteration;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_bariteration)(XPRSprob cbprob, void* cbdata, int* p_action), void* p, int priority)> XPRSaddcbbariteration;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_bariteration)(XPRSprob cbprob, void* cbdata, int* p_action), void* p)> XPRSremovecbbariteration;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_presolve)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbpresolve;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_presolve)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbpresolve;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_presolve)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbpresolve;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_presolve)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbpresolve;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgbranchobject)(XPRSprob cbprob, void* cbdata, XPRSbranchobject branch, XPRSbranchobject* p_newbranch), void* p)> XPRSsetcbchgbranchobject;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_chgbranchobject)(XPRSprob cbprob, void* cbdata, XPRSbranchobject branch, XPRSbranchobject* p_newbranch), void** p)> XPRSgetcbchgbranchobject;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgbranchobject)(XPRSprob cbprob, void* cbdata, XPRSbranchobject branch, XPRSbranchobject* p_newbranch), void* p, int priority)> XPRSaddcbchgbranchobject;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_chgbranchobject)(XPRSprob cbprob, void* cbdata, XPRSbranchobject branch, XPRSbranchobject* p_newbranch), void* p)> XPRSremovecbchgbranchobject;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_computerestart)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbcomputerestart;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_computerestart)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbcomputerestart;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_computerestart)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbcomputerestart;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_computerestart)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbcomputerestart;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_gapnotify)(XPRSprob cbprob, void* cbdata, double* p_relgapnotifytarget, double* p_absgapnotifytarget, double* p_absgapnotifyobjtarget, double* p_absgapnotifyboundtarget), void* p)> XPRSsetcbgapnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_gapnotify)(XPRSprob cbprob, void* cbdata, double* p_relgapnotifytarget, double* p_absgapnotifytarget, double* p_absgapnotifyobjtarget, double* p_absgapnotifyboundtarget), void** p)> XPRSgetcbgapnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_gapnotify)(XPRSprob cbprob, void* cbdata, double* p_relgapnotifytarget, double* p_absgapnotifytarget, double* p_absgapnotifyobjtarget, double* p_absgapnotifyboundtarget), void* p, int priority)> XPRSaddcbgapnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_gapnotify)(XPRSprob cbprob, void* cbdata, double* p_relgapnotifytarget, double* p_absgapnotifytarget, double* p_absgapnotifyobjtarget, double* p_absgapnotifyboundtarget), void* p)> XPRSremovecbgapnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_usersolnotify)(XPRSprob cbprob, void* cbdata, const char* solname, int status), void* p)> XPRSsetcbusersolnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_usersolnotify)(XPRSprob cbprob, void* cbdata, const char* solname, int status), void** p)> XPRSgetcbusersolnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_usersolnotify)(XPRSprob cbprob, void* cbdata, const char* solname, int status), void* p, int priority)> XPRSaddcbusersolnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_usersolnotify)(XPRSprob cbprob, void* cbdata, const char* solname, int status), void* p)> XPRSremovecbusersolnotify;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_beforesolve)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbbeforesolve;
extern std::function<int(XPRSprob prob, void (XPRS_CC **f_beforesolve)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbbeforesolve;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_beforesolve)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbbeforesolve;
extern std::function<int(XPRSprob prob, void (XPRS_CC *f_beforesolve)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbbeforesolve;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_checktime)(XPRSprob cbprob, void* cbdata), void* p)> XPRSsetcbchecktime;
extern std::function<int(XPRSprob prob, int (XPRS_CC **f_checktime)(XPRSprob cbprob, void* cbdata), void** p)> XPRSgetcbchecktime;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_checktime)(XPRSprob cbprob, void* cbdata), void* p, int priority)> XPRSaddcbchecktime;
extern std::function<int(XPRSprob prob, int (XPRS_CC *f_checktime)(XPRSprob cbprob, void* cbdata), void* p)> XPRSremovecbchecktime;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], double lower[], double upper[])> XPRSobjsa;
extern std::function<int(XPRSprob prob, int ncols, const int colind[], double lblower[], double lbupper[], double ublower[], double ubupper[])> XPRSbndsa;
extern std::function<int(XPRSprob prob, int nrows, const int rowind[], double lower[], double upper[])> XPRSrhssa;
extern std::function<int(int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p)> XPRS_ge_setcbmsghandler;
extern std::function<int(int (XPRS_CC **f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void** p)> XPRS_ge_getcbmsghandler;
extern std::function<int(int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p, int priority)> XPRS_ge_addcbmsghandler;
extern std::function<int(int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p)> XPRS_ge_removecbmsghandler;
extern std::function<int(int consistent)> XPRS_ge_setarchconsistency;
extern std::function<int(int safemode)> XPRS_ge_setsafemode;
extern std::function<int(int* p_safemode)> XPRS_ge_getsafemode;
extern std::function<int(int debugmode)> XPRS_ge_setdebugmode;
extern std::function<int(int* p_debugmode)> XPRS_ge_getdebugmode;
extern std::function<int(int* p_msgcode, char* msg, int maxbytes, int* p_nbytes)> XPRS_ge_getlasterror;
extern std::function<int(int allow)> XPRS_ge_setcomputeallowed;
extern std::function<int(int* p_allow)> XPRS_ge_getcomputeallowed;
extern std::function<int(XPRSmipsolpool* msp)> XPRS_msp_create;
extern std::function<int(XPRSmipsolpool msp)> XPRS_msp_destroy;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob)> XPRS_msp_probattach;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob)> XPRS_msp_probdetach;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int iRankAttrib, int bRankAscending, int iRankFirstIndex_Ob, int iRankLastIndex_Ob, int iSolutionIds_Zb[], int* nReturnedSolIds, int* nSols)> XPRS_msp_getsollist;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int iRankAttrib, int bRankAscending, int iRankFirstIndex_Ob, int iRankLastIndex_Ob, int bUseUserBitFilter, int iUserBitMask, int iUserBitPattern, int bUseInternalBitFilter, int iInternalBitMask, int iInternalBitPattern, int iSolutionIds_Zb[], int* nReturnedSolIds, int* nSols)> XPRS_msp_getsollist2;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_, double x[], int iColFirst, int iColLast, int* nValuesReturned)> XPRS_msp_getsol;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int iSolutionId, int* iSolutionIdStatus_, double slack[], int iRowFirst, int iRowLast, int* nValuesReturned)> XPRS_msp_getslack;
extern std::function<int(XPRSmipsolpool msp, int* iSolutionId, const double x[], int nCols, const char* sSolutionName, int* bNameModifiedForUniqueness, int* iSolutionIdOfExistingDuplicatePreventedLoad)> XPRS_msp_loadsol;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_)> XPRS_msp_delsol;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int iSolutionId, int* iSolutionIdStatus_, int iAttribId, int* Dst)> XPRS_msp_getintattribprobsol;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int iSolutionId, int* iSolutionIdStatus_, int iAttribId, double* Dst)> XPRS_msp_getdblattribprobsol;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob, int iAttribId, int* Dst)> XPRS_msp_getintattribprob;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob, int iAttribId, double* Dst)> XPRS_msp_getdblattribprob;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_, int iAttribId, int* Dst)> XPRS_msp_getintattribsol;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_, int iAttribId, double* Dst)> XPRS_msp_getdblattribsol;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_, int iControlId, int* Val)> XPRS_msp_getintcontrolsol;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_, int iControlId, double* Val)> XPRS_msp_getdblcontrolsol;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_, int iControlId, int Val)> XPRS_msp_setintcontrolsol;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, int* iSolutionIdStatus_, int iControlId, double Val)> XPRS_msp_setdblcontrolsol;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int bGet_Max_Otherwise_Min, int* iSolutionId, int iAttribId, int* ExtremeVal)> XPRS_msp_getintattribprobextreme;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_to_rank_against, int bGet_Max_Otherwise_Min, int* iSolutionId, int iAttribId, double* ExtremeVal)> XPRS_msp_getdblattribprobextreme;
extern std::function<int(XPRSmipsolpool msp, int iAttribId, int* Val)> XPRS_msp_getintattrib;
extern std::function<int(XPRSmipsolpool msp, int iAttribId, double* Val)> XPRS_msp_getdblattrib;
extern std::function<int(XPRSmipsolpool msp, int iControlId, int* Val)> XPRS_msp_getintcontrol;
extern std::function<int(XPRSmipsolpool msp, int iControlId, double* Val)> XPRS_msp_getdblcontrol;
extern std::function<int(XPRSmipsolpool msp, int iControlId, int Val)> XPRS_msp_setintcontrol;
extern std::function<int(XPRSmipsolpool msp, int iControlId, double Val)> XPRS_msp_setdblcontrol;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, const char* sNewSolutionBaseName, int* bNameModifiedForUniqueness, int* iSolutionIdStatus_)> XPRS_msp_setsolname;
extern std::function<int(XPRSmipsolpool msp, int iSolutionId, char* _sname, int _iStringBufferBytes, int* _iBytesInInternalString, int* iSolutionIdStatus_)> XPRS_msp_getsolname;
extern std::function<int(XPRSmipsolpool msp, const char* sSolutionName, int* iSolutionId)> XPRS_msp_findsolbyname;
extern std::function<int(XPRSmipsolpool msp, XPRSprob prob_context, int iSolutionId, int* iSolutionIdStatus_, const char* sFileName, const char* sFlags)> XPRS_msp_writeslxsol;
extern std::function<int(XPRSmipsolpool msp, XPRSnamelist col_name_list, const char* sFileName, const char* sFlags, int* iSolutionId_Beg, int* iSolutionId_End)> XPRS_msp_readslxsol;
extern std::function<int(XPRSmipsolpool msp, int* iMsgCode, char* _msg, int _iStringBufferBytes, int* _iBytesInInternalString)> XPRS_msp_getlasterror;
extern std::function<int(XPRSmipsolpool msp, int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p)> XPRS_msp_setcbmsghandler;
extern std::function<int(XPRSmipsolpool msp, int (XPRS_CC **f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void** p)> XPRS_msp_getcbmsghandler;
extern std::function<int(XPRSmipsolpool msp, int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p, int priority)> XPRS_msp_addcbmsghandler;
extern std::function<int(XPRSmipsolpool msp, int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p)> XPRS_msp_removecbmsghandler;
extern std::function<int(XPRSnamelist* p_nml)> XPRS_nml_create;
extern std::function<int(XPRSnamelist nml)> XPRS_nml_destroy;
extern std::function<int(XPRSnamelist nml, int* p_count)> XPRS_nml_getnamecount;
extern std::function<int(XPRSnamelist nml, int* p_namelen)> XPRS_nml_getmaxnamelen;
extern std::function<int(XPRSnamelist nml, int pad, char buffer[], int maxbytes, int* p_nbytes, int first, int last)> XPRS_nml_getnames;
extern std::function<int(XPRSnamelist nml, const char names[], int first, int last)> XPRS_nml_addnames;
extern std::function<int(XPRSnamelist nml, int first, int last)> XPRS_nml_removenames;
extern std::function<int(XPRSnamelist nml, const char* name, int* p_index)> XPRS_nml_findname;
extern std::function<int(XPRSnamelist dest, XPRSnamelist src)> XPRS_nml_copynames;
extern std::function<int(XPRSnamelist nml, int* p_msgcode, char* msg, int maxbytes, int* p_nbytes)> XPRS_nml_getlasterror;
extern std::function<int(XPRSprob prob, int row, int ncoefs, const int rowqcol1[], const int rowqcol2[], const double rowqcoef[])> XPRSaddqmatrix;
extern std::function<int(XPRSprob prob, int row, XPRSint64 ncoefs, const int rowqcol1[], const int rowqcol2[], const double rowqcoef[])> XPRSaddqmatrix64;
extern std::function<int(XPRSprob prob, int row)> XPRSdelqmatrix;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const int start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], int nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[], int nqrows, const int qrowind[], const int nrowqcoef[], const int rowqcol1[], const int rowqcol2[], const double rowqcoef[])> XPRSloadqcqp;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const XPRSint64 start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], XPRSint64 nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[], int nqrows, const int qrowind[], const XPRSint64 nrowqcoef[], const int rowqcol1[], const int rowqcol2[], const double rowqcoef[])> XPRSloadqcqp64;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const int start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], int nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[], int nqrows, const int qrowind[], const int nrowqcoefs[], const int rowqcol1[], const int rowqcol2[], const double rowqcoef[], const int nentities, const int nsets, const char coltype[], const int entind[], const double limit[], const char settype[], const int setstart[], const int setind[], const double refval[])> XPRSloadqcqpglobal;
extern std::function<int(XPRSprob prob, const char* probname, int ncols, int nrows, const char rowtype[], const double rhs[], const double rng[], const double objcoef[], const XPRSint64 start[], const int collen[], const int rowind[], const double rowcoef[], const double lb[], const double ub[], XPRSint64 nobjqcoefs, const int objqcol1[], const int objqcol2[], const double objqcoef[], int nqrows, const int qrowind[], const XPRSint64 nrowqcoefs[], const int rowqcol1[], const int rowqcol2[], const double rowqcoef[], const int nentities, const int nsets, const char coltype[], const int entind[], const double limit[], const char settype[], const XPRSint64 setstart[], const int setind[], const double refval[])> XPRSloadqcqpglobal64;
extern std::function<int(XPRSprob prob, int row, int rowqcol1, int rowqcol2, double* p_rowqcoef)> XPRSgetqrowcoeff;
extern std::function<int(XPRSprob prob, int row, int start[], int colind[], double rowqcoef[], int maxcoefs, int* p_ncoefs, int first, int last)> XPRSgetqrowqmatrix;
extern std::function<int(XPRSprob prob, int row, int* p_ncoefs, int rowqcol1[], int rowqcol2[], double rowqcoef[])> XPRSgetqrowqmatrixtriplets;
extern std::function<int(XPRSprob prob, int row, int rowqcol1, int rowqcol2, double rowqcoef)> XPRSchgqrowcoeff;
extern std::function<int(XPRSprob prob, int* p_nrows, int rowind[])> XPRSgetqrows;
extern std::function<int(XPRSmipsolenum* mse)> XPRS_mse_create;
extern std::function<int(XPRSmipsolenum mse)> XPRS_mse_destroy;
extern std::function<int(XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, int (XPRS_CC *f_mse_handler)(XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, void* vContext, int* nMaxSols, const double x_Zb[], const int nCols, const double dMipObject, double* dModifiedObject, int* bRejectSoln, int* bUpdateMipAbsCutOffOnCurrentSet), void* p, int* nMaxSols)> XPRS_mse_minim;
extern std::function<int(XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, int (XPRS_CC *f_mse_handler)(XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, void* vContext, int* nMaxSols, const double x_Zb[], const int nCols, const double dMipObject, double* dModifiedObject, int* bRejectSoln, int* bUpdateMipAbsCutOffOnCurrentSet), void* p, int* nMaxSols)> XPRS_mse_maxim;
extern std::function<int(XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, int (XPRS_CC *f_mse_handler)(XPRSmipsolenum mse, XPRSprob prob, XPRSmipsolpool msp, void* vContext, int* nMaxSols, const double x_Zb[], const int nCols, const double dMipObject, double* dModifiedObject, int* bRejectSoln, int* bUpdateMipAbsCutOffOnCurrentSet), void* p, int* nMaxSols)> XPRS_mse_opt;
extern std::function<int(XPRSmipsolenum mse, int iMetricId, int iRankFirstIndex_Ob, int iRankLastIndex_Ob, int iSolutionIds[], int* nReturnedSolIds, int* nSols)> XPRS_mse_getsollist;
extern std::function<int(XPRSmipsolenum mse, int iSolutionId, int* iSolutionIdStatus, int iMetricId, double* dMetric)> XPRS_mse_getsolmetric;
extern std::function<int(XPRSmipsolenum mse, int iMetricId, int cull_sol_id_list[], int nMaxSolsToCull, int* nSolsToCull, double dNewSolMetric, const double x[], int nCols, int* bRejectSoln)> XPRS_mse_getcullchoice;
extern std::function<int(XPRSmipsolenum mse, int iAttribId, int* Val)> XPRS_mse_getintattrib;
extern std::function<int(XPRSmipsolenum mse, int iAttribId, double* Val)> XPRS_mse_getdblattrib;
extern std::function<int(XPRSmipsolenum mse, int iAttribId, int* Val)> XPRS_mse_getintcontrol;
extern std::function<int(XPRSmipsolenum mse, int iAttribId, double* Val)> XPRS_mse_getdblcontrol;
extern std::function<int(XPRSmipsolenum mse, int iAttribId, int Val)> XPRS_mse_setintcontrol;
extern std::function<int(XPRSmipsolenum mse, int iAttribId, double Val)> XPRS_mse_setdblcontrol;
extern std::function<int(XPRSmipsolenum mse, int* iMsgCode, char* _msg, int _iStringBufferBytes, int* _iBytesInInternalString)> XPRS_mse_getlasterror;
extern std::function<int(XPRSmipsolenum mse, const char* sSolutionBaseName)> XPRS_mse_setsolbasename;
extern std::function<int(XPRSmipsolenum mse, char* _sname, int _iStringBufferBytes, int* _iBytesInInternalString)> XPRS_mse_getsolbasename;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC *f_mse_getsolutiondiff)(XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1, int iElemCount_1, double dMipObj_1, const double Vals_1[], const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2, double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[], double* dDiffMetric), void* p)> XPRS_mse_setcbgetsolutiondiff;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC **f_mse_getsolutiondiff)(XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1, int iElemCount_1, double dMipObj_1, const double Vals_1[], const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2, double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[], double* dDiffMetric), void** p)> XPRS_mse_getcbgetsolutiondiff;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC *f_mse_getsolutiondiff)(XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1, int iElemCount_1, double dMipObj_1, const double Vals_1[], const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2, double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[], double* dDiffMetric), void* p, int priority)> XPRS_mse_addcbgetsolutiondiff;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC *f_mse_getsolutiondiff)(XPRSmipsolenum mse, void* vContext, int nCols, int iSolutionId_1, int iElemCount_1, double dMipObj_1, const double Vals_1[], const int iSparseIndices_1[], int iSolutionId_2, int iElemCount_2, double dMipObj_2, const double Vals_2[], const int iSparseIndices_2[], double* dDiffMetric), void* p)> XPRS_mse_removecbgetsolutiondiff;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p)> XPRS_mse_setcbmsghandler;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC **f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void** p)> XPRS_mse_getcbmsghandler;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p, int priority)> XPRS_mse_addcbmsghandler;
extern std::function<int(XPRSmipsolenum mse, int (XPRS_CC *f_msghandler)(XPRSobject vXPRSObject, void* cbdata, void* thread, const char* msg, int msgtype, int msgcode), void* p)> XPRS_mse_removecbmsghandler;
extern std::function<int(XPRSbranchobject* p_bo, XPRSprob prob, int isoriginal)> XPRS_bo_create;
extern std::function<int(XPRSbranchobject bo)> XPRS_bo_destroy;
extern std::function<int(XPRSbranchobject bo, int* p_status)> XPRS_bo_store;
extern std::function<int(XPRSbranchobject bo, int nbranches)> XPRS_bo_addbranches;
extern std::function<int(XPRSbranchobject bo, int* p_nbranches)> XPRS_bo_getbranches;
extern std::function<int(XPRSbranchobject bo, int priority)> XPRS_bo_setpriority;
extern std::function<int(XPRSbranchobject bo, int branch)> XPRS_bo_setpreferredbranch;
extern std::function<int(XPRSbranchobject bo, int branch, int nbounds, const char bndtype[], const int colind[], const double bndval[])> XPRS_bo_addbounds;
extern std::function<int(XPRSbranchobject bo, int branch, int* p_nbounds, int maxbounds, char bndtype[], int colind[], double bndval[])> XPRS_bo_getbounds;
extern std::function<int(XPRSbranchobject bo, int branch, int nrows, int ncoefs, const char rowtype[], const double rhs[], const int start[], const int colind[], const double rowcoef[])> XPRS_bo_addrows;
extern std::function<int(XPRSbranchobject bo, int branch, int* p_nrows, int maxrows, int* p_ncoefs, int maxcoefs, char rowtype[], double rhs[], int start[], int colind[], double rowcoef[])> XPRS_bo_getrows;
extern std::function<int(XPRSbranchobject bo, int branch, int ncuts, const XPRScut cutind[])> XPRS_bo_addcuts;
extern std::function<int(XPRSbranchobject bo, int* p_id)> XPRS_bo_getid;
extern std::function<int(XPRSbranchobject bo, int* p_msgcode, char* msg, int maxbytes, int* p_nbytes)> XPRS_bo_getlasterror;
extern std::function<int(XPRSbranchobject bo, int* p_status)> XPRS_bo_validate;
extern std::function<int(XPRSprob prob, const char* flags)> XPRSminim;
extern std::function<int(XPRSprob prob, const char* flags)> XPRSmaxim;
extern std::function<int(XPRSprob prob)> XPRSinitglobal;
extern std::function<int(XPRSprob prob)> XPRSglobal;
extern std::function<int(XPRSprob prob, double* p_cond, double* p_scaledcond)> XPRSbasiscondition;
extern std::function<int(XPRSprob prob, int options, const char* flags, const double solution[], double refined[], int* p_status)> XPRSrefinemipsol;


}  // namespace operations_research

#endif  // OR_TOOLS_XPRESS_ENVIRONMENT_H
