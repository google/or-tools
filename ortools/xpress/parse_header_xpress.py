#!/usr/bin/env python3
"""Xpress header parser script to generate code for the environment.{cc|h}.

To use, run the script
  ./parse_header_xpress.py <path to xprs.h>

This will printout on the console 3 sections:

------------------- header -------------------

to copy paste in environment.h

------------------- define -------------------

to copy in the define part of environment.cc

------------------- assign -------------------

to copy in the assign part of environment.cc
"""

import argparse
import re


# from absl import app


class XpressHeaderParser(object):
    """Converts xprs.h to something pastable in ./environment.h|.cc."""

    def __init__(self):
        self.__header = ''
        self.__define = ''
        self.__assign = ''
        self.__state = 0
        self.__return_type = ''
        self.__args = ''
        self.__fun_name = ''
        self.__required_defines = {"XPRS_PLUSINFINITY", "XPRS_MINUSINFINITY", "XPRS_MAXBANNERLENGTH", "XPVERSION",
                                   "XPRS_MPSRHSNAME", "XPRS_MPSOBJNAME", "XPRS_MPSRANGENAME", "XPRS_MPSBOUNDNAME",
                                   "XPRS_OUTPUTMASK", "XPRS_TUNERMETHODFILE", "XPRS_TUNEROUTPUTPATH",
                                   "XPRS_TUNERSESSIONNAME", "XPRS_COMPUTEEXECSERVICE", "XPRS_MATRIXTOL",
                                   "XPRS_PIVOTTOL", "XPRS_FEASTOL", "XPRS_OUTPUTTOL", "XPRS_SOSREFTOL",
                                   "XPRS_OPTIMALITYTOL", "XPRS_ETATOL", "XPRS_RELPIVOTTOL", "XPRS_MIPTOL",
                                   "XPRS_MIPTOLTARGET", "XPRS_BARPERTURB", "XPRS_MIPADDCUTOFF", "XPRS_MIPABSCUTOFF",
                                   "XPRS_MIPRELCUTOFF", "XPRS_PSEUDOCOST", "XPRS_PENALTY", "XPRS_BIGM",
                                   "XPRS_MIPABSSTOP", "XPRS_MIPRELSTOP", "XPRS_CROSSOVERACCURACYTOL",
                                   "XPRS_PRIMALPERTURB", "XPRS_DUALPERTURB", "XPRS_BAROBJSCALE", "XPRS_BARRHSSCALE",
                                   "XPRS_CHOLESKYTOL", "XPRS_BARGAPSTOP", "XPRS_BARDUALSTOP", "XPRS_BARPRIMALSTOP",
                                   "XPRS_BARSTEPSTOP", "XPRS_ELIMTOL", "XPRS_PERTURB", "XPRS_MARKOWITZTOL",
                                   "XPRS_MIPABSGAPNOTIFY", "XPRS_MIPRELGAPNOTIFY", "XPRS_BARLARGEBOUND",
                                   "XPRS_PPFACTOR", "XPRS_REPAIRINDEFINITEQMAX", "XPRS_BARGAPTARGET",
                                   "XPRS_BARSTARTWEIGHT", "XPRS_BARFREESCALE", "XPRS_SBEFFORT",
                                   "XPRS_HEURDIVERANDOMIZE", "XPRS_HEURSEARCHEFFORT", "XPRS_CUTFACTOR",
                                   "XPRS_EIGENVALUETOL", "XPRS_INDLINBIGM", "XPRS_TREEMEMORYSAVINGTARGET",
                                   "XPRS_GLOBALFILEBIAS", "XPRS_INDPRELINBIGM", "XPRS_RELAXTREEMEMORYLIMIT",
                                   "XPRS_MIPABSGAPNOTIFYOBJ", "XPRS_MIPABSGAPNOTIFYBOUND", "XPRS_PRESOLVEMAXGROW",
                                   "XPRS_HEURSEARCHTARGETSIZE", "XPRS_CROSSOVERRELPIVOTTOL",
                                   "XPRS_CROSSOVERRELPIVOTTOLSAFE", "XPRS_DETLOGFREQ", "XPRS_MAXIMPLIEDBOUND",
                                   "XPRS_FEASTOLTARGET", "XPRS_OPTIMALITYTOLTARGET", "XPRS_PRECOMPONENTSEFFORT",
                                   "XPRS_LPLOGDELAY", "XPRS_HEURDIVEITERLIMIT", "XPRS_BARKERNEL", "XPRS_FEASTOLPERTURB",
                                   "XPRS_CROSSOVERFEASWEIGHT", "XPRS_LUPIVOTTOL", "XPRS_MIPRESTARTGAPTHRESHOLD",
                                   "XPRS_NODEPROBINGEFFORT", "XPRS_INPUTTOL", "XPRS_MIPRESTARTFACTOR",
                                   "XPRS_BAROBJPERTURB", "XPRS_EXTRAROWS", "XPRS_EXTRACOLS", "XPRS_LPITERLIMIT",
                                   "XPRS_LPLOG", "XPRS_SCALING", "XPRS_PRESOLVE", "XPRS_CRASH", "XPRS_PRICINGALG",
                                   "XPRS_INVERTFREQ", "XPRS_INVERTMIN", "XPRS_MAXNODE", "XPRS_MAXTIME",
                                   "XPRS_MAXMIPSOL", "XPRS_SIFTPASSES", "XPRS_DEFAULTALG", "XPRS_VARSELECTION",
                                   "XPRS_NODESELECTION", "XPRS_BACKTRACK", "XPRS_MIPLOG", "XPRS_KEEPNROWS",
                                   "XPRS_MPSECHO", "XPRS_MAXPAGELINES", "XPRS_OUTPUTLOG", "XPRS_BARSOLUTION",
                                   "XPRS_CACHESIZE", "XPRS_CROSSOVER", "XPRS_BARITERLIMIT", "XPRS_CHOLESKYALG",
                                   "XPRS_BAROUTPUT", "XPRS_CSTYLE", "XPRS_EXTRAMIPENTS", "XPRS_REFACTOR",
                                   "XPRS_BARTHREADS", "XPRS_KEEPBASIS", "XPRS_CROSSOVEROPS", "XPRS_VERSION",
                                   "XPRS_CROSSOVERTHREADS", "XPRS_BIGMMETHOD", "XPRS_MPSNAMELENGTH", "XPRS_ELIMFILLIN",
                                   "XPRS_PRESOLVEOPS", "XPRS_MIPPRESOLVE", "XPRS_MIPTHREADS", "XPRS_BARORDER",
                                   "XPRS_BREADTHFIRST", "XPRS_AUTOPERTURB", "XPRS_DENSECOLLIMIT",
                                   "XPRS_CALLBACKFROMMASTERTHREAD", "XPRS_MAXMCOEFFBUFFERELEMS", "XPRS_REFINEOPS",
                                   "XPRS_LPREFINEITERLIMIT", "XPRS_MIPREFINEITERLIMIT", "XPRS_DUALIZEOPS",
                                   "XPRS_CROSSOVERITERLIMIT", "XPRS_PREBASISRED", "XPRS_PRESORT", "XPRS_PREPERMUTE",
                                   "XPRS_PREPERMUTESEED", "XPRS_MAXMEMORYSOFT", "XPRS_CUTFREQ", "XPRS_SYMSELECT",
                                   "XPRS_SYMMETRY", "XPRS_MAXMEMORYHARD", "XPRS_LPTHREADS", "XPRS_MIQCPALG",
                                   "XPRS_QCCUTS", "XPRS_QCROOTALG", "XPRS_PRECONVERTSEPARABLE", "XPRS_ALGAFTERNETWORK",
                                   "XPRS_TRACE", "XPRS_MAXIIS", "XPRS_CPUTIME", "XPRS_COVERCUTS", "XPRS_GOMCUTS",
                                   "XPRS_LPFOLDING", "XPRS_MPSFORMAT", "XPRS_CUTSTRATEGY", "XPRS_CUTDEPTH",
                                   "XPRS_TREECOVERCUTS", "XPRS_TREEGOMCUTS", "XPRS_CUTSELECT", "XPRS_TREECUTSELECT",
                                   "XPRS_DUALIZE", "XPRS_DUALGRADIENT", "XPRS_SBITERLIMIT", "XPRS_SBBEST",
                                   "XPRS_MAXCUTTIME", "XPRS_ACTIVESET", "XPRS_BARINDEFLIMIT", "XPRS_HEURSTRATEGY",
                                   "XPRS_HEURFREQ", "XPRS_HEURDEPTH", "XPRS_HEURMAXSOL", "XPRS_HEURNODES",
                                   "XPRS_LNPBEST", "XPRS_LNPITERLIMIT", "XPRS_BRANCHCHOICE", "XPRS_BARREGULARIZE",
                                   "XPRS_SBSELECT", "XPRS_LOCALCHOICE", "XPRS_LOCALBACKTRACK", "XPRS_DUALSTRATEGY",
                                   "XPRS_HEURDIVESTRATEGY", "XPRS_HEURSELECT", "XPRS_BARSTART",
                                   "XPRS_PRESOLVEPASSES", "XPRS_BARNUMSTABILITY", "XPRS_BARORDERTHREADS",
                                   "XPRS_EXTRASETS", "XPRS_FEASIBILITYPUMP", "XPRS_PRECOEFELIM", "XPRS_PREDOMCOL",
                                   "XPRS_HEURSEARCHFREQ", "XPRS_HEURDIVESPEEDUP", "XPRS_SBESTIMATE", "XPRS_BARCORES",
                                   "XPRS_MAXCHECKSONMAXTIME", "XPRS_MAXCHECKSONMAXCUTTIME", "XPRS_HISTORYCOSTS",
                                   "XPRS_ALGAFTERCROSSOVER", "XPRS_LINELENGTH", "XPRS_MUTEXCALLBACKS", "XPRS_BARCRASH",
                                   "XPRS_HEURDIVESOFTROUNDING", "XPRS_HEURSEARCHROOTSELECT",
                                   "XPRS_HEURSEARCHTREESELECT", "XPRS_ROOTPRESOLVE", "XPRS_MPS18COMPATIBLE",
                                   "XPRS_CROSSOVERDRP", "XPRS_FORCEOUTPUT", "XPRS_DETERMINISTIC", "XPRS_PREPROBING",
                                   "XPRS_EXTRAQCELEMENTS", "XPRS_EXTRAQCROWS", "XPRS_TREEMEMORYLIMIT",
                                   "XPRS_TREECOMPRESSION", "XPRS_TREEDIAGNOSTICS", "XPRS_MAXGLOBALFILESIZE",
                                   "XPRS_PRECLIQUESTRATEGY", "XPRS_REPAIRINFEASMAXTIME", "XPRS_IFCHECKCONVEXITY",
                                   "XPRS_PRIMALUNSHIFT", "XPRS_REPAIRINDEFINITEQ", "XPRS_MIPRAMPUP",
                                   "XPRS_MAXLOCALBACKTRACK", "XPRS_USERSOLHEURISTIC", "XPRS_FORCEPARALLELDUAL",
                                   "XPRS_BACKTRACKTIE", "XPRS_BRANCHDISJ", "XPRS_MIPFRACREDUCE",
                                   "XPRS_CONCURRENTTHREADS", "XPRS_MAXSCALEFACTOR", "XPRS_HEURTHREADS", "XPRS_THREADS",
                                   "XPRS_HEURBEFORELP", "XPRS_PREDOMROW", "XPRS_BRANCHSTRUCTURAL",
                                   "XPRS_QUADRATICUNSHIFT", "XPRS_BARPRESOLVEOPS", "XPRS_QSIMPLEXOPS",
                                   "XPRS_MIPRESTART", "XPRS_CONFLICTCUTS", "XPRS_PREPROTECTDUAL", "XPRS_CORESPERCPU",
                                   "XPRS_RESOURCESTRATEGY", "XPRS_CLAMPING", "XPRS_SLEEPONTHREADWAIT", "XPRS_PREDUPROW",
                                   "XPRS_CPUPLATFORM", "XPRS_BARALG", "XPRS_SIFTING", "XPRS_LPLOGSTYLE",
                                   "XPRS_RANDOMSEED", "XPRS_TREEQCCUTS", "XPRS_PRELINDEP", "XPRS_DUALTHREADS",
                                   "XPRS_PREOBJCUTDETECT", "XPRS_PREBNDREDQUAD", "XPRS_PREBNDREDCONE",
                                   "XPRS_PRECOMPONENTS", "XPRS_MAXMIPTASKS", "XPRS_MIPTERMINATIONMETHOD",
                                   "XPRS_PRECONEDECOMP", "XPRS_HEURFORCESPECIALOBJ", "XPRS_HEURSEARCHROOTCUTFREQ",
                                   "XPRS_PREELIMQUAD", "XPRS_PREIMPLICATIONS", "XPRS_TUNERMODE", "XPRS_TUNERMETHOD",
                                   "XPRS_TUNERTARGET", "XPRS_TUNERTHREADS", "XPRS_TUNERMAXTIME", "XPRS_TUNERHISTORY",
                                   "XPRS_TUNERPERMUTE", "XPRS_TUNERROOTALG", "XPRS_TUNERVERBOSE", "XPRS_TUNEROUTPUT",
                                   "XPRS_PREANALYTICCENTER", "XPRS_NETCUTS", "XPRS_LPFLAGS", "XPRS_MIPKAPPAFREQ",
                                   "XPRS_OBJSCALEFACTOR", "XPRS_GLOBALFILELOGINTERVAL", "XPRS_IGNORECONTAINERCPULIMIT",
                                   "XPRS_IGNORECONTAINERMEMORYLIMIT", "XPRS_MIPDUALREDUCTIONS",
                                   "XPRS_GENCONSDUALREDUCTIONS", "XPRS_PWLDUALREDUCTIONS", "XPRS_BARFAILITERLIMIT",
                                   "XPRS_AUTOSCALING", "XPRS_GENCONSABSTRANSFORMATION", "XPRS_COMPUTEJOBPRIORITY",
                                   "XPRS_PREFOLDING", "XPRS_COMPUTE", "XPRS_NETSTALLLIMIT", "XPRS_SERIALIZEPREINTSOL",
                                   "XPRS_PWLNONCONVEXTRANSFORMATION", "XPRS_MIPCOMPONENTS", "XPRS_MIPCONCURRENTNODES",
                                   "XPRS_MIPCONCURRENTSOLVES", "XPRS_OUTPUTCONTROLS", "XPRS_SIFTSWITCH",
                                   "XPRS_HEUREMPHASIS", "XPRS_COMPUTEMATX", "XPRS_COMPUTEMATX_IIS",
                                   "XPRS_COMPUTEMATX_IISMAXTIME", "XPRS_BARREFITER", "XPRS_COMPUTELOG",
                                   "XPRS_SIFTPRESOLVEOPS", "XPRS_ESCAPENAMES", "XPRS_IOTIMEOUT", "XPRS_MAXSTALLTIME",
                                   "XPRS_AUTOCUTTING", "XPRS_EXTRAELEMS", "XPRS_EXTRAPRESOLVE", "XPRS_EXTRASETELEMS",
                                   "XPRS_LPOBJVAL", "XPRS_MIPOBJVAL", "XPRS_BESTBOUND", "XPRS_OBJRHS", "XPRS_OBJSENSE",
                                   "XPRS_ROWS", "XPRS_SIMPLEXITER", "XPRS_LPSTATUS", "XPRS_MIPSTATUS", "XPRS_NODES",
                                   "XPRS_COLS", "XPRS_LP_OPTIMAL", "XPRS_LP_INFEAS", "XPRS_LP_UNBOUNDED",
                                   "XPRS_MIP_SOLUTION", "XPRS_MIP_INFEAS", "XPRS_MIP_OPTIMAL", "XPRS_MIP_UNBOUNDED",
                                   "XPRS_OBJ_MINIMIZE", "XPRS_OBJ_MAXIMIZE", "XPRS_L1CACHE"}
        self.__missing_required_defines = self.__required_defines
        self.__required_functions = {"XPRScreateprob", "XPRSdestroyprob", "XPRSinit", "XPRSfree", "XPRSgetlicerrmsg",
                                     "XPRSlicense", "XPRSgetbanner", "XPRSgetversion", "XPRSsetdefaultcontrol",
                                     "XPRSsetintcontrol", "XPRSsetintcontrol64", "XPRSsetdblcontrol",
                                     "XPRSsetstrcontrol", "XPRSgetintcontrol", "XPRSgetintcontrol64",
                                     "XPRSgetdblcontrol", "XPRSgetstringcontrol", "XPRSgetintattrib",
                                     "XPRSgetdblattrib", "XPRSloadlp", "XPRSloadlp64", "XPRSgetobj", "XPRSgetrhs",
                                     "XPRSgetrhsrange", "XPRSgetlb", "XPRSgetub", "XPRSgetcoef", "XPRSaddrows",
                                     "XPRSdelrows", "XPRSaddcols", "XPRSdelcols", "XPRSchgcoltype", "XPRSloadbasis",
                                     "XPRSpostsolve", "XPRSchgobjsense", "XPRSgetlasterror", "XPRSgetbasis",
                                     "XPRSwriteprob", "XPRSgetrowtype", "XPRSgetcoltype", "XPRSgetlpsol",
                                     "XPRSgetmipsol", "XPRSchgbounds", "XPRSchgobj", "XPRSchgcoef", "XPRSchgmcoef",
                                     "XPRSchgrhs", "XPRSchgrhsrange", "XPRSchgrowtype", "XPRSsetcbmessage", "XPRSminim",
                                     "XPRSmaxim"}
        self.__missing_required_functions = self.__required_functions

    def write_define(self, symbol, value):
        if symbol in self.__required_defines:
            self.__header += f'#define {symbol} {value}\n'
            self.__missing_required_defines.remove(symbol)
        else:
            print('skipping ' + symbol)

    def write_fun(self, return_type, name, args):
        if name in self.__required_functions:
            self.__header += f'extern std::function<{return_type}({args})> {name};\n'
            self.__define += f'std::function<{return_type}({args})> {name} = nullptr;\n'
            self.__assign += f'  xpress_dynamic_library->GetFunction(&{name}, '
            self.__assign += f'"{name}");\n'
            self.__missing_required_functions.remove(name)
        else:
            print('skipping ' + name)

    def parse(self, filepath):
        """Main method to parser the Xpress header."""

        with open(filepath) as fp:
            all_lines = fp.read()

        for line in all_lines.splitlines():
            if not line:  # Ignore empty lines.
                continue
            if re.match(r'/\*', line, re.M):  # Ignore comments.
                continue

            if self.__state == 0:
                match_def = re.match(r'#define ([A-Z0-9_]*)\s+([^/]+)', line,
                                     re.M)
                if match_def:
                    self.write_define(match_def.group(1), match_def.group(2))
                    continue

                # Single line function definition.
                match_fun = re.match(
                    r'([a-z]+) XPRS_CC (XPRS[A-Za-z0-9_]*)\(([^;]*)\);', line,
                    re.M)
                if match_fun:
                    self.write_fun(match_fun.group(1), match_fun.group(2),
                                   match_fun.group(3))
                    continue

                # Simple type declaration (i.e. int XPRS_CC).
                match_fun = re.match(r'([a-z]+) XPRS_CC\s*$', line, re.M)
                if match_fun:
                    self.__return_type = match_fun.group(1)
                    self.__state = 1
                    continue

                # Complex type declaration with pointer.
                match_fun = re.match(r'([A-Za-z0-9 ]+)\*\s*XPRS_CC\s*$', line,
                                     re.M)
                if match_fun:
                    self.__return_type = match_fun.group(1) + '*'
                    self.__state = 1
                    continue

            elif self.__state == 1:  # The return type was defined at the line before.
                # Function definition terminates in this line.
                match_fun = re.match(r'\s*(XPRS[A-Za-z0-9_]*)\(([^;]+)\);', line,
                                     re.M)
                if match_fun:
                    self.write_fun(match_fun.group(1), self.__return_type,
                                   match_fun.group(2))
                    self.__state = 0
                    self.__return_type = ''
                    continue

                # Function definition does not terminate in this line.
                match_fun = re.match(r'\s*(XPRS[A-Za-z0-9_]*)\(([^;]+)$', line,
                                     re.M)
                if match_fun:
                    self.__fun_name = match_fun.group(1)
                    self.__args = match_fun.group(2)
                    self.__state = 2
                    continue

            elif self.__state == 2:  # Extra arguments.
                # Arguments end in this line.
                match_fun = re.match(r'\s*([^;]+)\);', line, re.M)
                if match_fun:
                    self.__args += match_fun.group(1)
                    self.write_fun(self.__fun_name, self.__return_type,
                                   self.__args)
                    self.__args = ''
                    self.__fun_name = ''
                    self.__return_type = ''
                    self.__state = 0
                    continue

                # Arguments do not end in this line.
                match_fun = re.match(r'\s*([^;]+)$', line, re.M)
                if match_fun:
                    self.__args += match_fun.group(1)
                    continue

    def output(self):
        """Output the 3 generated code on standard out."""
        print('------------------- header (to copy in environment.h) -------------------')
        print(self.__header)

        print('------------------- define (to copy in the define part of environment.cc) -------------------')
        print(self.__define)

        print('------------------- assign (to copy in the assign part of environment.cc) -------------------')
        print(self.__assign)

    def print_missing_elements(self):
        if self.__missing_required_defines:
            print('------WARNING------ missing required defines -------------------')
            print(self.__missing_required_defines)

        if self.__missing_required_functions:
            print('------WARNING------ missing required functions -------------------')
            print(self.__missing_required_functions)

        if self.__missing_required_defines or self.__missing_required_functions:
            raise LookupError("Some required defines or functions are missing (see detail above)")


def main(path: str) -> None:
    parser = XpressHeaderParser()
    parser.parse(path)
    parser.output()
    parser.print_missing_elements()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Xpress header parser.')
    parser.add_argument('filepath', type=str)
    args = parser.parse_args()
    main(args.filepath)
