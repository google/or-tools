STEINER TRIPLE COVERING PROBLEM test instances

Let n = number of variables
    m = number of triples
    T(i) = set of 3 indices of i-th triple

The problem is;

MIN \sum_{j=1]^n x_j

SUBJ. TO:

\sum_{j \in T(i)} x_j \geq 1, for i = 1,...,m,

x_j \in {0,1} for all j = 1,...,n




The following files are in the distribution:

FILE       : INSTANCE  :    n   :     m
............................................
data.9     : stn9      :    9   :    12
data.15    : stn15     :   15   :    35   
data.27    : stn27     :   27   :   117   
data.45    : stn45     :   45   :   330   
data.81    : stn81     :   81   :  1080   
data.135   : stn135    :  135   :  3015   
data.243   : stn243    :  243   :  9801   
data.405   : stn405    :  405   : 27270   
data.729   : stn729    :  729   : 88452   

The file format is:

Line 1: n m
Lines 2 to m+1:  3 variable indices for triple


Best known solutions 

stn9      :   5  optimal (Fulkerson et al., 1974)
stn15     :   9  optimal (Fulkerson et al., 1974)
stn27     :  18  optimal (Fulkerson et al., 1974)
stn45     :  30  optimal (Ratliff, 1979)
stn81     :  61  optimal (Mannino and Sassano, 1995)
stn135    : 103  optimal (Ostrowski et al., 2009, 2010)
stn243    : 198  optimal (Ostrowski et al., 2009, 2010)
stn405    : 335  BKS     (Resende & Toso, 2010)
stn729    : 617  BKS     (Resende & Toso, 2010)

