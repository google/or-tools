#!/usr/bin/env bash
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euxo pipefail

# Check tar is in PATH
command -v jar

source Version.txt

OUTPUT_DIR="docs/javadoc"
rm -rf ${OUTPUT_DIR}
mkdir -p ${OUTPUT_DIR}

ARCHIVE=$(find "build_make/java/ortools-java/target" -iname "ortools-java-${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}.*-javadoc.jar")
(cd ${OUTPUT_DIR} && jar -xvf "../../${ARCHIVE}")
