#!/usr/bin/env python3
"""Xpress header parser script to generate code for the environment.{cc|h}.

To use, run the script
  ./parse_header_xpress.py <path to xprs.h>

This will printout on the console 9 sections:

------------------- header -------------------

to copy paste in environment.h

------------------- define -------------------

to copy in the define part of environment.cc

------------------- assign -------------------

to copy in the assign part of environment.cc

------------------- string parameters -------------------

to copy in the "getMapStringControls" function of linear_solver/xpress_interface.cc

------------------- string parameters tests -------------------

to copy in the "setStringControls" TEST of linear_solver/unittests/xpress_interface.cc

------------------- double parameters -------------------

to copy in the "getMapDoubleControls" function of linear_solver/xpress_interface.cc

------------------- double parameters tests -------------------

to copy in the "setDoubleControls" TEST of linear_solver/unittests/xpress_interface.cc

------------------- int parameters -------------------

to copy in the "getMapIntControls" function of linear_solver/xpress_interface.cc

------------------- int parameters tests -------------------

to copy in the "setIntControl" TEST of linear_solver/unittests/xpress_interface.cc

------------------- int64 parameters -------------------

to copy in the "getMapInt64Controls" function of linear_solver/xpress_interface.cc

------------------- int64 parameters tests -------------------

to copy in the "setInt64Control" TEST of linear_solver/unittests/xpress_interface.cc
"""

import argparse
import re
from enum import Enum


# from absl import app

# This enum is used to detect different sections in the xprs.h document
class XprsDocumentSection(Enum):
    STRING_PARAMS = 1
    DOUBLE_PARAMS = 2
    INT_PARAMS = 3
    INT64_PARAMS = 4
    OTHER = 5


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
        self.__string_parameters = ''
        self.__string_parameters_unittest = ''
        self.__double_parameters = ''
        self.__double_parameters_unittest = ''
        self.__int_parameters = ''
        self.__int_parameters_unittest = ''
        self.__int64_parameters = ''
        self.__int64_parameters_unittest = ''
        # These are the definitions required for compiling the XPRESS interface, excluding control parameters
        self.__required_defines = {"XPRS_PLUSINFINITY", "XPRS_MINUSINFINITY", "XPRS_MAXBANNERLENGTH", "XPVERSION",
                                   "XPRS_LPOBJVAL", "XPRS_MIPOBJVAL", "XPRS_BESTBOUND", "XPRS_OBJRHS", "XPRS_OBJSENSE",
                                   "XPRS_ROWS", "XPRS_SIMPLEXITER", "XPRS_LPSTATUS", "XPRS_MIPSTATUS", "XPRS_NODES",
                                   "XPRS_COLS", "XPRS_LP_OPTIMAL", "XPRS_LP_INFEAS", "XPRS_LP_UNBOUNDED",
                                   "XPRS_MIP_SOLUTION", "XPRS_MIP_INFEAS", "XPRS_MIP_OPTIMAL", "XPRS_MIP_UNBOUNDED",
                                   "XPRS_OBJ_MINIMIZE", "XPRS_OBJ_MAXIMIZE"}
        self.__missing_required_defines = self.__required_defines
        # These enum will detect control parameters that will all be imported
        self.__doc_section = XprsDocumentSection.OTHER
        # These parameters are not supported
        self.__excluded_defines = {"XPRS_COMPUTE"}
        # These are the functions required for compiling the XPRESS interface
        self.__required_functions = {"XPRScreateprob", "XPRSdestroyprob", "XPRSinit", "XPRSfree", "XPRSgetlicerrmsg",
                                     "XPRSlicense", "XPRSgetbanner", "XPRSgetversion", "XPRSsetdefaultcontrol",
                                     "XPRSsetintcontrol", "XPRSsetintcontrol64", "XPRSsetdblcontrol",
                                     "XPRSsetstrcontrol", "XPRSgetintcontrol", "XPRSgetintcontrol64",
                                     "XPRSgetdblcontrol", "XPRSgetstringcontrol", "XPRSgetintattrib",
                                     "XPRSgetdblattrib", "XPRSloadlp", "XPRSloadlp64", "XPRSgetobj", "XPRSgetrhs",
                                     "XPRSgetrhsrange", "XPRSgetlb", "XPRSgetub", "XPRSgetcoef", "XPRSaddrows",
                                     "XPRSdelrows", "XPRSaddcols", "XPRSaddnames", "XPRSgetnames", "XPRSdelcols", "XPRSchgcoltype", "XPRSloadbasis",
                                     "XPRSpostsolve", "XPRSchgobjsense", "XPRSgetlasterror", "XPRSgetbasis",
                                     "XPRSwriteprob", "XPRSgetrowtype", "XPRSgetcoltype", "XPRSgetlpsol",
                                     "XPRSgetmipsol", "XPRSchgbounds", "XPRSchgobj", "XPRSchgcoef", "XPRSchgmcoef",
                                     "XPRSchgrhs", "XPRSchgrhsrange", "XPRSchgrowtype", "XPRSaddcbmessage",
                                     "XPRSminim", "XPRSmaxim", "XPRSaddmipsol", "XPRSaddcbintsol", "XPRSremovecbintsol",
                                     "XPRSinterrupt"}
        self.__missing_required_functions = self.__required_functions
        self.__XPRSprob_section = False

    def write_define(self, symbol, value):
        if symbol in self.__excluded_defines:
            print('skipping ' + symbol)
            return

        # If it is a control parameter, import it to expose it to the user
        # Else import it only if required
        if self.__doc_section in [XprsDocumentSection.STRING_PARAMS, XprsDocumentSection.DOUBLE_PARAMS,
                                  XprsDocumentSection.INT_PARAMS, XprsDocumentSection.INT64_PARAMS]:
            self.__header += f'#define {symbol} {value}\n'
            ortools_symbol = symbol.replace("XPRS_", "")
            if self.__doc_section == XprsDocumentSection.STRING_PARAMS:
                self.__string_parameters += f'{{\"{ortools_symbol}\", {symbol}}},\n'
                self.__string_parameters_unittest += f'{{\"{ortools_symbol}\", {symbol}, "default_value"}},\n'
            elif self.__doc_section == XprsDocumentSection.DOUBLE_PARAMS:
                self.__double_parameters += f'{{\"{ortools_symbol}\", {symbol}}},\n'
                self.__double_parameters_unittest += f'{{\"{ortools_symbol}\", {symbol}, 1.}},\n'
            elif self.__doc_section == XprsDocumentSection.INT_PARAMS:
                self.__int_parameters += f'{{\"{ortools_symbol}\", {symbol}}},\n'
                self.__int_parameters_unittest += f'{{\"{ortools_symbol}\", {symbol}, 1}},\n'
            elif self.__doc_section == XprsDocumentSection.INT64_PARAMS:
                self.__int64_parameters += f'{{\"{ortools_symbol}\", {symbol}}},\n'
                self.__int64_parameters_unittest += f'{{\"{ortools_symbol}\", {symbol}, 1}},\n'
        elif symbol in self.__required_defines:
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

        self.__XPRSprob_section = False

        for line in all_lines.splitlines():
            if not line:  # Ignore empty lines.
                continue

            self.detect_XPRSprob_section(line)

            if re.match(r'/\*', line, re.M):  # Comments in xprs.h indicate the section
                if self.__XPRSprob_section:
                    if "string control parameters" in line.lower():
                        self.__doc_section = XprsDocumentSection.STRING_PARAMS
                    elif "double control parameters" in line.lower():
                        self.__doc_section = XprsDocumentSection.DOUBLE_PARAMS
                    elif "integer control parameters" in line.lower() and "64-bit" in line.lower():
                        self.__doc_section = XprsDocumentSection.INT64_PARAMS
                    elif "integer control parameters" in line.lower():
                        self.__doc_section = XprsDocumentSection.INT_PARAMS
                    else:
                        self.__doc_section = XprsDocumentSection.OTHER
                else:
                    self.__doc_section = XprsDocumentSection.OTHER

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

    def detect_XPRSprob_section(self, line):
        """This method detects the section between these commented lines:
        /***************************************************************************\
         * control parameters for XPRSprob                                         *
        ...
        /***************************************************************************\
        """
        if " * control parameters for XPRSprob" in line:
            self.__XPRSprob_section = True
        elif self.__XPRSprob_section and \
                "/***************************************************************************\\" in line:
            self.__XPRSprob_section = False

    def output(self):
        """Output the 3 generated code on standard out."""
        print('------------------- header (to copy in environment.h) -------------------')
        print(self.__header)

        print('------------------- define (to copy in the define part of environment.cc) -------------------')
        print(self.__define)

        print('------------------- assign (to copy in the assign part of environment.cc) -------------------')
        print(self.__assign)

        print('------------------- string params (to copy in the "getMapStringControls" function of linear_solver/xpress_interface.cc) -------------------')
        print(self.__string_parameters)

        print('------------------- string params test (to copy in the "setStringControls" TEST of linear_solver/unittests/xpress_interface.cc) -------------------')
        print(self.__string_parameters_unittest)

        print('------------------- double params (to copy in the "getMapDoubleControls" function of linear_solver/xpress_interface.cc) -------------------')
        print(self.__double_parameters)

        print('------------------- double params test (to copy in the "setDoubleControls" TEST of linear_solver/unittests/xpress_interface.cc) -------------------')
        print(self.__double_parameters_unittest)

        print('------------------- int params (to copy in the "getMapIntControls" function of linear_solver/xpress_interface.cc) -------------------')
        print(self.__int_parameters)

        print('------------------- int params test (to copy in the "setIntControls" TEST of linear_solver/unittests/xpress_interface.cc) -------------------')
        print(self.__int_parameters_unittest)

        print('------------------- int64 params (to copy in the "getMapInt64Controls" function of linear_solver/xpress_interface.cc) -------------------')
        print(self.__int64_parameters)

        print('------------------- int64 params test (to copy in the "setInt64Controls" TEST of linear_solver/unittests/xpress_interface.cc) -------------------')
        print(self.__int64_parameters_unittest)

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
