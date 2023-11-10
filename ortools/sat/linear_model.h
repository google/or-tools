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

#ifndef OR_TOOLS_SAT_LINEAR_MODEL_H_
#define OR_TOOLS_SAT_LINEAR_MODEL_H_

#include <vector>

#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

// This class is meant to be a view on the full CpModelProto, with hidden and
// additional constraints.
// Currently, this class is meant to be used by the feasibility jump subsolver.
// It could also contains the linear relaxation at level 1 or 2 of the model and
// could be computed once for all workers of a given linearization level.
class LinearModel {
 public:
  explicit LinearModel(const CpModelProto& model_proto);

  const CpModelProto& model_proto() const { return model_proto_; }

  // Mask on the constraints of the model passed to the ctor.
  const std::vector<bool>& ignored_constraints() const {
    return ignored_constraints_;
  }

  // Additional constraints created during the initialization.
  const std::vector<ConstraintProto>& additional_constraints() const {
    return additional_constraints_;
  }

  int num_ignored_constraints() const { return num_ignored_constraints_; }
  int num_exactly_ones() const { return num_exactly_ones_; }
  int num_full_encodings() const { return num_full_encodings_; }
  int num_element_encodings() const { return num_element_encodings_; }

 private:
  // Initial model.
  const CpModelProto& model_proto_;

  // Model delta.
  std::vector<bool> ignored_constraints_;
  std::vector<ConstraintProto> additional_constraints_;

  // Statistics.
  int num_ignored_constraints_ = 0;
  int num_exactly_ones_ = 0;
  int num_full_encodings_ = 0;
  int num_element_encodings_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_MODEL_H_
