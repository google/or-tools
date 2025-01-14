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

#include "ortools/math_opt/solvers/message_callback_data.h"

#include <algorithm>
#include <functional>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <tuple>
#include <type_traits>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::MockFunction;

TEST(MessageCallbackDataTest, ParseNotCalled) {
  MessageCallbackData message_callback_data;
  EXPECT_THAT(message_callback_data.Flush(), IsEmpty());
}

TEST(MessageCallbackDataTest, ParseCalledOnceWithEmptyString) {
  MessageCallbackData message_callback_data;
  EXPECT_THAT(message_callback_data.Parse(""), IsEmpty());
  EXPECT_THAT(message_callback_data.Flush(), IsEmpty());
}

TEST(MessageCallbackDataTest, MultipleUnfinishedMessages) {
  MessageCallbackData message_callback_data;
  EXPECT_THAT(message_callback_data.Parse("First."), IsEmpty());
  EXPECT_THAT(message_callback_data.Parse(" Second."), IsEmpty());
  EXPECT_THAT(message_callback_data.Flush(), ElementsAre("First. Second."));

  // Test that the flush actually reset the object.
  EXPECT_THAT(message_callback_data.Flush(), IsEmpty());
}

TEST(MessageCallbackDataTest, UnfinishedFollowedByUnfinished) {
  MessageCallbackData message_callback_data;
  EXPECT_THAT(message_callback_data.Parse("First.\nSecond.\nTh"),
              ElementsAre("First.", "Second."));
  EXPECT_THAT(message_callback_data.Parse("i"), IsEmpty());
  EXPECT_THAT(message_callback_data.Parse("rd."), IsEmpty());
  EXPECT_THAT(message_callback_data.Parse("\nFourth.\nFifth."),
              ElementsAre("Third.", "Fourth."));
  EXPECT_THAT(message_callback_data.Flush(), ElementsAre("Fifth."));

  // Test that the flush actually reset the object.
  EXPECT_THAT(message_callback_data.Flush(), IsEmpty());
}

TEST(BufferedMessageCallbackTest, CallbackInvokedWithNewLines) {
  MockFunction<void(const std::vector<std::string>&)> user_cb;
  EXPECT_CALL(user_cb, Call(ElementsAre("line one", "line two")));
  BufferedMessageCallback buffered_cb(user_cb.AsStdFunction());
  buffered_cb.OnMessage("line one\nline two\n");

  // Final state: buffer should be empty.
  Mock::VerifyAndClearExpectations(&user_cb);
  EXPECT_CALL(user_cb, Call(testing::_)).Times(0);
  buffered_cb.Flush();
}

TEST(BufferedMessageCallbackTest, CallbackNotInvokedWithoutNewLines) {
  MockFunction<void(const std::vector<std::string>&)> user_cb;
  EXPECT_CALL(user_cb, Call(testing::_)).Times(0);
  BufferedMessageCallback buffered_cb(user_cb.AsStdFunction());
  buffered_cb.OnMessage("line one. line two.");

  // Final state: buffer have "line one. line two.".
  Mock::VerifyAndClearExpectations(&user_cb);
  EXPECT_CALL(user_cb, Call(ElementsAre("line one. line two.")));
  buffered_cb.Flush();
}

TEST(BufferedMessageCallbackTest, CallbackBuffersIncompleteLines) {
  MockFunction<void(const std::vector<std::string>&)> user_cb;
  EXPECT_CALL(user_cb, Call(ElementsAre("part one. part two.")));
  BufferedMessageCallback buffered_cb(user_cb.AsStdFunction());
  buffered_cb.OnMessage("part one.");
  buffered_cb.OnMessage(" part two.\n");

  // Final state: buffer should be empty.
  Mock::VerifyAndClearExpectations(&user_cb);
  EXPECT_CALL(user_cb, Call(testing::_)).Times(0);
  buffered_cb.Flush();
}

TEST(BufferedMessageCallbackTest, FlushTwiceNoEffect) {
  MockFunction<void(const std::vector<std::string>&)> user_cb;
  EXPECT_CALL(user_cb, Call(ElementsAre("part one.")));
  BufferedMessageCallback buffered_cb(user_cb.AsStdFunction());
  buffered_cb.OnMessage("part one.");
  buffered_cb.Flush();

  // Final state: buffer should be empty.
  Mock::VerifyAndClearExpectations(&user_cb);
  EXPECT_CALL(user_cb, Call(testing::_)).Times(0);
  buffered_cb.Flush();
}

TEST(BufferedMessageCallbackTest, NullFunctionNoEffect) {
  BufferedMessageCallback buffered_cb(nullptr);
  ASSERT_FALSE(buffered_cb.has_user_message_callback());
  buffered_cb.OnMessage("abc\n123\n");
  buffered_cb.Flush();
}

TEST(BufferedMessageCallbackTest, NonNullFunctionHasCallback) {
  BufferedMessageCallback buffered_cb(
      [](absl::Span<const std::string> messages) {});
  EXPECT_TRUE(buffered_cb.has_user_message_callback());
}

TEST(BufferedMessageCallbackTest, AllowsConcurrentUserCallbacks) {
  absl::BlockingCounter workers_ready(2);
  absl::Notification main_thread_ready;
  auto cb = [&](absl::Span<const std::string> messages) {
    workers_ready.DecrementCount();
    main_thread_ready.WaitForNotification();
  };
  BufferedMessageCallback buffered_cb(cb);
  std::thread worker1([&buffered_cb]() { buffered_cb.OnMessage("test1\n"); });
  const absl::Cleanup worker1_join = [&]() { worker1.join(); };
  std::thread worker2([&buffered_cb]() { buffered_cb.OnMessage("test2\n"); });
  const absl::Cleanup worker2_join = [&]() { worker2.join(); };
  workers_ready.Wait();
  main_thread_ready.Notify();
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
