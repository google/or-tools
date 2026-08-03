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
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.ortools.util.SolveInterrupter.CallbackRemover;
import java.util.ConcurrentModificationException;
import org.junit.jupiter.api.Test;

public final class SolveInterrupterTest {
  @Test
  public void interrupter_interrupt_isInterrupted() {
    var interrupter = new SolveInterrupter();
    assertThat(interrupter.isInterrupted()).isFalse();

    interrupter.interrupt();

    assertThat(interrupter.isInterrupted()).isTrue();
  }

  private static final class TestCallback implements Runnable {
    private int numTimesInvoked = 0;

    @Override
    public void run() {
      numTimesInvoked++;
    }

    public int getNumTimesInvoked() {
      return numTimesInvoked;
    }
  }

  @Test
  public void interrupt_withCallback_isInvoked() {
    var interrupter = new SolveInterrupter();
    var cb = interrupter.addCallback(new TestCallback());
    assertThat(cb.getNumTimesInvoked()).isEqualTo(0);

    interrupter.interrupt();

    assertThat(cb.getNumTimesInvoked()).isEqualTo(1);
  }

  @Test
  public void interrupt_withRemovedCallback_notInvoked() {
    var interrupter = new SolveInterrupter();
    var cb = interrupter.addCallback(new TestCallback());

    assertThat(interrupter.removeCallback(cb)).isTrue();
    interrupter.interrupt();

    assertThat(cb.getNumTimesInvoked()).isEqualTo(0);
  }

  @Test
  public void interruptTwice_withCallback_isInvokedOnce() {
    var interrupter = new SolveInterrupter();
    var cb = interrupter.addCallback(new TestCallback());

    interrupter.interrupt();
    interrupter.interrupt();

    assertThat(cb.getNumTimesInvoked()).isEqualTo(1);
  }

  @Test
  public void interrupt_beforeAddingCallback_isInvokedOnce() {
    var interrupter = new SolveInterrupter();
    interrupter.interrupt();

    var cb = interrupter.addCallback(new TestCallback());

    assertThat(cb.getNumTimesInvoked()).isEqualTo(1);
  }

  @Test
  public void autoclose_interruptBeforeRemove_isInvoked() {
    var interrupter = new SolveInterrupter();
    var cb = new TestCallback();

    try (CallbackRemover remover = interrupter.addAutoRemovedCallback(cb)) {
      interrupter.interrupt();
    }

    assertThat(cb.getNumTimesInvoked()).isEqualTo(1);
  }

  @Test
  public void autoclose_interruptAfterRemove_isNotInvoked() {
    var interrupter = new SolveInterrupter();
    var cb = new TestCallback();
    try (CallbackRemover remover = interrupter.addAutoRemovedCallback(cb)) {
      // Auto formatter + linter can't agree on empty try with resources
    }

    interrupter.interrupt();

    assertThat(cb.getNumTimesInvoked()).isEqualTo(0);
  }

  @Test
  public void addCallbackTwice_interrupt_isInvokedTwice() {
    var interrupter = new SolveInterrupter();
    var cb = new TestCallback();

    interrupter.addCallback(cb);
    interrupter.addCallback(cb);
    interrupter.interrupt();

    assertThat(cb.getNumTimesInvoked()).isEqualTo(2);
  }

  @Test
  public void removeCallback_neverAdded_isFalse() {
    var interrupter = new SolveInterrupter();
    assertThat(interrupter.removeCallback(new TestCallback())).isFalse();
  }

  @Test
  public void removeCallbackTwice_addedOnce_isFalse() {
    var interrupter = new SolveInterrupter();
    var cb = interrupter.addCallback(new TestCallback());

    assertThat(interrupter.removeCallback(cb)).isTrue();
    assertThat(interrupter.removeCallback(cb)).isFalse();
  }

  @Test
  public void removeCallbackTwice_addedTwice_isTrue() {
    var interrupter = new SolveInterrupter();
    var cb = interrupter.addCallback(new TestCallback());
    interrupter.addCallback(cb);

    assertThat(interrupter.removeCallback(cb)).isTrue();
    assertThat(interrupter.removeCallback(cb)).isTrue();
  }

  @Test
  public void interrupt_fromCallback_throws() {
    var interrupter = new SolveInterrupter();
    interrupter.addCallback(interrupter::interrupt);

    var error = assertThrows(ConcurrentModificationException.class, interrupter::interrupt);

    assertThat(error).hasMessageThat().contains("interrupt()");
  }

  @Test
  public void addCallback_fromCallback_throws() {
    var interrupter = new SolveInterrupter();
    interrupter.addCallback(() -> interrupter.addCallback(() -> {}));

    var error = assertThrows(ConcurrentModificationException.class, interrupter::interrupt);

    assertThat(error).hasMessageThat().contains("addCallback()");
  }

  @Test
  public void removeCallback_fromCallback_throws() {
    var interrupter = new SolveInterrupter();
    var cb2 = new TestCallback();
    interrupter.addCallback(() -> interrupter.removeCallback(cb2));

    var error = assertThrows(ConcurrentModificationException.class, interrupter::interrupt);

    assertThat(error).hasMessageThat().contains("removeCallback()");
  }

  @Test
  public void addCallback_alreadyInterruptedAndCallbackInterrupts_throws() {
    var interrupter = new SolveInterrupter();
    interrupter.interrupt();
    Runnable invalidCb = interrupter::interrupt;

    var error = assertThrows(
        ConcurrentModificationException.class, () -> interrupter.addCallback(invalidCb));

    assertThat(error).hasMessageThat().contains("interrupt()");
  }

  @Test
  public void interrupt_callbackThrows_interrupterStillReady() {
    var interrupter = new SolveInterrupter();
    class TestException extends RuntimeException {}
    interrupter.addCallback(() -> { throw new TestException(); });

    assertThrows(TestException.class, interrupter::interrupt);

    // We want to test that the interrupter is in a valid state, specifically, that we do not get a
    // concurrent modification error.
    assertThat(interrupter.isInterrupted()).isTrue();
    interrupter.interrupt();
  }

  @Test
  public void addCallback_alreadyInterruptedCallbackThrows_interrupterStillReady() {
    var interrupter = new SolveInterrupter();
    interrupter.interrupt();
    class TestException extends RuntimeException {}
    Runnable cb = () -> {
      throw new TestException();
    };

    assertThrows(TestException.class, () -> interrupter.addCallback(cb));

    // We want to test that the interrupter is in a valid state, specifically, that we do not get a
    // concurrent modification error.
    assertThat(interrupter.isInterrupted()).isTrue();
    interrupter.interrupt();
  }

  @Test
  public void chain_interruptAfter_chainedInterrupterIsInterrupted() {
    var interrupter = new SolveInterrupter();
    var dependent = new SolveInterrupter();
    try (CallbackRemover unused = interrupter.chain(dependent)) {
      interrupter.interrupt();
    }
    assertThat(dependent.isInterrupted()).isTrue();
  }

  @Test
  public void chain_interruptedBefore_chainedInterrupterIsInterrupted() {
    var interrupter = new SolveInterrupter();
    interrupter.interrupt();
    var dependent = new SolveInterrupter();
    try (CallbackRemover unused = interrupter.chain(dependent)) {
      // Nothing to do
    }
    assertThat(dependent.isInterrupted()).isTrue();
  }

  @Test
  public void chain_interruptedAfterRemovingCallback_chainedInterrupterIsNotInterrupted() {
    var interrupter = new SolveInterrupter();
    var dependent = new SolveInterrupter();
    try (CallbackRemover unused = interrupter.chain(dependent)) {
      // Nothing to do
    }
    interrupter.interrupt();
    assertThat(dependent.isInterrupted()).isFalse();
  }
}
