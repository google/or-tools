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


#include <memory>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/file.h"
#include "ortools/base/recordio.h"
#include "ortools/base/join.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/model.pb.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/util/graph_export.h"
#include "ortools/util/string_array.h"
#include "ortools/base/status.h"

DEFINE_string(input, "", "Input file of the problem.");
DEFINE_string(output, "", "Output file when doing modifications.");
DEFINE_string(dot_file, "", "Exports model to dot file.");
DEFINE_string(gml_file, "", "Exports model to gml file.");

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
DEFINE_bool(collect_variables, false,
            "Shows effect of the variable collector.");

namespace operations_research {

// ----- Utilities -----

static const int kProblem = -1;
static const int kOk = 0;

// Colors
static const char kGreen1[] = "#A2CD5A";
static const char kGreen2[] = "#76EEC6";
static const char kGreen3[] = "#00CD00";
static const char kWhite[] = "#FAFAFA";
static const char kBlue[] = "#87CEFA";
static const char kYellow[] = "#FFF68F";
static const char kRed[] = "#A52A2A";

// Creates node labels.
std::string ExprLabel(int index) { return StringPrintf("expr_%i", index); }

std::string IntervalLabel(int index) { return StringPrintf("interval_%i", index); }

std::string SequenceLabel(int index) { return StringPrintf("sequence_%i", index); }

std::string ConstraintLabel(int index) { return StringPrintf("ct_%i", index); }

// Scans argument to add links in the graph.
template <class T>
void ExportLinks(const CpModel& model, const std::string& source, const T& proto,
                 GraphExporter* const exporter) {
  const std::string& arg_name = model.tags(proto.argument_index());
  if (proto.type() == CpArgument::EXPRESSION) {
    exporter->WriteLink(source, ExprLabel(proto.integer_expression_index()),
                        arg_name);
  }
  for (int i = 0; i < proto.integer_expression_array_size(); ++i) {
    exporter->WriteLink(source, ExprLabel(proto.integer_expression_array(i)),
                        arg_name);
  }
  if (proto.type() == CpArgument::INTERVAL) {
    exporter->WriteLink(source, IntervalLabel(proto.interval_index()),
                        arg_name);
  }
  for (int i = 0; i < proto.interval_array_size(); ++i) {
    exporter->WriteLink(source, IntervalLabel(proto.interval_array(i)),
                        arg_name);
  }
  if (proto.type() == CpArgument::SEQUENCE) {
    exporter->WriteLink(source, SequenceLabel(proto.sequence_index()),
                        arg_name);
  }
  for (int i = 0; i < proto.sequence_array_size(); ++i) {
    exporter->WriteLink(source, SequenceLabel(proto.sequence_array(i)),
                        arg_name);
  }
}

// Scans the expression protobuf to see if it corresponds to an
// integer variable with min_value == max_value.
bool GetValueIfConstant(const CpModel& model, const CpIntegerExpression& proto,
                        int64* const value) {
  CHECK_NOTNULL(value);
  const int expr_type = proto.type_index();
  if (model.tags(expr_type) != ModelVisitor::kIntegerVariable) {
    return false;
  }
  if (proto.arguments_size() != 2) {
    return false;
  }
  const CpArgument& arg1_proto = proto.arguments(0);
  if (model.tags(arg1_proto.argument_index()) != ModelVisitor::kMinArgument) {
    return false;
  }
  const int64 value1 = arg1_proto.integer_value();
  const CpArgument& arg2_proto = proto.arguments(1);
  if (model.tags(arg2_proto.argument_index()) != ModelVisitor::kMaxArgument) {
    return false;
  }
  const int64 value2 = arg2_proto.integer_value();
  if (value1 == value2) {
    *value = value1;
    return true;
  } else {
    return false;
  }
}

// Declares a labelled expression in the graph file.
void DeclareExpression(int index, const CpModel& proto,
                       GraphExporter* const exporter) {
  const CpIntegerExpression& expr = proto.expressions(index);
  const std::string label = ExprLabel(index);
  int64 value = 0;
  if (!expr.name().empty()) {
    exporter->WriteNode(label, expr.name(), "oval", kGreen1);
  } else if (GetValueIfConstant(proto, expr, &value)) {
    exporter->WriteNode(label, StrCat(value), "oval", kYellow);
  } else {
    const std::string& type = proto.tags(expr.type_index());
    exporter->WriteNode(label, type, "oval", kWhite);
  }
}

void DeclareInterval(int index, const CpModel& proto,
                     GraphExporter* const exporter) {
  const CpIntervalVariable& interval = proto.intervals(index);
  const std::string label = IntervalLabel(index);
  if (!interval.name().empty()) {
    exporter->WriteNode(label, interval.name(), "circle", kGreen2);
  } else {
    const std::string& type = proto.tags(interval.type_index());
    exporter->WriteNode(label, type, "circle", kWhite);
  }
}

void DeclareSequence(int index, const CpModel& proto,
                     GraphExporter* const exporter) {
  const CpSequenceVariable& sequence = proto.sequences(index);
  const std::string label = SequenceLabel(index);
  if (!sequence.name().empty()) {
    exporter->WriteNode(label, sequence.name(), "circle", kGreen3);
  } else {
    const std::string& type = proto.tags(sequence.type_index());
    exporter->WriteNode(label, type, "circle", kWhite);
  }
}

void DeclareConstraint(int index, const CpModel& proto,
                       GraphExporter* const exporter) {
  const CpConstraint& ct = proto.constraints(index);
  const std::string& type = proto.tags(ct.type_index());
  const std::string label = ConstraintLabel(index);
  exporter->WriteNode(label, type, "rectangle", kBlue);
}

// Parses the proto and exports it to a graph file.
void ExportToGraphFile(const CpModel& proto, File* const file,
                       GraphExporter::GraphFormat format) {
  std::unique_ptr<GraphExporter> exporter(
      GraphExporter::MakeFileExporter(file, format));
  exporter->WriteHeader(proto.model());
  for (int i = 0; i < proto.expressions_size(); ++i) {
    DeclareExpression(i, proto, exporter.get());
  }

  for (int i = 0; i < proto.intervals_size(); ++i) {
    DeclareInterval(i, proto, exporter.get());
  }

  for (int i = 0; i < proto.sequences_size(); ++i) {
    DeclareSequence(i, proto, exporter.get());
  }

  for (int i = 0; i < proto.constraints_size(); ++i) {
    DeclareConstraint(i, proto, exporter.get());
  }

  const char kObjLabel[] = "obj";
  if (proto.has_objective()) {
    const std::string name = proto.objective().maximize() ? "Maximize" : "Minimize";
    exporter->WriteNode(kObjLabel, name, "diamond", kRed);
  }

  for (int i = 0; i < proto.expressions_size(); ++i) {
    const CpIntegerExpression& expr = proto.expressions(i);
    const std::string label = ExprLabel(i);
    for (int j = 0; j < expr.arguments_size(); ++j) {
      ExportLinks(proto, label, expr.arguments(j), exporter.get());
    }
  }

  for (int i = 0; i < proto.intervals_size(); ++i) {
    const CpIntervalVariable& interval = proto.intervals(i);
    const std::string label = IntervalLabel(i);
    for (int j = 0; j < interval.arguments_size(); ++j) {
      ExportLinks(proto, label, interval.arguments(j), exporter.get());
    }
  }

  for (int i = 0; i < proto.sequences_size(); ++i) {
    const CpSequenceVariable& sequence = proto.sequences(i);
    const std::string label = SequenceLabel(i);
    for (int j = 0; j < sequence.arguments_size(); ++j) {
      ExportLinks(proto, label, sequence.arguments(j), exporter.get());
    }
  }

  for (int i = 0; i < proto.constraints_size(); ++i) {
    const CpConstraint& ct = proto.constraints(i);
    const std::string label = ConstraintLabel(i);
    for (int j = 0; j < ct.arguments_size(); ++j) {
      ExportLinks(proto, label, ct.arguments(j), exporter.get());
    }
  }

  if (proto.has_objective()) {
    const CpObjective& obj = proto.objective();
    exporter->WriteLink(kObjLabel, ExprLabel(obj.objective_index()),
                        ModelVisitor::kExpressionArgument);
  }
  exporter->WriteFooter();
}

// ----- Main Method -----

int Run() {
  // ----- Load input file into protobuf -----

  File* file;
  if (!file::Open(FLAGS_input, "r", &file, file::Defaults()).ok()) {
    LOG(WARNING) << "Cannot open " << FLAGS_input;
    return kProblem;
  }

  CpModel model_proto;
  recordio::RecordReader reader(file);
  if (!(reader.ReadProtocolMessage(&model_proto) && reader.Close())) {
    LOG(INFO) << "No model found in " << file->filename();
    return kProblem;
  }

  // ----- Display loaded protobuf -----

  LOG(INFO) << "Read model " << model_proto.model();
  if (!model_proto.license_text().empty()) {
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
    File* license;
    if (!file::Open(FLAGS_insert_license, "rb", &license, file::Defaults())
             .ok()) {
      LOG(WARNING) << "Cannot open " << FLAGS_insert_license;
      return kProblem;
    }
    const int size = license->Size();
    char* const text = new char[size + 1];
    license->Read(text, size);
    text[size] = '\0';
    model_proto.set_license_text(text);
    license->Close(file::Defaults()).IgnoreError();
  }

  // ----- Reporting -----

  if (FLAGS_print_proto) {
    LOG(INFO) << model_proto.DebugString();
  }
  if (FLAGS_test_proto || FLAGS_model_stats || FLAGS_print_model ||
      FLAGS_collect_variables) {
    Solver solver(model_proto.model());
    std::vector<SearchMonitor*> monitors;
    if (!solver.LoadModelWithSearchMonitors(model_proto, &monitors)) {
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
    if (FLAGS_collect_variables) {
      std::vector<IntVar*> primary_integer_variables;
      std::vector<IntVar*> secondary_integer_variables;
      std::vector<SequenceVar*> sequence_variables;
      std::vector<IntervalVar*> interval_variables;
      solver.CollectDecisionVariables(&primary_integer_variables,
                                      &secondary_integer_variables,
                                      &sequence_variables, &interval_variables);
      LOG(INFO) << "Primary integer variables = "
                << JoinDebugStringPtr(primary_integer_variables, ", ");
      LOG(INFO) << "Secondary integer variables = "
                << JoinDebugStringPtr(secondary_integer_variables, ", ");
      LOG(INFO) << "Sequence variables = " << JoinDebugStringPtr(
                                                  sequence_variables, ", ");
      LOG(INFO) << "Interval variables = " << JoinDebugStringPtr(
                                                  interval_variables, ", ");
    }
  }

  // ----- Output -----

  if (!FLAGS_output.empty()) {
    File* output;
    if (!file::Open(FLAGS_output, "wb", &output, file::Defaults()).ok()) {
      LOG(INFO) << "Cannot open " << FLAGS_output;
      return kProblem;
    }
    recordio::RecordWriter writer(output);
    if (!(writer.WriteProtocolMessage(model_proto) && writer.Close())) {
      return kProblem;
    } else {
      LOG(INFO) << "Model successfully written to " << FLAGS_output;
    }
  }

  if (!FLAGS_dot_file.empty()) {
    File* dot_file;
    if (!file::Open(FLAGS_dot_file, "w", &dot_file, file::Defaults()).ok()) {
      LOG(INFO) << "Cannot open " << FLAGS_dot_file;
      return kProblem;
    }
    ExportToGraphFile(model_proto, dot_file, GraphExporter::DOT_FORMAT);
    dot_file->Close(file::Defaults()).IgnoreError();
  }

  if (!FLAGS_gml_file.empty()) {
    File* gml_file;
    if (!file::Open(FLAGS_gml_file, "w", &gml_file, file::Defaults()).ok()) {
      LOG(INFO) << "Cannot open " << FLAGS_gml_file;
      return kProblem;
    }
    ExportToGraphFile(model_proto, gml_file, GraphExporter::GML_FORMAT);
    gml_file->Close(file::Defaults()).IgnoreError();
  }
  return kOk;
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Filename not specified";
  }
  return operations_research::Run();
}
