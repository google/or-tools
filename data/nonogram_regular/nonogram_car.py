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
# Problem from ECLiPSe
# http:#eclipse.crosscoreop.com/eclipse/examples/nono.ecl.txt
# Problem n3 ( http:#www.pro.or.jp/~fuji/java/puzzle/nonogram/index-eng.html )
# 'Car'
#
rows = 10;
row_rule_len = 4;
row_rules = [
    [0,0,0,4],
    [0,1,1,6],
    [0,1,1,6],
    [0,1,1,6],
    [0,0,4,9],
    [0,0,1,1],
    [0,0,1,1],
    [0,2,7,2],
    [1,1,1,1],
    [0,0,2,2]
    ]

cols = 15;
col_rule_len = 2;
col_rules = [
    [0,4],
    [1,2],
    [1,1],
    [5,1],
    [1,2],
    [1,1],
    [5,1],
    [1,1],
    [4,1],
    [4,1],
    [4,2],
    [4,1],
    [4,1],
    [4,2],
    [0,4]
    ]
