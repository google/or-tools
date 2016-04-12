# Copyright 2010 Hakan Kjellerstrand hakank@bonetmail.com
#
# Licensed under the Apache License, Version 2.0 (the 'License'); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at 
#
#     http://www.apache.org/licenses/LICENSE-2.0 
#
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an 'AS IS' BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License. 
#
# Nonogram problem from Gecode: P200
# http://www.gecode.org/gecode-doc-latest/classNonogram.html
#
rows = 25
row_rule_len = 7
row_rules = [
    [0,0,0,0,2,2,3],
    [0,0,4,1,1,1,4],
    [0,0,4,1,2,1,1],
    [4,1,1,1,1,1,1],
    [0,2,1,1,2,3,5],
    [0,1,1,1,1,2,1],
    [0,0,3,1,5,1,2],
    [0,3,2,2,1,2,2],
    [2,1,4,1,1,1,1],
    [0,2,2,1,2,1,2],
    [0,1,1,1,3,2,3],
    [0,0,1,1,2,7,3],
    [0,0,1,2,2,1,5],
    [0,0,3,2,2,1,2],
    [0,0,0,3,2,1,2],
    [0,0,0,0,5,1,2],
    [0,0,0,2,2,1,2],
    [0,0,0,4,2,1,2],
    [0,0,0,6,2,3,2],
    [0,0,0,7,4,3,2],
    [0,0,0,0,7,4,4],
    [0,0,0,0,7,1,4],
    [0,0,0,0,6,1,4],
    [0,0,0,0,4,2,2],
    [0,0,0,0,0,2,1]
    ]

cols = 25
col_rule_len = 6
col_rules = [
    [0,0,1,1,2,2],
    [0,0,0,5,5,7],
    [0,0,5,2,2,9],
    [0,0,3,2,3,9],
    [0,1,1,3,2,7],
    [0,0,0,3,1,5],
    [0,7,1,1,1,3],
    [1,2,1,1,2,1],
    [0,0,0,4,2,4],
    [0,0,1,2,2,2],
    [0,0,0,4,6,2],
    [0,0,1,2,2,1],
    [0,0,3,3,2,1],
    [0,0,0,4,1,15],
    [1,1,1,3,1,1],
    [2,1,1,2,2,3],
    [0,0,1,4,4,1],
    [0,0,1,4,3,2],
    [0,0,1,1,2,2],
    [0,7,2,3,1,1],
    [0,2,1,1,1,5],
    [0,0,0,1,2,5],
    [0,0,1,1,1,3],
    [0,0,0,4,2,1],
    [0,0,0,0,0,3]
    ]

