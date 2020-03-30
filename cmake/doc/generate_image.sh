#/usr/bin/env bash
set -ex

rm -f *.svg *.png
for i in *.dot; do
  #plantuml -Tpng "$i";
  plantuml -Tsvg "$i";
done
