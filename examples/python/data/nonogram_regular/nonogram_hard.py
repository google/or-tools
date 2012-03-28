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
# Nonogram "hard"
# Note: I don't remember where I found this.
# It isn't very hard, though.
#
rows = 10
row_rule_len = 4
row_rules = [
    [0,0,0,1],
    [0,0,0,3],
    [0,0,1,3],
    [0,0,2,4],
    [0,0,1,2],
    [0,2,1,1],
    [1,1,1,1],
    [0,2,1,1],
    [0,0,2,2],
    [0,0,0,5]
    ]

cols = 10
col_rule_len = 4
col_rules = [
    [0,0,0,4],
    [0,0,1,3],
    [0,0,2,3],
    [0,0,1,2],
    [0,0,2,2],
    [0,1,1,1],
    [1,1,1,1],
    [0,1,1,1],
    [0,0,1,2],
    [0,0,0,5]
    ]

