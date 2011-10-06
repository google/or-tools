// Copyright 2010-2011 Google
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

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/file.h"
#include "base/recordio.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/model.pb.h"

DEFINE_string(input, "", "Input file of the problem.");
DEFINE_string(output, "", "Output file when doing modifications.");
DEFINE_string(dot_file, "", "Exports model to dot file.");

DEFINE_bool(print_proto, false, "Prints the raw model protobuf.");
DEFINE_bool(test_proto, false, "Performs various tests on the model protobuf.");
DEFINE_bool(model_stats, false, "Prints model statistics.");
DEFINE_bool(print_model, false, "Pretty print loaded model.");

DEFINE_string(rename_model, "", "Renames to the model.");
DEFINE_bool(strip_limit, false, "Strips limits from the model.");
DEFINE_bool(strip_groups, false, "Strips variable groups from the model.");
DEFINE_bool(upgrade_proto, false, "Upgrade the model to the latest version.");
DEFINE_string(insert_license, "",
              "Insert content of the given file into the license file.");

namespace operations_research {
static const int kProblem = -1;
static const int kOk = 0;

// ----- Export to .dot file -----

// Appends a string to te file.
void Write(File* const file, const string& string) {
  file->Write(string.c_str(), string.size());
}

// Adds one link in the generated graph.
void WriteExprLink(const string& origin,
                   int index,
                   const string& label,
                   File* const file) {
  const string other = StringPrintf("expr_%i", index);
  Write(file, StringPrintf("%s -- %s [label=%s]\n",
                           origin.c_str(),
                           other.c_str(),
                           label.c_str()));
}

// Adds one link in the generated graph.
void WriteIntervalLink(const string& origin,
                       int index,
                       const string& label,
                       File* const file) {
  const string other = StringPrintf("interval_%i", index);
  Write(file, StringPrintf("%s -- %s [label=%s]\n",
                           origin.c_str(),
                           other.c_str(),
                           label.c_str()));
}

// Scans argument to add links in the graph.
template <class T> void ExportLinks(const CPModelProto& model,
                                    const string& origin,
                                    const T& proto,
                                    File* const file) {
  const string& arg_name = model.tags(proto.argument_index());
  if (proto.has_integer_expression_index()) {
    WriteExprLink(origin, proto.integer_expression_index(), arg_name, file);
  }
  for (int i = 0; i < proto.integer_expression_array_size(); ++i) {
    WriteExprLink(origin, proto.integer_expression_array(i), arg_name, file);
  }
  if (proto.has_interval_index()) {
    WriteIntervalLink(origin, proto.interval_index(), arg_name, file);
  }
  for (int i = 0; i < proto.interval_array_size(); ++i) {
    WriteIntervalLink(origin, proto.interval_array(i), arg_name, file);
  }
}

// Declares a labelled expression in the .dot file.
void DeclareExpression(int index, const CPModelProto& proto, File* const file) {
  const CPIntegerExpressionProto& expr = proto.expressions(index);
  const string short_name = StringPrintf("expr_%i", index);
  if (expr.has_name()) {
    Write(file, StringPrintf("%s [shape=oval label=\"%s\" color=green]\n",
                             short_name.c_str(),
                             expr.name().c_str()));
  } else {
    const string& type = proto.tags(expr.type_index());
    Write(file, StringPrintf("%s [shape=oval label=\"%s\"]\n",
                             short_name.c_str(),
                             type.c_str()));
  }
}

void DeclareInterval(int index, const CPModelProto& proto, File* const file) {
  const CPIntervalVariableProto& interval = proto.intervals(index);
  const string short_name = StringPrintf("interval_%i", index);
  if (interval.has_name()) {
    Write(file, StringPrintf("%s [shape=circle label=\"%s\" color=green]\n",
                             short_name.c_str(),
                             interval.name().c_str()));
  } else {
    const string& type = proto.tags(interval.type_index());
    Write(file, StringPrintf("%s [shape=oval label=\"%s\"]\n",
                             short_name.c_str(),
                             type.c_str()));
  }
}

void DeclareConstraint(int index, const CPModelProto& proto, File* const file) {
    const CPConstraintProto& ct = proto.constraints(index);
    const string& type = proto.tags(ct.type_index());
    const string short_name = StringPrintf("ct_%i", index);
    Write(file, StringPrintf("%s [shape=box label=\"%s\"]\n",
                             short_name.c_str(),
                             type.c_str()));
}

// Parses the proto and exports it to a .dot file.
void ExportToDot(const CPModelProto& proto, File* const file) {
  Write(file, StringPrintf("graph %s {\n", proto.model().c_str()));

  for (int i = 0; i < proto.expressions_size(); ++i) {
    DeclareExpression(i, proto, file);
  }

  for (int i = 0; i < proto.intervals_size(); ++i) {
    DeclareInterval(i, proto, file);
  }

  for (int i = 0; i < proto.constraints_size(); ++i) {
    DeclareConstraint(i, proto, file);
  }

  if (proto.has_objective()) {
    if (proto.objective().maximize()) {
      Write(file, "obj [shape=diamond label=\"Maximize\" color=red]\n");
    } else {
      Write(file, "obj [shape=diamond label=\"Minimize\" color=red]\n");
    }
  }

  for (int i = 0; i < proto.expressions_size(); ++i) {
    const CPIntegerExpressionProto& expr = proto.expressions(i);
    const string short_name = StringPrintf("expr_%i", i);
    for (int j = 0; j < expr.arguments_size(); ++j) {
      ExportLinks(proto, short_name, expr.arguments(j), file);
    }
  }

  for (int i = 0; i < proto.intervals_size(); ++i) {
    const CPIntervalVariableProto& interval = proto.intervals(i);
    const string short_name = StringPrintf("interval_%i", i);
    for (int j = 0; j < interval.arguments_size(); ++j) {
      ExportLinks(proto, short_name, interval.arguments(j), file);
    }
  }

  for (int i = 0; i < proto.constraints_size(); ++i) {
    const CPConstraintProto& ct = proto.constraints(i);
    const string short_name = StringPrintf("ct_%i", i);
    for (int j = 0; j < ct.arguments_size(); ++j) {
      ExportLinks(proto, short_name, ct.arguments(j), file);
    }
  }

  if (proto.has_objective()) {
    const CPObjectiveProto& obj = proto.objective();
    WriteExprLink("obj",
                  obj.objective_index(),
                  ModelVisitor::kExpressionArgument,
                  file);
  }

  Write(file, "}\n");
}

// ----- Main Method -----

int Run() {
  // ----- Load input file into protobuf -----

  File::Init();
  File* const file = File::Open(FLAGS_input, "r");
  if (file == NULL) {
    LOG(WARNING) << "Cannot open " << FLAGS_input;
    return kProblem;
  }

  CPModelProto model_proto;
  RecordReader reader(file);
  if (!(reader.ReadProtocolMessage(&model_proto) && reader.Close())) {
    LOG(INFO) << "No model found in " << file->CreateFileName();
    return kProblem;
  }

  // ----- Display loaded protobuf -----

  LOG(INFO) << "Read model " << model_proto.model();
  if (model_proto.has_license_text()) {
    LOG(INFO) << "License = " << model_proto.license_text();
  }

  // ----- Modifications -----

  if (!FLAGS_rename_model.empty()) {
    model_proto.set_model(FLAGS_rename_model);
  }

  if (FLAGS_strip_limit) {
    model_proto.clear_search_limit();
  }

  if (FLAGS_strip_groups) {
    model_proto.clear_variable_groups();
  }

  if (FLAGS_upgrade_proto) {
    if (!Solver::UpgradeModel(&model_proto)) {
      LOG(ERROR) << "Model upgrade failed";
      return kProblem;
    }
  }

  if (!FLAGS_insert_license.empty()) {
    File* const license = File::Open(FLAGS_insert_license, "r");
    if (license == NULL) {
      LOG(WARNING) << "Cannot open " << FLAGS_insert_license;
      return kProblem;
    }
    const int size = license->Size();
    char* const text = new char[size + 1];
    license->Read(text, size);
    text[size] = '\0';
    model_proto.set_license_text(text);
    license->Close();
  }

  // ----- Reporting -----

  if (FLAGS_print_proto) {
    LOG(INFO) << model_proto.DebugString();
  }
  if (FLAGS_test_proto || FLAGS_model_stats || FLAGS_print_model) {
    Solver solver(model_proto.model());
    std::vector<SearchMonitor*> monitors;
    if (!solver.LoadModel(model_proto, &monitors)) {
      LOG(INFO) << "Could not load model into the solver";
      return kProblem;
    }
    if (FLAGS_test_proto) {
      LOG(INFO) << "Model " << model_proto.model() << " loaded OK";
    }
    if (FLAGS_model_stats) {
      ModelVisitor* const visitor = solver.MakeStatisticsModelVisitor();
      solver.Accept(visitor, monitors);
    }
    if (FLAGS_print_model) {
      ModelVisitor* const visitor = solver.MakePrintModelVisitor();
      solver.Accept(visitor, monitors);
    }
  }

  // ----- Output -----

  if (!FLAGS_output.empty()) {
    File* const output = File::Open(FLAGS_output, "w");
    if (output == NULL) {
      LOG(INFO) << "Cannot open " << FLAGS_output;
      return kProblem;
    }
    RecordWriter writer(output);
    if (!(writer.WriteProtocolMessage(model_proto) && writer.Close())) {
      return kProblem;
    } else {
      LOG(INFO) << "Model successfully written to " << FLAGS_output;
    }
  }

  if (!FLAGS_dot_file.empty()) {
    File* const dot_file = File::Open(FLAGS_dot_file, "w");
    if (dot_file == NULL) {
      LOG(INFO) << "Cannot open " << FLAGS_dot_file;
      return kProblem;
    }
    ExportToDot(model_proto, dot_file);
    dot_file->Close();
  }
  return kOk;
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Filename not specified";
  }
  return operations_research::Run();
}
