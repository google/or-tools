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
# From http://twan.home.fmf.nl/blog/haskell/Nonograms.details
# The lambda picture
#
rows = 12
row_rule_len = 3
row_rules = [
    [0,0,2],
    [0,1,2],
    [0,1,1],
    [0,0,2],
    [0,0,1],
    [0,0,3],
    [0,0,3],
    [0,2,2],
    [0,2,1],
    [2,2,1],
    [0,2,3],
    [0,2,2]
    ]

cols = 10
col_rule_len = 2
col_rules = [
    [2,1],
    [1,3],
    [2,4],
    [3,4],
    [0,4],
    [0,3],
    [0,3],
    [0,3],
    [0,2],
    [0,2]
    ]
