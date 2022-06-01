NAME          MIN_SIZE_MAX_FEATURES

ROWS
 N  COST
 E  ROW1
 L  ROW2
 G  ROW3
 E  ROW4
 E  ROW5
 G  ROW6
 E  ROW7
 L  ROW8
 N  UNCONST.

COLUMNS
    X1        ROW1              1      UNCONST.            1
    X2        ROW1              1      UNCONST.            2
    X1        COST              1
    X2        COST              2
    X1        ROW2              3
    X2        ROW2              4
    X1        ROW3              1
    X2        ROW3              2
    X3        ROW3              3      ROW7                1
    X4        ROW3              4      ROW7                1
    X5        ROW3              5      ROW4                2
    X6        ROW3              6      ROW4                13
    X7        ROW5              1      ROW6                7
    X8        ROW5              3      ROW6                2
    X7        ROW8              1
    X8        ROW8              1


RHS
              ROW1              2
              ROW2              6
              ROW3              -85
              ROW4              2
              ROW5              8
              ROW6              0
              ROW7              50
              ROW8              50

BOUNDS
 FR           X1
 UP           X2                125.
 FX           X3                12
 MI           X4
 LO           X5                5.
 PL           X6

RANGES
    RANGE1    ROW2              20.
              ROW3              25.
              ROW4              12.
              ROW7              -100.
ENDATA

