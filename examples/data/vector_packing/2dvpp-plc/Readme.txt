@Qian HU, janoy.qian@gmail.com
@20110811

Readme file for data format.

Files Structure:
data/				# specified input folder
	cf.txt			# piecewise linear cost function
	opt200/			# "opt200" instance set
		opt200-0.txt	# instance "opt200-0"
		opt200-1.txt	# instance "opt200-1"
	rand1024-2/		# instance set "rand1024-2"
		rand1024-2-3.txt	# instance "rand1024-2-3"  			
	...
	
	

Format of piecewise linear cost function, e.g. cf_seg.txt:
cf-001		# name of the cost function
SEGMENT_TYPE    # type of the file
 k		# the piecewise linear cost function has k pieces
 x_0   y_0	# coordinates of the start point
 x_1   y_1
 ...
 x_k   y_k	# coordinates of the last point
 -----------------------------------------
#e.g.
cf-001
SEGMENT_TYPE
3
0 	5.0
10 	5.0
70 	17.0
150 	57.0 


Format of instance file:
# filename: xxx.txt
instance_id  instance_set  # id of the instance ; name of the instance set
|B|   optimal_Cost  # number of bins used in an optimal solution, "-1" for "rand" instances ; cost in an optimal solution, "-1" for "rand" instances
W   V		# max weight and volume for each bin provided
N   T		# the total number of items ; the total number of types
w_1   v_1   d_1	# info of the first type of item, w_1 is its weight, v_1 for volume, d_1 is the total number of this item type
w_2   v_2   d_2
....
w_T   v_T   d_T # info of the last item type
		# (notice that, N = d_1 + d_2 +... d_T)
----------------------
# e.g.
rand256-3-1 rand256-3		
-1 	-1		
150	150	
256	8	
58	69	32
48	69	32
22	31	32
65	64	32
19	35	32
34	22	32
4	30	32
6	8	32