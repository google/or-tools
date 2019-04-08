// Copyright 2010-2018 Google LLC
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

// This file implements piecewise linear functions over int64. It is built
// by inserting segments.
//
// This class maintains a minimal internal representation and checks for
// overflow.

#ifndef OR_TOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_H_
#define OR_TOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
// This structure stores one straight line. It contains the start point, the
// end point and the slope.
// It is defined for x values between start_x and end_x.
class PiecewiseSegment {
 public:
  PiecewiseSegment(int64 point_x, int64 point_y, int64 slope,
                   int64 other_point_x);

  // Returns the value of the segment at point x.
  int64 Value(int64 x) const;
  // Returns the start of the segment's domain.
  int64 start_x() const { return start_x_; }
  // Returns the end of the segment's domain.
  int64 end_x() const { return end_x_; }
  // Returns the value at the start of the segment's domain.
  int64 start_y() const { return Value(start_x_); }
  // Returns the value at the end of the segment's domain.
  int64 end_y() const { return Value(end_x_); }
  // Returns the segment's slope.
  int64 slope() const { return slope_; }
  // Returns the intersection of the segment's extension with the y axis.
  int64 intersection_y() const { return intersection_y_; }

  // Comparison method useful for sorting a sequence of segments.
  static bool SortComparator(const PiecewiseSegment& segment1,
                             const PiecewiseSegment& segment2);
  // Comparison method useful for finding in which segment a point belongs.
  static bool FindComparator(int64 point, const PiecewiseSegment& segment);

  // Expands segment to the specified endpoint, if it is further
  // than the current endpoint. The reference point of the segment
  // doesn't change for overflow reasons.
  void ExpandEnd(int64 end_x);
  // Adds 'constant' to the 'x' the segments.
  void AddConstantToX(int64 constant);
  // Adds 'constant' to the 'y' the segments.
  void AddConstantToY(int64 constant);

  std::string DebugString() const;

 private:
  // Computes the value of the segment at point x, taking care of possible
  // overflows when the value x follow the x coordinate of the segment's
  // reference point.
  int64 SafeValuePostReference(int64 x) const;
  // Computes the value of the segment at point x, taking care of possible
  // overflows when the value x follow the x coordinate of the segment's
  // reference point.
  int64 SafeValuePreReference(int64 x) const;

  // The x coordinate of the segment's left endpoint.
  int64 start_x_;
  // The x coordinate of the segment's right endpoint.
  int64 end_x_;
  // The segment's slope.
  int64 slope_;
  // The x coordinate of the segment's finite reference point.
  int64 reference_x_;
  // The y coordinate of the segment's finite reference point.
  int64 reference_y_;
  // The intersection of the segment's extension with the y axis.
  int64 intersection_y_;
};

// In mathematics, a piecewise linear function is a function composed
// of straight-line, non overlapping sections.
class PiecewiseLinearFunction {
 public:
  static const int kNotFound;

  // This API provides a factory for creating different families of Piecewise
  // Linear Functions based on specific properties of each family. The
  // PiecewiseLinearFunction is composed by a set of PiecwiseSegments and upon
  // creation is not modifiable but with the provided function operations.
  // The object returned by any of these builders in the factory is owned by
  // the client code.

  // Builds the most generic form of multiple-segment piecewise linear function
  // supporting domain holes. For a fixed index i the elements in points_x[i]
  // points_y[i], slopes[i], other_points_x[i] represent a segment.
  // The point (points_x[i], points_y[i]) represents one of the endpoints of
  // the segment and the other_points_x[i] represents the x coordinate of the
  // other endpoint which may precede, follow or coincide with points_x[i].
  // The segments represented by these vectors should not be overlapping.
  // Common endpoints are allowed.
  static PiecewiseLinearFunction* CreatePiecewiseLinearFunction(
      std::vector<int64> points_x, std::vector<int64> points_y,
      std::vector<int64> slopes, std::vector<int64> other_points_x);

  // Builds a multiple-segment step function with continuous or non continuous
  // domain. The arguments have the same semantics with the generic builder of
  // the piecewise linear function. In the step function all the slopes are 0.
  static PiecewiseLinearFunction* CreateStepFunction(
      std::vector<int64> points_x, std::vector<int64> points_y,
      std::vector<int64> other_points_x);

  // Builds a multiple-segment piecewise linear function with domain from
  // from kint64min to kint64max with n points and n+1 slopes. Each slope
  // stops at the point with the corresponding index apart from the last one
  // which stops at kint64max. The first slope stops at the first point at
  // the level specified.
  static PiecewiseLinearFunction* CreateFullDomainFunction(
      int64 initial_level, std::vector<int64> points_x,
      std::vector<int64> slopes);

  // Builds a function consisting of one segment.
  static PiecewiseLinearFunction* CreateOneSegmentFunction(int64 point_x,
                                                           int64 point_y,
                                                           int64 slope,
                                                           int64 other_point_x);

  // Builds a function consisting of one ray starting at the specified
  // x and y coordinates with the specified slope.
  static PiecewiseLinearFunction* CreateRightRayFunction(int64 point_x,
                                                         int64 point_y,
                                                         int64 slope);

  // Builds a function consisting of one ray starting at the specified
  // x and y coordinates with the specified slope.
  static PiecewiseLinearFunction* CreateLeftRayFunction(int64 point_x,
                                                        int64 point_y,
                                                        int64 slope);

  // Builds a two-segment fixed charge piecewise linear cost function. For
  // values less than zero, the cost is zero. For values greater than zero,
  // cost follows the line specified by the slope and the value given as
  // arguments. The slope and value are positive.
  static PiecewiseLinearFunction* CreateFixedChargeFunction(int64 slope,
                                                            int64 value);

  // Builds an earliness-tardiness two-segment piecewise linear cost function.
  // The reference specifies the point where the cost is zero. Before the
  // reference, the cost increases with the earliness slope and after the
  // referece, it increases with the tardiness slope. The absolute values of
  // the slopes are given.
  static PiecewiseLinearFunction* CreateEarlyTardyFunction(
      int64 reference, int64 earliness_slope, int64 tardiness_slope);

  // Builds an earliness-tardiness three-segment piecewise linear cost function
  // with a slack period around the due date. The early slack is the point
  // before which the cost increases with the ealiness slope specified. The
  // late slack is the point after which the cost increases with the late slope
  // specified. Between the early and the late slack point, the cost is zero.
  // The absolute values of the slopes are given.
  static PiecewiseLinearFunction* CreateEarlyTardyFunctionWithSlack(
      int64 early_slack, int64 late_slack, int64 earliness_slope,
      int64 tardiness_slope);

  // Returns if x is in the domain of the function.
  bool InDomain(int64 x) const;
  // Determines whether the piecewise linear function is convex or non-convex
  // and returns true when the function is convex.
  bool IsConvex() const;
  // Returns true if the piecewise linear function is non-decreasing.
  bool IsNonDecreasing() const;
  // Returns true if the piecewise linear function is non-increasing.
  bool IsNonIncreasing() const;
  // Returns the value of the piecewise linear function for x.
  int64 Value(int64 x) const;
  // Returns the maximum value of all the segments in the function.
  int64 GetMaximum() const;
  // Returns the minimum value of all the segments in the function.
  int64 GetMinimum() const;
  // Returns the maximum endpoint value of the segments in the specified
  // range. If the range is disjoint from the segments in the function, it
  // returns kint64max.
  int64 GetMaximum(int64 range_start, int64 range_end) const;
  // Returns the minimum endpoint value of the segments in the specified
  // range. If the range is disjoint from the segments in the function, it
  // returns kint64max.
  int64 GetMinimum(int64 range_start, int64 range_end) const;
  // Returns the smallest range within a given range containing all values
  // greater than a given value.
  std::pair<int64, int64> GetSmallestRangeGreaterThanValue(int64 range_start,
                                                           int64 range_end,
                                                           int64 value) const;
  // Returns the smallest range within a given range containing all values
  // less than a given value.
  std::pair<int64, int64> GetSmallestRangeLessThanValue(int64 range_start,
                                                        int64 range_end,
                                                        int64 value) const;
  // Returns the smallest range within a given range containing all values
  // greater than value_min and less than value_max.
  std::pair<int64, int64> GetSmallestRangeInValueRange(int64 range_start,
                                                       int64 range_end,
                                                       int64 value_min,
                                                       int64 value_max) const;

  // Adds 'constant' to the 'x' of all segments. If the argument is positive,
  // the translation is to the right and when it's negative, to the left. The
  // overflows and the underflows are sticky.
  void AddConstantToX(int64 constant);
  // Adds 'constant' to the 'y' of all segments. If the argument is positive,
  // the translation is up and when it's negative, down. The overflows and the
  // underflows are sticky.
  void AddConstantToY(int64 constant);
  // Adds the function to the existing one. The domain of the resulting
  // function is the intersection of the two domains. The overflows and
  // the underflows are sticky.
  void Add(const PiecewiseLinearFunction& other);
  // Subtracts the function to the existing one. The domain of the
  // resulting function is the intersection of the two domains. The
  // overflows and the underflows are sticky.
  void Subtract(const PiecewiseLinearFunction& other);
  // Decomposes the piecewise linear function in a set of convex piecewise
  // linear functions. The objects in the vector are owned by the client code.
  std::vector<PiecewiseLinearFunction*> DecomposeToConvexFunctions() const;

  const std::vector<PiecewiseSegment>& segments() const { return segments_; }

  std::string DebugString() const;

 private:
  // Takes the sequence of segments, sorts them on increasing start and inserts
  // them in the piecewise linear function.
  explicit PiecewiseLinearFunction(std::vector<PiecewiseSegment> segments);
  // Inserts a segment in the function.
  void InsertSegment(const PiecewiseSegment& segment);
  // Operation between two functions. In any operation between two functions the
  // final domain is the intersection between the two domains.
  void Operation(const PiecewiseLinearFunction& other,
                 const std::function<int64(int64, int64)>& operation);
  // Finds start and end segment indices from a range; returns false if the
  // range is outside the domain of the function.
  bool FindSegmentIndicesFromRange(int64 range_start, int64 range_end,
                                   int* start_segment, int* end_segment) const;
  void UpdateStatus() {
    if (is_modified_) {
      is_convex_ = IsConvexInternal();
      is_non_decreasing_ = IsNonDecreasingInternal();
      is_non_increasing_ = IsNonIncreasingInternal();
      is_modified_ = false;
    }
  }
  bool IsConvexInternal() const;
  bool IsNonDecreasingInternal() const;
  bool IsNonIncreasingInternal() const;

  // The vector of segments in the function, sorted in ascending order of start
  // points.
  std::vector<PiecewiseSegment> segments_;
  bool is_modified_;
  bool is_convex_;
  bool is_non_decreasing_;
  bool is_non_increasing_;
};
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_H_
