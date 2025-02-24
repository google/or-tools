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

#include "ortools/pdlp/sharder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/time/time.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/timer.h"
#include "ortools/pdlp/scheduler.h"

namespace operations_research::pdlp {

using ::Eigen::VectorXd;

Sharder::Sharder(const int64_t num_elements, const int num_shards,
                 Scheduler* const scheduler,
                 const std::function<int64_t(int64_t)>& element_mass)
    : scheduler_(scheduler) {
  CHECK_GE(num_elements, 0);
  if (num_elements == 0) {
    shard_starts_.push_back(0);
    return;
  }
  CHECK_GE(num_shards, 1);
  shard_starts_.reserve(
      std::min(static_cast<int64_t>(num_shards), num_elements) + 1);
  shard_masses_.reserve(
      std::min(static_cast<int64_t>(num_shards), num_elements));
  int64_t overall_mass = 0;
  for (int64_t elem = 0; elem < num_elements; ++elem) {
    overall_mass += element_mass(elem);
  }
  shard_starts_.push_back(0);
  int64_t this_shard_mass = element_mass(0);
  for (int64_t elem = 1; elem < num_elements; ++elem) {
    int64_t this_elem_mass = element_mass(elem);
    if (this_shard_mass + (this_elem_mass / 2) >= overall_mass / num_shards) {
      // `elem` starts a new shard.
      shard_masses_.push_back(this_shard_mass);
      shard_starts_.push_back(elem);
      this_shard_mass = this_elem_mass;
    } else {
      this_shard_mass += this_elem_mass;
    }
  }
  shard_starts_.push_back(num_elements);
  shard_masses_.push_back(this_shard_mass);
  CHECK_EQ(NumShards(), shard_masses_.size());
}

Sharder::Sharder(const int64_t num_elements, const int num_shards,
                 Scheduler* const scheduler)
    : scheduler_(scheduler) {
  CHECK_GE(num_elements, 0);
  if (num_elements == 0) {
    shard_starts_.push_back(0);
    return;
  }
  CHECK_GE(num_shards, 1);
  shard_starts_.reserve(std::min(int64_t{num_shards}, num_elements) + 1);
  shard_masses_.reserve(std::min(int64_t{num_shards}, num_elements));
  if (num_shards >= num_elements) {
    for (int64_t element = 0; element < num_elements; ++element) {
      shard_starts_.push_back(static_cast<int>(element));
      shard_masses_.push_back(1);
    }
  } else {
    for (int shard = 0; shard < num_shards; ++shard) {
      const int64_t this_shard_start = ((num_elements * shard) / num_shards);
      const int64_t next_shard_start =
          ((num_elements * (shard + 1)) / num_shards);
      if (next_shard_start - this_shard_start > 0) {
        shard_starts_.push_back(this_shard_start);
        shard_masses_.push_back(next_shard_start - this_shard_start);
      }
    }
  }
  shard_starts_.push_back(num_elements);
  CHECK_EQ(NumShards(), shard_masses_.size());
}

Sharder::Sharder(const Sharder& other_sharder, const int64_t num_elements)
    // The `std::max()` protects against `other_sharder.NumShards() == 0`, which
    // will happen if `other_sharder` had `num_elements == 0`.
    : Sharder(num_elements, std::max(1, other_sharder.NumShards()),
              other_sharder.scheduler_) {}

void Sharder::ParallelForEachShard(
    const std::function<void(const Shard&)>& func) const {
  if (scheduler_) {
    absl::BlockingCounter counter(NumShards());
    VLOG(2) << "Starting ParallelForEachShard()";
    scheduler_->ParallelFor(0, NumShards(), [&](int shard_num) {
      WallTimer timer;
      if (VLOG_IS_ON(2)) {
        timer.Start();
      }
      func(Shard(shard_num, this));
      if (VLOG_IS_ON(2)) {
        timer.Stop();
        VLOG(2) << "Shard " << shard_num << " with " << ShardSize(shard_num)
                << " elements and " << ShardMass(shard_num)
                << " mass finished with "
                << ShardMass(shard_num) /
                       std::max(int64_t{1},
                                absl::ToInt64Microseconds(timer.GetDuration()))
                << " mass/usec.";
      }
    });
    VLOG(2) << "Done ParallelForEachShard()";
  } else {
    for (int shard_num = 0; shard_num < NumShards(); ++shard_num) {
      func(Shard(shard_num, this));
    }
  }
}

double Sharder::ParallelSumOverShards(
    const std::function<double(const Shard&)>& func) const {
  VectorXd local_sums(NumShards());
  ParallelForEachShard([&](const Sharder::Shard& shard) {
    local_sums[shard.Index()] = func(shard);
  });
  return local_sums.sum();
}

bool Sharder::ParallelTrueForAllShards(
    const std::function<bool(const Shard&)>& func) const {
  // Recall `std::vector<bool>` is not thread-safe.
  std::vector<int> local_result(NumShards());
  ParallelForEachShard([&](const Sharder::Shard& shard) {
    local_result[shard.Index()] = static_cast<int>(func(shard));
  });
  return std::all_of(local_result.begin(), local_result.end(),
                     [](const int v) { return static_cast<bool>(v); });
}

VectorXd TransposedMatrixVectorProduct(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
    const VectorXd& vector, const Sharder& sharder) {
  CHECK_EQ(vector.size(), matrix.rows());
  VectorXd answer(matrix.cols());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    // NOTE: For very sparse columns, assignment to `shard(answer)` incurs a
    // measurable overhead compared to using a constructor
    // (i.e. `VectorXd temp = ...`). It is not clear why this is the case, nor
    // how to avoid it.
    shard(answer) = shard(matrix).transpose() * vector;
  });
  return answer;
}

void SetZero(const Sharder& sharder, VectorXd& dest) {
  dest.resize(sharder.NumElements());
  sharder.ParallelForEachShard(
      [&](const Sharder::Shard& shard) { shard(dest).setZero(); });
}

VectorXd ZeroVector(const Sharder& sharder) {
  VectorXd result(sharder.NumElements());
  SetZero(sharder, result);
  return result;
}

VectorXd OnesVector(const Sharder& sharder) {
  VectorXd result(sharder.NumElements());
  sharder.ParallelForEachShard(
      [&](const Sharder::Shard& shard) { shard(result).setOnes(); });
  return result;
}

void AddScaledVector(const double scale, const VectorXd& increment,
                     const Sharder& sharder, VectorXd& dest) {
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    shard(dest) += scale * shard(increment);
  });
}

void AssignVector(const VectorXd& vec, const Sharder& sharder, VectorXd& dest) {
  dest.resize(vec.size());
  sharder.ParallelForEachShard(
      [&](const Sharder::Shard& shard) { shard(dest) = shard(vec); });
}

VectorXd CloneVector(const VectorXd& vec, const Sharder& sharder) {
  VectorXd dest;
  AssignVector(vec, sharder, dest);
  return dest;
}

void CoefficientWiseProductInPlace(const VectorXd& scale,
                                   const Sharder& sharder, VectorXd& dest) {
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    shard(dest) = shard(dest).cwiseProduct(shard(scale));
  });
}

void CoefficientWiseQuotientInPlace(const VectorXd& scale,
                                    const Sharder& sharder, VectorXd& dest) {
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    shard(dest) = shard(dest).cwiseQuotient(shard(scale));
  });
}

double Dot(const VectorXd& v1, const VectorXd& v2, const Sharder& sharder) {
  return sharder.ParallelSumOverShards(
      [&](const Sharder::Shard& shard) { return shard(v1).dot(shard(v2)); });
}

double LInfNorm(const VectorXd& vector, const Sharder& sharder) {
  VectorXd local_max(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    local_max[shard.Index()] = shard(vector).lpNorm<Eigen::Infinity>();
  });
  return local_max.lpNorm<Eigen::Infinity>();
}

double L1Norm(const VectorXd& vector, const Sharder& sharder) {
  return sharder.ParallelSumOverShards(
      [&](const Sharder::Shard& shard) { return shard(vector).lpNorm<1>(); });
}

double SquaredNorm(const VectorXd& vector, const Sharder& sharder) {
  return sharder.ParallelSumOverShards(
      [&](const Sharder::Shard& shard) { return shard(vector).squaredNorm(); });
}

double Norm(const VectorXd& vector, const Sharder& sharder) {
  return std::sqrt(SquaredNorm(vector, sharder));
}

double SquaredDistance(const VectorXd& vector1, const VectorXd& vector2,
                       const Sharder& sharder) {
  return sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
    return (shard(vector1) - shard(vector2)).squaredNorm();
  });
}

double Distance(const VectorXd& vector1, const VectorXd& vector2,
                const Sharder& sharder) {
  return std::sqrt(SquaredDistance(vector1, vector2, sharder));
}

double ScaledLInfNorm(const VectorXd& vector, const VectorXd& scale,
                      const Sharder& sharder) {
  VectorXd local_max(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    local_max[shard.Index()] =
        shard(vector).cwiseProduct(shard(scale)).lpNorm<Eigen::Infinity>();
  });
  return local_max.lpNorm<Eigen::Infinity>();
}

double ScaledSquaredNorm(const VectorXd& vector, const VectorXd& scale,
                         const Sharder& sharder) {
  return sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
    return shard(vector).cwiseProduct(shard(scale)).squaredNorm();
  });
}

double ScaledNorm(const VectorXd& vector, const VectorXd& scale,
                  const Sharder& sharder) {
  return std::sqrt(ScaledSquaredNorm(vector, scale, sharder));
}

VectorXd ScaledColLInfNorm(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
    const VectorXd& row_scaling_vec, const VectorXd& col_scaling_vec,
    const Sharder& sharder) {
  CHECK_EQ(matrix.cols(), col_scaling_vec.size());
  CHECK_EQ(matrix.rows(), row_scaling_vec.size());
  VectorXd answer(matrix.cols());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    auto matrix_shard = shard(matrix);
    auto col_scaling_shard = shard(col_scaling_vec);
    for (int64_t col_num = 0; col_num < shard(matrix).outerSize(); ++col_num) {
      double max = 0.0;
      for (decltype(matrix_shard)::InnerIterator it(matrix_shard, col_num); it;
           ++it) {
        max = std::max(max, std::abs(it.value() * row_scaling_vec[it.row()]));
      }
      shard(answer)[col_num] = max * std::abs(col_scaling_shard[col_num]);
    }
  });
  return answer;
}

VectorXd ScaledColL2Norm(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
    const VectorXd& row_scaling_vec, const VectorXd& col_scaling_vec,
    const Sharder& sharder) {
  CHECK_EQ(matrix.cols(), col_scaling_vec.size());
  CHECK_EQ(matrix.rows(), row_scaling_vec.size());
  VectorXd answer(matrix.cols());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    auto matrix_shard = shard(matrix);
    auto col_scaling_shard = shard(col_scaling_vec);
    for (int64_t col_num = 0; col_num < shard(matrix).outerSize(); ++col_num) {
      double sum_of_squares = 0.0;
      for (decltype(matrix_shard)::InnerIterator it(matrix_shard, col_num); it;
           ++it) {
        sum_of_squares +=
            MathUtil::Square(it.value() * row_scaling_vec[it.row()]);
      }
      shard(answer)[col_num] =
          std::sqrt(sum_of_squares) * std::abs(col_scaling_shard[col_num]);
    }
  });
  return answer;
}

}  // namespace operations_research::pdlp
