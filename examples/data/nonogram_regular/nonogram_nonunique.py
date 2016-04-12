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
# Nonogram problem from Gecode: Nonunique
# There are 43 solutions to this nonogram.
# http://www.gecode.org/gecode-doc-latest/classNonogram.html
#
rows = 15
row_rule_len = 4
row_rules = [
    [0,0,2,2],
    [0,0,2,2],
    [0,0,0,4],
    [0,0,1,1],
    [0,0,1,1],
    [1,1,1,1],
    [0,0,1,1],
    [0,0,1,4],
    [0,1,1,1],
    [0,1,1,4],
    [0,0,1,3],
    [0,0,1,2],
    [0,0,0,5],
    [0,0,2,2],
    [0,0,3,3]
    ]

cols = 11
col_rule_len = 5
col_rules = [
    [0,0,0,0,5],
    [0,0,1,2,4],
    [0,0,2,1,3],
    [0,2,2,1,1],
    [0,1,1,1,1],
    [0,0,0,1,5],
    [2,1,1,3,2],
    [2,1,1,1,1],
    [0,0,1,4,1],
    [0,0,0,1,1],
    [0,0,0,0,1]
    ]
