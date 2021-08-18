include "lex_less.mzn";
include "lex_lesseq.mzn";
include "lex_greater.mzn";
include "lex_greatereq.mzn";
include "regular.mzn";
include "regular_nfa.mzn";
include "global_cardinality.mzn";

int: DEF_MAXLENGTH = 10000;

%function var bool:
%'in'(var string: x, set of string: D) = str_dom(x,D);
function var string:
'++'(var string: x, var string: y) ::promise_total = str_concat(x, y);

% Reverse mapping array of integers => string.
function var string:
reverse_map(array[int] of var int: A);
function string:
reverse_map(array[int] of int: A) ::promise_total =
  array_to_string(A);

% Mapping string => array of integers.
function array[int] of var int:
str2ints(var string: x) ::promise_total =
  if is_fixed(x) then string_to_array(fix(x)) else
let {
  var int: lx = str_len(x);
  int: max_length = if ub(lx) == infinity then
    DEF_MAXLENGTH
  else
    ub(lx)
  endif;
  constraint lx <= max_length;
  array[1..max_length] of var {0} union str_intdomain(x): Ax;
  constraint (x = reverse_map(Ax)) :: is_reverse_map;
  constraint forall (i in 1..max_length) (i > lx <-> Ax[i] = 0);
} in Ax
  endif;

% String equality x = y.
predicate
str_eq(var string: x, var string: y) =
let {
  array[int] of var int: Ax = str2ints(x);
  array[int] of var int: Ay = str2ints(y);
  var int: lx = str_len(x);
} in lx = str_len(y) /\ forall (i in 1..ub(lx)) (i <= lx -> Ax[i] = Ay[i]);

% String inequality x != y.
predicate
str_ne(var string: x, var string: y) =
let {
  array[int] of var int: Ax = str2ints(x);
  array[int] of var int: Ay = str2ints(y);
  var int: lx = str_len(x);
} in lx != str_len(y) \/ exists (i in 1..ub(lx)) (i <= lx -> Ax[i] != Ay[i]);

% String concatenation x ++ y.
function var string:
str_concat(var string: x, var string: y) ::promise_total =
let {
  array[int] of var int: Ax = str2ints(x);
  array[int] of var int: Ay = str2ints(y);
  var int: lx = str_len(x);
  var int: ly = str_len(y);
  int: nz = if max(ub(lx), ub(ly)) == infinity then
    DEF_MAXLENGTH
  else
    min(DEF_MAXLENGTH, ub(lx) + ub(ly))
  endif;
  var string(nz): z;
  array[int] of var int: Az = str2ints(z);
  constraint str_len(z) = lx + ly;
  constraint forall (i in 1..ub(lx)) (i <= lx -> Az[i] = Ax[i]);
  constraint forall (i in 1..ub(ly)) (i <= ly -> Az[i + lx] = Ay[i]);
} in z;

% Substring y = x[i..j].
function var string:
str_sub(var string: x, var int: i, var int: j) =
let {
  array[int] of var int: Ax = str2ints(x);
  var int: lx = str_len(x);
  var int: n  = max(1,  i);
  var int: m  = min(lx, j);
  var string(min(ub(m - n + 1), ub(lx))): y;
  array[int] of var int: Ay = str2ints(y);
  var int: ly = str_len(y);
  constraint ly = max(0, m - n + 1);
  constraint forall (k in 1..ub(ly)) (k <= ly -> Ay[k] = Ax[k + n - 1]);
} in y;

% Char at x[i] = x[i..i].
function var string:
str_char_at(var string: x, var int: i) =
let {
  array[int] of var int: Ax = str2ints(x);
  var string(1): y;
  array[int] of var int: Ay = str2ints(y);
  constraint if (1 <= i /\ i <= str_len(x)) then
    Ay[1] = Ax[i]
  else
    Ay[1] = 0
  endif;
} in y;

% String reverse of x.
function var string:
str_rev(var string: x) ::promise_total =
let {
  var int: lx = str_len(x);
  array[int] of var int: Ax = str2ints(x);
  var string(ub(lx)): y;
  array[int] of var int: Ay = str2ints(y);
  constraint lx = str_len(y) /\ forall (i in 1..ub(lx)) (
    i <= lx -> Ax[i] = Ay[lx - i + 1]
  );
} in y;

% String lenght |x|.
function var int:
str_len(var string: x) ::promise_total =
let {
  var 0..str_maxlength(x): l;
} in l;
function int:
str_len(string: x) ::promise_total = string_length(x);


% Lexicographic constraints: x < y, x <= y, x > y, x >= y (w.r.t. ASCII code).
predicate
str_lt(var string: x, var string: y) =
  lex_less(str2ints(x), str2ints(y));
predicate
str_le(var string: x, var string: y) =
  lex_lesseq(str2ints(x), str2ints(y));
predicate
str_gr(var string: x, var string: y) =
  lex_greater(str2ints(x), str2ints(y));
predicate
str_ge(var string: x, var string: y) =
  lex_greatereq(str2ints(x), str2ints(y));

% Iterated concatenation of x for n >= 0 times.
function var string:
str_pow(var string: x, var int: n) ::promise_total =
let {
  array[int] of var int: Ax = str2ints(x);
  var int: lx = str_len(x);
  int: ny = if max(ub(lx), ub(n)) == infinity then
    DEF_MAXLENGTH
  else
    min(DEF_MAXLENGTH, ub(lx) * ub(n))
  endif;
  var string(ny): y;
  array[int] of var int: Ay = str2ints(y);
  var int: ly = str_len(y);
  constraint n >= 0 /\ ly = lx * n /\
  forall (i in 1..ub(lx)) (
    forall (j in 1..ub(ly)) (
      (i <= lx /\ j <= ly) -> Ax[i] = Ay[(j - 1) * lx + i]
    )
  );
} in y;
function var string:
str_pow(string: x, var int: n) ::promise_total =
let {
  array[int] of var int: Ax = str2ints(x);
  int: lx = str_len(x);
  int: ny = if max(lx, ub(n)) == infinity then
    DEF_MAXLENGTH
  else
    min(DEF_MAXLENGTH, lx * ub(n))
  endif;
  var string(ny): y;
  array[int] of var int: Ay = str2ints(y);
  var int: ly = str_len(y);
  constraint n >= 0 /\ ly = lx * n /\
  forall (i in 1..lx) (
    forall (j in 1..ub(ly)) (
      j <= ly -> Ax[i] = Ay[(j - 1) * lx + i]
    )
  );
} in y;

% String nsub: x is not a substring of y.
predicate
str_nsub(var string: x, var string: y) =
let {
  var int: lx = str_len(x);
  var int: ly = str_len(y);
  array[int] of var int: Ax = str2ints(x);
  array[int] of var int: Ay = str2ints(y);
} in lx > ly \/
  forall (i in 1..ly - lx + 1) (
    exists (j in 1..lx) (Ax[j] != Ay[i + j - 1])
  );

% String find: n is the first occurrence of x in y.
function var int:
str_find(var string: x, var string: y) =
let {
  var int: ly = str_len(y);
  int: uy = ub(ly);
  var 0..uy: n;
  var string(uy): y1;
  var string(uy): y2;
  constraint
    str_len(y1) = max(0, n - 1)   /\
    (n == 0 <-> str_nsub(x, y))   /\
    (n  > 0 <-> y = y1 ++ x ++ y2 /\ str_nsub(x, y1))
} in n;

% String replace: returns the replacement of first occurrence of x with x1 in y.
function var string:
str_replace(var string: x, var string: x1, var string: y) =
let {
  var string: y1;
  var string: p;
  var string: s;
  var 0..1: i;
  var 0..ub(str_len(y)): n;
  constraint
    y = p ++ str_pow(x, i) ++ s /\ y1 = p ++ str_pow(x1, i) ++ s
    /\ i = bool2int(n > 0) /\ n = str_find(x, y) /\ str_len(p) = max(0, n - 1)
    /\ str_find(x, p) = (str_len(x) = 0)
} in y1;

% String range: each character of x must belong to the range of characters
% "a".."b" according to the lexicographic order.
predicate
str_range(var string: x, string: a, string: b) =
let {
  int: i = fix(str2ints(a)[1]);
  int: j = fix(str2ints(b)[1]);
  array[int] of var int: Ax = str2ints(x);
} in forall (k in index_set(Ax)) (Ax[k] in {0} union i..j);

% String domain: each character of x must belong to D.
predicate
str_chars(var string: x, set of string: D) =
let {
  array[int] of var int: Ax = str2ints(x);
} in forall (i in index_set(Ax)) (Ax[i] in {0} union str_intdomain(D));

% The set of characters occurring in x must be equal to D
predicate
str_alphabet(var string: x, set of string: D) =
let {
  array[int] of var int: Ax = str2ints(x);
  set of int: I = index_set(Ax);
  set of int: J = str_intdomain(D);
} in forall (i in I) (Ax[i] in {0} union J) /\
     forall (j in J) (exists (i in I) (Ax[i] = j));

% Character S[i] occurs C[i] times in x.
predicate
str_gcc(var string: x, array[int] of string: S, array[int] of var int: C) =
let {
  array[int] of var int: Ax = str2ints(x);
  array[int] of int: Sx = [fix(str2ints(S[i])[1]) | i in index_set(S)];
} in global_cardinality(Ax, Sx, C);

% Membership of x to a regular language defined by a DFA.
% S is the alphabet (the i-th symbol of S is w.r.t. lexicographic ordering).
predicate
str_dfa(var string: x, int: Q, set of string: S,
        array[int, int] of int: d, int: q0, set of int: F) =
let {
  int: s = card(S);
  array[int] of var int: Ax = str2ints(x);
  array[int] of int: map = sort([i | i in str_intdomain(S)]);
  array[1..length(Ax)] of var 1..s + 1: Ay;
  % d1 is the transposed matrix of d.
  array[1..s, 1..Q] of int: d1 =
    array2d(1..s, 1..Q, [d[i, j] | j in 1..s, i in 1..Q]);
  % d2 add to d1 a row (the first one) for the empty character.
  array[1..s + 1, 1..Q] of int: d2 = array2d(1..s + 1, 1..Q,
    [if i in F then i else 0 endif | i in 1..Q ] ++
    [d1[i, j] | i in 1..s, j in 1..Q]
  );
  % dy is the transposed matrix of d2.
  array[1..Q, 1..s + 1] of int: dy =
    array2d(1..Q, 1..s + 1, [d2[i, j] | j in 1..Q, i in 1..s + 1]);
} in regular(Ay, Q, s + 1, dy, q0, F) /\
     forall (i in index_set(Ax)) (
     Ax[i] = if Ay[i] > 1 then map[Ay[i] - 1] else 0 endif
     );

% Membership of x to a regular language defined by a NFA.
% S is the alphabet (the i-th symbol of S is w.r.t. lexicographic ordering).
predicate
str_nfa(var string: x, int: Q, set of string: S,
        array[int, int] of set of int: d, int: q0, set of int: F) =
let {
  int: s = card(S);
  array[int] of var int: Ax = str2ints(x);
  array[int] of int: map = sort([i | i in str_intdomain(S)]);
  array[1..length(Ax)] of var 1..s + 1: Ay;
  % d1 is the transposed matrix of d.
  array[1..s, 1..Q] of set of int: d1 =
    array2d(1..s, 1..Q, [d[i, j] | j in 1..s, i in 1..Q]);
  % d2 add to d1 a row (the first one) for the empty character.
  array[1..s + 1, 1..Q] of set of int: d2 = array2d(1..s + 1, 1..Q,
    [if i in F then {i} else {0} endif | i in 1..Q ] ++
    [d1[i, j] | i in 1..s, j in 1..Q]
  );
  % dy is the transposed matrix of d2.
  array[1..Q, 1..s + 1] of set of int: dy =
    array2d(1..Q, 1..s + 1, [d2[i, j] | j in 1..Q, i in 1..s + 1]);
} in regular_nfa(Ay, Q, s + 1, dy, q0, F) /\
     forall (i in index_set(Ax)) (
       Ax[i] = if Ay[i] > 1 then map[Ay[i] - 1] else 0 endif
     );
