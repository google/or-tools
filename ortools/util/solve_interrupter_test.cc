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

#include "ortools/util/solve_interrupter.h"

#include <string>
#include <type_traits>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(SolveInterrupterTest, NotCopyableOrAssignable) {
  EXPECT_FALSE(std::is_copy_constructible<SolveInterrupter>::value);
  EXPECT_FALSE(std::is_copy_assignable<SolveInterrupter>::value);
}

TEST(SolveInterrupterTest, Untriggered) {
  // Note that we use a `const SolveInterrupter` here to test that the
  // "readonly" part of the interface is indeed `const`.
  const SolveInterrupter interrupter;
  EXPECT_FALSE(interrupter.IsInterrupted());

  std::vector<bool> calls;
  const SolveInterrupter::CallbackId id =
      interrupter.AddInterruptionCallback([&]() {
        // Test that we can call IsInterrupted() from the callback on top of
        // registering all calls.
        calls.push_back(interrupter.IsInterrupted());
      });

  // Test that AddInterruptionCallback() has not called the callback.
  EXPECT_THAT(calls, IsEmpty());

  interrupter.RemoveInterruptionCallback(id);
}

TEST(SolveInterrupterTest, Triggered) {
  SolveInterrupter interrupter;
  interrupter.Interrupt();

  EXPECT_TRUE(interrupter.IsInterrupted());

  std::vector<bool> calls;
  interrupter.AddInterruptionCallback([&]() {
    // Test that we can call IsInterrupted() from the callback on top of
    // registering all calls.
    calls.push_back(interrupter.IsInterrupted());
  });

  // Test that AddInterruptionCallback() has called the callback once and that
  // IsInterrupted() was true at that time.
  EXPECT_THAT(calls, ElementsAre(true));
}

TEST(SolveInterrupterTest, Triggering) {
  SolveInterrupter interrupter;

  std::vector<std::string> calls;
  interrupter.AddInterruptionCallback([&]() {
    calls.push_back(absl::StrCat("callback1: ", interrupter.IsInterrupted()));
  });
  interrupter.AddInterruptionCallback([&]() {
    calls.push_back(absl::StrCat("callback2: ", interrupter.IsInterrupted()));
  });

  EXPECT_THAT(calls, IsEmpty());

  interrupter.Interrupt();

  EXPECT_TRUE(interrupter.IsInterrupted());
  EXPECT_THAT(calls, ElementsAre("callback1: 1", "callback2: 1"));

  // Testing that calling Interrupt() a second time does not call the callbacks
  // again.
  calls.clear();
  interrupter.Interrupt();

  EXPECT_TRUE(interrupter.IsInterrupted());
  EXPECT_THAT(calls, IsEmpty());
}

TEST(SolveInterrupterTest, Unregistering) {
  SolveInterrupter interrupter;

  std::vector<bool> callback1_calls;
  const SolveInterrupter::CallbackId callback1 =
      interrupter.AddInterruptionCallback([&]() {
        // Test that we can call IsInterrupted() from the callback on top of
        // registering all calls.
        callback1_calls.push_back(interrupter.IsInterrupted());
      });
  std::vector<bool> callback2_calls;
  interrupter.AddInterruptionCallback([&]() {
    // Test that we can call IsInterrupted() from the callback on top of
    // registering all calls.
    callback2_calls.push_back(interrupter.IsInterrupted());
  });

  EXPECT_THAT(callback1_calls, IsEmpty());
  EXPECT_THAT(callback2_calls, IsEmpty());

  interrupter.RemoveInterruptionCallback(callback1);
  interrupter.Interrupt();

  EXPECT_TRUE(interrupter.IsInterrupted());
  EXPECT_THAT(callback1_calls, IsEmpty());
  EXPECT_THAT(callback2_calls, ElementsAre(true));
}

TEST(SolveInterrupterDeathTest, UnregisteringTwice) {
  SolveInterrupter interrupter;
  const SolveInterrupter::CallbackId callback =
      interrupter.AddInterruptionCallback([]() {});

  interrupter.RemoveInterruptionCallback(callback);
  EXPECT_DEATH(interrupter.RemoveInterruptionCallback(callback),
               "unregistered callback id");
}

TEST(ScopedSolveInterrupterCallbackTest, CallbackNotCalled) {
  // Note that we use a `const` variable here to test that we can use
  // ScopedSolveInterrupterCallbackTest with a pointer to a `const
  // SolveInterrupter`.
  const SolveInterrupter interrupter;
  int calls = 0;
  {
    const ScopedSolveInterrupterCallback scoped_callback(&interrupter,
                                                         [&]() { ++calls; });
  }
  EXPECT_EQ(calls, 0);
}

TEST(ScopedSolveInterrupterCallbackTest, CallbackCalled) {
  SolveInterrupter interrupter;
  int calls = 0;
  {
    const ScopedSolveInterrupterCallback scoped_callback(&interrupter,
                                                         [&]() { ++calls; });
    interrupter.Interrupt();
  }
  EXPECT_EQ(calls, 1);
}

TEST(ScopedSolveInterrupterCallbackTest, CallbackRemoved) {
  SolveInterrupter interrupter;
  int calls = 0;
  {
    const ScopedSolveInterrupterCallback scoped_callback(&interrupter,
                                                         [&]() { ++calls; });
  }
  interrupter.Interrupt();
  EXPECT_EQ(calls, 0);
}

TEST(ScopedSolveInterrupterCallbackTest, CallbackRemovedBeforeDestruction) {
  SolveInterrupter interrupter;
  int calls = 0;
  {
    ScopedSolveInterrupterCallback scoped_callback(&interrupter,
                                                   [&]() { ++calls; });
    scoped_callback.RemoveCallbackIfNecessary();
    interrupter.Interrupt();
  }
  EXPECT_EQ(calls, 0);
}

TEST(ScopedSolveInterrupterCallbackTest, NullInterrupter) {
  int calls = 0;
  {
    const ScopedSolveInterrupterCallback scoped_callback(nullptr,
                                                         [&]() { ++calls; });
  }
  EXPECT_EQ(calls, 0);
}

TEST(ScopedSolveInterrupterCallbackTest, RemoveCallbackNullInterrupter) {
  int calls = 0;
  {
    ScopedSolveInterrupterCallback scoped_callback(nullptr, [&]() { ++calls; });
    scoped_callback.RemoveCallbackIfNecessary();
  }
  EXPECT_EQ(calls, 0);
}

}  // namespace
}  // namespace operations_research
