predicate int_div_mod(var int: x, var int:y, var int:d, var int:r) =
  y != 0 /\
  x == y * d + r /\
  x * r >= 0 /\
  -abs(y) < r /\ r < abs(y) /\
  min(lb(x),-ub(x)) <= d /\ d <= max(ub(x),-lb(x)) /\
  %% the next 5 lines are probably unnecessary for propagation solvers
%  if lb(x) > 0 then
%     0 <= r /\ r < max(ub(y),-lb(y))
%  else if ub(x) < 0 then
%     min(lb(y),-ub(y)) < r /\ r <= 0
%  else true endif endif /\
%  if lb(y) > 0 then
%     lb(x) div lb(y) <= d /\ d <= ub(x) div lb(y)
%       else if ub(y) < 0  then
%           ub(x) div ub(y) <= d /\ d <= lb(x) div ub(y)
%  else true endif endif;


predicate int_div(var int:x, var int:y, var int:d) =
  let { var min(lb(y),-ub(y))+1 .. max(ub(y),-lb(y))-1: r } in
  int_div_mod(x,y,d,r);

predicate int_mud(var int:x, var int:y, var int:r) =
  let { var min(lb(x),-ub(x)) .. max(ub(x),-lb(x)): d } in
  int_div_mod(x,y,d,r);

%%predicate int_mod(var int x, int y, var int r);
