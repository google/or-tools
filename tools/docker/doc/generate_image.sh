#!/usr/bin/env bash
set -eux

rm -f ./*.svg ./*.png
for i in *.dot; do
  #plantuml -Tpng "$i";
  plantuml -Tsvg "$i";
done
