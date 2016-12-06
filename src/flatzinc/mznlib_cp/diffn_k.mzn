predicate diffn_k(array[int,int] of var int: box_posn,
                  array[int,int] of var int: box_size) =
  let { int: num_boxes = card(index_set_1of2(box_posn));
        int: num_dims = card(index_set_2of2(box_posn)) } in
  diffn_k_with_sizes(box_posn, box_size, num_boxes, num_dims);

predicate diffn_k_with_sizes(array[int,int] of var int: box_posn,
                       array[int,int] of var int: box_size,
                             int: num_boxes, int: num_dims);
