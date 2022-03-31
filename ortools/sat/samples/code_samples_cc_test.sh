#!/bin/bash

function test::operations_research_examples::code_samples_sat_cc() {
    EXPECT_SUCCEED "${TEST_SRCDIR}/google3/ortools/sat/samples"/$1_cc"
}

test::operations_research_examples::code_samples_sat_cc $1
