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

import re
import argparse
from typing import Sequence
#from absl import app


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
        self.__skipped_functions = set([])

    def should_be_skipped(self, name):
        return name in self.__skipped_functions

    def write_define(self, symbol, value):
        self.__header += f'#define {symbol} {value}\n'

    def write_fun(self, return_type, name, args):
        if self.should_be_skipped(name):
            print('skipping ' + name)
            return

        self.__header += f'extern std::function<{return_type}({args})> {name};\n'
        self.__define += f'std::function<{return_type}({args})> {name} = nullptr;\n'
        self.__assign += f'  xpress_dynamic_library->GetFunction(&{name}, '
        self.__assign += f'"{name}");\n'

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

        print('------------------- header -------------------')
        print(self.__header)

        print('------------------- define -------------------')
        print(self.__define)

        print('------------------- assign -------------------')
        print(self.__assign)


def main(path: str) -> None:
    parser = XpressHeaderParser()
    parser.parse(path)
    parser.output()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Xpress header parser.')
    parser.add_argument('filepath', type=str)
    args = parser.parse_args()
    main(args.filepath)
