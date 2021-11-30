// Copyright 2010-2021 Google LLC
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

using System;
using Xunit;
using Google.OrTools.Init;
using static Google.OrTools.Init.operations_research_init;

namespace Google.OrTools.Tests
{
    public class InitTest
    {
        [Fact]
        public void CheckLogging()
        {
          Init.CppBridge.InitLogging("init");
          Init.CppBridge.ShutdownLogging();
        }

        [Fact]
        public void CheckFlags()
        {
          Init.CppFlags cpp_flags = new Init.CppFlags();
          cpp_flags.logtostderr = true;
          cpp_flags.log_prefix = true;
          cpp_flags.cp_model_dump_prefix = "init";
          cpp_flags.cp_model_dump_models = true;
          cpp_flags.cp_model_dump_lns = true;
          cpp_flags.cp_model_dump_response = true;
          Init.CppBridge.SetFlags(cpp_flags);
        }

        [Fact]
        public void CheckOrToolsVersion()
        {
          int major = OrToolsVersion.MajorNumber();
          int minor = OrToolsVersion.MinorNumber();
          int patch = OrToolsVersion.PatchNumber();
          string version = OrToolsVersion.VersionString();
          Assert.Equal($"{major}.{minor}.{patch}", version);
        }
    }
} // namespace Google.OrTools.Tests
