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

package com.google.ortools.util;

import static com.google.common.truth.Truth.assertThat;

import com.google.ortools.Loader;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class ScopedSwigInterrupterTest {
  @BeforeEach
  public void loadJni() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void new_notPresent_swiggedObjectIsNull() {
    var swigInterrupter = new ScopedSwigInterrupter(/* isPresent= */ false);
    assertThat(swigInterrupter.getSwigInterrupter()).isNull();
  }

  @Test
  public void new_notPresent_closeOk() {
    var swigInterrupter = new ScopedSwigInterrupter(/* isPresent= */ false);
    swigInterrupter.close();
    assertThat(swigInterrupter.getSwigInterrupter()).isNull();
  }

  @Test
  public void new_notPresent_interruptOk() {
    var swigInterrupter = new ScopedSwigInterrupter(/* isPresent= */ false);
    swigInterrupter.interrupt();
    assertThat(swigInterrupter.getSwigInterrupter()).isNull();
  }

  @Test
  public void new_isPresent_closeOnceOk() {
    var swigInterrupter = new ScopedSwigInterrupter();
    assertThat(swigInterrupter.getSwigInterrupter()).isNotNull();
    swigInterrupter.close();
    assertThat(swigInterrupter.getSwigInterrupter()).isNull();
  }

  @Test
  public void new_isPresent_closeTwiceOk() {
    var swigInterrupter = new ScopedSwigInterrupter();
    swigInterrupter.close();
    swigInterrupter.close();
    assertThat(swigInterrupter.getSwigInterrupter()).isNull();
  }

  @Test
  public void new_isPresentAndInterrupt_isInterrupted() {
    var swigInterrupter = new ScopedSwigInterrupter();
    assertThat(swigInterrupter.getSwigInterrupter().isInterrupted()).isFalse();
    swigInterrupter.interrupt();
    assertThat(swigInterrupter.getSwigInterrupter().isInterrupted()).isTrue();
  }

  @Test
  public void interrupter_tryWithResources_cleanupOk() {
    ScopedSwigInterrupter interrupterRef = null;
    try (var swigInterrupter = new ScopedSwigInterrupter()) {
      interrupterRef = swigInterrupter;
    }
    assertThat(interrupterRef.getSwigInterrupter()).isNull();
  }
}
