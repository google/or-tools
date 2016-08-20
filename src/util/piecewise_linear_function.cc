// Copyright 2010-2014 Google
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

#include "util/piecewise_linear_function.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stringprintf.h"

namespace operations_research {
namespace {
// If the x value is in the function's domain, it returns the index of the
// segment it belongs to. The segments are closed to the left and open to
// the right, hence if x is a common endpoint of two segments, it returns
// the index of the right segment. If the x value is not in the function's
// domain, it returns the index of the previous segment or kNotFound if x
// is before the first segment's start.
int FindSegmentIndex(const std::vector<PiecewiseSegment>& segments, int64 x) {
  if (segments.empty() || segments.front().start_x() > x) {
    return PiecewiseLinearFunction::kNotFound;
  }

  // Returns an iterator pointing to the first segment whose the x coordinate
  // of its start point which compares greater than the x value.
  std::vector<PiecewiseSegment>::const_iterator position = std::upper_bound(
      segments.begin(), segments.end(), x, PiecewiseSegment::FindComparator);
  if (position == segments.end()) {
    return segments.size() - 1;
  }
  position -= position->start_x() > x ? 1 : 0;

  return position - segments.begin();
}

inline bool IsAtBounds(int64 value) {
  return value == kint64min || value == kint64max;
}

inline bool PointInsideRange(int64 point, int64 range_start, int64 range_end) {
  return range_start <= point && range_end >= point;
}

// Checks whether two segments form a convex pair, i.e. they are continuous and
// the slope of the right is bigger than the slope of the left.
inline bool FormConvexPair(const PiecewiseSegment& left,
                           const PiecewiseSegment& right) {
  return right.slope() >= left.slope() && right.start_x() == left.end_x() &&
         right.start_y() == left.end_y();
}

uint64 UnsignedCapAdd(uint64 left, uint64 right) {
  return left > kuint64max - right ? kuint64max : left + right;
}

uint64 UnsignedCapProd(uint64 left, uint64 right) {
  if (right == 0) return 0;
  if (left > kuint64max / right) return kuint64max;
  return left * right;
}
}  // namespace

PiecewiseSegment::PiecewiseSegment(int64 point_x, int64 point_y, int64 slope,
                                   int64 other_point_x)
    : slope_(slope), reference_x_(point_x), reference_y_(point_y) {
  start_x_ = std::min(point_x, other_point_x);
  end_x_ = std::max(point_x, other_point_x);
  intersection_y_ =
      reference_x_ < 0 ? SafeValuePostReference(0) : SafeValuePreReference(0);
}

int64 PiecewiseSegment::Value(int64 x) const {
  CHECK_GE(x, start_x_);
  CHECK_LE(x, end_x_);

  const int64 span_x = CapSub(x, reference_x_);

  if (span_x == kint64max) {
    return SafeValuePostReference(x);
  }
  if (span_x == kint64min) {
    return SafeValuePreReference(x);
  }

  const int64 span_y = CapProd(slope_, span_x);
  if (IsAtBounds(span_y)) {
    if (span_x >= 0) {
      return SafeValuePostReference(x);
    } else {
      return SafeValuePreReference(x);
    }
  }

  const int64 value = CapAdd(reference_y_, span_y);
  if (IsAtBounds(value)) {
    if (span_x >= 0) {
      return SafeValuePostReference(x);
    } else {
      return SafeValuePreReference(x);
    }
  } else {
    return value;
  }
}

int64 PiecewiseSegment::SafeValuePostReference(int64 x) const {
  DCHECK_GE(x, reference_x_);
  const uint64 span_x = x - reference_x_;
  if (span_x == 0) {
    return reference_y_;
  }
  if (slope_ == 0) {
    // Zero slope segment.
    return reference_y_;
  } else if (slope_ > 0) {
    // Positive slope segment.
    const uint64 span_y = UnsignedCapProd(span_x, slope_);
    if (reference_y_ == 0) {
      return span_y > kint64max ? kint64max : span_y;
    } else if (reference_y_ > 0) {
      const uint64 unsigned_sum = UnsignedCapAdd(reference_y_, span_y);
      return unsigned_sum > kint64max ? kint64max
                                      : static_cast<int64>(unsigned_sum);
    } else {
      const uint64 opp_reference_y = static_cast<uint64>(-reference_y_);
      if (span_y >= opp_reference_y) {
        return span_y - opp_reference_y > kint64max
                   ? kint64max
                   : static_cast<int64>(span_y - opp_reference_y);
      } else {
        return opp_reference_y - span_y > static_cast<uint64>(kint64max) + 1
                   ? kint64min
                   : -static_cast<int64>(opp_reference_y - span_y);
      }
    }
  } else {
    // Negative slope segment.
    const uint64 span_y = UnsignedCapProd(span_x, -slope_);
    if (reference_y_ == 0) {
      return span_y > kint64max ? kint64min : -static_cast<int64>(span_y);
    } else if (reference_y_ < 0) {
      const uint64 opp_reference_y = static_cast<uint64>(-reference_y_);
      const uint64 opp_unsigned_sum = UnsignedCapAdd(opp_reference_y, span_y);
      return opp_unsigned_sum > kint64max
                 ? kint64min
                 : -static_cast<int64>(opp_unsigned_sum);
    } else {
      if (reference_y_ >= span_y) {
        return reference_y_ - span_y > kint64max
                   ? kint64max
                   : static_cast<int64>(reference_y_ - span_y);
      } else {
        return span_y - reference_y_ > static_cast<uint64>(kint64max) + 1
                   ? kint64min
                   : -static_cast<int64>(span_y - reference_y_);
      }
    }
  }
}

int64 PiecewiseSegment::SafeValuePreReference(int64 x) const {
  DCHECK_LE(x, reference_x_);
  const uint64 span_x = reference_x_ - x;
  if (slope_ == 0) {
    // Zero slope segment.
    return reference_y_;
  } else if (slope_ > 0) {
    // Positive slope segment.
    const uint64 span_y = UnsignedCapProd(span_x, slope_);
    if (reference_y_ == 0) {
      return span_y > kint64max ? kint64min : -static_cast<int64>(span_y);
    } else if (reference_y_ > 0) {
      if (reference_y_ >= span_y) {
        return reference_y_ - span_y > kint64max
                   ? kint64max
                   : static_cast<int64>(reference_y_ - span_y);
      } else {
        return span_y - reference_y_ > static_cast<uint64>(kint64max) + 1
                   ? kint64min
                   : -static_cast<int64>(span_y - reference_y_);
      }
    } else {
      const uint64 opp_reference_y = static_cast<uint64>(-reference_y_);
      const uint64 opp_unsigned_sum = UnsignedCapAdd(opp_reference_y, span_y);
      return opp_unsigned_sum > kint64max
                 ? kint64min
                 : -static_cast<int64>(opp_unsigned_sum);
    }
  } else {
    // Negative slope segment.
    const uint64 span_y = UnsignedCapProd(span_x, -slope_);
    if (reference_y_ == 0) {
      return span_y > kint64max ? kint64max : span_y;
    } else if (reference_y_ < 0) {
      const uint64 opp_reference_y = static_cast<uint64>(-reference_y_);
      if (span_y >= opp_reference_y) {
        return span_y - opp_reference_y > kint64max
                   ? kint64max
                   : static_cast<int64>(span_y - opp_reference_y);
      } else {
        return opp_reference_y - span_y > static_cast<uint64>(kint64max) + 1
                   ? kint64min
                   : -static_cast<int64>(opp_reference_y - span_y);
      }
    } else {
      const uint64 unsigned_sum = UnsignedCapAdd(reference_y_, span_y);
      return unsigned_sum > kint64max ? kint64max
                                      : static_cast<int64>(unsigned_sum);
    }
  }
}

bool PiecewiseSegment::SortComparator(const PiecewiseSegment& segment1,
                                      const PiecewiseSegment& segment2) {
  return segment1.start_x_ < segment2.start_x_;
}

bool PiecewiseSegment::FindComparator(int64 point,
                                      const PiecewiseSegment& segment) {
  return point == kint64min || point < segment.start_x();
}

void PiecewiseSegment::ExpandEnd(int64 end_x) {
  end_x_ = std::max(end_x_, end_x);
}

void PiecewiseSegment::AddConstantToX(int64 constant) {
  if (IsAtBounds(CapAdd(reference_x_, constant))) {
    LOG(ERROR) << "Segment Overflow: " << DebugString();
    return;
  }
  start_x_ = CapAdd(start_x_, constant);
  end_x_ = CapAdd(end_x_, constant);
  reference_x_ = CapAdd(reference_x_, constant);
}

void PiecewiseSegment::AddConstantToY(int64 constant) {
  if (IsAtBounds(CapAdd(reference_y_, constant))) {
    LOG(ERROR) << "Segment Overflow: " << DebugString();
    return;
  }
  reference_y_ = CapAdd(reference_y_, constant);
}

std::string PiecewiseSegment::DebugString() const {
  std::string result = StringPrintf("PiecewiseSegment(<start: (%" GG_LL_FORMAT
                               "d, %" GG_LL_FORMAT
                               "d), "
                               "end: (%" GG_LL_FORMAT "d, %" GG_LL_FORMAT
                               "d), "
                               "reference: (%" GG_LL_FORMAT "d, %" GG_LL_FORMAT
                               "d), "
                               "slope = %" GG_LL_FORMAT "d>)",
                               start_x_, Value(start_x_), end_x_, Value(end_x_),
                               reference_x_, reference_y_, slope_);
  return result;
}

const int PiecewiseLinearFunction::kNotFound = -1;

PiecewiseLinearFunction::PiecewiseLinearFunction(
    std::vector<PiecewiseSegment>* segments) {
  CHECK_NOTNULL(segments);

  // Sort the segments in ascending order of start.
  std::sort(segments->begin(), segments->end(),
            PiecewiseSegment::SortComparator);
  // Check for overlapping segments.
  for (int i = 0; i < segments->size() - 1; ++i) {
    if (segments->at(i).end_x() > segments->at(i + 1).start_x()) {
      LOG(FATAL) << "Overlapping segments: " << segments->at(i).DebugString()
                 << " & " << segments->at(i + 1).DebugString();
    }
  }
  // Construct the piecewise linear function.
  for (int i = 0; i < segments->size(); ++i) {
    InsertSegment(segments->at(i));
  }
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
    std::vector<int64> points_x, std::vector<int64> points_y, std::vector<int64> slopes,
    std::vector<int64> other_points_x) {
  CHECK_EQ(points_x.size(), points_y.size());
  CHECK_EQ(points_x.size(), other_points_x.size());
  CHECK_EQ(points_x.size(), slopes.size());
  CHECK_GT(points_x.size(), 0);

  std::vector<PiecewiseSegment> segments;
  for (int i = 0; i < points_x.size(); ++i) {
    segments.push_back(PiecewiseSegment(points_x[i], points_y[i], slopes[i],
                                        other_points_x[i]));
  }

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreateStepFunction(
    std::vector<int64> points_x, std::vector<int64> points_y,
    std::vector<int64> other_points_x) {
  CHECK_EQ(points_x.size(), points_y.size());
  CHECK_EQ(points_x.size(), other_points_x.size());
  CHECK_GT(points_x.size(), 0);

  std::vector<PiecewiseSegment> segments;
  for (int i = 0; i < points_x.size(); ++i) {
    segments.push_back(
        PiecewiseSegment(points_x[i], points_y[i], 0, other_points_x[i]));
  }

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreateFullDomainFunction(
    int64 initial_level, std::vector<int64> points_x, std::vector<int64> slopes) {
  CHECK_EQ(points_x.size(), slopes.size() - 1);
  CHECK_GT(points_x.size(), 0);

  int64 level = initial_level;
  std::vector<PiecewiseSegment> segments;
  PiecewiseSegment segment =
      PiecewiseSegment(points_x[0], level, slopes[0], kint64min);
  segments.push_back(segment);
  level = segment.Value(points_x[0]);
  for (int i = 1; i < points_x.size(); ++i) {
    PiecewiseSegment segment =
        PiecewiseSegment(points_x[i - 1], level, slopes[i], points_x[i]);
    segments.push_back(segment);
    level = segment.Value(points_x[i]);
  }
  segments.push_back(
      PiecewiseSegment(points_x.back(), level, slopes.back(), kint64max));

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreateOneSegmentFunction(
    int64 point_x, int64 point_y, int64 slope, int64 other_point_x) {
  std::vector<PiecewiseSegment> segments;
  segments.push_back(PiecewiseSegment(point_x, point_y, slope, other_point_x));

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreateRightRayFunction(
    int64 point_x, int64 point_y, int64 slope) {
  std::vector<PiecewiseSegment> segments;
  segments.push_back(PiecewiseSegment(point_x, point_y, slope, kint64max));

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreateLeftRayFunction(
    int64 point_x, int64 point_y, int64 slope) {
  std::vector<PiecewiseSegment> segments;
  segments.push_back(PiecewiseSegment(point_x, point_y, slope, kint64min));

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreateFixedChargeFunction(
    int64 slope, int64 value) {
  std::vector<PiecewiseSegment> segments;
  CHECK_GE(slope, 0);
  CHECK_GE(value, 0);
  segments.push_back(PiecewiseSegment(0, 0, 0, kint64min));
  segments.push_back(PiecewiseSegment(0, value, slope, kint64max));

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction* PiecewiseLinearFunction::CreateEarlyTardyFunction(
    int64 reference, int64 earliness_slope, int64 tardiness_slope) {
  std::vector<PiecewiseSegment> segments;
  CHECK_GE(earliness_slope, 0);
  CHECK_GE(tardiness_slope, 0);
  segments.push_back(
      PiecewiseSegment(reference, 0, -earliness_slope, kint64min));
  segments.push_back(
      PiecewiseSegment(reference, 0, tardiness_slope, kint64max));

  return new PiecewiseLinearFunction(&segments);
}

PiecewiseLinearFunction*
PiecewiseLinearFunction::CreateEarlyTardyFunctionWithSlack(
    int64 early_slack, int64 late_slack, int64 earliness_slope,
    int64 tardiness_slope) {
  std::vector<PiecewiseSegment> segments;
  CHECK_GE(earliness_slope, 0);
  CHECK_GE(tardiness_slope, 0);
  segments.push_back(
      PiecewiseSegment(early_slack, 0, -earliness_slope, kint64min));
  segments.push_back(PiecewiseSegment(early_slack, 0, 0, late_slack));
  segments.push_back(
      PiecewiseSegment(late_slack, 0, tardiness_slope, kint64max));

  return new PiecewiseLinearFunction(&segments);
}

bool PiecewiseLinearFunction::InDomain(int64 x) const {
  int index = FindSegmentIndex(segments_, x);
  if (index == kNotFound) {
    return false;
  }
  if (segments_[index].end_x() < x) {
    return false;
  }
  return true;
}

bool PiecewiseLinearFunction::IsConvex() const {
  for (int i = 1; i < segments_.size(); ++i) {
    if (!FormConvexPair(segments_[i - 1], segments_[i])) {
      return false;
    }
  }
  return true;
}

int64 PiecewiseLinearFunction::Value(int64 x) const {
  if (!InDomain(x)) {
    // TODO(user): Allow the user to specify the
    // undefined value and use kint64max as the default.
    return kint64max;
  }
  const int index = FindSegmentIndex(segments_, x);
  return segments_[index].Value(x);
}

int64 PiecewiseLinearFunction::GetMaximum(int64 range_start,
                                          int64 range_end) const {
  const int start_segment = FindSegmentIndex(segments_, range_start);
  const int end_segment = FindSegmentIndex(segments_, range_end);
  CHECK_GE(end_segment, start_segment);

  if (start_segment == end_segment) {
    if (start_segment < 0) {
      // Given range before function's domain start.
      return kint64max;
    }
    if (segments_[start_segment].end_x() < range_start) {
      // Given range in a hole of the function's domain.
      return kint64max;
    }
  }

  int64 range_maximum = kint64min;
  if (InDomain(range_start)) {
    range_maximum = std::max(Value(range_start), range_maximum);
  }
  if (InDomain(range_end)) {
    range_maximum = std::max(Value(range_end), range_maximum);
  }

  for (int i = std::max(0, start_segment); i <= end_segment; ++i) {
    if (PointInsideRange(segments_[i].start_x(), range_start, range_end)) {
      range_maximum = std::max(range_maximum, segments_[i].start_y());
    }
    if (PointInsideRange(segments_[i].end_x(), range_start, range_end)) {
      range_maximum = std::max(range_maximum, segments_[i].end_y());
    }
  }
  return range_maximum;
}

int64 PiecewiseLinearFunction::GetMinimum(int64 range_start,
                                          int64 range_end) const {
  const int start_segment = FindSegmentIndex(segments_, range_start);
  const int end_segment = FindSegmentIndex(segments_, range_end);
  CHECK_GE(end_segment, start_segment);

  if (start_segment == end_segment) {
    if (start_segment < 0) {
      // Given range before function's domain start.
      return kint64max;
    }
    if (segments_[start_segment].end_x() < range_start) {
      // Given range in a hole of the function's domain.
      return kint64max;
    }
  }

  int64 range_minimum = kint64max;
  if (InDomain(range_start)) {
    range_minimum = std::min(Value(range_start), range_minimum);
  }
  if (InDomain(range_end)) {
    range_minimum = std::min(Value(range_end), range_minimum);
  }

  for (int i = std::max(0, start_segment); i <= end_segment; ++i) {
    if (PointInsideRange(segments_[i].start_x(), range_start, range_end)) {
      range_minimum = std::min(range_minimum, segments_[i].start_y());
    }
    if (PointInsideRange(segments_[i].end_x(), range_start, range_end)) {
      range_minimum = std::min(range_minimum, segments_[i].end_y());
    }
  }
  return range_minimum;
}

int64 PiecewiseLinearFunction::GetMaximum() const {
  return GetMaximum(segments_.front().start_x(), segments_.back().end_x());
}

int64 PiecewiseLinearFunction::GetMinimum() const {
  return GetMinimum(segments_.front().start_x(), segments_.back().end_x());
}

void PiecewiseLinearFunction::AddConstantToX(int64 constant) {
  for (int i = 0; i < segments_.size(); ++i) {
    segments_[i].AddConstantToX(constant);
  }
}

void PiecewiseLinearFunction::AddConstantToY(int64 constant) {
  for (int i = 0; i < segments_.size(); ++i) {
    segments_[i].AddConstantToY(constant);
  }
}

void PiecewiseLinearFunction::Add(const PiecewiseLinearFunction& other) {
  std::unique_ptr<ResultCallback2<int64, int64, int64> > add(
      NewPermanentCallback(&CapAdd));
  Operation(other, add.get());
}

void PiecewiseLinearFunction::Subtract(const PiecewiseLinearFunction& other) {
  std::unique_ptr<ResultCallback2<int64, int64, int64> > subtract(
      NewPermanentCallback(&CapSub));
  Operation(other, subtract.get());
}

void PiecewiseLinearFunction::DecomposeToConvexFunctions(
    std::vector<PiecewiseLinearFunction*>* convex_functions) {
  CHECK_NOTNULL(convex_functions);
  CHECK_GE(segments_.size(), 1);

  if (IsConvex()) {
    std::vector<PiecewiseSegment> convex_segments(segments_);
    convex_functions->push_back(new PiecewiseLinearFunction(&convex_segments));
    return;
  }

  CHECK_NOTNULL(convex_functions);
  CHECK_GE(segments_.size(), 1);

  std::vector<PiecewiseSegment> convex_segments;

  for (PiecewiseSegment segment : segments_) {
    if (convex_segments.empty()) {
      convex_segments.push_back(segment);
      continue;
    }

    const PiecewiseSegment& last = convex_segments.back();
    if (FormConvexPair(last, segment)) {
      // The segment belongs to the convex sub-function formulated up to now.
      convex_segments.push_back(segment);
    } else {
      PiecewiseLinearFunction* function(
          new PiecewiseLinearFunction(&convex_segments));
      convex_functions->push_back(function);
      convex_segments.clear();
      convex_segments.push_back(segment);
    }
  }

  if (convex_segments.size() > 0) {
    PiecewiseLinearFunction* function(
        new PiecewiseLinearFunction(&convex_segments));
    convex_functions->push_back(function);
  }
}

std::string PiecewiseLinearFunction::DebugString() const {
  std::string result = "PiecewiseLinearFunction(";
  for (int i = 0; i < segments_.size(); ++i) {
    result.append(segments_[i].DebugString());
    result.append(" ");
  }
  return result;
}

void PiecewiseLinearFunction::InsertSegment(const PiecewiseSegment& segment) {
  // No intersetion.
  if (segments_.empty() || segments_.back().end_x() < segment.start_x()) {
    segments_.push_back(segment);
    return;
  }

  // Common endpoint.
  if (segments_.back().end_x() == segment.start_x()) {
    if (segments_.back().end_y() == segment.start_y() &&
        segments_.back().slope() == segment.slope()) {
      segments_.back().ExpandEnd(segment.end_x());
      return;
    }
    segments_.push_back(segment);
  }
}

void PiecewiseLinearFunction::Operation(
    const PiecewiseLinearFunction& other,
    ResultCallback2<int64, int64, int64>* operation) {
  std::vector<PiecewiseSegment> own_segments;
  const std::vector<PiecewiseSegment>& other_segments = other.segments();
  own_segments.swap(segments_);

  std::set<int64> start_x_points;
  for (int i = 0; i < own_segments.size(); ++i) {
    start_x_points.insert(own_segments[i].start_x());
  }
  for (int i = 0; i < other_segments.size(); ++i) {
    start_x_points.insert(other_segments[i].start_x());
  }

  for (int64 start_x : start_x_points) {
    const int own_index = FindSegmentIndex(own_segments, start_x);
    const int other_index = FindSegmentIndex(other_segments, start_x);
    if (own_index >= 0 && other_index >= 0) {
      const PiecewiseSegment& own_segment = own_segments[own_index];
      const PiecewiseSegment& other_segment = other_segments[other_index];

      const int64 end_x = std::min(own_segment.end_x(), other_segment.end_x());
      const int64 start_y = operation->Run(own_segment.Value(start_x),
                                           other_segment.Value(start_x));
      const int64 end_y =
          operation->Run(own_segment.Value(end_x), other_segment.Value(end_x));
      const int64 slope =
          operation->Run(own_segment.slope(), other_segment.slope());

      int64 point_x, point_y, other_point_x;
      if (IsAtBounds(start_y)) {
        point_x = end_x;
        point_y = end_y;
        other_point_x = start_x;
      } else {
        point_x = start_x;
        point_y = start_y;
        other_point_x = end_x;
      }
      InsertSegment(PiecewiseSegment(point_x, point_y, slope, other_point_x));
    }
  }
}

}  // namespace operations_research
