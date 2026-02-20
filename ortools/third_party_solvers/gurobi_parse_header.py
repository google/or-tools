#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

r"""Gurobi header parser script to generate code for the environment.{cc|h}.

To use, run the script
  copy gurobi_c.h somewhere.
  edit the file and add the signature for the GRBisqp:
  int __stdcall
    GRBisqp(GRBenv**, const char*, const char*, const char*, int, const char*);
  blaze run \
    ortools/third_party_solvers/gurobi_parse_header \
    -- <path to gurobi_c.h>

It will output all methods defined in the EXPORTED_FUNCTIONS field, and all
defined symbols.

The list of symbols to export is found by the following command:
    grep -oh -e "GRB[A-Za-z0-9_]*" <path_to>/gurobi_interface.cc  \
        <path_to>/gurobi_proto_solver.* <path_to>/math_opt/solvers/gurobi* \
        <path_to>/math_opt/solvers/gurobi/g_gurobi* | sort -u

This will printout on the console 3 sections:

------------------- header -------------------

to copy paste in environment.h

------------------- define -------------------

to copy in the define part of environment.cc

------------------- assign -------------------

to copy in the assign part of environment.cc
"""

import re
from typing import Sequence
from absl import app

EXPORTED_FUNCTIONS = frozenset(
    [
        "GRBaddconstr",
        "GRBaddconstrs",
        "GRBaddgenconstrAbs",
        "GRBaddgenconstrAnd",
        "GRBaddgenconstrIndicator",
        "GRBaddgenconstrMax",
        "GRBaddgenconstrMin",
        "GRBaddgenconstrOr",
        "GRBaddqconstr",
        "GRBaddqpterms",
        "GRBaddrangeconstr",
        "GRBaddsos",
        "GRBaddvar",
        "GRBaddvars",
        "GRBcbcut",
        "GRBcbget",
        "GRBcblazy",
        "GRBcbsolution",
        "GRBchgcoeffs",
        "GRBcopyparams",
        "GRBdelconstrs",
        "GRBdelgenconstrs",
        "GRBdelq",
        "GRBdelqconstrs",
        "GRBdelsos",
        "GRBdelvars",
        "GRBenv",
        "GRBenvUniquePtr",
        "GRBemptyenv",
        "GRBgetnumparams",
        "GRBgetparamtype",
        "GRBgetparamname",
        "GRBgetintparaminfo",
        "GRBgetdblparaminfo",
        "GRBgetstrparaminfo",
        "GRBfreeenv",
        "GRBfreemodel",
        "GRBgetcharattrarray",
        "GRBgetcharattrelement",
        "GRBgetdblattr",
        "GRBgetdblattrarray",
        "GRBgetdblattrelement",
        "GRBgetdblparam",
        "GRBgetenv",
        "GRBgeterrormsg",
        "GRBgetintattr",
        "GRBgetintattrarray",
        "GRBgetintattrelement",
        "GRBgetintparam",
        "GRBgetstrattr",
        "GRBgetstrparam",
        "GRBgetvars",
        "GRBisattravailable",
        "GRBisqp",
        "GRBloadenv",
        "GRBmodel",
        "GRBnewmodel",
        "GRBoptimize",
        "GRBcomputeIIS",
        "GRBplatform",
        "GRBresetparams",
        "GRBsetcallbackfunc",
        "GRBsetcharattrarray",
        "GRBsetcharattrelement",
        "GRBsetcharattrlist",
        "GRBsetdblattr",
        "GRBsetdblattrarray",
        "GRBsetdblattrelement",
        "GRBsetdblattrlist",
        "GRBsetdblparam",
        "GRBsetintattr",
        "GRBsetintattrarray",
        "GRBsetintattrelement",
        "GRBsetintattrlist",
        "GRBsetintparam",
        "GRBsetobjectiven",
        "GRBsetparam",
        "GRBsetstrattr",
        "GRBsetstrparam",
        "GRBterminate",
        "GRBupdatemodel",
        "GRBversion",
        "GRBwrite",
    ]
)

# TODO(user): Filter #define too.


class GurobiHeaderParser:
    """Converts gurobi_c.h to something pastable in ./environment.h|.cc."""

    def __init__(self):
        self.__header = ""
        self.__define = ""
        self.__assign = ""
        self.__state = 0
        self.__return_type = ""
        self.__args = ""
        self.__fun_name = ""

    def should_be_exported(self, name: str) -> bool:
        return name in EXPORTED_FUNCTIONS

    def write_define(self, symbol: str, value: str) -> None:
        self.__header += f"#define {symbol} {value}\n"

    def write_fun(self, name: str, return_type: str, args: str) -> None:
        if not self.should_be_exported(name):
            print("skipping " + name)
            return

        self.__header += f"extern std::function<{return_type}({args})> {name};\n"
        self.__define += f"std::function<{return_type}({args})> {name} = nullptr;\n"
        self.__assign += f"  gurobi_dynamic_library->GetFunction(&{name}, "
        self.__assign += f'"{name}");\n'

    def parse(self, filepath: str) -> None:
        """Main method to parser the gurobi header."""

        with open(filepath) as fp:
            all_lines = fp.read()

        for line in all_lines.splitlines():
            if not line:  # Ignore empty lines.
                continue
            if re.fullmatch(r"/\*", line, re.M):  # Ignore comments.
                continue

            if self.__state == 0:
                # Note: fullmatch does not work.
                match_def = re.match(r"#define ([A-Z0-9_]*)\s+([^/]+)", line, re.M)
                if match_def:
                    self.write_define(match_def.group(1), match_def.group(2))
                    continue

                # Single line function definition.
                match_fun = re.fullmatch(
                    r"([a-z]+) __stdcall (GRB[A-Za-z_]*)\(([^;]*)\);", line, re.M
                )
                if match_fun:
                    self.write_fun(
                        match_fun.group(1), match_fun.group(2), match_fun.group(3)
                    )
                    continue

                # Simple type declaration (i.e. int __stdcall).
                match_fun = re.fullmatch(r"([a-z]+) __stdcall\s*$", line, re.M)
                if match_fun:
                    self.__return_type = match_fun.group(1)
                    self.__state = 1
                    continue

                # Complex type declaration with pointer (i.e. GRBModel* __stdcall).
                match_fun = re.fullmatch(r"([A-Za-z ]+)\*\s*__stdcall\s*$", line, re.M)
                if match_fun:
                    self.__return_type = match_fun.group(1) + "*"
                    self.__state = 1
                    continue

            elif self.__state == 1:  # The return type was defined at the line before.
                # Function definition terminates in this line.
                match_fun = re.fullmatch(r"\s*(GRB[A-Za-z_]*)\(([^;]+)\);", line, re.M)
                if match_fun:
                    self.write_fun(
                        match_fun.group(1), self.__return_type, match_fun.group(2)
                    )
                    self.__state = 0
                    self.__return_type = ""
                    continue

                # Function definition does not terminate in this line.
                match_fun = re.fullmatch(r"\s*(GRB[A-Za-z_]*)\(([^;]+)$", line, re.M)
                if match_fun:
                    self.__fun_name = match_fun.group(1)
                    self.__args = match_fun.group(2)
                    self.__state = 2
                    continue

            elif self.__state == 2:  # Extra arguments.
                # Arguments end in this line.
                match_fun = re.fullmatch(r"\s*([^;]+)\);", line, re.M)
                if match_fun:
                    self.__args += match_fun.group(1)
                    self.write_fun(self.__fun_name, self.__return_type, self.__args)
                    self.__args = ""
                    self.__fun_name = ""
                    self.__return_type = ""
                    self.__state = 0
                    continue

                # Arguments do not end in this line.
                match_fun = re.fullmatch(r"\s*([^;]+)$", line, re.M)
                if match_fun:
                    self.__args += match_fun.group(1)
                    continue

    def output(self) -> None:
        """Output the 3 generated code on standard out."""

        # replace __stdcall by GUROBI_STDCALL.
        self.__header = self.__header.replace("__stdcall", "GUROBI_STDCALL")
        self.__define = self.__define.replace("__stdcall", "GUROBI_STDCALL")

        print("------------------- header -------------------")
        print(self.__header)

        print("------------------- define -------------------")
        print(self.__define)

        print("------------------- assign -------------------")
        print(self.__assign)


def main(argv: Sequence[str]) -> None:
    if len(argv) > 2:
        raise app.UsageError("Too many command-line arguments.")
    if len(argv) == 1:
        raise app.UsageError("Please supply path to gurobi_c.h on the command line.")

    parser = GurobiHeaderParser()
    parser.parse(argv[1])
    parser.output()


if __name__ == "__main__":
    app.run(main)
