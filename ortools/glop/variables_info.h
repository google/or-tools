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

#ifndef OR_TOOLS_GLOP_VARIABLES_INFO_H_
#define OR_TOOLS_GLOP_VARIABLES_INFO_H_

#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {

// Class responsible for maintaining diverse information for each variable that
// depend on its bounds and status.
//
// Note(user): Not all information is needed at all time, but it is cheap to
// maintain it since it only requires a few calls to Update() per simplex
// iteration.
class VariablesInfo {
 public:
  // Takes references to the linear program data we need.
  VariablesInfo(const CompactSparseMatrix& matrix, const DenseRow& lower_bound,
                const DenseRow& upper_bound);

  // Resets all the quantities to a default value. Note that nothing can be
  // assumed on the return values of the getters until Update() has been called
  // at least once on all the columns.
  void Initialize();

  // Updates the information of the given variable. Note that it is not needed
  // to call this if the status or the bound of a variable didn't change.
  void Update(ColIndex col, VariableStatus status);

  // Slighlty optimized version of Update() above for the two separate cases.
  void UpdateToBasicStatus(ColIndex col);
  void UpdateToNonBasicStatus(ColIndex col, VariableStatus status);

  // Various getter, see the corresponding member declaration below for more
  // information.
  const VariableTypeRow& GetTypeRow() const;
  const VariableStatusRow& GetStatusRow() const;
  const DenseBitRow& GetCanIncreaseBitRow() const;
  const DenseBitRow& GetCanDecreaseBitRow() const;
  const DenseBitRow& GetIsRelevantBitRow() const;
  const DenseBitRow& GetIsBasicBitRow() const;
  const DenseBitRow& GetNotBasicBitRow() const;
  const DenseBitRow& GetNonBasicBoxedVariables() const;

  // Returns the variable bounds.
  const DenseRow& GetVariableLowerBounds() const { return lower_bound_; }
  const DenseRow& GetVariableUpperBounds() const { return upper_bound_; }

  const ColIndex GetNumberOfColumns() const { return matrix_.num_cols(); }

  // Changes whether or not a non-basic boxed variable is 'relevant' and will be
  // returned as such by GetIsRelevantBitRow().
  void MakeBoxedVariableRelevant(bool are_boxed_variables_relevant);

  // This is used in UpdateRow to decide wheter to compute it using the row-wise
  // or column-wise representation.
  EntryIndex GetNumEntriesInRelevantColumns() const;

  // Returns the distance between the upper and lower bound of the given column.
  Fractional GetBoundDifference(ColIndex col) const {
    return upper_bound_[col] - lower_bound_[col];
  }

 private:
  // Computes the variable type from its lower and upper bound.
  VariableType ComputeVariableType(ColIndex col) const;

  // Sets the column relevance and updates num_entries_in_relevant_columns_.
  void SetRelevance(ColIndex col, bool relevance);

  // Problem data that should be updated from outside.
  const CompactSparseMatrix& matrix_;
  const DenseRow& lower_bound_;
  const DenseRow& upper_bound_;

  // Array of variable statuses, indexed by column index.
  VariableStatusRow variable_status_;

  // Array of variable types, indexed by column index.
  VariableTypeRow variable_type_;

  // Indicates if a non-basic variable can move up or down while not increasing
  // the primal infeasibility. Note that all combinaisons are possible for a
  // variable according to its status: fixed, free, upper or lower bounded. This
  // is always false for basic variable.
  DenseBitRow can_increase_;
  DenseBitRow can_decrease_;

  // Indicates if we should consider this variable for entering the basis during
  // the simplex algorithm. Only non-fixed and non-basic columns are relevant.
  DenseBitRow relevance_;

  // Indicates if a variable is BASIC or not. There are currently two members
  // because the DenseBitRow class only supports a nice range-based iteration on
  // the non-zero positions and not on the others.
  DenseBitRow is_basic_;
  DenseBitRow not_basic_;

  // Set of boxed variables that are non-basic.
  DenseBitRow non_basic_boxed_variables_;

  // Number of entries for the relevant matrix columns (see relevance_).
  EntryIndex num_entries_in_relevant_columns_;

  // Whether or not a boxed variable should be considered relevant.
  bool boxed_variables_are_relevant_;

  DISALLOW_COPY_AND_ASSIGN(VariablesInfo);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_VARIABLES_INFO_H_
