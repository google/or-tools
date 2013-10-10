// Copyright 2010-2013 Google
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
// This file implements piecewise linear functions over int64. It is built by
// inserting rays going right (increasing x).
// This class maintains a minimal internal representation by merging equivalent
// rays, and checks for overflow.

#ifndef OR_TOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_H_
#define OR_TOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_H_

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/integral_types.h"
#include "base/macros.h"

using std::string;

namespace operations_research {
// This structure stores one ray. It contains the origin and the
// slope. It is always going in the direction of increasing abcissa. It
// cannot be vertical.
struct PiecewiseRay {
  PiecewiseRay(int64 x_, int64 y_, int64 slope_)
      : x(x_), y(y_), slope(slope_) {}

  // This method checks that xx >= this.x.
  int64 Value(int64 xx) const;

  int64 x;
  int64 y;
  int64 slope;

  // For sorting.
  bool operator<(const PiecewiseRay& other) const {
    return x < other.x;
  }

 private:
  // This methods performs a slower computation, but is always correct w.r.t.
  // integer overflow. It also checks that xx >= this.x.
  int64 SafeValue(int64 xx) const;
};

// This class represents a piecewise linear function as a sequence of
// rays.  Before the first ray, the value of the function is always 0.
// For a given x value, it find the last ray that contains x and uses
// this ray to compute the returned value of the function.
class PiecewiseLinearFunction {
 public:
  static const int kNotFound;
  // Returns the value of the piecewise linear function for x.
  int64 Value(int64 x);

  // Inserts a semi-straight line originated at (x, y), defined for all x' > x,
  // and with a slope 'slope'.
  void InsertRay(int64 x, int64 y, int64 slope);

  // Adds 'function' to the current piecewise linear function.
  void Add(const PiecewiseLinearFunction& function);

  string DebugString() const;

  // Access to the sequence of rays.
  const std::vector<PiecewiseRay>& rays() const { return rays_; }
  // Useful to debug.
  int rays_size() const { return rays_.size(); }

 private:
  std::vector<PiecewiseRay> rays_;
};
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_H_
