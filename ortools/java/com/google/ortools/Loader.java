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

import com.sun.jna.Platform;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.FileSystem;
import java.nio.file.FileSystemAlreadyExistsException;
import java.nio.file.FileSystems;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Collections;
import java.util.Objects;

/** Load native libraries needed for using ortools-java.*/
public class Loader {
  private static final String RESOURCE_PATH = "ortools-" + Platform.RESOURCE_PREFIX + "/";

  /** Try to locate the native libraries directory.*/
  private static URI getNativeResourceURI() throws IOException {
    ClassLoader loader = Loader.class.getClassLoader();
    URL resourceURL = loader.getResource(RESOURCE_PATH);
    Objects.requireNonNull(resourceURL,
        String.format("Resource %s was not found in ClassLoader %s", RESOURCE_PATH, loader));

    URI resourceURI;
    try {
      resourceURI = resourceURL.toURI();
    } catch (URISyntaxException e) {
      throw new IOException(e);
    }
    return resourceURI;
  }

  @FunctionalInterface
  private interface PathConsumer<T extends IOException> {
    void accept(Path path) throws T;
  }

  /**
   * Extract native resources in a temp directory.
   * @param resourceURI Native resource location.
   * @return The directory path containing all extracted libraries.
   */
  private static Path unpackNativeResources(URI resourceURI) throws IOException {
    Path tempPath;
    tempPath = Files.createTempDirectory("ortools-java");
    tempPath.toFile().deleteOnExit();

    PathConsumer<?> visitor;
    visitor = (Path sourcePath) -> Files.walkFileTree(sourcePath, new SimpleFileVisitor<Path>() {
      @Override
      public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
        Path newPath = tempPath.resolve(sourcePath.getParent().relativize(file).toString());
        Files.copy(file, newPath);
        newPath.toFile().deleteOnExit();
        return FileVisitResult.CONTINUE;
      }

      @Override
      public FileVisitResult preVisitDirectory(Path dir, BasicFileAttributes attrs)
          throws IOException {
        Path newPath = tempPath.resolve(sourcePath.getParent().relativize(dir).toString());
        Files.copy(dir, newPath);
        newPath.toFile().deleteOnExit();
        return FileVisitResult.CONTINUE;
      }
    });

    FileSystem fs;
    try {
      fs = FileSystems.newFileSystem(resourceURI, Collections.emptyMap());
    } catch (FileSystemAlreadyExistsException e) {
      fs = FileSystems.getFileSystem(resourceURI);
      if (fs == null) {
        throw new IllegalArgumentException();
      }
    }
    Path p = fs.provider().getPath(resourceURI);
    visitor.accept(p);
    return tempPath;
  }

  /** Unpack and Load the native libraries needed for using ortools-java.*/
  private static boolean loaded = false;
  public static void loadNativeLibraries() {
    if (!loaded) {
      try {
        URI resourceURI = getNativeResourceURI();
        Path tempPath = unpackNativeResources(resourceURI);
        // Load the native library
        System.load(tempPath.resolve(RESOURCE_PATH)
                        .resolve(System.mapLibraryName("jniortools"))
                        .toString());
        loaded = true;
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
