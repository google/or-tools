// Copyright 2010-2024 Google LLC
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

#include "ortools/linear_solver/users_allowing_model_storage.h"

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"

namespace operations_research {
const absl::flat_hash_set<absl::string_view>& UsersAllowingModelStorage() {
  static const auto* const set = new absl::flat_hash_set<absl::string_view>{
      // Approved by default.
      "operations-research",

      // Approved hmajaya@ on 2019/05/17 by e-mail.
      "apex-eng",

      // Approved by jhuchette@ on 2024-02-29 by code review.
      "apps-capacity-auxon",
      "autocap-automation",
      "autocap-solver-access",

      // Approved by mlubin@, dapplegate@, and bwydrowski@ on 2019/05/17
      // by e-mail. As of 2020/04/08, prod queries are sent by "muppet-packer".
      "blokus-prod",
      "blokus-planning",
      "blokus-packer-dev",
      "muppet-packer",

      // Approved by sjoakley@ on 2019/10/22 by e-mail.
      "cloud-capacity",
      "techinfra-capacity",

      // Approved by sgowal@ on 2019/05/17 by e-mail.
      "deepmind-research",

      // Approved by yxz@ on 2019/05/17 by e-mail. As of 2020/04/09, many
      // queries are sent by "logs-placement".
      "logs-front-door",
      "logs-front-door-unprivileged",
      "logs-placement",

      // Approved by ansha@ on 2019/05/17 by e-mail. We add netarch-wand-* mdb
      // groups explicitly, because as of 2019/10/22 our naive logic collects
      // a model iff the mdb group listed here matches exactly the mdb group
      // of the RPC sender (i.e., we do not check group transitive memberships,
      // and here all netarch-wand-* groups belong to tetraligh-jobs).
      "tetralight-jobs",
      "netarch-wand-prod",
      "netarch-wand-dev",
      "netarch-wand-test",

      // Approved by haoxu@ on 2019/05/17 by e-mail.
      // As of 2019/10/22, some models are sent by user xiaob@ (instead of
      // raptical@), so we add the user explicitly to this allowlist.
      "cluster-planning-urp-state-runner",
      "cluster-planning-urp-compute",
      "raptical",
      "xiaob",

      // Approved by nharsha@ and mattard@ on 2019/05/17 by e-mail.
      "resource-planning-optimization",
      "resource-planning-optimization-eng-team",
      "resource-portal-test",
  };
  return *set;
}
}  // namespace operations_research
