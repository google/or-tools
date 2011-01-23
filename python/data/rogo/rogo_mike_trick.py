# Data from
# Mike Trick: "Operations Research, Sudoko, Rogo, and Puzzles"
# http://mat.tepper.cmu.edu/blog/?p=1302
#
# Best points: 8
# 
# This has 48 solutions with symmetries;
# 4 when the path symmetry is removed.
#
rows = 5
cols = 9
max_steps = 12
W = 0
B = -1
problem = [
    [2,W,W,W,W,W,W,W,W],
    [W,3,W,W,1,W,W,2,W],
    [W,W,W,W,W,W,B,W,2],
    [W,W,2,B,W,W,W,W,W],
    [W,W,W,W,2,W,W,1,W]
    ]

# Copyright 2011 Hakan Kjellerstrand hakank@bonetmail.com
#
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at 
#
#     http://www.apache.org/licenses/LICENSE-2.0 
#
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an "AS IS" BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License. 
