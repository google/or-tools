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
# Nonogram problem from Gecode: "Unknown"
# http://www.gecode.org/gecode-doc-latest/classNonogram.html
#
rows = 9
row_rule_len = 5
row_rules = [
    [0, 0, 0, 0, 3],
    [0, 0, 2, 3, 2],
    [0, 0, 0, 10, 3],
    [0, 0, 0, 0, 15],
    [1, 1, 1, 1, 6],
    [0, 0, 0, 1, 7],
    [0, 0, 0, 1, 4],
    [0, 0, 0, 1, 4],
    [0, 0, 0, 0, 4]
    ]

cols = 15
col_rule_len = 2
col_rules = [
    [0, 3],
    [0, 4],
    [2, 2],
    [3, 1],
    [2, 3],
    [3, 2],
    [2, 3],
    [4, 2],
    [3, 2],
    [0, 6],
    [1, 3],
    [1, 3],
    [1, 4],
    [0, 5],
    [0, 5]
    ]
