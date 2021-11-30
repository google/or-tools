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

package com.google.ortools.init;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.ortools.Loader;
import com.google.ortools.init.CppBridge;
import com.google.ortools.init.CppFlags;
import com.google.ortools.init.OrToolsVersion;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests the Init java interface. */
public final class InitTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testLogging() {
    CppBridge.initLogging("init");
    CppBridge.shutdownLogging();
  }

  @Test
  public void testFlags() {
     final CppFlags cpp_flags = new CppFlags();
     assertNotNull(cpp_flags);
     cpp_flags.setLogtostderr(true);
     cpp_flags.setLog_prefix(true);
     cpp_flags.setCp_model_dump_prefix("init");
     cpp_flags.setCp_model_dump_models(true);
     cpp_flags.setCp_model_dump_lns(true);
     cpp_flags.setCp_model_dump_response(true);
     CppBridge.setFlags(cpp_flags);
  }

  @Test
  public void testVersion() {
    final OrToolsVersion v = new OrToolsVersion();
    assertNotNull(v);
    final int major = v.getMajorNumber();
    final int minor = v.getMinorNumber();
    final int patch = v.getPatchNumber();
    final String version =  v.getVersionString();
    final String check = major + "." + minor + "." + patch;
    assertEquals(check, version);
  }
}
