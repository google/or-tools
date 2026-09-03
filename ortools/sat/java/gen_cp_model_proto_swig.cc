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

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/die_if_null.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_format.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/java/wrappers.h"

namespace operations_research::sat {

void ParseAndGenerate() {
  absl::PrintF(
      R"(
// This is a generated file, do not edit.
%%module cp_model_proto

%%typemap(javabody) SWIGTYPE %%{
  private transient long swigCPtr;
  protected transient boolean swigCMemOwn;

  public $javaclassname(long cPtr, boolean cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = cPtr;
  }

  public static long getCPtr($javaclassname obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
  }
%%}

%%{
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
%%}

%%include "ortools/base/base.i"

%s
)",
      operations_research::util::java::GenerateJavaSwigCode(
          {
              ABSL_DIE_IF_NULL(AllDifferentConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(AutomatonConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(BoolArgumentProto::descriptor()),
              ABSL_DIE_IF_NULL(CircuitConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(ConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(CpModelProto::descriptor()),
              ABSL_DIE_IF_NULL(CpObjectiveProto::descriptor()),
              ABSL_DIE_IF_NULL(CpSolverResponse::descriptor()),
              ABSL_DIE_IF_NULL(CpSolverSolution::descriptor()),
              ABSL_DIE_IF_NULL(CumulativeConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(DecisionStrategyProto::descriptor()),
              ABSL_DIE_IF_NULL(DenseMatrixProto::descriptor()),
              ABSL_DIE_IF_NULL(ElementConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(FloatObjectiveProto::descriptor()),
              ABSL_DIE_IF_NULL(IntegerVariableProto::descriptor()),
              ABSL_DIE_IF_NULL(IntervalConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(InverseConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(LinearArgumentProto::descriptor()),
              ABSL_DIE_IF_NULL(LinearConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(LinearExpressionProto::descriptor()),
              ABSL_DIE_IF_NULL(ListOfVariablesProto::descriptor()),
              ABSL_DIE_IF_NULL(NoOverlap2DConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(NoOverlapConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(PartialVariableAssignment::descriptor()),
              ABSL_DIE_IF_NULL(ReservoirConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(RoutesConstraintProto::descriptor()),
              ABSL_DIE_IF_NULL(SparsePermutationProto::descriptor()),
              ABSL_DIE_IF_NULL(SymmetryProto::descriptor()),
              ABSL_DIE_IF_NULL(TableConstraintProto::descriptor()),
          },
          {
              ABSL_DIE_IF_NULL(CpSolverStatus_descriptor()),
              ABSL_DIE_IF_NULL(
                  DecisionStrategyProto_VariableSelectionStrategy_descriptor()),
              ABSL_DIE_IF_NULL(
                  DecisionStrategyProto_DomainReductionStrategy_descriptor()),
          }));
}

}  // namespace operations_research::sat

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);
  operations_research::sat::ParseAndGenerate();
  return 0;
}
