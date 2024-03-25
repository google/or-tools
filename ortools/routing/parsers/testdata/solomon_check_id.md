The instance `solomon_check-id.txt` is supposed to have only one feasible
solution, so that it can be tested explicitly. This is useful to ensure that the
processing of node indices is correct.

Here are the timings used to determine the time windows:

Node (0, 0): 0                                                   0
Move to (2, 2): sqrt(2² + 2²) = 2 * sqrt(2) \approx 3            3
Time at (2, 2): 1                                                4
Move to (5, 5): sqrt(3² + 3²) = 3 * sqrt(2) \approx 4            8
Time at (5, 5): 1                                                9
Move to (9, 9): sqrt(4² + 4²) = 4 * sqrt(2) \approx 6            15
Time at (9, 9): 1                                                16
Move to (0, 0): sqrt(9² + 9²) = 9 * sqrt(2) \approx 13           29
