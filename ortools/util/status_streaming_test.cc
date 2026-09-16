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

#include "ortools/util/status_streaming.h"

#include <cstdint>

#include "absl/log/scoped_mock_log.h"
#include "absl/status/status.h"
#include "absl/types/source_location.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/util/status.pb.h"

namespace operations_research {
namespace {

using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::status::StatusIs;

TEST(StreamStatusTest, Ok) {
  EXPECT_THAT(StreamStatus(absl::OkStatus()), EqualsProto(StatusProto{}));
}

TEST(StreamStatusTest, Failure) {
  StatusProto expected;
  expected.set_code(static_cast<int32_t>(absl::StatusCode::kUnimplemented));
  expected.set_message("some message");
  EXPECT_THAT(StreamStatus(absl::UnimplementedError("some message")),
              EqualsProto(expected));
}

TEST(StreamStatusTest, FailureWithPayload) {
  absl::Status status = absl::UnimplementedError("some message");
  status.SetPayload("some_payload_url", {});

  StatusProto expected;
  expected.set_code(static_cast<int32_t>(absl::StatusCode::kUnimplemented));
  expected.set_message("some message");

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log).Times(AnyNumber());
  EXPECT_CALL(log, Log(absl::LogSeverity::kWarning, ::testing::_,
                       HasSubstr("some_payload_url")))
      .Times(1);

  log.StartCapturingLogs();
  EXPECT_THAT(StreamStatus(status), EqualsProto(expected));
}

TEST(UnstreamStatusTest, Ok) { EXPECT_OK(UnstreamStatus(StatusProto{})); }

TEST(UnstreamStatusTest, FailureWithDefaultLocation) {
  StatusProto proto;
  proto.set_code(static_cast<int32_t>(absl::StatusCode::kUnimplemented));
  proto.set_message("some message");

  const absl::Status unstreamed_status = UnstreamStatus(proto);
  EXPECT_THAT(unstreamed_status,
              StatusIs(absl::StatusCode::kUnimplemented, "some message"));
}

TEST(UnstreamStatusTest, FailureWithCustomLocation) {
  StatusProto proto;
  proto.set_code(static_cast<int32_t>(absl::StatusCode::kUnimplemented));
  proto.set_message("some message");

  const absl::SourceLocation location = absl::SourceLocation::current();
  const absl::Status unstreamed_status = UnstreamStatus(proto, location);
  EXPECT_THAT(unstreamed_status,
              StatusIs(absl::StatusCode::kUnimplemented, "some message"));
}

}  // namespace
}  // namespace operations_research
