#!/usr/bin/env python3
# Copyright 2010 Hakan Kjellerstrand hakank@gmail.com
#
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
"""Word square in Google CP Solver.

From http://en.wikipedia.org/wiki/Word_square ''' A word square is a special
case of acrostic. It consists of a set of words, all having the same number of
letters as the total number of words (the 'order' of the square); when the words
are written out in a square grid horizontally, the same set of words can be read
vertically. '''

Compare with the following models:
* MiniZinc: http://www.hakank.org/minizinc/word_square.mzn
* Comet   : http://www.hakank.org/comet/word_square.co
* Choco   : http://www.hakank.org/choco/WordSquare.java
* Gecode  : http://www.hakank.org/gecode/word_square.cpp
* Gecode  : http://www.hakank.org/gecode/word_square2.cpp
* JaCoP   : http://www.hakank.org/JaCoP/WordSquare.java
* Zinc: http://hakank.org/minizinc/word_square.zinc

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

import re
import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main(words, word_len, num_answers=20):
    # Create the solver.
    solver = cp.Solver("Problem")

    #
    # data
    #
    num_words = len(words)
    n = word_len
    d, rev = get_dict()

    #
    # declare variables
    #
    A = {}
    for i in range(num_words):
        for j in range(word_len):
            A[(i, j)] = solver.new_int_var(0, 29, "A(%i,%i)" % (i, j))

    A_flat = [A[(i, j)] for i in range(num_words) for j in range(word_len)]

    E = [solver.new_int_var(0, num_words, "E%i" % i) for i in range(n)]

    #
    # constraints
    #
    solver.add_all_different(E)

    # copy the words to a Matrix
    for I in range(num_words):
        for J in range(word_len):
            solver.add(A[(I, J)] == d[words[I][J]])

    for i in range(word_len):
        for j in range(word_len):
            # This is what I would like to do:
            # solver.add(A[(E[i],j)] == A[(E[j],i)])

            # We must use Element explicitly
            solver.add(
                solver.element(A_flat, E[i] * word_len + j)
                == solver.element(A_flat, E[j] * word_len + i)
            )

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(E)

    # db: DecisionBuilder
    db = solver.phase(
        E + A_flat,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )

    solver.new_search(db)
    num_solutions = 0
    while solver.next_solution():
        # print E
        print_solution(E, words)
        num_solutions += 1
        if num_solutions > num_answers:
            break

    solver.end_search()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


#
# convert a character to integer
#
def get_dict():
    alpha = "abcdefghijklmnopqrstuvwxyzåäö"
    d = {}
    rev = {}
    count = 1
    for a in alpha:
        d[a] = count
        rev[count] = a
        count += 1
    return d, rev


def print_solution(E, words):
    # print E
    for e in E:
        print(words[e.value()])
    print()


def read_words(word_list, word_len, limit):
    dict = {}
    all_words = []
    count = 0
    words = open(word_list).readlines()
    for w in words:
        w = w.strip().lower()
        # if len(w) == word_len and not dict.has_key(w) and not re.search("[^a-zåäö]",w) and count < limit:
        # Later note: The limit is not needed anymore with Mistral
        if len(w) == word_len and w not in dict and not re.search("[^a-zåäö]", w):
            dict[w] = 1
            all_words.append(w)
            count += 1
    return all_words


word_dict = "examples/contrib/words_list.txt"
word_len = 4
limit = 1000000
num_answers = 5

if __name__ == "__main__":
    if len(sys.argv) > 1:
        word_dict = sys.argv[1]
    if len(sys.argv) > 2:
        word_len = int(sys.argv[2])
    if len(sys.argv) > 3:
        limit = int(sys.argv[3])
    if len(sys.argv) > 4:
        num_answers = int(sys.argv[4])

    # Note: I have to use a limit, otherwise it seg faults
    words = read_words(word_dict, word_len, limit)
    print(f"It was {len(words)} words")
    main(words, word_len, num_answers)
