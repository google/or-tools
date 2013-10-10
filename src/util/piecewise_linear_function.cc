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
#include "util/piecewise_linear_function.h"

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "util/saturated_arithmetic.h"

namespace operations_research {
namespace {
int FindRayIndex(const std::vector<PiecewiseRay>& rays, int64 x) {
  // TODO(user): Can we add a sentinel (kint64min, 0, 0)?
  if (rays.empty() || rays.front().x > x) {
    return PiecewiseLinearFunction::kNotFound;
  }
  // Linear scan for the time being.
  // TODO(user): Implement binary search.
  for (int i = 0; i < rays.size() - 1; ++i) {
    if (rays[i].x <= x && x < rays[i + 1].x) {
      return i;
    }
  }
  return rays.size() - 1;
}

bool IsAtBounds(int64 value) {
  return value == kint64min || value == kint64max;
}
}  // namespace

int64 PiecewiseRay::SafeValue(int64 xx) const {
  const uint64 span_x = xx - x;
  if (span_x == 0) {
    return y;
  }
  if (slope == 0) {
    return y;
  }
  if (slope > 0) {
    const uint64 span_y = UnsignedCapProd(span_x, slope);
    if (y == 0) {
      return span_y > kint64max ? kint64max : span_y;
    } else if (y > 0) {
      const uint64 unsigned_sum = UnsignedCapAdd(y, span_y);
      return unsigned_sum > kint64max ? kint64max
                                      : static_cast<int64>(unsigned_sum);
    } else {
      const uint64 abs_y = static_cast<uint64>(-y);
      if (span_y > abs_y) {
        return static_cast<int64>(span_y - abs_y);
      } else {
        return -static_cast<int64>(abs_y - span_y);
      }
    }
  } else {  // slope < 0.
    const uint64 opp_span_y = UnsignedCapProd(span_x, -slope);
    if (y == 0) {
      return opp_span_y > kint64max ? kint64min
                                    : -static_cast<int64>(opp_span_y);
    } else if (y < 0) {
      const uint64 opp_unsigned_sum = UnsignedCapAdd(-y, opp_span_y);
      return opp_unsigned_sum > kint64max
                 ? kint64min
                 : -static_cast<int64>(opp_unsigned_sum);
    } else {
      if (opp_span_y > y) {
        return -static_cast<int64>(opp_span_y - y);
      } else {
        return static_cast<int64>(y - opp_span_y);
      }
    }
  }
}

int64 PiecewiseRay::Value(int64 xx) const {
  DCHECK_GE(xx, x);
  const int64 span_x = CapSub(xx, x);
  if (IsAtBounds(span_x)) {
    return SafeValue(xx);
  }
  const int64 span_y = CapProd(slope, span_x);
  if (IsAtBounds(span_y)) {
    return SafeValue(xx);
  }
  const int64 value = CapAdd(y, span_y);
  return IsAtBounds(value) ? SafeValue(xx) : value;
}

const int PiecewiseLinearFunction::kNotFound = -1;

void PiecewiseLinearFunction::InsertRay(int64 x, int64 y, int64 slope) {
  if (rays_.empty() ||
      (x > rays_.back().x && (rays_.back().slope != slope || Value(x) != y))) {
    rays_.push_back(PiecewiseRay(x, y, slope));
  } else {
    const int index = FindRayIndex(rays_, x);
    if (index == kNotFound) {
      const PiecewiseRay new_ray(x, y, slope);
      if (rays_.front().slope == slope &&
          new_ray.Value(rays_.front().x) == rays_.front().y) {
        // Overwrite first ray.
        rays_.front().x = x;
        rays_.front().y = y;
      } else {
        // Insert new ray in front.
        rays_.insert(rays_.begin(), new_ray);
      }
    } else {
      if (rays_[index].x == x) {
        // Overwrite the current ray.
        rays_[index].y = y;
        rays_[index].slope = slope;
      } else {
        const PiecewiseRay new_ray(x, y, slope);
        if (rays_[index].slope == slope && Value(x) == y) {
          // Same interval, ignore it.
          return;
        } else if (index < rays_.size() && rays_[index + 1].slope == slope &&
                   new_ray.Value(rays_[index + 1].x == rays_[index + 1].y)) {
          // Overwrite the following ray.
          rays_[index + 1].x = x;
          rays_[index + 1].y = y;
        } else {
          // Add the new ray in the right place.
          rays_.insert(rays_.begin() + index + 1, new_ray);
        }
      }
    }
  }
}

int64 PiecewiseLinearFunction::Value(int64 x) {
  const int index = FindRayIndex(rays_, x);
  if (index == kNotFound) {
    return 0;
  }
  return rays_[index].Value(x);
}

string PiecewiseLinearFunction::DebugString() const {
  string result = "PiecewiseLinearFunction(";
  for (int i = 0; i < rays_.size(); ++i) {
    result.append(StringPrintf("<x = %" GG_LL_FORMAT "d, y = %" GG_LL_FORMAT
                               "d, slope = %" GG_LL_FORMAT "d>",
                               rays_[i].x, rays_[i].y, rays_[i].slope));
    if (i != rays_.size() - 1) {
      result.append(" ");
    }
  }
  return result;
}

void PiecewiseLinearFunction::Add(const PiecewiseLinearFunction& other) {
  if (other.rays_.empty()) {
    return;
  }
  if (rays_.empty()) {
    rays_ = other.rays_;
    return;
  }
  std::vector<PiecewiseRay> own_rays_;
  const std::vector<PiecewiseRay>& other_rays_ = other.rays_;
  own_rays_.swap(rays_);
  std::set<int64> time_points;
  for (int i = 0; i < own_rays_.size(); ++i) {
    time_points.insert(own_rays_[i].x);
  }
  for (int i = 0; i < other_rays_.size(); ++i) {
    time_points.insert(other_rays_[i].x);
  }
  for (std::set<int64>::const_iterator it = time_points.begin();
       it != time_points.end(); ++it) {
    const int64 t = *it;
    const int index1 = FindRayIndex(own_rays_, t);
    const int index2 = FindRayIndex(other_rays_, t);
    if (index1 == kNotFound) {
      CHECK_NE(index2, kNotFound);
      const PiecewiseRay& ray = other_rays_[index2];
      rays_.push_back(ray);
    } else if (index2 == kNotFound) {
      const PiecewiseRay& ray = own_rays_[index1];
      rays_.push_back(ray);
    } else {
      const PiecewiseRay& ray1 = own_rays_[index1];
      const PiecewiseRay& ray2 = other_rays_[index2];
      const int64 y1 = ray1.Value(t);
      const int64 y2 = ray2.Value(t);
      const int64 y = CapAdd(y1, y2);
      const int64 slope = CapAdd(ray1.slope, ray2.slope);
      rays_.push_back(PiecewiseRay(t, y, slope));
    }
  }
}
}  // namespace operations_research
