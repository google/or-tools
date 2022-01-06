#/usr/bin/env bash
set -ex

rm -f *.svg
for i in *.dot; do
  plantuml -Tsvg "$i";
done
