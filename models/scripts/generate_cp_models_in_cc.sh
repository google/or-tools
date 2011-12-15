#!/bin/bash

make -j 4 cpexe

for i in 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18
do
  name=costas_array_hard_$i
  echo $name
  ./costas_array --minsize=$i --cp_export_file=/tmp/$name --cp_no_solve
  ./model_util --input=/tmp/$name --rename_model=$name --insert_license=models/headers/google_license.txt --output=models/$name.cp
done

for i in 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18
do
  name=costas_array_soft_$i
  echo $name
  ./costas_array --minsize=$i --cp_export_file=/tmp/$name --cp_no_solve --soft_constraints
  ./model_util --input=/tmp/$name --rename_model=$name --insert_license=models/headers/google_license.txt --output=models/$name.cp
done

for name in cryptarithm tsp cvrptw
do
  echo $name
  ./$name --cp_export_file=/tmp/$name --cp_no_solve
  ./model_util --input=/tmp/$name --rename_model=$name --insert_license=models/headers/google_license.txt --output=models/$name.cp
done

for i in 4 5 6 7 8 9 10 11 12 13
do
  name=golomb_$i
  echo $name
  ./golomb --size=$i --cp_export_file=/tmp/$name --cp_no_solve
  ./model_util --input=/tmp/$name --rename_model=$name --insert_license=models/headers/google_license.txt --output=models/$name.cp
done

for data_file in data/jobshop/*
do
  data=${data_file##*/}
  name=jobshop_$data
  echo $name
  ./jobshop --data_file=$data_file --cp_export_file=/tmp/$name --cp_no_solve
  ./model_util --input=/tmp/$name --rename_model=$name --insert_license=models/headers/google_license.txt --output=models/$name.cp
done

for i in 4 5 6 7 8 9 10 11 12 13 15 18 20 25 30 35 40 50
do
  name=magic_square_$i
  echo $name
  ./magic_square --size=$i --cp_export_file=/tmp/$name --cp_no_solve
  ./model_util --input=/tmp/$name --rename_model=$name --insert_license=models/headers/google_license.txt --output=models/$name.cp
done

for i in 4 5 6 7 8 9 10 15 20 25 30 40 50 100 150 200 250 300 500 750 1000
do
  name=nqueen_$i
  echo $name
  ./nqueens --size=$i --cp_export_file=/tmp/$name --cp_no_solve
  ./model_util --input=/tmp/$name --rename_model=$name --insert_license=models/headers/google_license.txt --output=models/$name.cp
done

for clients in 0 10 20
do
  for backbones in 10 20
  do
    for seed in 0 1 2 3 4 5 6 7 8 9
    do
      client_degree=2
      demands=$((clients * clients / 2))
      if [ "${demands}" = "0" ]; then
        demands=$((backbones * backbones / 3))
      fi
      traffic_min=5
      traffic_max=50
      capacity=$((demands * traffic_max / 10))
      backbone_degree=4
      name=network_design_clients${clients}_backbones${backbones}_demands${demands}_traffic${traffic_min}-${traffic_max}_capacity${capacity}_client_degree${client_degree}_backbone_degree${backbone_degree}_seed${seed}
      echo $name
      ./network_routing \
        --clients=${clients}\
        --backbones=${backbones}\
        --demands=${demands}\
        --traffic_min=${traffic_min}\
        --traffic_max=${traffic_max}\
        --min_client_degree=${client_degree}\
        --max_client_degree=${client_degree}\
        --min_backbone_degree=${backbone_degree}\
        --max_backbone_degree=${backbone_degree}\
        --max_capacity=${capacity}\
        --seed=${seed}\
        --cp_export_file=/tmp/$name\
        --time_limit=1
      ./model_util \
        --input=/tmp/$name\
         --rename_model=$name\
        --insert_license=models/headers/google_license.txt\
        --output=models/$name.cp\
        --strip_limit
    done
  done
done
