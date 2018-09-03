%%% Inclusion of this file eliminates set variables by converting them into arrays of var bool.
% AUTHORS
% Guido Tack <guido.tack@monash.edu>
% Gleb Belov <gleb.belov@monash.edu>
%
% Set comparisons are often used just to avoid symmetries. For that, lex cmp of direct set2bools
% would be much cheaper than the actual set_le++ below                    TODO


function var set of int: reverse_map(array[int] of var bool: b);
function set of int: reverse_map(array[int] of bool: b) ::promise_total =
  { i | i in index_set(b) where b[i] };

array[int] of var bool: set2bools(var set of int: x) ::promise_total =
  if is_fixed(x) then set2bools(fix(x)) else
  let {
    array[min(ub(x))..max(ub(x))] of var bool: b =
      [ set2bools_bit( x, i ) | i in min(ub(x))..max(ub(x)) ];
    constraint (x = reverse_map(b)) :: is_reverse_map;
  } in b
  endif;
array[int] of var bool: set2bools(var set of int: x, set of int: ubx) ::promise_total =
  if is_fixed(x) then set2bools(fix(x), ubx) else
  let {
    array[int] of var bool: b0 = set2bools( x );       %% Call in any case ?? TODO
    array[min(ubx)..max(ubx)] of var bool: b =
      [ if i in ubx then set2bools_bit( x, i ) else false endif
        | i in min(ubx)..max(ubx) ];
%% Not again:    constraint (x = reverse_map(b)) :: is_reverse_map;
  } in b
  endif;
array[int] of bool: set2bools(set of int: x) ::promise_total =
  array1d(min(x)..max(x),[i in x | i in min(x)..max(x)]);
array[int] of bool: set2bools(set of int: x, set of int: ubx) ::promise_total =
  array1d(min(ubx)..max(ubx),[i in x | i in min(ubx)..max(ubx)]);
function var bool: set2bools_bit( var set of int: x, int: i ) ::promise_total =
  if i in ub(x) then
  let {
    var bool: bi;
  } in bi
  else false endif;
  
  
predicate set_eq(var set of int: x, var set of int: y) =
  if not has_ub_set(x) /\ not has_ub_set(y) then
    assert(false, "Cannot determine bounds of set variables")
  elseif not has_ub_set(x) then set_eq(y,x)
  else
  let {
    set of int: uby = if has_ub_set(y) then ub(y) else ub(x) endif;
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y,uby);
  } in forall (i in index_set(bx) union index_set(by)) (
    if not (i in index_set(bx)) then not by[i]
    elseif not (i in index_set(by)) then not bx[i]
    else bx[i]=by[i] endif
  )
  endif;

predicate set_eq_reif(var set of int: x, var set of int: y, var bool: b) =
  if is_fixed(b) then
    if true==fix(b) then x==y else x!=y endif
  else
    let {
      array[int] of var bool: bx = set2bools(x);
      array[int] of var bool: by = set2bools(y);
    } in b <-> forall (i in index_set(bx) union index_set(by)) (
      if not (i in index_set(bx)) then not by[i]
      elseif not (i in index_set(by)) then not bx[i]
      else bx[i]=by[i] endif
    )
  endif;

predicate set_ne(var set of int: x, var set of int: y) =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
  } in exists (i in index_set(bx) union index_set(by)) (
    if not (i in index_set(bx)) then by[i]
    elseif not (i in index_set(by)) then bx[i]
    else bx[i]!=by[i] endif
  );

predicate set_ne_reif(var set of int: x, var set of int: y, var bool: b) =
  set_eq_reif( x, y, not b );

  %%% lex cmp of the sparse vectors
predicate set_le_lt_base__(var set of int: x, var set of int: y,
                         var bool: fValReif, bool: fStrict) ::promise_total =
  let {         %%   DOES NOT HELP in -Glinear globals_alldiff_set_nosets.mzn ....
    array[int] of var bool: bx0 = set2bools(x);   %% just to call reverse_map
    array[int] of var bool: by0 = set2bools(y);
  } in
  if not has_ub_set(x) \/ not has_ub_set(y) then
    assert(false, "set_le_lt_base__: cannot determine bounds of set variables")
  elseif {}==ub(x) /\ {}==ub(y) then
    fValReif==not fStrict
  elseif {}!=ub(x) /\ {}!=ub(y) /\ max(ub(x))<min(ub(y)) /\ not fStrict then
    fValReif
  elseif {}!=ub(x) /\ {}!=ub(y) /\ min(ub(x))>max(ub(y)) /\ fStrict then
    not fValReif
  else
  let {
    set of int: ubxy = ub(x) union ub(y);
    array[int] of var bool: bx = set2bools(x, ubxy);
    array[int] of var bool: by = set2bools(y, ubxy);
    int: i0 = min( ubxy );
    array[i0..i0+length(bx)] of var bool: bOkFrom;
    array[int] of var bool: bXMaxFrom = getMaxRearBools( set2bools( x ), i0+1..max(ubxy)+1 );
    array[int] of var bool: bYMaxFrom = getMaxRearBools( set2bools( y ), i0+1..max(ubxy)+1 );
  } in
    % trace( "set_le_lt_base__: " ++  show(ub(x)) ++ ", " ++ show(ub(y)) ++ "\n") /\
    (bOkFrom[i0] == fValReif)
    /\
    (bOkFrom[i0+length(bx)] == not fStrict)
    /\
    forall(i in ubxy) (                %% Can linearize stronger?    TODO
        bOkFrom[i] <-> (
              ( (bx[i] == by[i]) /\ bOkFrom[i+1] )
              \/
              ( (bx[i] <  by[i]) /\ not bXMaxFrom[i+1] )
              \/
              ( (bx[i] >  by[i]) /\ bYMaxFrom[i+1] )
        )
    )
  endif;

function var bool: getMaxRearBools_bit( array[int] of var bool: b, int: i ) =
  if {}==index_set( b ) \/ i>max( index_set( b ) ) then 
    false
  elseif i in index_set( b ) then
    b[i] \/ getMaxRearBools_bit( b, i+1 )
  else
    getMaxRearBools_bit( b, min( index_set( b ) ) )
  endif;

%% Returns an array of bools each is the max of b[i] and the later elements
function array[int] of var bool: getMaxRearBools( array[int] of var bool: b ) =
  array1d( index_set(b), [ getMaxRearBools_bit( b, i )
    | i in index_set( b ) ] );
  
%% Generalizes getMaxRearBools to a sub/superset of the array's indexes
function array[int] of var bool: getMaxRearBools(
                                 array[int] of var bool: b, set of int: ubb ) =
  array1d( min(ubb)..max(ubb), [ getMaxRearBools_bit( b, i )
             | i in min(ubb)..max(ubb) ] );

  
predicate set_le(var set of int: x, var set of int: y) ::promise_total =
  set_le_lt_base__(x, y, true, false);

predicate set_le_reif(var set of int: x, var set of int: y, var bool: p) ::promise_total =
  set_le_lt_base__(x, y, p, false);

predicate set_lt(var set of int: x, var set of int: y) ::promise_total =
  set_le_lt_base__(x, y, true, true);

predicate set_lt_reif(var set of int: x, var set of int: y, var bool: p) ::promise_total =
  set_le_lt_base__(x, y, p, true);

predicate set_subset(var set of int: x, var set of int: y) =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
  } in forall (i in index_set(bx)) (
    if not (i in index_set(by)) then not bx[i]
    else bx[i] -> by[i] endif
  );

predicate set_subset_reif(var set of int: x, var set of int: y, var bool: b) =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
  } in b <-> forall (i in index_set(bx)) (
    if not (i in index_set(by)) then not bx[i]
    else bx[i] -> by[i] endif
  );

function var set of int: set_intersect(var set of int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
    var set of (index_set(bx) intersect index_set(by)): z;
    array[int] of var bool: bz = set2bools(z);
    constraint forall (i in index_set(bz)) (
      bz[i] = (bx[i] /\ by[i])
    );
  } in z;

function var set of int: set_union(var set of int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
    var set of (index_set(bx) union index_set(by)): z;
    array[int] of var bool: bz = set2bools(z);
    constraint forall (i in index_set(bx) union index_set(by)) (
      if (i in index_set(bx)) then
        if (i in index_set(by)) then
          bz[i] = (bx[i] \/ by[i])
        else
          bz[i] = bx[i]
        endif
      else
          bz[i] = by[i]
      endif
    );
  } in z;

function var set of int: set_diff(var set of int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
    var set of (index_set(bx) diff index_set(by)): z;
    array[int] of var bool: bz = set2bools(z);
    constraint forall (i in index_set(bz)) (
      bz[i] = (bx[i] /\ (not by[i]))
    );
  } in z;

function var set of int: set_symdiff(var set of int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
    var set of (index_set(bx) diff index_set(by)): z;
    array[int] of var bool: bz = set2bools(z);
    constraint forall (i in index_set(bz)) (
      bz[i] = (bx[i] xor by[i])
    );
  } in z;

function var int: card(var set of int: x) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    var 0..length(bx) : c;
    constraint bool_lin_eq([1 | i in index_set(bx)],bx,c);
  } in c;

predicate set_in(var int: x, var set of int: y) =
  let {
    array[int] of var bool: by = set2bools(y);
  } in by[x];

predicate set_in_reif(var int: x, var set of int: y, var bool: b) =
  let {
    array[int] of var bool: by = set2bools(y);
  } in b <-> by[x];

function array[int] of var bool: setarray2bools(array[int] of var set of int: x) =
  if length(x)=0 then [] else
  set2bools(x[1])++setarray2bools([x[i]|i in 2..length(x)])
  endif;
%% Par version no sense
  
annotation set_search(array[int] of var set of int: x, ann: a1, ann: a2, ann: a3) =
  bool_search(setarray2bools(x),a1,a2,a3);
