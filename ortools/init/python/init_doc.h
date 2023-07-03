// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_OPEN_SOURCE_INIT_PYTHON_INIT_DOC_H_
#define OR_TOOLS_OPEN_SOURCE_INIT_PYTHON_INIT_DOC_H_

/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define __EXPAND(x) x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...) COUNT
#define __VA_SIZE(...) __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))
#define __CAT1(a, b) a##b
#define __CAT2(a, b) __CAT1(a, b)
#define __DOC1(n1) __doc_##n1
#define __DOC2(n1, n2) __doc_##n1##_##n2
#define __DOC3(n1, n2, n3) __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4) __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5) __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...) \
  __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static const char* __doc_ABSL_DECLARE_FLAG = R"doc()doc";

static const char* __doc_ABSL_DECLARE_FLAG_2 = R"doc()doc";

static const char* __doc_ABSL_DECLARE_FLAG_3 = R"doc()doc";

static const char* __doc_ABSL_DECLARE_FLAG_4 = R"doc()doc";

static const char* __doc_ABSL_DECLARE_FLAG_5 = R"doc()doc";

static const char* __doc_operations_research_CppBridge =
    R"doc(This class performs various C++ initialization.

It is meant to be used once at the start of a program.)doc";

static const char* __doc_operations_research_CppBridge_DeleteByteArray =
    R"doc(Delete a temporary C++ byte array.)doc";

static const char* __doc_operations_research_CppBridge_InitLogging =
    R"doc(Initialize the C++ logging layer.

This must be called once before any other library from OR-Tools are
used.)doc";

static const char* __doc_operations_research_CppBridge_LoadGurobiSharedLibrary =
    R"doc(Load the gurobi shared library.

This is necessary if the library is installed in a non canonical
directory, or if for any reason, it is not found. You need to pass the
full path, including the shared library file. It returns true if the
library was found and correctly loaded.)doc";

static const char* __doc_operations_research_CppBridge_SetFlags =
    R"doc(Sets all the C++ flags contained in the CppFlags structure.)doc";

static const char* __doc_operations_research_CppBridge_ShutdownLogging =
    R"doc(Shutdown the C++ logging layer.

This can be called to shutdown the C++ logging layer from OR-Tools. It
should only be called once.

Deprecated: this is a no-op.)doc";

static const char* __doc_operations_research_CppFlags =
    R"doc(Simple structure that holds useful C++ flags to setup from non-C++
languages.)doc";

static const char* __doc_operations_research_CppFlags_cp_model_dump_lns =
    R"doc(DEBUG ONLY: Dump CP-SAT LNS models during solve.

When set to true, solve will dump all lns models proto in text format
to 'FLAGS_cp_model_dump_prefix'lns_xxx.pbtxt.)doc";

static const char* __doc_operations_research_CppFlags_cp_model_dump_models =
    R"doc(DEBUG ONLY: Dump CP-SAT models during solve.

When set to true, SolveCpModel() will dump its model protos (original
model, presolved model, mapping model) in text format to 'FLAGS_cp_mod
el_dump_prefix'{model|presolved_model|mapping_model}.pbtxt.)doc";

static const char* __doc_operations_research_CppFlags_cp_model_dump_prefix =
    R"doc(Prefix filename for all dumped files (models, solutions, lns sub-
models).)doc";

static const char* __doc_operations_research_CppFlags_cp_model_dump_response =
    R"doc(DEBUG ONLY: Dump the CP-SAT final response found during solve.

If true, the final response of each solve will be dumped to
'FLAGS_cp_model_dump_prefix'response.pbtxt.)doc";

static const char* __doc_operations_research_CppFlags_log_prefix =
    R"doc(Controls if time and source code info are used to prefix logging
messages.)doc";

static const char* __doc_operations_research_CppFlags_stderrthreshold =
    R"doc(Controls the logging level shown on stderr.

By default, the logger will only display ERROR and FATAL logs (value 2
and 3) to stderr. To display INFO and WARNING logs (value 0 and 1),
change the threshold to the min value of the message that should be
printed.)doc";

static const char* __doc_operations_research_OrToolsVersion = R"doc()doc";

static const char* __doc_operations_research_OrToolsVersion_MajorNumber =
    R"doc(Returns the major version of OR-Tools.)doc";

static const char* __doc_operations_research_OrToolsVersion_MinorNumber =
    R"doc(Returns the minor version of OR-Tools.)doc";

static const char* __doc_operations_research_OrToolsVersion_PatchNumber =
    R"doc(Returns the patch version of OR-Tools.)doc";

static const char* __doc_operations_research_OrToolsVersion_VersionString =
    R"doc(Returns the string version of OR-Tools.)doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

#endif  // OR_TOOLS_OPEN_SOURCE_INIT_PYTHON_INIT_DOC_H_
