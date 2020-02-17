#/usr/bin/env bash
set -ex

rm *.svg
for i in *.dot; do
  plantuml -Tsvg $i;
done
