predicate ortools_network_flow_cost(array [int] of int: arc,
                                    array [int] of int: balance,
                                    int: base_node,
                                    array [int] of var int: flow,
                                    array [int] of int: weight,
                                    var int: cost);

predicate fzn_network_flow_cost(array [int,int] of int: arc,
                                array [int] of int: balance,
                                array [int] of int: weight,
                                array [int] of var int: flow,
                                var int: cost) =
    ortools_network_flow_cost(array1d(arc),
                              balance,
                              min(index_set(balance)),
                              flow,
                              weight,
                              cost);
