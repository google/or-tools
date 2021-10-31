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

package com.google.ortools;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Objects;

/**
 * Helper class to load native libraries needed for using ortools-java.
 */
public class Loader {
  private static final String RESOURCE_PREFIX;
  private static final String RESOURCE_SUFFIX;
  private static final String[] FILES_TO_EXTRACT;
  static {
    RESOURCE_SUFFIX = System.mapLibraryName("test").split("test\\.")[1];
    switch (RESOURCE_SUFFIX) {
      case "dll":
        RESOURCE_PREFIX = "win32-x86-64";
        FILES_TO_EXTRACT = new String[]{"jniortools"};
        break;
      case "so":
        RESOURCE_PREFIX = "linux-x86-64";
        FILES_TO_EXTRACT = new String[]{"libjniortools", "libortools"};
        break;
      case "dylib":
        RESOURCE_PREFIX = "darwin-x86-64";
        FILES_TO_EXTRACT = new String[]{"libjniortools", "libortools"};
        break;
      default:
        throw new UnsupportedOperationException(String.format("Unknown library suffix %s!", RESOURCE_SUFFIX));
    }
  }

  /**
   * Extract native resources to a temp directory.
   * @return The path containing all extracted libraries.
   */
  private static Path unpackNativeResources() throws IOException {
    Path tempPath = Files.createTempDirectory("ortools-java");
    tempPath.toFile().deleteOnExit();

    ClassLoader loader = Loader.class.getClassLoader();
    for (String file : FILES_TO_EXTRACT) {
      String fullPath = String.format("ortools-%s/%s.%s", RESOURCE_PREFIX, file, RESOURCE_SUFFIX);
      URL url = Objects.requireNonNull(
              loader.getResource(fullPath),
              String.format("Resource %s was not found in ClassLoader %s", fullPath, loader)
      );
      try (InputStream lib = url.openStream()) {
        Files.copy(lib, tempPath.resolve(file));
      }
    }

    return tempPath;
  }

  private static boolean loaded = false;

  /**
   * Extract and load the native libraries needed for using ortools-java.
   */
  public static synchronized void loadNativeLibraries() {
    if (!loaded) {
      try {
        // Extract natives
        Path tempPath = unpackNativeResources();
        // Load natives
        System.load(tempPath.resolve(System.mapLibraryName("jniortools")).toString());
        loaded = true;
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
