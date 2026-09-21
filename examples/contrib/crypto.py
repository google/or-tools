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
"""Crypto problem in Google CP Solver.

Prolog benchmark problem GNU Prolog (crypta.pl)
'''
Name           : crypta.pl
Title          : crypt-arithmetic
Original Source: P. Van Hentenryck's book
Adapted by     : Daniel Diaz - INRIA France
Date           : September 1992

Solve the operation:

   B A I J J A J I I A H F C F E B B J E A
 + D H F G A B C D I D B I F F A G F E J E
 -----------------------------------------
 = G J E G A C D D H F A F J B F I H E E F
'''

Compare with the following models:
* MiniZinc: http://www.hakank.org/minizinc/crypta.mzn
* Comet   : http://www.hakank.org/comet/crypta.co
* ECLiPSe : http://www.hakank.org/eclipse/crypta.ecl
* SICStus : http://hakank.org/sicstus/crypta.pl


This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

from ortools.constraint_solver.python import constraint_solver as cp


def main():

    # Create the solver.
    solver = cp.Solver("Crypto problem")

    #
    # data
    #
    num_letters = 26

    BALLET = 45
    CELLO = 43
    CONCERT = 74
    FLUTE = 30
    FUGUE = 50
    GLEE = 66
    JAZZ = 58
    LYRE = 47
    OBOE = 53
    OPERA = 65
    POLKA = 59
    QUARTET = 50
    SAXOPHONE = 134
    SCALE = 51
    SOLO = 37
    SONG = 61
    SOPRANO = 82
    THEME = 72
    VIOLIN = 100
    WALTZ = 34

    #
    # variables
    #
    LD = [solver.new_int_var(1, num_letters, "LD[%i]" % i) for i in range(num_letters)]
    (
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
    ) = LD

    #
    # constraints
    #
    solver.add_all_different(LD)
    solver.add(B + A + L + L + E + T == BALLET)
    solver.add(C + E + L + L + O == CELLO)
    solver.add(C + O + N + C + E + R + T == CONCERT)
    solver.add(F + L + U + T + E == FLUTE)
    solver.add(F + U + G + U + E == FUGUE)
    solver.add(G + L + E + E == GLEE)
    solver.add(J + A + Z + Z == JAZZ)
    solver.add(L + Y + R + E == LYRE)
    solver.add(O + B + O + E == OBOE)
    solver.add(O + P + E + R + A == OPERA)
    solver.add(P + O + L + K + A == POLKA)
    solver.add(Q + U + A + R + T + E + T == QUARTET)
    solver.add(S + A + X + O + P + H + O + N + E == SAXOPHONE)
    solver.add(S + C + A + L + E == SCALE)
    solver.add(S + O + L + O == SOLO)
    solver.add(S + O + N + G == SONG)
    solver.add(S + O + P + R + A + N + O == SOPRANO)
    solver.add(T + H + E + M + E == THEME)
    solver.add(V + I + O + L + I + N == VIOLIN)
    solver.add(W + A + L + T + Z == WALTZ)

    #
    # search and result
    #
    db = solver.phase(
        LD,
        cp.IntVarStrategy.CHOOSE_MIN_SIZE_LOWEST_MIN,
        cp.IntValueStrategy.ASSIGN_CENTER_VALUE,
    )

    solver.new_search(db)

    num_solutions = 0
    str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    while solver.next_solution():
        num_solutions += 1
        for letter, val in [(str[i], LD[i].value()) for i in range(num_letters)]:
            print("%s: %i" % (letter, val))
        print()

    solver.end_search()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


if __name__ == "__main__":
    main()
