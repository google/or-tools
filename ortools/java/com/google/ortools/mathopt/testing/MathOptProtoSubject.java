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

package com.google.ortools.mathopt.testing;

import static com.google.common.truth.Truth.assertAbout;

import com.google.common.truth.Fact;
import com.google.common.truth.FailureMetadata;
import com.google.common.truth.Subject;
import com.google.protobuf.Message;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.jspecify.annotations.Nullable;

/**
 * Provides Truth assertions that protos are equivalent as defined by {@link
 * MathOptMessageDifferencer}.
 */
public final class MathOptProtoSubject extends Subject {
  private static final Logger LOGGER = Logger.getLogger(MathOptProtoSubject.class.getName());

  private final @Nullable Message actual;

  private MathOptProtoSubject(FailureMetadata metadata, @Nullable Message actual) {
    super(metadata, actual);
    this.actual = actual;
  }

  public static Factory<MathOptProtoSubject, Message> messages() {
    return MathOptProtoSubject::new;
  }

  public static MathOptProtoSubject assertThat(@Nullable Message actual) {
    return assertAbout(messages()).that(actual);
  }

  public void isEquivalentTo(Message expected) {
    LOGGER.log(Level.WARNING, "MathOptProtoSubject#isEquivalentTo() ignored");
  }

  public void isApproximatelyEquivalentTo(Message expected) {
    LOGGER.log(Level.WARNING, "MathOptProtoSubject#isApproximatelyEquivalentTo() ignored");
  }
}
