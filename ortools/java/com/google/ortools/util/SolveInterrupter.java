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

import com.google.errorprone.annotations.concurrent.GuardedBy;
import java.util.ArrayList;
import java.util.ConcurrentModificationException;
import java.util.List;

/**
 * Interrupter used by solvers to know if/when they should interrupt the solve.
 *
 * <p>Once triggered with {@link #interrupt()}, an interrupter can't be reset. It can be triggered
 * from any thread.
 *
 * <p>Setting a callback on interruption is an advanced feature, most users should just create a
 * {@link SolveInterrupter} and pass it to {@link com.google.ortools.mathopt.Solve#solve(Model,
 * SolverType, com.google.ortools.mathopt.Solve.Args)}.
 *
 * <p>Thread-safety: the functions on this class are threadsafe, in that they can be invoked from
 * any thread without additional synchronization. However, in a callback registered through {@link
 * #addCallback(Runnable)}, users should generally not invoke or block on functions of {@code this}.
 * Specifically, users are not allowed to call {@link #addCallback(Runnable)}, {@link
 * #removeCallback(Runnable)} or {@link #interrupt()} on the same {@link SolveInterrupter}. Doing so
 * synchronously in the same thread will give a {@link ConcurrentModificationException}. Further,
 * blocking in a callback until another thread calls one of these functions (or {@link
 * #isInterrupted()}) will result in deadlock.
 *
 * <p>An example interrupting after 5 seconds is below (generally prefer using a time limit):
 *
 * <pre>{@code
 * static SolveResult CancelSolveAfter5s(Model model) {
 *   final var interrupter = new SolveInterrupter();
 *   new Thread(() -> {
 *       Thread.sleep(5000);
 *       interrupter.interrupt();
 *   }).start();
 *   final var args = new Solve.Args().setInterrupter(interrupter);
 *   return Solve(model, SolverType.GLOP, args);
 * }
 * }</pre>
 */
public final class SolveInterrupter implements Interruptable {
  // Begins as false, can only be changed to true.
  @GuardedBy("this") private boolean interrupted = false;

  @GuardedBy("this") private final List<Runnable> callbacks = new ArrayList<>();

  // Used to determine if we are already running callbacks while attempting to add or remove a
  // callback. This is needed because java locks are reentrant.
  @GuardedBy("this") private boolean runningCallbacks = false;

  /** A new interrupter in the not interrupted state. */
  public SolveInterrupter() {}

  /**
   * Marks this interrupter as interrupted and runs callbacks if this is the first interruption.
   *
   * <p>Invoking this from within a callback on the same instance (or blocking in the callback until
   * another thread invokes this function) is not allowed.
   */
  @Override
  public synchronized void interrupt() {
    checkNotAlreadyRunningCallbacks("interrupt()");
    if (interrupted) {
      return;
    }
    interrupted = true;
    runningCallbacks = true;
    try {
      for (Runnable cb : callbacks) {
        cb.run();
      }
    } finally {
      runningCallbacks = false;
    }
  }

  /**
   * Indicates if {@link #interrupt()} has ever been called.
   *
   * <p>Invoking this from within a callback on the same instance (or blocking in the callback until
   * another thread invokes this function) is not allowed.
   */
  public synchronized boolean isInterrupted() {
    return interrupted;
  }

  /**
   * Registers {@code callback} to be run on the first interrupt, or runs {@code callback}
   * immediately if {@code this} already {@link #isInterrupted()}.
   *
   * <p>The input callback is returned. This can be useful if you initialize the callback inline and
   * then need to remove it later.
   *
   * <p>Invoking this from within a callback on the same instance (or blocking in the callback until
   * another thread invokes this function) is not allowed.
   *
   * <p>If the same {@link Runnable} is added as a callback multiple times, it will be invoked
   * multiple times.
   */
  public synchronized<T extends Runnable> T addCallback(T callback) {
    checkNotAlreadyRunningCallbacks("addCallback()");
    callbacks.add(callback);
    if (interrupted) {
      runningCallbacks = true;
      try {
        callback.run();
      } finally {
        runningCallbacks = false;
      }
    }
    return callback;
  }

  /**
   * Removes {@code callback} from the list of callbacks to be run on first interrupt, returning
   * true if callback was in the list of callbacks.
   *
   * <p>Invoking this from within a callback on the same instance (or blocking in the callback until
   * another thread invokes this function) is not allowed.
   *
   * <p>If the same {@link Runnable} is added as a callback multiple times, it must be removed this
   * many times so it will not be invoked again.
   */
  public synchronized boolean removeCallback(Runnable callback) {
    checkNotAlreadyRunningCallbacks("removeCallback()");
    return callbacks.remove(callback);
  }

  private synchronized void checkNotAlreadyRunningCallbacks(String errorContext) {
    if (runningCallbacks) {
      throw new ConcurrentModificationException("Attempted to invoke " + errorContext
          + " from within a callback on a SolveInterrupter that is already running, this is not"
          + " allowed");
    }
  }

  /** See {@link #addAutoRemovedCallback(Runnable)} for use. */
  public final class CallbackRemover implements AutoCloseable {
    private final Runnable callback;

    CallbackRemover(Runnable callback) {
      this.callback = callback;
    }

    @Override
    public void close() {
      removeCallback(callback);
    }
  }

  /**
   * Add a callback to be invoked on interruption that can be removed using try-with-resources.
   *
   * <p>Useful when you must ensure that a callback will not be invoked, e.g. the callback calls a
   * SWIG wrapped C++ object that has been disposed of.
   *
   * <p>Invoking this from within a callback on the same instance (or blocking in the callback until
   * another thread invokes this function) is not allowed.
   *
   * <p>An example of use:
   *
   * <pre>{@code
   * static SolveResult LogSolveInterruption(Model model) {
   *   final var interrupter = new SolveInterrupter();
   *   Runnable onInterrupt = () -> System.out.println("was interrupted");
   *   try(CallbackRemover unused = interrupter.addAutoRemovedCallback(onInterrupt)) {
   *     new Thread(() -> {
   *       Thread.sleep(5000);
   *       interrupter.interrupt();
   *     }).start();
   *     final var args = new Solve.Args().setInterrupter(interrupter);
   *     return Solve(model, SolverType.GLOP, args);
   *   }
   * }
   * }</pre>
   *
   * <p>In the example above, if the background thread finishes after {@code LogSolveInterruption}
   * returns, the message will not be printed, since the callback will be removed.
   */
  public CallbackRemover addAutoRemovedCallback(Runnable callback) {
    addCallback(callback);
    return new CallbackRemover(callback);
  }

  /**
   * Adds a callback to {@code this} to interrupt {@code interruptable} when {@code this} is
   * interrupted, or does so immediately if already {@link #isInterrupted()}.
   *
   * <p>Returns an {@link AutoCloseable} to remove the callback from {@code this}.
   *
   * <p>An example of use:
   *
   * <pre>{@code
   * var primaryInterrupter = new SolveInterrupter();
   * var dependantInterrupter = new SolveInterrupter();
   * try(var unused = primaryInterrupter.chain(dependantInterrupter)) {
   *   primaryInterrupter.interrupt();  // Triggers both interrupters.
   * }
   * // Callback connecting interrupters is unregistered at the end of the try-with-resources block.
   * dependantInterrupter.isInterrupted();  // Returns true.
   * }</pre>
   *
   * @see #addAutoRemovedCallback(Runnable)
   */
  public CallbackRemover chain(Interruptable interruptable) {
    return addAutoRemovedCallback(interruptable::interrupt);
  }
}
