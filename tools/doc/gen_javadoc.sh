#!/usr/bin/env bash
set -euxo pipefail

# Check tar is in PATH
command -v jar

source Version.txt

OUTPUT_DIR="docs/javadoc"
rm -rf ${OUTPUT_DIR}
mkdir -p ${OUTPUT_DIR}

ARCHIVE=$(find "temp_java/ortools-java/target" -iname "ortools-java-${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}.*-javadoc.jar")
(cd ${OUTPUT_DIR} && jar -xvf "../../${ARCHIVE}")
