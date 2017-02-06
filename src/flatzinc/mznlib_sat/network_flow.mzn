predicate network_flow_cost(array [int,int] of int: arc,
                            array [int] of int: balance,
                            array [int] of int: weight,
                            array [int] of var int: flow,
                            var int: cost);

predicate network_flow(array [int,int] of int: arc,
                       array [int] of int: balance,
                       array [int] of var int: flow);
