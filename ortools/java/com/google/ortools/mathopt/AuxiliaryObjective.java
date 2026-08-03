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

package com.google.ortools.mathopt;

import com.google.common.base.Preconditions;

/**
 * Additional objectives for optimization models with multiple hierarchical objectives.
 *
 * <p>Unlike the primary objective, auxiliary objectives have an id and can be deleted. Users who do
 * not need these features (most users) can program against the parent class {@link Objective}
 * instead, allowing more uniform treatment of the primary and auxiliary objectives.
 */
public final class AuxiliaryObjective extends Objective implements ModelElement {
  private final long id;
  private boolean deleted = false;

  AuxiliaryObjective(
      long priority, String name, long id, OnChangeListener listener, ModelId modelId) {
    super(priority, name, listener, modelId);
    Preconditions.checkArgument(id >= 0, "id should be nonnegative, was %s", id);
    this.id = id;
  }

  @Override
  public long getId() {
    return id;
  }

  @Override
  public boolean isDeleted() {
    return deleted;
  }

  @Override
  public String toString() {
    String label = "Objective " + id;
    if (getName().isEmpty()) {
      return label;
    } else {
      return getName() + " (" + label + ")";
    }
  }

  /**
   * To be called when this objective is deleted from the model, ensures future modifications result
   * in an error. It is an error to call this on the primary objective.
   */
  void markDeleted() {
    deleted = true;
  }
}
