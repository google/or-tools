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

#ifndef OR_TOOLS_UTIL_SIGINT_H_
#define OR_TOOLS_UTIL_SIGINT_H_

#include <functional>

namespace operations_research {

class SigintHandler {
 public:
  SigintHandler() {}
  ~SigintHandler();

  // Catches ^C and call f() the first time this happen. If ^C is pressed 3
  // times, kill the program.
  void Register(const std::function<void()>& f);

 private:
  static void ControlCHandler(int s);

  int num_sigint_calls_ = 0;
  thread_local static std::function<void()> handler_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SIGINT_H_
