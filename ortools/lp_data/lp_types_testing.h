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

#ifndef ORTOOLS_LP_DATA_LP_TYPES_TESTING_H_
#define ORTOOLS_LP_DATA_LP_TYPES_TESTING_H_

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research::glop {

// Matches an OK AbnormalityStatus (no cause).
::testing::Matcher<AbnormalityStatus> AbnormalityStatusIsOK();

// Matches an failing AbnormalityStatus.
::testing::Matcher<AbnormalityStatus> AbnormalityStatusIs(
    AbnormalityCause cause);

// Matches a given Alternative of SolveStatus with the provided matcher.
//
// I.e. is matches when:
// * SolveStatus::value is of type Alternative,
// * and the alternative value matches m.
//
// See SolveStatusWithCause() to match alternative with a cause.
template <typename Alternative>
::testing::Matcher<SolveStatus> SolveStatusWith(
    ::testing::Matcher<const Alternative&> m) {
  return ::testing::Field("value", &SolveStatus::value,
                          ::testing::VariantWith<Alternative>(m));
}

// Matches a given Alternative of SolveStatus with a `cause` field that matches
// the input m.
//
// I.e. is matches when:
// * SolveStatus::value is of type Alternative,
// * and Alternative::cause matches m.
template <typename Alternative>
::testing::Matcher<SolveStatus> SolveStatusWithCause(
    ::testing::Matcher<decltype(Alternative::cause)> m) {
  return SolveStatusWith<Alternative>(
      ::testing::Field("cause", &Alternative::cause, m));
}

// Matches a given SolveStatus with the provided matcher for its
// SolveStatus::problem_status().
//
// See SolveStatusWith() and SolveStatusWithCause() for matching a specific
// alternative and/or its data.
::testing::Matcher<SolveStatus> SolveStatusProblemStatusIs(
    ::testing::Matcher<const ProblemStatus&> m);

}  // namespace operations_research::glop

#endif  // ORTOOLS_LP_DATA_LP_TYPES_TESTING_H_
