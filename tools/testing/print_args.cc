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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

// [START program]
// This program prints a "Hello world!" message and its command-line arguments,
// one per line.
//
// It has a few options:
//  --fail: If present, the program will exit with a non-zero status code.
//  --stderr: This argument and the following ones are printed to stderr instead
//            of stdout.
//  --file=<file>: Prints the content of the file.
//
// Example usage:
//  print_args --fail --stderr arg1 arg2
//
// This will print:
//  Hello world!
//  --fail
// to stdout, and
//  --stderr
//  arg1
//  arg2
// to stderr, and exit with a non-zero status code.
int main(int argc, char** argv) {
  const std::string file_flag("--file=");
  puts("Hello world!");
  int return_code = EXIT_SUCCESS;
  FILE* out_file = stdout;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    std::string output = arg;
    if (arg == "--fail") {
      return_code = EXIT_FAILURE;
    } else if (arg == "--stderr") {
      out_file = stderr;
    } else if (arg.starts_with(file_flag)) {
      std::ifstream stream(arg.substr(file_flag.size()));
      std::stringstream buffer;
      buffer << stream.rdbuf();
      output = buffer.str();
    }
    fprintf(out_file, "%s\n", output.c_str());
  }
  return return_code;
}
// [END program]
