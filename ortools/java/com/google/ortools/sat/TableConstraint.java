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

package com.google.ortools.sat;

import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.TableConstraintProto;

/**
 * Specialized assignment constraint.
 *
 * <p>This constraint allows adding tuples to the allowed/forbidden assignment constraint
 * incrementally.
 */
public class TableConstraint extends Constraint {
  public TableConstraint(CpModelProto.Builder builder) {
    super(builder);
  }

  /**
   * Adds a tuple of possible/forbidden values to the constraint.
   *
   * @param tuple the tuple to add to the constraint.
   * @throws CpModel.WrongLength if the tuple does not have the same length as the array of
   *     expressions of the constraint.
   */
  public TableConstraint addTuple(int[] tuple) {
    TableConstraintProto.Builder table = getBuilder().getTableBuilder();
    if (tuple.length != table.getExprsCount()) {
      throw new CpModel.WrongLength(
          "addTuple", "tuple does not have the same length as the expressions");
    }
    for (int value : tuple) {
      table.addValues(value);
    }
    return this;
  }

  /**
   * Adds a tuple of possible/forbidden values to the constraint.
   *
   * @param tuple the tuple to add to the constraint.
   * @throws CpModel.WrongLength if the tuple does not have the same length as the array of
   *     expressions of the constraint.
   */
  public TableConstraint addTuple(long[] tuple) {
    TableConstraintProto.Builder table = getBuilder().getTableBuilder();
    if (tuple.length != table.getExprsCount()) {
      throw new CpModel.WrongLength(
          "addTuple", "tuple does not have the same length as the expressions");
    }
    for (long value : tuple) {
      table.addValues(value);
    }
    return this;
  }

  /**
   * Adds a list of tuples of possible/forbidden values to the constraint.
   *
   * @param tuples the list of tuples to add to the constraint.
   * @throws CpModel.WrongLength if one tuple does not have the same length as the array of
   *     variables of the constraint.
   */
  public TableConstraint addTuples(int[][] tuples) {
    TableConstraintProto.Builder table = getBuilder().getTableBuilder();
    for (int[] tuple : tuples) {
      if (tuple.length != table.getExprsCount()) {
        throw new CpModel.WrongLength(
            "addTuples", "a tuple does not have the same length as the expressions");
      }
      for (int value : tuple) {
        table.addValues(value);
      }
    }
    return this;
  }

  /**
   * Adds a list of tuples of possible/forbidden values to the constraint.
   *
   * @param tuples the list of tuples to add to the constraint.
   * @throws CpModel.WrongLength if one tuple does not have the same length as the array of
   *     expressions of the constraint.
   */
  public TableConstraint addTuples(long[][] tuples) {
    TableConstraintProto.Builder table = getBuilder().getTableBuilder();
    for (long[] tuple : tuples) {
      if (tuple.length != table.getExprsCount()) {
        throw new CpModel.WrongLength(
            "addTuples", "a tuple does not have the same length as the variables");
      }
      for (long value : tuple) {
        table.addValues(value);
      }
    }
    return this;
  }
}
