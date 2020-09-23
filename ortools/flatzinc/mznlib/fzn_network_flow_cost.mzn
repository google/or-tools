predicate ortools_network_flow_cost(array [int] of int: arc,
                                    array [int] of int: balance,
                                    array [int] of int: weight,
                                    array [int] of var int: flow,
                                    var int: cost);

predicate fzn_network_flow_cost(array [int,int] of int: arc,
                                array [int] of int: balance,
                                array [int] of int: weight,
                                array [int] of var int: flow,
                                var int: cost) =
    ortools_network_flow_cost(array1d(arc),balance,weight,flow,cost);
