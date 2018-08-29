%%% Inclusion of this file eliminates set variables by converting them into arrays of var bool.
% AUTHORS
% Guido Tack <guido.tack@monash.edu>
% Gleb Belov <gleb.belov@monash.edu>
%

include "lex_less.mzn";
include "lex_lesseq.mzn";

predicate mzn_reverse_map_var(var set of int: x) = let {
  array[int] of var bool: b = set2bools(x)
} in true;

function var set of int: reverse_map(array[int] of var bool: b);
function set of int: reverse_map(array[int] of bool: b) ::promise_total =
  { i | i in index_set(b) where b[i] };

array[int] of var bool: set2bools(var set of int: x) ::promise_total =
  if is_fixed(x) then set2bools(fix(x)) else
  let {
    array[int] of var bool: b = array1d( min(ub(x))..max(ub(x)),
      [ set2bools_bit( x, i ) | i in min(ub(x))..max(ub(x)) ] );
    constraint (x = reverse_map(b)) :: is_reverse_map;
  } in b
  endif;
array[int] of var bool: set2bools(var set of int: x, set of int: ubx) ::promise_total =
  if is_fixed(x) then set2bools(fix(x), ubx) else
  let {
    array[int] of var bool: b0 = set2bools( x );       %% Call in any case ?? TODO
    array[int] of var bool: b = array1d( min(ubx)..max(ubx),
      [ if i in ubx then set2bools_bit( x, i ) else false endif
        | i in min(ubx)..max(ubx) ] );
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
  
  
predicate set_eq(var set of int: x, var set of int: y) ::promise_total =
  if not has_ub_set(x) /\ not has_ub_set(y) then
    assert(false, "set_eq: cannot determine bounds of set variables")
  elseif not has_ub_set(x) then set_eq(y,x)
  else
  let {
    set of int: uby = if has_ub_set(y) then ub(y) else
      let {
        constraint y subset x,
      } in ub(x)
    endif;
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y,uby);
  } in forall (i in index_set(bx) union index_set(by)) (
    if not (i in index_set(bx)) then not by[i]
    elseif not (i in index_set(by)) then not bx[i]
    else bx[i]=by[i] endif
  )
  endif;

predicate set_eq_reif(var set of int: x, var set of int: y, var bool: b) ::promise_total =
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

predicate set_ne(var set of int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
  } in exists (i in index_set(bx) union index_set(by)) (
    if not (i in index_set(bx)) then by[i]
    elseif not (i in index_set(by)) then bx[i]
    else bx[i]!=by[i] endif
  );

predicate set_ne_reif(var set of int: x, var set of int: y, var bool: b) ::promise_total =
  set_eq_reif( x, y, not b );

  
% Set comparisons are often used just to avoid symmetries.
% Reifs/imps are done automatically from these templates
predicate set_le(var set of int: x, var set of int: y) ::promise_total =
  let {
    set of int: ubxy = ub(x) union ub(y),
    array[int] of var bool: bx = set2bools(x, ubxy);
    array[int] of var bool: by = set2bools(y, ubxy);
  } in
    lex_lesseq( by, bx );

predicate set_lt(var set of int: x, var set of int: y) ::promise_total =
  let {
    set of int: ubxy = ub(x) union ub(y),
    array[int] of var bool: bx = set2bools(x, ubxy);
    array[int] of var bool: by = set2bools(y, ubxy);
  } in
    lex_less( by, bx );


predicate set_subset(var set of int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
  } in forall (i in index_set(bx)) (
    if not (i in index_set(by)) then not bx[i]
    else bx[i] -> by[i] endif
  );

predicate set_subset_reif(var set of int: x, var set of int: y, var bool: b) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
  } in b <-> forall (i in index_set(bx)) (
    if not (i in index_set(by)) then not bx[i]
    else bx[i] -> by[i] endif
  );

%%% Map the subset operation to superset
predicate set_superset(var set of int: x, var set of int: y) ::promise_total = set_subset( y, x );
predicate set_superset_reif(var set of int: x, var set of int: y, var bool: b) ::promise_total =
    set_subset_reif( y, x, b );

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
    var set of ub(x) diff lb(y): z;
    array[int] of var bool: bz = set2bools(z);
    constraint forall (i in ub(z)) (
      bz[i] = (bx[i] /\ (not by[i]))
    );
  } in z;

function var set of int: set_symdiff(var set of int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    array[int] of var bool: by = set2bools(y);
    var set of (ub(x) diff lb(y)) union (ub(y) diff lb(x)): z;
    array[int] of var bool: bz = set2bools(z);
    constraint forall (i in ub(z)) (
      bz[i] = (bx[i] xor by[i])
    );
  } in z;

function var int: card(var set of int: x) ::promise_total =
  let {
    array[int] of var bool: bx = set2bools(x);
    var 0..length(bx) : c;
    constraint bool_lin_eq([1 | i in index_set(bx)],[bx[i] | i in index_set(bx)],c);
  } in c;

predicate set_in(int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: by = set2bools(y);
  } in by[x];
predicate set_in(var int: x, var set of int: y) ::promise_total =
  let {
    array[int] of var bool: by = set2bools(y);
  } in by[x];

predicate set_in_reif(int: x, var set of int: y, var bool: b) ::promise_total =
  if x in ub(y) then
    b <-> set2bools(y)[x]
  else
    not b
  endif;
predicate set_in_reif(var int: x, var set of int: y, var bool: b) ::promise_total =
  b <-> ( x in ub(y) /\ set2bools(y)[x] );

function array[int] of var bool: setarray2bools(array[int] of var set of int: x) =
  if length(x)=0 then [] else
  set2bools(x[1])++setarray2bools([x[i]|i in 2..length(x)])
  endif;
%% Par version no sense
  

predicate array_var_set_element(var int: x, array[int] of var set of int: y, var set of int: z) ::promise_total =
  let {
    constraint x in { i | i in index_set( y ) where
      lb(y[i]) subset ub(z) /\ lb(z) subset ub(y[i])
    };
    set of int: sUB = array_union( [ ub(y[i]) | i in dom(x) ] );
    set of int: sLB = array_intersect( [ lb(y[i]) | i in dom(x) ] );
    constraint z subset sUB,
    constraint sLB subset z,
  } in
  forall (k in ub(z)) (
    set2bools(z)[k] == 
      if k in sUB then
        if k in sLB then
          true
        else
          array1d( lb(x)..ub(x),
            [ if k in ub(y[i]) then set2bools(y[i])[k] else false endif | i in dom(x) ] )[x]
        endif
      else
        false
      endif
  ) /\
  forall (k in sUB diff ub(z))(
    if k in sLB then
      false                     %% fail the constraint
    else
      not array1d( lb(x)..ub(x),
        [ if k in ub(y[i]) then set2bools(y[i])[k] else false endif | i in dom(x) ] )[x]
    endif
  );

predicate array_set_element(var int: x, array[int] of set of int: y, var set of int: z) ::promise_total =
  let {
    constraint x in { i | i in index_set( y ) where
      y[i] subset ub(z) /\ lb(z) subset y[i]
    };
    set of int: sUB = array_union( [ y[i] | i in dom(x) ] );
    set of int: sLB = array_intersect( [ y[i] | i in dom(x) ] );
    constraint z subset sUB,
    constraint sLB subset z,
  } in
  forall (k in ub(z)) (
    set2bools(z)[k] == 
      if k in sUB then
        if k in sLB then
          true
        else
          array1d( lb(x)..ub(x),
            [ if k in ub(y[i]) then set2bools(y[i])[k] else false endif | i in dom(x) ] )[x]
        endif
      else
        false
      endif
  ) /\
  forall (k in sUB diff ub(z))(
    if k in sLB then
      false                     %% fail the constraint
    else
      not array1d( lb(x)..ub(x),
        [ if k in ub(y[i]) then set2bools(y[i])[k] else false endif | i in dom(x) ] )[x]
    endif
  );

annotation set_search(array[int] of var set of int: x, ann: a1, ann: a2, ann: a3) =
  seq_search( [ bool_search(set2bools(x[i]),a1,a2,a3) | i in index_set(x) ] );

annotation warm_start( array[int] of var set of int: x, array[int] of set of int: v ) =
   warm_start_array( [
       let {
         array[int] of var bool: xb = set2bools(x[i]),
         set of int: is_var = ub(x[i]) diff lb(x[i]),
         int: iV = i - min(index_set(x)) + if 0<length(v) then min(index_set(v)) else 0 endif,
       } in
       warm_start( [ xb[k] | k in is_var ],
         if iV in index_set(v) then [ k in v[iV] | k in is_var ] else [] endif )
   | i in index_set(x) ] );       %% ignoring v[i] diff ub(x[i]) and lb(x[i]) diff v[i]

