// Copyright 2010-2014 Google
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

#include "ortools/flatzinc/parser.h"

#include <cstdio>

#include "ortools/flatzinc/parser.tab.hh"

// Declare external functions in the flatzinc.tab.cc generated file.
extern int orfz_parse(operations_research::fz::ParserContext* parser,
                      operations_research::fz::Model* model, bool* ok,
                      void* scanner);
extern int orfz_lex_init(void** scanner);
extern int orfz_lex_destroy(void* scanner);
extern void orfz_set_in(FILE* in_str, void* yyscanner);
// Declare external functions and structures in the flatzinc.yy.cc
// generated file.
struct yy_buffer_state;
extern yy_buffer_state* orfz__scan_bytes(const char* input, size_t size,
                                         void* scanner);
extern void orfz__delete_buffer(yy_buffer_state* b, void* scanner);

namespace operations_research {
namespace fz {
// ----- public parsing API -----

bool ParseFlatzincFile(const std::string& filename, Model* model) {
  // Init.
  FILE* const input = fopen(filename.c_str(), "r");
  if (input == nullptr) {
    LOG(INFO) << "Could not open file '" << filename << "'";
    return false;
  }
  ParserContext context;
  bool ok = true;
  void* scanner = nullptr;
  orfz_lex_init(&scanner);
  orfz_set_in(input, scanner);
  // Parse.
  orfz_parse(&context, model, &ok, scanner);
  // Clean up.
  if (scanner != nullptr) {
    orfz_lex_destroy(scanner);
  }
  fclose(input);
  return ok;
}

bool ParseFlatzincString(const std::string& input, Model* model) {
  // Init.
  ParserContext context;
  bool ok = true;
  void* scanner = nullptr;
  orfz_lex_init(&scanner);
  yy_buffer_state* const string_buffer =
      orfz__scan_bytes(input.data(), input.size(), scanner);
  // Parse.
  orfz_parse(&context, model, &ok, scanner);
  // Clean up.
  if (string_buffer != nullptr) {
    orfz__delete_buffer(string_buffer, scanner);
  }
  if (scanner != nullptr) {
    orfz_lex_destroy(scanner);
  }
  return ok;
}
}  // namespace fz
}  // namespace operations_research
