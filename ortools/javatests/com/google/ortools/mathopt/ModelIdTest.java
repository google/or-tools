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

import static com.google.common.truth.Truth.assertThat;

import org.junit.jupiter.api.Test;

public final class ModelIdTest {
  @Test
  public void create_readNameBack() {
    var modelId = new ModelId("testId");
    assertThat(modelId.getName()).isEqualTo("testId");
  }

  @Test
  public void multipleIds_sameNameNotEqual() {
    var modelId1 = new ModelId("testId");
    var modelId2 = new ModelId("testId");

    assertThat(modelId1.equals(modelId2)).isFalse();
  }
}
