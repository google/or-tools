#!/usr/bin/env bash
set -euxo pipefail

# Check plantuml is in PATH
command -v plantuml

rm -f "*.svg"
for i in *.dot; do
  plantuml -Tsvg "$i";
done
