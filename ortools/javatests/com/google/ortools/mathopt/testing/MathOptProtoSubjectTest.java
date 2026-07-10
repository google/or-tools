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

import static com.google.common.truth.ExpectFailure.expectFailureAbout;
import static com.google.common.truth.Truth.assertThat;
import static com.google.ortools.mathopt.testing.MathOptProtoSubject.assertThat;

import com.google.common.truth.ExpectFailure.SimpleSubjectBuilderCallback;
import com.google.protobuf.Message;
import org.junit.jupiter.api.Test;

public final class MathOptProtoSubjectTest {
  @Test
  public void assertThat_expectedIsIdentical_isOk() {
    var message =
        TestMessage.newBuilder().setScalar(4.0).setScalarOptional(6.0).addScalarRepeated(7.0);
    message.getSubmessageBuilder().setScalar(true).setScalarOptional(true);
    message.addSubmessageRepeatedBuilder().setScalar(true).setScalarOptional(false);
    assertThat(message.build()).isEquivalentTo(message.build());
  }

  @Test
  public void assertThat_differOptionalScalarPresence_fails() {
    var actual = TestMessage.newBuilder().setScalarOptional(0.0).build();
    AssertionError error = expectFailure(
        whenTesting -> whenTesting.that(actual).isEquivalentTo(TestMessage.getDefaultInstance()));
    assertThat(error).hasMessageThat().contains("scalar_optional");
  }

  @Test
  public void assertThat_expectedIsApproximateNearBy_isOk() {
    var actual = TestMessage.newBuilder().setScalarOptional(100.0).build();
    var expected = TestMessage.newBuilder().setScalarOptional(Math.nextUp(100.0)).build();
    assertThat(actual).isApproximatelyEquivalentTo(expected);
  }

  @Test
  public void assertThat_expectedIsApproximateFarAway_fails() {
    var actual = TestMessage.newBuilder().setScalarOptional(100.0).build();
    var expected = TestMessage.newBuilder().setScalarOptional(101.0).build();
    AssertionError error = expectFailure(
        whenTesting -> whenTesting.that(actual).isApproximatelyEquivalentTo(expected));
    assertThat(error).hasMessageThat().contains("scalar_optional");
  }

  private static AssertionError expectFailure(
      SimpleSubjectBuilderCallback<MathOptProtoSubject, Message> callback) {
    return expectFailureAbout(MathOptProtoSubject.messages(), callback);
  }
}
