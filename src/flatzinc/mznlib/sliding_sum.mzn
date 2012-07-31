%-----------------------------------------------------------------------------%
% Requires that in each subsequence 'vs[i], ..., vs[i + seq - 1]' the sum of the
% values belongs to the interval ['low', 'up'].
%-----------------------------------------------------------------------------%

predicate sliding_sum(int: low, int: up, int: seq, array[int] of var int: vs);
