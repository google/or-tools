%-----------------------------------------------------------------------------%
% diffn: constrains rectangles, given by their origins and sizes, to be
% non-overlapping
%-----------------------------------------------------------------------------%

predicate diffn_nonstrict(array[int] of var int: x,
                          array[int] of var int: y,
                          array[int] of var int: dx,
                          array[int] of var int: dy);

