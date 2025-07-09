# LP Data

This directory contains a rich collection of C++ libraries for handling Linear
Programming (LP) data structures.

It provides core components for representing, manipulating, and solving linear
programs, with a focus on efficient handling of sparse data and various utility
functions for pre-processing and analysis.

## Core Data Structures

This set of libraries provides the fundamental building blocks for representing
and working with linear programming problems.

* [`lp_types.h`][lp_types_h]: Defines common types and constants used throughout
  the linear programming solver.
* [`lp_data.h`][lp_data_h]: Provides the main `LinearProgram` class for storing
  the complete data of a linear program, including the objective function,
  constraint matrix, and variable bounds.
* [`lp_utils.h`][lp_utils_h]: Contains basic utility functions for operations on
  fractional numbers and row/column vectors.

## Sparse Data Representation

Given that large-scale linear programs are often sparse, this directory offers a
suite of libraries for efficient sparse data handling.

* [`sparse.h`][sparse_h]: Implements data structures for sparse matrices, based
  on well-established references in the field of direct methods for sparse
  matrices.
* [`sparse_vector.h`][sparse_vector_h]: Provides classes to represent sparse
  vectors efficiently.
* [`sparse_column.h`][sparse_column_h] & [`sparse_row.h`][sparse_row_h]:
  Specializations of sparse vectors for column-oriented and row-oriented matrix
  storage schemes.
* [`scattered_vector.h`][scattered_vector_h]: Implements vectors that offer a
  sparse interface to what is internally a dense storage, which can be useful
  for certain computations.

## LP Solvers and Utilities

A collection of tools for preprocessing, analyzing, and manipulating linear
programs.

* [`matrix_scaler.h`][matrix_scaler_h]: Provides the `SparseMatrixScaler` class,
  which scales a `SparseMatrix` to improve numerical stability during the
  solving process.
* [`lp_decomposer.h`][lp_decomposer_h]: Implements a tool to decompose a large
  `LinearProgram` into several smaller, independent subproblems by identifying
  disconnected components in the constraint matrix.
* [`permutation.h`][permutation_h]: Contains utilities for handling row and
  column permutations on LP data structures.

## Parsers and I/O Utilities

This group of libraries handles reading and writing LP data in various formats.

* [`lp_parser.h`][lp_parser_h]: A simple parser for creating a linear program
  from a string representation.
* [`mps_reader.h`][mps_reader_h]: A reader for the industry-standard MPS file
  format for mathematical programming problems.
* [`sol_reader.h`][sol_reader_h]: A reader for .sol files, which are used to
  specify solution values for a given model.
* [`proto_utils.h`][proto_utils_h]: Provides utilities to convert
  `LinearProgram` objects to and from the MPModelProto protobuf format.
* [`lp_print_utils.h`][lp_print_utils_h]: Contains utilities to display linear
  expressions in a human-readable way, including rational approximations.

<!-- Links used throughout the document. -->

[lp_types_h]: ../lp_data/lp_types.h
[lp_data_h]: ../lp_data/lp_data.h
[lp_utils_h]: ../lp_data/lp_utils.h
[sparse_h]: ../lp_data/sparse.h
[sparse_vector_h]: ../lp_data/sparse_vector.h
[sparse_column_h]: ../lp_data/sparse_column.h
[sparse_row_h]: ../lp_data/sparse_row.h
[scattered_vector_h]: ../lp_data/scattered_vector.h
[matrix_scaler_h]: ../lp_data/matrix_scaler.h
[lp_decomposer_h]: ../lp_data/lp_decomposer.h
[permutation_h]: ../lp_data/permutation.h
[lp_parser_h]: ../lp_data/lp_parser.h
[mps_reader_h]: ../lp_data/mps_reader.h
[sol_reader_h]: ../lp_data/sol_reader.h
[proto_utils_h]: ../lp_data/proto_utils.h
[lp_print_utils_h]: ../lp_data/lp_print_utils.h
