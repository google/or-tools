predicate ortools_network_flow(array [int] of int: arc,
                               array [int] of int: balance,
                               int: base_node,
                               array [int] of var int: flow);

predicate fzn_network_flow(array [int,int] of int: arc,
                           array [int] of int: balance,
                           array [int] of var int: flow) =
    ortools_network_flow(array1d(arc), balance, min(index_set(balance)), flow);
