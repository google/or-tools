#!/bin/bash

declare -r DIR="${TEST_SRCDIR}/com_google_ortools/ortools/graph/samples"

function test::ortools::code_samples_graph_py() {
    "${DIR}/$1_py3"
}

test::ortools::code_samples_graph_py $1
